#include "compat_internal.h"
#include <winsvc.h>
#include "phpdetect.h"

static void jstr(const char *s)
{
    putchar('"');
    for (; *s; ++s)
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

static void jwstr(const wchar_t *w)
{
    char buf[MAX_PATH * 3];
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL) <= 0)
        buf[0] = 0;
    jstr(buf);
}

static int read_text(const wchar_t *path, char **out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got = 0;
    *out = NULL;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > (8u << 20) || (*out = (char *)malloc(size + 1)) == NULL
        || !ReadFile(h, *out, size, &got, NULL))
    {
        CloseHandle(h);
        free(*out);
        *out = NULL;
        return -1;
    }
    CloseHandle(h);
    (*out)[got] = 0;
    return 0;
}

static int listen_port(const wchar_t *file, const char *anchor, int def)
{
    char *t = NULL, *p, *q, *c = NULL;
    int port = def;
    if (read_text(file, &t) != 0)
        return def;
    if ((p = strstr(t, anchor)) != NULL && (p = strstr(p, "address")) != NULL)
    {
        for (q = p; *q && *q != '\r' && *q != '\n'; ++q)
            if (*q == ':')
                c = q;
        if (c)
            port = atoi(c + 1);
    }
    free(t);
    return port;
}

static int server_running(const wchar_t *root, int *service)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT), svc;
    wchar_t pidf[MAX_PATH];
    char *t = NULL;
    DWORD pid, code = 0;
    HANDLE h;
    *service = 0;
    if (scm)
    {
        if ((svc = OpenServiceW(scm, L"OpenLiteSpeed", SERVICE_QUERY_STATUS)) != NULL)
        {
            SERVICE_STATUS st;
            int r = QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_RUNNING;
            *service = 1;
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return r;
        }
        CloseServiceHandle(scm);
    }
    swprintf(pidf, MAX_PATH, L"%s\\tmp\\lshttpd\\lshttpd.pid", root);
    if (read_text(pidf, &t) != 0)
        return 0;
    pid = (DWORD)atoi(t);
    free(t);
    if (!pid || (h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) == NULL)
        return 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return code == STILL_ACTIVE;
}

static void php_info(const wchar_t *root)
{
    wchar_t conf[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH];
    char *t = NULL, *p, *e, ver[32] = "";
    int ma, mi, pa;
    dir[0] = 0;
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (read_text(conf, &t) == 0)
    {
        if ((p = strstr(t, "# php begin")) != NULL && (p = strstr(p, "PHPRC=")) != NULL)
        {
            p += 6;
            for (e = p; *e && *e != '\r' && *e != '\n' && *e != '"'; ++e)
                ;
            *e = 0;
            MultiByteToWideChar(CP_UTF8, 0, p, -1, dir, MAX_PATH);
        }
        free(t);
    }
    for (wchar_t *q = dir; *q; ++q)
        if (*q == L'/')
            *q = L'\\';
    swprintf(exe, MAX_PATH, L"%s\\php-cgi.exe", dir);
    if (dir[0] && phpd_version(exe, &ma, &mi, &pa) == 0)
        snprintf(ver, sizeof(ver), "%d.%d.%d", ma, mi, pa);
    printf("\"php\":{\"dir\":");
    jwstr(dir);
    printf(",\"version\":");
    jstr(ver);
    printf("}");
}

static void sites(const wchar_t *root, const wchar_t *ini, const wchar_t *www)
{
    wchar_t conf[MAX_PATH], wn[64], f[MAX_PATH];
    char *t = NULL, *p, *end, *line, *next, tmpl[96] = "", member[64] = "", url[160];
    int first = 1;
    printf("\"sites\":[");
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (read_text(conf, &t) == 0 && (p = strstr(t, "# dev-sites begin")) != NULL
        && (end = strstr(p, "# dev-sites end")) != NULL)
    {
        *end = 0;
        for (line = p; line && *line; line = next)
        {
            char *e = strchr(line, '\n'), name[96], dom[160];
            next = e ? e + 1 : NULL;
            if (e)
                *e = 0;
            while (*line == ' ' || *line == '\t')
                ++line;
            if (sscanf(line, "vhTemplate %95s", name) == 1)
                strcpy(tmpl, name);
            else if (sscanf(line, "member %63s", name) == 1)
                strcpy(member, name);
            else if (member[0] && sscanf(line, "vhDomain %159s", dom) == 1)
            {
                const char *type = "folder";
                int port = 0, tun;
                if (strcmp(member, "localhost") == 0)
                {
                    member[0] = 0;
                    continue;
                }
                MultiByteToWideChar(CP_UTF8, 0, member, -1, wn, 64);
                if (strcmp(tmpl, "devtools") == 0)
                    type = "tool";
                else if (strncmp(tmpl, "devproxy-", 9) == 0)
                {
                    type = "proxy";
                    port = GetPrivateProfileIntW(L"proxy", wn, 0, ini);
                }
                else
                {
                    swprintf(f, MAX_PATH, L"%s\\%s\\wp-settings.php", www, wn);
                    if (GetFileAttributesW(f) != INVALID_FILE_ATTRIBUTES)
                        type = "wordpress";
                }
                tun = ols_tunnel_query(member, url, sizeof(url));
                printf("%s{\"name\":", first ? "" : ",");
                jstr(member);
                printf(",\"domain\":");
                jstr(dom);
                printf(",\"type\":\"%s\",\"port\":%d,\"tunnel\":%s,\"tunnelUrl\":", type, port, tun ? "true" : "false");
                jstr(url);
                printf("}");
                first = 0;
                member[0] = 0;
            }
        }
    }
    free(t);
    printf("]");
}

int ols_status_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH], www[MAX_PATH], def[MAX_PATH], tld[32], ed[16], conf[MAX_PATH], admin[MAX_PATH];
    int service, dbi, dbr, dbp, ti, tt, tn;
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    (void)argc;
    (void)argv;
    if (n == 0 || n >= MAX_PATH)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    swprintf(def, MAX_PATH, L"%s\\www", root);
    GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
    GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, ini);
    GetPrivateProfileStringW(L"tray", L"edition", L"server", ed, 16, ini);
    if (wcscmp(ed, L"full") != 0)
        wcscpy(ed, L"server");
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    swprintf(admin, MAX_PATH, L"%s\\admin\\conf\\admin_config.conf", root);
    printf("{\"edition\":");
    jwstr(ed);
    printf(",\"server\":{\"running\":%s", server_running(root, &service) ? "true" : "false");
    printf(",\"service\":%s,\"http\":%d,\"https\":%d,\"admin\":%d},", service ? "true" : "false",
           listen_port(conf, "listener Default", 80), GetPrivateProfileIntW(L"sites", L"https", 0, ini),
           listen_port(admin, "listener", 7080));
    php_info(root);
    printf(",\"tld\":");
    jwstr(tld);
    printf(",\"www\":");
    jwstr(www);
    printf(",");
    sites(root, ini, www);
    ols_db_summary(&dbi, &dbr, &dbp);
    printf(",\"db\":{\"installed\":%s,\"running\":%s,\"port\":%d}", dbi ? "true" : "false", dbr ? "true" : "false", dbp);
    ols_tunnel_summary(&ti, &tt, &tn);
    printf(",\"tunnel\":{\"installed\":%s,\"token\":%s,\"named\":%s}}\n", ti ? "true" : "false", tt ? "true" : "false",
           tn ? "true" : "false");
    return 0;
}
