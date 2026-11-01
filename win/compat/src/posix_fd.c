#include "compat_internal.h"
#include <sys/epoll.h>

#define META_CHUNK_BITS 12
#define META_CHUNK (1 << META_CHUNK_BITS)
#define META_TOP (1 << 14)

static uint16_t *volatile g_meta[META_TOP];
static INIT_ONCE g_wsa_once = INIT_ONCE_STATIC_INIT;
static LPFN_WSARECVMSG g_recvmsg;
static LPFN_WSASENDMSG g_sendmsg;

static uint16_t *meta_slot(int fd, int create)
{
    unsigned top, idx;
    uint16_t *chunk;
    if (fd < 0)
        return NULL;
    top = (unsigned)fd >> META_CHUNK_BITS;
    idx = (unsigned)fd & (META_CHUNK - 1);
    if (top >= META_TOP)
        return NULL;
    chunk = g_meta[top];
    if (!chunk)
    {
        uint16_t *fresh;
        if (!create)
            return NULL;
        fresh = (uint16_t *)calloc(META_CHUNK, sizeof(uint16_t));
        if (!fresh)
            return NULL;
        chunk = (uint16_t *)InterlockedCompareExchangePointer(
                    (PVOID volatile *)&g_meta[top], fresh, NULL);
        if (chunk)
            free(fresh);
        else
            chunk = fresh;
    }
    return &chunk[idx];
}

uint16_t ols_fd_meta(int fd)
{
    uint16_t *p = meta_slot(fd, 0);
    return p ? *p : 0;
}

void ols_fd_set_meta(int fd, uint16_t meta)
{
    uint16_t *p = meta_slot(fd, 1);
    if (p)
        *p = meta | OLS_F_KNOWN;
}

void ols_fd_clear(int fd)
{
    uint16_t *p = meta_slot(fd, 0);
    if (p)
        *p = 0;
}

static BOOL CALLBACK wsa_init_cb(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    WSADATA d;
    SOCKET s;
    GUID g1 = WSAID_WSARECVMSG;
    GUID g2 = WSAID_WSASENDMSG;
    DWORD n;
    WSAStartup(MAKEWORD(2, 2), &d);
    s = WSASocketW(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
    if (s != INVALID_SOCKET)
    {
        WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &g1, sizeof(g1),
                 &g_recvmsg, sizeof(g_recvmsg), &n, NULL, NULL);
        WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &g2, sizeof(g2),
                 &g_sendmsg, sizeof(g_sendmsg), &n, NULL, NULL);
        closesocket(s);
    }
    return TRUE;
}

void ols_wsa_init(void)
{
    InitOnceExecuteOnce(&g_wsa_once, wsa_init_cb, NULL, NULL);
}

int ols_errno_from_wsa(int e)
{
    switch (e)
    {
    case WSAEWOULDBLOCK: return EAGAIN;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAEALREADY: return EALREADY;
    case WSAENOTSOCK: return ENOTSOCK;
    case WSAEDESTADDRREQ: return EDESTADDRREQ;
    case WSAEMSGSIZE: return EMSGSIZE;
    case WSAEPROTOTYPE: return EPROTOTYPE;
    case WSAENOPROTOOPT: return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
    case WSAESOCKTNOSUPPORT: return ESOCKTNOSUPPORT;
    case WSAEOPNOTSUPP: return EOPNOTSUPP;
    case WSAEPFNOSUPPORT: return EPFNOSUPPORT;
    case WSAEAFNOSUPPORT: return EAFNOSUPPORT;
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
    case WSAENETDOWN: return ENETDOWN;
    case WSAENETUNREACH: return ENETUNREACH;
    case WSAENETRESET: return ENETRESET;
    case WSAECONNABORTED: return ECONNABORTED;
    case WSAECONNRESET: return ECONNRESET;
    case WSAENOBUFS: return ENOBUFS;
    case WSAEISCONN: return EISCONN;
    case WSAENOTCONN: return ENOTCONN;
    case WSAESHUTDOWN: return EPIPE;
    case WSAETOOMANYREFS: return ETOOMANYREFS;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAELOOP: return ELOOP;
    case WSAENAMETOOLONG: return ENAMETOOLONG;
    case WSAEHOSTDOWN: return EHOSTDOWN;
    case WSAEHOSTUNREACH: return EHOSTUNREACH;
    case WSAENOTEMPTY: return ENOTEMPTY;
    case WSAEUSERS: return EUSERS;
    case WSAEDQUOT: return EDQUOT;
    case WSAESTALE: return ESTALE;
    case WSAEREMOTE: return EREMOTE;
    case WSAEINTR: return EINTR;
    case WSAEBADF: return EBADF;
    case WSAEACCES: return EACCES;
    case WSAEFAULT: return EFAULT;
    case WSAEINVAL: return EINVAL;
    case WSAEMFILE: return EMFILE;
    case WSA_NOT_ENOUGH_MEMORY: return ENOMEM;
    case WSANOTINITIALISED: return ENETDOWN;
    default: return EIO;
    }
}

int ols_errno_from_win(DWORD e)
{
    switch (e)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_INVALID_NAME:
    case ERROR_DIRECTORY:
    case ERROR_NOT_FOUND:
        return ENOENT;
    case ERROR_TOO_MANY_OPEN_FILES: return EMFILE;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_CANNOT_MAKE:
    case ERROR_DELETE_PENDING:
        return EACCES;
    case ERROR_INVALID_HANDLE: return EBADF;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_COMMITMENT_LIMIT:
        return ENOMEM;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_DIR_NOT_EMPTY: return ENOTEMPTY;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return ENOSPC;
    case ERROR_BROKEN_PIPE:
    case ERROR_NO_DATA:
        return EPIPE;
    case ERROR_NOT_SAME_DEVICE: return EXDEV;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BUFFER_OVERFLOW:
        return ENAMETOOLONG;
    case ERROR_INVALID_PARAMETER:
    case ERROR_NEGATIVE_SEEK:
    case ERROR_INVALID_FUNCTION:
        return EINVAL;
    case ERROR_WRITE_PROTECT: return EROFS;
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
        return ENOSYS;
    case ERROR_IO_PENDING: return EAGAIN;
    case ERROR_OPERATION_ABORTED: return EINTR;
    case ERROR_CANT_RESOLVE_FILENAME: return ELOOP;
    case ERROR_NOT_A_REPARSE_POINT: return EINVAL;
    case ERROR_FILE_TOO_LARGE: return EFBIG;
    default: return EIO;
    }
}

int ols_set_errno_wsa(void)
{
    errno = ols_errno_from_wsa(WSAGetLastError());
    return -1;
}

int ols_set_errno_win(void)
{
    errno = ols_errno_from_win(GetLastError());
    return -1;
}

static HANDLE std_handle(int fd)
{
    static const DWORD ids[3] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    return GetStdHandle(ids[fd]);
}

int ols_fd_from_handle(HANDLE h)
{
    uintptr_t v;
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return -1;
    v = (uintptr_t)h;
    if ((v >> 2) < 3)
    {
        HANDLE d;
        if (DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d,
                            0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            CloseHandle(h);
            return ols_fd_from_handle(d);
        }
    }
    return (int)(v >> 2);
}

HANDLE ols_handle_from_fd(int fd)
{
    if (fd < 0)
        return INVALID_HANDLE_VALUE;
    if (fd < 3)
        return std_handle(fd);
    return (HANDLE)((uintptr_t)(unsigned)fd << 2);
}

int ols_fd_from_socket(SOCKET s)
{
    if (s == INVALID_SOCKET)
        return -1;
    return (int)(s >> 2);
}

SOCKET ols_socket_from_fd(int fd)
{
    if (fd < 0)
        return INVALID_SOCKET;
    return (SOCKET)((uintptr_t)(unsigned)fd << 2);
}

int ols_fd_type(int fd)
{
    uint16_t m = ols_fd_meta(fd);
    HANDLE h;
    DWORD t;
    int type, len;
    if (m & OLS_F_KNOWN)
        return m & OLS_T_MASK;
    if (fd < 0)
        return OLS_T_UNKNOWN;
    h = ols_handle_from_fd(fd);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return OLS_T_UNKNOWN;
    t = GetFileType(h);
    if (t == FILE_TYPE_DISK)
        type = OLS_T_FILE;
    else if (t == FILE_TYPE_CHAR)
        type = OLS_T_CHAR;
    else
    {
        len = sizeof(int);
        ols_wsa_init();
        if (getsockopt(ols_socket_from_fd(fd), SOL_SOCKET, SO_TYPE, (char *)&type, &len) == 0)
            type = OLS_T_SOCKET;
        else if (t == FILE_TYPE_PIPE)
            type = OLS_T_PIPE;
        else
            return OLS_T_UNKNOWN;
    }
    ols_fd_set_meta(fd, (uint16_t)type);
    return type;
}

int ols_fd_is_socket(int fd)
{
    return ols_fd_type(fd) == OLS_T_SOCKET;
}

static int fill_random(void *buf, size_t n)
{
    while (n > 0)
    {
        ULONG chunk = n > 0x40000000 ? 0x40000000 : (ULONG)n;
        if (BCryptGenRandom(NULL, (PUCHAR)buf, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        {
            errno = EIO;
            return -1;
        }
        buf = (char *)buf + chunk;
        n -= chunk;
    }
    return 0;
}

ssize_t read(int fd, void *buf, size_t n)
{
    int type = ols_fd_type(fd);
    DWORD got = 0;
    HANDLE h;
    if (type == OLS_T_SOCKET)
    {
        int r = recv(ols_socket_from_fd(fd), (char *)buf, n > INT_MAX ? INT_MAX : (int)n, 0);
        if (r == SOCKET_ERROR)
        {
            int e = WSAGetLastError();
            if (e == WSAESHUTDOWN || e == WSAECONNABORTED)
                return 0;
            errno = ols_errno_from_wsa(e);
            return -1;
        }
        return r;
    }
    if (type == OLS_T_RANDOM)
        return fill_random(buf, n) == 0 ? (ssize_t)n : -1;
    if (type == OLS_T_NULL)
        return 0;
    if (type == OLS_T_EPOLL || type == OLS_T_UNKNOWN)
    {
        errno = EBADF;
        return -1;
    }
    h = ols_handle_from_fd(fd);
    if (!ReadFile(h, buf, n > 0x7fffffff ? 0x7fffffff : (DWORD)n, &got, NULL))
    {
        DWORD e = GetLastError();
        if (e == ERROR_BROKEN_PIPE || e == ERROR_HANDLE_EOF)
            return 0;
        errno = ols_errno_from_win(e);
        return -1;
    }
    return got;
}

ssize_t write(int fd, const void *buf, size_t n)
{
    int type = ols_fd_type(fd);
    DWORD put = 0;
    HANDLE h;
    if (type == OLS_T_SOCKET)
    {
        int r = send(ols_socket_from_fd(fd), (const char *)buf, n > INT_MAX ? INT_MAX : (int)n, 0);
        if (r == SOCKET_ERROR)
            return ols_set_errno_wsa();
        return r;
    }
    if (type == OLS_T_NULL || type == OLS_T_RANDOM)
        return (ssize_t)n;
    if (type == OLS_T_EPOLL || type == OLS_T_UNKNOWN)
    {
        errno = EBADF;
        return -1;
    }
    h = ols_handle_from_fd(fd);
    if ((ols_fd_meta(fd) & OLS_F_APPEND) && type == OLS_T_FILE)
    {
        OVERLAPPED ov;
        memset(&ov, 0, sizeof(ov));
        ov.Offset = 0xffffffff;
        ov.OffsetHigh = 0xffffffff;
        if (!WriteFile(h, buf, n > 0x7fffffff ? 0x7fffffff : (DWORD)n, &put, &ov))
            return ols_set_errno_win();
        return put;
    }
    if (!WriteFile(h, buf, n > 0x7fffffff ? 0x7fffffff : (DWORD)n, &put, NULL))
        return ols_set_errno_win();
    return put;
}

int close(int fd)
{
    int type;
    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    type = ols_fd_type(fd);
    ols_fd_clear(fd);
    if (type == OLS_T_SOCKET)
    {
        if (closesocket(ols_socket_from_fd(fd)) == SOCKET_ERROR)
            return ols_set_errno_wsa();
        return 0;
    }
    if (type == OLS_T_EPOLL)
        return wep_epoll_close(ols_handle_from_fd(fd));
    if (fd < 3)
    {
        static const DWORD ids[3] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
        HANDLE h = GetStdHandle(ids[fd]);
        SetStdHandle(ids[fd], NULL);
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        return 0;
    }
    if (!CloseHandle(ols_handle_from_fd(fd)))
        return ols_set_errno_win();
    return 0;
}

int open(const char *path, int flags, ...)
{
    DWORD access = 0, disp, attrs, share;
    int acc = flags & O_ACCMODE, special = 0, fd;
    uint16_t meta;
    HANDLE h;
    SECURITY_ATTRIBUTES sa, *psa = NULL;
    if (flags & O_CREAT)
    {
        va_list ap;
        int mode;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
        if ((mode & 077) == 0)
        {
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = FALSE;
            sa.lpSecurityDescriptor = ols_private_sd(0);
            if (!sa.lpSecurityDescriptor)
            {
                errno = EACCES;
                return -1;
            }
            psa = &sa;
        }
    }
    if (acc == O_RDONLY)
        access = GENERIC_READ;
    else if (acc == O_WRONLY)
        access = (flags & O_APPEND) ? (FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE)
                                    : GENERIC_WRITE;
    else
        access = (flags & O_APPEND) ? (FILE_GENERIC_READ | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES)
                                    : (GENERIC_READ | GENERIC_WRITE);
    if (flags & O_PATH)
        access = FILE_READ_ATTRIBUTES;
    if ((flags & O_CREAT) && (flags & O_EXCL))
        disp = CREATE_NEW;
    else if ((flags & O_CREAT) && (flags & O_TRUNC))
        disp = CREATE_ALWAYS;
    else if (flags & O_CREAT)
        disp = OPEN_ALWAYS;
    else if (flags & O_TRUNC)
        disp = TRUNCATE_EXISTING;
    else
        disp = OPEN_EXISTING;
    if ((flags & O_TRUNC) && (flags & O_APPEND) && acc != O_RDONLY)
        access |= GENERIC_WRITE;
    attrs = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS;
    if (flags & O_NOFOLLOW)
        attrs |= FILE_FLAG_OPEN_REPARSE_POINT;
    if (flags & (O_SYNC | O_DSYNC))
        attrs |= FILE_FLAG_WRITE_THROUGH;
    share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    h = ols_open_path_sa(path, access, share, disp, attrs, &special, psa);
    if (psa)
        LocalFree(sa.lpSecurityDescriptor);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    fd = ols_fd_from_handle(h);
    if (special == OLS_SPECIAL_RANDOM)
        meta = OLS_T_RANDOM;
    else if (special == OLS_SPECIAL_NULL)
        meta = OLS_T_NULL;
    else
    {
        BY_HANDLE_FILE_INFORMATION bi;
        meta = OLS_T_FILE;
        if (GetFileInformationByHandle(h, &bi)
            && (bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (acc != O_RDONLY)
            {
                CloseHandle(h);
                errno = EISDIR;
                return -1;
            }
            meta |= OLS_F_DIR;
        }
        else if (flags & O_DIRECTORY)
        {
            CloseHandle(h);
            errno = ENOTDIR;
            return -1;
        }
    }
    if (flags & O_APPEND)
        meta |= OLS_F_APPEND;
    if (flags & O_CLOEXEC)
        meta |= OLS_F_CLOEXEC;
    if (flags & O_NONBLOCK)
        meta |= OLS_F_NONBLOCK;
    meta |= (uint16_t)(acc << OLS_F_ACC_SHIFT);
    ols_fd_set_meta(fd, meta);
    return fd;
}

int openat(int dirfd, const char *path, int flags, ...)
{
    va_list ap;
    int mode;
    char full[PATH_MAX * 3];
    wchar_t wdir[PATH_MAX];
    DWORD n;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
    if (dirfd == AT_FDCWD || path[0] == '/' || path[0] == '\\'
        || (path[0] && path[1] == ':'))
        return open(path, flags, mode);
    n = GetFinalPathNameByHandleW(ols_handle_from_fd(dirfd), wdir, PATH_MAX,
                                  FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (n == 0 || n >= PATH_MAX)
        return ols_set_errno_win();
    if (ols_w_to_path(wdir, full, sizeof(full) - 2) < 0)
        return -1;
    strcat(full, "/");
    if (strlen(full) + strlen(path) >= sizeof(full))
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcat(full, path);
    return open(full, flags, mode);
}

int creat(const char *path, mode_t mode)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

off_t lseek(int fd, off_t off, int whence)
{
    LARGE_INTEGER d, r;
    DWORD m;
    if (ols_fd_type(fd) == OLS_T_SOCKET)
    {
        errno = ESPIPE;
        return -1;
    }
    m = whence == SEEK_SET ? FILE_BEGIN : whence == SEEK_CUR ? FILE_CURRENT : FILE_END;
    d.QuadPart = off;
    if (!SetFilePointerEx(ols_handle_from_fd(fd), d, &r, m))
        return ols_set_errno_win();
    return r.QuadPart;
}

ssize_t pread(int fd, void *buf, size_t n, off_t off)
{
    OVERLAPPED ov;
    DWORD got = 0;
    int type = ols_fd_type(fd);
    if (type == OLS_T_SOCKET)
    {
        errno = ESPIPE;
        return -1;
    }
    if (type == OLS_T_RANDOM || type == OLS_T_NULL)
        return read(fd, buf, n);
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)(off & 0xffffffff);
    ov.OffsetHigh = (DWORD)((uint64_t)off >> 32);
    if (!ReadFile(ols_handle_from_fd(fd), buf, n > 0x7fffffff ? 0x7fffffff : (DWORD)n, &got, &ov))
    {
        DWORD e = GetLastError();
        if (e == ERROR_HANDLE_EOF)
            return 0;
        errno = ols_errno_from_win(e);
        return -1;
    }
    return got;
}

ssize_t pwrite(int fd, const void *buf, size_t n, off_t off)
{
    OVERLAPPED ov;
    DWORD put = 0;
    if (ols_fd_type(fd) == OLS_T_SOCKET)
    {
        errno = ESPIPE;
        return -1;
    }
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)(off & 0xffffffff);
    ov.OffsetHigh = (DWORD)((uint64_t)off >> 32);
    if (!WriteFile(ols_handle_from_fd(fd), buf, n > 0x7fffffff ? 0x7fffffff : (DWORD)n, &put, &ov))
        return ols_set_errno_win();
    return put;
}

#define OLS_MAX_WSABUF 64

ssize_t readv(int fd, const struct iovec *iov, int cnt)
{
    if (ols_fd_type(fd) == OLS_T_SOCKET)
    {
        WSABUF bufs[OLS_MAX_WSABUF];
        DWORD got = 0, flags = 0;
        int i;
        if (cnt > OLS_MAX_WSABUF)
            cnt = OLS_MAX_WSABUF;
        for (i = 0; i < cnt; ++i)
        {
            bufs[i].buf = (char *)iov[i].iov_base;
            bufs[i].len = iov[i].iov_len > 0x7fffffff ? 0x7fffffff : (ULONG)iov[i].iov_len;
        }
        if (WSARecv(ols_socket_from_fd(fd), bufs, cnt, &got, &flags, NULL, NULL) == SOCKET_ERROR)
        {
            int e = WSAGetLastError();
            if (e == WSAESHUTDOWN || e == WSAECONNABORTED)
                return 0;
            errno = ols_errno_from_wsa(e);
            return -1;
        }
        return got;
    }
    else
    {
        ssize_t total = 0, r;
        int i;
        for (i = 0; i < cnt; ++i)
        {
            if (iov[i].iov_len == 0)
                continue;
            r = read(fd, iov[i].iov_base, iov[i].iov_len);
            if (r < 0)
                return total ? total : r;
            total += r;
            if ((size_t)r < iov[i].iov_len)
                break;
        }
        return total;
    }
}

ssize_t writev(int fd, const struct iovec *iov, int cnt)
{
    if (ols_fd_type(fd) == OLS_T_SOCKET)
    {
        WSABUF bufs[OLS_MAX_WSABUF];
        DWORD put = 0;
        int i;
        if (cnt > OLS_MAX_WSABUF)
            cnt = OLS_MAX_WSABUF;
        for (i = 0; i < cnt; ++i)
        {
            bufs[i].buf = (char *)iov[i].iov_base;
            bufs[i].len = iov[i].iov_len > 0x7fffffff ? 0x7fffffff : (ULONG)iov[i].iov_len;
        }
        if (WSASend(ols_socket_from_fd(fd), bufs, cnt, &put, 0, NULL, NULL) == SOCKET_ERROR)
            return ols_set_errno_wsa();
        return put;
    }
    else
    {
        ssize_t total = 0, r;
        size_t len = 0;
        char *tmp, *p;
        int i;
        if (cnt == 1)
            return write(fd, iov[0].iov_base, iov[0].iov_len);
        for (i = 0; i < cnt; ++i)
            len += iov[i].iov_len;
        if (len <= 65536 || (ols_fd_meta(fd) & OLS_F_APPEND))
        {
            tmp = (char *)malloc(len ? len : 1);
            if (!tmp)
            {
                errno = ENOMEM;
                return -1;
            }
            for (p = tmp, i = 0; i < cnt; ++i)
            {
                memcpy(p, iov[i].iov_base, iov[i].iov_len);
                p += iov[i].iov_len;
            }
            r = write(fd, tmp, len);
            free(tmp);
            return r;
        }
        for (i = 0; i < cnt; ++i)
        {
            if (iov[i].iov_len == 0)
                continue;
            r = write(fd, iov[i].iov_base, iov[i].iov_len);
            if (r < 0)
                return total ? total : r;
            total += r;
            if ((size_t)r < iov[i].iov_len)
                break;
        }
        return total;
    }
}

int dup(int fd)
{
    int type = ols_fd_type(fd), nfd;
    uint16_t meta = ols_fd_meta(fd);
    if (type == OLS_T_SOCKET)
    {
        WSAPROTOCOL_INFOW pi;
        SOCKET s;
        if (WSADuplicateSocketW(ols_socket_from_fd(fd), GetCurrentProcessId(), &pi) == SOCKET_ERROR)
            return ols_set_errno_wsa();
        s = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &pi, 0,
                       WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        if (s == INVALID_SOCKET)
            return ols_set_errno_wsa();
        nfd = ols_fd_from_socket(s);
    }
    else
    {
        HANDLE d;
        if (!DuplicateHandle(GetCurrentProcess(), ols_handle_from_fd(fd), GetCurrentProcess(),
                             &d, 0, FALSE, DUPLICATE_SAME_ACCESS))
            return ols_set_errno_win();
        nfd = ols_fd_from_handle(d);
    }
    ols_fd_set_meta(nfd, (uint16_t)(meta & ~(OLS_F_CLOEXEC | OLS_F_KNOWN)));
    return nfd;
}

static void ignore_invalid_param(const wchar_t *e, const wchar_t *f, const wchar_t *file,
                                 unsigned int line, uintptr_t r)
{
    (void)e; (void)f; (void)file; (void)line; (void)r;
}

int dup2(int fd, int fd2)
{
    static const DWORD ids[3] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    HANDLE d;
    if (fd == fd2)
        return fd2;
    if (fd2 >= 300 && ols_fd_type(fd) == OLS_T_SOCKET)
    {
        extern int ols_handoff_put(int fd, int slot);
        return ols_handoff_put(fd, fd2);
    }
    if (fd2 < 0 || fd2 > 2)
    {
        errno = EBADF;
        return -1;
    }
    if (!DuplicateHandle(GetCurrentProcess(), ols_handle_from_fd(fd), GetCurrentProcess(),
                         &d, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return ols_set_errno_win();
    {
        HANDLE old = GetStdHandle(ids[fd2]), crtOld;
        _invalid_parameter_handler prev = _set_thread_local_invalid_parameter_handler(ignore_invalid_param);
        int crt, crtClosedOld = 0;
        crtOld = (HANDLE)_get_osfhandle(fd2);
        SetStdHandle(ids[fd2], d);
        crt = _open_osfhandle((intptr_t)d, _O_BINARY | (fd2 == 0 ? _O_RDONLY : _O_WRONLY));
        if (crt >= 0)
        {
            if (_dup2(crt, fd2) == 0)
            {
                HANDLE d2 = (HANDLE)_get_osfhandle(fd2);
                crtClosedOld = (crtOld == old);
                if (d2 != INVALID_HANDLE_VALUE)
                {
                    SetStdHandle(ids[fd2], d2);
                    _close(crt);
                }
            }
        }
        _set_thread_local_invalid_parameter_handler(prev);
        if (!crtClosedOld && old && old != INVALID_HANDLE_VALUE && old != d)
            CloseHandle(old);
    }
    ols_fd_set_meta(fd2, (uint16_t)(ols_fd_type(fd) == OLS_T_FILE ? OLS_T_FILE : OLS_T_CHAR));
    return fd2;
}

int dup3(int fd, int fd2, int flags)
{
    return dup2(fd, fd2);
}

int pipe(int fds[2])
{
    return socketpair(AF_INET, SOCK_STREAM, 0, fds);
}

int pipe2(int fds[2], int flags)
{
    int ret = socketpair(AF_INET, SOCK_STREAM | (flags & (O_NONBLOCK | O_CLOEXEC)), 0, fds);
    return ret;
}

static int set_nonblock(int fd, int on)
{
    u_long v = on ? 1 : 0;
    uint16_t m = ols_fd_meta(fd);
    if (ioctlsocket(ols_socket_from_fd(fd), FIONBIO, &v) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    if (on)
        m |= OLS_F_NONBLOCK;
    else
        m &= ~OLS_F_NONBLOCK;
    ols_fd_set_meta(fd, m);
    return 0;
}

static int lock_range(int fd, int cmd, struct flock *fl)
{
    HANDLE h = ols_handle_from_fd(fd);
    OVERLAPPED ov;
    LARGE_INTEGER start, len;
    off_t base = 0;
    DWORD flags = 0;
    if (fl->l_whence == SEEK_CUR)
        base = lseek(fd, 0, SEEK_CUR);
    else if (fl->l_whence == SEEK_END)
    {
        LARGE_INTEGER sz;
        GetFileSizeEx(h, &sz);
        base = sz.QuadPart;
    }
    start.QuadPart = 0x4000000000000000LL + base + fl->l_start;
    len.QuadPart = fl->l_len ? fl->l_len : 0x3fffffffffffffffLL;
    memset(&ov, 0, sizeof(ov));
    ov.Offset = start.LowPart;
    ov.OffsetHigh = (DWORD)start.HighPart;
    if (cmd == F_GETLK)
    {
        flags = LOCKFILE_FAIL_IMMEDIATELY | (fl->l_type == F_WRLCK ? LOCKFILE_EXCLUSIVE_LOCK : 0);
        if (LockFileEx(h, flags, 0, len.LowPart, (DWORD)len.HighPart, &ov))
        {
            UnlockFileEx(h, 0, len.LowPart, (DWORD)len.HighPart, &ov);
            fl->l_type = F_UNLCK;
        }
        else
            fl->l_pid = 0;
        return 0;
    }
    if (fl->l_type == F_UNLCK)
    {
        if (!UnlockFileEx(h, 0, len.LowPart, (DWORD)len.HighPart, &ov))
        {
            if (GetLastError() == ERROR_NOT_LOCKED)
                return 0;
            return ols_set_errno_win();
        }
        return 0;
    }
    if (fl->l_type == F_WRLCK)
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (cmd == F_SETLK)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!LockFileEx(h, flags, 0, len.LowPart, (DWORD)len.HighPart, &ov))
    {
        if (GetLastError() == ERROR_LOCK_VIOLATION || GetLastError() == ERROR_IO_PENDING)
        {
            errno = EAGAIN;
            return -1;
        }
        return ols_set_errno_win();
    }
    return 0;
}

int fcntl(int fd, int cmd, ...)
{
    va_list ap;
    intptr_t arg;
    uint16_t m;
    int type = ols_fd_type(fd);
    va_start(ap, cmd);
    arg = va_arg(ap, intptr_t);
    va_end(ap);
    if (type == OLS_T_UNKNOWN)
    {
        errno = EBADF;
        return -1;
    }
    m = ols_fd_meta(fd);
    switch (cmd)
    {
    case F_GETFL:
        return ((m & OLS_F_ACC_MASK) >> OLS_F_ACC_SHIFT)
               | ((m & OLS_F_NONBLOCK) ? O_NONBLOCK : 0)
               | ((m & OLS_F_APPEND) ? O_APPEND : 0);
    case F_SETFL:
        if (type == OLS_T_SOCKET)
        {
            if (((arg & O_NONBLOCK) != 0) != ((m & OLS_F_NONBLOCK) != 0))
                return set_nonblock(fd, (arg & O_NONBLOCK) != 0);
            return 0;
        }
        if (arg & O_NONBLOCK)
            m |= OLS_F_NONBLOCK;
        else
            m &= ~OLS_F_NONBLOCK;
        ols_fd_set_meta(fd, m);
        return 0;
    case F_GETFD:
        return (m & OLS_F_CLOEXEC) ? FD_CLOEXEC : 0;
    case F_SETFD:
        if (arg & FD_CLOEXEC)
            m |= OLS_F_CLOEXEC;
        else
            m &= ~OLS_F_CLOEXEC;
        SetHandleInformation(ols_handle_from_fd(fd), HANDLE_FLAG_INHERIT,
                             (arg & FD_CLOEXEC) ? 0 : HANDLE_FLAG_INHERIT);
        ols_fd_set_meta(fd, m);
        return 0;
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
        return dup(fd);
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
        return lock_range(fd, cmd, (struct flock *)arg);
    case F_SETPIPE_SZ:
    case F_GETPIPE_SZ:
        return 65536;
    case F_SETOWN:
    case F_SETSIG:
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

int ioctl(int fd, unsigned long req, ...)
{
    va_list ap;
    void *arg;
    va_start(ap, req);
    arg = va_arg(ap, void *);
    va_end(ap);
    if (ols_fd_type(fd) == OLS_T_SOCKET)
    {
        if (req == (unsigned long)FIONBIO)
            return set_nonblock(fd, *(int *)arg != 0);
        if (req == (unsigned long)FIONREAD)
        {
            u_long v = 0;
            if (ioctlsocket(ols_socket_from_fd(fd), FIONREAD, &v) == SOCKET_ERROR)
                return ols_set_errno_wsa();
            *(int *)arg = (int)v;
            return 0;
        }
    }
    errno = ENOTTY;
    return -1;
}

int ftruncate(int fd, off_t len)
{
    FILE_END_OF_FILE_INFO info;
    info.EndOfFile.QuadPart = len;
    if (!SetFileInformationByHandle(ols_handle_from_fd(fd), FileEndOfFileInfo, &info, sizeof(info)))
        return ols_set_errno_win();
    return 0;
}

int fsync(int fd)
{
    int type = ols_fd_type(fd);
    if (type != OLS_T_FILE)
        return 0;
    if (!FlushFileBuffers(ols_handle_from_fd(fd)))
        return ols_set_errno_win();
    return 0;
}

int fdatasync(int fd)
{
    return fsync(fd);
}

int flock(int fd, int op)
{
    HANDLE h = ols_handle_from_fd(fd);
    OVERLAPPED ov;
    DWORD flags = 0;
    memset(&ov, 0, sizeof(ov));
    ov.OffsetHigh = 0x7fffffff;
    if (op & LOCK_UN)
    {
        if (!UnlockFileEx(h, 0, 1, 0, &ov) && GetLastError() != ERROR_NOT_LOCKED)
            return ols_set_errno_win();
        return 0;
    }
    if (op & LOCK_EX)
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (op & LOCK_NB)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!LockFileEx(h, flags, 0, 1, 0, &ov))
    {
        if (GetLastError() == ERROR_LOCK_VIOLATION)
        {
            errno = EWOULDBLOCK;
            return -1;
        }
        return ols_set_errno_win();
    }
    return 0;
}

int isatty(int fd)
{
    return GetFileType(ols_handle_from_fd(fd)) == FILE_TYPE_CHAR;
}

int fchmod(int fd, mode_t mode)
{
    return 0;
}

int fchown(int fd, uid_t uid, gid_t gid)
{
    return 0;
}

int fchdir(int fd)
{
    wchar_t w[PATH_MAX];
    DWORD n = GetFinalPathNameByHandleW(ols_handle_from_fd(fd), w, PATH_MAX,
                                        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (n == 0 || n >= PATH_MAX)
        return ols_set_errno_win();
    if (!SetCurrentDirectoryW(w))
        return ols_set_errno_win();
    return 0;
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
    char buf[65536];
    size_t chunk = count > sizeof(buf) ? sizeof(buf) : count;
    ssize_t got, sent;
    off_t pos;
    if (chunk == 0)
        return 0;
    if (offset)
        got = pread(in_fd, buf, chunk, *offset);
    else
    {
        pos = lseek(in_fd, 0, SEEK_CUR);
        got = read(in_fd, buf, chunk);
    }
    if (got <= 0)
        return got;
    sent = write(out_fd, buf, (size_t)got);
    if (sent < 0)
    {
        if (!offset)
            lseek(in_fd, pos, SEEK_SET);
        return -1;
    }
    if (offset)
        *offset += sent;
    else if (sent < got)
        lseek(in_fd, pos + sent, SEEK_SET);
    return sent;
}

int posix_fadvise(int fd, off_t off, off_t len, int advice)
{
    return 0;
}

int posix_fallocate(int fd, off_t off, off_t len)
{
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(ols_handle_from_fd(fd), &sz))
        return ols_errno_from_win(GetLastError());
    if (sz.QuadPart < off + len)
        return ftruncate(fd, off + len) == 0 ? 0 : errno;
    return 0;
}

static void udp_no_reset(SOCKET s)
{
    BOOL off = FALSE;
    DWORD n = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), NULL, 0, &n, NULL, NULL);
    WSAIoctl(s, SIO_UDP_NETRESET, &off, sizeof(off), NULL, 0, &n, NULL, NULL);
}

int ols_socket(int domain, int type, int proto)
{
    SOCKET s;
    int fd, base = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    uint16_t meta = OLS_T_SOCKET | (O_RDWR << OLS_F_ACC_SHIFT);
    ols_wsa_init();
    s = WSASocketW(domain, base, proto, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (s == INVALID_SOCKET)
        return ols_set_errno_wsa();
    if (base == SOCK_DGRAM && (domain == AF_INET || domain == AF_INET6))
        udp_no_reset(s);
    fd = ols_fd_from_socket(s);
    if (type & SOCK_CLOEXEC)
        meta |= OLS_F_CLOEXEC;
    ols_fd_set_meta(fd, meta);
    if (type & SOCK_NONBLOCK)
        set_nonblock(fd, 1);
    return fd;
}

int accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags)
{
    SOCKET ls = ols_socket_from_fd(fd);
    int locked = ols_accept_lock(ls);
    SOCKET s;
    int nfd, err;
    uint16_t meta = OLS_T_SOCKET | (O_RDWR << OLS_F_ACC_SHIFT);
    if (locked < 0)
    {
        errno = EAGAIN;
        return -1;
    }
    s = accept(ls, addr, len);
    err = WSAGetLastError();
    if (locked > 0)
        ols_accept_unlock();
    if (s == INVALID_SOCKET)
    {
        WSASetLastError(err);
        return ols_set_errno_wsa();
    }
    nfd = ols_fd_from_socket(s);
    if (flags & SOCK_CLOEXEC)
        meta |= OLS_F_CLOEXEC;
    ols_fd_set_meta(nfd, meta);
    if ((flags & SOCK_NONBLOCK) || (ols_fd_meta(fd) & OLS_F_NONBLOCK))
        set_nonblock(nfd, 1);
    else
        set_nonblock(nfd, 0);
    return nfd;
}

int ols_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
    return accept4(fd, addr, len, 0);
}

static int unix_addr_native(const struct sockaddr *addr, socklen_t len, struct sockaddr_un *out)
{
    const struct sockaddr_un *in = (const struct sockaddr_un *)addr;
    size_t off = offsetof(struct sockaddr_un, sun_path), n, cl;
    char buf[PATH_MAX * 3], cwd[PATH_MAX * 3];
    wchar_t wcwd[PATH_MAX];
    const char *p = buf;
    char *q;
    if (len <= (socklen_t)off)
    {
        errno = EINVAL;
        return -1;
    }
    n = strnlen(in->sun_path, len - off);
    if (ols_native_path(in->sun_path, (int)n, buf, sizeof(buf)) < 0)
    {
        errno = EINVAL;
        return -1;
    }
    for (q = buf; *q; ++q)
        if (*q == '/')
            *q = '\\';
    if (strlen(buf) >= sizeof(out->sun_path))
    {
        DWORD wn = GetCurrentDirectoryW(PATH_MAX, wcwd);
        if (wn == 0 || wn >= PATH_MAX
            || WideCharToMultiByte(CP_UTF8, 0, wcwd, -1, cwd, sizeof(cwd), NULL, NULL) <= 0)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        cl = strlen(cwd);
        if (cl && cwd[cl - 1] == '\\')
            --cl;
        if (_strnicmp(buf, cwd, cl) != 0 || buf[cl] != '\\'
            || strlen(buf + cl + 1) >= sizeof(out->sun_path))
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        p = buf + cl + 1;
    }
    memset(out, 0, sizeof(*out));
    out->sun_family = AF_UNIX;
    strcpy(out->sun_path, p);
    return 0;
}

static int secure_unix_path(const char *path)
{
    wchar_t w[PATH_MAX];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w, PATH_MAX) <= 0)
        return -1;
    return ols_secure_wpath(w, 0);
}

int ols_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    struct sockaddr_un un;
    if (addr && addr->sa_family == AF_UNIX)
    {
        if (unix_addr_native(addr, len, &un) < 0)
            return -1;
        addr = (const struct sockaddr *)&un;
        len = sizeof(un);
    }
    if (connect(ols_socket_from_fd(fd), addr, len) == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK)
            errno = EINPROGRESS;
        else
            errno = ols_errno_from_wsa(e);
        return -1;
    }
    return 0;
}

int ols_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
    struct sockaddr_un un;
    if (addr && addr->sa_family == AF_UNIX)
    {
        if (unix_addr_native(addr, len, &un) < 0)
            return -1;
        if (bind(ols_socket_from_fd(fd), (const struct sockaddr *)&un, sizeof(un)) == SOCKET_ERROR)
            return ols_set_errno_wsa();
        if (secure_unix_path(un.sun_path) < 0)
        {
            errno = EACCES;
            return -1;
        }
        return 0;
    }
    if (bind(ols_socket_from_fd(fd), addr, len) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return 0;
}

int ols_listen(int fd, int backlog)
{
    if (listen(ols_socket_from_fd(fd), backlog) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return 0;
}

int ols_shutdown(int fd, int how)
{
    if (shutdown(ols_socket_from_fd(fd), how) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return 0;
}

ssize_t ols_send(int fd, const void *buf, size_t len, int flags)
{
    int r = send(ols_socket_from_fd(fd), (const char *)buf, len > INT_MAX ? INT_MAX : (int)len,
                 flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL));
    if (r == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return r;
}

ssize_t ols_recv(int fd, void *buf, size_t len, int flags)
{
    int r = recv(ols_socket_from_fd(fd), (char *)buf, len > INT_MAX ? INT_MAX : (int)len,
                 flags & ~MSG_DONTWAIT);
    if (r == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        if (e == WSAESHUTDOWN || e == WSAECONNABORTED)
            return 0;
        errno = ols_errno_from_wsa(e);
        return -1;
    }
    return r;
}

ssize_t ols_sendto(int fd, const void *buf, size_t len, int flags,
                   const struct sockaddr *to, socklen_t tolen)
{
    int r = sendto(ols_socket_from_fd(fd), (const char *)buf, len > INT_MAX ? INT_MAX : (int)len,
                   flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL), to, tolen);
    if (r == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return r;
}

ssize_t ols_recvfrom(int fd, void *buf, size_t len, int flags,
                     struct sockaddr *from, socklen_t *fromlen)
{
    int r = recvfrom(ols_socket_from_fd(fd), (char *)buf, len > INT_MAX ? INT_MAX : (int)len,
                     flags & ~MSG_DONTWAIT, from, fromlen);
    if (r == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        if (e == WSAEMSGSIZE)
            return (ssize_t)len;
        errno = ols_errno_from_wsa(e);
        return -1;
    }
    return r;
}

int ols_setsockopt(int fd, int level, int name, const void *val, socklen_t len)
{
    if (name >= OLS_LINUX_OPT && name < OLS_LINUX_OPT + 0x100)
    {
        if (name == SO_RCVBUFFORCE)
            name = SO_RCVBUF;
        else if (name == SO_SNDBUFFORCE)
            name = SO_SNDBUF;
        else if (name == IPV6_RECVPKTINFO)
            name = IPV6_PKTINFO;
        else
            return 0;
    }
    if (level == SOL_SOCKET && name == SO_REUSEADDR)
        return 0;
    if (setsockopt(ols_socket_from_fd(fd), level, name, (const char *)val, len) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return 0;
}

int ols_getsockopt(int fd, int level, int name, void *val, socklen_t *len)
{
    if (name >= OLS_LINUX_OPT && name < OLS_LINUX_OPT + 0x100)
    {
        errno = ENOPROTOOPT;
        return -1;
    }
    if (getsockopt(ols_socket_from_fd(fd), level, name, (char *)val, len) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    if (level == SOL_SOCKET && name == SO_ERROR && *len >= (socklen_t)sizeof(int)
        && *(int *)val != 0)
        *(int *)val = ols_errno_from_wsa(*(int *)val);
    return 0;
}

int ols_getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
    if (getsockname(ols_socket_from_fd(fd), addr, len) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return 0;
}

int ols_getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
    if (getpeername(ols_socket_from_fd(fd), addr, len) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return 0;
}

static int unix_stream_pair(SOCKET *pa, SOCKET *pc)
{
    static volatile LONG s_seq;
    wchar_t dir[MAX_PATH], wpath[MAX_PATH];
    struct sockaddr_un addr;
    SECURITY_ATTRIBUTES sa;
    SOCKET l = INVALID_SOCKET, a = INVALID_SOCKET, c = INVALID_SOCKET;
    unsigned char rnd[8];
    DWORD n = GetTempPathW(MAX_PATH - 64, dir);
    int ok = 0;
    if (n == 0 || n >= MAX_PATH - 64
        || BCryptGenRandom(NULL, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return -1;
    swprintf(dir + n, MAX_PATH - n, L"ols-sp-%lu-%ld-%02x%02x%02x%02x%02x%02x%02x%02x", GetCurrentProcessId(),
             InterlockedIncrement(&s_seq), rnd[0], rnd[1], rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7]);
    swprintf(wpath, MAX_PATH, L"%s\\s", dir);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (WideCharToMultiByte(CP_UTF8, 0, wpath, -1, addr.sun_path, sizeof(addr.sun_path), NULL, NULL) <= 0)
        return -1;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = ols_private_sd(1);
    if (!sa.lpSecurityDescriptor)
        return -1;
    n = CreateDirectoryW(dir, &sa);
    LocalFree(sa.lpSecurityDescriptor);
    if (!n)
        return -1;
    l = WSASocketW(AF_UNIX, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    c = WSASocketW(AF_UNIX, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (l != INVALID_SOCKET && c != INVALID_SOCKET
        && bind(l, (struct sockaddr *)&addr, sizeof(addr)) == 0 && listen(l, 1) == 0
        && connect(c, (struct sockaddr *)&addr, sizeof(addr)) == 0)
    {
        a = accept(l, NULL, NULL);
        ok = a != INVALID_SOCKET;
    }
    if (l != INVALID_SOCKET)
        closesocket(l);
    DeleteFileW(wpath);
    RemoveDirectoryW(dir);
    if (!ok)
    {
        if (c != INVALID_SOCKET)
            closesocket(c);
        return -1;
    }
    *pa = a;
    *pc = c;
    return 0;
}

int socketpair(int domain, int type, int proto, int fds[2])
{
    SOCKET l = INVALID_SOCKET, a = INVALID_SOCKET, c = INVALID_SOCKET;
    struct sockaddr_in addr, caddr, paddr;
    int alen, base = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC), tries;
    ols_wsa_init();
    if (domain == AF_UNIX && base == SOCK_STREAM && unix_stream_pair(&a, &c) == 0)
        goto done;
    if (base == SOCK_DGRAM)
    {
        a = WSASocketW(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        c = WSASocketW(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        if (a == INVALID_SOCKET || c == INVALID_SOCKET)
            goto fail;
        udp_no_reset(a);
        udp_no_reset(c);
        {
            BOOL ex = TRUE;
            setsockopt(a, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&ex, sizeof(ex));
            setsockopt(c, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&ex, sizeof(ex));
        }
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(a, (struct sockaddr *)&addr, sizeof(addr)) || bind(c, (struct sockaddr *)&addr, sizeof(addr)))
            goto fail;
        alen = sizeof(caddr);
        getsockname(a, (struct sockaddr *)&caddr, &alen);
        alen = sizeof(paddr);
        getsockname(c, (struct sockaddr *)&paddr, &alen);
        if (connect(a, (struct sockaddr *)&paddr, sizeof(paddr)) || connect(c, (struct sockaddr *)&caddr, sizeof(caddr)))
            goto fail;
    }
    else
    {
        for (tries = 0; tries < 8; ++tries)
        {
            l = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
            c = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
            if (l == INVALID_SOCKET || c == INVALID_SOCKET)
                goto fail;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            {
                BOOL ex = TRUE;
                setsockopt(l, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&ex, sizeof(ex));
            }
            if (bind(l, (struct sockaddr *)&addr, sizeof(addr)) || listen(l, 1))
                goto fail;
            alen = sizeof(addr);
            getsockname(l, (struct sockaddr *)&addr, &alen);
            if (connect(c, (struct sockaddr *)&addr, sizeof(addr)))
                goto fail;
            alen = sizeof(paddr);
            a = accept(l, (struct sockaddr *)&paddr, &alen);
            if (a == INVALID_SOCKET)
                goto fail;
            alen = sizeof(caddr);
            getsockname(c, (struct sockaddr *)&caddr, &alen);
            closesocket(l);
            l = INVALID_SOCKET;
            if (paddr.sin_port == caddr.sin_port && paddr.sin_addr.s_addr == caddr.sin_addr.s_addr)
                break;
            closesocket(a);
            closesocket(c);
            a = c = INVALID_SOCKET;
        }
        if (a == INVALID_SOCKET)
        {
            errno = EIO;
            return -1;
        }
        {
            BOOL nd = TRUE;
            setsockopt(a, IPPROTO_TCP, TCP_NODELAY, (char *)&nd, sizeof(nd));
            setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (char *)&nd, sizeof(nd));
        }
    }
done:
    fds[0] = ols_fd_from_socket(a);
    fds[1] = ols_fd_from_socket(c);
    ols_fd_set_meta(fds[0], OLS_T_SOCKET | (O_RDWR << OLS_F_ACC_SHIFT));
    ols_fd_set_meta(fds[1], OLS_T_SOCKET | (O_RDWR << OLS_F_ACC_SHIFT));
    if (type & SOCK_NONBLOCK)
    {
        set_nonblock(fds[0], 1);
        set_nonblock(fds[1], 1);
    }
    return 0;
fail:
    ols_set_errno_wsa();
    if (l != INVALID_SOCKET)
        closesocket(l);
    if (a != INVALID_SOCKET)
        closesocket(a);
    if (c != INVALID_SOCKET)
        closesocket(c);
    return -1;
}

static short to_wsa_events(short ev)
{
    short w = 0;
    if (ev & (POLLIN | POLLRDNORM))
        w |= WSA_POLLRDNORM;
    if (ev & POLLRDBAND)
        w |= WSA_POLLRDBAND;
    if (ev & (POLLOUT | POLLWRNORM))
        w |= WSA_POLLWRNORM;
    if (ev & POLLPRI)
        w |= WSA_POLLRDBAND;
    return w;
}

static short from_wsa_events(short w)
{
    short ev = 0;
    if (w & (WSA_POLLRDNORM | WSA_POLLRDBAND))
        ev |= POLLIN;
    if (w & WSA_POLLWRNORM)
        ev |= POLLOUT;
    if (w & WSA_POLLERR)
        ev |= POLLERR;
    if (w & WSA_POLLHUP)
        ev |= POLLHUP | POLLIN;
    if (w & WSA_POLLNVAL)
        ev |= POLLNVAL;
    return ev;
}

int ols_poll(struct ols_pollfd *fds, nfds_t n, int timeout)
{
    WSAPOLLFD stack[64], *w = stack;
    int mstack[64], *map = mstack;
    nfds_t i;
    int ready = 0, socks = 0, r;
    if (n > 64)
    {
        w = (WSAPOLLFD *)malloc(n * (sizeof(WSAPOLLFD) + sizeof(int)));
        if (!w)
        {
            errno = ENOMEM;
            return -1;
        }
        map = (int *)(w + n);
    }
    for (i = 0; i < n; ++i)
    {
        fds[i].revents = 0;
        w[i].fd = INVALID_SOCKET;
        w[i].events = 0;
        w[i].revents = 0;
        if (fds[i].fd < 0)
            continue;
        if (ols_fd_type(fds[i].fd) == OLS_T_SOCKET)
        {
            w[i].fd = ols_socket_from_fd(fds[i].fd);
            w[i].events = to_wsa_events(fds[i].events);
            ++socks;
        }
        else
        {
            fds[i].revents = fds[i].events & (POLLIN | POLLOUT);
            if (fds[i].revents)
                ++ready;
        }
    }
    if (socks == 0)
    {
        if (!ready && timeout != 0)
            Sleep(timeout < 0 ? INFINITE : (DWORD)timeout);
        if (w != stack)
            free(w);
        return ready;
    }
    {
        WSAPOLLFD *packed = w;
        nfds_t j = 0;
        for (i = 0; i < n; ++i)
            if (w[i].fd != INVALID_SOCKET)
            {
                packed[j] = w[i];
                map[j++] = (int)i;
            }
        r = WSAPoll(packed, (ULONG)socks, ready ? 0 : timeout);
        if (r == SOCKET_ERROR)
        {
            ols_set_errno_wsa();
            if (w != stack)
                free(w);
            return -1;
        }
        for (j = 0; j < (nfds_t)socks; ++j)
        {
            if (packed[j].revents)
            {
                fds[map[j]].revents = from_wsa_events(packed[j].revents)
                                      & (fds[map[j]].events | POLLERR | POLLHUP | POLLNVAL);
                if (fds[map[j]].revents)
                    ++ready;
            }
        }
    }
    if (w != stack)
        free(w);
    return ready;
}

int ols_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
    fd_set *sets[3];
    u_int i;
    int k, ret;
    sets[0] = r;
    sets[1] = w;
    sets[2] = e;
    for (k = 0; k < 3; ++k)
        if (sets[k])
            for (i = 0; i < sets[k]->fd_count; ++i)
                sets[k]->fd_array[i] = ols_socket_from_fd((int)sets[k]->fd_array[i]);
    if ((!r || !r->fd_count) && (!w || !w->fd_count) && (!e || !e->fd_count))
    {
        if (tv)
            Sleep((DWORD)(tv->tv_sec * 1000 + tv->tv_usec / 1000));
        return 0;
    }
    ret = select(n, r, w, e, tv);
    for (k = 0; k < 3; ++k)
        if (sets[k])
            for (i = 0; i < sets[k]->fd_count; ++i)
                sets[k]->fd_array[i] = (SOCKET)ols_fd_from_socket(sets[k]->fd_array[i]);
    if (ret == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return ret;
}

struct cmsghdr *ols_cmsg_nxthdr(struct msghdr *m, struct cmsghdr *c)
{
    char *next = (char *)c + OLS_CMSG_ALIGN(c->cmsg_len);
    char *end = (char *)m->msg_control + m->msg_controllen;
    if (next + sizeof(struct cmsghdr) > end)
        return NULL;
    return (struct cmsghdr *)next;
}

ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
    WSAMSG wm;
    WSABUF bufs[OLS_MAX_WSABUF];
    DWORD sent = 0;
    size_t i, n = msg->msg_iovlen > OLS_MAX_WSABUF ? OLS_MAX_WSABUF : msg->msg_iovlen;
    ols_wsa_init();
    for (i = 0; i < n; ++i)
    {
        bufs[i].buf = (char *)msg->msg_iov[i].iov_base;
        bufs[i].len = (ULONG)msg->msg_iov[i].iov_len;
    }
    memset(&wm, 0, sizeof(wm));
    wm.name = (LPSOCKADDR)msg->msg_name;
    wm.namelen = msg->msg_namelen;
    wm.lpBuffers = bufs;
    wm.dwBufferCount = (ULONG)n;
    wm.Control.buf = (char *)msg->msg_control;
    wm.Control.len = (ULONG)msg->msg_controllen;
    if (g_sendmsg)
    {
        if (g_sendmsg(ols_socket_from_fd(fd), &wm, 0, &sent, NULL, NULL) == SOCKET_ERROR)
            return ols_set_errno_wsa();
        return sent;
    }
    if (WSASendTo(ols_socket_from_fd(fd), bufs, (DWORD)n, &sent, 0, wm.name, wm.namelen, NULL, NULL) == SOCKET_ERROR)
        return ols_set_errno_wsa();
    return sent;
}

ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
    WSAMSG wm;
    WSABUF bufs[OLS_MAX_WSABUF];
    DWORD got = 0;
    size_t i, n = msg->msg_iovlen > OLS_MAX_WSABUF ? OLS_MAX_WSABUF : msg->msg_iovlen;
    ols_wsa_init();
    for (i = 0; i < n; ++i)
    {
        bufs[i].buf = (char *)msg->msg_iov[i].iov_base;
        bufs[i].len = (ULONG)msg->msg_iov[i].iov_len;
    }
    memset(&wm, 0, sizeof(wm));
    wm.name = (LPSOCKADDR)msg->msg_name;
    wm.namelen = msg->msg_namelen;
    wm.lpBuffers = bufs;
    wm.dwBufferCount = (ULONG)n;
    wm.Control.buf = (char *)msg->msg_control;
    wm.Control.len = (ULONG)msg->msg_controllen;
    wm.dwFlags = 0;
    if (!g_recvmsg)
    {
        errno = ENOSYS;
        return -1;
    }
    if (g_recvmsg(ols_socket_from_fd(fd), &wm, &got, NULL, NULL) == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        if (e != WSAEMSGSIZE)
        {
            errno = ols_errno_from_wsa(e);
            return -1;
        }
        wm.dwFlags |= MSG_TRUNC_COMPAT;
    }
    msg->msg_namelen = wm.namelen;
    msg->msg_controllen = wm.Control.len;
    msg->msg_flags = (int)wm.dwFlags;
    return got;
}

int sendmmsg(int fd, struct mmsghdr *vec, unsigned int vlen, int flags)
{
    unsigned int i;
    for (i = 0; i < vlen; ++i)
    {
        ssize_t r = sendmsg(fd, &vec[i].msg_hdr, flags);
        if (r < 0)
            return i ? (int)i : -1;
        vec[i].msg_len = (unsigned int)r;
    }
    return (int)i;
}

int recvmmsg(int fd, struct mmsghdr *vec, unsigned int vlen, int flags, struct timespec *timeout)
{
    unsigned int i;
    for (i = 0; i < vlen; ++i)
    {
        ssize_t r = recvmsg(fd, &vec[i].msg_hdr, flags);
        if (r < 0)
            return i ? (int)i : -1;
        vec[i].msg_len = (unsigned int)r;
    }
    return (int)i;
}

int epoll_create(int size)
{
    return epoll_create1(0);
}

int epoll_create1(int flags)
{
    HANDLE h;
    int fd, wfd;
    struct epoll_event ev;
    ols_wsa_init();
    h = wep_epoll_create1(0);
    if (h == NULL)
        return ols_set_errno_win();
    fd = (int)((uintptr_t)h >> 2);
    ols_fd_set_meta(fd, OLS_T_EPOLL);
    wfd = ols_signal_wake_fd();
    if (wfd >= 0)
    {
        ev.events = EPOLLIN;
        ev.data.u64 = 0xffffffff00000000ULL | (uint32_t)wfd;
        wep_epoll_ctl(h, EPOLL_CTL_ADD, ols_socket_from_fd(wfd), &ev);
    }
    return fd;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
{
    int r;
    if (ols_fd_type(fd) != OLS_T_SOCKET)
    {
        errno = EPERM;
        return -1;
    }
    r = wep_epoll_ctl((HANDLE)((uintptr_t)(unsigned)epfd << 2), op, ols_socket_from_fd(fd), ev);
    if (r < 0)
    {
        if (errno == 0)
            errno = EINVAL;
        return -1;
    }
    return r;
}

int epoll_wait(int epfd, struct epoll_event *evs, int max, int timeout)
{
    HANDLE h = (HANDLE)((uintptr_t)(unsigned)epfd << 2);
    int n, i, j, woke = 0;
    uint64_t wake = 0xffffffff00000000ULL | (uint32_t)ols_signal_wake_fd();
    if (ols_signal_pending())
    {
        ols_signal_dispatch();
        errno = EINTR;
        return -1;
    }
    n = wep_epoll_wait(h, evs, max, timeout);
    if (n <= 0)
        return n;
    for (i = 0, j = 0; i < n; ++i)
    {
        if (evs[i].data.u64 == wake)
        {
            woke = 1;
            continue;
        }
        if (i != j)
            evs[j] = evs[i];
        ++j;
    }
    if (woke)
    {
        ols_signal_drain();
        if (ols_signal_pending())
            ols_signal_dispatch();
        if (j == 0)
        {
            errno = EINTR;
            return -1;
        }
    }
    return j;
}

int epoll_pwait(int epfd, struct epoll_event *evs, int max, int timeout, const sigset_t *mask)
{
    return epoll_wait(epfd, evs, max, timeout);
}
