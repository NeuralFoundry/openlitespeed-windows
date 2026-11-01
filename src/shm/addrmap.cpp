/*
 * 
 */

#include "addrmap.h"
#include <shm/lsshmtypes.h>
#include <shm/lsshmhash.h>
#include <sys/mman.h>


int Addr2OffMap::alloc(size_t size)
{
    assert(m_table == NULL);
    m_table = (AddrOffPair *)malloc(size * sizeof(*m_table));
    if (m_table)
    {
        memset(m_table, 0, size * sizeof(*m_table));
        m_tableEnd  = m_table + size;
        m_tableSize = size;
        return LS_OK;
    }
    return LS_FAIL;
}    


void Addr2OffMap::release()
{
    if (m_table)
    {
        free(m_table);
        m_table = NULL;
        m_tableEnd = NULL;
        m_tableSize = 0;
    }
}


void Addr2OffMap::swap(Addr2OffMap &rhs)
{
    char buf[sizeof(Addr2OffMap)];
    memmove(buf, (void *) &rhs, sizeof(buf));
    memmove((void *) &rhs, this, sizeof(buf));
    memmove(this, buf, sizeof(buf));
}


int Addr2OffMap::rehash(int new_size)
{
    new_size = LsShmHash::roundUp(new_size);

    Addr2OffMap newTable(new_size);
    if (newTable.m_table == NULL)
        return LS_FAIL;
    AddrOffPair *p = m_table;
    AddrOffPair *pEnd = m_table + m_tableSize;
    while(p < pEnd)
    {
        if (p->m_ptr)
        {
            if (newTable.addEx(p) == LS_FAIL)
            {
                newTable.release();
                return rehash(new_size * 2);
            }
        }
        p++;
    }
    swap(newTable);
    return LS_OK;
}
    
    
int Addr2OffMap::add_collide(const AddrOffPair *p, AddrOffPair *pInfo)
{
    AddrOffPair *pEnd = pInfo + 3;
    if (pEnd >= m_tableEnd)
        pEnd = m_table + (pEnd - m_tableEnd);
    while(pInfo != pEnd)
    {
        assert(((size_t)pInfo->m_ptr) >> LARGE_PAGE_BITS
                != ((size_t)p->m_ptr) >> LARGE_PAGE_BITS);
        if (pInfo == m_tableEnd - 1)
        {
            pInfo = m_table;
        }
        else
        {
            ++pInfo;
        }
        if (pInfo->m_ptr == NULL)
        {
            memmove(pInfo, p, sizeof(*p));
            return LS_OK;
        }
    }
    return LS_FAIL;
}


const AddrOffPair *Addr2OffMap::lookup_collid(const char *p, AddrOffPair *pInfo) const
{
    AddrOffPair *pEnd = pInfo + 3;
    if (pEnd >= m_tableEnd)
        pEnd = m_table + (pEnd - m_tableEnd);
    while(pInfo != pEnd)
    {
        if (pInfo == m_tableEnd - 1)
        {
            pInfo = m_table;
        }
        else
        {
            ++pInfo;
        }
        if (((size_t)pInfo->m_ptr >> LARGE_PAGE_BITS) == 
            ((size_t)p) >> LARGE_PAGE_BITS)
        {
            return pInfo;
        }
    }
    return NULL;
}
    


AddrMap::~AddrMap()
{

}


int AddrMap::addLargePage(AddrOffPair info, size_t size)
{
    assert((info.m_offset & LARGE_PAGE_MASK) == 0);
    assert((size   & LARGE_PAGE_MASK) == 0);
#ifndef NDEBUG
    int page = info.m_offset >> LARGE_PAGE_BITS;
#endif
    assert(page == m_off2ptrTable.size());
    char * end = info.m_ptr + size;
    while(info.m_ptr < end)
    {
        if (m_off2ptrTable.push_back(info.m_ptr) == -1)
            return LS_FAIL;
        m_ptr2offTable.add(&info);
        info.m_ptr += LARGE_PAGE_SIZE;
        info.m_offset += LARGE_PAGE_SIZE;
    }
    return LS_OK;
}


#ifdef _WIN32

#include <map>

#ifndef MEM_RESERVE_PLACEHOLDER
#define MEM_RESERVE_PLACEHOLDER     0x00040000
#endif
#ifndef MEM_REPLACE_PLACEHOLDER
#define MEM_REPLACE_PLACEHOLDER     0x00004000
#endif
#ifndef MEM_PRESERVE_PLACEHOLDER
#define MEM_PRESERVE_PLACEHOLDER    0x00000002
#endif
#ifndef MEM_COALESCE_PLACEHOLDERS
#define MEM_COALESCE_PLACEHOLDERS   0x00000001
#endif

typedef PVOID (WINAPI *ols_va2_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, void *, ULONG);
typedef PVOID (WINAPI *ols_mv3_t)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG,
                                  void *, ULONG);
typedef BOOL (WINAPI *ols_uv2_t)(HANDLE, PVOID, ULONG);

static ols_va2_t s_va2;
static ols_mv3_t s_mv3;
static ols_uv2_t s_uv2;
static std::map<char *, size_t> s_viewLen;

static int loadPlaceholderApi()
{
    if (s_va2)
        return 0;
    HMODULE m = GetModuleHandleW(L"kernelbase.dll");
    if (!m)
        return -1;
    s_mv3 = (ols_mv3_t)GetProcAddress(m, "MapViewOfFile3");
    s_uv2 = (ols_uv2_t)GetProcAddress(m, "UnmapViewOfFile2");
    s_va2 = (ols_va2_t)GetProcAddress(m, "VirtualAlloc2");
    if (!s_va2 || !s_mv3 || !s_uv2)
    {
        s_va2 = NULL;
        errno = ENOSYS;
        return -1;
    }
    return 0;
}

int AddrMap::mapAddrSpace(size_t total)
{
    int needed = total - getMaxOffset();
    if (needed <= 0)
        return LS_OK;
    if (loadPlaceholderApi() != 0)
        return LS_FAIL;
    size_t bytes = (needed + LARGE_PAGE_MASK) & ~LARGE_PAGE_MASK;
    char *base = (char *)s_va2(NULL, NULL, bytes + LARGE_PAGE_SIZE,
                               MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                               NULL, 0);
    if (!base)
        return LS_FAIL;
    char *aligned = (char *)(((uintptr_t)base + LARGE_PAGE_MASK) & ~LARGE_PAGE_MASK);
    char *end = base + bytes + LARGE_PAGE_SIZE;
    if (aligned > base)
    {
        VirtualFree(base, aligned - base, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
        VirtualFree(base, 0, MEM_RELEASE);
    }
    if (aligned + bytes < end)
    {
        VirtualFree(aligned, bytes, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
        VirtualFree(aligned + bytes, 0, MEM_RELEASE);
    }
    for (char *p = aligned; p + LARGE_PAGE_SIZE < aligned + bytes; p += LARGE_PAGE_SIZE)
        VirtualFree(p, LARGE_PAGE_SIZE, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    AddrOffPair pair;
    pair.m_ptr = aligned;
    pair.m_offset = getMaxOffset();
    if (addLargePage(pair, bytes) == LS_FAIL)
    {
        for (char *p = aligned; p < aligned + bytes; p += LARGE_PAGE_SIZE)
            VirtualFree(p, 0, MEM_RELEASE);
        return LS_FAIL;
    }
    return LS_OK;
}


int AddrMap::remap(int fd, size_t start_offset, size_t new_size,
                   size_t page_size)
{
    if (mapAddrSpace(new_size) == LS_FAIL)
        return LS_FAIL;
    HANDLE hFile = ols_handle_from_fd(fd);
    HANDLE hSec = CreateFileMappingW(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hSec)
    {
        errno = ols_errno_from_win(GetLastError());
        return LS_FAIL;
    }
    size_t page_start = start_offset & ~LARGE_PAGE_MASK;
    int ret = LS_OK;
    while (page_start < new_size)
    {
        size_t block_end = page_start + LARGE_PAGE_SIZE;
        if (block_end > new_size)
            block_end = new_size;
        size_t len = block_end - page_start;
        char *ptr = offset2ptr(page_start);
        std::map<char *, size_t>::iterator it = s_viewLen.find(ptr);
        size_t cur = (it == s_viewLen.end()) ? 0 : it->second;
        if (cur != len)
        {
            if (cur > 0)
            {
                s_uv2(GetCurrentProcess(), ptr, MEM_PRESERVE_PLACEHOLDER);
                if (cur < LARGE_PAGE_SIZE)
                    VirtualFree(ptr, LARGE_PAGE_SIZE,
                                MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
                s_viewLen.erase(ptr);
            }
            if (len < LARGE_PAGE_SIZE)
                VirtualFree(ptr, len, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
            void *view = s_mv3(hSec, GetCurrentProcess(), ptr, (ULONG64)page_start,
                               len, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
            if (view != ptr)
            {
                errno = ols_errno_from_win(GetLastError());
                ret = LS_FAIL;
                break;
            }
            s_viewLen[ptr] = len;
        }
        page_start = block_end;
    }
    CloseHandle(hSec);
    return ret;
}

static int unmap_large_page(void *pPage)
{
    char *ptr = (char *)pPage;
    std::map<char *, size_t>::iterator it = s_viewLen.find(ptr);
    if (it != s_viewLen.end())
    {
        size_t len = it->second;
        s_uv2(GetCurrentProcess(), ptr, MEM_PRESERVE_PLACEHOLDER);
        if (len < LARGE_PAGE_SIZE)
            VirtualFree(ptr, LARGE_PAGE_SIZE, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
        s_viewLen.erase(it);
    }
    VirtualFree(ptr, 0, MEM_RELEASE);
    return 0;
}


void AddrMap::unmap()
{
    m_off2ptrTable.for_each(m_off2ptrTable.begin(), m_off2ptrTable.end(),
                            unmap_large_page);
    m_off2ptrTable.clear();
    m_ptr2offTable.clear();
}

#else

int AddrMap::mapAddrSpace(size_t total)
{
    int needed = total - getMaxOffset();
    if (needed <= 0)
        return LS_OK;
    AddrOffPair pair;
    size_t bytes = (needed + LARGE_PAGE_MASK ) & ~LARGE_PAGE_MASK;
    pair.m_ptr = (char *) mmap(NULL, bytes + LARGE_PAGE_SIZE, PROT_NONE,
                               MAP_ANON|MAP_SHARED, -1, 0);
    if (pair.m_ptr == MAP_FAILED)
        return LS_FAIL;
    if (((uintptr_t)pair.m_ptr & LARGE_PAGE_MASK) == 0)
        bytes += LARGE_PAGE_SIZE;
    else
    {
        char *pRelease = pair.m_ptr;
        size_t size; 
        pair.m_ptr = (char *)(((uintptr_t)pair.m_ptr & ~LARGE_PAGE_MASK) 
                            + LARGE_PAGE_SIZE);
        size = pair.m_ptr - pRelease;
        munmap(pRelease, size);
        pRelease = pair.m_ptr + bytes;
        munmap(pRelease, LARGE_PAGE_SIZE - size);
    }
    pair.m_offset = getMaxOffset();
    if (addLargePage(pair, bytes) == LS_FAIL)
    {
        munmap(pair.m_ptr, bytes);
        return LS_FAIL;
    }
    return LS_OK;
    
}


int AddrMap::remap(int fd, size_t start_offset, size_t new_size,
                   size_t page_size)
{
    if (mapAddrSpace(new_size) == LS_FAIL)
        return LS_FAIL;
    start_offset = start_offset & ~(page_size - 1);
    while(start_offset < new_size)
    {
        size_t block_end = (start_offset + LARGE_PAGE_SIZE) & ~LARGE_PAGE_MASK;
        if (block_end > new_size)
            block_end = new_size;
        char * ptr = offset2ptr(start_offset);
        ptr = (char *)mmap(ptr, block_end - start_offset, PROT_READ | PROT_WRITE,
                            MAP_FIXED | MAP_SHARED, fd, start_offset);
        if (ptr == MAP_FAILED)
            return LS_FAIL;
        start_offset = block_end;
    }
    return LS_OK;
    
}

static int unmap_large_page(void *pPage)
{
    munmap(pPage, LARGE_PAGE_SIZE);
    return 0;
}


void AddrMap::unmap()
{
    m_off2ptrTable.for_each(m_off2ptrTable.begin(), m_off2ptrTable.end(), 
                            unmap_large_page);
    m_off2ptrTable.clear();
    m_ptr2offTable.clear();
}


#endif


size_t AddrMap::getAvailAddrSpace( size_t offset, size_t required_size)
{
    size_t avail = 0;
    int page = offset >> LARGE_PAGE_BITS;
    avail = LARGE_PAGE_SIZE - (offset & LARGE_PAGE_MASK);
    while(avail < required_size && page < m_off2ptrTable.size() - 1)
    {
        if (required_size > LARGE_PAGE_SIZE 
            && m_off2ptrTable[page + 1] - m_off2ptrTable[page] == LARGE_PAGE_SIZE)
        {
            avail += LARGE_PAGE_SIZE;
            ++page;
        }
        else
            break;
    }
    return avail;    
}
