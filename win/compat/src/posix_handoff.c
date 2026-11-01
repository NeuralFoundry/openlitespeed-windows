#include "compat_internal.h"

#define HANDOFF_MAX 256
#define HANDOFF_MAGIC 0x4f4c5349
#define ACCEPT_KEY_LEN 16

typedef struct
{
    int slot;
    WSAPROTOCOL_INFOW info;
} handoff_rec;

static struct
{
    int slot;
    int fd;
} g_out[HANDOFF_MAX], g_in[HANDOFF_MAX];
static int g_nout, g_nin, g_inpos;
static SRWLOCK g_lock = SRWLOCK_INIT;

static unsigned char g_accept_key[ACCEPT_KEY_LEN];
static int g_have_key;
static SOCKET g_shared[HANDOFF_MAX];
static int g_nshared;
static HANDLE g_accept_mutex;
static SRWLOCK g_shared_lock = SRWLOCK_INIT;

static void ensure_accept_key(void)
{
    if (!g_have_key
        && BCryptGenRandom(NULL, g_accept_key, ACCEPT_KEY_LEN, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0)
        g_have_key = 1;
}

static void mark_shared(SOCKET s)
{
    int i;
    AcquireSRWLockExclusive(&g_shared_lock);
    for (i = 0; i < g_nshared; ++i)
        if (g_shared[i] == s)
            break;
    if (i == g_nshared && g_nshared < HANDOFF_MAX)
        g_shared[g_nshared++] = s;
    ReleaseSRWLockExclusive(&g_shared_lock);
}

static HANDLE accept_mutex(void)
{
    wchar_t name[64];
    SECURITY_ATTRIBUTES sa;
    HANDLE m;
    int i;
    if (g_accept_mutex || !g_have_key)
        return g_accept_mutex;
    i = swprintf(name, 64, L"Local\\ols-accept-");
    for (int k = 0; k < ACCEPT_KEY_LEN && i < 62; ++k)
        i += swprintf(name + i, 64 - i, L"%02x", g_accept_key[k]);
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = ols_private_sd(0);
    m = CreateMutexW(sa.lpSecurityDescriptor ? &sa : NULL, FALSE, name);
    if (sa.lpSecurityDescriptor)
        LocalFree(sa.lpSecurityDescriptor);
    if (m && InterlockedCompareExchangePointer(&g_accept_mutex, m, NULL) != NULL)
        CloseHandle(m);
    return g_accept_mutex;
}

int ols_accept_lock(SOCKET s)
{
    int i, shared = 0;
    HANDLE m;
    DWORD r;
    AcquireSRWLockShared(&g_shared_lock);
    for (i = 0; i < g_nshared; ++i)
        if (g_shared[i] == s)
        {
            shared = 1;
            break;
        }
    ReleaseSRWLockShared(&g_shared_lock);
    if (!shared || (m = accept_mutex()) == NULL)
        return 0;
    r = WaitForSingleObject(m, 0);
    if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED)
        return 1;
    return -1;
}

void ols_accept_unlock(void)
{
    if (g_accept_mutex)
        ReleaseMutex(g_accept_mutex);
}

int ols_handoff_put(int fd, int slot)
{
    int i;
    AcquireSRWLockExclusive(&g_lock);
    for (i = 0; i < g_nout; ++i)
        if (g_out[i].slot == slot)
            break;
    if (i == g_nout)
    {
        if (g_nout >= HANDOFF_MAX)
        {
            ReleaseSRWLockExclusive(&g_lock);
            errno = EMFILE;
            return -1;
        }
        ++g_nout;
    }
    g_out[i].slot = slot;
    g_out[i].fd = fd;
    ReleaseSRWLockExclusive(&g_lock);
    return slot;
}

static int handoff_file(DWORD pid, wchar_t *w, size_t len)
{
    char path[128];
    snprintf(path, sizeof(path), "/tmp/lshttpd/handoff.%lu", (unsigned long)pid);
    return ols_path_to_w(path, w, len, NULL);
}

static int write_handoff(DWORD pid, const int *fds, const int *slots, int n)
{
    wchar_t file[PATH_MAX + 8];
    HANDLE f;
    int i, ok = 1;
    uint32_t hdr[2];
    DWORD w;
    SECURITY_ATTRIBUTES sa;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = ols_private_sd(0);
    if (!sa.lpSecurityDescriptor || handoff_file(pid, file, PATH_MAX + 8) < 0)
        ok = 0;
    if (ok)
        DeleteFileW(file);
    f = ok ? CreateFileW(file, GENERIC_WRITE, 0, &sa, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)
           : INVALID_HANDLE_VALUE;
    if (sa.lpSecurityDescriptor)
        LocalFree(sa.lpSecurityDescriptor);
    if (f == INVALID_HANDLE_VALUE)
        return -1;
    ensure_accept_key();
    hdr[0] = HANDOFF_MAGIC;
    hdr[1] = (uint32_t)n;
    WriteFile(f, hdr, sizeof(hdr), &w, NULL);
    WriteFile(f, g_accept_key, ACCEPT_KEY_LEN, &w, NULL);
    for (i = 0; i < n; ++i)
    {
        handoff_rec rec;
        memset(&rec, 0, sizeof(rec));
        rec.slot = slots[i];
        mark_shared(ols_socket_from_fd(fds[i]));
        if (WSADuplicateSocketW(ols_socket_from_fd(fds[i]), pid, &rec.info) != 0)
            rec.slot = -1;
        WriteFile(f, &rec, sizeof(rec), &w, NULL);
    }
    CloseHandle(f);
    return 0;
}

static int start_suspended(const wchar_t *worker, PROCESS_INFORMATION *pi, int breakaway)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 16];
    STARTUPINFOW si;
    DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
    DWORD n;
    BOOL created;

    n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    swprintf(cmd, MAX_PATH + 16, L"\"%s\" -d", exe);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    SetEnvironmentVariableW(L"OLS_HANDOFF", L"1");
    SetEnvironmentVariableW(L"OLS_WORKER", worker);
    created = (breakaway && CreateProcessW(exe, cmd, NULL, NULL, FALSE, flags | CREATE_BREAKAWAY_FROM_JOB,
                                           NULL, NULL, &si, pi))
              || CreateProcessW(exe, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, pi);
    SetEnvironmentVariableW(L"OLS_HANDOFF", NULL);
    SetEnvironmentVariableW(L"OLS_WORKER", NULL);
    if (!created)
        return ols_set_errno_win();
    return 0;
}

pid_t ols_handoff_restart(void)
{
    PROCESS_INFORMATION pi;
    int fds[HANDOFF_MAX], slots[HANDOFF_MAX], i, n, ok;

    if (start_suspended(NULL, &pi, 1) != 0)
        return -1;
    AcquireSRWLockShared(&g_lock);
    n = g_nout;
    for (i = 0; i < n; ++i)
    {
        fds[i] = g_out[i].fd;
        slots[i] = g_out[i].slot;
    }
    ReleaseSRWLockShared(&g_lock);
    ok = write_handoff(pi.dwProcessId, fds, slots, n) == 0;
    if (!ok)
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        errno = EIO;
        return -1;
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (pid_t)pi.dwProcessId;
}

pid_t ols_worker_spawn(int procNo, const int *fds, int nfds, void *job, void **phProcess)
{
    PROCESS_INFORMATION pi;
    wchar_t num[16];
    int slots[HANDOFF_MAX], i;

    if (procNo < 2 || nfds <= 0 || nfds > HANDOFF_MAX)
    {
        errno = EINVAL;
        return -1;
    }
    swprintf(num, 16, L"%d", procNo);
    if (start_suspended(num, &pi, 0) != 0)
        return -1;
    if (job)
        AssignProcessToJobObject((HANDLE)job, pi.hProcess);
    for (i = 0; i < nfds; ++i)
        slots[i] = 1000 + i;
    if (write_handoff(pi.dwProcessId, fds, slots, nfds) != 0)
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        errno = EIO;
        return -1;
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    *phProcess = pi.hProcess;
    return (pid_t)pi.dwProcessId;
}

int ols_worker_no(void)
{
    static int s_no = -1;
    if (s_no < 0)
    {
        wchar_t v[16];
        DWORD n = GetEnvironmentVariableW(L"OLS_WORKER", v, 16);
        s_no = (n > 0 && n < 16) ? _wtoi(v) : 0;
        if (s_no < 2 || s_no > 32)
            s_no = 0;
        SetEnvironmentVariableW(L"OLS_WORKER", NULL);
        _wputenv_s(L"OLS_WORKER", L"");
    }
    return s_no;
}

static int cmp_slot(const void *a, const void *b)
{
    return ((const int *)a)[0] - ((const int *)b)[0];
}

void ols_handoff_load(void)
{
    wchar_t file[PATH_MAX + 8];
    HANDLE f;
    uint32_t hdr[2];
    DWORD r;
    uint32_t i, nsocks = 0;
    SOCKET socks[HANDOFF_MAX], low[HANDOFF_MAX];
    int slots[HANDOFF_MAX];
    wchar_t flag[4];
    DWORD fl = GetEnvironmentVariableW(L"OLS_HANDOFF", flag, 4);
    SetEnvironmentVariableW(L"OLS_HANDOFF", NULL);
    _wputenv_s(L"OLS_HANDOFF", L"");
    if (fl != 1 || flag[0] != L'1')
        return;
    if (handoff_file(GetCurrentProcessId(), file, PATH_MAX + 8) < 0)
        return;
    f = CreateFileW(file, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    if (!ols_is_private_handle(f)
        || !ReadFile(f, hdr, sizeof(hdr), &r, NULL) || r != sizeof(hdr) || hdr[0] != HANDOFF_MAGIC
        || hdr[1] > HANDOFF_MAX
        || !ReadFile(f, g_accept_key, ACCEPT_KEY_LEN, &r, NULL) || r != ACCEPT_KEY_LEN)
    {
        CloseHandle(f);
        return;
    }
    g_have_key = 1;
    ols_wsa_init();
    for (i = 0; i < hdr[1]; ++i)
    {
        handoff_rec rec;
        if (!ReadFile(f, &rec, sizeof(rec), &r, NULL) || r != sizeof(rec))
            break;
        if (rec.slot < 0)
            continue;
        socks[nsocks] = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                                   &rec.info, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        if (socks[nsocks] == INVALID_SOCKET)
            continue;
        slots[nsocks++] = rec.slot;
    }
    CloseHandle(f);
    for (i = 0; i < nsocks; ++i)
    {
        WSAPROTOCOL_INFOW info;
        low[i] = INVALID_SOCKET;
        if ((uintptr_t)socks[i] > 8)
            continue;
        low[i] = socks[i];
        socks[i] = INVALID_SOCKET;
        if (WSADuplicateSocketW(low[i], GetCurrentProcessId(), &info) == 0)
            socks[i] = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &info,
                                  0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    }
    for (i = 0; i < nsocks; ++i)
        if (low[i] != INVALID_SOCKET)
            closesocket(low[i]);
    ols_reserve_low_handles();
    for (i = 0; i < nsocks; ++i)
    {
        if (socks[i] == INVALID_SOCKET || (uintptr_t)socks[i] <= 8)
            continue;
        g_in[g_nin].slot = slots[i];
        g_in[g_nin].fd = ols_fd_from_socket(socks[i]);
        ols_fd_set_meta(g_in[g_nin].fd, OLS_T_SOCKET | (O_RDWR << OLS_F_ACC_SHIFT));
        mark_shared(socks[i]);
        ++g_nin;
    }
    qsort(g_in, g_nin, sizeof(g_in[0]), cmp_slot);
}

int ols_handoff_next(void)
{
    if (g_inpos >= g_nin)
        return -1;
    return g_in[g_inpos++].fd;
}

int ols_handoff_count(int in)
{
    return in ? g_nin : g_nout;
}
