#include "compat_internal.h"
#include <openssl/sha.h>
#include "phpdetect.h"

#define PHP_INDEX L"https://windows.php.net/downloads/releases/releases.json"
#define PHP_BASE L"https://windows.php.net/downloads/releases/"
#define NODE_INDEX L"https://nodejs.org/dist/index.json"
#define NODE_BASE L"https://nodejs.org/dist/"
#define COMPOSER_PHAR L"https://getcomposer.org/download/latest-stable/composer.phar"
#define COMPOSER_SUM L"https://getcomposer.org/download/latest-stable/composer.phar.sha256"
#define PHP_BEGIN "# php begin"
#define LONGP 32768
#define MAX_NODE 32

void ols_jstr(const char *s)
{
    putchar('"');
    for (; s && *s; ++s)
    {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')
            printf("\\%c", c);
        else if (c < 0x20)
            printf("\\u%04x", c);
        else
            putchar(c);
    }
    putchar('"');
}

void ols_jwstr(const wchar_t *w)
{
    char buf[MAX_PATH * 3];
    if (!w || WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL) <= 0)
        buf[0] = 0;
    ols_jstr(buf);
}

int ols_path_search(const wchar_t *exe, wchar_t *out)
{
    DWORD n = GetEnvironmentVariableW(L"PATH", NULL, 0);
    wchar_t *path, *tok, *ctx = NULL, cand[MAX_PATH];
    int found = 0;
    out[0] = 0;
    if (n == 0 || n > 65536 || (path = (wchar_t *)malloc(n * sizeof(wchar_t))) == NULL)
        return -1;
    GetEnvironmentVariableW(L"PATH", path, n);
    for (tok = wcstok(path, L";", &ctx); tok && !found; tok = wcstok(NULL, L";", &ctx))
    {
        size_t l;
        DWORD a;
        if (*tok == L'"')
            ++tok;
        l = wcslen(tok);
        while (l && (tok[l - 1] == L'"' || tok[l - 1] == L'\\' || tok[l - 1] == L' '))
            tok[--l] = 0;
        if (l < 3 || !((tok[1] == L':' && tok[2] == L'\\') || (tok[0] == L'\\' && tok[1] == L'\\'))
            || l + wcslen(exe) + 2 >= MAX_PATH)
            continue;
        swprintf(cand, MAX_PATH, L"%s\\%s", tok, exe);
        a = GetFileAttributesW(cand);
        if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY) && !wcsstr(cand, L"\\WindowsApps\\"))
        {
            wcscpy(out, cand);
            found = 1;
        }
    }
    free(path);
    return found ? 0 : -1;
}

static int rt_root(wchar_t *root)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    wchar_t *p;
    if (n == 0 || n >= MAX_PATH)
        return -1;
    for (p = root; *p; ++p)
        if (*p == L'/')
            *p = L'\\';
    while (n > 3 && root[n - 1] == L'\\')
        root[--n] = 0;
    return 0;
}

static int rt_exists(const wchar_t *p)
{
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static int rt_is_dir(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}

static void rt_fwd(const wchar_t *in, char *out, size_t n)
{
    char *q;
    if (WideCharToMultiByte(CP_UTF8, 0, in, -1, out, (int)n, NULL, NULL) <= 0)
        out[0] = 0;
    for (q = out; *q; ++q)
        if (*q == '\\')
            *q = '/';
}

static int rt_exec(const wchar_t *exe, wchar_t *cmd, DWORD timeout)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    if (WaitForSingleObject(pi.hProcess, timeout) == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
    }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

int ols_download(const wchar_t *url, const wchar_t *file, long long max, const char *what)
{
    wchar_t sys[MAX_PATH], exe[MAX_PATH], cmd[1400 + MAX_PATH * 2];
    const wchar_t *p;
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0 || wcslen(url) > 1024 || wcsncmp(url, L"https://", 8) != 0)
        return 1;
    for (p = url; *p; ++p)
        if (*p <= L' ' || *p >= 0x7f || wcschr(L"\"<>\\^`{|}", *p))
            return 1;
    swprintf(exe, MAX_PATH, L"%s\\curl.exe", sys);
    swprintf(cmd, sizeof(cmd) / sizeof(cmd[0]),
             L"\"%s\" -fsS --proto =https --proto-redir =https --tlsv1.2 --connect-timeout 20 --max-time 1800 "
             L"--max-filesize %lld -L -o \"%s\" %s",
             exe, max, file, url);
    if (what)
    {
        printf("downloading %s...\n", what);
        fflush(stdout);
    }
    DeleteFileW(file);
    if (rt_exec(exe, cmd, 1900000) != 0)
    {
        DeleteFileW(file);
        if (what)
            fprintf(stderr, "could not download %s\n", what);
        return 1;
    }
    return 0;
}

int ols_sha256_handle(HANDLE h, char hex[65], long long *size)
{
    SHA256_CTX c;
    static unsigned char buf[1 << 16];
    unsigned char md[SHA256_DIGEST_LENGTH];
    DWORD got;
    LARGE_INTEGER sz, zero;
    int i;
    zero.QuadPart = 0;
    if (h == INVALID_HANDLE_VALUE || !GetFileSizeEx(h, &sz) || !SetFilePointerEx(h, zero, NULL, FILE_BEGIN))
        return -1;
    *size = sz.QuadPart;
    SHA256_Init(&c);
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got)
        SHA256_Update(&c, buf, got);
    SHA256_Final(md, &c);
    for (i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        snprintf(hex + i * 2, 3, "%02x", md[i]);
    return 0;
}

HANDLE ols_open_verified(const wchar_t *file, const char *sha, long long size)
{
    HANDLE h = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    char hex[65];
    long long got = 0;
    if (h == INVALID_HANDLE_VALUE)
        return h;
    if (ols_sha256_handle(h, hex, &got) != 0 || (size >= 0 && got != size) || _stricmp(hex, sha) != 0)
    {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    return h;
}

int ols_unpack(const wchar_t *zip, const wchar_t *dir, int strip)
{
    wchar_t sys[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH * 4];
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return 1;
    swprintf(exe, MAX_PATH, L"%s\\tar.exe", sys);
    CreateDirectoryW(dir, NULL);
    swprintf(cmd, MAX_PATH * 4, L"\"%s\" -xf \"%s\" -C \"%s\"%s", exe, zip, dir,
             strip ? L" --strip-components=1" : L"");
    return rt_exec(exe, cmd, 900000) == 0 ? 0 : 1;
}

int ols_temp_dir(const wchar_t *root, wchar_t *out)
{
    swprintf(out, MAX_PATH, L"%s\\tmp", root);
    CreateDirectoryW(out, NULL);
    swprintf(out, MAX_PATH, L"%s\\tmp\\dl-%08lx%08lx", root, GetCurrentProcessId(), GetTickCount());
    return ols_private_dir(out);
}

static int rm_tree(wchar_t *path, size_t len)
{
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int r = 0;
    if (len + 3 >= LONGP)
        return -1;
    wcscpy(path + len, L"\\*");
    f = FindFirstFileW(path, &fd);
    path[len] = 0;
    if (f != INVALID_HANDLE_VALUE)
    {
        do
        {
            size_t n = wcslen(fd.cFileName);
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            if (len + 1 + n >= LONGP)
            {
                r = -1;
                continue;
            }
            path[len] = L'\\';
            wcscpy(path + len + 1, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                SetFileAttributesW(path, fd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            {
                if (rm_tree(path, len + 1 + n) != 0)
                    r = -1;
            }
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!RemoveDirectoryW(path))
                    r = -1;
            }
            else if (!DeleteFileW(path))
                r = -1;
            path[len] = 0;
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    if (!RemoveDirectoryW(path))
        r = -1;
    return r;
}

int ols_remove_tree(const wchar_t *dir)
{
    wchar_t *path;
    DWORD a = GetFileAttributesW(dir);
    int r;
    if (a == INVALID_FILE_ATTRIBUTES)
        return 0;
    if (!(a & FILE_ATTRIBUTE_DIRECTORY) || (a & FILE_ATTRIBUTE_REPARSE_POINT))
        return -1;
    if ((path = (wchar_t *)malloc(LONGP * sizeof(wchar_t))) == NULL)
        return -1;
    if (wcsncmp(dir, L"\\\\", 2) == 0)
        wcscpy(path, dir);
    else
        swprintf(path, LONGP, L"\\\\?\\%s", dir);
    r = rm_tree(path, wcslen(path));
    free(path);
    return r;
}

static int rt_retire(const wchar_t *root, const wchar_t *dir)
{
    wchar_t gone[MAX_PATH];
    swprintf(gone, MAX_PATH, L"%s\\tmp", root);
    CreateDirectoryW(gone, NULL);
    swprintf(gone, MAX_PATH, L"%s\\tmp\\del-%08lx%08lx", root, GetCurrentProcessId(), GetTickCount());
    if (!MoveFileExW(dir, gone, 0))
        return -1;
    if (ols_remove_tree(gone) != 0)
        fprintf(stderr, "warning: %ls could not be removed completely\n", gone);
    return 0;
}

static int rt_clear(const wchar_t *root, const wchar_t *dest, const wchar_t *marker)
{
    wchar_t m[MAX_PATH];
    if (GetFileAttributesW(dest) == INVALID_FILE_ATTRIBUTES)
        return 0;
    swprintf(m, MAX_PATH, L"%s\\%s", dest, marker);
    if (rt_exists(m))
        return 0;
    return rt_retire(root, dest);
}

static int rt_read(const wchar_t *path, char **out, size_t *len, size_t cap)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    DWORD got = 0;
    *out = NULL;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > (long long)cap || (*out = (char *)malloc((size_t)sz.QuadPart + 1)) == NULL
        || !ReadFile(h, *out, (DWORD)sz.QuadPart, &got, NULL) || got != (DWORD)sz.QuadPart)
    {
        CloseHandle(h);
        free(*out);
        *out = NULL;
        return -1;
    }
    CloseHandle(h);
    (*out)[got] = 0;
    *len = got;
    return 0;
}

static ols_json *fetch_json(const wchar_t *dl, const wchar_t *url, const wchar_t *name, size_t cap)
{
    wchar_t file[MAX_PATH];
    char *t = NULL;
    size_t n;
    ols_json *j = NULL;
    swprintf(file, MAX_PATH, L"%s\\%s", dl, name);
    if (ols_download(url, file, (long long)cap, NULL) == 0 && rt_read(file, &t, &n, cap) == 0)
        j = ols_json_parse(t, n);
    free(t);
    DeleteFileW(file);
    return j;
}

static int safe_name(const char *s, size_t max)
{
    size_t n = s ? strlen(s) : 0;
    if (n == 0 || n > max || strstr(s, ".."))
        return 0;
    for (; *s; ++s)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '.'
              || *s == '-' || *s == '_'))
            return 0;
    return 1;
}

static int is_hex64(const char *s)
{
    int i;
    if (!s || strlen(s) != 64)
        return 0;
    for (i = 0; i < 64; ++i)
        if (!isxdigit((unsigned char)s[i]))
            return 0;
    return 1;
}

static int is_version(const char *s, int parts)
{
    int n = 0, digits = 0;
    if (!s || !*s || strlen(s) > 20)
        return 0;
    for (; *s; ++s)
    {
        if (*s >= '0' && *s <= '9')
            ++digits;
        else if (*s == '.' && digits)
        {
            ++n;
            digits = 0;
        }
        else
            return 0;
    }
    return digits && n == parts - 1;
}

int ols_php_current(const wchar_t *root, wchar_t *dir)
{
    wchar_t conf[MAX_PATH], *w;
    char *t = NULL, *p, *e;
    size_t n;
    dir[0] = 0;
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (rt_read(conf, &t, &n, 8u << 20) != 0)
        return -1;
    if ((p = strstr(t, PHP_BEGIN)) != NULL && (p = strstr(p, "PHPRC=")) != NULL)
    {
        p += 6;
        for (e = p; *e && *e != '\r' && *e != '\n' && *e != '"'; ++e)
            ;
        *e = 0;
        MultiByteToWideChar(CP_UTF8, 0, p, -1, dir, MAX_PATH);
    }
    free(t);
    for (w = dir; *w; ++w)
        if (*w == L'/')
            *w = L'\\';
    return dir[0] ? 0 : -1;
}

static void php_detect_all(const wchar_t *root)
{
    wchar_t laragon[MAX_PATH], xampp[MAX_PATH], ini[MAX_PATH], extra[MAX_PATH], cur[MAX_PATH];
    if (!phpd_find_laragon(laragon))
        laragon[0] = 0;
    if (!phpd_find_xampp(xampp))
        xampp[0] = 0;
    phpd_detect(laragon, xampp, root);
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (GetPrivateProfileStringW(L"php", L"extra", L"", extra, MAX_PATH, ini) && extra[0])
        phpd_scan(extra, L"*", L"Custom", root);
    if (ols_php_current(root, cur) == 0)
        phpd_add(cur, L"Current", root);
}

int ols_php_known(const wchar_t *root, const wchar_t *dir, wchar_t *full)
{
    int i;
    size_t n;
    if (!GetFullPathNameW(dir, MAX_PATH, full, NULL))
        return 0;
    n = wcslen(full);
    while (n > 3 && (full[n - 1] == L'\\' || full[n - 1] == L'/'))
        full[--n] = 0;
    php_detect_all(root);
    for (i = 0; i < phpd_count; ++i)
        if (_wcsicmp(phpd_list[i].dir, full) == 0)
            return 1;
    return 0;
}

typedef struct
{
    char **line;
    int n, cap;
    const char *eol;
} inidoc_t;

static void doc_free(inidoc_t *d)
{
    int i;
    for (i = 0; i < d->n; ++i)
        free(d->line[i]);
    free(d->line);
    memset(d, 0, sizeof(*d));
}

static int doc_insert(inidoc_t *d, int at, const char *s)
{
    char *c;
    if (d->n == d->cap)
    {
        int cap = d->cap ? d->cap * 2 : 256;
        char **g = (char **)realloc(d->line, (size_t)cap * sizeof(char *));
        if (!g)
            return -1;
        d->line = g;
        d->cap = cap;
    }
    if ((c = _strdup(s)) == NULL)
        return -1;
    memmove(d->line + at + 1, d->line + at, (size_t)(d->n - at) * sizeof(char *));
    d->line[at] = c;
    ++d->n;
    return 0;
}

static int doc_set(inidoc_t *d, int i, const char *s)
{
    char *c = _strdup(s);
    if (!c)
        return -1;
    free(d->line[i]);
    d->line[i] = c;
    return 0;
}

static int doc_load(const wchar_t *path, inidoc_t *d)
{
    char *t = NULL, *p, *e;
    size_t n;
    memset(d, 0, sizeof(*d));
    if (rt_read(path, &t, &n, 4u << 20) != 0)
        return -1;
    d->eol = strstr(t, "\r\n") ? "\r\n" : "\n";
    for (p = t; *p; p = e)
    {
        char save;
        size_t l;
        e = strchr(p, '\n');
        if (!e)
            e = p + strlen(p);
        l = (size_t)(e - p);
        if (l && p[l - 1] == '\r')
            --l;
        save = p[l];
        p[l] = 0;
        if (doc_insert(d, d->n, p) != 0)
        {
            free(t);
            doc_free(d);
            return -1;
        }
        p[l] = save;
        if (*e == '\n')
            ++e;
    }
    free(t);
    return 0;
}

static int doc_save(const wchar_t *path, const inidoc_t *d)
{
    size_t total = 0, el = strlen(d->eol), off = 0;
    char *buf;
    int i, r;
    for (i = 0; i < d->n; ++i)
        total += strlen(d->line[i]) + el;
    if ((buf = (char *)malloc(total + 1)) == NULL)
        return -1;
    for (i = 0; i < d->n; ++i)
    {
        size_t l = strlen(d->line[i]);
        memcpy(buf + off, d->line[i], l);
        memcpy(buf + off + l, d->eol, el);
        off += l + el;
    }
    r = ols_write_wfile(path, buf, off, 0);
    free(buf);
    return r;
}

static const char *skip_sp(const char *s)
{
    while (*s == ' ' || *s == '\t')
        ++s;
    return s;
}

static int line_key(const char *line, const char *key, int *commented, const char **value)
{
    const char *s = skip_sp(line);
    size_t kl = strlen(key);
    *commented = 0;
    if (*s == ';')
    {
        *commented = 1;
        ++s;
        if (*s == ' ' || *s == '\t')
            return 0;
    }
    if (_strnicmp(s, key, kl) != 0)
        return 0;
    s = skip_sp(s + kl);
    if (*s != '=')
        return 0;
    *value = skip_sp(s + 1);
    return 1;
}

static void ext_value(const char *v, char *out, size_t n)
{
    size_t i = 0;
    const char *b;
    if (*v == '"')
        ++v;
    b = v;
    while (*v && *v != '"' && *v != ';' && *v != ' ' && *v != '\t')
    {
        if (*v == '/' || *v == '\\')
            b = v + 1;
        ++v;
    }
    if (v - b > 4 && _strnicmp(v - 4, ".dll", 4) == 0)
        v -= 4;
    if (v - b > 4 && _strnicmp(b, "php_", 4) == 0)
        b += 4;
    while (b < v && i + 1 < n)
        out[i++] = (char)tolower((unsigned char)*b++);
    out[i] = 0;
}

static int is_extdir_ext(const char *line)
{
    const char *s = skip_sp(line);
    if (*s == ';')
        s = skip_sp(s + 1);
    if (strncmp(s, "extension_dir", 13) != 0)
        return 0;
    s = skip_sp(s + 13);
    if (*s != '=')
        return 0;
    s = skip_sp(s + 1);
    return strncmp(s, "\"ext\"", 5) == 0 && skip_sp(s + 5)[0] == 0;
}

static int is_zend(const char *name)
{
    return strcmp(name, "opcache") == 0 || strncmp(name, "xdebug", 6) == 0;
}

static int php_make_ini(const wchar_t *dir)
{
    static const char *const exts[] = { "bz2", "curl", "fileinfo", "gd", "gettext", "intl", "mbstring", "exif",
                                        "mysqli", "openssl", "pdo_mysql", "pdo_sqlite", "sqlite3", "sodium", "zip" };
    wchar_t ini[MAX_PATH], src[MAX_PATH], dll[MAX_PATH], wn[64];
    char fdir[MAX_PATH * 3], line[MAX_PATH * 3 + 32];
    inidoc_t d;
    int i, j, c;
    const char *v;
    swprintf(ini, MAX_PATH, L"%s\\php.ini", dir);
    if (rt_exists(ini))
        return 0;
    swprintf(src, MAX_PATH, L"%s\\php.ini-production", dir);
    if (!rt_exists(src))
        swprintf(src, MAX_PATH, L"%s\\php.ini-development", dir);
    if (doc_load(src, &d) != 0)
        return -1;
    rt_fwd(dir, fdir, sizeof(fdir));
    for (i = 0; i < d.n; ++i)
    {
        if (is_extdir_ext(d.line[i]))
        {
            snprintf(line, sizeof(line), "extension_dir = \"%s/ext\"", fdir);
            doc_set(&d, i, line);
        }
        else if (line_key(d.line[i], "extension", &c, &v) && c)
        {
            char name[64];
            ext_value(v, name, sizeof(name));
            for (j = 0; j < (int)(sizeof(exts) / sizeof(exts[0])); ++j)
                if (strcmp(name, exts[j]) == 0)
                {
                    MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 64);
                    swprintf(dll, MAX_PATH, L"%s\\ext\\php_%s.dll", dir, wn);
                    if (rt_exists(dll))
                    {
                        snprintf(line, sizeof(line), "extension=%s", name);
                        doc_set(&d, i, line);
                    }
                }
        }
        else if (line_key(d.line[i], "upload_max_filesize", &c, &v) && !c)
            doc_set(&d, i, "upload_max_filesize = 64M");
        else if (line_key(d.line[i], "post_max_size", &c, &v) && !c)
            doc_set(&d, i, "post_max_size = 64M");
        else if (line_key(d.line[i], "memory_limit", &c, &v) && !c)
            doc_set(&d, i, "memory_limit = 256M");
    }
    i = doc_save(ini, &d);
    doc_free(&d);
    return i;
}

static int php_dir_arg(const wchar_t *root, const char *arg, wchar_t *full)
{
    wchar_t w[MAX_PATH];
    if (!arg || MultiByteToWideChar(CP_UTF8, 0, arg, -1, w, MAX_PATH) <= 0 || !ols_php_known(root, w, full))
    {
        fprintf(stderr, "that PHP folder is not one OpenLiteSpeed found on this computer\n");
        return -1;
    }
    return 0;
}

static int php_ext_list(const wchar_t *root, const char *arg)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH], ini[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    inidoc_t d;
    char names[160][48], on[160];
    int n = 0, i, j, c, have;
    const char *v;
    if (php_dir_arg(root, arg, dir) != 0)
        return 2;
    swprintf(pat, MAX_PATH, L"%s\\ext\\php_*.dll", dir);
    f = FindFirstFileW(pat, &fd);
    if (f != INVALID_HANDLE_VALUE)
    {
        do
        {
            char mb[MAX_PATH];
            size_t l;
            if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName + 4, -1, mb, sizeof(mb), NULL, NULL) <= 0)
                continue;
            l = strlen(mb);
            if (l <= 4 || l - 4 >= sizeof(names[0]) || n >= 160)
                continue;
            mb[l - 4] = 0;
            for (i = 0; mb[i]; ++i)
                mb[i] = (char)tolower((unsigned char)mb[i]);
            if (!safe_name(mb, 40) || strcmp(mb, "dl_test") == 0 || strcmp(mb, "zend_test") == 0)
                continue;
            strcpy(names[n], mb);
            on[n++] = 0;
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    swprintf(ini, MAX_PATH, L"%s\\php.ini", dir);
    have = doc_load(ini, &d) == 0;
    for (i = 0; have && i < d.n; ++i)
    {
        char name[64];
        if ((line_key(d.line[i], "extension", &c, &v) || line_key(d.line[i], "zend_extension", &c, &v)) && !c)
        {
            ext_value(v, name, sizeof(name));
            for (j = 0; j < n; ++j)
                if (strcmp(names[j], name) == 0)
                    on[j] = 1;
        }
    }
    if (have)
        doc_free(&d);
    printf("{\"ini\":%s,\"extensions\":[", have ? "true" : "false");
    for (i = 0; i < n; ++i)
    {
        printf("%s{\"name\":", i ? "," : "");
        ols_jstr(names[i]);
        printf(",\"on\":%s}", on[i] ? "true" : "false");
    }
    printf("]}\n");
    return 0;
}

static int php_ext_set(const wchar_t *root, const char *arg, const char *name, int enable)
{
    wchar_t dir[MAX_PATH], ini[MAX_PATH], dll[MAX_PATH], wn[64];
    inidoc_t d;
    int i, c, done = 0, first = -1, zend;
    const char *v;
    char line[96], lower[48];
    if (php_dir_arg(root, arg, dir) != 0)
        return 2;
    if (!safe_name(name, 40) || strchr(name, '.'))
    {
        fprintf(stderr, "unknown extension\n");
        return 2;
    }
    for (i = 0; name[i] && i < 47; ++i)
        lower[i] = (char)tolower((unsigned char)name[i]);
    lower[i] = 0;
    MultiByteToWideChar(CP_UTF8, 0, lower, -1, wn, 64);
    swprintf(dll, MAX_PATH, L"%s\\ext\\php_%s.dll", dir, wn);
    if (!rt_exists(dll))
    {
        fprintf(stderr, "this PHP has no %s extension\n", lower);
        return 2;
    }
    if (php_make_ini(dir) != 0)
    {
        fprintf(stderr, "cannot create php.ini\n");
        return 1;
    }
    swprintf(ini, MAX_PATH, L"%s\\php.ini", dir);
    if (doc_load(ini, &d) != 0)
    {
        fprintf(stderr, "cannot read php.ini\n");
        return 1;
    }
    zend = is_zend(lower);
    for (i = 0; i < d.n; ++i)
    {
        char cur[64];
        if (!line_key(d.line[i], "extension", &c, &v) && !line_key(d.line[i], "zend_extension", &c, &v))
            continue;
        ext_value(v, cur, sizeof(cur));
        if (strcmp(cur, lower) != 0)
            continue;
        if (!c && !enable)
        {
            snprintf(line, sizeof(line), ";%s", skip_sp(d.line[i]));
            doc_set(&d, i, line);
        }
        else if (!c)
            done = 1;
        else if (first < 0)
            first = i;
    }
    if (enable && !done)
    {
        snprintf(line, sizeof(line), "%s=%s", zend ? "zend_extension" : "extension", lower);
        if (first >= 0)
            doc_set(&d, first, line);
        else
            doc_insert(&d, d.n, line);
    }
    for (i = 0; enable && i < d.n; ++i)
        if (is_extdir_ext(d.line[i]))
        {
            char fdir[MAX_PATH * 3], l2[MAX_PATH * 3 + 32];
            rt_fwd(dir, fdir, sizeof(fdir));
            snprintf(l2, sizeof(l2), "extension_dir = \"%s/ext\"", fdir);
            doc_set(&d, i, l2);
            break;
        }
    i = doc_save(ini, &d);
    doc_free(&d);
    if (i != 0)
    {
        fprintf(stderr, "cannot write php.ini\n");
        return 1;
    }
    printf("%s %s\n", lower, enable ? "enabled" : "disabled");
    return 0;
}

static const char *const s_ini_keys[] = { "memory_limit",    "upload_max_filesize", "post_max_size", "max_execution_time",
                                          "max_input_vars", "display_errors",      "date.timezone" };
#define INI_KEYS ((int)(sizeof(s_ini_keys) / sizeof(s_ini_keys[0])))

static int ini_value_ok(int k, const char *v)
{
    size_t n = strlen(v), i;
    if (n == 0 || n > 64)
        return 0;
    if (k <= 2)
    {
        if (strcmp(v, "-1") == 0)
            return k == 0;
        for (i = 0; i < n; ++i)
            if (!isdigit((unsigned char)v[i]) && !(i == n - 1 && i > 0 && strchr("KMG", v[i])))
                return 0;
        return n <= 7;
    }
    if (k <= 4)
    {
        for (i = 0; i < n; ++i)
            if (!isdigit((unsigned char)v[i]))
                return 0;
        return n <= 7;
    }
    if (k == 5)
        return strcmp(v, "On") == 0 || strcmp(v, "Off") == 0;
    for (i = 0; i < n; ++i)
        if (!isalnum((unsigned char)v[i]) && !strchr("/_+-", v[i]))
            return 0;
    return isalpha((unsigned char)v[0]) && !strstr(v, "//");
}

static void ini_clean(const char *v, char *out, size_t n)
{
    size_t i = 0;
    int q = *v == '"';
    if (q)
        ++v;
    while (*v && i + 1 < n && (q ? *v != '"' : (*v != ';' && *v != ' ' && *v != '\t')))
        out[i++] = *v++;
    out[i] = 0;
}

static int php_ini_get(const wchar_t *root, const char *arg)
{
    wchar_t dir[MAX_PATH], ini[MAX_PATH];
    inidoc_t d;
    char vals[INI_KEYS][72];
    int i, k, c, have;
    const char *v;
    if (php_dir_arg(root, arg, dir) != 0)
        return 2;
    memset(vals, 0, sizeof(vals));
    swprintf(ini, MAX_PATH, L"%s\\php.ini", dir);
    have = doc_load(ini, &d) == 0;
    for (i = 0; have && i < d.n; ++i)
        for (k = 0; k < INI_KEYS; ++k)
            if (line_key(d.line[i], s_ini_keys[k], &c, &v) && !c)
                ini_clean(v, vals[k], sizeof(vals[k]));
    if (have)
        doc_free(&d);
    printf("{\"ini\":%s", have ? "true" : "false");
    for (k = 0; k < INI_KEYS; ++k)
    {
        printf(",");
        ols_jstr(s_ini_keys[k]);
        printf(":");
        ols_jstr(vals[k]);
    }
    printf("}\n");
    return 0;
}

static int php_ini_set(const wchar_t *root, const char *arg, const char *key, const char *value)
{
    wchar_t dir[MAX_PATH], ini[MAX_PATH];
    inidoc_t d;
    int i, k, c, last = -1, first = -1;
    const char *v;
    char line[160];
    if (php_dir_arg(root, arg, dir) != 0)
        return 2;
    for (k = 0; k < INI_KEYS && strcmp(s_ini_keys[k], key) != 0; ++k)
        ;
    if (k == INI_KEYS || !value || !ini_value_ok(k, value))
    {
        fprintf(stderr, "that value is not allowed for %s\n", k == INI_KEYS ? "this setting" : key);
        return 2;
    }
    if (php_make_ini(dir) != 0)
    {
        fprintf(stderr, "cannot create php.ini\n");
        return 1;
    }
    swprintf(ini, MAX_PATH, L"%s\\php.ini", dir);
    if (doc_load(ini, &d) != 0)
    {
        fprintf(stderr, "cannot read php.ini\n");
        return 1;
    }
    for (i = 0; i < d.n; ++i)
        if (line_key(d.line[i], key, &c, &v))
        {
            if (!c)
                last = i;
            else if (first < 0)
                first = i;
        }
    snprintf(line, sizeof(line), k == 6 ? "%s = \"%s\"" : "%s = %s", key, value);
    if (last >= 0)
        doc_set(&d, last, line);
    else if (first >= 0)
        doc_set(&d, first, line);
    else
        doc_insert(&d, d.n, line);
    i = doc_save(ini, &d);
    doc_free(&d);
    if (i != 0)
    {
        fprintf(stderr, "cannot write php.ini\n");
        return 1;
    }
    printf("%s = %s\n", key, value);
    return 0;
}

typedef struct
{
    char branch[16], version[24], path[2][128], sha[2][65];
} phprel_t;

static int php_releases(const wchar_t *dl, phprel_t *out, int max)
{
    ols_json *j = fetch_json(dl, PHP_INDEX, L"releases.json", 1u << 20), *b, *e;
    int n = 0, t;
    if (!j)
    {
        fprintf(stderr, "could not read the PHP release list from windows.php.net\n");
        return -1;
    }
    for (b = j->type == OLS_JSON_OBJ ? j->child : NULL; b && n < max; b = b->next)
    {
        const char *ver = ols_json_str(b, "version"), *dash;
        char base[24];
        phprel_t *r = &out[n];
        if (b->type != OLS_JSON_OBJ || !b->key || strlen(b->key) >= sizeof(r->branch) || !is_version(b->key, 2) || !ver
            || strlen(ver) >= sizeof(base) || atof(b->key) < 7.35)
            continue;
        strcpy(base, ver);
        if ((dash = strchr(ver, '-')) != NULL)
        {
            if (!dash[1] || strspn(dash + 1, "0123456789") != strlen(dash + 1))
                continue;
            base[dash - ver] = 0;
        }
        if (!is_version(base, 3))
            continue;
        memset(r, 0, sizeof(*r));
        strcpy(r->branch, b->key);
        strcpy(r->version, base);
        for (e = b->child; e; e = e->next)
        {
            const char *path, *sha;
            size_t kl = e->key ? strlen(e->key) : 0;
            if (e->type != OLS_JSON_OBJ || kl < 8 || strcmp(e->key + kl - 4, "-x64") != 0)
                continue;
            t = strncmp(e->key, "nts-", 4) == 0 ? 0 : strncmp(e->key, "ts-", 3) == 0 ? 1 : -1;
            path = ols_json_str(ols_json_get(e, "zip"), "path");
            sha = ols_json_str(ols_json_get(e, "zip"), "sha256");
            if (t < 0 || !safe_name(path, 120) || strncmp(path, "php-", 4) != 0
                || _stricmp(path + strlen(path) - 4, ".zip") != 0 || !is_hex64(sha))
                continue;
            strcpy(r->path[t], path);
            strcpy(r->sha[t], sha);
        }
        if (r->path[0][0] || r->path[1][0])
            ++n;
    }
    ols_json_free(j);
    return n;
}

static void php_dest(const wchar_t *root, const char *version, int ts, wchar_t *out)
{
    wchar_t wv[24];
    MultiByteToWideChar(CP_UTF8, 0, version, -1, wv, 24);
    swprintf(out, MAX_PATH, L"%s\\php\\%s%s", root, wv, ts ? L"-ts" : L"");
}

static int php_available(const wchar_t *root)
{
    wchar_t dl[MAX_PATH], dest[MAX_PATH], cgi[MAX_PATH], ls[MAX_PATH];
    phprel_t rel[16];
    int n, i, t, first = 1;
    if (ols_temp_dir(root, dl) != 0)
        return 1;
    n = php_releases(dl, rel, 16);
    RemoveDirectoryW(dl);
    if (n < 0)
        return 1;
    printf("[");
    for (i = 0; i < n; ++i)
        for (t = 0; t < 2; ++t)
        {
            wchar_t wb[16] = L"";
            if (!rel[i].path[t][0])
                continue;
            php_dest(root, rel[i].version, t, dest);
            swprintf(cgi, MAX_PATH, L"%s\\php-cgi.exe", dest);
            MultiByteToWideChar(CP_UTF8, 0, rel[i].branch, -1, wb, 16);
            swprintf(ls, MAX_PATH, L"%s\\bin\\lsphp\\%s-%s\\lsphp.exe", root, wb, t ? L"ts" : L"nts");
            printf("%s{\"branch\":", first ? "" : ",");
            ols_jstr(rel[i].branch);
            printf(",\"version\":");
            ols_jstr(rel[i].version);
            printf(",\"ts\":%s,\"lsapi\":%s,\"installed\":%s}", t ? "true" : "false", rt_exists(ls) ? "true" : "false",
                   rt_exists(cgi) ? "true" : "false");
            first = 0;
        }
    printf("]\n");
    return 0;
}

static int php_install(const wchar_t *root, const char *branch, int ts)
{
    wchar_t dl[MAX_PATH], dest[MAX_PATH], stage[MAX_PATH], cgi[MAX_PATH], zip[MAX_PATH], url[512], wp[128], base[MAX_PATH];
    phprel_t rel[16];
    HANDLE lock;
    int n, i, r = 1;
    char what[64];
    if (!is_version(branch, 2))
    {
        fprintf(stderr, "use a PHP branch such as 8.4\n");
        return 2;
    }
    swprintf(base, MAX_PATH, L"%s\\php", root);
    CreateDirectoryW(base, NULL);
    if (!rt_is_dir(base))
    {
        fprintf(stderr, "cannot use %ls\n", base);
        return 1;
    }
    if (ols_temp_dir(root, dl) != 0)
    {
        fprintf(stderr, "cannot prepare a private download folder\n");
        return 1;
    }
    n = php_releases(dl, rel, 16);
    for (i = 0; i < n && strcmp(rel[i].branch, branch) != 0; ++i)
        ;
    if (n < 0 || i == n || !rel[i].path[ts][0])
    {
        if (n >= 0)
            fprintf(stderr, "PHP %s %s is not offered by windows.php.net\n", branch, ts ? "TS" : "NTS");
        RemoveDirectoryW(dl);
        return 2;
    }
    php_dest(root, rel[i].version, ts, dest);
    swprintf(cgi, MAX_PATH, L"%s\\php-cgi.exe", dest);
    if (rt_exists(cgi))
    {
        RemoveDirectoryW(dl);
        php_make_ini(dest);
        printf("PHP %s %s is already in %ls\n", rel[i].version, ts ? "TS" : "NTS", dest);
        return 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, rel[i].path[ts], -1, wp, 128);
    swprintf(url, 512, L"%s%s", PHP_BASE, wp);
    swprintf(zip, MAX_PATH, L"%s\\php.zip", dl);
    snprintf(what, sizeof(what), "PHP %s %s", rel[i].version, ts ? "TS" : "NTS");
    swprintf(stage, MAX_PATH, L"%s.part", dest);
    ols_remove_tree(stage);
    if (ols_download(url, zip, 128LL << 20, what) == 0)
    {
        if ((lock = ols_open_verified(zip, rel[i].sha[ts], -1)) == INVALID_HANDLE_VALUE)
            fprintf(stderr, "%s failed the checksum check\n", what);
        else
        {
            if (ols_unpack(zip, stage, 0) != 0)
                fprintf(stderr, "could not unpack %s\n", what);
            else
            {
                swprintf(cgi, MAX_PATH, L"%s\\php-cgi.exe", stage);
                if (!rt_exists(cgi))
                    fprintf(stderr, "the PHP archive was not what we expected\n");
                else if (rt_clear(root, dest, L"php-cgi.exe") != 0 || !MoveFileExW(stage, dest, 0))
                    fprintf(stderr, "cannot move PHP into %ls (error %lu)\n", dest, GetLastError());
                else
                    r = 0;
            }
            CloseHandle(lock);
        }
    }
    DeleteFileW(zip);
    RemoveDirectoryW(dl);
    if (r != 0)
    {
        ols_remove_tree(stage);
        return 1;
    }
    if (php_make_ini(dest) != 0)
        fprintf(stderr, "warning: could not create php.ini\n");
    printf("%s is in %ls\n", what, dest);
    return 0;
}

static int php_in_use(const wchar_t *root, const wchar_t *dir)
{
    wchar_t ini[MAX_PATH], cur[MAX_PATH], sect[16384], *e;
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (ols_php_current(root, cur) == 0 && _wcsicmp(cur, dir) == 0)
        return 1;
    if (GetPrivateProfileSectionW(L"sitephp", sect, 16384, ini) > 0)
        for (e = sect; *e; e += wcslen(e) + 1)
        {
            wchar_t *eq = wcschr(e, L'='), full[MAX_PATH];
            if (eq && GetFullPathNameW(eq + 1, MAX_PATH, full, NULL) && _wcsicmp(full, dir) == 0)
                return 2;
        }
    return 0;
}

static int php_remove(const wchar_t *root, const char *arg)
{
    wchar_t dir[MAX_PATH], base[MAX_PATH], *rest;
    size_t bl;
    int use;
    if (php_dir_arg(root, arg, dir) != 0)
        return 2;
    swprintf(base, MAX_PATH, L"%s\\php\\", root);
    bl = wcslen(base);
    rest = dir + bl;
    if (_wcsnicmp(dir, base, bl) != 0 || !*rest || wcschr(rest, L'\\') || rest[0] == L'.')
    {
        fprintf(stderr, "only PHP versions downloaded by OpenLiteSpeed can be removed here\n");
        return 2;
    }
    if ((use = php_in_use(root, dir)) != 0)
    {
        fprintf(stderr, use == 1 ? "this PHP is the default; choose another default first\n"
                                 : "a site still uses this PHP; switch the site first\n");
        return 2;
    }
    if (rt_retire(root, dir) != 0)
    {
        fprintf(stderr, "this PHP is still in use; reload the web server and try again\n");
        return 1;
    }
    printf("removed %ls\n", dir);
    return 0;
}

typedef struct
{
    wchar_t exe[MAX_PATH];
    wchar_t source[24];
    int major, minor, patch, ours;
} noded_t;

static noded_t s_node[MAX_NODE];
static int s_nnode;

static void node_add(const wchar_t *exe, const wchar_t *source, int ours)
{
    wchar_t full[MAX_PATH];
    noded_t *n;
    int i;
    if (s_nnode >= MAX_NODE || !GetFullPathNameW(exe, MAX_PATH, full, NULL) || !rt_exists(full)
        || wcsstr(full, L"\\WindowsApps\\"))
        return;
    for (i = 0; i < s_nnode; ++i)
        if (_wcsicmp(s_node[i].exe, full) == 0)
            return;
    n = &s_node[s_nnode];
    if (phpd_version(full, &n->major, &n->minor, &n->patch) != 0 || n->major < 10 || !phpd_is_x64(full))
        return;
    wcscpy(n->exe, full);
    wcsncpy(n->source, source, 23);
    n->source[23] = 0;
    n->ours = ours;
    ++s_nnode;
}

static void node_scan(const wchar_t *base, const wchar_t *pattern, const wchar_t *inner, const wchar_t *source, int ours)
{
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    HANDLE f;
    swprintf(pat, MAX_PATH, L"%s\\%s", base, pattern);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.')
            continue;
        swprintf(p, MAX_PATH, L"%s\\%s\\%s", base, fd.cFileName, inner);
        node_add(p, source, ours);
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
}

static int node_cmp(const void *a, const void *b)
{
    const noded_t *x = (const noded_t *)a, *y = (const noded_t *)b;
    if (x->major != y->major)
        return y->major - x->major;
    if (x->minor != y->minor)
        return y->minor - x->minor;
    return y->patch - x->patch;
}

static void node_detect(const wchar_t *root)
{
    wchar_t p[MAX_PATH], env[MAX_PATH], w[MAX_PATH];
    DWORD n;
    s_nnode = 0;
    swprintf(p, MAX_PATH, L"%s\\node", root);
    node_scan(p, L"*", L"node.exe", L"OpenLiteSpeed", 1);
    if (phpd_find_laragon(env))
    {
        swprintf(p, MAX_PATH, L"%s\\bin\\nodejs", env);
        node_scan(p, L"*", L"node.exe", L"Laragon", 0);
    }
    if (GetEnvironmentVariableW(L"ProgramFiles", env, MAX_PATH))
    {
        swprintf(p, MAX_PATH, L"%s\\nodejs\\node.exe", env);
        node_add(p, L"Node.js", 0);
    }
    if (GetEnvironmentVariableW(L"NVM_HOME", env, MAX_PATH))
        node_scan(env, L"v*", L"node.exe", L"nvm", 0);
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", env, MAX_PATH))
    {
        swprintf(p, MAX_PATH, L"%s\\Volta\\tools\\image\\node", env);
        node_scan(p, L"*", L"node.exe", L"Volta", 0);
    }
    (void)n;
    if (ols_path_search(L"node.exe", w) == 0)
        node_add(w, L"PATH", 0);
    qsort(s_node, s_nnode, sizeof(noded_t), node_cmp);
}

int ols_node_default(const wchar_t *root, wchar_t *out, size_t n)
{
    wchar_t ini[MAX_PATH], v[MAX_PATH];
    int i;
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    node_detect(root);
    if (GetPrivateProfileStringW(L"node", L"bin", L"", v, MAX_PATH, ini) && v[0])
        for (i = 0; i < s_nnode; ++i)
            if (_wcsicmp(s_node[i].exe, v) == 0)
            {
                wcsncpy(out, s_node[i].exe, n - 1);
                out[n - 1] = 0;
                return 0;
            }
    for (i = 0; i < s_nnode; ++i)
        if (s_node[i].ours)
        {
            wcsncpy(out, s_node[i].exe, n - 1);
            out[n - 1] = 0;
            return 0;
        }
    if (s_nnode > 0)
    {
        wcsncpy(out, s_node[0].exe, n - 1);
        out[n - 1] = 0;
        return 0;
    }
    out[0] = 0;
    return -1;
}

static int node_list(const wchar_t *root)
{
    wchar_t def[MAX_PATH];
    int i;
    char ver[32];
    ols_node_default(root, def, MAX_PATH);
    printf("[");
    for (i = 0; i < s_nnode; ++i)
    {
        snprintf(ver, sizeof(ver), "%d.%d.%d", s_node[i].major, s_node[i].minor, s_node[i].patch);
        printf("%s{\"exe\":", i ? "," : "");
        ols_jwstr(s_node[i].exe);
        printf(",\"version\":");
        ols_jstr(ver);
        printf(",\"source\":");
        ols_jwstr(s_node[i].source);
        printf(",\"ours\":%s,\"default\":%s}", s_node[i].ours ? "true" : "false",
               _wcsicmp(s_node[i].exe, def) == 0 ? "true" : "false");
    }
    printf("]\n");
    return 0;
}

typedef struct
{
    int major;
    char version[24], lts[32];
} noderel_t;

static int has_str(const ols_json *arr, const char *s)
{
    const ols_json *c;
    if (!arr || arr->type != OLS_JSON_ARR)
        return 0;
    for (c = arr->child; c; c = c->next)
        if (c->type == OLS_JSON_STR && strcmp(c->str, s) == 0)
            return 1;
    return 0;
}

static int node_releases(const wchar_t *dl, noderel_t *out, int max)
{
    ols_json *j = fetch_json(dl, NODE_INDEX, L"index.json", 16u << 20), *e, *lts;
    int n = 0, i, newest = 0, lcount = 0;
    noderel_t all[64];
    int na = 0;
    if (!j || j->type != OLS_JSON_ARR)
    {
        ols_json_free(j);
        fprintf(stderr, "could not read the Node.js release list from nodejs.org\n");
        return -1;
    }
    for (e = j->child; e && na < 64; e = e->next)
    {
        const char *v = ols_json_str(e, "version");
        int major;
        if (!v || v[0] != 'v' || !is_version(v + 1, 3) || !has_str(ols_json_get(e, "files"), "win-x64-zip"))
            continue;
        major = atoi(v + 1);
        for (i = 0; i < na && all[i].major != major; ++i)
            ;
        if (i < na || major < 18)
            continue;
        all[na].major = major;
        strcpy(all[na].version, v + 1);
        lts = ols_json_get(e, "lts");
        all[na].lts[0] = 0;
        if (lts && lts->type == OLS_JSON_STR && safe_name(lts->str, 30))
            strcpy(all[na].lts, lts->str);
        if (major > newest)
            newest = major;
        ++na;
    }
    ols_json_free(j);
    for (i = 0; i < na && n < max; ++i)
        if (all[i].major == newest && !all[i].lts[0])
            out[n++] = all[i];
    for (i = 0; i < na && n < max && lcount < 3; ++i)
        if (all[i].lts[0])
        {
            out[n++] = all[i];
            ++lcount;
        }
    return n;
}

static int node_available(const wchar_t *root)
{
    wchar_t dl[MAX_PATH], exe[MAX_PATH], wv[24];
    noderel_t rel[8];
    int n, i;
    if (ols_temp_dir(root, dl) != 0)
        return 1;
    n = node_releases(dl, rel, 8);
    RemoveDirectoryW(dl);
    if (n < 0)
        return 1;
    printf("[");
    for (i = 0; i < n; ++i)
    {
        MultiByteToWideChar(CP_UTF8, 0, rel[i].version, -1, wv, 24);
        swprintf(exe, MAX_PATH, L"%s\\node\\%s\\node.exe", root, wv);
        printf("%s{\"major\":%d,\"version\":", i ? "," : "", rel[i].major);
        ols_jstr(rel[i].version);
        printf(",\"lts\":");
        ols_jstr(rel[i].lts);
        printf(",\"installed\":%s}", rt_exists(exe) ? "true" : "false");
    }
    printf("]\n");
    return 0;
}

static int sum_lookup(const char *text, const char *file, char *sha)
{
    const char *p = text;
    size_t fl = strlen(file);
    while ((p = strstr(p, file)) != NULL)
    {
        const char *b = p;
        if ((p[fl] == '\n' || p[fl] == '\r' || p[fl] == 0) && b - text >= 66 && b[-1] == ' ' && b[-2] == ' ')
        {
            memcpy(sha, b - 66, 64);
            sha[64] = 0;
            if (is_hex64(sha) && (b - 66 == text || b[-67] == '\n'))
                return 0;
        }
        p += fl;
    }
    return -1;
}

static int node_install(const wchar_t *root, int major)
{
    wchar_t dl[MAX_PATH], base[MAX_PATH], dest[MAX_PATH], stage[MAX_PATH], zip[MAX_PATH], sums[MAX_PATH], url[512],
        wv[24], exe[MAX_PATH], ini[MAX_PATH], cur[MAX_PATH];
    noderel_t rel[8];
    char *t = NULL, file[64], sha[65], what[48];
    size_t tl;
    HANDLE lock;
    int n, i, r = 1;
    swprintf(base, MAX_PATH, L"%s\\node", root);
    CreateDirectoryW(base, NULL);
    if (!rt_is_dir(base) || ols_temp_dir(root, dl) != 0)
    {
        fprintf(stderr, "cannot prepare the Node.js folder\n");
        return 1;
    }
    n = node_releases(dl, rel, 8);
    for (i = 0; i < n && rel[i].major != major; ++i)
        ;
    if (n < 0 || i == n)
    {
        if (n >= 0)
            fprintf(stderr, "Node.js %d is not offered for Windows\n", major);
        RemoveDirectoryW(dl);
        return 2;
    }
    MultiByteToWideChar(CP_UTF8, 0, rel[i].version, -1, wv, 24);
    swprintf(dest, MAX_PATH, L"%s\\%s", base, wv);
    swprintf(exe, MAX_PATH, L"%s\\node.exe", dest);
    snprintf(what, sizeof(what), "Node.js %s", rel[i].version);
    if (rt_exists(exe))
    {
        RemoveDirectoryW(dl);
        printf("%s is already in %ls\n", what, dest);
        return 0;
    }
    swprintf(sums, MAX_PATH, L"%s\\SHASUMS256.txt", dl);
    swprintf(url, 512, L"%sv%s/SHASUMS256.txt", NODE_BASE, wv);
    snprintf(file, sizeof(file), "node-v%s-win-x64.zip", rel[i].version);
    if (ols_download(url, sums, 1 << 20, NULL) != 0 || rt_read(sums, &t, &tl, 1 << 20) != 0 || sum_lookup(t, file, sha) != 0)
    {
        free(t);
        DeleteFileW(sums);
        RemoveDirectoryW(dl);
        fprintf(stderr, "could not read the checksums of %s\n", what);
        return 1;
    }
    free(t);
    DeleteFileW(sums);
    swprintf(url, 512, L"%sv%s/node-v%s-win-x64.zip", NODE_BASE, wv, wv);
    swprintf(zip, MAX_PATH, L"%s\\node.zip", dl);
    swprintf(stage, MAX_PATH, L"%s.part", dest);
    ols_remove_tree(stage);
    if (ols_download(url, zip, 256LL << 20, what) == 0)
    {
        if ((lock = ols_open_verified(zip, sha, -1)) == INVALID_HANDLE_VALUE)
            fprintf(stderr, "%s failed the checksum check\n", what);
        else
        {
            if (ols_unpack(zip, stage, 1) != 0)
                fprintf(stderr, "could not unpack %s\n", what);
            else
            {
                swprintf(exe, MAX_PATH, L"%s\\node.exe", stage);
                if (!rt_exists(exe))
                    fprintf(stderr, "the Node.js archive was not what we expected\n");
                else if (rt_clear(root, dest, L"node.exe") != 0 || !MoveFileExW(stage, dest, 0))
                    fprintf(stderr, "cannot move Node.js into %ls (error %lu)\n", dest, GetLastError());
                else
                    r = 0;
            }
            CloseHandle(lock);
        }
    }
    DeleteFileW(zip);
    RemoveDirectoryW(dl);
    if (r != 0)
    {
        ols_remove_tree(stage);
        return 1;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (!GetPrivateProfileStringW(L"node", L"bin", L"", cur, MAX_PATH, ini) || !cur[0] || !rt_exists(cur))
    {
        swprintf(exe, MAX_PATH, L"%s\\node.exe", dest);
        WritePrivateProfileStringW(L"node", L"bin", exe, ini);
        ols_dev_resync(root, ini);
    }
    printf("%s is in %ls\n", what, dest);
    return 0;
}

static int node_pick(const wchar_t *root, const char *arg, int *idx)
{
    wchar_t w[MAX_PATH], full[MAX_PATH];
    int i;
    node_detect(root);
    if (!arg || MultiByteToWideChar(CP_UTF8, 0, arg, -1, w, MAX_PATH) <= 0 || !GetFullPathNameW(w, MAX_PATH, full, NULL))
        return -1;
    for (i = 0; i < s_nnode; ++i)
        if (_wcsicmp(s_node[i].exe, full) == 0)
        {
            *idx = i;
            return 0;
        }
    fprintf(stderr, "that node.exe is not one OpenLiteSpeed found on this computer\n");
    return -1;
}

static int node_set_default(const wchar_t *root, const char *arg)
{
    wchar_t ini[MAX_PATH];
    int i;
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (arg && strcmp(arg, "auto") == 0)
        WritePrivateProfileStringW(L"node", L"bin", NULL, ini);
    else if (node_pick(root, arg, &i) != 0)
        return 2;
    else
        WritePrivateProfileStringW(L"node", L"bin", s_node[i].exe, ini);
    if (ols_dev_resync(root, ini) != 0)
        return 1;
    printf("Node.js default saved\n");
    return 0;
}

static int node_remove(const wchar_t *root, const char *arg)
{
    wchar_t dir[MAX_PATH], def[MAX_PATH], ini[MAX_PATH], v[MAX_PATH], *s;
    int i;
    if (node_pick(root, arg, &i) != 0)
        return 2;
    if (!s_node[i].ours)
    {
        fprintf(stderr, "only Node.js versions downloaded by OpenLiteSpeed can be removed here\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    wcscpy(dir, s_node[i].exe);
    if ((s = wcsrchr(dir, L'\\')) != NULL)
        *s = 0;
    if (GetPrivateProfileStringW(L"node", L"bin", L"", v, MAX_PATH, ini) && _wcsicmp(v, s_node[i].exe) == 0)
        WritePrivateProfileStringW(L"node", L"bin", NULL, ini);
    if (rt_retire(root, dir) != 0)
    {
        fprintf(stderr, "this Node.js is still in use; restart the apps and try again\n");
        return 1;
    }
    if (ols_node_default(root, def, MAX_PATH) == 0)
        WritePrivateProfileStringW(L"node", L"bin", def, ini);
    ols_dev_resync(root, ini);
    printf("removed %ls\n", dir);
    return 0;
}

static int composer_install(const wchar_t *root)
{
    wchar_t dl[MAX_PATH], dir[MAX_PATH], phar[MAX_PATH], sum[MAX_PATH], tmp[MAX_PATH], bat[MAX_PATH];
    char *t = NULL, sha[65];
    size_t n;
    HANDLE lock;
    int r = 1;
    static const char s_bat[] = "@php \"%~dp0composer.phar\" %*\r\n";
    swprintf(dir, MAX_PATH, L"%s\\tools", root);
    CreateDirectoryW(dir, NULL);
    swprintf(dir, MAX_PATH, L"%s\\tools\\composer", root);
    CreateDirectoryW(dir, NULL);
    if (!rt_is_dir(dir) || ols_temp_dir(root, dl) != 0)
    {
        fprintf(stderr, "cannot prepare the Composer folder\n");
        return 1;
    }
    swprintf(sum, MAX_PATH, L"%s\\composer.sha256", dl);
    swprintf(tmp, MAX_PATH, L"%s\\composer.phar", dl);
    if (ols_download(COMPOSER_SUM, sum, 4096, NULL) != 0 || rt_read(sum, &t, &n, 4096) != 0 || n < 64)
        fprintf(stderr, "could not read the Composer checksum\n");
    else
    {
        memcpy(sha, t, 64);
        sha[64] = 0;
        if (!is_hex64(sha))
            fprintf(stderr, "the Composer checksum was not what we expected\n");
        else if (ols_download(COMPOSER_PHAR, tmp, 16 << 20, "Composer") == 0)
        {
            if ((lock = ols_open_verified(tmp, sha, -1)) == INVALID_HANDLE_VALUE)
                fprintf(stderr, "Composer failed the checksum check\n");
            else
            {
                swprintf(phar, MAX_PATH, L"%s\\composer.phar", dir);
                CloseHandle(lock);
                if (!MoveFileExW(tmp, phar, MOVEFILE_REPLACE_EXISTING))
                    fprintf(stderr, "cannot write %ls\n", phar);
                else
                    r = 0;
            }
        }
    }
    free(t);
    DeleteFileW(sum);
    DeleteFileW(tmp);
    RemoveDirectoryW(dl);
    if (r != 0)
        return 1;
    swprintf(bat, MAX_PATH, L"%s\\composer.bat", dir);
    if (ols_write_wfile(bat, s_bat, sizeof(s_bat) - 1, 0) != 0)
    {
        fprintf(stderr, "cannot write %ls\n", bat);
        return 1;
    }
    printf("Composer is in %ls\n", dir);
    return 0;
}

int ols_php_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH];
    const char *act = argc >= 1 ? argv[0] : "";
    if (rt_root(root) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    if (strcmp(act, "available") == 0)
        return php_available(root);
    if (strcmp(act, "install") == 0 && argc >= 2)
        return php_install(root, argv[1], argc >= 3 && strcmp(argv[2], "ts") == 0);
    if (strcmp(act, "remove") == 0 && argc >= 2)
        return php_remove(root, argv[1]);
    if (strcmp(act, "ext") == 0 && argc >= 2)
        return php_ext_list(root, argv[1]);
    if (strcmp(act, "ext-set") == 0 && argc >= 4)
        return php_ext_set(root, argv[1], argv[2], strcmp(argv[3], "on") == 0);
    if (strcmp(act, "ini") == 0 && argc >= 2)
        return php_ini_get(root, argv[1]);
    if (strcmp(act, "ini-create") == 0 && argc >= 2)
    {
        wchar_t dir[MAX_PATH];
        if (php_dir_arg(root, argv[1], dir) != 0)
            return 2;
        return php_make_ini(dir) == 0 ? 0 : 1;
    }
    if (strcmp(act, "ini-set") == 0 && argc >= 4)
        return php_ini_set(root, argv[1], argv[2], argv[3]);
    fprintf(stderr, "usage: openlitespeed --php available | install <8.x> [ts] | remove <dir> | ext <dir> |\n"
                    "       ext-set <dir> <name> on|off | ini <dir> | ini-set <dir> <key> <value>\n");
    return 2;
}

int ols_node_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH];
    const char *act = argc >= 1 ? argv[0] : "";
    if (rt_root(root) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    if (strcmp(act, "list") == 0)
        return node_list(root);
    if (strcmp(act, "available") == 0)
        return node_available(root);
    if (strcmp(act, "install") == 0 && argc >= 2 && atoi(argv[1]) >= 18 && atoi(argv[1]) < 1000)
        return node_install(root, atoi(argv[1]));
    if (strcmp(act, "default") == 0 && argc >= 2)
        return node_set_default(root, argv[1]);
    if (strcmp(act, "remove") == 0 && argc >= 2)
        return node_remove(root, argv[1]);
    fprintf(stderr, "usage: openlitespeed --node list | available | install <major> | default <node.exe|auto> |\n"
                    "       remove <node.exe>\n");
    return 2;
}

int ols_composer_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH];
    (void)argv;
    if (rt_root(root) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    if (argc >= 1 && strcmp(argv[0], "install") == 0)
        return composer_install(root);
    fprintf(stderr, "usage: openlitespeed --composer install\n");
    return 2;
}

void ols_status_runtime(const wchar_t *root, const wchar_t *ini)
{
    wchar_t def[MAX_PATH], phar[MAX_PATH];
    int i;
    char ver[32] = "";
    (void)ini;
    if (ols_node_default(root, def, MAX_PATH) == 0)
        for (i = 0; i < s_nnode; ++i)
            if (_wcsicmp(s_node[i].exe, def) == 0)
                snprintf(ver, sizeof(ver), "%d.%d.%d", s_node[i].major, s_node[i].minor, s_node[i].patch);
    printf(",\"node\":{\"exe\":");
    ols_jwstr(def);
    printf(",\"version\":");
    ols_jstr(ver);
    printf(",\"count\":%d}", s_nnode);
    swprintf(phar, MAX_PATH, L"%s\\tools\\composer\\composer.phar", root);
    printf(",\"composer\":%s", rt_exists(phar) ? "true" : "false");
}
