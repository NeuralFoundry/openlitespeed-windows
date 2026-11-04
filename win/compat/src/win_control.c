#include "compat_internal.h"
#include <winsvc.h>
#include <sddl.h>
#include <aclapi.h>

#define OLS_SERVICE_NAME    L"OpenLiteSpeed"
#define OLS_SERVICE_ACCOUNT L"NT SERVICE\\OpenLiteSpeed"
#define OLS_CTRL_RELOAD     128

static SERVICE_STATUS_HANDLE g_svc;
static SERVICE_STATUS g_svc_status;
static HANDLE g_evt_stop;
static HANDLE g_evt_reload;

static int exe_path(wchar_t *buf, DWORD len)
{
    DWORD n = GetModuleFileNameW(NULL, buf, len);
    return (n == 0 || n >= len) ? -1 : 0;
}

static int root_path(wchar_t *buf, DWORD len)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", buf, len);
    if (n == 0 || n >= len)
        return -1;
    while (n > 3 && (buf[n - 1] == L'\\' || buf[n - 1] == L'/'))
        buf[--n] = 0;
    return 0;
}

static pid_t read_pid(void)
{
    const char *path = getenv("OLS_PID_FILE");
    char buf[32];
    int fd;
    ssize_t n;
    fd = open(path ? path : "/tmp/lshttpd/lshttpd.pid", O_RDONLY);
    if (fd < 0)
        return -1;
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;
    buf[n] = 0;
    return (pid_t)atoi(buf);
}

static int server_alive(pid_t pid)
{
    return pid > 0 && kill(pid, 0) == 0 && ols_pid_is_ours(pid);
}

static int wait_exit(pid_t pid, int seconds)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    DWORD r;
    if (!h)
        return 0;
    r = WaitForSingleObject(h, seconds * 1000);
    CloseHandle(h);
    return r == WAIT_OBJECT_0 ? 0 : -1;
}

static int start_detached(void)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 16];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    if (exe_path(exe, MAX_PATH) < 0)
        return 1;
    swprintf(cmd, MAX_PATH + 16, L"\"%s\" -d", exe);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE,
                        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi))
    {
        fprintf(stderr, "failed to start server: error %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    printf("started, pid %lu\n", pi.dwProcessId);
    return 0;
}

static int signal_server(int sig, const char *what)
{
    pid_t pid = read_pid();
    if (!server_alive(pid))
    {
        printf("not running\n");
        return 1;
    }
    if (kill(pid, sig) != 0)
    {
        fprintf(stderr, "failed to %s server pid %d: %s\n", what, (int)pid, strerror(errno));
        return 1;
    }
    if (sig == SIGTERM)
    {
        if (wait_exit(pid, 60) != 0)
        {
            fprintf(stderr, "server pid %d did not stop in 60 seconds\n", (int)pid);
            return 1;
        }
        printf("stopped\n");
    }
    return 0;
}

static SC_HANDLE open_service(DWORD access, int *installed)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE svc;
    *installed = 0;
    if (!scm)
        return NULL;
    svc = OpenServiceW(scm, OLS_SERVICE_NAME, access);
    if (svc || GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
        *installed = 1;
    CloseServiceHandle(scm);
    return svc;
}

static DWORD svc_state(SC_HANDLE svc, DWORD *pid)
{
    SERVICE_STATUS_PROCESS st;
    DWORD n;
    if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (BYTE *)&st, sizeof(st), &n))
        return 0;
    if (pid)
        *pid = st.dwProcessId;
    return st.dwCurrentState;
}

static int svc_wait(SC_HANDLE svc, DWORD want, int seconds)
{
    int i;
    for (i = 0; i < seconds * 10; ++i)
    {
        if (svc_state(svc, NULL) == want)
            return 0;
        Sleep(100);
    }
    return -1;
}

static int control_service(SC_HANDLE svc, const char *cmd)
{
    SERVICE_STATUS ss;
    DWORD state = svc_state(svc, NULL);
    int stop = strcmp(cmd, "stop") == 0 || strcmp(cmd, "restart") == 0;
    int start = strcmp(cmd, "start") == 0 || strcmp(cmd, "restart") == 0;
    if (strcmp(cmd, "status") == 0)
    {
        pid_t pid = read_pid();
        if (state == SERVICE_RUNNING)
        {
            if (pid > 0)
                printf("running (service), pid %d\n", (int)pid);
            else
                printf("running (service)\n");
            return 0;
        }
        printf("not running (service)\n");
        return 3;
    }
    if (strcmp(cmd, "reload") == 0 || strcmp(cmd, "graceful") == 0)
    {
        pid_t old = read_pid(), npid;
        int i;
        if (state != SERVICE_RUNNING)
            start = 1;
        else
        {
            if (!ControlService(svc, OLS_CTRL_RELOAD, &ss))
            {
                fprintf(stderr, "reload failed (error %lu)\n", GetLastError());
                return 1;
            }
            for (i = 0; i < 100; ++i)
            {
                Sleep(100);
                npid = read_pid();
                if (npid > 0 && npid != old && server_alive(npid))
                {
                    printf("reloaded, new pid %d\n", (int)npid);
                    return 0;
                }
            }
            printf("reload requested\n");
            return 0;
        }
    }
    if (stop && state != SERVICE_STOPPED)
    {
        if (!ControlService(svc, SERVICE_CONTROL_STOP, &ss) && GetLastError() != ERROR_SERVICE_NOT_ACTIVE)
        {
            fprintf(stderr, "stop failed (error %lu)\n", GetLastError());
            return 1;
        }
        if (svc_wait(svc, SERVICE_STOPPED, 90) != 0)
        {
            fprintf(stderr, "service did not stop in 90 seconds\n");
            return 1;
        }
        printf("stopped\n");
    }
    else if (stop && !start)
        printf("not running\n");
    if (start)
    {
        if (!StartServiceW(svc, 0, NULL) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
        {
            fprintf(stderr, "start failed (error %lu)\n", GetLastError());
            return 1;
        }
        if (svc_wait(svc, SERVICE_RUNNING, 60) != 0)
        {
            fprintf(stderr, "service did not start\n");
            return 1;
        }
        printf("started (service)\n");
    }
    return 0;
}

int ols_win_control(const char *cmd)
{
    pid_t pid;
    int installed;
    SC_HANDLE svc;
    if (strcmp(cmd, "start") && strcmp(cmd, "stop") && strcmp(cmd, "restart") && strcmp(cmd, "reload")
        && strcmp(cmd, "graceful") && strcmp(cmd, "status"))
    {
        fprintf(stderr, "usage: openlitespeed -s start|stop|restart|reload|status\n");
        return 2;
    }
    svc = open_service(SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP | SERVICE_USER_DEFINED_CONTROL,
                       &installed);
    if (!svc && installed && strcmp(cmd, "status") == 0)
        svc = open_service(SERVICE_QUERY_STATUS, &installed);
    if (svc)
    {
        int r = control_service(svc, cmd);
        CloseServiceHandle(svc);
        return r;
    }
    if (installed)
    {
        fprintf(stderr, "the OpenLiteSpeed service is installed; run this command as Administrator\n");
        return 1;
    }
    if (strcmp(cmd, "stop") == 0)
        return signal_server(SIGTERM, "stop");
    if (strcmp(cmd, "start") == 0)
    {
        pid = read_pid();
        if (server_alive(pid))
        {
            printf("already running, pid %d\n", (int)pid);
            return 0;
        }
        return start_detached();
    }
    if (strcmp(cmd, "reload") == 0 || strcmp(cmd, "graceful") == 0)
    {
        pid_t npid;
        int i;
        pid = read_pid();
        if (!server_alive(pid))
            return start_detached();
        if (kill(pid, SIGUSR1) != 0)
        {
            fprintf(stderr, "failed to signal server pid %d: %s\n", (int)pid, strerror(errno));
            return 1;
        }
        for (i = 0; i < 100; ++i)
        {
            Sleep(100);
            npid = read_pid();
            if (npid > 0 && npid != pid && server_alive(npid))
            {
                printf("reloaded, new pid %d\n", (int)npid);
                return 0;
            }
        }
        fprintf(stderr, "new server process did not come up\n");
        return 1;
    }
    if (strcmp(cmd, "restart") == 0)
    {
        pid = read_pid();
        if (server_alive(pid) && signal_server(SIGTERM, "stop") != 0)
            return 1;
        return start_detached();
    }
    pid = read_pid();
    if (server_alive(pid))
    {
        printf("running, pid %d\n", (int)pid);
        return 0;
    }
    printf("not running\n");
    return 3;
}

static int wait_new_pid(pid_t old)
{
    pid_t npid;
    int i;
    for (i = 0; i < 100; ++i)
    {
        Sleep(100);
        npid = read_pid();
        if (npid > 0 && npid != old && server_alive(npid))
            return 0;
    }
    return -1;
}

int ols_win_reload_running(void)
{
    SERVICE_STATUS ss;
    SC_HANDLE svc;
    pid_t pid = read_pid();
    int installed, up, ok;
    DWORD err;
    if ((svc = open_service(SERVICE_QUERY_STATUS | SERVICE_USER_DEFINED_CONTROL, &installed)) != NULL)
    {
        up = svc_state(svc, NULL) == SERVICE_RUNNING;
        ok = up && ControlService(svc, OLS_CTRL_RELOAD, &ss);
        err = GetLastError();
        CloseServiceHandle(svc);
        if (!up)
            return 1;
        if (!ok)
        {
            fprintf(stderr, "the web server could not be reloaded (error %lu)\n", err);
            return -1;
        }
        wait_new_pid(pid);
        return 0;
    }
    if (installed)
    {
        svc = open_service(SERVICE_QUERY_STATUS, &installed);
        up = svc && svc_state(svc, NULL) == SERVICE_RUNNING;
        if (svc)
            CloseServiceHandle(svc);
        if (!up)
            return 1;
        fprintf(stderr, "the OpenLiteSpeed service could not be reloaded; run openlitespeed -s reload as Administrator\n");
        return -1;
    }
    if (!server_alive(pid))
        return 1;
    if (kill(pid, SIGUSR1) != 0 || wait_new_pid(pid) != 0)
    {
        fprintf(stderr, "the web server could not be reloaded\n");
        return -1;
    }
    return 0;
}

static void svc_report(DWORD state, DWORD exitCode, DWORD wait)
{
    static DWORD checkpoint = 1;
    g_svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svc_status.dwCurrentState = state;
    g_svc_status.dwWin32ExitCode = exitCode;
    g_svc_status.dwServiceSpecificExitCode = exitCode == ERROR_SERVICE_SPECIFIC_ERROR ? 1 : 0;
    g_svc_status.dwWaitHint = wait;
    g_svc_status.dwControlsAccepted = state == SERVICE_START_PENDING ? 0
                                      : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
        g_svc_status.dwCheckPoint = 0;
    else
        g_svc_status.dwCheckPoint = checkpoint++;
    SetServiceStatus(g_svc, &g_svc_status);
}

static DWORD WINAPI svc_handler(DWORD ctrl, DWORD type, LPVOID data, LPVOID ctx)
{
    (void)type; (void)data; (void)ctx;
    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        svc_report(SERVICE_STOP_PENDING, NO_ERROR, 70000);
        SetEvent(g_evt_stop);
        return NO_ERROR;
    case OLS_CTRL_RELOAD:
        SetEvent(g_evt_reload);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

static HANDLE spawn_server(HANDLE job, pid_t *pid)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 16];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    if (exe_path(exe, MAX_PATH) < 0)
        return NULL;
    swprintf(cmd, MAX_PATH + 16, L"\"%s\" -d", exe);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        NULL, NULL, &si, &pi))
        return NULL;
    if (job && !AssignProcessToJobObject(job, pi.hProcess))
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return NULL;
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    *pid = (pid_t)pi.dwProcessId;
    return pi.hProcess;
}

static HANDLE adopt_server(pid_t pid)
{
    if (!server_alive(pid))
        return NULL;
    return OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE,
                       (DWORD)pid);
}

static void stop_server(pid_t pid, HANDLE h)
{
    int i;
    pid_t npid;
    kill(pid, SIGTERM);
    for (i = 0; i < 12; ++i)
    {
        if (WaitForSingleObject(h, 5000) == WAIT_OBJECT_0)
            break;
        svc_report(SERVICE_STOP_PENDING, NO_ERROR, 70000);
    }
    if (WaitForSingleObject(h, 0) != WAIT_OBJECT_0)
        TerminateProcess(h, 1);
    npid = read_pid();
    if (npid != pid && server_alive(npid))
    {
        HANDLE nh = adopt_server(npid);
        kill(npid, SIGTERM);
        if (nh)
        {
            if (WaitForSingleObject(nh, 10000) != WAIT_OBJECT_0)
                TerminateProcess(nh, 1);
            CloseHandle(nh);
        }
    }
}

static void WINAPI svc_main(DWORD argc, LPWSTR *argv)
{
    HANDLE job, h;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
    pid_t pid;
    DWORD exitCode = NO_ERROR;
    ULONGLONG crashStart = 0;
    int crashes = 0;
    (void)argc; (void)argv;
    g_svc = RegisterServiceCtrlHandlerExW(OLS_SERVICE_NAME, svc_handler, NULL);
    if (!g_svc)
        return;
    svc_report(SERVICE_START_PENDING, NO_ERROR, 30000);
    g_evt_stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_evt_reload = CreateEventW(NULL, FALSE, FALSE, NULL);
    job = CreateJobObjectW(NULL, NULL);
    if (job)
    {
        memset(&li, 0, sizeof(li));
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
    }
    pid = read_pid();
    h = adopt_server(pid);
    if (!h)
        h = spawn_server(job, &pid);
    if (!h || !g_evt_stop || !g_evt_reload)
    {
        svc_report(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, 0);
        return;
    }
    svc_report(SERVICE_RUNNING, NO_ERROR, 0);
    for (;;)
    {
        HANDLE w[3] = { g_evt_stop, g_evt_reload, h };
        DWORD r = WaitForMultipleObjects(3, w, FALSE, INFINITE), code = 0;
        HANDLE nh = NULL;
        int i;
        if (r == WAIT_OBJECT_0)
        {
            stop_server(pid, h);
            break;
        }
        if (r == WAIT_OBJECT_0 + 1)
        {
            kill(pid, SIGUSR1);
            continue;
        }
        GetExitCodeProcess(h, &code);
        for (i = 0; i < 20 && !nh; ++i)
        {
            pid_t npid = read_pid();
            if (npid != pid)
                nh = adopt_server(npid);
            if (nh)
                pid = npid;
            else if (WaitForSingleObject(g_evt_stop, 100) == WAIT_OBJECT_0)
                break;
        }
        CloseHandle(h);
        h = nh;
        if (h)
            continue;
        if (WaitForSingleObject(g_evt_stop, 0) == WAIT_OBJECT_0 || code == 0)
            break;
        if (GetTickCount64() - crashStart > 60000)
        {
            crashStart = GetTickCount64();
            crashes = 0;
        }
        if (++crashes > 5)
        {
            exitCode = ERROR_SERVICE_SPECIFIC_ERROR;
            break;
        }
        if (WaitForSingleObject(g_evt_stop, 1000) == WAIT_OBJECT_0)
            break;
        h = spawn_server(job, &pid);
        if (!h)
        {
            exitCode = ERROR_SERVICE_SPECIFIC_ERROR;
            break;
        }
    }
    if (h)
        CloseHandle(h);
    if (job)
        CloseHandle(job);
    svc_report(SERVICE_STOPPED, exitCode, 0);
}

int ols_win_service_run(void)
{
    SERVICE_TABLE_ENTRYW table[] = { { (LPWSTR)OLS_SERVICE_NAME, svc_main }, { NULL, NULL } };
    if (!StartServiceCtrlDispatcherW(table))
    {
        fprintf(stderr, "--service must be started by the Service Control Manager (error %lu)\n",
                GetLastError());
        return 1;
    }
    return 0;
}

static int set_dacl(const wchar_t *path, const wchar_t *sddl, int protect)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    PACL dacl = NULL, empty = NULL;
    BOOL present = FALSE, def = FALSE;
    BYTE ebuf[sizeof(ACL)];
    DWORD r;
    if (sddl)
    {
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL))
            return -1;
        GetSecurityDescriptorDacl(sd, &present, &dacl, &def);
    }
    else
    {
        empty = (PACL)ebuf;
        InitializeAcl(empty, sizeof(ebuf), ACL_REVISION);
        dacl = empty;
    }
    r = SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION
                              | (protect ? PROTECTED_DACL_SECURITY_INFORMATION
                                         : UNPROTECTED_DACL_SECURITY_INFORMATION),
                              NULL, NULL, dacl, NULL);
    if (sd)
        LocalFree(sd);
    return r == ERROR_SUCCESS ? 0 : -1;
}

static const wchar_t *const s_rw_dirs[] = {
    L"logs", L"tmp", L"conf", L"cachedata", L"admin\\conf", L"admin\\tmp", L"admin\\logs", NULL
};

static int is_protected(const wchar_t *path)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    SECURITY_DESCRIPTOR_CONTROL ctl = 0;
    DWORD rev;
    int r = 0;
    if (GetNamedSecurityInfoW(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, NULL,
                              NULL, &sd) != ERROR_SUCCESS)
        return 0;
    if (GetSecurityDescriptorControl(sd, &ctl, &rev))
        r = (ctl & SE_DACL_PROTECTED) != 0;
    LocalFree(sd);
    return r;
}

static int reset_private(const wchar_t *dir, int depth)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int rc = 0;
    if (depth > 32 || swprintf(pat, MAX_PATH, L"%s\\*", dir) < 0)
        return -1;
    f = FindFirstFileExW(pat, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        if ((fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0
             || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
            || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        if (swprintf(p, MAX_PATH, L"%s\\%s", dir, fd.cFileName) < 0)
        {
            rc = -1;
            continue;
        }
        if (is_protected(p) && set_dacl(p, NULL, 0) != 0)
            rc = -1;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && reset_private(p, depth + 1) != 0)
            rc = -1;
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
    return rc;
}

static void for_each_vhost_logs(const wchar_t *root, const wchar_t *sddl)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    swprintf(pat, MAX_PATH, L"%s\\*", root);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do
    {
        DWORD a;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || fd.cFileName[0] == L'.')
            continue;
        swprintf(p, MAX_PATH, L"%s\\%s\\logs", root, fd.cFileName);
        a = GetFileAttributesW(p);
        if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)
            && !(a & FILE_ATTRIBUTE_REPARSE_POINT))
            set_dacl(p, sddl, 0);
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
}

static int apply_acls(const wchar_t *root, const wchar_t *sid)
{
    wchar_t sddl[512], p[MAX_PATH];
    const wchar_t *me = ols_user_sid();
    int i, rc = 0;
    if (sid)
        swprintf(sddl, 512, L"D:PAI(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;0x1200a9;;;%s)", sid);
    else if (me)
        swprintf(sddl, 512, L"D:PAI(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;%s)", me);
    if (!sid && !me)
        return -1;
    if (set_dacl(root, sddl, 1) != 0)
        rc = -1;
    if (sid)
        swprintf(sddl, 512, L"D:AI(A;OICI;0x1301bf;;;%s)", sid);
    for (i = 0; s_rw_dirs[i]; ++i)
    {
        swprintf(p, MAX_PATH, L"%s\\%s", root, s_rw_dirs[i]);
        if (sid)
            CreateDirectoryW(p, NULL);
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES && set_dacl(p, sid ? sddl : NULL, 0) != 0)
            rc = -1;
    }
    for_each_vhost_logs(root, sid ? sddl : NULL);
    if (reset_private(root, 0) != 0)
        rc = -1;
    return rc;
}

static int service_sid(wchar_t *out, size_t len)
{
    BYTE sid[SECURITY_MAX_SID_SIZE];
    DWORD n = sizeof(sid), dn = 64;
    wchar_t dom[64], *str;
    SID_NAME_USE use;
    if (!LookupAccountNameW(NULL, OLS_SERVICE_ACCOUNT, sid, &n, dom, &dn, &use))
        return -1;
    if (!ConvertSidToStringSidW(sid, &str))
        return -1;
    if (wcslen(str) >= len)
    {
        LocalFree(str);
        return -1;
    }
    wcscpy(out, str);
    LocalFree(str);
    return 0;
}

int ols_win_service_install(int install)
{
    SC_HANDLE scm, svc;
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 32], root[MAX_PATH], sid[192];
    SERVICE_DESCRIPTIONW desc;
    SERVICE_FAILURE_ACTIONSW fa;
    SERVICE_SID_INFO si;
    SERVICE_REQUIRED_PRIVILEGES_INFOW rp;
    SC_ACTION actions[3];
    if (exe_path(exe, MAX_PATH) < 0 || root_path(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "cannot determine the installation directory\n");
        return 1;
    }
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
    {
        fprintf(stderr, "cannot open service manager (error %lu), run as Administrator\n",
                GetLastError());
        return 1;
    }
    if (!install)
    {
        SERVICE_STATUS st;
        union { QUERY_SERVICE_CONFIGW c; unsigned char b[8192]; } qc;
        DWORD need;
        svc = OpenServiceW(scm, OLS_SERVICE_NAME, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
        if (!svc)
        {
            fprintf(stderr, "service not installed\n");
            CloseServiceHandle(scm);
            return 1;
        }
        if (QueryServiceConfigW(svc, &qc.c, sizeof(qc), &need) && qc.c.lpBinaryPathName
            && _wcsnicmp(qc.c.lpBinaryPathName + (qc.c.lpBinaryPathName[0] == L'"'), exe, wcslen(exe)) != 0)
        {
            fprintf(stderr, "the service belongs to another installation: %ls\n", qc.c.lpBinaryPathName);
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return 1;
        }
        if (ControlService(svc, SERVICE_CONTROL_STOP, &st))
            svc_wait(svc, SERVICE_STOPPED, 90);
        if (!DeleteService(svc))
            fprintf(stderr, "delete failed (error %lu)\n", GetLastError());
        else
            printf("service removed\n");
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        if (apply_acls(root, NULL) != 0)
            fprintf(stderr, "warning: could not reset permissions on %ls\n", root);
        return 0;
    }
    swprintf(cmd, MAX_PATH + 32, L"\"%s\" --service", exe);
    svc = CreateServiceW(scm, OLS_SERVICE_NAME, L"OpenLiteSpeed Web Server",
                         SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                         SERVICE_ERROR_NORMAL, cmd, NULL, NULL, L"Tcpip\0Afd\0\0",
                         OLS_SERVICE_ACCOUNT, NULL);
    if (!svc && GetLastError() == ERROR_SERVICE_EXISTS)
    {
        svc = OpenServiceW(scm, OLS_SERVICE_NAME, SERVICE_ALL_ACCESS);
        if (svc && !ChangeServiceConfigW(svc, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                                         SERVICE_ERROR_NORMAL, cmd, NULL, NULL, L"Tcpip\0Afd\0\0",
                                         OLS_SERVICE_ACCOUNT, L"", NULL))
        {
            CloseServiceHandle(svc);
            svc = NULL;
        }
    }
    if (!svc)
    {
        fprintf(stderr, "create service failed (error %lu)\n", GetLastError());
        CloseServiceHandle(scm);
        return 1;
    }
    desc.lpDescription = (LPWSTR)L"OpenLiteSpeed HTTP server for Windows";
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);
    si.dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED;
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_SERVICE_SID_INFO, &si);
    rp.pmszRequiredPrivileges = (LPWSTR)L"SeChangeNotifyPrivilege\0";
    if (!ChangeServiceConfig2W(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &rp))
        fprintf(stderr, "warning: could not limit service privileges (error %lu)\n", GetLastError());
    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 5000;
    actions[1].Type = SC_ACTION_RESTART;
    actions[1].Delay = 15000;
    actions[2].Type = SC_ACTION_NONE;
    actions[2].Delay = 0;
    memset(&fa, 0, sizeof(fa));
    fa.dwResetPeriod = 86400;
    fa.cActions = 3;
    fa.lpsaActions = actions;
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (service_sid(sid, sizeof(sid) / sizeof(sid[0])) != 0 || apply_acls(root, sid) != 0)
    {
        fprintf(stderr, "service installed but setting permissions on %ls failed (error %lu)\n",
                root, GetLastError());
        return 1;
    }
    printf("service installed: %ls (runs as %ls)\n", OLS_SERVICE_NAME, OLS_SERVICE_ACCOUNT);
    return 0;
}
