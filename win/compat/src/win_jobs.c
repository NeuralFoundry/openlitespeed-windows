#include "compat_internal.h"

#define JOBS_MAX 64
#define JOB_CMD 4000
#define JOB_LOG_LIMIT (10LL << 20)
#define JOB_GIVEUP 10

typedef struct
{
    char site[64], name[40];
    wchar_t cmd[JOB_CMD + 1];
    int autostart;
} jobcfg_t;

typedef struct
{
    char key[110];
    DWORD pid;
    int running, restarts;
} jobstate_t;

typedef struct
{
    char site[64], name[40], key[110];
    wchar_t cmd[JOB_CMD + 1], log[MAX_PATH];
    int used, present, running, failed, restarts, nhist;
    HANDLE proc, job, pump, rd;
    DWORD pid;
    ULONGLONG started, next, delay, hist[JOB_GIVEUP];
} runjob_t;

static wchar_t s_root[MAX_PATH];
static runjob_t s_run[JOBS_MAX];
static int s_nrun;

static int jb_root(wchar_t *root)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    wchar_t *p;
    if (n == 0 || n >= MAX_PATH)
        return -1;
    for (p = root; *p; ++p)
        if (*p == L'/')
            *p = L'\\';
    while (n > 3 && root[n - 1] == L'\\')
        root[--n] = 0;
    return 0;
}

static int jb_label(const char *s, size_t max)
{
    size_t n = s ? strlen(s) : 0, i;
    if (n == 0 || n > max || s[0] == '-' || s[n - 1] == '-')
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-'))
            return 0;
    return 1;
}

static int jb_name_ok(const char *s)
{
    size_t n = s ? strlen(s) : 0, i;
    if (n == 0 || n > 32)
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-'))
            return 0;
    return 1;
}

static void jb_ini(const wchar_t *root, wchar_t *ini)
{
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
}

static void jb_dir(const wchar_t *root, wchar_t *out)
{
    swprintf(out, MAX_PATH, L"%s\\tmp\\jobs", root);
}

static void jb_file(const wchar_t *root, const char *key, const wchar_t *ext, wchar_t *out)
{
    wchar_t wk[110];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wk, 110);
    swprintf(out, MAX_PATH, L"%s\\tmp\\jobs\\%s%s", root, wk, ext);
}

static void jb_logpath(const wchar_t *root, const char *site, const char *name, wchar_t *out)
{
    wchar_t ws[64], wn[40];
    MultiByteToWideChar(CP_UTF8, 0, site, -1, ws, 64);
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 40);
    swprintf(out, MAX_PATH, L"%s\\logs\\jobs\\%s.%s.log", root, ws, wn);
}

static void jb_old_log(const wchar_t *root, const char *site, const char *name, const wchar_t *log)
{
    wchar_t ws[64], wn[40], old[MAX_PATH], a[MAX_PATH + 4], b[MAX_PATH + 4];
    if (GetFileAttributesW(log) != INVALID_FILE_ATTRIBUTES)
        return;
    MultiByteToWideChar(CP_UTF8, 0, site, -1, ws, 64);
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 40);
    swprintf(old, MAX_PATH, L"%s\\logs\\jobs\\%s-%s.log", root, ws, wn);
    if (!MoveFileExW(old, log, 0))
        return;
    swprintf(a, MAX_PATH + 4, L"%s.1", old);
    swprintf(b, MAX_PATH + 4, L"%s.1", log);
    MoveFileExW(a, b, 0);
}

static int jb_site_dir(const wchar_t *root, const char *site, wchar_t *out)
{
    wchar_t ini[MAX_PATH], def[MAX_PATH], www[MAX_PATH], ws[64], p[MAX_PATH];
    DWORD a;
    size_t n;
    jb_ini(root, ini);
    swprintf(def, MAX_PATH, L"%s\\www", root);
    GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
    if (MultiByteToWideChar(CP_UTF8, 0, site, -1, ws, 64) <= 0)
        return -1;
    swprintf(p, MAX_PATH, L"%s\\%s", www, ws);
    if (!GetFullPathNameW(p, MAX_PATH, out, NULL))
        return -1;
    n = wcslen(out);
    while (n > 3 && out[n - 1] == L'\\')
        out[--n] = 0;
    a = GetFileAttributesW(out);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) ? 0 : -1;
}

static int jb_exists(const wchar_t *p)
{
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static void jb_touch(const wchar_t *p)
{
    ols_write_wfile(p, "1\n", 2, 0);
}

static int jb_config(const wchar_t *root, jobcfg_t *out, int max)
{
    wchar_t ini[MAX_PATH], *sect, *e;
    int n = 0;
    jb_ini(root, ini);
    if ((sect = (wchar_t *)malloc(262144 * sizeof(wchar_t))) == NULL)
        return 0;
    if (GetPrivateProfileSectionW(L"jobs", sect, 262144, ini) > 0)
        for (e = sect; *e && n < max; e += wcslen(e) + 1)
        {
            wchar_t *eq = wcschr(e, L'='), *v;
            char key[128], *dot;
            jobcfg_t *j = &out[n];
            if (!eq || eq - e >= 100)
                continue;
            *eq = 0;
            v = eq + 1;
            if (WideCharToMultiByte(CP_UTF8, 0, e, -1, key, sizeof(key), NULL, NULL) <= 0
                || (dot = strchr(key, '.')) == NULL)
                continue;
            *dot = 0;
            if (!jb_label(key, 63) || !jb_name_ok(dot + 1) || (v[0] != L'0' && v[0] != L'1') || v[1] != L' ' || !v[2]
                || wcslen(v + 2) > JOB_CMD)
                continue;
            strcpy(j->site, key);
            strcpy(j->name, dot + 1);
            j->autostart = v[0] == L'1';
            wcscpy(j->cmd, v + 2);
            ++n;
        }
    free(sect);
    return n;
}

static int jb_find(const jobcfg_t *c, int n, const char *site, const char *name)
{
    int i;
    for (i = 0; i < n; ++i)
        if (strcmp(c[i].site, site) == 0 && strcmp(c[i].name, name) == 0)
            return i;
    return -1;
}

static void jb_key(const char *site, const char *name, char *key)
{
    snprintf(key, 110, "%s.%s", site, name);
}

static int jb_read(const wchar_t *path, char **out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got = 0;
    *out = NULL;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > (1u << 20) || (*out = (char *)malloc(size + 1)) == NULL
        || !ReadFile(h, *out, size, &got, NULL))
    {
        CloseHandle(h);
        free(*out);
        *out = NULL;
        return -1;
    }
    CloseHandle(h);
    (*out)[got] = 0;
    return 0;
}

static DWORD jb_pidfile(const wchar_t *path)
{
    char *t = NULL;
    DWORD pid = 0;
    if (jb_read(path, &t) == 0)
    {
        pid = (DWORD)strtoul(t, NULL, 10);
        free(t);
    }
    return pid && ols_pid_is_ours((pid_t)pid) ? pid : 0;
}

static DWORD jb_super(const wchar_t *root)
{
    wchar_t f[MAX_PATH];
    swprintf(f, MAX_PATH, L"%s\\tmp\\jobs\\supervisor.pid", root);
    return jb_pidfile(f);
}

static int jb_states(const wchar_t *root, jobstate_t *st, int max)
{
    wchar_t f[MAX_PATH];
    char *t = NULL, *p, *e;
    int n = 0;
    swprintf(f, MAX_PATH, L"%s\\tmp\\jobs\\state", root);
    if (jb_read(f, &t) != 0)
        return 0;
    for (p = t; *p && n < max; p = e)
    {
        unsigned long pid;
        int running, restarts;
        char key[110];
        e = strchr(p, '\n');
        e = e ? e + 1 : p + strlen(p);
        if (sscanf(p, "%109s %lu %d %d", key, &pid, &running, &restarts) == 4)
        {
            strcpy(st[n].key, key);
            st[n].pid = (DWORD)pid;
            st[n].running = running;
            st[n].restarts = restarts;
            ++n;
        }
    }
    free(t);
    return n;
}

static const jobstate_t *jb_state_of(const jobstate_t *st, int n, const char *key)
{
    int i;
    for (i = 0; i < n; ++i)
        if (strcmp(st[i].key, key) == 0)
            return &st[i];
    return NULL;
}

int ols_helper_spawn(const wchar_t *args)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 64];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return -1;
    swprintf(cmd, MAX_PATH + 64, L"\"%s\" %s", exe, args);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, NULL,
                        NULL, &si, &pi))
        return -1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

static wchar_t *jb_env(const wchar_t *root, const wchar_t *php, const wchar_t *node)
{
    wchar_t *blk = GetEnvironmentStringsW(), *e, *env, add[MAX_PATH * 5];
    const wchar_t *path = L"";
    size_t total = 0, pos = 0, al = 0;
    wchar_t d[MAX_PATH];
    DWORD a;
    add[0] = 0;
    if (php[0])
        al += (size_t)swprintf(add + al, MAX_PATH * 5 - al, L"%s;", php);
    if (node[0])
    {
        wcscpy(d, node);
        if (wcsrchr(d, L'\\'))
            *wcsrchr(d, L'\\') = 0;
        al += (size_t)swprintf(add + al, MAX_PATH * 5 - al, L"%s;", d);
    }
    swprintf(d, MAX_PATH, L"%s\\tools\\composer", root);
    if ((a = GetFileAttributesW(d)) != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY))
        al += (size_t)swprintf(add + al, MAX_PATH * 5 - al, L"%s;", d);
    swprintf(d, MAX_PATH, L"%s\\mariadb\\bin", root);
    if ((a = GetFileAttributesW(d)) != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY))
        al += (size_t)swprintf(add + al, MAX_PATH * 5 - al, L"%s;", d);
    for (e = blk; e && *e; e += wcslen(e) + 1)
    {
        if (_wcsnicmp(e, L"PATH=", 5) == 0)
            path = e + 5;
        total += wcslen(e) + 1;
    }
    if ((env = (wchar_t *)malloc((total + al + MAX_PATH + 128) * sizeof(wchar_t))) == NULL)
    {
        FreeEnvironmentStringsW(blk);
        return NULL;
    }
    for (e = blk; e && *e; e += wcslen(e) + 1)
        if (_wcsnicmp(e, L"PATH=", 5) != 0 && _wcsnicmp(e, L"PHPRC=", 6) != 0
            && _wcsnicmp(e, L"NoDefaultCurrentDirectoryInExePath=", 35) != 0 && _wcsnicmp(e, L"OLS_", 4) != 0)
        {
            wcscpy(env + pos, e);
            pos += wcslen(e) + 1;
        }
    pos += (size_t)swprintf(env + pos, total + al + MAX_PATH + 128 - pos, L"PATH=%s%s", add, path) + 1;
    pos += (size_t)swprintf(env + pos, total + al + MAX_PATH + 128 - pos, L"NoDefaultCurrentDirectoryInExePath=1") + 1;
    if (php[0])
        pos += (size_t)swprintf(env + pos, total + al + MAX_PATH + 128 - pos, L"PHPRC=%s", php) + 1;
    env[pos] = 0;
    FreeEnvironmentStringsW(blk);
    return env;
}

static DWORD WINAPI jb_pump_main(LPVOID arg)
{
    runjob_t *j = (runjob_t *)arg;
    static const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE log = CreateFileW(j->log, FILE_APPEND_DATA, share, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    char *buf = (char *)malloc(65536);
    DWORD got, w;
    LARGE_INTEGER sz;
    wchar_t old[MAX_PATH + 4];
    while (buf && ReadFile(j->rd, buf, 65536, &got, NULL) && got)
    {
        if (log != INVALID_HANDLE_VALUE && GetFileSizeEx(log, &sz) && sz.QuadPart + got > JOB_LOG_LIMIT)
        {
            CloseHandle(log);
            swprintf(old, MAX_PATH + 4, L"%s.1", j->log);
            MoveFileExW(j->log, old, MOVEFILE_REPLACE_EXISTING);
            log = CreateFileW(j->log, FILE_APPEND_DATA, share, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (log != INVALID_HANDLE_VALUE)
            WriteFile(log, buf, got, &w, NULL);
    }
    if (log != INVALID_HANDLE_VALUE)
        CloseHandle(log);
    free(buf);
    return 0;
}

static int jb_spawn(runjob_t *j)
{
    wchar_t ini[MAX_PATH], dir[MAX_PATH], php[MAX_PATH] = L"", node[MAX_PATH] = L"", sp[MAX_PATH], sys[MAX_PATH],
        cmdexe[MAX_PATH], *cmd, *env;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd = NULL, wr = NULL, nul;
    const char *from;
    size_t n;
    int ok;
    jb_ini(s_root, ini);
    if (jb_site_dir(s_root, j->site, dir) != 0 || GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return -1;
    if (ols_site_php(ini, j->site, sp, MAX_PATH) == 0 && GetFullPathNameW(sp, MAX_PATH, php, NULL))
    {
        n = wcslen(php);
        while (n > 3 && php[n - 1] == L'\\')
            php[--n] = 0;
    }
    else if (ols_php_current(s_root, php) != 0)
        php[0] = 0;
    if (php[0] && GetFileAttributesW(php) == INVALID_FILE_ATTRIBUTES)
        php[0] = 0;
    ols_site_node(s_root, ini, j->site, dir, node, MAX_PATH, &from);
    swprintf(cmdexe, MAX_PATH, L"%s\\cmd.exe", sys);
    if ((cmd = (wchar_t *)malloc((wcslen(j->cmd) + MAX_PATH + 32) * sizeof(wchar_t))) == NULL)
        return -1;
    swprintf(cmd, wcslen(j->cmd) + MAX_PATH + 32, L"\"%s\" /d /s /c \"%s\"", cmdexe, j->cmd);
    if ((env = jb_env(s_root, php, node)) == NULL || !CreatePipe(&rd, &wr, &sa, 0))
    {
        free(cmd);
        free(env);
        return -1;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    j->job = CreateJobObjectW(NULL, NULL);
    if (j->job)
    {
        memset(&li, 0, sizeof(li));
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(j->job, JobObjectExtendedLimitInformation, &li, sizeof(li));
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    ok = j->job && CreateProcessW(cmdexe, cmd, NULL, NULL, TRUE,
                                  CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT,
                                  env, dir, &si, &pi);
    if (ok && !AssignProcessToJobObject(j->job, pi.hProcess))
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        ok = 0;
    }
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    free(cmd);
    free(env);
    if (!ok)
    {
        CloseHandle(rd);
        if (j->job)
            CloseHandle(j->job);
        j->job = NULL;
        return -1;
    }
    j->rd = rd;
    j->proc = pi.hProcess;
    j->pid = pi.dwProcessId;
    jb_old_log(s_root, j->site, j->name, j->log);
    j->pump = CreateThread(NULL, 65536, jb_pump_main, j, 0, NULL);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    j->running = 1;
    j->started = GetTickCount64();
    return 0;
}

static void jb_reap(runjob_t *j)
{
    if (j->job)
        TerminateJobObject(j->job, 1);
    if (j->proc)
    {
        WaitForSingleObject(j->proc, 10000);
        CloseHandle(j->proc);
    }
    if (j->pump)
    {
        WaitForSingleObject(j->pump, 5000);
        CloseHandle(j->pump);
    }
    if (j->rd)
        CloseHandle(j->rd);
    if (j->job)
        CloseHandle(j->job);
    j->proc = j->pump = j->rd = j->job = NULL;
    j->running = 0;
    j->pid = 0;
}

static void jb_write_states(void)
{
    wchar_t f[MAX_PATH], tmp[MAX_PATH];
    char *buf = (char *)malloc((size_t)JOBS_MAX * 160 + 1);
    size_t o = 0;
    int i;
    if (!buf)
        return;
    buf[0] = 0;
    for (i = 0; i < s_nrun; ++i)
        if (s_run[i].used)
            o += (size_t)snprintf(buf + o, (size_t)JOBS_MAX * 160 + 1 - o, "%s %lu %d %d\n", s_run[i].key,
                                  (unsigned long)s_run[i].pid, s_run[i].running, s_run[i].restarts);
    swprintf(f, MAX_PATH, L"%s\\tmp\\jobs\\state", s_root);
    swprintf(tmp, MAX_PATH, L"%s\\tmp\\jobs\\state.tmp", s_root);
    if (ols_write_wfile(tmp, buf, o, 0) == 0 && !MoveFileExW(tmp, f, MOVEFILE_REPLACE_EXISTING))
        DeleteFileW(tmp);
    free(buf);
}

static int jb_sync(const jobcfg_t *c, int n)
{
    int i, k, f, changed = 0;
    for (i = 0; i < s_nrun; ++i)
        s_run[i].present = 0;
    for (k = 0; k < n; ++k)
    {
        char key[110];
        jb_key(c[k].site, c[k].name, key);
        for (i = 0, f = -1; i < s_nrun && !(s_run[i].used && strcmp(s_run[i].key, key) == 0); ++i)
            if (!s_run[i].used && f < 0)
                f = i;
        if (i == s_nrun)
        {
            if (f < 0 && s_nrun == JOBS_MAX)
                continue;
            i = f >= 0 ? f : s_nrun++;
            memset(&s_run[i], 0, sizeof(s_run[i]));
            s_run[i].used = 1;
            strcpy(s_run[i].key, key);
            strcpy(s_run[i].site, c[k].site);
            strcpy(s_run[i].name, c[k].name);
            jb_logpath(s_root, c[k].site, c[k].name, s_run[i].log);
            s_run[i].delay = 1000;
            changed = 1;
        }
        wcscpy(s_run[i].cmd, c[k].cmd);
        s_run[i].present = 1;
    }
    for (i = 0; i < s_nrun; ++i)
        if (s_run[i].used && !s_run[i].present && !s_run[i].running)
        {
            s_run[i].used = 0;
            changed = 1;
        }
    return changed;
}

static int jb_supervise(const wchar_t *root)
{
    wchar_t dir[MAX_PATH], lockf[MAX_PATH], pidf[MAX_PATH], stopf[MAX_PATH], logs[MAX_PATH], f[MAX_PATH];
    static jobcfg_t cfg[JOBS_MAX];
    HANDLE lock;
    ULONGLONG lastcfg = 0;
    char buf[32];
    int n, i, ncfg = 0, dirty = 1;
    wcscpy(s_root, root);
    jb_dir(root, dir);
    swprintf(f, MAX_PATH, L"%s\\tmp", root);
    CreateDirectoryW(f, NULL);
    CreateDirectoryW(dir, NULL);
    swprintf(logs, MAX_PATH, L"%s\\logs", root);
    CreateDirectoryW(logs, NULL);
    swprintf(logs, MAX_PATH, L"%s\\logs\\jobs", root);
    CreateDirectoryW(logs, NULL);
    swprintf(lockf, MAX_PATH, L"%s\\supervisor.lock", dir);
    swprintf(pidf, MAX_PATH, L"%s\\supervisor.pid", dir);
    swprintf(stopf, MAX_PATH, L"%s\\stop", dir);
    lock = CreateFileW(lockf, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (lock == INVALID_HANDLE_VALUE)
        return 1;
    DeleteFileW(stopf);
    n = snprintf(buf, sizeof(buf), "%lu\n", GetCurrentProcessId());
    if (ols_write_wfile(pidf, buf, (size_t)n, 0) != 0)
        return 1;
    for (;;)
    {
        ULONGLONG now = GetTickCount64();
        if (jb_exists(stopf))
            break;
        if (now - lastcfg >= 1000)
        {
            ncfg = jb_config(root, cfg, JOBS_MAX);
            if (jb_sync(cfg, ncfg))
                dirty = 1;
            lastcfg = now;
        }
        for (i = 0; i < s_nrun; ++i)
        {
            runjob_t *j = &s_run[i];
            wchar_t wantf[MAX_PATH], restf[MAX_PATH];
            int want;
            if (!j->used)
                continue;
            jb_file(root, j->key, L".want", wantf);
            jb_file(root, j->key, L".restart", restf);
            want = j->present && jb_exists(wantf);
            if (!j->present && jb_exists(wantf))
                DeleteFileW(wantf);
            if (jb_exists(restf))
            {
                DeleteFileW(restf);
                if (j->running)
                    jb_reap(j);
                j->failed = 0;
                j->restarts = 0;
                j->nhist = 0;
                j->delay = 1000;
                j->next = 0;
                dirty = 1;
            }
            if (j->running && WaitForSingleObject(j->proc, 0) == WAIT_OBJECT_0)
            {
                ULONGLONG ran = now - j->started;
                int k, recent = 0;
                jb_reap(j);
                dirty = 1;
                if (ran >= 300000)
                    j->delay = 1000;
                for (k = 0; k < j->nhist; ++k)
                    if (now - j->hist[k] < 600000)
                        j->hist[recent++] = j->hist[k];
                j->nhist = recent;
                if (want && j->nhist >= JOB_GIVEUP)
                {
                    j->failed = 1;
                    DeleteFileW(wantf);
                    want = 0;
                }
                else if (want)
                {
                    j->hist[j->nhist++] = now;
                    ++j->restarts;
                    j->next = now + j->delay;
                    j->delay = j->delay >= 30000 ? 60000 : j->delay * 2;
                }
            }
            if (j->running && !want)
            {
                jb_reap(j);
                dirty = 1;
            }
            else if (!j->running && want && !j->failed && now >= j->next)
            {
                if (jb_spawn(j) != 0)
                {
                    j->hist[j->nhist < JOB_GIVEUP ? j->nhist++ : JOB_GIVEUP - 1] = now;
                    ++j->restarts;
                    j->next = now + j->delay;
                    j->delay = j->delay >= 30000 ? 60000 : j->delay * 2;
                    if (j->nhist >= JOB_GIVEUP)
                    {
                        j->failed = 1;
                        DeleteFileW(wantf);
                    }
                }
                dirty = 1;
            }
        }
        if (dirty)
        {
            jb_write_states();
            dirty = 0;
        }
        Sleep(250);
    }
    for (i = 0; i < s_nrun; ++i)
        if (s_run[i].used && s_run[i].running)
            jb_reap(&s_run[i]);
    jb_write_states();
    DeleteFileW(stopf);
    DeleteFileW(pidf);
    CloseHandle(lock);
    return 0;
}

static int jb_wait(const wchar_t *root, const char *key, int running, int ms)
{
    static jobstate_t st[JOBS_MAX];
    int i, n;
    const jobstate_t *s;
    for (i = 0; i <= ms / 200; ++i)
    {
        n = jb_states(root, st, JOBS_MAX);
        s = jb_state_of(st, n, key);
        if (running ? (s && (s->running || s->restarts > 0) && jb_super(root)) : (!s || !s->running || !jb_super(root)))
            return 0;
        Sleep(200);
    }
    return -1;
}

static int jb_kick(const wchar_t *root)
{
    int svc = ols_service_mode();
    if (jb_super(root))
        return 0;
    if (svc == 0)
        return ols_helper_spawn(L"--job run");
    if (svc == 2)
        return ols_win_reload_running() < 0 ? -1 : 0;
    return 1;
}

int ols_jobs_ensure(const wchar_t *root, int autostart)
{
    static jobcfg_t cfg[JOBS_MAX];
    wchar_t dir[MAX_PATH], f[MAX_PATH];
    int n = jb_config(root, cfg, JOBS_MAX), i;
    char key[110];
    if (n == 0)
        return 0;
    if (autostart || !jb_super(root))
    {
        swprintf(f, MAX_PATH, L"%s\\tmp", root);
        CreateDirectoryW(f, NULL);
        jb_dir(root, dir);
        CreateDirectoryW(dir, NULL);
        for (i = 0; i < n; ++i)
            if (cfg[i].autostart)
            {
                jb_key(cfg[i].site, cfg[i].name, key);
                jb_file(root, key, L".want", f);
                jb_touch(f);
            }
    }
    if (jb_super(root))
        return 0;
    return ols_helper_spawn(L"--job run");
}

int ols_jobs_halt(const wchar_t *root)
{
    wchar_t f[MAX_PATH], pat[MAX_PATH], dir[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    DWORD pid = jb_super(root);
    int i;
    jb_dir(root, dir);
    swprintf(pat, MAX_PATH, L"%s\\*.want", dir);
    h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            swprintf(f, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
            DeleteFileW(f);
        }
        while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (!pid)
        return 0;
    swprintf(f, MAX_PATH, L"%s\\stop", dir);
    jb_touch(f);
    for (i = 0; i < 100 && jb_super(root); ++i)
        Sleep(200);
    if (jb_super(root) && (h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid)) != NULL)
    {
        TerminateProcess(h, 1);
        WaitForSingleObject(h, 10000);
        CloseHandle(h);
    }
    DeleteFileW(f);
    return jb_super(root) ? -1 : 0;
}

void ols_jobs_summary(const wchar_t *root)
{
    static jobcfg_t cfg[JOBS_MAX];
    static jobstate_t st[JOBS_MAX];
    int n = jb_config(root, cfg, JOBS_MAX), ns = n && jb_super(root) ? jb_states(root, st, JOBS_MAX) : 0, i, run = 0;
    char key[110];
    const jobstate_t *s;
    for (i = 0; i < n; ++i)
    {
        jb_key(cfg[i].site, cfg[i].name, key);
        if ((s = jb_state_of(st, ns, key)) != NULL && s->running)
            ++run;
    }
    printf(",\"jobs\":{\"total\":%d,\"running\":%d}", n, run);
}

int ols_jobs_forget_site(const wchar_t *root, const char *site)
{
    static jobcfg_t cfg[JOBS_MAX];
    wchar_t ini[MAX_PATH], wk[110], f[MAX_PATH];
    char key[110];
    int n = jb_config(root, cfg, JOBS_MAX), i;
    jb_ini(root, ini);
    for (i = 0; i < n; ++i)
        if (strcmp(cfg[i].site, site) == 0)
        {
            jb_key(cfg[i].site, cfg[i].name, key);
            MultiByteToWideChar(CP_UTF8, 0, key, -1, wk, 110);
            WritePrivateProfileStringW(L"jobs", wk, NULL, ini);
            jb_file(root, key, L".want", f);
            DeleteFileW(f);
        }
    return 0;
}

static int jb_list(const wchar_t *root)
{
    static jobcfg_t cfg[JOBS_MAX];
    static jobstate_t st[JOBS_MAX];
    wchar_t dir[MAX_PATH], log[MAX_PATH];
    int n = jb_config(root, cfg, JOBS_MAX), ns = jb_states(root, st, JOBS_MAX), up = n && jb_super(root), i;
    char key[110];
    const jobstate_t *s;
    printf("[");
    for (i = 0; i < n; ++i)
    {
        jobcfg_t *c = &cfg[i];
        int running;
        jb_key(c->site, c->name, key);
        s = jb_state_of(st, ns, key);
        running = up && s && s->running;
        if (jb_site_dir(root, c->site, dir) != 0)
            dir[0] = 0;
        jb_logpath(root, c->site, c->name, log);
        printf("%s{\"site\":", i ? "," : "");
        ols_jstr(c->site);
        printf(",\"name\":");
        ols_jstr(c->name);
        printf(",\"command\":");
        ols_jwstr(c->cmd);
        printf(",\"dir\":");
        ols_jwstr(dir);
        printf(",\"running\":%s,\"pid\":%lu,\"autostart\":%s,\"restarts\":%d,\"log\":", running ? "true" : "false",
               running ? (unsigned long)s->pid : 0UL, c->autostart ? "true" : "false", s ? s->restarts : 0);
        ols_jwstr(log);
        printf("}");
    }
    printf("]\n");
    return 0;
}

static int jb_args(const wchar_t *root, int argc, char **argv, jobcfg_t *cfg, int *ncfg, int *idx, char *key)
{
    wchar_t dir[MAX_PATH];
    if (argc < 3 || !jb_label(argv[1], 63) || !jb_name_ok(argv[2]))
    {
        fprintf(stderr, "use a site name and a job name of lowercase letters, digits and '-'\n");
        return 2;
    }
    *ncfg = jb_config(root, cfg, JOBS_MAX);
    *idx = jb_find(cfg, *ncfg, argv[1], argv[2]);
    jb_key(argv[1], argv[2], key);
    if (strcmp(argv[0], "add") != 0 && *idx < 0)
    {
        fprintf(stderr, "site %s has no job named %s\n", argv[1], argv[2]);
        return 2;
    }
    if (strcmp(argv[0], "add") == 0 && jb_site_dir(root, argv[1], dir) != 0)
    {
        fprintf(stderr, "the projects folder has no site named %s\n", argv[1]);
        return 2;
    }
    return 0;
}

static int jb_save(const wchar_t *root, const char *key, int autostart, const wchar_t *cmd)
{
    wchar_t ini[MAX_PATH], wk[110], *v;
    size_t n = wcslen(cmd) + 4;
    int ok;
    jb_ini(root, ini);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wk, 110);
    if ((v = (wchar_t *)malloc(n * sizeof(wchar_t))) == NULL)
        return -1;
    swprintf(v, n, L"%c %s", autostart ? L'1' : L'0', cmd);
    ok = WritePrivateProfileStringW(L"jobs", wk, v, ini);
    free(v);
    return ok ? 0 : -1;
}

static int jb_start(const wchar_t *root, const char *key, const char *site, const char *name, int fresh)
{
    wchar_t f[MAX_PATH], log[MAX_PATH];
    int r;
    swprintf(f, MAX_PATH, L"%s\\tmp", root);
    CreateDirectoryW(f, NULL);
    jb_dir(root, f);
    CreateDirectoryW(f, NULL);
    jb_file(root, key, L".want", f);
    jb_touch(f);
    if (fresh)
    {
        jb_file(root, key, L".restart", f);
        jb_touch(f);
    }
    if ((r = jb_kick(root)) < 0)
    {
        fprintf(stderr, "the job supervisor did not start\n");
        return 1;
    }
    if (r == 1)
        return 0;
    for (r = 0; fresh && r < 50 && jb_exists(f); ++r)
        Sleep(200);
    if (jb_wait(root, key, 1, 10000) != 0)
    {
        jb_logpath(root, site, name, log);
        fprintf(stderr, "job %s of %s did not start; see %ls\n", name, site, log);
        return 1;
    }
    return 0;
}

int ols_job_main(int argc, char **argv)
{
    static jobcfg_t cfg[JOBS_MAX];
    static jobstate_t st[JOBS_MAX];
    wchar_t root[MAX_PATH], ini[MAX_PATH], f[MAX_PATH], wk[110];
    const char *act = argc >= 1 ? argv[0] : "";
    char key[110];
    int n, idx, r;
    if (jb_root(root) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    jb_ini(root, ini);
    if (strcmp(act, "run") == 0)
        return jb_supervise(root);
    if (strcmp(act, "list") == 0)
        return jb_list(root);
    if (strcmp(act, "add") == 0 && argc >= 4)
    {
        wchar_t *cmd;
        size_t total = 0;
        int i, len;
        if ((r = jb_args(root, argc, argv, cfg, &n, &idx, key)) != 0)
            return r;
        if (idx >= 0)
        {
            fprintf(stderr, "site %s already has a job named %s\n", argv[1], argv[2]);
            return 2;
        }
        if (n >= JOBS_MAX)
        {
            fprintf(stderr, "there are already %d jobs\n", JOBS_MAX);
            return 2;
        }
        for (i = 3; i < argc; ++i)
            total += strlen(argv[i]) + 1;
        if ((cmd = (wchar_t *)calloc(total + 1, sizeof(wchar_t))) == NULL)
            return 1;
        for (i = 3, len = 0; i < argc; ++i)
        {
            if (len)
                cmd[len++] = L' ';
            len += MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, cmd + len, (int)(total + 1 - (size_t)len)) - 1;
        }
        while (len && cmd[len - 1] == L' ')
            cmd[--len] = 0;
        if (!len || len > JOB_CMD || wcspbrk(cmd, L"\r\n") || cmd[0] == L' ')
        {
            free(cmd);
            fprintf(stderr, "the command must be one line of at most %d characters\n", JOB_CMD);
            return 2;
        }
        r = jb_save(root, key, 1, cmd);
        free(cmd);
        if (r != 0)
        {
            fprintf(stderr, "cannot write %ls\n", ini);
            return 1;
        }
        printf("job %s added to %s\n", argv[2], argv[1]);
        if (ols_server_running())
            return jb_start(root, key, argv[1], argv[2], 1);
        return 0;
    }
    if ((strcmp(act, "remove") == 0 || strcmp(act, "start") == 0 || strcmp(act, "stop") == 0
         || strcmp(act, "restart") == 0) && argc >= 3)
    {
        if ((r = jb_args(root, argc, argv, cfg, &n, &idx, key)) != 0)
            return r;
        if (strcmp(act, "start") == 0 || strcmp(act, "restart") == 0)
        {
            const jobstate_t *s;
            int ns = jb_super(root) ? jb_states(root, st, JOBS_MAX) : 0, fresh = 1;
            if (strcmp(act, "start") == 0 && (s = jb_state_of(st, ns, key)) != NULL && s->running)
                fresh = 0;
            if (fresh && (r = jb_start(root, key, argv[1], argv[2], 1)) != 0)
                return r;
            printf("job %s of %s %s\n", argv[2], argv[1], fresh ? (act[0] == 's' ? "started" : "restarted") : "is running");
            return 0;
        }
        jb_file(root, key, L".want", f);
        DeleteFileW(f);
        if (strcmp(act, "remove") == 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, key, -1, wk, 110);
            WritePrivateProfileStringW(L"jobs", wk, NULL, ini);
        }
        if (jb_super(root) && jb_wait(root, key, 0, 15000) != 0)
        {
            fprintf(stderr, "job %s of %s did not stop\n", argv[2], argv[1]);
            return 1;
        }
        printf("job %s of %s %s\n", argv[2], argv[1], act[0] == 'r' ? "removed" : "stopped");
        return 0;
    }
    if (strcmp(act, "autostart") == 0 && argc >= 4 && (strcmp(argv[3], "on") == 0 || strcmp(argv[3], "off") == 0))
    {
        if ((r = jb_args(root, argc, argv, cfg, &n, &idx, key)) != 0)
            return r;
        if (jb_save(root, key, argv[3][1] == 'n', cfg[idx].cmd) != 0)
        {
            fprintf(stderr, "cannot write %ls\n", ini);
            return 1;
        }
        printf("job %s of %s starts with the server: %s\n", argv[2], argv[1], argv[3]);
        return 0;
    }
    fprintf(stderr, "usage: openlitespeed --job list | add <site> <name> <command...> | remove <site> <name> |\n"
                    "       start|stop|restart <site> <name> | autostart <site> <name> on|off\n");
    return 2;
}

void ols_dev_helpers(int start)
{
    wchar_t root[MAX_PATH];
    if (jb_root(root) != 0)
        return;
    if (start)
    {
        ols_mail_ensure(root);
        ols_jobs_ensure(root, start > 1);
    }
    else
    {
        ols_jobs_halt(root);
        ols_mail_halt(root);
    }
}
