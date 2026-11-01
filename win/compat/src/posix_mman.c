#include "compat_internal.h"

typedef struct
{
    char *user;
    void *base;
    size_t len;
    int anon;
} mapping_t;

static SRWLOCK g_map_lock = SRWLOCK_INIT;
static mapping_t *g_maps;
static int g_nmaps, g_capmaps;

static int add_map(void *user, void *base, size_t len, int anon)
{
    AcquireSRWLockExclusive(&g_map_lock);
    if (g_nmaps == g_capmaps)
    {
        int cap = g_capmaps ? g_capmaps * 2 : 64;
        mapping_t *n = (mapping_t *)realloc(g_maps, cap * sizeof(mapping_t));
        if (!n)
        {
            ReleaseSRWLockExclusive(&g_map_lock);
            return -1;
        }
        g_maps = n;
        g_capmaps = cap;
    }
    g_maps[g_nmaps].user = (char *)user;
    g_maps[g_nmaps].base = base;
    g_maps[g_nmaps].len = len;
    g_maps[g_nmaps].anon = anon;
    ++g_nmaps;
    ReleaseSRWLockExclusive(&g_map_lock);
    return 0;
}

static int take_map(void *addr, mapping_t *out)
{
    int i;
    AcquireSRWLockExclusive(&g_map_lock);
    for (i = 0; i < g_nmaps; ++i)
    {
        if (g_maps[i].user == (char *)addr
            || ((char *)addr > g_maps[i].user && (char *)addr < g_maps[i].user + g_maps[i].len))
        {
            *out = g_maps[i];
            g_maps[i] = g_maps[--g_nmaps];
            ReleaseSRWLockExclusive(&g_map_lock);
            return 0;
        }
    }
    ReleaseSRWLockExclusive(&g_map_lock);
    return -1;
}

static DWORD page_prot(int prot, int priv)
{
    if (prot & PROT_EXEC)
    {
        if (prot & PROT_WRITE)
            return priv ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READWRITE;
        return prot & PROT_READ ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    }
    if (prot & PROT_WRITE)
        return priv ? PAGE_WRITECOPY : PAGE_READWRITE;
    if (prot & PROT_READ)
        return PAGE_READONLY;
    return PAGE_NOACCESS;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    SYSTEM_INFO si;
    HANDLE h, m;
    LARGE_INTEGER fsz;
    uint64_t aligned, delta, maxsz;
    DWORD access, protect;
    char *view;
    int priv = (flags & MAP_PRIVATE) != 0;
    if (len == 0)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if (flags & MAP_ANONYMOUS)
    {
        void *p = VirtualAlloc((flags & MAP_FIXED) ? addr : NULL, len, MEM_RESERVE | MEM_COMMIT,
                               page_prot(prot, 0));
        if (!p)
        {
            ols_set_errno_win();
            return MAP_FAILED;
        }
        add_map(p, p, len, 1);
        return p;
    }
    h = ols_handle_from_fd(fd);
    GetSystemInfo(&si);
    aligned = (uint64_t)off & ~((uint64_t)si.dwAllocationGranularity - 1);
    delta = (uint64_t)off - aligned;
    protect = page_prot(prot, priv);
    if (protect == PAGE_NOACCESS)
        protect = PAGE_READONLY;
    if (!GetFileSizeEx(h, &fsz))
    {
        ols_set_errno_win();
        return MAP_FAILED;
    }
    maxsz = (uint64_t)off + len;
    if (maxsz <= (uint64_t)fsz.QuadPart || !(prot & PROT_WRITE) || priv)
    {
        if ((uint64_t)fsz.QuadPart == 0)
        {
            errno = ENODEV;
            return MAP_FAILED;
        }
        maxsz = 0;
        if ((uint64_t)off + len > (uint64_t)fsz.QuadPart)
            len = (size_t)((uint64_t)fsz.QuadPart - (uint64_t)off);
    }
    m = CreateFileMappingW(h, NULL, protect, (DWORD)(maxsz >> 32), (DWORD)maxsz, NULL);
    if (!m)
    {
        ols_set_errno_win();
        return MAP_FAILED;
    }
    if (priv)
        access = FILE_MAP_COPY;
    else if (prot & PROT_WRITE)
        access = FILE_MAP_WRITE | FILE_MAP_READ;
    else
        access = FILE_MAP_READ;
    if (prot & PROT_EXEC)
        access |= FILE_MAP_EXECUTE;
    view = (char *)MapViewOfFileEx(m, access, (DWORD)(aligned >> 32), (DWORD)aligned,
                                   (SIZE_T)(len + delta),
                                   (flags & MAP_FIXED) ? (char *)addr - delta : NULL);
    CloseHandle(m);
    if (!view)
    {
        ols_set_errno_win();
        return MAP_FAILED;
    }
    add_map(view + delta, view, len + delta, 0);
    return view + delta;
}

int munmap(void *addr, size_t len)
{
    mapping_t mp;
    if (take_map(addr, &mp) < 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (mp.anon)
    {
        if (!VirtualFree(mp.base, 0, MEM_RELEASE))
            return ols_set_errno_win();
    }
    else if (!UnmapViewOfFile(mp.base))
        return ols_set_errno_win();
    return 0;
}

int msync(void *addr, size_t len, int flags)
{
    if (!FlushViewOfFile(addr, len))
        return ols_set_errno_win();
    return 0;
}

int madvise(void *addr, size_t len, int advice)
{
    return 0;
}

int posix_madvise(void *addr, size_t len, int advice)
{
    return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
    DWORD old;
    if (!VirtualProtect(addr, len, page_prot(prot, 0), &old))
        return ols_set_errno_win();
    return 0;
}

int mlock(const void *addr, size_t len)
{
    if (!VirtualLock((LPVOID)addr, len))
        return ols_set_errno_win();
    return 0;
}

int munlock(const void *addr, size_t len)
{
    VirtualUnlock((LPVOID)addr, len);
    return 0;
}

int mlockall(int flags)
{
    return 0;
}
