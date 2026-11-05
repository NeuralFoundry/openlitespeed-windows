#include "compat_internal.h"
#include "phpdetect.h"

#define DEV_BEGIN "# dev-sites begin"
#define DEV_END   "# dev-sites end"
#define HOSTS_BEGIN "# OpenLiteSpeed dev sites begin"
#define HOSTS_END   "# OpenLiteSpeed dev sites end"

static const char s_template[] =
    "allowSymbolLink          1\r\n"
    "enableScript             1\r\n"
    "restrained               0\r\n"
    "setUIDMode               0\r\n"
    "vhRoot                   $SERVER_ROOT/www/$VH_NAME/\r\n"
    "\r\n"
    "virtualHostConfig {\r\n"
    "  docRoot                $VH_ROOT/\r\n"
    "  enableGzip             1\r\n"
    "\r\n"
    "  index {\r\n"
    "    useServer            0\r\n"
    "    autoIndex            1\r\n"
    "    indexFiles           index.php, index.html, index.htm\r\n"
    "    autoIndexURI         /_autoindex/default.php\r\n"
    "  }\r\n"
    "\r\n"
    "  context /_autoindex/ {\r\n"
    "    location             $SERVER_ROOT/share/autoindex/\r\n"
    "    allowBrowse          1\r\n"
    "  }\r\n"
    "\r\n"
    "  rewrite {\r\n"
    "    enable               1\r\n"
    "    autoLoadHtaccess     1\r\n"
    "  }\r\n"
    "}\r\n";

#define MAX_PROXIES 64

static const char s_proxy[] =
    "allowSymbolLink          1\r\n"
    "enableScript             0\r\n"
    "restrained               1\r\n"
    "setUIDMode               0\r\n"
    "vhRoot                   $SERVER_ROOT/tmp/proxy/$VH_NAME/\r\n"
    "\r\n"
    "virtualHostConfig {\r\n"
    "  docRoot                $VH_ROOT/\r\n"
    "\r\n"
    "  accessControl {\r\n"
    "    allow                127.0.0.1, ::1\r\n"
    "    deny                 ALL\r\n"
    "  }\r\n"
    "\r\n"
    "  extprocessor devproxy-%s {\r\n"
    "    type                 proxy\r\n"
    "    address              127.0.0.1:%d\r\n"
    "    maxConns             100\r\n"
    "    pcKeepAliveTimeout   60\r\n"
    "    initTimeout          60\r\n"
    "    retryTimeout         0\r\n"
    "    respBuffer           0\r\n"
    "  }\r\n"
    "\r\n"
    "  context / {\r\n"
    "    type                 proxy\r\n"
    "    handler              devproxy-%s\r\n"
    "    addDefaultCharset    off\r\n"
    "  }\r\n"
    "\r\n"
    "  websocket / {\r\n"
    "    address              127.0.0.1:%d\r\n"
    "  }\r\n"
    "}\r\n";

static const char s_wp_guard[] =
    "<FilesMatch \"^(install|setup-config)\\.php$\">\r\n"
    "Require ip 127.0.0.1 ::1\r\n"
    "</FilesMatch>\r\n";

static const char s_tools[] =
    "allowSymbolLink          1\r\n"
    "enableScript             1\r\n"
    "restrained               0\r\n"
    "setUIDMode               0\r\n"
    "vhRoot                   $SERVER_ROOT/share/$VH_NAME/\r\n"
    "\r\n"
    "virtualHostConfig {\r\n"
    "  docRoot                $VH_ROOT/\r\n"
    "  enableGzip             1\r\n"
    "\r\n"
    "  index {\r\n"
    "    useServer            0\r\n"
    "    autoIndex            0\r\n"
    "    indexFiles           index.php\r\n"
    "  }\r\n"
    "\r\n"
    "  accessControl {\r\n"
    "    allow                127.0.0.1, ::1\r\n"
    "    deny                 ALL\r\n"
    "  }\r\n"
    "\r\n"
    "  context /setup/ {\r\n"
    "    location             $VH_ROOT/setup/\r\n"
    "    allowBrowse          0\r\n"
    "  }\r\n"
    "}\r\n";

static int read_file(const wchar_t *path, char **out, size_t *len)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    DWORD got;
    *out = NULL;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > 16 * 1024 * 1024)
    {
        CloseHandle(h);
        return -1;
    }
    *out = (char *)malloc((size_t)sz.QuadPart + 1);
    if (!*out || !ReadFile(h, *out, (DWORD)sz.QuadPart, &got, NULL))
    {
        free(*out);
        *out = NULL;
        CloseHandle(h);
        return -1;
    }
    (*out)[got] = 0;
    *len = got;
    CloseHandle(h);
    return 0;
}

static int write_in_place(const wchar_t *path, const char *data, size_t len)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w;
    BOOL ok;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    ok = WriteFile(h, data, (DWORD)len, &w, NULL) && w == len && SetEndOfFile(h);
    CloseHandle(h);
    return ok ? 0 : -1;
}

static int write_file(const wchar_t *path, const char *data, size_t len)
{
    wchar_t tmp[MAX_PATH + 16];
    HANDLE h;
    DWORD w;
    if (swprintf(tmp, MAX_PATH + 16, L"%s.%lu", path, GetCurrentProcessId()) < 0)
        return -1;
    h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!WriteFile(h, data, (DWORD)len, &w, NULL) || w != len)
    {
        CloseHandle(h);
        DeleteFileW(tmp);
        return -1;
    }
    CloseHandle(h);
    if (!MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DWORD e = GetLastError();
        DeleteFileW(tmp);
        SetLastError(e);
        return -1;
    }
    return 0;
}

static void buf_add(char **b, size_t *len, size_t *cap, const char *s, size_t n)
{
    if (*len + n + 1 > *cap)
    {
        size_t nc = (*cap + n + 1) * 2;
        char *nb = (char *)realloc(*b, nc);
        if (!nb)
            return;
        *b = nb;
        *cap = nc;
    }
    memcpy(*b + *len, s, n);
    *len += n;
    (*b)[*len] = 0;
}

static void buf_str(char **b, size_t *len, size_t *cap, const char *s)
{
    buf_add(b, len, cap, s, strlen(s));
}

static int valid_label(const char *s)
{
    size_t n = strlen(s), i;
    if (n == 0 || n > 63 || s[0] == '-' || s[n - 1] == '-')
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-'))
            return 0;
    return 1;
}

static int root_dir(wchar_t *buf, DWORD len)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", buf, len);
    if (n == 0 || n >= len)
        return -1;
    while (n > 3 && (buf[n - 1] == L'\\' || buf[n - 1] == L'/'))
        buf[--n] = 0;
    return 0;
}

static char *replace_block(const char *text, const char *begin, const char *end, const char *block)
{
    const char *b = strstr(text, begin), *e;
    char *out = NULL;
    size_t len = 0, cap = 0;
    if (b && (e = strstr(b, end)) != NULL)
    {
        e += strlen(end);
        while (*e == '\r' || *e == '\n')
            ++e;
        buf_add(&out, &len, &cap, text, (size_t)(b - text));
        buf_str(&out, &len, &cap, block);
        buf_str(&out, &len, &cap, e);
    }
    else
    {
        buf_str(&out, &len, &cap, text);
        if (len && out[len - 1] != '\n')
            buf_str(&out, &len, &cap, "\r\n");
        buf_str(&out, &len, &cap, "\r\n");
        buf_str(&out, &len, &cap, block);
    }
    return out;
}

static int dir_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int file_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void add_aliases_from(const wchar_t *file, char **out, size_t *len, size_t *cap)
{
    char *text = NULL, *line, *next;
    size_t n;
    if (read_file(file, &text, &n) != 0 || !text)
        return;
    for (line = text; line && *line; line = next)
    {
        char url[256], path[MAX_PATH * 3], ctx[1024], *p = line;
        wchar_t wpath[MAX_PATH];
        size_t i;
        next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        while (*p == ' ' || *p == '\t')
            ++p;
        if (_strnicmp(p, "Alias", 5) != 0 || (p[5] != ' ' && p[5] != '\t'))
            continue;
        p += 5;
        while (*p == ' ' || *p == '\t')
            ++p;
        for (i = 0; *p && *p != ' ' && *p != '\t' && i < sizeof(url) - 2; ++p)
            if (*p != '"')
                url[i++] = *p;
        url[i] = 0;
        while (*p == ' ' || *p == '\t')
            ++p;
        if (*p == '"')
            ++p;
        for (i = 0; *p && *p != '"' && *p != '\r' && *p != '\n' && i < sizeof(path) - 2; ++p)
            path[i++] = *p == '\\' ? '/' : *p;
        path[i] = 0;
        while (i && (path[i - 1] == ' ' || path[i - 1] == '\t'))
            path[--i] = 0;
        if (url[0] != '/' || url[1] == 0 || !i || strpbrk(url, "\r\n{}") || strpbrk(path, "{}"))
            continue;
        if (path[0] == '/' && path[1] != '/' && file[0] && file[1] == L':' && i + 2 < sizeof(path))
        {
            memmove(path + 2, path, i + 1);
            path[0] = (char)file[0];
            path[1] = ':';
            i += 2;
        }
        i = strlen(url);
        if (url[i - 1] != '/')
        {
            url[i] = '/';
            url[i + 1] = 0;
        }
        i = strlen(path);
        if (path[i - 1] != '/')
        {
            path[i] = '/';
            path[i + 1] = 0;
        }
        if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH) <= 0 || !dir_exists(wpath))
            continue;
        {
            wchar_t real[MAX_PATH + 8], *r = real;
            HANDLE h = CreateFileW(wpath, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            DWORD rn = 0;
            if (h != INVALID_HANDLE_VALUE)
            {
                rn = GetFinalPathNameByHandleW(h, real, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                CloseHandle(h);
            }
            if (rn == 0 || rn >= MAX_PATH)
                continue;
            if (wcsncmp(r, L"\\\\?\\", 4) == 0)
                r += 4;
            if (WideCharToMultiByte(CP_UTF8, 0, r, -1, path, sizeof(path) - 2, NULL, NULL) <= 0)
                continue;
            for (i = 0; path[i]; ++i)
                if (path[i] == '\\')
                    path[i] = '/';
            if (i && path[i - 1] != '/')
            {
                path[i] = '/';
                path[i + 1] = 0;
            }
        }
        snprintf(ctx, sizeof(ctx), "context %s {", url);
        if (*out && strstr(*out, ctx))
            continue;
        snprintf(ctx, sizeof(ctx),
                 "\r\n  context %s {\r\n"
                 "    location             %s\r\n"
                 "    allowBrowse          1\r\n"
                 "    accessControl {\r\n"
                 "      allow              127.0.0.1, ::1\r\n"
                 "      deny               ALL\r\n"
                 "    }\r\n"
                 "    rewrite {\r\n"
                 "      enable             0\r\n"
                 "    }\r\n"
                 "  }\r\n",
                 url, path);
        buf_str(out, len, cap, ctx);
        printf("http://localhost%s  ->  %s (local only)\n", url, path);
    }
    free(text);
}

static const char s_app[] =
    "allowSymbolLink          1\r\n"
    "enableScript             1\r\n"
    "restrained               1\r\n"
    "setUIDMode               0\r\n"
    "vhRoot                   %s/%s/\r\n"
    "\r\n"
    "virtualHostConfig {\r\n"
    "  docRoot                $VH_ROOT/public/\r\n"
    "  enableGzip             1\r\n"
    "\r\n"
    "  context / {\r\n"
    "    type                 appserver\r\n"
    "    location             %s/%s/\r\n"
    "    appType              %s\r\n"
    "%s"
    "    appserverEnv         0\r\n"
    "  }\r\n"
    "\r\n"
    "  rewrite {\r\n"
    "    enable               0\r\n"
    "  }\r\n"
    "}\r\n";

static const char s_hidden[] =
    "\r\n"
    "  context exp:^/(?:.*/)?\\.(?!well-known/) {\r\n"
    "    allowBrowse          0\r\n"
    "  }\r\n";

#define MAX_CFG 128
#define MAX_GROUPS 8

static int php_proc(const wchar_t *root, const wchar_t *wdir, const char *name, int tcp, char *out, size_t n,
                    int *pmajor, int *pminor, int *pts, int *plsapi, int noexec);

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

static int conf_safe(const char *s)
{
    return !strpbrk(s, "{}\r\n");
}

static void mail_sync(const wchar_t *root)
{
    wchar_t ini[MAX_PATH];
    int changed;
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (GetPrivateProfileIntW(L"mail", L"on", 0, ini))
        ols_php_mail_sync(root, 1, &changed);
}

static int read_names(const wchar_t *ini, const wchar_t *sect, char (*names)[64], wchar_t (*vals)[MAX_PATH], int max)
{
    wchar_t buf[16384], *e;
    int n = 0;
    if (GetPrivateProfileSectionW(sect, buf, 16384, ini) <= 0)
        return 0;
    for (e = buf; *e && n < max; e += wcslen(e) + 1)
    {
        wchar_t *eq = wcschr(e, L'=');
        if (!eq || eq - e >= 64)
            continue;
        *eq = 0;
        if (WideCharToMultiByte(CP_UTF8, 0, e, -1, names[n], 64, NULL, NULL) <= 0 || !valid_label(names[n])
            || wcslen(eq + 1) >= MAX_PATH)
            continue;
        wcscpy(vals[n], eq + 1);
        ++n;
    }
    return n;
}

static unsigned dir_hash(const wchar_t *d)
{
    unsigned h = 2166136261u;
    for (; *d; ++d)
        h = (h ^ (unsigned)towlower(*d)) * 16777619u;
    return h;
}

static char *build_template(const wchar_t *root, int with_aliases)
{
    char *out = NULL;
    size_t len = 0, cap = 0;
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    const wchar_t *laragon[2], *xampp[2];
    wchar_t lr[MAX_PATH], xr[MAX_PATH], lr2[MAX_PATH], xr2[MAX_PATH], d[MAX_PATH], ini[MAX_PATH], stack[16];
    int i, use_lr, use_xr;
    buf_add(&out, &len, &cap, s_template, sizeof(s_template) - 1 - 3);
    buf_str(&out, &len, &cap, s_hidden);
    if (with_aliases)
    {
        swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
        GetPrivateProfileStringW(L"sites", L"stack", L"", stack, 16, ini);
        use_lr = !stack[0] || _wcsicmp(stack, L"laragon") == 0;
        use_xr = !stack[0] || _wcsicmp(stack, L"xampp") == 0;
        lr[0] = xr[0] = lr2[0] = xr2[0] = 0;
        if (use_lr)
        {
            swprintf(lr, MAX_PATH, L"%s\\..\\..\\etc\\apache2\\alias", root);
            if (phpd_find_laragon(d))
                swprintf(lr2, MAX_PATH, L"%s\\etc\\apache2\\alias", d);
        }
        if (use_xr)
        {
            swprintf(xr, MAX_PATH, L"%s\\..\\apache\\conf\\extra\\httpd-xampp.conf", root);
            if (phpd_find_xampp(d))
                swprintf(xr2, MAX_PATH, L"%s\\apache\\conf\\extra\\httpd-xampp.conf", d);
        }
        laragon[0] = lr;
        laragon[1] = lr2;
        xampp[0] = xr;
        xampp[1] = xr2;
        for (i = 0; i < 2; ++i)
        {
            WIN32_FIND_DATAW fd;
            HANDLE f;
            swprintf(pat, MAX_PATH, L"%s\\*.conf", laragon[i]);
            f = laragon[i][0] && dir_exists(laragon[i]) ? FindFirstFileW(pat, &fd) : INVALID_HANDLE_VALUE;
            if (f != INVALID_HANDLE_VALUE)
            {
                do
                {
                    swprintf(p, MAX_PATH, L"%s\\%s", laragon[i], fd.cFileName);
                    add_aliases_from(p, &out, &len, &cap);
                }
                while (FindNextFileW(f, &fd));
                FindClose(f);
            }
            if (xampp[i][0] && file_exists(xampp[i]))
                add_aliases_from(xampp[i], &out, &len, &cap);
        }
    }
    buf_str(&out, &len, &cap, "}\r\n");
    return out;
}

int ols_dev_sites(const char *www, const char *tld, int with_root)
{
    wchar_t root[MAX_PATH], conf[MAX_PATH], tpl[MAX_PATH], wwww[MAX_PATH], pat[MAX_PATH], p[MAX_PATH], ini[MAX_PATH];
    char *text = NULL, *out, *block = NULL, *mem = NULL, name[256], wwwfwd[MAX_PATH * 3], line[1024];
    char **names = NULL;
    size_t len, blen = 0, bcap = 0, mlen = 0, mcap = 0;
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int count = 0, nnames = 0, ncap = 0, https, ssl = 0, tools, i, np = 0, pports[MAX_PROXIES];
    char pnames[MAX_PROXIES][64];
    char *q;
    static char appn[MAX_CFG][64], spn[MAX_CFG][64];
    static wchar_t appv[MAX_CFG][MAX_PATH], spv[MAX_CFG][MAX_PATH];
    wchar_t gdir[MAX_GROUPS][MAX_PATH], cur[MAX_PATH], full[MAX_PATH];
    char *gmem[MAX_GROUPS], *procs = NULL, ghash[MAX_GROUPS][12];
    size_t gml[MAX_GROUPS], gmc[MAX_GROUPS], plen = 0, pcap = 0;
    int napp, nsp, ng = 0, g, spg[MAX_CFG], appk[MAX_CFG], appon[MAX_CFG], elev = token_elevated();
    char *rootmem = NULL, *rootctx = NULL;
    size_t rml = 0, rmc = 0, rcl = 0, rcc = 0;
    memset(gmem, 0, sizeof(gmem));
    memset(gml, 0, sizeof(gml));
    memset(gmc, 0, sizeof(gmc));
    if (!tld || !*tld)
        tld = "test";
    if (!valid_label(tld) || root_dir(root, MAX_PATH) < 0
        || MultiByteToWideChar(CP_UTF8, 0, www, -1, wwww, MAX_PATH) <= 0 || !dir_exists(wwww))
    {
        fprintf(stderr, "invalid www folder or tld\n");
        return 2;
    }
    GetFullPathNameW(wwww, MAX_PATH, p, NULL);
    wcscpy(wwww, p);
    if (WideCharToMultiByte(CP_UTF8, 0, wwww, -1, wwwfwd, sizeof(wwwfwd), NULL, NULL) <= 0)
        return 2;
    for (q = wwwfwd; *q; ++q)
        if (*q == '\\')
            *q = '/';
    if (q > wwwfwd && q[-1] == '/')
        q[-1] = 0;
    if (!conf_safe(wwwfwd))
    {
        fprintf(stderr, "the path of the projects folder must not contain { or }\n");
        return 2;
    }

    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    napp = read_names(ini, L"apps", appn, appv, MAX_CFG);
    for (i = 0; i < napp; ++i)
    {
        appk[i] = _wcsicmp(appv[i], L"python") == 0 ? 2 : _wcsicmp(appv[i], L"node") == 0 ? 1 : 0;
        appon[i] = 0;
    }
    nsp = read_names(ini, L"sitephp", spn, spv, MAX_CFG);
    if (ols_php_current(root, cur) != 0)
        cur[0] = 0;
    for (i = 0; i < nsp; ++i)
    {
        spg[i] = -1;
        if (!ols_php_usable(root, spv[i], full) || (cur[0] && _wcsicmp(full, cur) == 0))
            continue;
        for (g = 0; g < ng && _wcsicmp(gdir[g], full) != 0; ++g)
            ;
        if (g == ng)
        {
            if (ng == MAX_GROUPS)
                continue;
            wcscpy(gdir[ng], full);
            snprintf(ghash[ng], sizeof(ghash[ng]), "%08x", dir_hash(full));
            ++ng;
        }
        spg[i] = g;
    }

    swprintf(tpl, MAX_PATH, L"%s\\conf\\templates", root);
    CreateDirectoryW(tpl, NULL);
    wcscat(tpl, L"\\devsite.conf");
    {
        char *t = build_template(root, with_root);
        if (!t || write_file(tpl, t, strlen(t)) != 0)
        {
            fprintf(stderr, "cannot write %ls (error %lu)\n", tpl, GetLastError());
            free(t);
            return 1;
        }
        free(t);
    }

    if (with_root)
    {
        snprintf(line, sizeof(line),
                 "    member localhost {\r\n        vhDomain             localhost\r\n"
                 "        vhAliases            127.0.0.1, *\r\n        vhRoot               %s/\r\n    }\r\n",
                 wwwfwd);
        buf_str(&rootmem, &rml, &rmc, line);
        printf("http://localhost/  ->  %s/\n", wwwfwd);
    }
    swprintf(pat, MAX_PATH, L"%s\\*", wwww);
    f = FindFirstFileW(pat, &fd);
    if (f != INVALID_HANDLE_VALUE)
    {
        do
        {
            const char *sub = "";
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.'
                || (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
                continue;
            if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof(name), NULL, NULL) <= 0)
                continue;
            if (!valid_label(name) || strcmp(name, "localhost") == 0)
            {
                printf("skipped %s (use lowercase letters, digits and '-')\n", name);
                continue;
            }
            for (i = 0; i < napp && (appk[i] == 0 || strcmp(appn[i], name) != 0); ++i)
                ;
            swprintf(p, MAX_PATH, L"%s\\%s\\artisan", wwww, fd.cFileName);
            if (i < napp || file_exists(p))
            {
                snprintf(line, sizeof(line), "\r\n  context exp:^/%s/(?!public/) {\r\n    allowBrowse          0\r\n  }\r\n",
                         name);
                buf_str(&rootctx, &rcl, &rcc, line);
            }
            if (i < napp)
            {
                appon[i] = 1;
                printf("http://%s.%s/  ->  %s app in %s/%s/\n", name, tld, appk[i] == 2 ? "Python" : "Node.js", wwwfwd,
                       name);
            }
            else
            {
                swprintf(p, MAX_PATH, L"%s\\%s\\public\\index.php", wwww, fd.cFileName);
                if (file_exists(p))
                    sub = "public/";
                snprintf(line, sizeof(line),
                         "    member %s {\r\n        vhDomain             %s.%s\r\n"
                         "        vhAliases            www.%s.%s\r\n        vhRoot               %s/%s/%s\r\n    }\r\n",
                         name, name, tld, name, tld, wwwfwd, name, sub);
                for (i = 0; i < nsp && strcmp(spn[i], name) != 0; ++i)
                    ;
                if (i < nsp && spg[i] >= 0)
                    buf_str(&gmem[spg[i]], &gml[spg[i]], &gmc[spg[i]], line);
                else
                    buf_str(&mem, &mlen, &mcap, line);
                printf("http://%s.%s/  ->  %s/%s/%s\n", name, tld, wwwfwd, name, sub);
            }
            if (nnames == ncap)
            {
                char **grown = (char **)realloc(names, (size_t)(ncap ? ncap * 2 : 32) * sizeof(char *));
                if (grown)
                {
                    names = grown;
                    ncap = ncap ? ncap * 2 : 32;
                }
            }
            if (nnames < ncap && (names[nnames] = _strdup(name)) != NULL)
                ++nnames;
            ++count;
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    swprintf(p, MAX_PATH, L"%s\\share\\phpmyadmin\\index.php", root);
    tools = file_exists(p);
    for (i = 0; tools && i < nnames; ++i)
        if (strcmp(names[i], "phpmyadmin") == 0)
            tools = 0;
    if (tools)
    {
        swprintf(tpl, MAX_PATH, L"%s\\conf\\templates\\devtools.conf", root);
        if (write_file(tpl, s_tools, sizeof(s_tools) - 1) != 0)
            tools = 0;
        else
        {
            if (nnames == ncap)
            {
                char **grown = (char **)realloc(names, (size_t)(ncap ? ncap * 2 : 32) * sizeof(char *));
                if (grown)
                {
                    names = grown;
                    ncap = ncap ? ncap * 2 : 32;
                }
            }
            if (nnames < ncap && (names[nnames] = _strdup("phpmyadmin")) != NULL)
                ++nnames;
            printf("http://phpmyadmin.%s/  ->  phpMyAdmin (this computer only)\n", tld);
        }
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    {
        wchar_t sect[8192], *e;
        char pn[64], body[2048];
        int pp, j, clash;
        if (GetPrivateProfileSectionW(L"proxy", sect, 8192, ini) > 0)
            for (e = sect; *e && np < MAX_PROXIES; e += wcslen(e) + 1)
            {
                wchar_t *eq = wcschr(e, L'=');
                if (!eq || eq - e >= 64)
                    continue;
                *eq = 0;
                pp = _wtoi(eq + 1);
                if (WideCharToMultiByte(CP_UTF8, 0, e, -1, pn, sizeof(pn), NULL, NULL) <= 0 || !valid_label(pn)
                    || pp <= 0 || pp > 65535 || strcmp(pn, "localhost") == 0 || strcmp(pn, "phpmyadmin") == 0)
                    continue;
                for (j = 0, clash = 0; j < nnames && !clash; ++j)
                    clash = strcmp(names[j], pn) == 0;
                if (clash)
                {
                    printf("skipped proxy %s: a folder or tool has that name\n", pn);
                    continue;
                }
                swprintf(tpl, MAX_PATH, L"%s\\tmp\\proxy", root);
                CreateDirectoryW(tpl, NULL);
                swprintf(tpl, MAX_PATH, L"%s\\tmp\\proxy\\%s", root, e);
                CreateDirectoryW(tpl, NULL);
                snprintf(body, sizeof(body), s_proxy, pn, pp, pn, pp);
                swprintf(tpl, MAX_PATH, L"%s\\conf\\templates\\proxy-%s.conf", root, e);
                if (write_file(tpl, body, strlen(body)) != 0)
                    continue;
                strcpy(pnames[np], pn);
                pports[np++] = pp;
                if (nnames == ncap)
                {
                    char **grown = (char **)realloc(names, (size_t)(ncap ? ncap * 2 : 32) * sizeof(char *));
                    if (grown)
                    {
                        names = grown;
                        ncap = ncap ? ncap * 2 : 32;
                    }
                }
                if (nnames < ncap && (names[nnames] = _strdup(pn)) != NULL)
                    ++nnames;
                printf("http://%s.%s/  ->  127.0.0.1:%d\n", pn, tld, pp);
            }
    }
    for (g = 0; g < ng; ++g)
    {
        char proc[4096], *t;
        int maj, mn, ts, lsapi;
        size_t tl;
        if (!gmem[g])
            continue;
        snprintf(line, sizeof(line), "lsphp-%s", ghash[g]);
        t = php_proc(root, gdir[g], line, 9101 + g, proc, sizeof(proc), &maj, &mn, &ts, &lsapi, elev) == 0
                ? build_template(root, with_root)
                : NULL;
        if (t && (tl = strlen(t)) >= 3)
        {
            char *tt = NULL;
            size_t ttl = 0, ttc = 0;
            buf_add(&tt, &ttl, &ttc, t, tl - 3);
            snprintf(line, sizeof(line), "\r\n  scriptHandler {\r\n    add                  %s:lsphp-%s php\r\n  }\r\n}\r\n",
                     lsapi ? "lsapi" : "fcgi", ghash[g]);
            buf_str(&tt, &ttl, &ttc, line);
            free(t);
            t = tt;
            swprintf(tpl, MAX_PATH, L"%s\\conf\\templates\\devsite-%S.conf", root, ghash[g]);
            if (t && write_file(tpl, t, strlen(t)) == 0)
            {
                buf_str(&procs, &plen, &pcap, proc);
                printf("PHP %d.%d %s for: %ls\n", maj, mn, ts ? "TS" : "NTS", gdir[g]);
                free(t);
                continue;
            }
        }
        free(t);
        fprintf(stderr, "sites that use %ls fall back to the default PHP\n", gdir[g]);
        buf_str(&mem, &mlen, &mcap, gmem[g]);
        free(gmem[g]);
        gmem[g] = NULL;
    }
    {
        wchar_t node[MAX_PATH], wn[64];
        char nodef[MAX_PATH * 3], binl[MAX_PATH * 3 + 64], body[4096 + MAX_PATH * 6];
        const char *from;
        for (i = 0; i < napp; ++i)
        {
            if (!appon[i])
                continue;
            MultiByteToWideChar(CP_UTF8, 0, appn[i], -1, wn, 64);
            swprintf(p, MAX_PATH, L"%s\\%s", wwww, wn);
            nodef[0] = 0;
            if (appk[i] == 1 && ols_site_node(root, ini, appn[i], p, node, MAX_PATH, &from) == 0
                && WideCharToMultiByte(CP_UTF8, 0, node, -1, nodef, sizeof(nodef), NULL, NULL) > 0)
                for (q = nodef; *q; ++q)
                    if (*q == '\\')
                        *q = '/';
            swprintf(p, MAX_PATH, L"%s\\%s\\public", wwww, wn);
            CreateDirectoryW(p, NULL);
            binl[0] = 0;
            if (appk[i] == 1 && nodef[0] && conf_safe(nodef))
                snprintf(binl, sizeof(binl), "    binPath              %s\r\n", nodef);
            snprintf(body, sizeof(body), s_app, wwwfwd, appn[i], wwwfwd, appn[i], appk[i] == 2 ? "wsgi" : "node", binl);
            swprintf(tpl, MAX_PATH, L"%s\\conf\\templates\\app-%s.conf", root, wn);
            if (write_file(tpl, body, strlen(body)) != 0)
            {
                fprintf(stderr, "cannot write %ls\n", tpl);
                appon[i] = 0;
            }
        }
    }
    if (rootmem)
    {
        char *t = build_template(root, 1), *tt = NULL;
        size_t tl = t ? strlen(t) : 0, ttl = 0, ttc = 0;
        if (tl >= 3)
        {
            buf_add(&tt, &ttl, &ttc, t, tl - 3);
            if (rootctx)
                buf_str(&tt, &ttl, &ttc, rootctx);
            buf_str(&tt, &ttl, &ttc, "}\r\n");
        }
        free(t);
        swprintf(tpl, MAX_PATH, L"%s\\conf\\templates\\devroot.conf", root);
        if (!tt || write_file(tpl, tt, strlen(tt)) != 0)
        {
            fprintf(stderr, "cannot write %ls (error %lu)\n", tpl, GetLastError());
            free(rootmem);
            rootmem = NULL;
        }
        free(tt);
    }
    free(rootctx);
    https = GetPrivateProfileIntW(L"sites", L"https", 0, ini);
    if (https > 0 && https < 65536)
    {
        wchar_t lc[MAX_PATH], lk[MAX_PATH];
        int r = ols_devca_issue(root, tld, names, nnames);
        swprintf(lc, MAX_PATH, L"%s\\conf\\cert\\devsites.crt", root);
        swprintf(lk, MAX_PATH, L"%s\\conf\\cert\\devsites.key", root);
        if (r == 0)
            ssl = 1;
        else if (r == 3)
            fprintf(stderr, "trusted HTTPS needs the .test, .localhost or .internal suffix, so it is not used\n");
        else if (file_exists(lc) && file_exists(lk))
        {
            ssl = 1;
            fprintf(stderr, "kept the previous HTTPS certificate; turn trusted HTTPS on again to renew it\n");
        }
        else
            fprintf(stderr, "trusted HTTPS is on but no certificate could be made; turn it on again\n");
    }
    while (nnames > 0)
        free(names[--nnames]);
    free(names);
    buf_str(&block, &blen, &bcap, DEV_BEGIN "\r\n");
    if (procs)
        buf_str(&block, &blen, &bcap, procs);
    free(procs);
    if (ssl)
    {
        snprintf(line, sizeof(line),
                 "listener DevSSL {\r\n    address                  *:%d\r\n    secure                   1\r\n"
                 "    keyFile                  $SERVER_ROOT/conf/cert/devsites.key\r\n"
                 "    certFile                 $SERVER_ROOT/conf/cert/devsites.crt\r\n}\r\n",
                 https);
        buf_str(&block, &blen, &bcap, line);
    }
    if (rootmem)
    {
        buf_str(&block, &blen, &bcap, "vhTemplate devroot {\r\n    templateFile             conf/templates/devroot.conf\r\n");
        buf_str(&block, &blen, &bcap, ssl ? "    listeners                Default, DevSSL\r\n"
                                          : "    listeners                Default\r\n");
        buf_str(&block, &blen, &bcap, rootmem);
        buf_str(&block, &blen, &bcap, "}\r\n");
        free(rootmem);
    }
    buf_str(&block, &blen, &bcap, "vhTemplate devsites {\r\n    templateFile             conf/templates/devsite.conf\r\n");
    buf_str(&block, &blen, &bcap, ssl ? "    listeners                Default, DevSSL\r\n"
                                      : "    listeners                Default\r\n");
    if (mem)
        buf_str(&block, &blen, &bcap, mem);
    free(mem);
    buf_str(&block, &blen, &bcap, "}\r\n");
    if (tools)
    {
        snprintf(line, sizeof(line),
                 "vhTemplate devtools {\r\n    templateFile             conf/templates/devtools.conf\r\n"
                 "    listeners                Default%s\r\n    member phpmyadmin {\r\n"
                 "        vhDomain             phpmyadmin.%s\r\n    }\r\n}\r\n",
                 ssl ? ", DevSSL" : "", tld);
        buf_str(&block, &blen, &bcap, line);
    }
    for (i = 0; i < np; ++i)
    {
        snprintf(line, sizeof(line),
                 "vhTemplate devproxy-%s {\r\n    templateFile             conf/templates/proxy-%s.conf\r\n"
                 "    listeners                Default%s\r\n    member %s {\r\n"
                 "        vhDomain             %s.%s\r\n        vhAliases            www.%s.%s\r\n    }\r\n}\r\n",
                 pnames[i], pnames[i], ssl ? ", DevSSL" : "", pnames[i], pnames[i], tld, pnames[i], tld);
        buf_str(&block, &blen, &bcap, line);
    }
    for (g = 0; g < ng; ++g)
    {
        if (!gmem[g])
            continue;
        snprintf(line, sizeof(line),
                 "vhTemplate devsites-%s {\r\n    templateFile             conf/templates/devsite-%s.conf\r\n"
                 "    listeners                Default%s\r\n",
                 ghash[g], ghash[g], ssl ? ", DevSSL" : "");
        buf_str(&block, &blen, &bcap, line);
        buf_str(&block, &blen, &bcap, gmem[g]);
        buf_str(&block, &blen, &bcap, "}\r\n");
        free(gmem[g]);
    }
    for (i = 0; i < napp; ++i)
    {
        if (!appon[i])
            continue;
        snprintf(line, sizeof(line),
                 "vhTemplate devapp-%s {\r\n    templateFile             conf/templates/app-%s.conf\r\n"
                 "    listeners                Default%s\r\n    member %s {\r\n"
                 "        vhDomain             %s.%s\r\n        vhAliases            www.%s.%s\r\n    }\r\n}\r\n",
                 appn[i], appn[i], ssl ? ", DevSSL" : "", appn[i], appn[i], tld, appn[i], tld);
        buf_str(&block, &blen, &bcap, line);
    }
    buf_str(&block, &blen, &bcap, DEV_END "\r\n");
    if (!block)
        return 1;

    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (read_file(conf, &text, &len) != 0 || !text)
    {
        fprintf(stderr, "cannot read %ls\n", conf);
        free(block);
        return 1;
    }
    if (with_root)
    {
        char *fixed = NULL, *ls = text;
        size_t fl = 0, fc = 0;
        while (*ls)
        {
            char *nl = strchr(ls, '\n'), *e, *m = ls;
            size_t ll = nl ? (size_t)(nl - ls + 1) : strlen(ls);
            e = ls + ll;
            while (e > ls && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
                --e;
            while (*m == ' ' || *m == '\t')
                ++m;
            if (e - m > 5 && strncmp(m, "map", 3) == 0 && e[-1] == '*' && (e[-2] == ' ' || e[-2] == '\t')
                && strstr(m, "Example") && strstr(m, "Example") < e)
            {
                buf_add(&fixed, &fl, &fc, ls, (size_t)(e - 1 - ls));
                buf_str(&fixed, &fl, &fc, "example.localhost");
                buf_add(&fixed, &fl, &fc, e, ll - (size_t)(e - ls));
            }
            else
                buf_add(&fixed, &fl, &fc, ls, ll);
            ls += ll;
        }
        if (fixed)
        {
            free(text);
            text = fixed;
        }
    }
    out = replace_block(text, DEV_BEGIN, DEV_END, block);
    free(text);
    free(block);
    if (!out || write_file(conf, out, strlen(out)) != 0)
    {
        fprintf(stderr, "cannot write %ls (error %lu)\n", conf, GetLastError());
        free(out);
        return 1;
    }
    free(out);
    mail_sync(root);
    printf("%d site(s)\n", count);
    return 0;
}

static int run_wait(wchar_t *cmd)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

static int install_wordpress(const wchar_t *dir)
{
    wchar_t sys[MAX_PATH], zip[MAX_PATH], chk[MAX_PATH], cmd[MAX_PATH * 4];
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return 1;
    swprintf(zip, MAX_PATH, L"%s\\.wp-download.zip", dir);
    swprintf(cmd, MAX_PATH * 4,
             L"\"%s\\curl.exe\" -q -fsS --proto =https --tlsv1.2 --connect-timeout 20 --max-time 900 -L -o \"%s\" "
             L"https://wordpress.org/latest.zip",
             sys, zip);
    printf("downloading WordPress...\n");
    if (run_wait(cmd) != 0)
    {
        fprintf(stderr, "could not download WordPress\n");
        DeleteFileW(zip);
        return 1;
    }
    swprintf(chk, MAX_PATH, L"%s\\wp-admin", dir);
    CreateDirectoryW(chk, NULL);
    swprintf(chk, MAX_PATH, L"%s\\wp-admin\\.htaccess", dir);
    if (write_file(chk, s_wp_guard, sizeof(s_wp_guard) - 1) != 0)
    {
        fprintf(stderr, "could not protect the WordPress installer\n");
        DeleteFileW(zip);
        return 1;
    }
    swprintf(cmd, MAX_PATH * 4, L"\"%s\\tar.exe\" -xf \"%s\" -C \"%s\" --strip-components=1", sys, zip, dir);
    if (run_wait(cmd) != 0)
    {
        fprintf(stderr, "could not extract WordPress\n");
        DeleteFileW(zip);
        return 1;
    }
    DeleteFileW(zip);
    swprintf(chk, MAX_PATH, L"%s\\wp-includes", dir);
    if (!dir_exists(chk))
    {
        fprintf(stderr, "the WordPress archive was not what we expected\n");
        return 1;
    }
    printf("WordPress installed; open the site and finish the setup in the browser\n");
    return 0;
}

static int run_shown(const wchar_t *exe, wchar_t *cmd, const wchar_t *cwd, DWORD timeout)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    fflush(stdout);
    fflush(stderr);
    if (!CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd, &si, &pi))
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

static int php_has_module(const wchar_t *php, const wchar_t *phprc, const char *mod)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd, wr, nul;
    wchar_t cmd[MAX_PATH + 8], old[MAX_PATH];
    char out[16384], want[64];
    DWORD got, total = 0, had;
    int ok;
    if (!CreatePipe(&rd, &wr, &sa, 65536))
        return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    swprintf(cmd, MAX_PATH + 8, L"\"%s\" -m", php);
    had = GetEnvironmentVariableW(L"PHPRC", old, MAX_PATH);
    SetEnvironmentVariableW(L"PHPRC", phprc);
    ok = CreateProcessW(php, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, phprc, &si, &pi);
    SetEnvironmentVariableW(L"PHPRC", had && had < MAX_PATH ? old : NULL);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    if (!ok)
    {
        CloseHandle(rd);
        return 0;
    }
    CloseHandle(pi.hThread);
    while (total < sizeof(out) - 1 && ReadFile(rd, out + total, (DWORD)(sizeof(out) - 1 - total), &got, NULL) && got)
        total += got;
    CloseHandle(rd);
    if (WaitForSingleObject(pi.hProcess, 20000) != WAIT_OBJECT_0)
        TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    out[total] = 0;
    snprintf(want, sizeof(want), "\n%s\r", mod);
    if (strstr(out, want))
        return 1;
    snprintf(want, sizeof(want), "\n%s\n", mod);
    return strstr(out, want) != NULL;
}

static int install_laravel(const wchar_t *root, const wchar_t *dir)
{
    wchar_t cur[MAX_PATH], php[MAX_PATH], phar[MAX_PATH], cmd[MAX_PATH * 4], old[MAX_PATH], www[MAX_PATH], *s;
    DWORD had;
    int r;
    if (ols_php_current(root, cur) != 0)
    {
        fprintf(stderr, "choose a PHP version first\n");
        return 1;
    }
    swprintf(php, MAX_PATH, L"%s\\php.exe", cur);
    swprintf(phar, MAX_PATH, L"%s\\tools\\composer\\composer.phar", root);
    if (!file_exists(php))
    {
        fprintf(stderr, "php.exe is missing from %ls\n", cur);
        return 1;
    }
    if (!file_exists(phar))
    {
        fprintf(stderr, "install Composer first\n");
        return 1;
    }
    if (!php_has_module(php, cur, "pdo_sqlite"))
    {
        fprintf(stderr, "Laravel stores its data in SQLite: turn on the pdo_sqlite extension of the default PHP on the PHP page, then try again\n");
        return 1;
    }
    wcscpy(www, dir);
    if ((s = wcsrchr(www, L'\\')) != NULL)
        *s = 0;
    swprintf(cmd, MAX_PATH * 4,
             L"\"%s\" \"%s\" create-project laravel/laravel \"%s\" --no-interaction --prefer-dist --no-progress --no-ansi",
             php, phar, dir);
    printf("creating a Laravel project with Composer...\n");
    had = GetEnvironmentVariableW(L"PHPRC", old, MAX_PATH);
    SetEnvironmentVariableW(L"PHPRC", cur);
    r = run_shown(php, cmd, www, 1800000);
    SetEnvironmentVariableW(L"PHPRC", had && had < MAX_PATH ? old : NULL);
    swprintf(cmd, MAX_PATH * 4, L"%s\\artisan", dir);
    if (r != 0 || !file_exists(cmd))
    {
        fprintf(stderr, "Composer could not create the Laravel project\n");
        return 1;
    }
    return 0;
}

static int write_app_starter(const wchar_t *dir, const char *name, const char *tld, int python)
{
    wchar_t f[MAX_PATH];
    char text[2048];
    int n;
    swprintf(f, MAX_PATH, L"%s\\public", dir);
    if (!CreateDirectoryW(f, NULL))
        return -1;
    if (python)
    {
        n = snprintf(text, sizeof(text),
                     "import sys\r\n\r\n\r\ndef application(environ, start_response):\r\n"
                     "    body = ('<!doctype html><title>%s.%s</title><h1>%s</h1>'\r\n"
                     "            '<p>OpenLiteSpeed is running this Python %%s app.</p>' %% sys.version.split()[0]).encode()\r\n"
                     "    start_response('200 OK', [('Content-Type', 'text/html; charset=utf-8'),\r\n"
                     "                              ('Content-Length', str(len(body)))])\r\n"
                     "    return [body]\r\n",
                     name, tld, name);
        swprintf(f, MAX_PATH, L"%s\\wsgi.py", dir);
        return n > 0 && (size_t)n < sizeof(text) ? write_file(f, text, (size_t)n) : -1;
    }
    n = snprintf(text, sizeof(text),
                 "{\r\n  \"name\": \"%s\",\r\n  \"private\": true,\r\n  \"main\": \"server.js\",\r\n"
                 "  \"scripts\": {\r\n    \"start\": \"node server.js\"\r\n  }\r\n}\r\n",
                 name);
    swprintf(f, MAX_PATH, L"%s\\package.json", dir);
    if (n <= 0 || (size_t)n >= sizeof(text) || write_file(f, text, (size_t)n) != 0)
        return -1;
    n = snprintf(text, sizeof(text),
                 "const http = require('http');\r\n\r\n"
                 "const server = http.createServer((req, res) => {\r\n"
                 "  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });\r\n"
                 "  res.end('<!doctype html><title>%s.%s</title><h1>%s</h1>' +\r\n"
                 "    '<p>OpenLiteSpeed is running this Node.js ' + process.version + ' app.</p>');\r\n"
                 "});\r\n\r\n"
                 "server.listen(process.env.PORT || 3000);\r\n",
                 name, tld, name);
    swprintf(f, MAX_PATH, L"%s\\server.js", dir);
    return n > 0 && (size_t)n < sizeof(text) ? write_file(f, text, (size_t)n) : -1;
}

int ols_site_php(const wchar_t *root, const wchar_t *ini, const char *site, wchar_t *dir, size_t n)
{
    wchar_t wn[64], v[MAX_PATH];
    dir[0] = 0;
    if (n < MAX_PATH || MultiByteToWideChar(CP_UTF8, 0, site, -1, wn, 64) <= 0)
        return -1;
    GetPrivateProfileStringW(L"sitephp", wn, L"", v, MAX_PATH, ini);
    if (!v[0] || !ols_php_usable(root, v, dir))
    {
        dir[0] = 0;
        return -1;
    }
    return 0;
}

int ols_site_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH], www[MAX_PATH], def[MAX_PATH], dir[MAX_PATH], wname[64], w[MAX_PATH],
        full[MAX_PATH], cur[MAX_PATH];
    const char *act = argc >= 1 ? argv[0] : "";
    int rc;
    if (root_dir(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (strcmp(act, "resync") == 0)
        return ols_dev_resync(root, ini);
    if (argc < 2 || !valid_label(argv[1]) || strcmp(argv[1], "localhost") == 0 || strcmp(argv[1], "phpmyadmin") == 0
        || MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wname, 64) <= 0)
    {
        fprintf(stderr, "usage: openlitespeed --site php <name> <php folder|default> | node <name> <node.exe|default> |\n"
                        "       app <name> node|python|off | forget <name>\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    swprintf(def, MAX_PATH, L"%s\\www", root);
    GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
    swprintf(dir, MAX_PATH, L"%s\\%s", www, wname);
    if (strcmp(act, "forget") == 0)
    {
        WritePrivateProfileStringW(L"apps", wname, NULL, ini);
        WritePrivateProfileStringW(L"sitephp", wname, NULL, ini);
        WritePrivateProfileStringW(L"sitenode", wname, NULL, ini);
        ols_jobs_forget_site(root, argv[1]);
    }
    else if (!dir_exists(dir))
    {
        fprintf(stderr, "the projects folder has no site named %s\n", argv[1]);
        return 2;
    }
    else if (strcmp(act, "php") == 0 && argc >= 3)
    {
        if (strcmp(argv[2], "default") == 0)
            WritePrivateProfileStringW(L"sitephp", wname, NULL, ini);
        else if (MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, w, MAX_PATH) <= 0 || !ols_php_known(root, w, full))
        {
            fprintf(stderr, "that PHP folder is not one OpenLiteSpeed found on this computer\n");
            return 2;
        }
        else if (ols_php_current(root, cur) == 0 && _wcsicmp(cur, full) == 0)
            WritePrivateProfileStringW(L"sitephp", wname, NULL, ini);
        else if (!WritePrivateProfileStringW(L"sitephp", wname, full, ini))
            return 1;
    }
    else if (strcmp(act, "node") == 0 && argc >= 3)
    {
        if (strcmp(argv[2], "default") == 0)
            WritePrivateProfileStringW(L"sitenode", wname, NULL, ini);
        else if (MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, w, MAX_PATH) <= 0 || !ols_node_known(root, w, full))
        {
            fprintf(stderr, "that node.exe is not one OpenLiteSpeed found on this computer\n");
            return 2;
        }
        else if (!WritePrivateProfileStringW(L"sitenode", wname, full, ini))
            return 1;
    }
    else if (strcmp(act, "app") == 0 && argc >= 3
             && (strcmp(argv[2], "node") == 0 || strcmp(argv[2], "python") == 0 || strcmp(argv[2], "off") == 0))
    {
        if (strcmp(argv[2], "off") == 0)
            WritePrivateProfileStringW(L"apps", wname, NULL, ini);
        else
        {
            swprintf(w, MAX_PATH, L"%s\\public", dir);
            CreateDirectoryW(w, NULL);
            if (!WritePrivateProfileStringW(L"apps", wname, argv[2][0] == 'p' ? L"python" : L"node", ini))
                return 1;
        }
    }
    else
    {
        fprintf(stderr, "usage: openlitespeed --site php <name> <php folder|default> | node <name> <node.exe|default> |\n"
                        "       app <name> node|python|off | forget <name>\n");
        return 2;
    }
    rc = ols_dev_resync(root, ini);
    if (rc == 0)
        printf("%s updated\n", argv[1]);
    return rc;
}

static int listener_port(const wchar_t *file, const char *anchor)
{
    char *text = NULL, *p, *q, *c = NULL;
    size_t len;
    int port = 0;
    if (read_file(file, &text, &len) != 0 || !text)
        return 0;
    if ((p = strstr(text, anchor)) != NULL && (p = strstr(p, "address")) != NULL)
    {
        for (q = p; *q && *q != '\r' && *q != '\n'; ++q)
            if (*q == ':')
                c = q;
        if (c)
            port = atoi(c + 1);
    }
    free(text);
    return port;
}

static int set_listener_port(const wchar_t *file, const char *anchor, int port)
{
    char *text = NULL, *p, *e, *c = NULL, *out = NULL, num[16];
    size_t len, olen = 0, ocap = 0;
    int r;
    if (read_file(file, &text, &len) != 0 || !text)
        return -1;
    if ((p = strstr(text, anchor)) == NULL || (p = strstr(p, "address")) == NULL)
    {
        free(text);
        return -1;
    }
    for (e = p; *e && *e != '\r' && *e != '\n'; ++e)
        if (*e == ':')
            c = e;
    if (!c)
    {
        free(text);
        return -1;
    }
    snprintf(num, sizeof(num), "%d", port);
    buf_add(&out, &olen, &ocap, text, (size_t)(c + 1 - text));
    buf_str(&out, &olen, &ocap, num);
    buf_str(&out, &olen, &ocap, e);
    free(text);
    r = out ? write_file(file, out, olen) : -1;
    free(out);
    return r;
}

int ols_set_port(const char *which, const char *port_s)
{
    wchar_t root[MAX_PATH], conf[MAX_PATH], admin[MAX_PATH], ini[MAX_PATH];
    int port = atoi(port_s), http, adm, https, admin_side;
    if (root_dir(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    admin_side = strcmp(which, "admin") == 0;
    if ((!admin_side && strcmp(which, "http") != 0) || port <= 0 || port > 65535 || strspn(port_s, "0123456789") != strlen(port_s))
    {
        fprintf(stderr, "usage: openlitespeed --set-port http|admin <port>\n");
        return 2;
    }
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    swprintf(admin, MAX_PATH, L"%s\\admin\\conf\\admin_config.conf", root);
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    http = ols_default_port(root);
    adm = listener_port(admin, "listener adminListener");
    https = GetPrivateProfileIntW(L"sites", L"https", 0, ini);
    if (port == (admin_side ? adm : http))
    {
        printf("the port is already %d\n", port);
        return 0;
    }
    if (port == (admin_side ? http : adm) || port == https)
    {
        fprintf(stderr, "port %d is already used by OpenLiteSpeed\n", port);
        return 2;
    }
    if (ols_port_listening(port))
    {
        fprintf(stderr, "another program is listening on port %d\n", port);
        return 2;
    }
    if (set_listener_port(admin_side ? admin : conf, admin_side ? "listener adminListener" : "listener Default", port) != 0)
    {
        fprintf(stderr, "cannot change the port in the configuration\n");
        return 1;
    }
    printf("%s port is now %d; restart the web server to use it\n", admin_side ? "WebAdmin" : "HTTP", port);
    return 0;
}

int ols_new_site(const char *name, const char *tld, const char *flavor)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH], www[MAX_PATH], def[MAX_PATH], dir[MAX_PATH], idx[MAX_PATH], wname[128];
    wchar_t wtld[32];
    char wwwmb[MAX_PATH * 3], html[512], ctld[32];
    int withRoot, rc, n, wp;
    if (!name || !valid_label(name) || strcmp(name, "localhost") == 0)
    {
        fprintf(stderr, "invalid site name (use lowercase letters, digits and '-')\n");
        return 2;
    }
    if (root_dir(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    GetPrivateProfileStringW(L"sites", L"tld", L"test", wtld, 32, ini);
    if (WideCharToMultiByte(CP_UTF8, 0, wtld, -1, ctld, sizeof(ctld), NULL, NULL) <= 0 || !valid_label(ctld))
    {
        fprintf(stderr, "the site suffix in olstray.ini is not valid\n");
        return 2;
    }
    if (tld && *tld && strcmp(tld, ctld) != 0)
    {
        fprintf(stderr, "the projects folder uses .%s, not .%s\n", ctld, tld);
        return 2;
    }
    tld = ctld;
    swprintf(def, MAX_PATH, L"%s\\www", root);
    GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
    CreateDirectoryW(www, NULL);
    if (!dir_exists(www))
    {
        fprintf(stderr, "cannot use the projects folder %ls\n", www);
        return 2;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 128) <= 0)
        return 2;
    swprintf(dir, MAX_PATH, L"%s\\%s", www, wname);
    if (dir_exists(dir))
    {
        fprintf(stderr, "a folder named %s already exists\n", name);
        return 1;
    }
    if (!CreateDirectoryW(dir, NULL))
    {
        fprintf(stderr, "cannot create %ls (error %lu)\n", dir, GetLastError());
        return 1;
    }
    wp = flavor && strcmp(flavor, "wordpress") == 0;
    if (wp)
    {
        if (install_wordpress(dir) != 0)
        {
            RemoveDirectoryW(dir);
            return 1;
        }
        if (ols_db_wordpress(root, name, dir) == 1)
            fprintf(stderr, "WordPress will ask for the database details in the browser\n");
    }
    else if (flavor && strcmp(flavor, "laravel") == 0)
    {
        if (install_laravel(root, dir) != 0)
        {
            if (ols_remove_tree(dir) != 0)
                fprintf(stderr, "remove the folder %ls before you try again\n", dir);
            return 1;
        }
        if (ols_mail_laravel_env(root, dir) != 0)
            fprintf(stderr, "warning: the mail settings in .env could not be changed\n");
    }
    else if (flavor && (strcmp(flavor, "node") == 0 || strcmp(flavor, "python") == 0))
    {
        if (write_app_starter(dir, name, tld, flavor[0] == 'p') != 0)
        {
            ols_remove_tree(dir);
            return 1;
        }
        WritePrivateProfileStringW(L"apps", wname, flavor[0] == 'p' ? L"python" : L"node", ini);
    }
    else
    {
        swprintf(idx, MAX_PATH, L"%s\\index.php", dir);
        n = snprintf(html, sizeof(html),
                     "<?php\r\nheader('Content-Type: text/html; charset=utf-8');\r\n"
                     "echo '<!doctype html><title>%s.%s</title>"
                     "<h1>%s</h1><p>OpenLiteSpeed is serving this site on PHP ' . PHP_VERSION . '.</p>';\r\n",
                     name, tld, name);
        if (n > 0 && (size_t)n < sizeof(html))
            write_file(idx, html, (size_t)n);
    }
    if (WideCharToMultiByte(CP_UTF8, 0, www, -1, wwwmb, sizeof(wwwmb), NULL, NULL) <= 0)
        return 1;
    withRoot = GetPrivateProfileIntW(L"sites", L"root", 0, ini);
    rc = ols_dev_sites(wwwmb, tld, withRoot);
    if (rc == 0)
    {
        ols_dev_hosts(0);
        printf("site %s.%s created at %ls\n", name, tld, dir);
    }
    return rc;
}

int ols_default_port(const wchar_t *root)
{
    wchar_t conf[MAX_PATH];
    char *text = NULL, *p, *q, *c = NULL;
    size_t len;
    int port = 80;
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (read_file(conf, &text, &len) != 0 || !text)
        return 80;
    if ((p = strstr(text, "listener Default")) != NULL && (p = strstr(p, "address")) != NULL)
    {
        for (q = p; *q && *q != '\r' && *q != '\n'; ++q)
            if (*q == ':')
                c = q;
        if (c)
            port = atoi(c + 1);
    }
    free(text);
    return port;
}

int ols_proxy_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH], wname[64], wport[16], def[MAX_PATH], www[MAX_PATH], dir[MAX_PATH];
    const char *act = argc >= 1 ? argv[0] : "list";
    int port = 0, rc;
    if (root_dir(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (strcmp(act, "list") == 0)
    {
        wchar_t sect[8192], *e;
        if (GetPrivateProfileSectionW(L"proxy", sect, 8192, ini) > 0)
            for (e = sect; *e; e += wcslen(e) + 1)
                printf("%ls\n", e);
        return 0;
    }
    if ((strcmp(act, "add") != 0 && strcmp(act, "remove") != 0) || argc < 2)
    {
        fprintf(stderr, "usage: openlitespeed --proxy add <name> <port> | remove <name> | list\n");
        return 2;
    }
    if (!valid_label(argv[1]) || strcmp(argv[1], "localhost") == 0 || strcmp(argv[1], "phpmyadmin") == 0
        || MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wname, 64) <= 0)
    {
        fprintf(stderr, "invalid site name (use lowercase letters, digits and '-')\n");
        return 2;
    }
    if (strcmp(act, "add") == 0)
    {
        if (argc < 3 || (port = atoi(argv[2])) <= 0 || port > 65535)
        {
            fprintf(stderr, "invalid port\n");
            return 2;
        }
        if (port == ols_default_port(root) || port == GetPrivateProfileIntW(L"sites", L"https", 0, ini))
        {
            fprintf(stderr, "port %d is OpenLiteSpeed itself; use the port your app listens on\n", port);
            return 2;
        }
        swprintf(def, MAX_PATH, L"%s\\www", root);
        GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
        swprintf(dir, MAX_PATH, L"%s\\%s", www, wname);
        if (dir_exists(dir))
        {
            fprintf(stderr, "the projects folder already has a site named %s\n", argv[1]);
            return 1;
        }
        if (GetPrivateProfileIntW(L"proxy", wname, 0, ini) > 0)
        {
            fprintf(stderr, "a proxy site named %s already exists\n", argv[1]);
            return 1;
        }
        swprintf(wport, 16, L"%d", port);
        if (!WritePrivateProfileStringW(L"proxy", wname, wport, ini))
            return GetLastError() == ERROR_ACCESS_DENIED ? 5 : 1;
    }
    else
    {
        if (GetPrivateProfileIntW(L"proxy", wname, 0, ini) <= 0)
        {
            fprintf(stderr, "no proxy site named %s\n", argv[1]);
            return 1;
        }
        WritePrivateProfileStringW(L"proxy", wname, NULL, ini);
        swprintf(dir, MAX_PATH, L"%s\\conf\\templates\\proxy-%s.conf", root, wname);
        DeleteFileW(dir);
    }
    rc = ols_dev_resync(root, ini);
    if (rc == 0)
    {
        ols_dev_hosts(0);
        if (port)
            printf("proxy site %s created for 127.0.0.1:%d\n", argv[1], port);
        else
            printf("proxy site %s removed\n", argv[1]);
    }
    return rc;
}

static char *remove_legacy_hosts(char *t, int *changed)
{
    static const char *nl[2] = { "\r\n", "\n" };
    char b[64], e[64], *out;
    int i, j;
    *changed = 0;
    for (i = 0; i < 2; ++i)
    {
        snprintf(b, sizeof(b), "%s%s", HOSTS_BEGIN, nl[i]);
        if (!strstr(t, b))
            continue;
        for (j = 0; j < 2; ++j)
        {
            snprintf(e, sizeof(e), "%s%s", HOSTS_END, nl[j]);
            if (strstr(strstr(t, b), e) && (out = replace_block(t, b, e, "")) != NULL)
            {
                free(t);
                *changed = 1;
                return out;
            }
        }
    }
    return t;
}

static int hosts_tld(const char *tld)
{
    static const char *const ok[] = { "test", "localhost", "internal", "local", "lan", "example", "invalid", NULL };
    int i;
    for (i = 0; ok[i]; ++i)
        if (strcmp(tld, ok[i]) == 0)
            return 1;
    return 0;
}

static int host_ok(const char *dom, const char *tld)
{
    size_t n = strlen(dom), t = strlen(tld);
    char label[64];
    if (!t || n <= t + 1 || n - t - 1 >= sizeof(label) || dom[n - t - 1] != '.' || strcmp(dom + n - t, tld) != 0)
        return 0;
    memcpy(label, dom, n - t - 1);
    label[n - t - 1] = 0;
    return valid_label(label);
}

int ols_dev_hosts(int clear)
{
    wchar_t root[MAX_PATH], conf[MAX_PATH], hosts[MAX_PATH], ini[MAX_PATH], wtld[32];
    char tld[32];
    char *text = NULL, *htext = NULL, *block = NULL, *out, rootfwd[MAX_PATH * 3], tb[MAX_PATH * 3 + 64],
        te[MAX_PATH * 3 + 64], *q;
    const char *p, *end;
    size_t len, blen = 0, bcap = 0;
    UINT n;
    int changed, names = 0;
    if (root_dir(root, MAX_PATH) < 0
        || WideCharToMultiByte(CP_UTF8, 0, root, -1, rootfwd, sizeof(rootfwd), NULL, NULL) <= 0)
        return 2;
    for (q = rootfwd; *q; ++q)
        if (*q == '\\')
            *q = '/';
    snprintf(tb, sizeof(tb), "%s [%s]", HOSTS_BEGIN, rootfwd);
    snprintf(te, sizeof(te), "%s [%s]", HOSTS_END, rootfwd);
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    GetPrivateProfileStringW(L"sites", L"tld", L"test", wtld, 32, ini);
    if (!clear && (WideCharToMultiByte(CP_UTF8, 0, wtld, -1, tld, sizeof(tld), NULL, NULL) <= 0 || !hosts_tld(tld)))
    {
        fprintf(stderr, "site names go into the hosts file only with the .test, .localhost, .internal, .local or .lan suffix\n");
        return 2;
    }
    n = GetSystemDirectoryW(hosts, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 32)
        return 1;
    wcscat(hosts, L"\\drivers\\etc\\hosts");
    if (read_file(conf, &text, &len) != 0 || !text)
        return 1;
    buf_str(&block, &blen, &bcap, tb);
    buf_str(&block, &blen, &bcap, "\r\n");
    p = clear ? NULL : strstr(text, DEV_BEGIN);
    end = p ? strstr(p, DEV_END) : NULL;
    while (p && end && (p = strstr(p, "vhDomain")) != NULL && p < end)
    {
        char dom[256], alias[300];
        size_t i = 0;
        p += 8;
        while (*p == ' ' || *p == '\t')
            ++p;
        while (*p && *p != '\r' && *p != '\n' && *p != ' ' && i < sizeof(dom) - 1)
            dom[i++] = *p++;
        dom[i] = 0;
        if (!i || strcmp(dom, "localhost") == 0 || !host_ok(dom, tld))
            continue;
        snprintf(alias, sizeof(alias), "127.0.0.1 %s www.%s\r\n::1 %s www.%s\r\n", dom, dom, dom, dom);
        buf_str(&block, &blen, &bcap, alias);
        ++names;
    }
    free(text);
    buf_str(&block, &blen, &bcap, te);
    buf_str(&block, &blen, &bcap, "\r\n");
    if (!block || read_file(hosts, &htext, &len) != 0)
    {
        free(block);
        return 1;
    }
    if (!htext)
        htext = _strdup("");
    if (!htext)
    {
        free(block);
        return 1;
    }
    htext = remove_legacy_hosts(htext, &changed);
    if (clear || !names)
    {
        block[0] = 0;
        p = strstr(htext, tb);
        if (!p || !strstr(p, te))
        {
            free(block);
            if (!changed)
            {
                free(htext);
                if (!clear)
                    printf("hosts file is up to date\n");
                return 0;
            }
            out = htext;
            goto write;
        }
    }
    out = replace_block(htext, tb, te, block);
    if (out && !block[0] && (p = strstr(htext, tb)) != NULL)
    {
        size_t at = (size_t)(p - htext);
        if (at >= 3 && htext[at - 1] == '\n' && htext[at - 2] == '\r' && htext[at - 3] == '\n')
            memmove(out + at - 2, out + at, strlen(out + at) + 1);
    }
    if (out && !changed && strcmp(out, htext) == 0)
    {
        free(out);
        free(htext);
        free(block);
        printf("hosts file is up to date\n");
        return 0;
    }
    free(htext);
    free(block);
write:
    if (!out)
        return 1;
    if (write_in_place(hosts, out, strlen(out)) != 0)
    {
        DWORD e = GetLastError();
        free(out);
        if (e == ERROR_ACCESS_DENIED)
        {
            fprintf(stderr, "updating the hosts file needs Administrator rights\n");
            return 5;
        }
        fprintf(stderr, "cannot write hosts file (error %lu)\n", e);
        return 1;
    }
    free(out);
    printf("hosts file updated\n");
    return 0;
}

#define PHP_BEGIN "# php begin"
#define PHP_END   "# php end"

static int php_version(const wchar_t *exe, int *major, int *minor)
{
    DWORD h = 0, n = GetFileVersionInfoSizeW(exe, &h);
    void *buf;
    VS_FIXEDFILEINFO *fi;
    UINT fl;
    int ok = 0;
    if (n == 0 || (buf = malloc(n)) == NULL)
        return -1;
    if (GetFileVersionInfoW(exe, 0, n, buf) && VerQueryValueW(buf, L"\\", (void **)&fi, &fl) && fl)
    {
        *major = HIWORD(fi->dwFileVersionMS);
        *minor = LOWORD(fi->dwFileVersionMS);
        ok = 1;
    }
    free(buf);
    return ok ? 0 : -1;
}

static int same_file_content(const wchar_t *a, const wchar_t *b)
{
    char *x = NULL, *y = NULL;
    size_t lx, ly;
    int same;
    if (read_file(a, &x, &lx) != 0 || read_file(b, &y, &ly) != 0 || !x || !y)
    {
        free(x);
        free(y);
        return 0;
    }
    same = lx == ly && memcmp(x, y, lx) == 0;
    free(x);
    free(y);
    return same;
}

static int probe_php(const wchar_t *exe, const wchar_t *dir, int major, int minor, const char *sapi,
                     DWORD *code)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd, wr, nul;
    wchar_t cmd[MAX_PATH + 8], old[MAX_PATH];
    char out[2048], want[32];
    DWORD got, total = 0, had;
    int ok;
    *code = (DWORD)-1;
    if (!CreatePipe(&rd, &wr, &sa, 65536))
        return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    swprintf(cmd, MAX_PATH + 8, L"\"%s\" -v", exe);
    had = GetEnvironmentVariableW(L"PHPRC", old, MAX_PATH);
    SetEnvironmentVariableW(L"PHPRC", dir);
    ok = CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, dir, &si, &pi);
    SetEnvironmentVariableW(L"PHPRC", had && had < MAX_PATH ? old : NULL);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    if (!ok)
    {
        CloseHandle(rd);
        return 0;
    }
    CloseHandle(pi.hThread);
    if (WaitForSingleObject(pi.hProcess, 20000) != WAIT_OBJECT_0)
        TerminateProcess(pi.hProcess, 1);
    GetExitCodeProcess(pi.hProcess, code);
    CloseHandle(pi.hProcess);
    while (total < sizeof(out) - 1 && ReadFile(rd, out + total, (DWORD)(sizeof(out) - 1 - total), &got, NULL) && got)
        total += got;
    CloseHandle(rd);
    out[total] = 0;
    snprintf(want, sizeof(want), "PHP %d.%d.", major, minor);
    return *code == 0 && strstr(out, want) && strstr(out, sapi);
}

static int php_proc(const wchar_t *root, const wchar_t *wdir, const char *name, int tcp, char *out, size_t n,
                    int *pmajor, int *pminor, int *pts, int *plsapi, int noexec)
{
    wchar_t cgi[MAX_PATH], dll[MAX_PATH], lsphp[MAX_PATH], src[MAX_PATH], sock[MAX_PATH], wn[64];
    char fdir[MAX_PATH * 3], addr[160], *q;
    int major = 0, minor = 0, ts, lsapi = 0, r;
    DWORD code;
    swprintf(cgi, MAX_PATH, L"%s\\php-cgi.exe", wdir);
    if (!file_exists(cgi) || php_version(cgi, &major, &minor) != 0)
    {
        fprintf(stderr, "php-cgi.exe not found in %ls\n", wdir);
        return 2;
    }
    if (noexec)
    {
        swprintf(dll, MAX_PATH, L"%s\\php%dts.dll", wdir, major);
        ts = file_exists(dll);
        swprintf(src, MAX_PATH, L"%s\\bin\\lsphp\\%d.%d-%s\\lsphp.exe", root, major, minor, ts ? L"ts" : L"nts");
        swprintf(lsphp, MAX_PATH, L"%s\\lsphp.exe", wdir);
        lsapi = file_exists(src) && same_file_content(src, lsphp);
    }
    else if (!probe_php(cgi, wdir, major, minor, "(cgi-fcgi)", &code))
    {
        const char *why = "";
        if (code == 0xC0000135)
        {
            HMODULE vc = LoadLibraryExW(L"vcruntime140.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
            why = vc ? ": a DLL is missing from the PHP folder" : ": the Microsoft Visual C++ runtime is missing";
            if (vc)
                FreeLibrary(vc);
        }
        fprintf(stderr, "php-cgi.exe in %ls does not start (exit code 0x%08lX)%s\n", wdir, code, why);
        return 3;
    }
    else
    {
        swprintf(dll, MAX_PATH, L"%s\\php%dts.dll", wdir, major);
        ts = file_exists(dll);
        swprintf(src, MAX_PATH, L"%s\\bin\\lsphp\\%d.%d-%s\\lsphp.exe", root, major, minor, ts ? L"ts" : L"nts");
        swprintf(lsphp, MAX_PATH, L"%s\\lsphp.exe", wdir);
    }
    if (!noexec && file_exists(src))
    {
        if (!same_file_content(src, lsphp) && !CopyFileW(src, lsphp, FALSE))
            fprintf(stderr, "warning: cannot copy lsphp.exe into %ls (error %lu), using php-cgi\n", wdir,
                    GetLastError());
        else if (!probe_php(lsphp, wdir, major, minor, "(litespeed)", &code))
            fprintf(stderr, "warning: lsphp.exe does not run with this PHP build (exit code 0x%08lX), using php-cgi\n",
                    code);
        else
            lsapi = 1;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, wdir, -1, fdir, sizeof(fdir), NULL, NULL) <= 0
        || MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 64) <= 0)
        return 2;
    if (!conf_safe(fdir))
    {
        fprintf(stderr, "the PHP folder %ls has { or } in its path and cannot be used\n", wdir);
        return 2;
    }
    for (q = fdir; *q; ++q)
        if (*q == '\\')
            *q = '/';
    swprintf(sock, MAX_PATH, L"%s\\tmp\\lshttpd\\%s.sock", root, wn);
    if (wcslen(sock) < 100)
        snprintf(addr, sizeof(addr), "uds://tmp/lshttpd/%s.sock", name);
    else
        snprintf(addr, sizeof(addr), "127.0.0.1:%d", tcp);
    r = snprintf(out, n,
                 "extProcessor %s {\r\n"
                 "    type                         %s\r\n"
                 "    address                      %s\r\n"
                 "    maxConns                     10\r\n"
                 "    env                          %s\r\n"
                 "    env                          PHPRC=%s\r\n"
                 "    initTimeout                  60\r\n"
                 "    retryTimeout                 0\r\n"
                 "    persistConn                  1\r\n"
                 "    respBuffer                   0\r\n"
                 "    autoStart                    1\r\n"
                 "    path                         %s/%s\r\n"
                 "    backlog                      100\r\n"
                 "    instances                    10\r\n"
                 "}\r\n",
                 name, lsapi ? "lsapi" : "fcgi", addr, lsapi ? "LSAPI_MAX_REQS=10000" : "PHP_FCGI_MAX_REQUESTS=0",
                 fdir, fdir, lsapi ? "lsphp.exe" : "php-cgi.exe");
    if (r <= 0 || (size_t)r >= n)
        return 2;
    *pmajor = major;
    *pminor = minor;
    *pts = ts;
    *plsapi = lsapi;
    return 0;
}

int ols_set_php(const char *dir)
{
    wchar_t root[MAX_PATH], wdir[MAX_PATH], full[MAX_PATH], conf[MAX_PATH];
    char *text = NULL, *out, block[4608], proc[4096];
    size_t len;
    int major = 0, minor = 0, ts = 0, lsapi = 0, rc;
    if (root_dir(root, MAX_PATH) < 0 || MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH) <= 0)
        return 2;
    GetFullPathNameW(wdir, MAX_PATH, full, NULL);
    wcscpy(wdir, full);
    len = wcslen(wdir);
    while (len > 3 && (wdir[len - 1] == L'\\' || wdir[len - 1] == L'/'))
        wdir[--len] = 0;
    if ((rc = php_proc(root, wdir, "lsphp", 9100, proc, sizeof(proc), &major, &minor, &ts, &lsapi, 0)) != 0)
        return rc;
    snprintf(block, sizeof(block),
             PHP_BEGIN "\r\n"
             "%s"
             "\r\n"
             "scriptHandler {\r\n"
             "    add                          %s:lsphp php\r\n"
             "}\r\n"
             PHP_END "\r\n",
             proc, lsapi ? "lsapi" : "fcgi");
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (read_file(conf, &text, &len) != 0 || !text)
        return 1;
    out = replace_block(text, PHP_BEGIN, PHP_END, block);
    free(text);
    if (!out || write_file(conf, out, strlen(out)) != 0)
    {
        free(out);
        fprintf(stderr, "cannot write configuration\n");
        return 1;
    }
    free(out);
    mail_sync(root);
    printf("PHP %d.%d %s via %s: %s\n", major, minor, ts ? "TS" : "NTS", lsapi ? "LSAPI (lsphp)" : "FastCGI (php-cgi)", dir);
    return 0;
}

static int rt_natcmp(const wchar_t *a, const wchar_t *b)
{
    while (*a && *b)
    {
        if (iswdigit(*a) && iswdigit(*b))
        {
            unsigned long x = wcstoul(a, (wchar_t **)&a, 10), y = wcstoul(b, (wchar_t **)&b, 10);
            if (x != y)
                return x < y ? -1 : 1;
            continue;
        }
        if (towlower(*a) != towlower(*b))
            return towlower(*a) < towlower(*b) ? -1 : 1;
        ++a;
        ++b;
    }
    return *a ? 1 : *b ? -1 : 0;
}

static int rt_pick(const wchar_t *pattern, const wchar_t *inner, wchar_t *best)
{
    WIN32_FIND_DATAW fd;
    wchar_t base[MAX_PATH], cand[MAX_PATH], name[MAX_PATH] = L"", *s;
    HANDLE h;
    wcscpy(base, pattern);
    if ((s = wcsrchr(base, L'\\')) == NULL)
        return 0;
    *s = 0;
    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.')
            continue;
        swprintf(cand, MAX_PATH, L"%s\\%s\\%s", base, fd.cFileName, inner);
        if (phpd_exists(cand) && (!name[0] || rt_natcmp(fd.cFileName, name) > 0))
        {
            wcscpy(name, fd.cFileName);
            wcscpy(best, cand);
        }
    }
    while (FindNextFileW(h, &fd));
    FindClose(h);
    return name[0] != 0;
}

int ols_find_runtime(const char *exe, char *out, int outlen)
{
    wchar_t w[MAX_PATH] = L"", wexe[64], d[MAX_PATH], pat[MAX_PATH], env[MAX_PATH];
    DWORD n;
    int node;
    char *p;
    if (MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, 64) <= 0)
        return -1;
    node = _wcsicmp(wexe, L"node.exe") == 0;
    n = ols_path_search(wexe, w) == 0 ? (DWORD)wcslen(w) : 0;
    if (n == 0 || n >= MAX_PATH || wcsstr(w, L"\\WindowsApps\\"))
    {
        w[0] = 0;
        if (phpd_find_laragon(d))
        {
            swprintf(pat, MAX_PATH, L"%s\\bin\\%s\\*", d, node ? L"nodejs" : L"python");
            rt_pick(pat, wexe, w);
        }
        if (!w[0] && node && GetEnvironmentVariableW(L"ProgramFiles", env, MAX_PATH))
        {
            swprintf(pat, MAX_PATH, L"%s\\nodejs\\node.exe", env);
            if (phpd_exists(pat))
                wcscpy(w, pat);
        }
        if (!w[0] && !node)
        {
            if (GetEnvironmentVariableW(L"LOCALAPPDATA", env, MAX_PATH))
            {
                swprintf(pat, MAX_PATH, L"%s\\Programs\\Python\\Python3*", env);
                rt_pick(pat, wexe, w);
            }
            if (!w[0] && GetEnvironmentVariableW(L"ProgramFiles", env, MAX_PATH))
            {
                swprintf(pat, MAX_PATH, L"%s\\Python3*", env);
                rt_pick(pat, wexe, w);
            }
            if (!w[0] && GetEnvironmentVariableW(L"SystemDrive", env, MAX_PATH))
            {
                swprintf(pat, MAX_PATH, L"%s\\Python3*", env);
                rt_pick(pat, wexe, w);
            }
        }
        if (!w[0])
            return -1;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, outlen, NULL, NULL) <= 0)
        return -1;
    for (p = out; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return 0;
}
