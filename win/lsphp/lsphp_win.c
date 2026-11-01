#define LSPHP_IO_SHIM
#include "lsphp_win.h"
#include <stdarg.h>
#include <stdio.h>

#undef read
#undef write
#undef close
#undef dup
#undef dup2
#undef open
#undef socket
#undef bind
#undef listen
#undef connect
#undef accept
#undef setsockopt
#undef getsockopt
#undef getpeername
#undef getsockname
#undef shutdown
#undef select
#undef FD_SET
#undef FD_ISSET
#define FD_SET(fd, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_set *)(set))->fd_count; ++__i) \
        if (((fd_set *)(set))->fd_array[__i] == (fd)) \
            break; \
    if (__i == ((fd_set *)(set))->fd_count && ((fd_set *)(set))->fd_count < FD_SETSIZE) \
        ((fd_set *)(set))->fd_array[((fd_set *)(set))->fd_count++] = (fd); \
} while (0)

#define MAX_NB 64
static struct { SOCKET s; int nb; } s_nb[MAX_NB];
static SRWLOCK s_lock = SRWLOCK_INIT;
static HANDLE s_parent;
static pid_t s_parent_pid;

static int is_sock(int fd)
{
    return fd >= 0 && (fd & LSW_SOCK_BIT) != 0;
}

static SOCKET to_sock(int fd)
{
    return (SOCKET)(UINT_PTR)(unsigned int)(fd & ~LSW_SOCK_BIT);
}

static int from_sock(SOCKET s)
{
    if (s == INVALID_SOCKET || ((UINT_PTR)s & ~(UINT_PTR)0x3fffffff) != 0)
        return -1;
    return (int)(UINT_PTR)s | LSW_SOCK_BIT;
}

static int wsa_errno(void)
{
    switch (WSAGetLastError())
    {
    case WSAEWOULDBLOCK: return EAGAIN;
    case WSAEINTR: return EINTR;
    case WSAECONNRESET: return ECONNRESET;
    case WSAECONNABORTED: return ECONNABORTED;
    case WSAESHUTDOWN: return EPIPE;
    case WSAENOTCONN: return ENOTCONN;
    case WSAENOTSOCK: return ENOTSOCK;
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAEINVAL: return EINVAL;
    case WSAEMFILE: return EMFILE;
    case WSAENOBUFS: return ENOBUFS;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAEACCES: return EACCES;
    default: return EIO;
    }
}

static void wsa_init(void)
{
    static LONG done;
    WSADATA d;
    if (InterlockedExchange(&done, 1) == 0)
        WSAStartup(MAKEWORD(2, 2), &d);
}

static void nb_set(SOCKET s, int nb)
{
    int i, free_slot = -1;
    AcquireSRWLockExclusive(&s_lock);
    for (i = 0; i < MAX_NB; ++i)
    {
        if (s_nb[i].s == s)
            break;
        if (free_slot < 0 && s_nb[i].s == 0)
            free_slot = i;
    }
    if (i == MAX_NB)
        i = free_slot;
    if (i >= 0)
    {
        s_nb[i].s = nb >= 0 ? s : 0;
        s_nb[i].nb = nb > 0;
    }
    ReleaseSRWLockExclusive(&s_lock);
}

static int nb_get(SOCKET s)
{
    int i, nb = 0;
    AcquireSRWLockShared(&s_lock);
    for (i = 0; i < MAX_NB; ++i)
        if (s_nb[i].s == s)
        {
            nb = s_nb[i].nb;
            break;
        }
    ReleaseSRWLockShared(&s_lock);
    return nb;
}

ssize_t lsw_read(int fd, void *buf, size_t len)
{
    int r;
    if (!is_sock(fd))
        return _read(fd, buf, (unsigned int)(len > INT_MAX ? INT_MAX : len));
    r = recv(to_sock(fd), (char *)buf, len > INT_MAX ? INT_MAX : (int)len, 0);
    if (r == SOCKET_ERROR)
    {
        int e = wsa_errno();
        if (e == ECONNRESET || e == ECONNABORTED || e == EPIPE)
            return 0;
        errno = e;
        return -1;
    }
    return r;
}

ssize_t lsw_write(int fd, const void *buf, size_t len)
{
    int r;
    if (!is_sock(fd))
        return _write(fd, buf, (unsigned int)(len > INT_MAX ? INT_MAX : len));
    r = send(to_sock(fd), (const char *)buf, len > INT_MAX ? INT_MAX : (int)len, 0);
    if (r == SOCKET_ERROR)
    {
        errno = wsa_errno();
        return -1;
    }
    return r;
}

ssize_t lsw_writev(int fd, const struct iovec *iov, int n)
{
    WSABUF stack[64], *b = stack;
    DWORD sent = 0;
    int i;
    if (!is_sock(fd))
    {
        ssize_t total = 0;
        for (i = 0; i < n; ++i)
        {
            ssize_t w = lsw_write(fd, iov[i].iov_base, iov[i].iov_len);
            if (w < 0)
                return total ? total : -1;
            total += w;
            if ((size_t)w < iov[i].iov_len)
                break;
        }
        return total;
    }
    if (n > 64)
    {
        b = (WSABUF *)malloc(n * sizeof(WSABUF));
        if (!b)
        {
            errno = ENOMEM;
            return -1;
        }
    }
    for (i = 0; i < n; ++i)
    {
        b[i].buf = (char *)iov[i].iov_base;
        b[i].len = (ULONG)iov[i].iov_len;
    }
    i = WSASend(to_sock(fd), b, (DWORD)n, &sent, 0, NULL, NULL);
    if (b != stack)
        free(b);
    if (i == SOCKET_ERROR)
    {
        errno = wsa_errno();
        return -1;
    }
    return (ssize_t)sent;
}

int lsw_close(int fd)
{
    if (!is_sock(fd))
        return _close(fd);
    nb_set(to_sock(fd), -1);
    if (closesocket(to_sock(fd)) == SOCKET_ERROR)
    {
        errno = wsa_errno();
        return -1;
    }
    return 0;
}

static SOCKET dup_sock(SOCKET s)
{
    WSAPROTOCOL_INFOW info;
    if (WSADuplicateSocketW(s, GetCurrentProcessId(), &info) != 0)
        return INVALID_SOCKET;
    return WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &info, 0,
                      WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
}

static SOCKET stdin_sock(void)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    int type;
    int len = sizeof(type);
    wsa_init();
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return INVALID_SOCKET;
    if (getsockopt((SOCKET)h, SOL_SOCKET, SO_TYPE, (char *)&type, &len) != 0)
        return INVALID_SOCKET;
    return (SOCKET)h;
}

int lsw_dup(int fd)
{
    SOCKET s;
    if (fd == STDIN_FILENO && (s = stdin_sock()) != INVALID_SOCKET)
    {
        SOCKET d = dup_sock(s);
        int r = from_sock(d);
        if (r < 0)
        {
            if (d != INVALID_SOCKET)
                closesocket(d);
            errno = EMFILE;
        }
        return r;
    }
    if (is_sock(fd))
    {
        int r = from_sock(dup_sock(to_sock(fd)));
        if (r < 0)
            errno = EMFILE;
        return r;
    }
    return _dup(fd);
}

int lsw_dup2(int fd, int fd2)
{
    if (is_sock(fd) || is_sock(fd2))
    {
        errno = EBADF;
        return -1;
    }
    if (fd2 == STDIN_FILENO && stdin_sock() != INVALID_SOCKET)
    {
        int r = _dup2(fd, fd2);
        SetStdHandle(STD_INPUT_HANDLE, (HANDLE)_get_osfhandle(fd2));
        return r;
    }
    return _dup2(fd, fd2);
}

int lsw_open(const char *path, int flags, ...)
{
    int mode = 0;
    if (flags & _O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    if (strcmp(path, "/dev/null") == 0)
        path = "NUL";
    return _open(path, (flags & ~O_NONBLOCK) | _O_BINARY, mode ? mode : (_S_IREAD | _S_IWRITE));
}

int lsw_fcntl(int fd, int cmd, ...)
{
    va_list ap;
    int arg = 0;
    va_start(ap, cmd);
    if (cmd == F_SETFL || cmd == F_SETFD)
        arg = va_arg(ap, int);
    va_end(ap);
    if (!is_sock(fd))
        return 0;
    if (cmd == F_GETFL)
        return nb_get(to_sock(fd)) ? O_NONBLOCK : 0;
    if (cmd == F_SETFL)
    {
        u_long nb = (arg & O_NONBLOCK) ? 1 : 0;
        if (ioctlsocket(to_sock(fd), FIONBIO, &nb) != 0)
        {
            errno = wsa_errno();
            return -1;
        }
        nb_set(to_sock(fd), (int)nb);
    }
    return 0;
}

int lsw_accept(int fd, struct sockaddr *a, int *len)
{
    SOCKET s = accept(to_sock(fd), a, len);
    int r;
    if (s == INVALID_SOCKET)
    {
        errno = wsa_errno();
        return -1;
    }
    SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
    {
        u_long nb = 0;
        ioctlsocket(s, FIONBIO, &nb);
        nb_set(s, 0);
    }
    r = from_sock(s);
    if (r < 0)
    {
        closesocket(s);
        errno = EMFILE;
    }
    return r;
}

int lsw_socket(int d, int t, int p)
{
    SOCKET s;
    int r;
    wsa_init();
    s = WSASocketW(d, t, p, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (s == INVALID_SOCKET)
    {
        errno = wsa_errno();
        return -1;
    }
    r = from_sock(s);
    if (r < 0)
    {
        closesocket(s);
        errno = EMFILE;
    }
    return r;
}

#define SOCK_CALL(expr) do { if ((expr) == SOCKET_ERROR) { errno = wsa_errno(); return -1; } return 0; } while (0)

int lsw_bind(int fd, const struct sockaddr *a, int len)            { SOCK_CALL(bind(to_sock(fd), a, len)); }
int lsw_listen(int fd, int n)                                      { SOCK_CALL(listen(to_sock(fd), n)); }
int lsw_connect(int fd, const struct sockaddr *a, int len)         { SOCK_CALL(connect(to_sock(fd), a, len)); }
int lsw_getsockname(int fd, struct sockaddr *a, int *len)          { SOCK_CALL(getsockname(to_sock(fd), a, len)); }
int lsw_shutdown(int fd, int how)                                  { SOCK_CALL(shutdown(to_sock(fd), how)); }

int lsw_getpeername(int fd, struct sockaddr *a, int *len)
{
    if (!is_sock(fd))
    {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_PIPE)
            return 0;
        errno = ENOTCONN;
        return -1;
    }
    SOCK_CALL(getpeername(to_sock(fd), a, len));
}

int lsw_setsockopt(int fd, int l, int n, const void *v, int len)
{
    if (l == SOL_SOCKET && n == SO_REUSEADDR)
        return 0;
    SOCK_CALL(setsockopt(to_sock(fd), l, n, (const char *)v, len));
}

int lsw_getsockopt(int fd, int l, int n, void *v, int *len)        { SOCK_CALL(getsockopt(to_sock(fd), l, n, (char *)v, len)); }

void lsw_fd_set(int fd, fd_set *s)
{
    SOCKET so = to_sock(fd);
    FD_SET(so, s);
}

int lsw_fd_isset(int fd, fd_set *s)
{
    return __WSAFDIsSet(to_sock(fd), s);
}

int lsw_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    int ret;
    (void)n;
    if ((!r || r->fd_count == 0) && (!w || w->fd_count == 0) && (!e || e->fd_count == 0))
    {
        Sleep(t ? (DWORD)(t->tv_sec * 1000 + t->tv_usec / 1000) : INFINITE);
        return 0;
    }
    ret = select(0, r, w, e, t);
    if (ret == SOCKET_ERROR)
    {
        errno = wsa_errno();
        return -1;
    }
    return ret;
}

ssize_t lsw_sendfile(int out, int in, long long *off, size_t size)
{
    char buf[65536];
    size_t chunk = size > sizeof(buf) ? sizeof(buf) : size;
    long long pos = off ? *off : _telli64(in);
    int got;
    ssize_t sent;
    if (chunk == 0)
        return 0;
    if (_lseeki64(in, pos, SEEK_SET) < 0)
        return -1;
    got = _read(in, buf, (unsigned int)chunk);
    if (got <= 0)
        return got;
    sent = lsw_write(out, buf, (size_t)got);
    if (sent > 0)
    {
        if (off)
            *off += sent;
        else
            _lseeki64(in, pos + sent, SEEK_SET);
    }
    return sent;
}

static void (*s_handlers[64])(int);

int lsw_sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{
    if (old)
    {
        memset(old, 0, sizeof(*old));
        old->sa_handler = (sig >= 0 && sig < 64 && s_handlers[sig]) ? s_handlers[sig] : SIG_DFL;
    }
    if (!act || sig < 0 || sig >= 64)
        return 0;
    s_handlers[sig] = act->sa_handler;
    if (sig == SIGINT || sig == SIGTERM || sig == SIGABRT || sig == SIGSEGV || sig == SIGILL
        || sig == SIGFPE)
        signal(sig, act->sa_handler);
    return 0;
}

int lsw_sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
    (void)how; (void)set;
    if (old)
        *old = 0;
    return 0;
}

pid_t lsw_fork(void)
{
    errno = ENOSYS;
    return -1;
}

typedef LONG (NTAPI *nqip_fn)(HANDLE, int, PVOID, ULONG, PULONG);

pid_t lsw_getppid(void)
{
    if (!s_parent_pid)
    {
        struct { PVOID r1; PVOID peb; PVOID r2[2]; ULONG_PTR pid; ULONG_PTR ppid; } pbi;
        nqip_fn fn = (nqip_fn)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                     "NtQueryInformationProcess");
        ULONG n;
        if (fn && fn(GetCurrentProcess(), 0, &pbi, sizeof(pbi), &n) == 0)
        {
            s_parent_pid = (pid_t)pbi.ppid;
            s_parent = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pbi.ppid);
        }
        if (!s_parent_pid)
            s_parent_pid = 1;
    }
    if (s_parent && WaitForSingleObject(s_parent, 0) == WAIT_OBJECT_0)
        return 1;
    return s_parent_pid;
}

pid_t lsw_setsid(void)                       { return _getpid(); }
pid_t lsw_getpgrp(void)                      { return _getpid(); }
int lsw_setpgid(pid_t pid, pid_t pg)         { (void)pid; (void)pg; return 0; }
unsigned int lsw_alarm(unsigned int s)       { (void)s; return 0; }
int lsw_sched_yield(void)                    { SwitchToThread(); return 0; }
uid_t lsw_getuid(void)                       { return 1000; }
int lsw_setuid(uid_t u)                      { (void)u; errno = EPERM; return -1; }
int lsw_setgid(gid_t g)                      { (void)g; errno = EPERM; return -1; }
int lsw_initgroups(const char *u, gid_t g)   { (void)u; (void)g; errno = EPERM; return -1; }
int lsw_chroot(const char *p)                { (void)p; errno = EPERM; return -1; }
struct passwd *lsw_getpwnam(const char *n)   { (void)n; return NULL; }
struct passwd *lsw_getpwuid(uid_t u)         { (void)u; return NULL; }
struct group *lsw_getgrnam(const char *n)    { (void)n; return NULL; }
int lsw_setrlimit(int r, const struct rlimit *l) { (void)r; (void)l; return 0; }
void *lsw_dlopen(const char *f, int m)       { (void)f; (void)m; return NULL; }
void *lsw_dlsym(void *h, const char *s)      { (void)h; (void)s; return NULL; }
int lsw_munmap(void *a, size_t len)          { (void)len; return VirtualFree(a, 0, MEM_RELEASE) ? 0 : -1; }

int lsw_kill(pid_t pid, int sig)
{
    HANDLE h;
    if (pid <= 0)
        return 0;
    h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h)
    {
        errno = ESRCH;
        return -1;
    }
    if (sig != 0)
        TerminateProcess(h, 128 + sig);
    CloseHandle(h);
    return 0;
}

pid_t lsw_waitpid(pid_t pid, int *status, int options)
{
    (void)pid; (void)options;
    if (status)
        *status = 0;
    errno = ECHILD;
    return -1;
}

int lsw_getrlimit(int r, struct rlimit *l)
{
    (void)r;
    l->rlim_cur = l->rlim_max = RLIM_INFINITY;
    return 0;
}

void *lsw_mmap(void *a, size_t len, int prot, int flags, int fd, long long off)
{
    void *p;
    (void)a; (void)prot; (void)off;
    if (!(flags & MAP_ANON) || fd != -1)
        return MAP_FAILED;
    p = VirtualAlloc(NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return p ? p : MAP_FAILED;
}

int lsw_setgroups(int n, const void *g)
{
    (void)n; (void)g;
    errno = EPERM;
    return -1;
}

ssize_t gsendfile(int out, int in, off_t *off, size_t size)
{
    long long o = off ? *off : 0;
    ssize_t r = lsw_sendfile(out, in, off ? &o : NULL, size);
    if (off && r > 0)
        *off = (off_t)o;
    return r;
}

void lsw_set_stdin_socket(int fd)
{
    if (is_sock(fd))
        SetStdHandle(STD_INPUT_HANDLE, (HANDLE)to_sock(fd));
}

void lsw_exit_now(int code)
{
    fflush(NULL);
    TerminateProcess(GetCurrentProcess(), (UINT)code);
    _exit(code);
}
