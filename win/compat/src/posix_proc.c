#include "compat_internal.h"
#include <psapi.h>

static volatile uid_t g_uid = 0, g_euid = 0;
static volatile gid_t g_gid = 0, g_egid = 0;
static DWORD g_main_tid;
static char g_drive[3];
static INIT_ONCE g_init_once = INIT_ONCE_STATIC_INIT;

static struct sigaction g_act[NSIG];
static volatile LONG64 g_pending;
static volatile LONG64 g_blocked;
static int g_wake_rd = -1, g_wake_wr = -1;
static HANDLE g_sig_events[NSIG];
static HANDLE g_sig_thread;
static const int g_ipc_sigs[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2, 0 };

#define MAX_CHILDREN 4096
static SRWLOCK g_child_lock = SRWLOCK_INIT;
static struct
{
    pid_t pid;
    HANDLE h;
    HANDLE wait;
    int killsig;
} g_children[MAX_CHILDREN];
static int g_nchildren;

static BOOL WINAPI console_handler(DWORD type)
{
    int sig;
    switch (type)
    {
    case CTRL_C_EVENT:
        sig = SIGINT;
        break;
    case CTRL_BREAK_EVENT:
        sig = SIGQUIT;
        break;
    default:
        sig = SIGTERM;
        break;
    }
    ols_raise(sig);
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT)
        Sleep(5000);
    return TRUE;
}

static DWORD WINAPI sig_thread(LPVOID arg)
{
    HANDLE hs[NSIG];
    int map[NSIG], n = 0, i;
    for (i = 0; g_ipc_sigs[i]; ++i)
        if (g_sig_events[g_ipc_sigs[i]])
        {
            hs[n] = g_sig_events[g_ipc_sigs[i]];
            map[n++] = g_ipc_sigs[i];
        }
    if (n == 0)
        return 0;
    for (;;)
    {
        DWORD r = WaitForMultipleObjects(n, hs, FALSE, INFINITE);
        if (r >= WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + (DWORD)n)
            ols_raise(map[r - WAIT_OBJECT_0]);
        else
            Sleep(100);
    }
}

void ols_ensure_tmp_dirs(void);
void ols_handoff_load(void);

void ols_reserve_low_handles(void)
{
    HANDLE h[64];
    int i, n = 0;
    for (i = 0; i < 64; ++i)
    {
        h[n] = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!h[n])
            break;
        if ((uintptr_t)h[n] > 8)
            ++n;
    }
    for (i = 0; i < n; ++i)
        CloseHandle(h[i]);
}

static int token_elevated(void)
{
    HANDLE t;
    TOKEN_ELEVATION e;
    DWORD n;
    int r = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
    {
        if (GetTokenInformation(t, TokenElevation, &e, sizeof(e), &n))
            r = e.TokenIsElevated != 0;
        CloseHandle(t);
    }
    return r;
}

static BOOL CALLBACK init_cb(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    wchar_t cwd[PATH_MAX], root[PATH_MAX];
    int fds[2], i;
    DWORD n;
    ols_reserve_low_handles();
    g_main_tid = GetCurrentThreadId();
    n = GetEnvironmentVariableW(L"OLS_ROOT", root, PATH_MAX);
    if (n == 0 || n >= PATH_MAX || token_elevated())
    {
        wchar_t exe[PATH_MAX], *slash;
        DWORD el = GetModuleFileNameW(NULL, exe, PATH_MAX);
        if (el > 0 && el < PATH_MAX && (slash = wcsrchr(exe, L'\\')) != NULL)
        {
            *slash = 0;
            slash = wcsrchr(exe, L'\\');
            if (slash && _wcsicmp(slash + 1, L"bin") == 0)
            {
                *slash = 0;
                SetEnvironmentVariableW(L"OLS_ROOT", exe);
                _wputenv_s(L"OLS_ROOT", exe);
                n = GetEnvironmentVariableW(L"OLS_ROOT", root, PATH_MAX);
            }
        }
    }
    ols_ensure_tmp_dirs();
    ols_worker_no();
    ols_handoff_load();
    if (n > 1 && n < PATH_MAX && root[1] == L':')
    {
        g_drive[0] = (char)root[0];
        g_drive[1] = ':';
    }
    else if (GetCurrentDirectoryW(PATH_MAX, cwd) > 1 && cwd[1] == L':')
    {
        g_drive[0] = (char)cwd[0];
        g_drive[1] = ':';
    }
    ols_wsa_init();
    if (socketpair(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == 0)
    {
        g_wake_rd = fds[0];
        g_wake_wr = fds[1];
    }
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = ols_private_sd(0);
        for (i = 0; sa.lpSecurityDescriptor && g_ipc_sigs[i]; ++i)
        {
            wchar_t name[64];
            HANDLE ev;
            swprintf(name, 64, L"Local\\ols-sig-%lu-%d", GetCurrentProcessId(), g_ipc_sigs[i]);
            ev = CreateEventW(&sa, FALSE, FALSE, name);
            if (ev && GetLastError() == ERROR_ALREADY_EXISTS)
            {
                CloseHandle(ev);
                ev = NULL;
            }
            g_sig_events[g_ipc_sigs[i]] = ev;
        }
        if (sa.lpSecurityDescriptor)
            LocalFree(sa.lpSecurityDescriptor);
    }
    g_sig_thread = CreateThread(NULL, 64 * 1024, sig_thread, NULL,
                                STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    SetConsoleCtrlHandler(console_handler, TRUE);
    return TRUE;
}

void ols_compat_init(void)
{
    InitOnceExecuteOnce(&g_init_once, init_cb, NULL, NULL);
}

const char *ols_server_drive(void)
{
    ols_compat_init();
    return g_drive[0] ? g_drive : NULL;
}

int ols_native_path(const char *p, int len, char *buf, int buflen)
{
    char tmp[PATH_MAX * 3];
    wchar_t w[PATH_MAX + 8];
    const wchar_t *src;
    int n;
    char *q;
    if (len <= 0 || p[0] != '/' || (len > 1 && p[1] == '/') || len >= (int)sizeof(tmp))
        return -1;
    memcpy(tmp, p, len);
    tmp[len] = 0;
    if (ols_path_to_w(tmp, w, PATH_MAX + 8, NULL) < 0)
        return -1;
    src = w;
    if (wcsncmp(src, L"\\\\?\\", 4) == 0)
        src += 4;
    n = WideCharToMultiByte(CP_UTF8, 0, src, -1, buf, buflen, NULL, NULL);
    if (n <= 0)
        return -1;
    for (q = buf; *q; ++q)
        if (*q == '\\')
            *q = '/';
    if (len > 1 && p[len - 1] == '/' && q > buf && q[-1] != '/' && (q - buf) + 1 < buflen)
    {
        *q++ = '/';
        *q = 0;
    }
    return (int)(q - buf);
}

int ols_mapped_path(const char *p, char *buf, int buflen)
{
    const char *d = ols_server_drive();
    int n = ols_native_path(p, (int)strlen(p), buf, buflen);
    if (n < 0)
        return -1;
    if (d && n >= 2 && buf[1] == ':' && toupper((unsigned char)buf[0]) == toupper((unsigned char)d[0]))
    {
        memmove(buf, buf + 2, n - 1);
        n -= 2;
    }
    return n;
}

int ols_strip_server_drive(char *p)
{
    const char *d;
    char *q;
    size_t l;
    if (!ols_is_drive_path(p))
        return 0;
    for (q = p; *q; ++q)
        if (*q == '\\')
            *q = '/';
    d = ols_server_drive();
    if (!d || toupper((unsigned char)p[0]) != toupper((unsigned char)d[0]))
        return 0;
    l = strlen(p + 2);
    memmove(p, p + 2, l + 1);
    return 2;
}

void ols_set_main_thread(void)
{
    ols_compat_init();
    g_main_tid = GetCurrentThreadId();
}

int ols_main_thread(void)
{
    ols_compat_init();
    return GetCurrentThreadId() == g_main_tid;
}

int ols_signal_wake_fd(void)
{
    ols_compat_init();
    return g_wake_rd;
}

void ols_signal_wake(void)
{
    char c = 1;
    if (g_wake_wr >= 0)
        send(ols_socket_from_fd(g_wake_wr), &c, 1, 0);
}

void ols_signal_drain(void)
{
    char buf[64];
    if (g_wake_rd >= 0)
        while (recv(ols_socket_from_fd(g_wake_rd), buf, sizeof(buf), 0) > 0)
            ;
}

int ols_signal_pending(void)
{
    return (g_pending & ~g_blocked) != 0;
}

static void default_action(int sig)
{
    switch (sig)
    {
    case SIGCHLD:
    case SIGURG:
    case SIGWINCH:
    case SIGCONT:
    case SIGPIPE:
        return;
    default:
        ExitProcess(128 + sig);
    }
}

void ols_signal_dispatch(void)
{
    int sig;
    for (sig = 1; sig < NSIG; ++sig)
    {
        LONG64 bit = 1LL << sig;
        if (!(g_pending & bit) || (g_blocked & bit))
            continue;
        _InterlockedAnd64(&g_pending, ~bit);
        if (g_act[sig].sa_flags & SA_SIGINFO)
        {
            if (g_act[sig].sa_sigaction)
            {
                siginfo_t si;
                memset(&si, 0, sizeof(si));
                si.si_signo = sig;
                g_act[sig].sa_sigaction(sig, &si, NULL);
            }
        }
        else if (g_act[sig].sa_handler == SIG_IGN)
            continue;
        else if (g_act[sig].sa_handler == SIG_DFL || g_act[sig].sa_handler == NULL)
            default_action(sig);
        else
        {
            void (*h)(int) = g_act[sig].sa_handler;
            if (g_act[sig].sa_flags & SA_RESETHAND)
                g_act[sig].sa_handler = SIG_DFL;
            h(sig);
        }
    }
}

int ols_raise(int sig)
{
    if (sig <= 0 || sig >= NSIG)
    {
        errno = EINVAL;
        return -1;
    }
    ols_compat_init();
    if (sig == SIGKILL)
        ExitProcess(128 + SIGKILL);
    if (!(g_act[sig].sa_flags & SA_SIGINFO) && g_act[sig].sa_handler == SIG_IGN)
        return 0;
    _InterlockedOr64(&g_pending, 1LL << sig);
    ols_signal_wake();
    return 0;
}

int sigemptyset(sigset_t *set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set)
{
    *set = ~(sigset_t)0;
    return 0;
}

int sigaddset(sigset_t *set, int sig)
{
    if (sig <= 0 || sig >= NSIG)
    {
        errno = EINVAL;
        return -1;
    }
    *set |= 1ULL << sig;
    return 0;
}

int sigdelset(sigset_t *set, int sig)
{
    if (sig <= 0 || sig >= NSIG)
    {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1ULL << sig);
    return 0;
}

int sigismember(const sigset_t *set, int sig)
{
    if (sig <= 0 || sig >= NSIG)
        return 0;
    return (*set >> sig) & 1;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{
    if (sig <= 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
    {
        errno = EINVAL;
        return -1;
    }
    ols_compat_init();
    if (old)
        *old = g_act[sig];
    if (act)
        g_act[sig] = *act;
    return 0;
}

sighandler_t ols_signal(int sig, sighandler_t handler)
{
    struct sigaction sa, old;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(sig, &sa, &old) < 0)
        return SIG_ERR;
    return old.sa_handler;
}

static __declspec(thread) sigset_t t_mask;

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
    if (!ols_main_thread())
    {
        if (old)
            *old = t_mask;
        if (!set)
            return 0;
        if (how == SIG_BLOCK)
            t_mask |= *set;
        else if (how == SIG_UNBLOCK)
            t_mask &= ~*set;
        else if (how == SIG_SETMASK)
            t_mask = *set;
        else
        {
            errno = EINVAL;
            return -1;
        }
        return 0;
    }
    if (old)
        *old = (sigset_t)g_blocked;
    if (!set)
        return 0;
    switch (how)
    {
    case SIG_BLOCK:
        _InterlockedOr64(&g_blocked, (LONG64)*set);
        break;
    case SIG_UNBLOCK:
        _InterlockedAnd64(&g_blocked, ~(LONG64)*set);
        if (ols_signal_pending())
            ols_signal_wake();
        break;
    case SIG_SETMASK:
        _InterlockedExchange64(&g_blocked, (LONG64)*set);
        if (ols_signal_pending())
            ols_signal_wake();
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout)
{
    ULONGLONG end = GetTickCount64() + (timeout ? (ULONGLONG)timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000 : 0);
    for (;;)
    {
        int sig;
        for (sig = 1; sig < NSIG; ++sig)
        {
            LONG64 bit = 1LL << sig;
            if ((*set & bit) && (g_pending & bit))
            {
                _InterlockedAnd64(&g_pending, ~bit);
                if (info)
                {
                    memset(info, 0, sizeof(*info));
                    info->si_signo = sig;
                }
                return sig;
            }
        }
        if (timeout && GetTickCount64() >= end)
        {
            errno = EAGAIN;
            return -1;
        }
        Sleep(5);
    }
}

int sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
    return sigtimedwait(set, info, NULL);
}

int sigwait(const sigset_t *set, int *sig)
{
    int r = sigtimedwait(set, NULL, NULL);
    if (r < 0)
        return errno;
    *sig = r;
    return 0;
}

int pause(void)
{
    while (!ols_signal_pending())
        Sleep(10);
    ols_signal_dispatch();
    errno = EINTR;
    return -1;
}

int sigsuspend(const sigset_t *mask)
{
    sigset_t old;
    sigprocmask(SIG_SETMASK, mask, &old);
    while (!ols_signal_pending())
        Sleep(10);
    ols_signal_dispatch();
    sigprocmask(SIG_SETMASK, &old, NULL);
    errno = EINTR;
    return -1;
}

int sigpending(sigset_t *set)
{
    *set = (sigset_t)(g_pending & g_blocked);
    return 0;
}

int siginterrupt(int sig, int flag)
{
    return 0;
}

int sigqueue(pid_t pid, int sig, const union sigval value)
{
    return kill(pid, sig);
}

int sigaltstack(const stack_t *ss, stack_t *old)
{
    if (old)
    {
        memset(old, 0, sizeof(*old));
        old->ss_flags = SS_DISABLE;
    }
    return 0;
}

const char *strsignal(int sig)
{
    switch (sig)
    {
    case SIGHUP: return "Hangup";
    case SIGINT: return "Interrupt";
    case SIGQUIT: return "Quit";
    case SIGILL: return "Illegal instruction";
    case SIGABRT: return "Aborted";
    case SIGFPE: return "Floating point exception";
    case SIGKILL: return "Killed";
    case SIGSEGV: return "Segmentation fault";
    case SIGPIPE: return "Broken pipe";
    case SIGALRM: return "Alarm clock";
    case SIGTERM: return "Terminated";
    case SIGUSR1: return "User defined signal 1";
    case SIGUSR2: return "User defined signal 2";
    case SIGCHLD: return "Child exited";
    default: return "Unknown signal";
    }
}

void psignal(int sig, const char *s)
{
    if (s && *s)
        fprintf(stderr, "%s: %s\n", s, strsignal(sig));
    else
        fprintf(stderr, "%s\n", strsignal(sig));
}

static int child_index(pid_t pid)
{
    int i;
    for (i = 0; i < g_nchildren; ++i)
        if (g_children[i].pid == pid)
            return i;
    return -1;
}

static VOID CALLBACK child_exited(PVOID ctx, BOOLEAN timeout)
{
    ols_raise(SIGCHLD);
}

int ols_register_child(pid_t pid, HANDLE h)
{
    int i;
    AcquireSRWLockExclusive(&g_child_lock);
    if (g_nchildren >= MAX_CHILDREN)
    {
        ReleaseSRWLockExclusive(&g_child_lock);
        errno = EAGAIN;
        return -1;
    }
    i = g_nchildren++;
    g_children[i].pid = pid;
    g_children[i].h = h;
    g_children[i].killsig = 0;
    g_children[i].wait = NULL;
    RegisterWaitForSingleObject(&g_children[i].wait, h, child_exited, NULL, INFINITE,
                                WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD);
    ReleaseSRWLockExclusive(&g_child_lock);
    return 0;
}

static void child_remove(int i)
{
    if (g_children[i].wait)
        UnregisterWaitEx(g_children[i].wait, NULL);
    CloseHandle(g_children[i].h);
    g_children[i] = g_children[--g_nchildren];
}

static int send_ipc(pid_t pid, int sig)
{
    wchar_t name[64];
    HANDLE e;
    swprintf(name, 64, L"Local\\ols-sig-%lu-%d", (unsigned long)pid, sig);
    e = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
    if (!e)
        return -1;
    SetEvent(e);
    CloseHandle(e);
    return 0;
}

int kill(pid_t pid, int sig)
{
    HANDLE h;
    DWORD code;
    if (pid <= 0)
    {
        int i, n;
        pid_t pids[MAX_CHILDREN];
        AcquireSRWLockShared(&g_child_lock);
        n = g_nchildren;
        for (i = 0; i < n; ++i)
            pids[i] = g_children[i].pid;
        ReleaseSRWLockShared(&g_child_lock);
        for (i = 0; i < n; ++i)
            kill(pids[i], sig);
        if (pid == 0 && sig != 0)
            ols_raise(sig);
        return 0;
    }
    if ((DWORD)pid == GetCurrentProcessId())
        return sig ? ols_raise(sig) : 0;
    h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!h)
        h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
    {
        errno = GetLastError() == ERROR_ACCESS_DENIED ? EPERM : ESRCH;
        return -1;
    }
    if (!GetExitCodeProcess(h, &code) || code != STILL_ACTIVE)
    {
        CloseHandle(h);
        errno = ESRCH;
        return -1;
    }
    if (sig == 0)
    {
        CloseHandle(h);
        return 0;
    }
    if (sig != SIGKILL && send_ipc(pid, sig) == 0)
    {
        CloseHandle(h);
        return 0;
    }
    if (sig == SIGKILL || sig == SIGTERM || sig == SIGINT || sig == SIGQUIT || sig == SIGHUP
        || sig == SIGABRT)
    {
        int i;
        if (!TerminateProcess(h, 128 + sig))
        {
            CloseHandle(h);
            errno = EPERM;
            return -1;
        }
        AcquireSRWLockExclusive(&g_child_lock);
        i = child_index(pid);
        if (i >= 0)
            g_children[i].killsig = sig;
        ReleaseSRWLockExclusive(&g_child_lock);
    }
    CloseHandle(h);
    return 0;
}

int ols_pid_is_ours(pid_t pid)
{
    wchar_t self[MAX_PATH + 1], other[MAX_PATH + 1];
    DWORD n = MAX_PATH + 1, sn;
    HANDLE h;
    BOOL ok;
    if (pid <= 0)
        return 0;
    sn = GetModuleFileNameW(NULL, self, MAX_PATH + 1);
    if (sn == 0 || sn > MAX_PATH)
        return 0;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h)
        return 0;
    ok = QueryFullProcessImageNameW(h, 0, other, &n);
    CloseHandle(h);
    return ok && CompareStringOrdinal(self, -1, other, -1, TRUE) == CSTR_EQUAL;
}

int killpg(pid_t pgrp, int sig)
{
    return kill(-pgrp, sig);
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    for (;;)
    {
        int i, found = 0;
        DWORD code;
        AcquireSRWLockExclusive(&g_child_lock);
        for (i = 0; i < g_nchildren; ++i)
        {
            if (pid > 0 && g_children[i].pid != pid)
                continue;
            found = 1;
            if (WaitForSingleObject(g_children[i].h, 0) == WAIT_OBJECT_0)
            {
                pid_t p = g_children[i].pid;
                GetExitCodeProcess(g_children[i].h, &code);
                if (status)
                {
                    if (g_children[i].killsig)
                        *status = g_children[i].killsig;
                    else
                        *status = W_EXITCODE(code & 0xff, 0);
                }
                child_remove(i);
                ReleaseSRWLockExclusive(&g_child_lock);
                return p;
            }
        }
        ReleaseSRWLockExclusive(&g_child_lock);
        if (!found)
        {
            errno = ECHILD;
            return -1;
        }
        if (options & WNOHANG)
            return 0;
        if (ols_signal_pending() && ols_main_thread())
        {
            ols_signal_dispatch();
            errno = EINTR;
            return -1;
        }
        Sleep(20);
    }
}

pid_t wait(int *status)
{
    return waitpid(-1, status, 0);
}

pid_t wait3(int *status, int options, struct rusage *ru)
{
    if (ru)
        memset(ru, 0, sizeof(*ru));
    return waitpid(-1, status, options);
}

pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru)
{
    if (ru)
        memset(ru, 0, sizeof(*ru));
    return waitpid(pid, status, options);
}

pid_t getpid(void)
{
    return (pid_t)GetCurrentProcessId();
}

pid_t getppid(void)
{
    static pid_t cached = -1;
    if (cached < 0)
    {
        typedef LONG (NTAPI *qip_t)(HANDLE, int, PVOID, ULONG, PULONG);
        struct
        {
            PVOID r1;
            PVOID peb;
            PVOID r2[2];
            ULONG_PTR pid;
            ULONG_PTR ppid;
        } pbi;
        qip_t q = (qip_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
        cached = 0;
        if (q && q(GetCurrentProcess(), 0, &pbi, sizeof(pbi), NULL) == 0)
            cached = (pid_t)pbi.ppid;
    }
    return cached;
}

pid_t gettid(void)
{
    return (pid_t)GetCurrentThreadId();
}

pid_t getpgrp(void)
{
    return getpid();
}

pid_t getpgid(pid_t pid)
{
    return pid ? pid : getpid();
}

int setpgid(pid_t pid, pid_t pgid)
{
    return 0;
}

pid_t setsid(void)
{
    return getpid();
}

pid_t getsid(pid_t pid)
{
    return getpid();
}

static volatile uid_t g_suid = 0;
static volatile gid_t g_sgid = 0;

uid_t getuid(void) { return g_uid; }
uid_t geteuid(void) { return g_euid; }
gid_t getgid(void) { return g_gid; }
gid_t getegid(void) { return g_egid; }

int setuid(uid_t uid)
{
    if (g_euid == 0)
    {
        g_uid = g_euid = g_suid = uid;
        return 0;
    }
    if (uid == g_uid || uid == g_suid)
    {
        g_euid = uid;
        return 0;
    }
    errno = EPERM;
    return -1;
}

int seteuid(uid_t uid)
{
    if (g_euid == 0 || uid == g_uid || uid == g_suid)
    {
        g_euid = uid;
        return 0;
    }
    errno = EPERM;
    return -1;
}

int setgid(gid_t gid)
{
    if (g_euid == 0)
    {
        g_gid = g_egid = g_sgid = gid;
        return 0;
    }
    if (gid == g_gid || gid == g_sgid)
    {
        g_egid = gid;
        return 0;
    }
    errno = EPERM;
    return -1;
}

int setegid(gid_t gid)
{
    if (g_euid == 0 || gid == g_gid || gid == g_sgid)
    {
        g_egid = gid;
        return 0;
    }
    errno = EPERM;
    return -1;
}

int setreuid(uid_t r, uid_t e)
{
    if (g_euid != 0)
    {
        if ((r != (uid_t)-1 && r != g_uid && r != g_euid)
            || (e != (uid_t)-1 && e != g_uid && e != g_euid && e != g_suid))
        {
            errno = EPERM;
            return -1;
        }
    }
    if (r != (uid_t)-1)
        g_uid = r;
    if (e != (uid_t)-1)
        g_euid = e;
    if (r != (uid_t)-1 || (e != (uid_t)-1 && e != g_uid))
        g_suid = g_euid;
    return 0;
}

int setregid(gid_t r, gid_t e)
{
    if (g_euid != 0)
    {
        if ((r != (gid_t)-1 && r != g_gid && r != g_egid)
            || (e != (gid_t)-1 && e != g_gid && e != g_egid && e != g_sgid))
        {
            errno = EPERM;
            return -1;
        }
    }
    if (r != (gid_t)-1)
        g_gid = r;
    if (e != (gid_t)-1)
        g_egid = e;
    if (r != (gid_t)-1 || (e != (gid_t)-1 && e != g_gid))
        g_sgid = g_egid;
    return 0;
}

int setresuid(uid_t r, uid_t e, uid_t s)
{
    return setreuid(r, e);
}

int setresgid(gid_t r, gid_t e, gid_t s)
{
    return setregid(r, e);
}

int setfsuid(uid_t uid)
{
    uid_t old = g_euid;
    return (int)old;
}

int setfsgid(gid_t gid)
{
    gid_t old = g_egid;
    return (int)old;
}

int getgroups(int size, gid_t list[])
{
    if (size > 0)
        list[0] = g_gid;
    return 1;
}

int setgroups(size_t size, const gid_t *list)
{
    if (g_euid != 0)
    {
        errno = EPERM;
        return -1;
    }
    return 0;
}

int initgroups(const char *user, gid_t group)
{
    if (g_euid != 0)
    {
        errno = EPERM;
        return -1;
    }
    return 0;
}

int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups)
{
    if (*ngroups < 1)
    {
        *ngroups = 1;
        return -1;
    }
    groups[0] = group;
    *ngroups = 1;
    return 1;
}

static uid_t name_uid(const char *name)
{
    uint32_t h = 2166136261u;
    if (strcmp(name, "root") == 0)
        return 0;
    if (strcmp(name, "nobody") == 0 || strcmp(name, "nogroup") == 0)
        return 65534;
    while (*name)
        h = (h ^ (unsigned char)*name++) * 16777619u;
    return 1000 + (h % 50000);
}

static __declspec(thread) struct passwd t_pw;
static __declspec(thread) struct group t_gr;
static __declspec(thread) char t_name[256];
static __declspec(thread) char *t_mem[1];

static void fill_pw(struct passwd *pw, const char *name, uid_t uid, char *buf)
{
    pw->pw_name = buf;
    pw->pw_passwd = (char *)"x";
    pw->pw_uid = uid;
    pw->pw_gid = uid;
    pw->pw_gecos = (char *)"";
    pw->pw_dir = (char *)"/";
    pw->pw_shell = (char *)"";
}

struct passwd *getpwnam(const char *name)
{
    if (!name || !*name || strlen(name) >= sizeof(t_name))
        return NULL;
    strcpy(t_name, name);
    fill_pw(&t_pw, name, name_uid(name), t_name);
    return &t_pw;
}

struct passwd *getpwuid(uid_t uid)
{
    if (uid == 0)
        strcpy(t_name, "root");
    else if (uid == 65534)
        strcpy(t_name, "nobody");
    else
        snprintf(t_name, sizeof(t_name), "u%u", uid);
    fill_pw(&t_pw, t_name, uid, t_name);
    return &t_pw;
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t len, struct passwd **result)
{
    if (!name || strlen(name) + 1 > len)
    {
        *result = NULL;
        return ERANGE;
    }
    strcpy(buf, name);
    fill_pw(pwd, name, name_uid(name), buf);
    *result = pwd;
    return 0;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t len, struct passwd **result)
{
    struct passwd *p = getpwuid(uid);
    if (strlen(p->pw_name) + 1 > len)
    {
        *result = NULL;
        return ERANGE;
    }
    strcpy(buf, p->pw_name);
    fill_pw(pwd, buf, uid, buf);
    *result = pwd;
    return 0;
}

static void fill_gr(struct group *gr, gid_t gid, char *buf)
{
    gr->gr_name = buf;
    gr->gr_passwd = (char *)"x";
    gr->gr_gid = gid;
    t_mem[0] = NULL;
    gr->gr_mem = t_mem;
}

struct group *getgrnam(const char *name)
{
    if (!name || !*name || strlen(name) >= sizeof(t_name))
        return NULL;
    strcpy(t_name, name);
    fill_gr(&t_gr, name_uid(name), t_name);
    return &t_gr;
}

struct group *getgrgid(gid_t gid)
{
    if (gid == 0)
        strcpy(t_name, "root");
    else if (gid == 65534)
        strcpy(t_name, "nogroup");
    else
        snprintf(t_name, sizeof(t_name), "g%u", gid);
    fill_gr(&t_gr, gid, t_name);
    return &t_gr;
}

int getgrnam_r(const char *name, struct group *grp, char *buf, size_t len, struct group **result)
{
    if (!name || strlen(name) + 1 > len)
    {
        *result = NULL;
        return ERANGE;
    }
    strcpy(buf, name);
    fill_gr(grp, name_uid(name), buf);
    *result = grp;
    return 0;
}

int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t len, struct group **result)
{
    struct group *g = getgrgid(gid);
    if (strlen(g->gr_name) + 1 > len)
    {
        *result = NULL;
        return ERANGE;
    }
    strcpy(buf, g->gr_name);
    fill_gr(grp, gid, buf);
    *result = grp;
    return 0;
}

struct passwd *getpwent(void) { return NULL; }
void setpwent(void) {}
void endpwent(void) {}
struct group *getgrent(void) { return NULL; }
void setgrent(void) {}
void endgrent(void) {}

pid_t fork(void)
{
    errno = ENOSYS;
    return -1;
}

pid_t vfork(void)
{
    errno = ENOSYS;
    return -1;
}

int daemon(int nochdir, int noclose)
{
    if (!nochdir)
        chdir("/");
    return 0;
}

int execv(const char *path, char *const argv[])
{
    errno = ENOSYS;
    return -1;
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    errno = ENOSYS;
    return -1;
}

int execvp(const char *file, char *const argv[])
{
    errno = ENOSYS;
    return -1;
}

int execl(const char *path, const char *arg, ...)
{
    errno = ENOSYS;
    return -1;
}

int execlp(const char *file, const char *arg, ...)
{
    errno = ENOSYS;
    return -1;
}

int nice(int inc)
{
    return 0;
}

char *getlogin(void)
{
    static char name[256];
    DWORD n = sizeof(name);
    if (!GetUserNameA(name, &n))
        return NULL;
    return name;
}

long gethostid(void)
{
    return 0;
}

int getdtablesize(void)
{
    return 1 << 20;
}

int getpagesize(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwPageSize;
}

long sysconf(int name)
{
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms;
    switch (name)
    {
    case _SC_PAGESIZE:
        GetSystemInfo(&si);
        return si.dwPageSize;
    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN:
        return (long)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    case _SC_OPEN_MAX:
        return 1 << 20;
    case _SC_CLK_TCK:
        return 100;
    case _SC_ARG_MAX:
        return 32767;
    case _SC_CHILD_MAX:
        return MAX_CHILDREN;
    case _SC_NGROUPS_MAX:
        return 64;
    case _SC_PHYS_PAGES:
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        GetSystemInfo(&si);
        return (long)(ms.ullTotalPhys / si.dwPageSize);
    case _SC_GETPW_R_SIZE_MAX:
    case _SC_GETGR_R_SIZE_MAX:
        return 1024;
    case _SC_HOST_NAME_MAX:
        return 255;
    default:
        errno = EINVAL;
        return -1;
    }
}

long pathconf(const char *path, int name)
{
    if (name == _PC_NAME_MAX)
        return 255;
    if (name == _PC_PATH_MAX)
        return PATH_MAX;
    errno = EINVAL;
    return -1;
}

static rlim_t g_rlim[RLIMIT_NLIMITS][2];

int getrlimit(int res, struct rlimit *rl)
{
    if (res < 0 || res >= RLIMIT_NLIMITS)
    {
        errno = EINVAL;
        return -1;
    }
    if (res == RLIMIT_NOFILE)
    {
        rl->rlim_cur = g_rlim[res][0] ? g_rlim[res][0] : (1 << 20);
        rl->rlim_max = 1 << 20;
        return 0;
    }
    if (res == RLIMIT_CORE)
    {
        rl->rlim_cur = 0;
        rl->rlim_max = RLIM_INFINITY;
        return 0;
    }
    rl->rlim_cur = g_rlim[res][0] ? g_rlim[res][0] : RLIM_INFINITY;
    rl->rlim_max = g_rlim[res][1] ? g_rlim[res][1] : RLIM_INFINITY;
    return 0;
}

int setrlimit(int res, const struct rlimit *rl)
{
    if (res < 0 || res >= RLIMIT_NLIMITS)
    {
        errno = EINVAL;
        return -1;
    }
    g_rlim[res][0] = rl->rlim_cur;
    g_rlim[res][1] = rl->rlim_max;
    return 0;
}

static void ft_to_tv(const FILETIME *ft, struct timeval *tv)
{
    uint64_t v = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    tv->tv_sec = (long)(v / 10000000ULL);
    tv->tv_usec = (long)((v % 10000000ULL) / 10);
}

int getrusage(int who, struct rusage *ru)
{
    FILETIME c, e, k, u;
    PROCESS_MEMORY_COUNTERS pmc;
    memset(ru, 0, sizeof(*ru));
    if (who == RUSAGE_THREAD)
    {
        if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u))
            return ols_set_errno_win();
    }
    else if (who == RUSAGE_SELF)
    {
        if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u))
            return ols_set_errno_win();
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        {
            ru->ru_maxrss = (long)(pmc.PeakWorkingSetSize / 1024);
            ru->ru_majflt = (long)pmc.PageFaultCount;
        }
    }
    else
        return 0;
    ft_to_tv(&u, &ru->ru_utime);
    ft_to_tv(&k, &ru->ru_stime);
    return 0;
}

int getpriority(int which, int who)
{
    DWORD c = GetPriorityClass(GetCurrentProcess());
    errno = 0;
    switch (c)
    {
    case IDLE_PRIORITY_CLASS: return 19;
    case BELOW_NORMAL_PRIORITY_CLASS: return 10;
    case ABOVE_NORMAL_PRIORITY_CLASS: return -5;
    case HIGH_PRIORITY_CLASS: return -10;
    case REALTIME_PRIORITY_CLASS: return -20;
    default: return 0;
    }
}

int setpriority(int which, int who, int prio)
{
    DWORD c;
    HANDLE h;
    if (prio >= 15)
        c = IDLE_PRIORITY_CLASS;
    else if (prio > 0)
        c = BELOW_NORMAL_PRIORITY_CLASS;
    else if (prio == 0)
        c = NORMAL_PRIORITY_CLASS;
    else if (prio > -10)
        c = ABOVE_NORMAL_PRIORITY_CLASS;
    else
        c = HIGH_PRIORITY_CLASS;
    if (which != PRIO_PROCESS || who == 0 || (DWORD)who == GetCurrentProcessId())
        h = GetCurrentProcess();
    else
    {
        h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, who);
        if (!h)
        {
            errno = ESRCH;
            return -1;
        }
    }
    if (!SetPriorityClass(h, c))
    {
        if (h != GetCurrentProcess())
            CloseHandle(h);
        return ols_set_errno_win();
    }
    if (h != GetCurrentProcess())
        CloseHandle(h);
    return 0;
}

int uname(struct utsname *buf)
{
    typedef LONG (WINAPI *rgv_t)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW vi;
    rgv_t f;
    DWORD n = sizeof(buf->nodename);
    memset(buf, 0, sizeof(*buf));
    strcpy(buf->sysname, "Windows");
    strcpy(buf->machine, "x86_64");
    if (!GetComputerNameExA(ComputerNameDnsHostname, buf->nodename, &n))
        strcpy(buf->nodename, "localhost");
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    f = (rgv_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (f && f(&vi) == 0)
    {
        snprintf(buf->release, sizeof(buf->release), "%lu.%lu.%lu", vi.dwMajorVersion,
                 vi.dwMinorVersion, vi.dwBuildNumber);
        snprintf(buf->version, sizeof(buf->version), "Windows %lu.%lu build %lu",
                 vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    }
    return 0;
}

int prctl(int option, ...)
{
    return 0;
}

long syscall(long number, ...)
{
    if (number == SYS_gettid)
        return (long)GetCurrentThreadId();
    if (number == SYS_getpid)
        return (long)GetCurrentProcessId();
    errno = ENOSYS;
    return -1;
}

static __declspec(thread) char t_dlerr[512];
static __declspec(thread) int t_dlerr_set;

static void set_dlerr(const char *what)
{
    DWORD e = GetLastError();
    int n = snprintf(t_dlerr, sizeof(t_dlerr), "%s: ", what);
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e, 0,
                   t_dlerr + n, (DWORD)(sizeof(t_dlerr) - n), NULL);
    t_dlerr_set = 1;
}

void *dlopen(const char *file, int mode)
{
    wchar_t w[PATH_MAX + 8];
    HMODULE m;
    if (!file)
        return GetModuleHandleW(NULL);
    if (ols_path_to_w(file, w, PATH_MAX + 8, NULL) < 0)
    {
        snprintf(t_dlerr, sizeof(t_dlerr), "%s: invalid path", file);
        t_dlerr_set = 1;
        return NULL;
    }
    m = LoadLibraryExW(w, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m)
        set_dlerr(file);
    return m;
}

void *dlsym(void *handle, const char *name)
{
    FARPROC p;
    if (handle == RTLD_DEFAULT || handle == RTLD_NEXT)
        handle = GetModuleHandleW(NULL);
    p = GetProcAddress((HMODULE)handle, name);
    if (!p)
        set_dlerr(name);
    return (void *)p;
}

int dlclose(void *handle)
{
    if (handle && handle != GetModuleHandleW(NULL))
        FreeLibrary((HMODULE)handle);
    return 0;
}

char *dlerror(void)
{
    if (!t_dlerr_set)
        return NULL;
    t_dlerr_set = 0;
    return t_dlerr;
}

int dladdr(const void *addr, Dl_info *info)
{
    static __declspec(thread) char name[MAX_PATH];
    HMODULE m;
    memset(info, 0, sizeof(*info));
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m))
        return 0;
    GetModuleFileNameA(m, name, MAX_PATH);
    info->dli_fname = name;
    info->dli_fbase = m;
    return 1;
}

static char g_ident[64] = "openlitespeed";

void openlog(const char *ident, int opt, int facility)
{
    if (ident)
        strncpy(g_ident, ident, sizeof(g_ident) - 1);
}

void vsyslog(int prio, const char *fmt, va_list ap)
{
    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "%s: ", g_ident);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    OutputDebugStringA(buf);
}

void syslog(int prio, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsyslog(prio, fmt, ap);
    va_end(ap);
}

void closelog(void)
{
}

int setlogmask(int mask)
{
    return 0xff;
}

int getloadavg(double loadavg[], int nelem)
{
    int i;
    for (i = 0; i < nelem && i < 3; ++i)
        loadavg[i] = 0;
    return nelem < 3 ? nelem : 3;
}
