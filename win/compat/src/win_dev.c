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

static char *build_template(const wchar_t *root, int with_aliases)
{
    char *out = NULL;
    size_t len = 0, cap = 0;
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    const wchar_t *laragon[2], *xampp[2];
    wchar_t lr[MAX_PATH], xr[MAX_PATH], lr2[MAX_PATH], xr2[MAX_PATH], d[MAX_PATH], ini[MAX_PATH], stack[16];
    int i, use_lr, use_xr;
    buf_add(&out, &len, &cap, s_template, sizeof(s_template) - 1 - 3);
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
    wchar_t root[MAX_PATH], conf[MAX_PATH], tpl[MAX_PATH], wwww[MAX_PATH], pat[MAX_PATH], p[MAX_PATH];
    char *text = NULL, *out, *block = NULL, name[256], wwwfwd[MAX_PATH * 3], line[1024];
    size_t len, blen = 0, bcap = 0;
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int count = 0;
    char *q;
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

    buf_str(&block, &blen, &bcap, DEV_BEGIN "\r\nvhTemplate devsites {\r\n"
            "    templateFile             conf/templates/devsite.conf\r\n"
            "    listeners                Default\r\n");
    if (with_root)
    {
        snprintf(line, sizeof(line),
                 "    member localhost {\r\n        vhDomain             localhost\r\n"
                 "        vhAliases            127.0.0.1, *\r\n        vhRoot               %s/\r\n    }\r\n",
                 wwwfwd);
        buf_str(&block, &blen, &bcap, line);
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
            swprintf(p, MAX_PATH, L"%s\\%s\\public\\index.php", wwww, fd.cFileName);
            if (file_exists(p))
                sub = "public/";
            snprintf(line, sizeof(line),
                     "    member %s {\r\n        vhDomain             %s.%s\r\n"
                     "        vhAliases            www.%s.%s\r\n        vhRoot               %s/%s/%s\r\n    }\r\n",
                     name, name, tld, name, tld, wwwfwd, name, sub);
            buf_str(&block, &blen, &bcap, line);
            printf("http://%s.%s/  ->  %s/%s/%s\n", name, tld, wwwfwd, name, sub);
            ++count;
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    buf_str(&block, &blen, &bcap, "}\r\n" DEV_END "\r\n");
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
             L"\"%s\\curl.exe\" -fsS --proto =https --tlsv1.2 -L -o \"%s\" https://wordpress.org/latest.zip",
             sys, zip);
    printf("downloading WordPress...\n");
    if (run_wait(cmd) != 0)
    {
        fprintf(stderr, "could not download WordPress\n");
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

int ols_new_site(const char *name, const char *tld, const char *flavor)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH], www[MAX_PATH], dir[MAX_PATH], idx[MAX_PATH], wname[128];
    char wwwmb[MAX_PATH * 3], html[512];
    int withRoot, rc, n, wp;
    if (!name || !valid_label(name) || strcmp(name, "localhost") == 0)
    {
        fprintf(stderr, "invalid site name (use lowercase letters, digits and '-')\n");
        return 2;
    }
    if (!tld || !*tld)
        tld = "test";
    if (!valid_label(tld))
    {
        fprintf(stderr, "invalid tld\n");
        return 2;
    }
    if (root_dir(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    GetPrivateProfileStringW(L"sites", L"www", L"", www, MAX_PATH, ini);
    if (!www[0] || !dir_exists(www))
    {
        fprintf(stderr, "no web root is configured; set one from the tray first\n");
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

int ols_dev_hosts(int clear)
{
    wchar_t root[MAX_PATH], conf[MAX_PATH], hosts[MAX_PATH];
    char *text = NULL, *htext = NULL, *block = NULL, *out, rootfwd[MAX_PATH * 3], tb[MAX_PATH * 3 + 64],
        te[MAX_PATH * 3 + 64], *q;
    const char *p, *end;
    size_t len, blen = 0, bcap = 0;
    UINT n;
    int changed;
    if (root_dir(root, MAX_PATH) < 0
        || WideCharToMultiByte(CP_UTF8, 0, root, -1, rootfwd, sizeof(rootfwd), NULL, NULL) <= 0)
        return 2;
    for (q = rootfwd; *q; ++q)
        if (*q == '\\')
            *q = '/';
    snprintf(tb, sizeof(tb), "%s [%s]", HOSTS_BEGIN, rootfwd);
    snprintf(te, sizeof(te), "%s [%s]", HOSTS_END, rootfwd);
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
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
        if (!i || strcmp(dom, "localhost") == 0)
            continue;
        snprintf(alias, sizeof(alias), "127.0.0.1 %s www.%s\r\n::1 %s www.%s\r\n", dom, dom, dom, dom);
        buf_str(&block, &blen, &bcap, alias);
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
    if (clear)
    {
        block[0] = 0;
        if (!strstr(htext, tb))
        {
            free(block);
            if (!changed)
            {
                free(htext);
                return 0;
            }
            out = htext;
            goto write;
        }
    }
    out = replace_block(htext, tb, te, block);
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

int ols_set_php(const char *dir)
{
    wchar_t root[MAX_PATH], wdir[MAX_PATH], cgi[MAX_PATH], dll[MAX_PATH], lsphp[MAX_PATH],
        src[MAX_PATH], conf[MAX_PATH], sock[MAX_PATH];
    char *text = NULL, *out, block[4096], fdir[MAX_PATH * 3], *q;
    size_t len;
    int major = 0, minor = 0, ts, lsapi = 0, uds;
    DWORD code;
    if (root_dir(root, MAX_PATH) < 0 || MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH) <= 0)
        return 2;
    GetFullPathNameW(wdir, MAX_PATH, cgi, NULL);
    wcscpy(wdir, cgi);
    len = wcslen(wdir);
    while (len > 3 && (wdir[len - 1] == L'\\' || wdir[len - 1] == L'/'))
        wdir[--len] = 0;
    swprintf(cgi, MAX_PATH, L"%s\\php-cgi.exe", wdir);
    if (!file_exists(cgi) || php_version(cgi, &major, &minor) != 0)
    {
        fprintf(stderr, "php-cgi.exe not found in %s\n", dir);
        return 2;
    }
    if (!probe_php(cgi, wdir, major, minor, "(cgi-fcgi)", &code))
    {
        const char *why = "";
        if (code == 0xC0000135)
        {
            HMODULE vc = LoadLibraryExW(L"vcruntime140.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
            why = vc ? ": a DLL is missing from the PHP folder" : ": the Microsoft Visual C++ runtime is missing";
            if (vc)
                FreeLibrary(vc);
        }
        fprintf(stderr, "php-cgi.exe in %s does not start (exit code 0x%08lX)%s\n", dir, code, why);
        return 3;
    }
    swprintf(dll, MAX_PATH, L"%s\\php%dts.dll", wdir, major);
    ts = file_exists(dll);
    swprintf(src, MAX_PATH, L"%s\\bin\\lsphp\\%d.%d-%s\\lsphp.exe", root, major, minor, ts ? L"ts" : L"nts");
    swprintf(lsphp, MAX_PATH, L"%s\\lsphp.exe", wdir);
    if (file_exists(src))
    {
        if (!same_file_content(src, lsphp) && !CopyFileW(src, lsphp, FALSE))
            fprintf(stderr, "warning: cannot copy lsphp.exe into %s (error %lu), using php-cgi\n", dir,
                    GetLastError());
        else if (!probe_php(lsphp, wdir, major, minor, "(litespeed)", &code))
            fprintf(stderr, "warning: lsphp.exe does not run with this PHP build (exit code 0x%08lX), using php-cgi\n",
                    code);
        else
            lsapi = 1;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, wdir, -1, fdir, sizeof(fdir), NULL, NULL) <= 0)
        return 2;
    for (q = fdir; *q; ++q)
        if (*q == '\\')
            *q = '/';
    swprintf(sock, MAX_PATH, L"%s\\tmp\\lshttpd\\lsphp.sock", root);
    uds = wcslen(sock) < 100;
    snprintf(block, sizeof(block),
             PHP_BEGIN "\r\n"
             "extProcessor lsphp {\r\n"
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
             "}\r\n"
             "\r\n"
             "scriptHandler {\r\n"
             "    add                          %s:lsphp php\r\n"
             "}\r\n"
             PHP_END "\r\n",
             lsapi ? "lsapi" : "fcgi",
             uds ? "uds://tmp/lshttpd/lsphp.sock" : "127.0.0.1:9100",
             lsapi ? "LSAPI_MAX_REQS=10000" : "PHP_FCGI_MAX_REQUESTS=0",
             fdir, fdir, lsapi ? "lsphp.exe" : "php-cgi.exe",
             lsapi ? "lsapi" : "fcgi");
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
    n = SearchPathW(NULL, wexe, NULL, MAX_PATH, w, NULL);
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
