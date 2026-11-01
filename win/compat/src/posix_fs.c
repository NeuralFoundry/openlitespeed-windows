#include "compat_internal.h"

static mode_t g_umask = 022;
static int g_strict = -1;
static wchar_t g_tmp_root[PATH_MAX];

static int strict_paths(void)
{
    if (g_strict < 0)
    {
        char v[8];
        DWORD n = GetEnvironmentVariableA("OLS_STRICT_PATHS", v, sizeof(v));
        g_strict = !(n > 0 && v[0] == '0');
    }
    return g_strict;
}

static const wchar_t *tmp_root(void)
{
    if (!g_tmp_root[0])
    {
        wchar_t buf[PATH_MAX];
        DWORD n = GetEnvironmentVariableW(L"OLS_TMP_ROOT", buf, PATH_MAX);
        if (n == 0 || n >= PATH_MAX)
        {
            n = GetEnvironmentVariableW(L"OLS_ROOT", buf, PATH_MAX - 8);
            if (n > 0 && n < PATH_MAX - 8)
                wcscat(buf, L"\\tmp");
            else
            {
                n = GetTempPathW(PATH_MAX, buf);
                if (n > 0 && buf[n - 1] == L'\\')
                    buf[n - 1] = 0;
                wcscat(buf, L"\\ols");
            }
        }
        wcsncpy(g_tmp_root, buf, PATH_MAX - 1);
    }
    return g_tmp_root;
}

static int is_device_name(const char *c, size_t len)
{
    static const char *names[] = { "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$", NULL };
    size_t base = 0;
    int i;
    while (base < len && c[base] != '.' && c[base] != ' ' && c[base] != ':')
        ++base;
    for (i = 0; names[i]; ++i)
        if (strlen(names[i]) == base && _strnicmp(c, names[i], base) == 0)
            return 1;
    if (base < 4 || (_strnicmp(c, "COM", 3) != 0 && _strnicmp(c, "LPT", 3) != 0))
        return 0;
    if (base == 4 && ((c[3] >= '0' && c[3] <= '9') || (unsigned char)c[3] >= 0x80))
        return 1;
    if (base == 5 && (unsigned char)c[3] == 0xC2
        && ((unsigned char)c[4] == 0xB9 || (unsigned char)c[4] == 0xB2 || (unsigned char)c[4] == 0xB3))
        return 1;
    return 0;
}

static int check_components(const char *path)
{
    const char *p = path, *c;
    size_t len;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':')
        p += 2;
    while (*p)
    {
        while (*p == '/' || *p == '\\')
            ++p;
        if (!*p)
            break;
        c = p;
        while (*p && *p != '/' && *p != '\\')
            ++p;
        len = p - c;
        if (memchr(c, ':', len))
            return -1;
        if ((len == 1 && c[0] == '.') || (len == 2 && c[0] == '.' && c[1] == '.'))
            continue;
        if (c[len - 1] == '.' || c[len - 1] == ' ')
            return -1;
        if (is_device_name(c, len))
            return -1;
        {
            size_t i;
            for (i = 0; i < len; ++i)
                if ((unsigned char)c[i] < 0x20 || c[i] == '<' || c[i] == '>' || c[i] == '"'
                    || c[i] == '|' || c[i] == '?' || c[i] == '*')
                    return -1;
        }
    }
    return 0;
}

int ols_path_to_w(const char *path, wchar_t *out, size_t outlen, int *special)
{
    wchar_t tmp[PATH_MAX], *src = tmp;
    const char *rest = NULL;
    int n;
    DWORD full;
    if (special)
        *special = OLS_SPECIAL_NONE;
    if (!path || !*path)
    {
        errno = ENOENT;
        return -1;
    }
    if (strcmp(path, "/dev/null") == 0)
    {
        wcscpy(out, L"NUL");
        if (special)
            *special = OLS_SPECIAL_NULL;
        return 0;
    }
    if (strcmp(path, "/dev/urandom") == 0 || strcmp(path, "/dev/random") == 0)
    {
        wcscpy(out, L"NUL");
        if (special)
            *special = OLS_SPECIAL_RANDOM;
        return 0;
    }
    if (strict_paths() && check_components(path) != 0)
    {
        errno = ENOENT;
        return -1;
    }
    if (strncmp(path, "/tmp/", 5) == 0 || strcmp(path, "/tmp") == 0)
        rest = path + 4;
    else if (strncmp(path, "/dev/shm/", 9) == 0 || strcmp(path, "/dev/shm") == 0)
        rest = path + 8;
    if (rest)
    {
        size_t rl = wcslen(tmp_root());
        wcscpy(tmp, tmp_root());
        if (strncmp(path, "/dev/shm", 8) == 0)
        {
            wcscat(tmp, L"\\shm");
            rl += 4;
        }
        n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, rest, -1, tmp + rl,
                                (int)(PATH_MAX - rl));
    }
    else
        n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, tmp, PATH_MAX);
    if (n <= 0)
    {
        errno = GetLastError() == ERROR_INSUFFICIENT_BUFFER ? ENAMETOOLONG : ENOENT;
        return -1;
    }
    for (src = tmp; *src; ++src)
        if (*src == L'/')
            *src = L'\\';
    if (outlen < 8)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    full = GetFullPathNameW(tmp, (DWORD)(outlen - 4), out + 4, NULL);
    if (full == 0 || full >= outlen - 4)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (full >= MAX_PATH - 12 && out[4] != L'\\')
    {
        out[0] = L'\\';
        out[1] = L'\\';
        out[2] = L'?';
        out[3] = L'\\';
    }
    else
        memmove(out, out + 4, (full + 1) * sizeof(wchar_t));
    return 0;
}

int ols_w_to_path(const wchar_t *w, char *out, size_t outlen)
{
    int n;
    char *p;
    char drive[4];
    if (wcsncmp(w, L"\\\\?\\UNC\\", 8) == 0)
    {
        out[0] = '/';
        out[1] = '/';
        n = WideCharToMultiByte(CP_UTF8, 0, w + 8, -1, out + 2, (int)outlen - 2, NULL, NULL);
        if (n > 0)
            n += 2;
    }
    else
    {
        if (wcsncmp(w, L"\\\\?\\", 4) == 0)
            w += 4;
        n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)outlen, NULL, NULL);
    }
    if (n <= 0)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (p = out; *p; ++p)
        if (*p == '\\')
            *p = '/';
    if (out[0] && out[1] == ':')
    {
        const char *sd = ols_server_drive();
        drive[0] = out[0];
        drive[1] = ':';
        drive[2] = 0;
        if (sd && _strnicmp(sd, drive, 2) == 0)
        {
            memmove(out, out + 2, strlen(out + 2) + 1);
            if (!out[0])
            {
                out[0] = '/';
                out[1] = 0;
            }
        }
    }
    return 0;
}

static const wchar_t *skip_prefix(const wchar_t *w, int *unc)
{
    *unc = 0;
    if (wcsncmp(w, L"\\\\?\\UNC\\", 8) == 0)
    {
        *unc = 1;
        return w + 7;
    }
    if (wcsncmp(w, L"\\\\?\\", 4) == 0)
        return w + 4;
    if (w[0] == L'\\' && w[1] == L'\\')
        *unc = 1;
    return w;
}

static int last_comp(const wchar_t *s, size_t *len, const wchar_t **comp, size_t *clen)
{
    size_t e = *len, b;
    while (e > 0 && s[e - 1] == L'\\')
        --e;
    if (e == 0)
        return 0;
    b = e;
    while (b > 0 && s[b - 1] != L'\\')
        --b;
    *comp = s + b;
    *clen = e - b;
    *len = b;
    return 1;
}

static int comps_before(const wchar_t *s, size_t len)
{
    int n = 0;
    const wchar_t *c;
    size_t cl;
    while (last_comp(s, &len, &c, &cl))
        ++n;
    return n;
}

static int final_path(HANDLE h, wchar_t *out, DWORD len)
{
    DWORD n = GetFinalPathNameByHandleW(h, out, len, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    return (n == 0 || n >= len) ? -1 : 0;
}

static int verify_names(const wchar_t *final, const wchar_t *requested, int depth)
{
    int ua, ub;
    const wchar_t *a = skip_prefix(final, &ua), *b = skip_prefix(requested, &ub), *ca, *cb;
    size_t la = wcslen(a), lb = wcslen(b), na, nb;
    for (;;)
    {
        int ha = last_comp(a, &la, &ca, &na);
        int hb = last_comp(b, &lb, &cb, &nb);
        wchar_t prefix[PATH_MAX + 8], pfinal[PATH_MAX + 8];
        size_t plen;
        HANDLE ph;
        int r;
        if (!hb)
            return 0;
        if (!ha)
            return -1;
        if (na == nb && wcsncmp(ca, cb, na) == 0)
            continue;
        if (na == nb && _wcsnicmp(ca, cb, na) == 0)
        {
            int idx = comps_before(a, la);
            if (idx == 0 && na == 2 && ca[1] == L':')
                continue;
            if (ua && ub && idx < 2)
                continue;
            return -1;
        }
        if (depth >= 32)
            return -1;
        for (plen = 0; plen < nb; ++plen)
            if (cb[plen] == L'~')
                return -1;
        plen = (size_t)(cb - requested) + nb;
        if (plen >= PATH_MAX)
            return -1;
        memcpy(prefix, requested, plen * sizeof(wchar_t));
        prefix[plen] = 0;
        ph = CreateFileW(prefix, FILE_READ_ATTRIBUTES,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                         FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (ph == INVALID_HANDLE_VALUE)
            return -1;
        r = final_path(ph, pfinal, PATH_MAX);
        CloseHandle(ph);
        if (r != 0)
            return -1;
        return verify_names(pfinal, prefix, depth + 1);
    }
}

#define VCACHE_SIZE 1024
#define VCACHE_PATH 264
#define VCACHE_TTL  1000

static struct
{
    ULONGLONG exp;
    uint32_t hash;
    wchar_t path[VCACHE_PATH];
} s_vcache[VCACHE_SIZE];
static SRWLOCK s_vcache_lock = SRWLOCK_INIT;

static uint32_t path_hash(const wchar_t *p, size_t *len)
{
    uint32_t h = 2166136261u;
    const wchar_t *s = p;
    for (; *s; ++s)
        h = (h ^ (uint32_t)*s) * 16777619u;
    *len = (size_t)(s - p);
    return h;
}

static int verified_hit(const wchar_t *p)
{
    size_t len;
    uint32_t h = path_hash(p, &len);
    int hit = 0, i = (int)(h & (VCACHE_SIZE - 1));
    if (len >= VCACHE_PATH)
        return 0;
    AcquireSRWLockShared(&s_vcache_lock);
    if (s_vcache[i].hash == h && s_vcache[i].exp > GetTickCount64() && wcscmp(s_vcache[i].path, p) == 0)
        hit = 1;
    ReleaseSRWLockShared(&s_vcache_lock);
    return hit;
}

static void verified_add(const wchar_t *p)
{
    size_t len;
    uint32_t h = path_hash(p, &len);
    int i = (int)(h & (VCACHE_SIZE - 1));
    if (len >= VCACHE_PATH)
        return;
    AcquireSRWLockExclusive(&s_vcache_lock);
    s_vcache[i].hash = h;
    s_vcache[i].exp = GetTickCount64() + VCACHE_TTL;
    memcpy(s_vcache[i].path, p, (len + 1) * sizeof(wchar_t));
    ReleaseSRWLockExclusive(&s_vcache_lock);
}

static int verify_case(HANDLE h, const wchar_t *requested)
{
    wchar_t final[PATH_MAX + 8];
    if (!strict_paths() || verified_hit(requested))
        return 0;
    if (final_path(h, final, PATH_MAX) != 0)
        return -1;
    if (verify_names(final, requested, 0) != 0)
        return -1;
    verified_add(requested);
    return 0;
}

HANDLE ols_open_path(const char *path, DWORD access, DWORD share, DWORD disp, DWORD flags, int *special)
{
    return ols_open_path_sa(path, access, share, disp, flags, special, NULL);
}

HANDLE ols_open_path_sa(const char *path, DWORD access, DWORD share, DWORD disp, DWORD flags,
                        int *special, SECURITY_ATTRIBUTES *sa)
{
    wchar_t w[PATH_MAX + 8];
    HANDLE h;
    int sp = 0;
    if (ols_path_to_w(path, w, PATH_MAX + 8, &sp) < 0)
        return INVALID_HANDLE_VALUE;
    if (special)
        *special = sp;
    h = CreateFileW(w, access, share, sa, disp, flags, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        DWORD e = GetLastError();
        if (e == ERROR_ACCESS_DENIED && (disp == OPEN_EXISTING || disp == OPEN_ALWAYS))
        {
            DWORD a = GetFileAttributesW(w);
            if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)
                && (access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)))
            {
                errno = EISDIR;
                return INVALID_HANDLE_VALUE;
            }
        }
        errno = ols_errno_from_win(e);
        if (e == ERROR_FILE_EXISTS)
            errno = EEXIST;
        return INVALID_HANDLE_VALUE;
    }
    if (sp == OLS_SPECIAL_NONE && verify_case(h, w) != 0)
    {
        CloseHandle(h);
        errno = ENOENT;
        return INVALID_HANDLE_VALUE;
    }
    return h;
}

static void ft_to_ts(const LARGE_INTEGER *ft, struct timespec *ts)
{
    int64_t v = ft->QuadPart - 116444736000000000LL;
    ts->tv_sec = (time_t)(v / 10000000LL);
    ts->tv_nsec = (long)((v % 10000000LL) * 100);
    if (ts->tv_nsec < 0)
    {
        ts->tv_nsec += 1000000000L;
        ts->tv_sec -= 1;
    }
}

static int is_exec_name(const wchar_t *name)
{
    const wchar_t *dot = wcsrchr(name, L'.');
    if (!dot)
        return 0;
    return _wcsicmp(dot, L".exe") == 0 || _wcsicmp(dot, L".bat") == 0
           || _wcsicmp(dot, L".cmd") == 0 || _wcsicmp(dot, L".com") == 0;
}

static int fill_stat_name(HANDLE h, struct stat *st, int lnk, const wchar_t *name);

int ols_fill_stat(HANDLE h, struct stat *st, int lnk)
{
    return fill_stat_name(h, st, lnk, NULL);
}

typedef struct
{
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
    ULONG ReparseTag;
    ULONG NumberOfLinks;
    ULONG DeviceType;
    ULONG DeviceCharacteristics;
    ULONG Reserved;
    LARGE_INTEGER VolumeSerialNumber;
    BYTE FileId128[16];
} ols_stat_basic;

typedef BOOL (WINAPI *ols_gfibn_fn)(PCWSTR, int, PVOID, ULONG);
static ols_gfibn_fn s_gfibn;
static INIT_ONCE s_gfibn_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK gfibn_init(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    HMODULE m = GetModuleHandleW(L"kernelbase.dll");
    (void)once; (void)param; (void)ctx;
    if (m)
        s_gfibn = (ols_gfibn_fn)(void *)GetProcAddress(m, "GetFileInformationByName");
    return TRUE;
}

static int stat_fast(const wchar_t *w, struct stat *st)
{
    ols_stat_basic fi;
    InitOnceExecuteOnce(&s_gfibn_once, gfibn_init, NULL, NULL);
    if (!s_gfibn || (strict_paths() && !verified_hit(w)))
        return -1;
    if (!s_gfibn(w, 3, &fi, sizeof(fi)) || (fi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        || (fi.DeviceType != 7 && fi.DeviceType != 8))
        return -1;
    memset(st, 0, sizeof(*st));
    st->st_dev = (dev_t)(uint32_t)fi.VolumeSerialNumber.LowPart;
    st->st_ino = (uint64_t)fi.FileId.QuadPart;
    st->st_nlink = (short)fi.NumberOfLinks;
    st->st_size = fi.EndOfFile.QuadPart;
    st->st_blksize = 4096;
    st->st_blocks = (st->st_size + 511) / 512;
    st->st_uid = getuid();
    st->st_gid = getgid();
    if (fi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = S_IFDIR | 0755;
    else
    {
        st->st_mode = S_IFREG | 0644;
        if (is_exec_name(w))
            st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
    }
    if (fi.FileAttributes & FILE_ATTRIBUTE_READONLY)
        st->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    ft_to_ts(&fi.LastAccessTime, &st->st_atim);
    ft_to_ts(&fi.LastWriteTime, &st->st_mtim);
    ft_to_ts(&fi.ChangeTime, &st->st_ctim);
    return 0;
}

static int fill_stat_name(HANDLE h, struct stat *st, int lnk, const wchar_t *hint)
{
    BY_HANDLE_FILE_INFORMATION bi;
    FILE_BASIC_INFO basic;
    LARGE_INTEGER t;
    memset(st, 0, sizeof(*st));
    if (!GetFileInformationByHandle(h, &bi))
        return ols_set_errno_win();
    st->st_dev = bi.dwVolumeSerialNumber;
    st->st_ino = ((uint64_t)bi.nFileIndexHigh << 32) | bi.nFileIndexLow;
    st->st_nlink = bi.nNumberOfLinks;
    st->st_size = ((int64_t)bi.nFileSizeHigh << 32) | bi.nFileSizeLow;
    st->st_blksize = 4096;
    st->st_blocks = (st->st_size + 511) / 512;
    st->st_uid = getuid();
    st->st_gid = getgid();
    if (lnk && (bi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        st->st_mode = S_IFLNK | 0777;
    else if (bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = S_IFDIR | 0755;
    else
    {
        DWORD ft = GetFileType(h);
        if (ft == FILE_TYPE_CHAR)
            st->st_mode = S_IFCHR | 0666;
        else if (ft == FILE_TYPE_PIPE)
            st->st_mode = S_IFIFO | 0600;
        else
            st->st_mode = S_IFREG | 0644;
    }
    if (bi.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
        st->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    if (S_ISREG(st->st_mode))
    {
        wchar_t name[PATH_MAX];
        if (hint ? is_exec_name(hint)
                 : (GetFinalPathNameByHandleW(h, name, PATH_MAX, FILE_NAME_OPENED | VOLUME_NAME_NONE)
                    && is_exec_name(name)))
            st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
    }
    if (GetFileInformationByHandleEx(h, FileBasicInfo, &basic, sizeof(basic)))
    {
        ft_to_ts(&basic.LastAccessTime, &st->st_atim);
        ft_to_ts(&basic.LastWriteTime, &st->st_mtim);
        ft_to_ts(&basic.ChangeTime, &st->st_ctim);
    }
    else
    {
        t.LowPart = bi.ftLastAccessTime.dwLowDateTime;
        t.HighPart = (LONG)bi.ftLastAccessTime.dwHighDateTime;
        ft_to_ts(&t, &st->st_atim);
        t.LowPart = bi.ftLastWriteTime.dwLowDateTime;
        t.HighPart = (LONG)bi.ftLastWriteTime.dwHighDateTime;
        ft_to_ts(&t, &st->st_mtim);
        st->st_ctim = st->st_mtim;
    }
    return 0;
}

static int stat_impl(const char *path, struct stat *st, int lnk)
{
    wchar_t w[PATH_MAX + 8];
    int special = 0, ret;
    HANDLE h;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (lnk ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
    if (ols_path_to_w(path, w, PATH_MAX + 8, &special) < 0)
        return -1;
    if (special != OLS_SPECIAL_NONE)
    {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFCHR | 0666;
        return 0;
    }
    if (stat_fast(w, st) == 0)
        return 0;
    h = CreateFileW(w, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, flags, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        errno = ols_errno_from_win(GetLastError());
        return -1;
    }
    if (verify_case(h, w) != 0)
    {
        CloseHandle(h);
        errno = ENOENT;
        return -1;
    }
    ret = fill_stat_name(h, st, lnk, w);
    CloseHandle(h);
    return ret;
}

int stat(const char *path, struct stat *st)
{
    return stat_impl(path, st, 0);
}

int lstat(const char *path, struct stat *st)
{
    return stat_impl(path, st, 1);
}

int fstat(int fd, struct stat *st)
{
    int type = ols_fd_type(fd);
    if (type == OLS_T_SOCKET)
    {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFSOCK | 0777;
        return 0;
    }
    if (type == OLS_T_RANDOM || type == OLS_T_NULL || type == OLS_T_CHAR)
    {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFCHR | 0666;
        return 0;
    }
    if (type == OLS_T_UNKNOWN || type == OLS_T_EPOLL)
    {
        errno = EBADF;
        return -1;
    }
    return ols_fill_stat(ols_handle_from_fd(fd), st, 0);
}

static int at_path(int dirfd, const char *path, char *out, size_t outlen)
{
    wchar_t w[PATH_MAX];
    DWORD n;
    if (dirfd == AT_FDCWD || path[0] == '/' || path[0] == '\\' || (path[0] && path[1] == ':'))
    {
        if (strlen(path) >= outlen)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        strcpy(out, path);
        return 0;
    }
    n = GetFinalPathNameByHandleW(ols_handle_from_fd(dirfd), w, PATH_MAX,
                                  FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (n == 0 || n >= PATH_MAX)
        return ols_set_errno_win();
    if (ols_w_to_path(w, out, outlen) < 0)
        return -1;
    if (strlen(out) + strlen(path) + 2 > outlen)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcat(out, "/");
    strcat(out, path);
    return 0;
}

int fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
    char full[PATH_MAX * 3];
    if ((flags & AT_EMPTY_PATH) && (!path || !*path))
        return fstat(dirfd, st);
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    return stat_impl(full, st, (flags & AT_SYMLINK_NOFOLLOW) != 0);
}

int unlink(const char *path)
{
    int special = 0;
    HANDLE h;
    FILE_DISPOSITION_INFO_EX info;
    BY_HANDLE_FILE_INFORMATION bi;
    h = ols_open_path(path, DELETE | FILE_READ_ATTRIBUTES,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, &special);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (GetFileInformationByHandle(h, &bi) && (bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        && !(bi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        CloseHandle(h);
        errno = EISDIR;
        return -1;
    }
    info.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS
                 | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;
    if (!SetFileInformationByHandle(h, FileDispositionInfoEx, &info, sizeof(info)))
    {
        FILE_DISPOSITION_INFO old;
        old.DeleteFile = TRUE;
        if (!SetFileInformationByHandle(h, FileDispositionInfo, &old, sizeof(old)))
        {
            ols_set_errno_win();
            CloseHandle(h);
            return -1;
        }
    }
    CloseHandle(h);
    return 0;
}

int ols_unlink_utf8(const char *path)
{
    return unlink(path);
}

int unlinkat(int dirfd, const char *path, int flags)
{
    char full[PATH_MAX * 3];
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    if (flags & AT_REMOVEDIR)
        return rmdir(full);
    return unlink(full);
}

int ols_rename(const char *from, const char *to)
{
    wchar_t wf[PATH_MAX + 8], wt[PATH_MAX + 8];
    HANDLE h;
    size_t tl, sz;
    FILE_RENAME_INFO *info;
    if (ols_path_to_w(from, wf, PATH_MAX + 8, NULL) < 0
        || ols_path_to_w(to, wt, PATH_MAX + 8, NULL) < 0)
        return -1;
    h = CreateFileW(wf, DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        tl = wcslen(wt);
        sz = sizeof(FILE_RENAME_INFO) + tl * sizeof(wchar_t);
        info = (FILE_RENAME_INFO *)calloc(1, sz);
        if (info)
        {
            info->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
            info->FileNameLength = (DWORD)(tl * sizeof(wchar_t));
            memcpy(info->FileName, wt, tl * sizeof(wchar_t));
            if (SetFileInformationByHandle(h, FileRenameInfoEx, info, (DWORD)sz))
            {
                free(info);
                CloseHandle(h);
                return 0;
            }
            free(info);
        }
        CloseHandle(h);
    }
    if (!MoveFileExW(wf, wt, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
        return ols_set_errno_win();
    return 0;
}

int renameat(int fd1, const char *from, int fd2, const char *to)
{
    char f[PATH_MAX * 3], t[PATH_MAX * 3];
    if (at_path(fd1, from, f, sizeof(f)) < 0 || at_path(fd2, to, t, sizeof(t)) < 0)
        return -1;
    return ols_rename(f, t);
}

int mkdir(const char *path, mode_t mode)
{
    wchar_t w[PATH_MAX + 8];
    if (ols_path_to_w(path, w, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (!CreateDirectoryW(w, NULL))
        return ols_set_errno_win();
    return 0;
}

int mkdirat(int dirfd, const char *path, mode_t mode)
{
    char full[PATH_MAX * 3];
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    return mkdir(full, mode);
}

int rmdir(const char *path)
{
    wchar_t w[PATH_MAX + 8];
    if (ols_path_to_w(path, w, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (!RemoveDirectoryW(w))
    {
        if (GetLastError() == ERROR_DIRECTORY)
        {
            errno = ENOTDIR;
            return -1;
        }
        return ols_set_errno_win();
    }
    return 0;
}

int chdir(const char *path)
{
    wchar_t w[PATH_MAX + 8];
    if (ols_path_to_w(path, w, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (!SetCurrentDirectoryW(w))
        return ols_set_errno_win();
    return 0;
}

char *getcwd(char *buf, size_t size)
{
    wchar_t w[PATH_MAX];
    char tmp[PATH_MAX * 3];
    DWORD n = GetCurrentDirectoryW(PATH_MAX, w);
    if (n == 0 || n >= PATH_MAX)
    {
        ols_set_errno_win();
        return NULL;
    }
    if (ols_w_to_path(w, tmp, sizeof(tmp)) < 0)
        return NULL;
    if (!buf)
        return _strdup(tmp);
    if (strlen(tmp) >= size)
    {
        errno = ERANGE;
        return NULL;
    }
    strcpy(buf, tmp);
    return buf;
}

int access(const char *path, int mode)
{
    struct stat st;
    if (stat(path, &st) < 0)
        return -1;
    if ((mode & W_OK) && !(st.st_mode & S_IWUSR))
    {
        errno = EACCES;
        return -1;
    }
    if ((mode & X_OK) && !S_ISDIR(st.st_mode) && !(st.st_mode & S_IXUSR))
    {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int faccessat(int dirfd, const char *path, int mode, int flags)
{
    char full[PATH_MAX * 3];
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    return access(full, mode);
}

int chmod(const char *path, mode_t mode)
{
    wchar_t w[PATH_MAX + 8];
    DWORD a;
    if (ols_path_to_w(path, w, PATH_MAX + 8, NULL) < 0)
        return -1;
    a = GetFileAttributesW(w);
    if (a == INVALID_FILE_ATTRIBUTES)
        return ols_set_errno_win();
    if (a & FILE_ATTRIBUTE_DIRECTORY)
        return 0;
    if (mode & S_IWUSR)
        a &= ~FILE_ATTRIBUTE_READONLY;
    else
        a |= FILE_ATTRIBUTE_READONLY;
    if (!SetFileAttributesW(w, a))
        return ols_set_errno_win();
    return 0;
}

int chown(const char *path, uid_t uid, gid_t gid)
{
    struct stat st;
    return stat(path, &st);
}

int lchown(const char *path, uid_t uid, gid_t gid)
{
    struct stat st;
    return lstat(path, &st);
}

int fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags)
{
    return 0;
}

int link(const char *from, const char *to)
{
    wchar_t wf[PATH_MAX + 8], wt[PATH_MAX + 8];
    if (ols_path_to_w(from, wf, PATH_MAX + 8, NULL) < 0
        || ols_path_to_w(to, wt, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (!CreateHardLinkW(wt, wf, NULL))
        return ols_set_errno_win();
    return 0;
}

int symlink(const char *target, const char *path)
{
    wchar_t wt[PATH_MAX + 8], wp[PATH_MAX + 8];
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE, a;
    int n;
    if (ols_path_to_w(path, wp, PATH_MAX + 8, NULL) < 0)
        return -1;
    n = MultiByteToWideChar(CP_UTF8, 0, target, -1, wt, PATH_MAX);
    if (n <= 0)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    {
        wchar_t *p;
        for (p = wt; *p; ++p)
            if (*p == L'/')
                *p = L'\\';
    }
    {
        wchar_t abs[PATH_MAX + 8];
        if (ols_path_to_w(target, abs, PATH_MAX + 8, NULL) == 0)
        {
            a = GetFileAttributesW(abs);
            if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY))
                flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
        }
    }
    if (!CreateSymbolicLinkW(wp, wt, flags))
        return ols_set_errno_win();
    return 0;
}

int symlinkat(const char *target, int dirfd, const char *path)
{
    char full[PATH_MAX * 3];
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    return symlink(target, full);
}

typedef struct
{
    ULONG ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union
    {
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
    };
} OLS_REPARSE_DATA;

ssize_t readlink(const char *path, char *buf, size_t size)
{
    char data[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    OLS_REPARSE_DATA *rd = (OLS_REPARSE_DATA *)data;
    wchar_t name[PATH_MAX];
    char out[PATH_MAX * 3];
    DWORD got;
    HANDLE h;
    WCHAR *p;
    USHORT off, len;
    size_t l;
    int relative = 0;
    h = ols_open_path(path, FILE_READ_ATTRIBUTES,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING,
                      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, data, sizeof(data), &got, NULL))
    {
        CloseHandle(h);
        errno = EINVAL;
        return -1;
    }
    CloseHandle(h);
    if (got < 8)
    {
        errno = EINVAL;
        return -1;
    }
    if (rd->ReparseTag == IO_REPARSE_TAG_SYMLINK)
    {
        p = rd->SymbolicLinkReparseBuffer.PathBuffer;
        off = rd->SymbolicLinkReparseBuffer.SubstituteNameOffset;
        len = rd->SymbolicLinkReparseBuffer.SubstituteNameLength;
        relative = (rd->SymbolicLinkReparseBuffer.Flags & 1) != 0;
    }
    else if (rd->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
    {
        p = rd->MountPointReparseBuffer.PathBuffer;
        off = rd->MountPointReparseBuffer.SubstituteNameOffset;
        len = rd->MountPointReparseBuffer.SubstituteNameLength;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
    if ((off & 1) || (len & 1) || (char *)p + off + len > data + got || len / sizeof(WCHAR) >= PATH_MAX)
    {
        errno = EINVAL;
        return -1;
    }
    len /= sizeof(WCHAR);
    memcpy(name, (char *)p + off, len * sizeof(WCHAR));
    name[len] = 0;
    if (relative)
    {
        char *q;
        if (WideCharToMultiByte(CP_UTF8, 0, name, -1, out, sizeof(out), NULL, NULL) <= 0)
        {
            errno = EINVAL;
            return -1;
        }
        for (q = out; *q; ++q)
            if (*q == '\\')
                *q = '/';
    }
    else
    {
        if (wcsncmp(name, L"\\??\\", 4) != 0)
        {
            errno = EINVAL;
            return -1;
        }
        memmove(name, name + 4, (wcslen(name + 4) + 1) * sizeof(WCHAR));
        if (ols_w_to_path(name, out, sizeof(out)) < 0)
            return -1;
        if (out[0] != '/')
        {
            errno = EXDEV;
            return -1;
        }
    }
    l = strlen(out);
    if (l > size)
        l = size;
    memcpy(buf, out, l);
    return (ssize_t)l;
}

ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t size)
{
    char full[PATH_MAX * 3];
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    return readlink(full, buf, size);
}

char *realpath(const char *path, char *resolved)
{
    wchar_t w[PATH_MAX];
    char out[PATH_MAX * 3];
    HANDLE h;
    DWORD n;
    h = ols_open_path(path, FILE_READ_ATTRIBUTES,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    n = GetFinalPathNameByHandleW(h, w, PATH_MAX, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (n == 0 || n >= PATH_MAX)
    {
        ols_set_errno_win();
        return NULL;
    }
    if (ols_w_to_path(w, out, sizeof(out)) < 0)
        return NULL;
    if (strlen(out) >= PATH_MAX)
    {
        errno = ENAMETOOLONG;
        return NULL;
    }
    if (!resolved)
        return _strdup(out);
    strcpy(resolved, out);
    return resolved;
}

mode_t umask(mode_t mask)
{
    mode_t old = g_umask;
    g_umask = mask & 0777;
    return old;
}

static int tmp_fill(char *tmpl)
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t len = strlen(tmpl), i;
    unsigned char rnd[6];
    if (len < 6 || strcmp(tmpl + len - 6, "XXXXXX") != 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (BCryptGenRandom(NULL, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    {
        errno = EIO;
        return -1;
    }
    for (i = 0; i < 6; ++i)
        tmpl[len - 6 + i] = chars[rnd[i] % (sizeof(chars) - 1)];
    return 0;
}

int mkostemp(char *tmpl, int flags)
{
    size_t len = strlen(tmpl);
    char save[7];
    int i, fd;
    if (len < 6)
    {
        errno = EINVAL;
        return -1;
    }
    memcpy(save, tmpl + len - 6, 7);
    for (i = 0; i < 100; ++i)
    {
        memcpy(tmpl + len - 6, save, 7);
        if (tmp_fill(tmpl) < 0)
            return -1;
        fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL | (flags & ~O_ACCMODE), 0600);
        if (fd >= 0 || errno != EEXIST)
            return fd;
    }
    errno = EEXIST;
    return -1;
}

int mkstemp(char *tmpl)
{
    return mkostemp(tmpl, 0);
}

char *mkdtemp(char *tmpl)
{
    size_t len = strlen(tmpl);
    char save[7];
    int i;
    if (len < 6)
    {
        errno = EINVAL;
        return NULL;
    }
    memcpy(save, tmpl + len - 6, 7);
    for (i = 0; i < 100; ++i)
    {
        memcpy(tmpl + len - 6, save, 7);
        if (tmp_fill(tmpl) < 0)
            return NULL;
        if (mkdir(tmpl, 0700) == 0)
            return tmpl;
        if (errno != EEXIST)
            return NULL;
    }
    return NULL;
}

char *mktemp(char *tmpl)
{
    if (tmp_fill(tmpl) < 0)
        tmpl[0] = 0;
    return tmpl;
}

int chroot(const char *path)
{
    errno = EPERM;
    return -1;
}

static int set_times(const char *path, int fd, const struct timespec *ts)
{
    HANDLE h;
    FILETIME a, m, *pa = NULL, *pm = NULL;
    int own = 0, ret = 0;
    if (fd >= 0)
        h = ols_handle_from_fd(fd);
    else
    {
        h = ols_open_path(path, FILE_WRITE_ATTRIBUTES,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return -1;
        own = 1;
    }
    if (ts)
    {
        int k;
        FILETIME *outs[2];
        FILETIME **ptrs[2];
        outs[0] = &a;
        outs[1] = &m;
        ptrs[0] = &pa;
        ptrs[1] = &pm;
        for (k = 0; k < 2; ++k)
        {
            ULARGE_INTEGER v;
            if (ts[k].tv_nsec == UTIME_OMIT)
                continue;
            if (ts[k].tv_nsec == UTIME_NOW)
                GetSystemTimeAsFileTime(outs[k]);
            else
            {
                v.QuadPart = (uint64_t)ts[k].tv_sec * 10000000ULL + ts[k].tv_nsec / 100
                             + 116444736000000000ULL;
                outs[k]->dwLowDateTime = v.LowPart;
                outs[k]->dwHighDateTime = v.HighPart;
            }
            *ptrs[k] = outs[k];
        }
    }
    else
    {
        GetSystemTimeAsFileTime(&a);
        m = a;
        pa = &a;
        pm = &m;
    }
    if (!SetFileTime(h, NULL, pa, pm))
        ret = ols_set_errno_win();
    if (own)
        CloseHandle(h);
    return ret;
}

int utime(const char *path, const struct utimbuf *times)
{
    struct timespec ts[2];
    if (!times)
        return set_times(path, -1, NULL);
    ts[0].tv_sec = times->actime;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = times->modtime;
    ts[1].tv_nsec = 0;
    return set_times(path, -1, ts);
}

int utimes(const char *path, const struct timeval tv[2])
{
    struct timespec ts[2];
    if (!tv)
        return set_times(path, -1, NULL);
    ts[0].tv_sec = tv[0].tv_sec;
    ts[0].tv_nsec = tv[0].tv_usec * 1000;
    ts[1].tv_sec = tv[1].tv_sec;
    ts[1].tv_nsec = tv[1].tv_usec * 1000;
    return set_times(path, -1, ts);
}

int futimes(int fd, const struct timeval tv[2])
{
    struct timespec ts[2];
    if (!tv)
        return set_times(NULL, fd, NULL);
    ts[0].tv_sec = tv[0].tv_sec;
    ts[0].tv_nsec = tv[0].tv_usec * 1000;
    ts[1].tv_sec = tv[1].tv_sec;
    ts[1].tv_nsec = tv[1].tv_usec * 1000;
    return set_times(NULL, fd, ts);
}

int futimens(int fd, const struct timespec ts[2])
{
    return set_times(NULL, fd, ts);
}

int utimensat(int dirfd, const char *path, const struct timespec ts[2], int flags)
{
    char full[PATH_MAX * 3];
    if (at_path(dirfd, path, full, sizeof(full)) < 0)
        return -1;
    return set_times(full, -1, ts);
}

int truncate(const char *path, off_t len)
{
    int fd = open(path, O_WRONLY), r;
    if (fd < 0)
        return -1;
    r = ftruncate(fd, len);
    close(fd);
    return r;
}

int statvfs(const char *path, struct statvfs *buf)
{
    wchar_t w[PATH_MAX + 8];
    ULARGE_INTEGER avail, total, freeb;
    if (ols_path_to_w(path, w, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (!GetDiskFreeSpaceExW(w, &avail, &total, &freeb))
        return ols_set_errno_win();
    memset(buf, 0, sizeof(*buf));
    buf->f_bsize = 4096;
    buf->f_frsize = 4096;
    buf->f_blocks = total.QuadPart / 4096;
    buf->f_bfree = freeb.QuadPart / 4096;
    buf->f_bavail = avail.QuadPart / 4096;
    buf->f_files = 0xffffffff;
    buf->f_ffree = 0xffffffff;
    buf->f_favail = 0xffffffff;
    buf->f_namemax = 255;
    return 0;
}

int fstatvfs(int fd, struct statvfs *buf)
{
    return statvfs(".", buf);
}

struct ols_dir
{
    HANDLE find;
    WIN32_FIND_DATAW data;
    int first;
    int fd;
    struct dirent ent;
    wchar_t pattern[PATH_MAX + 8];
};

DIR *opendir(const char *path)
{
    struct ols_dir *d;
    DWORD a;
    size_t l;
    d = (struct ols_dir *)calloc(1, sizeof(*d));
    if (!d)
    {
        errno = ENOMEM;
        return NULL;
    }
    if (ols_path_to_w(path, d->pattern, PATH_MAX, NULL) < 0)
    {
        free(d);
        return NULL;
    }
    a = GetFileAttributesW(d->pattern);
    if (a == INVALID_FILE_ATTRIBUTES)
    {
        ols_set_errno_win();
        free(d);
        return NULL;
    }
    if (!(a & FILE_ATTRIBUTE_DIRECTORY))
    {
        errno = ENOTDIR;
        free(d);
        return NULL;
    }
    l = wcslen(d->pattern);
    if (l && d->pattern[l - 1] != L'\\')
        d->pattern[l++] = L'\\';
    d->pattern[l++] = L'*';
    d->pattern[l] = 0;
    d->find = FindFirstFileExW(d->pattern, FindExInfoBasic, &d->data, FindExSearchNameMatch,
                               NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (d->find == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            ols_set_errno_win();
            free(d);
            return NULL;
        }
    }
    d->first = 1;
    d->fd = -1;
    return d;
}

DIR *fdopendir(int fd)
{
    wchar_t w[PATH_MAX];
    char p[PATH_MAX * 3];
    DWORD n = GetFinalPathNameByHandleW(ols_handle_from_fd(fd), w, PATH_MAX,
                                        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    DIR *d;
    if (n == 0 || n >= PATH_MAX)
    {
        ols_set_errno_win();
        return NULL;
    }
    if (ols_w_to_path(w, p, sizeof(p)) < 0)
        return NULL;
    d = opendir(p);
    if (d)
        d->fd = fd;
    return d;
}

struct dirent *readdir(DIR *d)
{
    for (;;)
    {
        if (d->find == INVALID_HANDLE_VALUE)
            return NULL;
        if (!d->first)
        {
            if (!FindNextFileW(d->find, &d->data))
                return NULL;
        }
        d->first = 0;
        if (WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1, d->ent.d_name,
                                sizeof(d->ent.d_name), NULL, NULL) <= 0)
            continue;
        if (d->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            d->ent.d_type = DT_LNK;
        else if (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            d->ent.d_type = DT_DIR;
        else
            d->ent.d_type = DT_REG;
        d->ent.d_ino = 1;
        d->ent.d_reclen = sizeof(d->ent);
        return &d->ent;
    }
}

int readdir_r(DIR *d, struct dirent *entry, struct dirent **result)
{
    struct dirent *e = readdir(d);
    if (!e)
    {
        *result = NULL;
        return 0;
    }
    memcpy(entry, e, sizeof(*entry));
    *result = entry;
    return 0;
}

int closedir(DIR *d)
{
    if (!d)
    {
        errno = EBADF;
        return -1;
    }
    if (d->find != INVALID_HANDLE_VALUE)
        FindClose(d->find);
    if (d->fd >= 0)
        close(d->fd);
    free(d);
    return 0;
}

void rewinddir(DIR *d)
{
    if (d->find != INVALID_HANDLE_VALUE)
        FindClose(d->find);
    d->find = FindFirstFileExW(d->pattern, FindExInfoBasic, &d->data, FindExSearchNameMatch,
                               NULL, FIND_FIRST_EX_LARGE_FETCH);
    d->first = 1;
}

int dirfd(DIR *d)
{
    return d->fd;
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *dir, struct dirent ***list,
            int (*filter)(const struct dirent *),
            int (*cmp)(const struct dirent **, const struct dirent **))
{
    DIR *d = opendir(dir);
    struct dirent *e, **v = NULL, **nv;
    int n = 0, cap = 0;
    if (!d)
        return -1;
    while ((e = readdir(d)) != NULL)
    {
        if (filter && !filter(e))
            continue;
        if (n == cap)
        {
            cap = cap ? cap * 2 : 32;
            nv = (struct dirent **)realloc(v, cap * sizeof(*v));
            if (!nv)
                goto fail;
            v = nv;
        }
        v[n] = (struct dirent *)malloc(sizeof(struct dirent));
        if (!v[n])
            goto fail;
        memcpy(v[n++], e, sizeof(struct dirent));
    }
    closedir(d);
    if (cmp && n > 1)
        qsort(v, n, sizeof(*v), (int (*)(const void *, const void *))cmp);
    *list = v;
    return n;
fail:
    while (n > 0)
        free(v[--n]);
    free(v);
    closedir(d);
    errno = ENOMEM;
    return -1;
}

int shm_open(const char *name, int oflag, mode_t mode)
{
    char path[PATH_MAX];
    if (name[0] == '/')
        ++name;
    snprintf(path, sizeof(path), "/dev/shm/%s", name);
    return open(path, oflag, mode);
}

int shm_unlink(const char *name)
{
    char path[PATH_MAX];
    if (name[0] == '/')
        ++name;
    snprintf(path, sizeof(path), "/dev/shm/%s", name);
    return unlink(path);
}

void ols_ensure_tmp_dirs(void)
{
    wchar_t w[PATH_MAX + 16];
    const wchar_t *r = tmp_root();
    CreateDirectoryW(r, NULL);
    swprintf(w, PATH_MAX + 16, L"%s/shm", r);
    CreateDirectoryW(w, NULL);
}

int ols_copy_file(const char *src, const char *dst)
{
    wchar_t ws[PATH_MAX + 8], wd[PATH_MAX + 8];
    if (ols_path_to_w(src, ws, PATH_MAX + 8, NULL) < 0 || ols_path_to_w(dst, wd, PATH_MAX + 8, NULL) < 0)
        return -1;
    if (!CopyFileW(ws, wd, FALSE))
        return ols_set_errno_win();
    return 0;
}
