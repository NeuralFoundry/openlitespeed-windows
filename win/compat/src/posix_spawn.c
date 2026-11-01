#include "compat_internal.h"

int ols_register_child(pid_t pid, HANDLE h);

static HANDLE g_job;
static HANDLE g_child_token;
static INIT_ONCE g_job_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK job_init(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
    HANDLE tok;
    if (OpenProcessToken(GetCurrentProcess(),
                         TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &tok))
    {
        if (!CreateRestrictedToken(tok, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL,
                                   &g_child_token))
            g_child_token = NULL;
        CloseHandle(tok);
    }
    g_job = CreateJobObjectW(NULL, NULL);
    if (g_job)
    {
        memset(&li, 0, sizeof(li));
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                                              | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
        SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &li, sizeof(li));
    }
    return TRUE;
}

static int append_quoted(wchar_t **buf, size_t *len, size_t *cap, const wchar_t *arg)
{
    size_t need = wcslen(arg) * 2 + 4;
    const wchar_t *p;
    int quote = (*arg == 0) || wcspbrk(arg, L" \t\n\v\"") != NULL;
    if (*len + need + 2 > *cap)
    {
        size_t ncap = (*cap + need + 2) * 2;
        wchar_t *n = (wchar_t *)realloc(*buf, ncap * sizeof(wchar_t));
        if (!n)
            return -1;
        *buf = n;
        *cap = ncap;
    }
    if (*len)
        (*buf)[(*len)++] = L' ';
    if (!quote)
    {
        wcscpy(*buf + *len, arg);
        *len += wcslen(arg);
        return 0;
    }
    (*buf)[(*len)++] = L'"';
    for (p = arg;; ++p)
    {
        size_t bs = 0;
        while (*p == L'\\')
        {
            ++p;
            ++bs;
        }
        if (*p == 0)
        {
            while (bs--)
            {
                (*buf)[(*len)++] = L'\\';
                (*buf)[(*len)++] = L'\\';
            }
            break;
        }
        if (*p == L'"')
        {
            bs = bs * 2 + 1;
            while (bs--)
                (*buf)[(*len)++] = L'\\';
            (*buf)[(*len)++] = L'"';
        }
        else
        {
            while (bs--)
                (*buf)[(*len)++] = L'\\';
            (*buf)[(*len)++] = *p;
        }
    }
    (*buf)[(*len)++] = L'"';
    (*buf)[*len] = 0;
    return 0;
}

static wchar_t *utf8_to_w(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w;
    if (n <= 0)
        return NULL;
    w = (wchar_t *)malloc(n * sizeof(wchar_t));
    if (w)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static int env_has(char *const envp[], const char *name)
{
    size_t l = strlen(name);
    int i;
    for (i = 0; envp && envp[i]; ++i)
        if (_strnicmp(envp[i], name, l) == 0 && envp[i][l] == '=')
            return 1;
    return 0;
}

static wchar_t *build_env(char *const envp[], const char *extra1, const char *extra2)
{
    static const char *inherit[] = { "SystemRoot", "SystemDrive", "windir", "TEMP", "TMP",
                                     "PATH", "PATHEXT", "COMSPEC", "ProgramData", "ALLUSERSPROFILE",
                                     "PROGRAMFILES", "LOCALAPPDATA", "APPDATA", "USERPROFILE",
                                     "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE", NULL };
    char **all;
    int n = 0, i, k = 0;
    size_t total = 1, pos = 0;
    wchar_t *block;
    for (i = 0; envp && envp[i]; ++i)
        ++n;
    all = (char **)calloc(n + 24, sizeof(char *));
    if (!all)
        return NULL;
    for (i = 0; i < n; ++i)
        all[k++] = (char *)envp[i];
    for (i = 0; inherit[i]; ++i)
    {
        char buf[32768];
        DWORD l;
        if (env_has(envp, inherit[i]))
            continue;
        l = GetEnvironmentVariableA(inherit[i], buf, sizeof(buf));
        if (l == 0 || l >= sizeof(buf))
            continue;
        all[k] = (char *)malloc(strlen(inherit[i]) + l + 2);
        if (!all[k])
            continue;
        sprintf(all[k], "%s=%s", inherit[i], buf);
        ++k;
    }
    if (extra1)
        all[k++] = (char *)extra1;
    if (extra2)
        all[k++] = (char *)extra2;
    for (i = 0; i < k; ++i)
        total += MultiByteToWideChar(CP_UTF8, 0, all[i], -1, NULL, 0);
    block = (wchar_t *)malloc(total * sizeof(wchar_t));
    if (block)
    {
        for (i = 0; i < k; ++i)
            pos += MultiByteToWideChar(CP_UTF8, 0, all[i], -1, block + pos, (int)(total - pos));
        block[pos] = 0;
    }
    for (i = n; i < k; ++i)
        if (all[i] != extra1 && all[i] != extra2)
            free(all[i]);
    free(all);
    return block;
}

pid_t ols_spawn(const char *path, char *const argv[], char *const envp[], const char *cwd,
                int fd_in, int fd_out, int fd_err, int flags)
{
    wchar_t wpath[PATH_MAX + 8], wcwd[PATH_MAX + 8], *cmd = NULL, *env = NULL;
    size_t clen = 0, ccap = 0;
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    HANDLE inherit[3];
    int ninherit = 0, i;
    SIZE_T attrsz = 0;
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = NULL;
    BOOL ok;
    DWORD err;
    HANDLE hin = INVALID_HANDLE_VALUE, hout = INVALID_HANDLE_VALUE, herr = INVALID_HANDLE_VALUE;

    InitOnceExecuteOnce(&g_job_once, job_init, NULL, NULL);
    if (!g_child_token)
    {
        errno = EPERM;
        return -1;
    }
    if (ols_path_to_w(path, wpath, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (GetFileAttributesW(wpath) == INVALID_FILE_ATTRIBUTES)
    {
        errno = ENOENT;
        return -1;
    }
    if (cwd && ols_path_to_w(cwd, wcwd, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (append_quoted(&cmd, &clen, &ccap, wpath) < 0)
        goto nomem;
    for (i = 1; argv && argv[i]; ++i)
    {
        wchar_t *a = utf8_to_w(argv[i]);
        if (!a || append_quoted(&cmd, &clen, &ccap, a) < 0)
        {
            free(a);
            goto nomem;
        }
        free(a);
    }
    env = build_env(envp, NULL, NULL);
    if (!env)
        goto nomem;

    if (fd_in >= 0)
    {
        hin = ols_fd_type(fd_in) == OLS_T_SOCKET ? (HANDLE)ols_socket_from_fd(fd_in)
                                                  : ols_handle_from_fd(fd_in);
        if (flags & OLS_SPAWN_FCGI)
        {
            u_long blocking = 0;
            ioctlsocket((SOCKET)hin, FIONBIO, &blocking);
        }
        SetHandleInformation(hin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        inherit[ninherit++] = hin;
    }
    if (!(flags & OLS_SPAWN_FCGI))
    {
        if (fd_out >= 0)
        {
            hout = ols_handle_from_fd(fd_out);
            SetHandleInformation(hout, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            if (hout != hin)
                inherit[ninherit++] = hout;
        }
        if (fd_err >= 0)
        {
            herr = ols_handle_from_fd(fd_err);
            SetHandleInformation(herr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            if (herr != hin && herr != hout)
                inherit[ninherit++] = herr;
        }
    }

    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = hin;
    si.StartupInfo.hStdOutput = hout;
    si.StartupInfo.hStdError = herr;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrsz);
    attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrsz);
    if (!attrs || !InitializeProcThreadAttributeList(attrs, 1, 0, &attrsz))
        goto nomem;
    if (ninherit)
        UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                                  ninherit * sizeof(HANDLE), NULL, NULL);
    si.lpAttributeList = attrs;

    ok = CreateProcessAsUserW(g_child_token, wpath, cmd, NULL, NULL, ninherit ? TRUE : FALSE,
                              CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW
                              | EXTENDED_STARTUPINFO_PRESENT,
                              env, cwd ? wcwd : NULL, &si.StartupInfo, &pi);
    err = GetLastError();
    for (i = 0; i < ninherit; ++i)
        SetHandleInformation(inherit[i], HANDLE_FLAG_INHERIT, 0);
    DeleteProcThreadAttributeList(attrs);
    free(attrs);
    free(cmd);
    free(env);
    if (!ok)
    {
        errno = ols_errno_from_win(err);
        return -1;
    }
    if (g_job)
        AssignProcessToJobObject(g_job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    if (ols_register_child((pid_t)pi.dwProcessId, pi.hProcess) != 0)
        CloseHandle(pi.hProcess);
    return (pid_t)pi.dwProcessId;

nomem:
    free(attrs);
    free(cmd);
    free(env);
    errno = ENOMEM;
    return -1;
}
