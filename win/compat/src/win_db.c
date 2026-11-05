#include "compat_internal.h"
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define MDB_VERSION "11.8.9"
#define MDB_URL L"https://archive.mariadb.org/mariadb-11.8.9/winx64-packages/mariadb-11.8.9-winx64.zip"
#define MDB_SHA256 "830c46727d9278eae212ae3eca44eeb9e71b2a68704e95f344a64fba7b1963f5"
#define MDB_SIZE 97016251LL
#define PMA_VERSION "5.2.3"
#define PMA_URL L"https://files.phpmyadmin.net/phpMyAdmin/5.2.3/phpMyAdmin-5.2.3-all-languages.zip"
#define PMA_SHA256 "2d2e13c735366d318425c78e4ee2cc8fc648d77faba3ddea2cd516e43885733f"
#define PMA_SIZE 16431330LL

typedef struct
{
    wchar_t root[MAX_PATH], base[MAX_PATH], bin[MAX_PATH], data[MAX_PATH], datadir[MAX_PATH], cnf[MAX_PATH];
    wchar_t myini[MAX_PATH], pid[MAX_PATH], pma[MAX_PATH], tray[MAX_PATH];
    char rootf[MAX_PATH * 3], datadirf[MAX_PATH * 3], pidf[MAX_PATH * 3], basef[MAX_PATH * 3];
} dbp_t;

static void fwd(const wchar_t *in, char *out, size_t n)
{
    char *q;
    if (WideCharToMultiByte(CP_UTF8, 0, in, -1, out, (int)n, NULL, NULL) <= 0)
        out[0] = 0;
    for (q = out; *q; ++q)
        if (*q == '\\')
            *q = '/';
}

static int db_paths(dbp_t *p)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", p->root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return -1;
    while (n > 3 && (p->root[n - 1] == L'\\' || p->root[n - 1] == L'/'))
        p->root[--n] = 0;
    swprintf(p->base, MAX_PATH, L"%s\\mariadb", p->root);
    swprintf(p->bin, MAX_PATH, L"%s\\mariadb\\bin", p->root);
    swprintf(p->data, MAX_PATH, L"%s\\data", p->root);
    swprintf(p->datadir, MAX_PATH, L"%s\\data\\mariadb", p->root);
    swprintf(p->cnf, MAX_PATH, L"%s\\data\\client.cnf", p->root);
    swprintf(p->pid, MAX_PATH, L"%s\\data\\mariadb.pid", p->root);
    swprintf(p->myini, MAX_PATH, L"%s\\conf\\mariadb.ini", p->root);
    swprintf(p->pma, MAX_PATH, L"%s\\share\\phpmyadmin", p->root);
    swprintf(p->tray, MAX_PATH, L"%s\\conf\\olstray.ini", p->root);
    fwd(p->root, p->rootf, sizeof(p->rootf));
    fwd(p->datadir, p->datadirf, sizeof(p->datadirf));
    fwd(p->pid, p->pidf, sizeof(p->pidf));
    fwd(p->base, p->basef, sizeof(p->basef));
    return 0;
}

static int wexists(const wchar_t *p)
{
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static int tool(const dbp_t *p, const wchar_t *exe, wchar_t *out)
{
    swprintf(out, MAX_PATH, L"%s\\%s", p->bin, exe);
    return wexists(out);
}

static int run_cmd(const wchar_t *exe, wchar_t *cmd, const char *input, DWORD timeout)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd = NULL, wr = NULL;
    DWORD code = 1, w;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (input)
    {
        if (!CreatePipe(&rd, &wr, &sa, 0))
            return -1;
        SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = rd;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
    if (!CreateProcessW(exe, cmd, NULL, NULL, input != NULL, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        if (rd)
            CloseHandle(rd);
        if (wr)
            CloseHandle(wr);
        return -1;
    }
    if (input)
    {
        CloseHandle(rd);
        WriteFile(wr, input, (DWORD)strlen(input), &w, NULL);
        CloseHandle(wr);
    }
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

static HANDLE g_started;

static int elevated(void)
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

static int start_detached(const wchar_t *exe, wchar_t *cmd)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
    {
        flags &= ~CREATE_BREAKAWAY_FROM_JOB;
        if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
            return -1;
    }
    CloseHandle(pi.hThread);
    if (g_started)
        CloseHandle(g_started);
    g_started = pi.hProcess;
    return 0;
}

static int file_sha256(HANDLE h, char hex[65], long long *size)
{
    SHA256_CTX c;
    static unsigned char buf[1 << 16];
    unsigned char md[SHA256_DIGEST_LENGTH];
    DWORD got;
    LARGE_INTEGER sz;
    int i;
    if (h == INVALID_HANDLE_VALUE || !GetFileSizeEx(h, &sz))
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

static int fetch(const wchar_t *url, const wchar_t *zip, const char *sha, long long size, const char *what,
                 HANDLE *lock)
{
    wchar_t sys[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH * 4];
    char hex[65];
    long long got = 0;
    *lock = INVALID_HANDLE_VALUE;
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return 1;
    swprintf(exe, MAX_PATH, L"%s\\curl.exe", sys);
    swprintf(cmd, MAX_PATH * 4,
             L"\"%s\" -q -fsS --proto =https --tlsv1.2 --connect-timeout 20 --max-time 1800 -L -o \"%s\" %s", exe, zip,
             url);
    printf("downloading %s...\n", what);
    fflush(stdout);
    if (run_cmd(exe, cmd, NULL, 1900000) != 0)
    {
        fprintf(stderr, "could not download %s\n", what);
        DeleteFileW(zip);
        return 1;
    }
    *lock = CreateFileW(zip, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file_sha256(*lock, hex, &got) != 0 || got != size || strcmp(hex, sha) != 0)
    {
        if (*lock != INVALID_HANDLE_VALUE)
            CloseHandle(*lock);
        *lock = INVALID_HANDLE_VALUE;
        fprintf(stderr, "%s failed the checksum check\n", what);
        DeleteFileW(zip);
        return 1;
    }
    return 0;
}

static int unpack(const wchar_t *zip, const wchar_t *dir)
{
    wchar_t sys[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH * 4];
    int r;
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return 1;
    swprintf(exe, MAX_PATH, L"%s\\tar.exe", sys);
    CreateDirectoryW(dir, NULL);
    swprintf(cmd, MAX_PATH * 4, L"\"%s\" -xf \"%s\" -C \"%s\" --strip-components=1", exe, zip, dir);
    r = run_cmd(exe, cmd, NULL, 600000);
    return r == 0 ? 0 : 1;
}

static int get_package(const wchar_t *dl, const wchar_t *name, const wchar_t *url, const char *sha, long long size,
                       const char *what, const wchar_t *dir)
{
    wchar_t zip[MAX_PATH];
    HANDLE lock;
    int r;
    swprintf(zip, MAX_PATH, L"%s\\%s", dl, name);
    DeleteFileW(zip);
    if (fetch(url, zip, sha, size, what, &lock) != 0)
        return 1;
    r = unpack(zip, dir);
    CloseHandle(lock);
    DeleteFileW(zip);
    if (r != 0)
        fprintf(stderr, "could not unpack %s\n", what);
    return r;
}

static int db_fetch(const dbp_t *p)
{
    wchar_t dir[MAX_PATH], dl[MAX_PATH], exe[MAX_PATH];
    int r = 0;
    swprintf(dir, MAX_PATH, L"%s\\tmp", p->root);
    CreateDirectoryW(dir, NULL);
    swprintf(dir, MAX_PATH, L"%s\\share", p->root);
    CreateDirectoryW(dir, NULL);
    swprintf(dl, MAX_PATH, L"%s\\tmp\\dl-%08lx%08lx", p->root, GetCurrentProcessId(), GetTickCount());
    if (ols_private_dir(dl) != 0)
    {
        fprintf(stderr, "cannot prepare a private download folder: %ls\n", dl);
        return 1;
    }
    if (!tool(p, L"mariadbd.exe", exe)
        && (get_package(dl, L"mariadb.zip", MDB_URL, MDB_SHA256, MDB_SIZE, "MariaDB " MDB_VERSION, p->base) != 0
            || !tool(p, L"mariadbd.exe", exe)))
        r = 1;
    swprintf(dir, MAX_PATH, L"%s\\index.php", p->pma);
    if (r == 0 && !wexists(dir)
        && get_package(dl, L"phpmyadmin.zip", PMA_URL, PMA_SHA256, PMA_SIZE, "phpMyAdmin " PMA_VERSION, p->pma) != 0)
        r = 1;
    RemoveDirectoryW(dl);
    return r;
}

static int kill_server(const dbp_t *p)
{
    wchar_t want[MAX_PATH], img[MAX_PATH];
    char *t = NULL;
    size_t n;
    DWORD pid, len = MAX_PATH;
    HANDLE h;
    int r = -1;
    if (g_started)
    {
        TerminateProcess(g_started, 1);
        WaitForSingleObject(g_started, 10000);
        CloseHandle(g_started);
        g_started = NULL;
        return 0;
    }
    if (ols_read_wfile(p->pid, &t, &n) != 0)
        return -1;
    pid = (DWORD)strtoul(t, NULL, 10);
    free(t);
    swprintf(want, MAX_PATH, L"%s\\mariadbd.exe", p->bin);
    if (!pid || (h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)) == NULL)
        return -1;
    if (QueryFullProcessImageNameW(h, 0, img, &len) && _wcsicmp(img, want) == 0 && TerminateProcess(h, 1))
    {
        WaitForSingleObject(h, 10000);
        r = 0;
    }
    CloseHandle(h);
    return r;
}

static void token(char *out, size_t n)
{
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    unsigned char b[64];
    size_t i = 0, j;
    while (i < n)
    {
        RAND_bytes(b, sizeof(b));
        for (j = 0; j < sizeof(b) && i < n; ++j)
            if (b[j] < 248)
                out[i++] = a[b[j] % 62];
    }
    out[n] = 0;
    OPENSSL_cleanse(b, sizeof(b));
}

static int db_port(const dbp_t *p)
{
    return GetPrivateProfileIntW(L"db", L"port", 0, p->tray);
}

static int installed(const dbp_t *p)
{
    wchar_t exe[MAX_PATH];
    return tool(p, L"mariadbd.exe", exe) && wexists(p->cnf) && wexists(p->datadir) && db_port(p) > 0;
}

static int client(const dbp_t *p, const wchar_t *exe_name, const wchar_t *args, const char *input)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH * 4];
    if (!tool(p, exe_name, exe))
        return -1;
    swprintf(cmd, MAX_PATH * 4, L"\"%s\" --defaults-extra-file=\"%s\" %s", exe, p->cnf, args);
    return run_cmd(exe, cmd, input, 120000);
}

static int alive(const dbp_t *p)
{
    return ols_port_listening(db_port(p)) && client(p, L"mariadb-admin.exe", L"--silent ping", NULL) == 0;
}

static int wait_port(int port, int up, int seconds)
{
    int i;
    for (i = 0; i < seconds * 4; ++i)
    {
        if (!!ols_port_listening(port) == up)
            return 0;
        Sleep(250);
    }
    return -1;
}

static int write_myini(const dbp_t *p, int port)
{
    char text[MAX_PATH * 12];
    int n = snprintf(text, sizeof(text),
                     "[mysqld]\r\nbasedir=%s\r\ndatadir=%s\r\nport=%d\r\nbind-address=127.0.0.1\r\n"
                     "pid-file=%s\r\nlog-error=%s/logs/mariadb.err\r\nsecure-file-priv=%s/data/files\r\n"
                     "character-set-server=utf8mb4\r\n"
                     "collation-server=utf8mb4_unicode_ci\r\nmax_allowed_packet=64M\r\n"
                     "innodb_buffer_pool_size=128M\r\n\r\n[client]\r\nhost=127.0.0.1\r\nport=%d\r\n"
                     "default-character-set=utf8mb4\r\n",
                     p->basef, p->datadirf, port, p->pidf, p->rootf, p->rootf, port);
    if (n <= 0 || (size_t)n >= sizeof(text))
        return -1;
    return ols_write_wfile(p->myini, text, (size_t)n, 0);
}

static int start_server(const dbp_t *p, const wchar_t *init)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH * 4], files[MAX_PATH];
    char *ini = NULL;
    size_t n;
    if (!tool(p, L"mariadbd.exe", exe))
        return -1;
    if (elevated())
    {
        fprintf(stderr, "MariaDB does not run as administrator; start it without administrator rights\n");
        return -1;
    }
    swprintf(files, MAX_PATH, L"%s\\files", p->data);
    CreateDirectoryW(files, NULL);
    if (ols_read_wfile(p->myini, &ini, &n) != 0)
    {
        if (write_myini(p, db_port(p)) != 0)
            return -1;
    }
    else if (!strstr(ini, "secure-file-priv") && !strstr(ini, "secure_file_priv"))
    {
        char line[MAX_PATH * 3 + 32], *sec = strstr(ini, "[mysqld]"), *out;
        size_t pre, ll;
        int ok;
        snprintf(line, sizeof(line), "\r\nsecure-file-priv=%s/data/files", p->rootf);
        ll = strlen(line);
        if (!sec || (out = (char *)malloc(n + ll + 1)) == NULL)
        {
            free(ini);
            return -1;
        }
        pre = (size_t)(sec - ini) + 8;
        memcpy(out, ini, pre);
        memcpy(out + pre, line, ll);
        memcpy(out + pre + ll, ini + pre, n - pre);
        ok = ols_write_wfile(p->myini, out, n + ll, 0) == 0;
        free(out);
        if (!ok)
        {
            free(ini);
            return -1;
        }
    }
    free(ini);
    if (init)
        swprintf(cmd, MAX_PATH * 4, L"\"%s\" --defaults-file=\"%s\" --init-file=\"%s\"", exe, p->myini, init);
    else
        swprintf(cmd, MAX_PATH * 4, L"\"%s\" --defaults-file=\"%s\"", exe, p->myini);
    if (start_detached(exe, cmd) != 0)
        return -1;
    return wait_port(db_port(p), 1, 60);
}

static int pick_port(const dbp_t *p)
{
    int port = db_port(p), i;
    if (port > 0)
        return port;
    for (i = 3306; i <= 3320; ++i)
        if (!ols_port_listening(i))
            return i;
    return 0;
}

static int write_pma(const dbp_t *p, int port)
{
    wchar_t cfg[MAX_PATH], tmp[MAX_PATH];
    char secret[33], text[MAX_PATH * 6];
    int n;
    swprintf(tmp, MAX_PATH, L"%s\\tmp\\phpmyadmin", p->root);
    CreateDirectoryW(tmp, NULL);
    token(secret, 32);
    n = snprintf(text, sizeof(text),
                 "<?php\r\ndeclare(strict_types=1);\r\n$cfg['blowfish_secret'] = '%s';\r\n$i = 1;\r\n"
                 "$cfg['Servers'][$i]['auth_type'] = 'cookie';\r\n$cfg['Servers'][$i]['host'] = '127.0.0.1';\r\n"
                 "$cfg['Servers'][$i]['port'] = '%d';\r\n$cfg['Servers'][$i]['AllowNoPassword'] = false;\r\n"
                 "$cfg['TempDir'] = '%s/tmp/phpmyadmin';\r\n$cfg['VersionCheck'] = false;\r\n",
                 secret, port, p->rootf);
    OPENSSL_cleanse(secret, sizeof(secret));
    swprintf(cfg, MAX_PATH, L"%s\\config.inc.php", p->pma);
    n = (n > 0 && (size_t)n < sizeof(text)) ? ols_write_wfile(cfg, text, (size_t)n, 1) : -1;
    OPENSSL_cleanse(text, sizeof(text));
    return n;
}

static int db_install(dbp_t *p)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH * 4], init[MAX_PATH], dir[MAX_PATH], val[16];
    char pw[25], text[1024], cnf[512];
    int port, fresh, n, r;
    if (installed(p))
    {
        printf("MariaDB is already installed\n");
        return alive(p) ? 0 : (start_server(p, NULL) == 0 ? 0 : 1);
    }
    if ((port = pick_port(p)) == 0)
    {
        fprintf(stderr, "no free port between 3306 and 3320\n");
        return 2;
    }
    swprintf(dir, MAX_PATH, L"%s\\tmp", p->root);
    CreateDirectoryW(dir, NULL);
    swprintf(dir, MAX_PATH, L"%s\\logs", p->root);
    CreateDirectoryW(dir, NULL);
    swprintf(dir, MAX_PATH, L"%s\\share", p->root);
    CreateDirectoryW(dir, NULL);
    if (db_fetch(p) != 0)
    {
        fprintf(stderr, "could not install MariaDB and phpMyAdmin\n");
        return 1;
    }
    if (ols_private_dir(p->data) != 0)
    {
        fprintf(stderr, "cannot prepare a private folder for the databases: %ls\n", p->data);
        return GetLastError() == ERROR_ACCESS_DENIED ? 5 : 1;
    }
    fresh = !wexists(p->datadir);
    if (!fresh && !wexists(p->cnf))
    {
        fprintf(stderr, "%ls has databases but no password file; move it away to start fresh\n", p->datadir);
        return 2;
    }
    swprintf(val, 16, L"%d", port);
    if (!WritePrivateProfileStringW(L"db", L"port", val, p->tray) || write_myini(p, port) != 0)
        return 1;
    if (fresh)
    {
        if (!tool(p, L"mariadb-install-db.exe", exe))
            return 1;
        swprintf(cmd, MAX_PATH * 4, L"\"%s\" --datadir=\"%s\" --port=%d --silent", exe, p->datadir, port);
        printf("creating the database files...\n");
        fflush(stdout);
        if (run_cmd(exe, cmd, NULL, 300000) != 0 || !wexists(p->datadir))
        {
            fprintf(stderr, "mariadb-install-db failed\n");
            return 1;
        }
        token(pw, 24);
        n = snprintf(text, sizeof(text),
                     "ALTER USER IF EXISTS 'root'@'localhost' IDENTIFIED BY '%s';\n"
                     "ALTER USER IF EXISTS 'root'@'127.0.0.1' IDENTIFIED BY '%s';\n"
                     "ALTER USER IF EXISTS 'root'@'::1' IDENTIFIED BY '%s';\n"
                     "DELETE FROM mysql.global_priv WHERE User='root' AND Host NOT IN ('localhost','127.0.0.1','::1');\n"
                     "DELETE FROM mysql.global_priv WHERE User='';\n"
                     "DROP DATABASE IF EXISTS test;\n"
                     "DELETE FROM mysql.db WHERE Db='test' OR Db='test\\\\_%%';\nFLUSH PRIVILEGES;\n",
                     pw, pw, pw);
        swprintf(init, MAX_PATH, L"%s\\init.sql", p->data);
        r = (n > 0 && (size_t)n < sizeof(text)) ? ols_write_wfile(init, text, (size_t)n, 1) : -1;
        OPENSSL_cleanse(text, sizeof(text));
        n = snprintf(cnf, sizeof(cnf), "[client]\r\nuser=root\r\npassword=%s\r\nhost=127.0.0.1\r\nport=%d\r\n", pw, port);
        OPENSSL_cleanse(pw, sizeof(pw));
        if (r != 0 || n <= 0 || (size_t)n >= sizeof(cnf) || ols_write_wfile(p->cnf, cnf, (size_t)n, 1) != 0)
        {
            OPENSSL_cleanse(cnf, sizeof(cnf));
            DeleteFileW(init);
            return 1;
        }
        OPENSSL_cleanse(cnf, sizeof(cnf));
        r = start_server(p, init);
        if (r == 0)
        {
            int i;
            for (i = 0; i < 40 && client(p, L"mariadb-admin.exe", L"--silent ping", NULL) != 0; ++i)
                Sleep(250);
        }
        DeleteFileW(init);
    }
    else
        r = start_server(p, NULL);
    if (r != 0 || !alive(p))
    {
        kill_server(p);
        fprintf(stderr, "MariaDB did not start; see logs\\mariadb.err\n");
        return 1;
    }
    if (write_pma(p, port) != 0)
        fprintf(stderr, "could not write the phpMyAdmin configuration\n");
    ols_dev_resync(p->root, p->tray);
    printf("MariaDB %s is running on 127.0.0.1:%d\n", MDB_VERSION, port);
    return 0;
}

static int db_stop(const dbp_t *p)
{
    int port = db_port(p);
    if (port <= 0 || !ols_port_listening(port))
    {
        printf("MariaDB is not running\n");
        return 0;
    }
    if (client(p, L"mariadb-admin.exe", L"--silent shutdown", NULL) != 0 || wait_port(port, 0, 60) != 0)
    {
        fprintf(stderr, "MariaDB did not stop\n");
        return 1;
    }
    printf("MariaDB stopped\n");
    return 0;
}

static void db_name(const char *site, int n, char *out, size_t len)
{
    char base[62];
    size_t i;
    snprintf(base, sizeof(base), "wp_%s", site);
    for (i = 0; base[i]; ++i)
        if (base[i] == '-')
            base[i] = '_';
    if (n > 1)
        snprintf(out, len, "%s_%d", base, n);
    else
        snprintf(out, len, "%s", base);
}

int ols_db_wordpress(const wchar_t *root, const char *site, const wchar_t *dir)
{
    dbp_t p;
    wchar_t cfg[MAX_PATH];
    char name[72], pw[25], salts[8][65], sql[2048], text[4096];
    static const char *const keys[8] = { "AUTH_KEY", "SECURE_AUTH_KEY", "LOGGED_IN_KEY", "NONCE_KEY",
                                         "AUTH_SALT", "SECURE_AUTH_SALT", "LOGGED_IN_SALT", "NONCE_SALT" };
    int i, n, port, made = 0;
    size_t off;
    (void)root;
    if (db_paths(&p) != 0 || !installed(&p))
        return 2;
    port = db_port(&p);
    if (!alive(&p) && start_server(&p, NULL) != 0)
        return 1;
    token(pw, 24);
    for (i = 1; i <= 9 && !made; ++i)
    {
        db_name(site, i, name, sizeof(name));
        n = snprintf(sql, sizeof(sql),
                     "CREATE DATABASE `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n"
                     "DROP USER IF EXISTS '%s'@'localhost';\nDROP USER IF EXISTS '%s'@'127.0.0.1';\n"
                     "CREATE USER '%s'@'localhost' IDENTIFIED BY '%s';\nCREATE USER '%s'@'127.0.0.1' IDENTIFIED BY '%s';\n"
                     "GRANT ALL PRIVILEGES ON `%s`.* TO '%s'@'localhost';\n"
                     "GRANT ALL PRIVILEGES ON `%s`.* TO '%s'@'127.0.0.1';\nFLUSH PRIVILEGES;\n",
                     name, name, name, name, pw, name, pw, name, name, name, name);
        if (n > 0 && (size_t)n < sizeof(sql) && client(&p, L"mariadb.exe", L"--batch", sql) == 0)
            made = 1;
    }
    OPENSSL_cleanse(sql, sizeof(sql));
    if (!made)
    {
        OPENSSL_cleanse(pw, sizeof(pw));
        fprintf(stderr, "could not create a WordPress database\n");
        return 1;
    }
    for (i = 0; i < 8; ++i)
        token(salts[i], 64);
    off = (size_t)snprintf(text, sizeof(text),
                           "<?php\r\ndefine( 'DB_NAME', '%s' );\r\ndefine( 'DB_USER', '%s' );\r\n"
                           "define( 'DB_PASSWORD', '%s' );\r\ndefine( 'DB_HOST', '127.0.0.1:%d' );\r\n"
                           "define( 'DB_CHARSET', 'utf8mb4' );\r\ndefine( 'DB_COLLATE', '' );\r\n\r\n",
                           name, name, pw, port);
    for (i = 0; i < 8 && off < sizeof(text); ++i)
        off += (size_t)snprintf(text + off, sizeof(text) - off, "define( '%s', '%s' );\r\n", keys[i], salts[i]);
    if (off < sizeof(text))
        off += (size_t)snprintf(text + off, sizeof(text) - off,
                                "\r\n$table_prefix = 'wp_';\r\n\r\ndefine( 'WP_DEBUG', false );\r\n\r\n"
                                "if ( ! defined( 'ABSPATH' ) ) {\r\n\tdefine( 'ABSPATH', __DIR__ . '/' );\r\n}\r\n\r\n"
                                "require_once ABSPATH . 'wp-settings.php';\r\n");
    OPENSSL_cleanse(pw, sizeof(pw));
    OPENSSL_cleanse(salts, sizeof(salts));
    swprintf(cfg, MAX_PATH, L"%s\\wp-config.php", dir);
    n = off < sizeof(text) ? ols_write_wfile(cfg, text, off, 1) : -1;
    OPENSSL_cleanse(text, sizeof(text));
    if (n != 0)
    {
        fprintf(stderr, "could not write wp-config.php\n");
        return 1;
    }
    printf("WordPress database %s created\n", name);
    return 0;
}

static int db_ident(const char *s)
{
    static const char *const reserved[] = { "mysql", "information_schema", "performance_schema", "sys", "root" };
    size_t n = s ? strlen(s) : 0, i;
    if (n == 0 || n > 64)
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_'))
            return 0;
    for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i)
        if (_stricmp(s, reserved[i]) == 0)
            return 0;
    return 1;
}

static int run_io(const wchar_t *exe, wchar_t *cmd, HANDLE in, char **out, size_t cap, DWORD timeout)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd = NULL, wr = NULL, nul = INVALID_HANDLE_VALUE;
    DWORD code = 1, got;
    size_t n = 0;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    if (out)
    {
        *out = NULL;
        if (!CreatePipe(&rd, &wr, &sa, 0))
            return -1;
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    }
    if (!in)
        nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    else
        SetHandleInformation(in, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    si.hStdInput = in ? in : nul;
    si.hStdOutput = out ? wr : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    fflush(stdout);
    fflush(stderr);
    if (!CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        if (rd)
            CloseHandle(rd);
        if (wr)
            CloseHandle(wr);
        if (nul != INVALID_HANDLE_VALUE)
            CloseHandle(nul);
        return -1;
    }
    if (wr)
        CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    if (out && (*out = (char *)malloc(cap + 1)) != NULL)
    {
        char buf[4096];
        while (ReadFile(rd, buf, sizeof(buf), &got, NULL) && got)
        {
            size_t take = got < cap - n ? got : cap - n;
            memcpy(*out + n, buf, take);
            n += take;
        }
        (*out)[n] = 0;
    }
    if (rd)
        CloseHandle(rd);
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

static int query(const dbp_t *p, const char *sql, char **out)
{
    wchar_t exe[MAX_PATH], cmd[MAX_PATH * 2 + 2048], wsql[2048];
    if (!tool(p, L"mariadb.exe", exe) || MultiByteToWideChar(CP_UTF8, 0, sql, -1, wsql, 2048) <= 0)
        return -1;
    swprintf(cmd, sizeof(cmd) / sizeof(cmd[0]),
             L"\"%s\" --defaults-extra-file=\"%s\" --batch --skip-column-names --default-character-set=utf8mb4 -e \"%s\"",
             exe, p->cnf, wsql);
    return run_io(exe, cmd, NULL, out, 1 << 20, 120000);
}

static int db_list(const dbp_t *p)
{
    char *out = NULL, *line, *next;
    int first = 1;
    if (query(p,
              "SELECT s.SCHEMA_NAME, COUNT(t.TABLE_NAME), COALESCE(SUM(t.DATA_LENGTH + t.INDEX_LENGTH), 0), "
              "(SELECT COUNT(*) FROM mysql.global_priv g WHERE g.User = s.SCHEMA_NAME AND g.Host IN ('localhost','127.0.0.1')) "
              "FROM information_schema.SCHEMATA s LEFT JOIN information_schema.TABLES t ON t.TABLE_SCHEMA = s.SCHEMA_NAME "
              "WHERE s.SCHEMA_NAME NOT IN ('mysql','information_schema','performance_schema','sys') "
              "GROUP BY s.SCHEMA_NAME ORDER BY s.SCHEMA_NAME",
              &out)
            != 0
        || !out)
    {
        free(out);
        fprintf(stderr, "could not read the database list\n");
        return 1;
    }
    printf("[");
    for (line = out; line && *line; line = next)
    {
        char *f[4], *e = strchr(line, '\n');
        int i;
        next = e ? e + 1 : NULL;
        if (e)
            *e = 0;
        if (e && e > line && e[-1] == '\r')
            e[-1] = 0;
        f[0] = line;
        for (i = 1; i < 4; ++i)
            if ((f[i] = strchr(f[i - 1], '\t')) != NULL)
                *f[i]++ = 0;
            else
                break;
        if (i < 4)
            continue;
        printf("%s{\"name\":", first ? "" : ",");
        ols_jstr(f[0]);
        printf(",\"tables\":%ld,\"size\":%lld,\"user\":%s,\"managed\":%s}", strtol(f[1], NULL, 10),
               _strtoi64(f[2], NULL, 10), strtol(f[3], NULL, 10) > 0 ? "true" : "false", db_ident(f[0]) ? "true" : "false");
        first = 0;
    }
    printf("]\n");
    free(out);
    return 0;
}

static int user_exists(const dbp_t *p, const char *name)
{
    char sql[256], *out = NULL;
    int r;
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM mysql.global_priv WHERE User = '%s'", name);
    if (query(p, sql, &out) != 0 || !out)
    {
        free(out);
        return -1;
    }
    r = atoi(out) > 0;
    free(out);
    return r;
}

static int db_create(const dbp_t *p, const char *name, int with_user)
{
    char pw[25], sql[1024];
    int n, r, u;
    if (!db_ident(name))
    {
        fprintf(stderr, "use lowercase letters, digits and '_' for the database name\n");
        return 2;
    }
    if (with_user && (u = user_exists(p, name)) != 0)
    {
        fprintf(stderr, u < 0 ? "could not check the database users\n"
                              : "a database user with this name already exists; create the database without a user\n");
        return u < 0 ? 1 : 2;
    }
    token(pw, 24);
    if (with_user)
        n = snprintf(sql, sizeof(sql),
                     "CREATE DATABASE `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n"
                     "CREATE USER '%s'@'localhost' IDENTIFIED BY '%s';\nCREATE USER '%s'@'127.0.0.1' IDENTIFIED BY '%s';\n"
                     "GRANT ALL PRIVILEGES ON `%s`.* TO '%s'@'localhost';\nGRANT ALL PRIVILEGES ON `%s`.* TO '%s'@'127.0.0.1';\n"
                     "FLUSH PRIVILEGES;\n",
                     name, name, pw, name, pw, name, name, name, name);
    else
        n = snprintf(sql, sizeof(sql), "CREATE DATABASE `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n", name);
    r = n > 0 && (size_t)n < sizeof(sql) ? client(p, L"mariadb.exe", L"--batch", sql) : -1;
    OPENSSL_cleanse(sql, sizeof(sql));
    if (r != 0)
    {
        OPENSSL_cleanse(pw, sizeof(pw));
        fprintf(stderr, "could not create the database %s\n", name);
        return 1;
    }
    printf("database %s created\n", name);
    if (with_user)
        printf("user %s\npassword %s\n", name, pw);
    fflush(stdout);
    OPENSSL_cleanse(pw, sizeof(pw));
    return 0;
}

static int db_drop(const dbp_t *p, const char *name, int with_user)
{
    char sql[512];
    int n;
    if (!db_ident(name))
    {
        fprintf(stderr, "this database cannot be removed here\n");
        return 2;
    }
    n = snprintf(sql, sizeof(sql), "DROP DATABASE `%s`;\n", name);
    if (with_user)
        n = snprintf(sql, sizeof(sql),
                     "DROP DATABASE `%s`;\nDROP USER IF EXISTS '%s'@'localhost';\nDROP USER IF EXISTS '%s'@'127.0.0.1';\n"
                     "FLUSH PRIVILEGES;\n",
                     name, name, name);
    if (n <= 0 || (size_t)n >= sizeof(sql) || client(p, L"mariadb.exe", L"--batch", sql) != 0)
    {
        fprintf(stderr, "could not remove the database %s\n", name);
        return 1;
    }
    printf("database %s removed\n", name);
    return 0;
}

static int sql_path(const char *arg, wchar_t *out)
{
    size_t n;
    if (!arg || MultiByteToWideChar(CP_UTF8, 0, arg, -1, out, MAX_PATH) <= 0 || wcschr(out, L'"'))
        return -1;
    n = wcslen(out);
    if (n < 7 || out[1] != L':' || out[2] != L'\\' || _wcsicmp(out + n - 4, L".sql") != 0)
        return -1;
    return 0;
}

static int db_export(const dbp_t *p, const char *name, const char *file)
{
    wchar_t exe[MAX_PATH], wf[MAX_PATH], wn[72], cmd[MAX_PATH * 4];
    if (!db_ident(name) || sql_path(file, wf) != 0 || MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 72) <= 0)
    {
        fprintf(stderr, "choose a .sql file to export to\n");
        return 2;
    }
    if (!tool(p, L"mariadb-dump.exe", exe))
        return 1;
    swprintf(cmd, MAX_PATH * 4,
             L"\"%s\" --defaults-extra-file=\"%s\" --single-transaction --routines --triggers --events --hex-blob "
             L"--default-character-set=utf8mb4 --result-file=\"%s\" %s",
             exe, p->cnf, wf, wn);
    if (run_io(exe, cmd, NULL, NULL, 0, 3600000) != 0)
    {
        DeleteFileW(wf);
        fprintf(stderr, "could not export %s\n", name);
        return 1;
    }
    printf("exported %s to %ls\n", name, wf);
    return 0;
}

static int db_import(const dbp_t *p, const char *name, const char *file)
{
    wchar_t exe[MAX_PATH], wf[MAX_PATH], wn[72], cmd[MAX_PATH * 4];
    char sql[160];
    HANDLE in;
    int r;
    if (!db_ident(name) || sql_path(file, wf) != 0 || MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 72) <= 0)
    {
        fprintf(stderr, "choose a .sql file to import\n");
        return 2;
    }
    if (!tool(p, L"mariadb.exe", exe))
        return 1;
    snprintf(sql, sizeof(sql), "CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n", name);
    if (client(p, L"mariadb.exe", L"--batch", sql) != 0)
    {
        fprintf(stderr, "could not create the database %s\n", name);
        return 1;
    }
    in = CreateFileW(wf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (in == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "cannot open %ls\n", wf);
        return 1;
    }
    swprintf(cmd, MAX_PATH * 4, L"\"%s\" --defaults-extra-file=\"%s\" --batch --default-character-set=utf8mb4 %s", exe,
             p->cnf, wn);
    r = run_io(exe, cmd, in, NULL, 0, 3600000);
    CloseHandle(in);
    if (r != 0)
    {
        fprintf(stderr, "the import into %s stopped with an error\n", name);
        return 1;
    }
    printf("imported %ls into %s\n", wf, name);
    return 0;
}

int ols_db_main(int argc, char **argv)
{
    dbp_t p;
    const char *act = argc >= 1 ? argv[0] : "status";
    int port;
    if (db_paths(&p) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    if (strcmp(act, "install") == 0 || strcmp(act, "fetch") == 0)
    {
        HANDLE m;
        DWORD w;
        int r = 1, inst = strcmp(act, "install") == 0;
        if (inst && elevated())
        {
            fprintf(stderr, "MariaDB does not run as administrator; install it without administrator rights\n");
            return 5;
        }
        m = CreateMutexW(NULL, FALSE, L"Local\\OpenLiteSpeedDbInstall");
        w = m ? WaitForSingleObject(m, 1800000) : WAIT_FAILED;
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED)
        {
            if (inst)
                r = db_install(&p);
            else if ((r = db_fetch(&p)) != 0)
                fprintf(stderr, "could not download MariaDB and phpMyAdmin\n");
            else
            {
                ols_dev_resync(p.root, p.tray);
                printf("MariaDB %s and phpMyAdmin %s are downloaded\n", MDB_VERSION, PMA_VERSION);
            }
            ReleaseMutex(m);
        }
        else
            fprintf(stderr, "another MariaDB installation is still running\n");
        if (m)
            CloseHandle(m);
        return r;
    }
    if (!installed(&p))
    {
        if (strcmp(act, "status") == 0)
        {
            printf("not installed\n");
            return 0;
        }
        if (strcmp(act, "stop") == 0)
            return 0;
        fprintf(stderr, "MariaDB is not installed; run --db install\n");
        return 2;
    }
    port = db_port(&p);
    if (strcmp(act, "start") == 0)
    {
        if (alive(&p))
            return 0;
        if (start_server(&p, NULL) != 0)
        {
            fprintf(stderr, "MariaDB did not start; see logs\\mariadb.err\n");
            return 1;
        }
        printf("MariaDB is running on 127.0.0.1:%d\n", port);
        return 0;
    }
    if (strcmp(act, "stop") == 0)
        return db_stop(&p);
    if (strcmp(act, "status") == 0)
    {
        printf("MariaDB %s, port %d, %s\n", MDB_VERSION, port, alive(&p) ? "running" : "stopped");
        return 0;
    }
    if (strcmp(act, "list") == 0 || strcmp(act, "create") == 0 || strcmp(act, "drop") == 0 || strcmp(act, "export") == 0
        || strcmp(act, "import") == 0)
    {
        int flag = argc >= 3 && strcmp(argv[2], "--user") == 0;
        if (!alive(&p))
        {
            fprintf(stderr, "start MariaDB first\n");
            return 3;
        }
        if (strcmp(act, "list") == 0)
            return db_list(&p);
        if (argc < 2)
            goto usage;
        if (strcmp(act, "create") == 0)
            return db_create(&p, argv[1], !(argc >= 3 && strcmp(argv[2], "--no-user") == 0));
        if (strcmp(act, "drop") == 0)
            return db_drop(&p, argv[1], flag);
        if (argc < 3)
            goto usage;
        return strcmp(act, "export") == 0 ? db_export(&p, argv[1], argv[2]) : db_import(&p, argv[1], argv[2]);
    }
usage:
    fprintf(stderr, "usage: openlitespeed --db install | fetch | start | stop | status | list | create <name> [--no-user] |\n"
                    "       drop <name> [--user] | export <name> <file.sql> | import <name> <file.sql>\n");
    return 2;
}

void ols_db_summary(int *is_installed, int *running, int *port)
{
    dbp_t p;
    *is_installed = *running = *port = 0;
    if (db_paths(&p) != 0 || !installed(&p))
        return;
    *is_installed = 1;
    *port = db_port(&p);
    *running = ols_port_listening(*port);
}
