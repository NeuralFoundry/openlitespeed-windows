#include "compat_internal.h"
#include <ctype.h>

int strcasecmp(const char *a, const char *b)
{
    return _stricmp(a, b);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    return _strnicmp(a, b, n);
}

char *strcasestr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    if (!nl)
        return (char *)h;
    for (; *h; ++h)
        if (tolower((unsigned char)*h) == tolower((unsigned char)*n) && _strnicmp(h, n, nl) == 0)
            return (char *)h;
    return NULL;
}

void *memmem(const void *h, size_t hl, const void *n, size_t nl)
{
    const char *p = (const char *)h, *end;
    if (nl == 0)
        return (void *)h;
    if (hl < nl)
        return NULL;
    end = p + hl - nl;
    for (; p <= end; ++p)
    {
        p = (const char *)memchr(p, *(const char *)n, end - p + 1);
        if (!p)
            return NULL;
        if (memcmp(p, n, nl) == 0)
            return (void *)p;
    }
    return NULL;
}

void *memrchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--)
        if (*--p == (unsigned char)c)
            return (void *)p;
    return NULL;
}

void *rawmemchr(const void *s, int c)
{
    const unsigned char *p = (const unsigned char *)s;
    while (*p != (unsigned char)c)
        ++p;
    return (void *)p;
}

void *mempcpy(void *dst, const void *src, size_t n)
{
    return (char *)memcpy(dst, src, n) + n;
}

char *strndup(const char *s, size_t n)
{
    size_t l = strnlen(s, n);
    char *d = (char *)malloc(l + 1);
    if (!d)
        return NULL;
    memcpy(d, s, l);
    d[l] = 0;
    return d;
}

char *strsep(char **sp, const char *delim)
{
    char *s = *sp, *e;
    if (!s)
        return NULL;
    e = s + strcspn(s, delim);
    if (*e)
        *e++ = 0;
    else
        e = NULL;
    *sp = e;
    return s;
}

char *stpcpy(char *dst, const char *src)
{
    size_t l = strlen(src);
    memcpy(dst, src, l + 1);
    return dst + l;
}

char *stpncpy(char *dst, const char *src, size_t n)
{
    size_t l = strnlen(src, n);
    memcpy(dst, src, l);
    memset(dst + l, 0, n - l);
    return dst + l;
}

size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t l = strlen(src);
    if (size)
    {
        size_t c = l >= size ? size - 1 : l;
        memcpy(dst, src, c);
        dst[c] = 0;
    }
    return l;
}

size_t strlcat(char *dst, const char *src, size_t size)
{
    size_t dl = strnlen(dst, size);
    if (dl == size)
        return size + strlen(src);
    return dl + strlcpy(dst + dl, src, size - dl);
}

char *strchrnul(const char *s, int c)
{
    while (*s && *s != (char)c)
        ++s;
    return (char *)s;
}

char *index(const char *s, int c)
{
    return (char *)strchr(s, c);
}

char *rindex(const char *s, int c)
{
    return (char *)strrchr(s, c);
}

void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}

void bcopy(const void *src, void *dst, size_t n)
{
    memmove(dst, src, n);
}

int bcmp(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}

int ffs(int i)
{
    unsigned long idx;
    if (!_BitScanForward(&idx, (unsigned long)i))
        return 0;
    return (int)idx + 1;
}

int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !*name || strchr(name, '='))
    {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name))
        return 0;
    if (_putenv_s(name, value) != 0)
    {
        errno = ENOMEM;
        return -1;
    }
    SetEnvironmentVariableA(name, value);
    return 0;
}

int unsetenv(const char *name)
{
    if (!name || !*name || strchr(name, '='))
    {
        errno = EINVAL;
        return -1;
    }
    _putenv_s(name, "");
    SetEnvironmentVariableA(name, NULL);
    return 0;
}

int clearenv(void)
{
    char **e = _environ;
    char name[1024];
    while (e && *e)
    {
        const char *eq = strchr(*e, '=');
        size_t l = eq ? (size_t)(eq - *e) : strlen(*e);
        if (l == 0 || l >= sizeof(name))
        {
            ++e;
            continue;
        }
        memcpy(name, *e, l);
        name[l] = 0;
        unsetenv(name);
        e = _environ;
    }
    return 0;
}

char *ols_basename(char *path)
{
    char *p;
    size_t l;
    if (!path || !*path)
        return (char *)".";
    l = strlen(path);
    while (l > 1 && (path[l - 1] == '/' || path[l - 1] == '\\'))
        path[--l] = 0;
    p = path + l;
    while (p > path && p[-1] != '/' && p[-1] != '\\')
        --p;
    return p;
}

char *ols_dirname(char *path)
{
    size_t l;
    if (!path || !*path)
        return (char *)".";
    l = strlen(path);
    while (l > 1 && (path[l - 1] == '/' || path[l - 1] == '\\'))
        --l;
    while (l > 0 && path[l - 1] != '/' && path[l - 1] != '\\')
        --l;
    if (l == 0)
        return (char *)".";
    while (l > 1 && (path[l - 1] == '/' || path[l - 1] == '\\'))
        --l;
    path[l] = 0;
    return path;
}

static unsigned int g_rand_state = 1;

long random(void)
{
    g_rand_state = g_rand_state * 1103515245u + 12345u;
    return (long)((g_rand_state >> 1) & 0x7fffffff);
}

void srandom(unsigned int seed)
{
    g_rand_state = seed;
}

int rand_r(unsigned int *seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return (int)((*seed >> 16) & 0x7fff);
}

FILE *ols_fdopen(int fd, const char *mode)
{
    HANDLE d;
    int crt, flags = _O_BINARY;
    FILE *f;
    if (!DuplicateHandle(GetCurrentProcess(), ols_handle_from_fd(fd), GetCurrentProcess(), &d,
                         0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        ols_set_errno_win();
        return NULL;
    }
    if (strchr(mode, 'a'))
        flags |= _O_APPEND;
    if (!strchr(mode, '+') && strchr(mode, 'r'))
        flags |= _O_RDONLY;
    crt = _open_osfhandle((intptr_t)d, flags);
    if (crt < 0)
    {
        CloseHandle(d);
        errno = EMFILE;
        return NULL;
    }
    f = _fdopen(crt, mode);
    if (!f)
    {
        _close(crt);
        return NULL;
    }
    close(fd);
    return f;
}

int posix_memalign(void **ptr, size_t align, size_t size)
{
    void *p = _aligned_malloc(size ? size : 1, align);
    if (!p)
        return ENOMEM;
    *ptr = p;
    return 0;
}

void *memalign(size_t align, size_t size)
{
    return _aligned_malloc(size ? size : 1, align);
}

void *ols_aligned_alloc(size_t align, size_t size)
{
    return _aligned_malloc(size ? size : 1, align);
}

static int fnm_class(const char **pp, int c, int flags)
{
    const char *p = *pp;
    int neg = 0, ok = 0, first = 1;
    if (*p == '!' || *p == '^')
    {
        neg = 1;
        ++p;
    }
    while (*p && (first || *p != ']'))
    {
        int lo = (unsigned char)*p, hi;
        first = 0;
        if (lo == '\\' && !(flags & FNM_NOESCAPE) && p[1])
            lo = (unsigned char)*++p;
        ++p;
        hi = lo;
        if (*p == '-' && p[1] && p[1] != ']')
        {
            hi = (unsigned char)p[1];
            if (hi == '\\' && !(flags & FNM_NOESCAPE) && p[2])
            {
                hi = (unsigned char)p[2];
                ++p;
            }
            p += 2;
        }
        if (flags & FNM_CASEFOLD)
        {
            int lc = tolower(c);
            if ((lc >= tolower(lo) && lc <= tolower(hi)) || (c >= lo && c <= hi))
                ok = 1;
        }
        else if (c >= lo && c <= hi)
            ok = 1;
    }
    if (*p != ']')
        return -1;
    *pp = p + 1;
    return ok ^ neg;
}

static int fnm(const char *p, const char *s, int flags, const char *start)
{
    for (;;)
    {
        int c = (unsigned char)*p++;
        switch (c)
        {
        case 0:
            if ((flags & FNM_LEADING_DIR) && *s == '/')
                return 0;
            return *s ? FNM_NOMATCH : 0;
        case '?':
            if (!*s)
                return FNM_NOMATCH;
            if (*s == '/' && (flags & FNM_PATHNAME))
                return FNM_NOMATCH;
            if (*s == '.' && (flags & FNM_PERIOD)
                && (s == start || ((flags & FNM_PATHNAME) && s[-1] == '/')))
                return FNM_NOMATCH;
            ++s;
            break;
        case '*':
            while (*p == '*')
                ++p;
            if (*s == '.' && (flags & FNM_PERIOD)
                && (s == start || ((flags & FNM_PATHNAME) && s[-1] == '/')))
                return FNM_NOMATCH;
            if (!*p)
            {
                if (flags & FNM_PATHNAME)
                    return ((flags & FNM_LEADING_DIR) || !strchr(s, '/')) ? 0 : FNM_NOMATCH;
                return 0;
            }
            if (*p == '/' && (flags & FNM_PATHNAME))
            {
                s = strchr(s, '/');
                if (!s)
                    return FNM_NOMATCH;
                break;
            }
            for (; *s; ++s)
            {
                if (fnm(p, s, flags & ~FNM_PERIOD, start) == 0)
                    return 0;
                if (*s == '/' && (flags & FNM_PATHNAME))
                    break;
            }
            return fnm(p, s, flags & ~FNM_PERIOD, start) == 0 ? 0 : FNM_NOMATCH;
        case '[':
        {
            int r;
            if (!*s)
                return FNM_NOMATCH;
            if (*s == '/' && (flags & FNM_PATHNAME))
                return FNM_NOMATCH;
            r = fnm_class(&p, (unsigned char)*s, flags);
            if (r < 0)
            {
                if (*s != '[')
                    return FNM_NOMATCH;
            }
            else if (!r)
                return FNM_NOMATCH;
            ++s;
            break;
        }
        case '\\':
            if (!(flags & FNM_NOESCAPE))
            {
                c = (unsigned char)*p++;
                if (!c)
                {
                    c = '\\';
                    --p;
                }
            }
        default:
            if ((flags & FNM_CASEFOLD) ? tolower(c) != tolower((unsigned char)*s) : c != (unsigned char)*s)
                return FNM_NOMATCH;
            ++s;
            break;
        }
    }
}

int fnmatch(const char *pattern, const char *string, int flags)
{
    return fnm(pattern, string, flags, string);
}

char *optarg;
int optind = 1, opterr = 1, optopt;
static int g_optpos;

int getopt(int argc, char *const argv[], const char *optstring)
{
    const char *arg, *o;
    if (optind >= argc || !argv[optind])
        return -1;
    arg = argv[optind];
    if (g_optpos == 0)
    {
        if (arg[0] != '-' || arg[1] == 0)
            return -1;
        if (strcmp(arg, "--") == 0)
        {
            ++optind;
            return -1;
        }
        g_optpos = 1;
    }
    optopt = (unsigned char)arg[g_optpos++];
    o = strchr(optstring, optopt);
    if (!o || optopt == ':')
    {
        if (!arg[g_optpos])
        {
            ++optind;
            g_optpos = 0;
        }
        if (opterr && *optstring != ':')
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], optopt);
        return '?';
    }
    if (o[1] == ':')
    {
        if (arg[g_optpos])
            optarg = (char *)arg + g_optpos;
        else if (o[2] == ':')
            optarg = NULL;
        else if (++optind < argc)
            optarg = argv[optind];
        else
        {
            g_optpos = 0;
            if (opterr && *optstring != ':')
                fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], optopt);
            return *optstring == ':' ? ':' : '?';
        }
        ++optind;
        g_optpos = 0;
    }
    else if (!arg[g_optpos])
    {
        ++optind;
        g_optpos = 0;
    }
    return optopt;
}

int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex)
{
    const char *arg;
    int i;
    if (optind >= argc || !argv[optind])
        return -1;
    arg = argv[optind];
    if (g_optpos == 0 && arg[0] == '-' && arg[1] == '-' && arg[2])
    {
        const char *name = arg + 2, *eq = strchr(name, '=');
        size_t nl = eq ? (size_t)(eq - name) : strlen(name);
        for (i = 0; longopts && longopts[i].name; ++i)
        {
            if (strlen(longopts[i].name) != nl || strncmp(longopts[i].name, name, nl) != 0)
                continue;
            ++optind;
            optarg = NULL;
            if (longopts[i].has_arg != no_argument)
            {
                if (eq)
                    optarg = (char *)eq + 1;
                else if (longopts[i].has_arg == required_argument)
                {
                    if (optind >= argc)
                        return '?';
                    optarg = argv[optind++];
                }
            }
            if (longindex)
                *longindex = i;
            if (longopts[i].flag)
            {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }
        ++optind;
        return '?';
    }
    return getopt(argc, argv, optstring);
}


int vdprintf(int fd, const char *fmt, va_list ap)
{
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < 0)
        return n;
    if (n >= (int)sizeof(buf))
        n = sizeof(buf) - 1;
    return (int)write(fd, buf, (size_t)n);
}

int dprintf(int fd, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vdprintf(fd, fmt, ap);
    va_end(ap);
    return n;
}
