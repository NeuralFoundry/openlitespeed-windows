#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <new>

static void *poison_alloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (p)
        memset(p, 0xA5, n);
    return p;
}

static void poison_free(void *p)
{
    if (p)
    {
        memset(p, 0x5A, _msize(p));
        free(p);
    }
}

void *operator new(size_t n)
{
    void *p = poison_alloc(n);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void *operator new[](size_t n)
{
    void *p = poison_alloc(n);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void *operator new(size_t n, const std::nothrow_t &) noexcept      { return poison_alloc(n); }
void *operator new[](size_t n, const std::nothrow_t &) noexcept    { return poison_alloc(n); }
void operator delete(void *p) noexcept                             { poison_free(p); }
void operator delete[](void *p) noexcept                           { poison_free(p); }
void operator delete(void *p, size_t) noexcept                     { poison_free(p); }
void operator delete[](void *p, size_t) noexcept                   { poison_free(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept     { poison_free(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept   { poison_free(p); }
