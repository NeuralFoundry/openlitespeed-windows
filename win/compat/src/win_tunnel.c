#include "compat_internal.h"
#undef NOCRYPT
#include <wincrypt.h>
#include <bcrypt.h>

#define CFD_VERSION "2026.10.7"
#define CFD_URL L"https://github.com/cloudflare/cloudflared/releases/download/2026.10.7/cloudflared-windows-amd64.exe"
#define CFD_SHA256 "2493dbc168923bb3e59e5e7716f71deced92992eea9acd6e9d74f84ac7751343"
#define CFD_SIZE 55390512LL
#define NAMED L"_named"

typedef struct
{
    wchar_t root[MAX_PATH], exe[MAX_PATH], dir[MAX_PATH], ini[MAX_PATH], token[MAX_PATH];
} tun_t;

static int tun_paths(tun_t *t)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", t->root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return -1;
    while (n > 3 && (t->root[n - 1] == L'\\' || t->root[n - 1] == L'/'))
        t->root[--n] = 0;
    swprintf(t->exe, MAX_PATH, L"%s\\bin\\cloudflared.exe", t->root);
    swprintf(t->dir, MAX_PATH, L"%s\\tmp\\tunnel", t->root);
    swprintf(t->ini, MAX_PATH, L"%s\\conf\\olstray.ini", t->root);
    swprintf(t->token, MAX_PATH, L"%s\\data\\tunnel.token", t->root);
    return 0;
}

static int exists(const wchar_t *p)
{
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static int name_ok(const char *s)
{
    size_t n = strlen(s), i;
    if (n == 0 || n > 63 || s[0] == '-' || s[n - 1] == '-')
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-'))
            return 0;
    return strcmp(s, "localhost") != 0 && strcmp(s, "phpmyadmin") != 0;
}

static int run_wait(const wchar_t *exe, wchar_t *cmd, DWORD timeout)
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

static int sha256_file(const wchar_t *path, char hex[65], long long *size)
{
    static UCHAR buf[1 << 16];
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hh = NULL;
    UCHAR md[32];
    DWORD got;
    LARGE_INTEGER sz;
    int rc = -1, i;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (GetFileSizeEx(h, &sz) && BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0
        && BCryptCreateHash(alg, &hh, NULL, 0, NULL, 0, 0) == 0)
    {
        while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got)
            BCryptHashData(hh, buf, got, 0);
        if (BCryptFinishHash(hh, md, sizeof(md), 0) == 0)
        {
            for (i = 0; i < 32; ++i)
                snprintf(hex + i * 2, 3, "%02x", md[i]);
            *size = sz.QuadPart;
            rc = 0;
        }
    }
    if (hh)
        BCryptDestroyHash(hh);
    if (alg)
        BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(h);
    return rc;
}

static int install(const tun_t *t)
{
    wchar_t sys[MAX_PATH], curl[MAX_PATH], tmp[MAX_PATH], cmd[MAX_PATH * 4];
    char hex[65];
    long long size = 0;
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return 1;
    swprintf(curl, MAX_PATH, L"%s\\curl.exe", sys);
    swprintf(tmp, MAX_PATH, L"%s\\tmp\\cloudflared.download", t->root);
    DeleteFileW(tmp);
    swprintf(cmd, MAX_PATH * 4,
             L"\"%s\" -fsS --proto =https --tlsv1.2 --connect-timeout 20 --max-time 900 -L -o \"%s\" %s", curl, tmp,
             CFD_URL);
    printf("downloading cloudflared %s...\n", CFD_VERSION);
    fflush(stdout);
    if (run_wait(curl, cmd, 960000) != 0)
    {
        DeleteFileW(tmp);
        fprintf(stderr, "could not download cloudflared\n");
        return 1;
    }
    if (sha256_file(tmp, hex, &size) != 0 || size != CFD_SIZE || strcmp(hex, CFD_SHA256) != 0)
    {
        DeleteFileW(tmp);
        fprintf(stderr, "cloudflared failed the checksum check\n");
        return 1;
    }
    if (!MoveFileExW(tmp, t->exe, MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(tmp);
        fprintf(stderr, "cannot install %ls\n", t->exe);
        return GetLastError() == ERROR_ACCESS_DENIED ? 5 : 1;
    }
    printf("cloudflared %s installed\n", CFD_VERSION);
    return 0;
}

static int ready(const tun_t *t)
{
    char hex[65];
    long long size = 0;
    return exists(t->exe) && sha256_file(t->exe, hex, &size) == 0 && size == CFD_SIZE && strcmp(hex, CFD_SHA256) == 0;
}

static int site_exists(const tun_t *t, const char *site)
{
    wchar_t w[64], def[MAX_PATH], www[MAX_PATH], dir[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, site, -1, w, 64) <= 0)
        return 0;
    if (GetPrivateProfileIntW(L"proxy", w, 0, t->ini) > 0)
        return 1;
    swprintf(def, MAX_PATH, L"%s\\www", t->root);
    GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, t->ini);
    swprintf(dir, MAX_PATH, L"%s\\%s", www, w);
    return (GetFileAttributesW(dir) & FILE_ATTRIBUTE_DIRECTORY) && exists(dir);
}

static int trust_loopback(const tun_t *t)
{
    wchar_t conf[MAX_PATH];
    char *text = NULL, *ac, *line, *eol, *close, *out, cur[512];
    size_t len, pre, n;
    int rc = 1;
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", t->root);
    if (ols_read_wfile(conf, &text, &len) != 0)
        return -1;
    ac = strstr(text, "\naccessControl {");
    if (!ac || (close = strchr(ac + 1, '}')) == NULL || (line = strstr(ac, "allow")) == NULL || line > close
        || (eol = strpbrk(line, "\r\n")) == NULL || eol - line >= (int)sizeof(cur))
    {
        free(text);
        return -1;
    }
    memcpy(cur, line, (size_t)(eol - line));
    cur[eol - line] = 0;
    if (strstr(cur, "127.0.0.1T"))
    {
        free(text);
        return 0;
    }
    n = (size_t)(eol - line);
    while (n && (line[n - 1] == ' ' || line[n - 1] == '\t'))
        --n;
    if (n < 3 || strncmp(line + n - 3, "ALL", 3) != 0)
    {
        free(text);
        fprintf(stderr, "add 127.0.0.1T to the server access control so visitors keep their own IP\n");
        return 2;
    }
    pre = (size_t)(line - text) + n;
    if ((out = (char *)malloc(len + 16)) == NULL)
    {
        free(text);
        return -1;
    }
    memcpy(out, text, pre);
    memcpy(out + pre, ", 127.0.0.1T", 12);
    memcpy(out + pre + 12, text + pre, len - pre);
    if (ols_write_wfile(conf, out, len + 12, 0) != 0)
        rc = -1;
    free(out);
    free(text);
    return rc;
}

static void state_path(const tun_t *t, const wchar_t *site, const wchar_t *ext, wchar_t *out)
{
    swprintf(out, MAX_PATH, L"%s\\%s.%s", t->dir, site, ext);
}

static DWORD running_pid(const tun_t *t, const wchar_t *site)
{
    wchar_t pidf[MAX_PATH], img[MAX_PATH];
    char *s = NULL;
    size_t n;
    DWORD pid = 0, len = MAX_PATH;
    HANDLE h;
    state_path(t, site, L"pid", pidf);
    if (ols_read_wfile(pidf, &s, &n) != 0)
        return 0;
    pid = (DWORD)strtoul(s, NULL, 10);
    free(s);
    if (!pid || (h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)) == NULL)
        return 0;
    if (WaitForSingleObject(h, 0) != WAIT_TIMEOUT || !QueryFullProcessImageNameW(h, 0, img, &len)
        || _wcsicmp(img, t->exe) != 0)
        pid = 0;
    CloseHandle(h);
    return pid;
}

static int read_shared(const wchar_t *path, char **out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got = 0;
    *out = NULL;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > (4u << 20) || (*out = (char *)malloc(size + 1)) == NULL
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

static int find_url(const char *log, char *url, size_t n)
{
    const char *p = log;
    while ((p = strstr(p, "https://")) != NULL)
    {
        const char *q = p + 8;
        size_t len;
        while ((*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') || *q == '-' || *q == '.')
            ++q;
        len = (size_t)(q - p);
        if (len > 26 && len < n && strncmp(q - 18, ".trycloudflare.com", 18) == 0)
        {
            memcpy(url, p, len);
            url[len] = 0;
            return 0;
        }
        p = q;
    }
    return -1;
}

static int spawn(const tun_t *t, const wchar_t *site, wchar_t *args, const char *token)
{
    wchar_t cmd[MAX_PATH * 4], pidf[MAX_PATH], logf[MAX_PATH], *env = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    char buf[32];
    int n, ok;
    CreateDirectoryW(t->dir, NULL);
    state_path(t, site, L"log", logf);
    DeleteFileW(logf);
    swprintf(cmd, MAX_PATH * 4, L"\"%s\" tunnel --no-autoupdate --logfile \"%s\" %s", t->exe, logf, args);
    if (token)
    {
        wchar_t *cur = GetEnvironmentStringsW(), *e;
        size_t total = 0, tl = strlen(token), pos = 0;
        for (e = cur; e && *e; e += wcslen(e) + 1)
            total += wcslen(e) + 1;
        env = (wchar_t *)calloc(total + tl + 32, sizeof(wchar_t));
        if (!env)
        {
            FreeEnvironmentStringsW(cur);
            return -1;
        }
        for (e = cur; e && *e; e += wcslen(e) + 1)
            if (_wcsnicmp(e, L"TUNNEL_TOKEN=", 13) != 0)
            {
                wcscpy(env + pos, e);
                pos += wcslen(e) + 1;
            }
        FreeEnvironmentStringsW(cur);
        wcscpy(env + pos, L"TUNNEL_TOKEN=");
        pos += 13;
        MultiByteToWideChar(CP_UTF8, 0, token, -1, env + pos, (int)(tl + 1));
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    ok = CreateProcessW(t->exe, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | (env ? CREATE_UNICODE_ENVIRONMENT : 0), env, NULL,
                        &si, &pi);
    if (env)
    {
        SecureZeroMemory(env, (wcslen(env) + 1) * sizeof(wchar_t));
        free(env);
    }
    if (!ok)
        return -1;
    n = snprintf(buf, sizeof(buf), "%lu\n", (unsigned long)pi.dwProcessId);
    state_path(t, site, L"pid", pidf);
    ols_write_wfile(pidf, buf, (size_t)n, 0);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

static int stop_one(const tun_t *t, const wchar_t *site)
{
    wchar_t f[MAX_PATH];
    DWORD pid = running_pid(t, site);
    if (pid)
    {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (h)
        {
            TerminateProcess(h, 0);
            WaitForSingleObject(h, 5000);
            CloseHandle(h);
        }
    }
    state_path(t, site, L"pid", f);
    DeleteFileW(f);
    state_path(t, site, L"url", f);
    DeleteFileW(f);
    return pid ? 1 : 0;
}

static int start_quick(const tun_t *t, const char *site)
{
    wchar_t w[64], args[512], tld[32], urlf[MAX_PATH], logf[MAX_PATH];
    char url[160], *log = NULL;
    size_t n;
    int i, r;
    if (!name_ok(site) || !site_exists(t, site) || MultiByteToWideChar(CP_UTF8, 0, site, -1, w, 64) <= 0)
    {
        fprintf(stderr, "there is no site named %s that can be shared\n", site);
        return 2;
    }
    if (running_pid(t, w))
    {
        state_path(t, w, L"url", urlf);
        if (ols_read_wfile(urlf, &log, &n) == 0)
        {
            printf("URL %.*s\n", (int)strcspn(log, "\r\n"), log);
            free(log);
            return 0;
        }
        stop_one(t, w);
    }
    if (!ready(t) && (r = install(t)) != 0)
        return r;
    if ((r = trust_loopback(t)) < 0)
        fprintf(stderr, "could not check the server access control\n");
    else if (r == 1)
        ols_win_control("reload");
    GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, t->ini);
    swprintf(args, 512, L"--url http://127.0.0.1:%d --http-host-header %s.%s", ols_default_port(t->root), w, tld);
    if (spawn(t, w, args, NULL) != 0)
    {
        fprintf(stderr, "cannot start cloudflared\n");
        return 1;
    }
    state_path(t, w, L"log", logf);
    for (i = 0; i < 120; ++i)
    {
        Sleep(500);
        if (read_shared(logf, &log) == 0)
        {
            r = find_url(log, url, sizeof(url));
            free(log);
            log = NULL;
            if (r == 0)
            {
                state_path(t, w, L"url", urlf);
                ols_write_wfile(urlf, url, strlen(url), 0);
                printf("URL %s\n", url);
                return 0;
            }
        }
        if (!running_pid(t, w))
            break;
    }
    stop_one(t, w);
    fprintf(stderr, "Cloudflare did not give a tunnel address; see tmp\\tunnel\\%s.log\n", site);
    return 1;
}

static size_t read_input(char *line, size_t cap)
{
    unsigned char raw[8192];
    size_t got = fread(raw, 1, sizeof(raw) - 2, stdin), n = 0, i, skip = 0;
    if (got >= 2 && ((raw[0] == 0xFF && raw[1] == 0xFE) || (raw[1] == 0 && raw[0] != 0)))
    {
        skip = (raw[0] == 0xFF && raw[1] == 0xFE) ? 2 : 0;
        raw[got] = raw[got + 1] = 0;
        n = (size_t)WideCharToMultiByte(CP_UTF8, 0, (const wchar_t *)(raw + skip), (int)((got - skip) / 2), line,
                                        (int)cap - 1, NULL, NULL);
    }
    else
    {
        if (got >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF)
            skip = 3;
        n = got - skip < cap - 1 ? got - skip : cap - 1;
        memcpy(line, raw + skip, n);
    }
    SecureZeroMemory(raw, sizeof(raw));
    line[n] = 0;
    for (i = 0; i < n && line[i] == ' '; ++i)
        ;
    if (i)
    {
        memmove(line, line + i, n - i + 1);
        n -= i;
    }
    n = strcspn(line, "\r\n \t");
    line[n] = 0;
    return n;
}

static int token_set(const tun_t *t)
{
    char line[4096];
    DATA_BLOB in, out;
    size_t n, i;
    int rc = 1;
    n = read_input(line, sizeof(line));
    for (i = 0; i < n; ++i)
        if (!((line[i] >= 'a' && line[i] <= 'z') || (line[i] >= 'A' && line[i] <= 'Z') || (line[i] >= '0' && line[i] <= '9')
              || line[i] == '+' || line[i] == '/' || line[i] == '=' || line[i] == '-' || line[i] == '_'))
            break;
    if (n < 32 || i != n)
    {
        SecureZeroMemory(line, sizeof(line));
        fprintf(stderr, "that does not look like a Cloudflare tunnel token\n");
        return 2;
    }
    {
        wchar_t data[MAX_PATH];
        swprintf(data, MAX_PATH, L"%s\\data", t->root);
        if (ols_private_dir(data) != 0)
        {
            SecureZeroMemory(line, sizeof(line));
            fprintf(stderr, "cannot prepare a private folder for the token\n");
            return 1;
        }
    }
    in.pbData = (BYTE *)line;
    in.cbData = (DWORD)n;
    if (CryptProtectData(&in, L"OpenLiteSpeed tunnel token", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        rc = ols_write_wfile(t->token, out.pbData, out.cbData, 1) == 0 ? 0 : 1;
        LocalFree(out.pbData);
    }
    SecureZeroMemory(line, sizeof(line));
    if (rc == 0)
        printf("tunnel token saved\n");
    else
        fprintf(stderr, "could not save the tunnel token\n");
    return rc;
}

static int start_named(const tun_t *t)
{
    char *blob = NULL, *tok;
    size_t n;
    DATA_BLOB in, out;
    wchar_t args[16] = L"run";
    int r;
    if (running_pid(t, NAMED))
    {
        printf("the tunnel is running\n");
        return 0;
    }
    if (ols_read_wfile(t->token, &blob, &n) != 0)
    {
        fprintf(stderr, "no tunnel token saved; run --tunnel token and paste it\n");
        return 2;
    }
    in.pbData = (BYTE *)blob;
    in.cbData = (DWORD)n;
    if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        free(blob);
        fprintf(stderr, "the saved token cannot be read by this user\n");
        return 1;
    }
    free(blob);
    if ((tok = (char *)calloc(out.cbData + 1, 1)) == NULL)
    {
        SecureZeroMemory(out.pbData, out.cbData);
        LocalFree(out.pbData);
        return 1;
    }
    memcpy(tok, out.pbData, out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (!ready(t) && (r = install(t)) != 0)
    {
        SecureZeroMemory(tok, strlen(tok));
        free(tok);
        return r;
    }
    if (trust_loopback(t) == 1)
        ols_win_control("reload");
    r = spawn(t, NAMED, args, tok);
    SecureZeroMemory(tok, strlen(tok));
    free(tok);
    if (r != 0)
    {
        fprintf(stderr, "cannot start cloudflared\n");
        return 1;
    }
    Sleep(3000);
    if (!running_pid(t, NAMED))
    {
        fprintf(stderr, "cloudflared stopped; see tmp\\tunnel\\_named.log\n");
        return 1;
    }
    printf("the tunnel is running\n");
    return 0;
}

static int status(const tun_t *t)
{
    wchar_t pat[MAX_PATH], site[64], urlf[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    char *url;
    size_t n;
    printf("cloudflared %s\n", ready(t) ? "installed" : "not installed");
    printf("token %s\n", exists(t->token) ? "saved" : "none");
    swprintf(pat, MAX_PATH, L"%s\\*.pid", t->dir);
    if ((f = FindFirstFileW(pat, &fd)) == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        wchar_t *dot = wcsrchr(fd.cFileName, L'.');
        size_t l = dot ? (size_t)(dot - fd.cFileName) : 0;
        if (!l || l >= 64)
            continue;
        wcsncpy(site, fd.cFileName, l);
        site[l] = 0;
        if (!running_pid(t, site))
            continue;
        if (wcscmp(site, NAMED) == 0)
        {
            printf("named running\n");
            continue;
        }
        state_path(t, site, L"url", urlf);
        if (ols_read_wfile(urlf, &url, &n) == 0)
        {
            printf("site %ls %.*s\n", site, (int)strcspn(url, "\r\n"), url);
            free(url);
        }
        else
            printf("site %ls starting\n", site);
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
    return 0;
}

int ols_tunnel_main(int argc, char **argv)
{
    tun_t t;
    const char *act = argc >= 1 ? argv[0] : "status";
    if (tun_paths(&t) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    if (strcmp(act, "install") == 0)
        return ready(&t) ? 0 : install(&t);
    if (strcmp(act, "start") == 0 && argc >= 2)
        return start_quick(&t, argv[1]);
    if (strcmp(act, "stop") == 0 && argc >= 2)
    {
        wchar_t w[64];
        if (strcmp(argv[1], "--all") == 0)
        {
            wchar_t pat[MAX_PATH];
            WIN32_FIND_DATAW fd;
            HANDLE f;
            swprintf(pat, MAX_PATH, L"%s\\*.pid", t.dir);
            if ((f = FindFirstFileW(pat, &fd)) != INVALID_HANDLE_VALUE)
            {
                do
                {
                    wchar_t *dot = wcsrchr(fd.cFileName, L'.');
                    if (dot && dot - fd.cFileName < 64)
                    {
                        *dot = 0;
                        stop_one(&t, fd.cFileName);
                    }
                }
                while (FindNextFileW(f, &fd));
                FindClose(f);
            }
            return 0;
        }
        if (!name_ok(argv[1]) || MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, w, 64) <= 0)
            return 2;
        stop_one(&t, w);
        return 0;
    }
    if (strcmp(act, "token") == 0)
    {
        if (argc >= 2 && strcmp(argv[1], "clear") == 0)
        {
            stop_one(&t, NAMED);
            DeleteFileW(t.token);
            return 0;
        }
        return token_set(&t);
    }
    if (strcmp(act, "run") == 0)
        return start_named(&t);
    if (strcmp(act, "run-stop") == 0)
    {
        stop_one(&t, NAMED);
        return 0;
    }
    if (strcmp(act, "status") == 0)
        return status(&t);
    fprintf(stderr, "usage: openlitespeed --tunnel install | start <site> | stop <site>|--all | status | "
                    "token [clear] | run | run-stop\n");
    return 2;
}
