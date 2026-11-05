#include "compat_internal.h"

#define MAIL_LIMIT 26214400
#define MAIL_KEEP 1000
#define MAIL_CONNS 20
#define MAIL_RCPTS 100
#define MAIL_IDLE 60000

typedef struct
{
    SOCKET s;
    wchar_t root[MAX_PATH];
    char in[16384];
    size_t inlen, inpos;
    char *line;
    size_t linecap;
} smtp_t;

static volatile LONG s_conns;

static int ml_root(wchar_t *root)
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

static void ml_ini(const wchar_t *root, wchar_t *ini)
{
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
}

static int ml_port(const wchar_t *ini)
{
    int p = GetPrivateProfileIntW(L"mail", L"port", 1025, ini);
    return p > 0 && p < 65536 ? p : 1025;
}

static void ml_dir(const wchar_t *root, wchar_t *dir)
{
    swprintf(dir, MAX_PATH, L"%s\\data\\mail", root);
}

static void ml_tmpfile(const wchar_t *root, const wchar_t *name, wchar_t *out)
{
    swprintf(out, MAX_PATH, L"%s\\tmp\\%s", root, name);
}

static int ml_prepare(const wchar_t *root, wchar_t *dir)
{
    wchar_t data[MAX_PATH];
    DWORD a;
    swprintf(data, MAX_PATH, L"%s\\data", root);
    CreateDirectoryW(data, NULL);
    ml_dir(root, dir);
    a = GetFileAttributesW(dir);
    if (a != INVALID_FILE_ATTRIBUTES)
        return (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT) ? 0 : -1;
    return ols_private_dir(dir);
}

static int ml_id_ok(const char *id)
{
    int i;
    if (!id || strlen(id) != 24 || id[8] != 'T' || id[15] != '-')
        return 0;
    for (i = 0; i < 24; ++i)
    {
        if (i == 8 || i == 15)
            continue;
        if (i < 15 ? !isdigit((unsigned char)id[i]) : !isxdigit((unsigned char)id[i]) || isupper((unsigned char)id[i]))
            return 0;
    }
    return 1;
}

static int ml_name_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)b, *(const char *const *)a);
}

static int ml_names(const wchar_t *dir, char ***out)
{
    wchar_t pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    char **v = NULL, mb[64];
    int n = 0, cap = 0;
    *out = NULL;
    swprintf(pat, MAX_PATH, L"%s\\*.eml", dir);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        size_t l;
        if ((fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            || WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, mb, sizeof(mb), NULL, NULL) <= 0)
            continue;
        l = strlen(mb);
        if (l != 28 || strcmp(mb + 24, ".eml") != 0)
            continue;
        mb[24] = 0;
        if (!ml_id_ok(mb))
            continue;
        if (n == cap)
        {
            char **g = (char **)realloc(v, (size_t)(cap ? cap * 2 : 64) * sizeof(char *));
            if (!g)
                break;
            v = g;
            cap = cap ? cap * 2 : 64;
        }
        if ((v[n] = _strdup(mb)) != NULL)
            ++n;
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
    qsort(v, (size_t)n, sizeof(char *), ml_name_cmp);
    *out = v;
    return n;
}

static void ml_names_free(char **v, int n)
{
    while (n > 0)
        free(v[--n]);
    free(v);
}

static void ml_path(const wchar_t *dir, const char *id, const wchar_t *ext, wchar_t *out)
{
    wchar_t wid[32];
    MultiByteToWideChar(CP_UTF8, 0, id, -1, wid, 32);
    swprintf(out, MAX_PATH, L"%s\\%s%s", dir, wid, ext);
}

static int ml_store(const wchar_t *root, const char *data, size_t len, const char *from, char (*rcpt)[256], int nrcpt,
                    char *idout)
{
    wchar_t dir[MAX_PATH], tmp[MAX_PATH], eml[MAX_PATH];
    SYSTEMTIME st;
    unsigned char rnd[4];
    char id[32], *buf;
    size_t pre = 0, cap, i;
    char **names;
    int n, r;
    if (ml_prepare(root, dir) != 0)
        return -1;
    cap = len + 64 + (from ? strlen(from) : 0) + (size_t)nrcpt * 260;
    if ((buf = (char *)malloc(cap)) == NULL)
        return -1;
    if (from && *from)
        pre += (size_t)snprintf(buf + pre, cap - pre, "Return-Path: <%s>\r\n", from);
    if (nrcpt > 0)
    {
        pre += (size_t)snprintf(buf + pre, cap - pre, "X-Original-To: ");
        for (i = 0; i < (size_t)nrcpt; ++i)
            pre += (size_t)snprintf(buf + pre, cap - pre, "%s%s", i ? ", " : "", rcpt[i]);
        pre += (size_t)snprintf(buf + pre, cap - pre, "\r\n");
    }
    memcpy(buf + pre, data, len);
    GetSystemTime(&st);
    if (BCryptGenRandom(NULL, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    {
        DWORD t = GetTickCount() ^ GetCurrentProcessId();
        memcpy(rnd, &t, 4);
    }
    snprintf(id, sizeof(id), "%04u%02u%02uT%02u%02u%02u-%08lx", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
             st.wSecond, ((unsigned long)st.wMilliseconds << 22) | (((unsigned long)rnd[0] << 16 | rnd[1] << 8 | rnd[2]) & 0x3fffffUL));
    ml_path(dir, id, L".tmp", tmp);
    ml_path(dir, id, L".eml", eml);
    r = ols_write_wfile(tmp, buf, pre + len, 1) == 0 && MoveFileExW(tmp, eml, 0) ? 0 : -1;
    free(buf);
    if (r != 0)
    {
        DeleteFileW(tmp);
        return -1;
    }
    if (idout)
        strcpy(idout, id);
    n = ml_names(dir, &names);
    for (i = MAIL_KEEP; i < (size_t)n; ++i)
    {
        ml_path(dir, names[i], L".eml", eml);
        DeleteFileW(eml);
    }
    ml_names_free(names, n);
    return 0;
}

static int sm_send(smtp_t *c, const char *t)
{
    return send(c->s, t, (int)strlen(t), 0) == (int)strlen(t) ? 0 : -1;
}

static int sm_fill(smtp_t *c)
{
    int got;
    if (c->inpos > 0 && c->inpos < c->inlen)
    {
        memmove(c->in, c->in + c->inpos, c->inlen - c->inpos);
        c->inlen -= c->inpos;
    }
    else if (c->inpos >= c->inlen)
        c->inlen = 0;
    c->inpos = 0;
    got = recv(c->s, c->in + c->inlen, (int)(sizeof(c->in) - c->inlen), 0);
    if (got <= 0)
        return got == 0 ? -1 : (WSAGetLastError() == WSAETIMEDOUT ? -2 : -1);
    c->inlen += (size_t)got;
    return 0;
}

static int sm_read(smtp_t *c, size_t max, size_t *len)
{
    size_t n = 0;
    int r;
    for (;;)
    {
        while (c->inpos < c->inlen)
        {
            char ch = c->in[c->inpos++];
            if (ch == '\n')
            {
                if (n && c->line[n - 1] == '\r')
                    --n;
                c->line[n] = 0;
                *len = n;
                return 0;
            }
            if (n + 2 > c->linecap)
            {
                size_t cap = c->linecap * 2;
                char *g;
                if (n + 2 > max || (g = (char *)realloc(c->line, cap)) == NULL)
                    return -3;
                c->line = g;
                c->linecap = cap;
            }
            c->line[n++] = ch;
        }
        if ((r = sm_fill(c)) != 0)
            return r;
    }
}

static int sm_addr(const char *arg, const char *key, char *out, size_t n, const char **rest)
{
    size_t kl = strlen(key), i = 0;
    const char *p = arg;
    if (_strnicmp(p, key, kl) != 0)
        return -1;
    p += kl;
    while (*p == ' ')
        ++p;
    if (*p == '<')
    {
        ++p;
        while (*p && *p != '>' && i + 1 < n)
            out[i++] = *p++;
        if (*p != '>')
            return -1;
        ++p;
    }
    else
        while (*p && *p != ' ' && i + 1 < n)
            out[i++] = *p++;
    out[i] = 0;
    if (rest)
        *rest = p;
    return 0;
}

static int sm_data(smtp_t *c, char **msg, size_t *len, int *big)
{
    size_t cap = 65536, n = 0, l;
    char *m = (char *)malloc(cap);
    int r;
    *big = 0;
    *msg = NULL;
    if (!m)
        return -1;
    for (;;)
    {
        const char *s;
        if ((r = sm_read(c, (size_t)MAIL_LIMIT + 16, &l)) != 0)
        {
            free(m);
            return r;
        }
        if (l == 1 && c->line[0] == '.')
            break;
        s = c->line[0] == '.' ? c->line + 1 : c->line;
        l -= (size_t)(s - c->line);
        if (*big || n + l + 2 > MAIL_LIMIT)
        {
            *big = 1;
            continue;
        }
        if (n + l + 3 > cap)
        {
            char *g;
            while (n + l + 3 > cap)
                cap *= 2;
            if ((g = (char *)realloc(m, cap)) == NULL)
            {
                free(m);
                return -1;
            }
            m = g;
        }
        memcpy(m + n, s, l);
        m[n + l] = '\r';
        m[n + l + 1] = '\n';
        n += l + 2;
    }
    m[n] = 0;
    *msg = m;
    *len = n;
    return 0;
}

static int sm_auth_line(smtp_t *c, const char *prompt)
{
    size_t l;
    int r;
    if (sm_send(c, prompt) != 0)
        return -1;
    if ((r = sm_read(c, 16384, &l)) != 0)
        return r;
    r = strcmp(c->line, "*") == 0 ? 1 : 0;
    SecureZeroMemory(c->line, l);
    return r;
}

static DWORD WINAPI sm_session(LPVOID arg)
{
    smtp_t *c = (smtp_t *)arg;
    char from[256] = "", (*rcpt)[256] = (char (*)[256])calloc(MAIL_RCPTS, 256), reply[160], id[32];
    int nrcpt = 0, mail = 0, authed = 0, r = 0;
    DWORD tmo = MAIL_IDLE;
    c->linecap = 1024;
    c->line = (char *)malloc(c->linecap);
    setsockopt(c->s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof(tmo));
    setsockopt(c->s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tmo, sizeof(tmo));
    if (!rcpt || !c->line || sm_send(c, "220 localhost ESMTP OpenLiteSpeed mail catcher\r\n") != 0)
        goto done;
    for (;;)
    {
        size_t l;
        char *cmd, *arg;
        if ((r = sm_read(c, 4096, &l)) != 0)
        {
            if (r == -2)
                sm_send(c, "421 4.4.2 Idle timeout, closing connection\r\n");
            else if (r == -3)
                sm_send(c, "500 5.5.2 Line too long\r\n");
            break;
        }
        cmd = c->line;
        while (*cmd == ' ')
            ++cmd;
        for (arg = cmd; *arg && *arg != ' '; ++arg)
            ;
        if (*arg)
            *arg++ = 0;
        while (*arg == ' ')
            ++arg;
        if (_stricmp(cmd, "EHLO") == 0)
        {
            mail = 0;
            nrcpt = 0;
            from[0] = 0;
            snprintf(reply, sizeof(reply),
                     "250-localhost\r\n250-SIZE %d\r\n250-8BITMIME\r\n250-PIPELINING\r\n250-AUTH PLAIN LOGIN\r\n"
                     "250 HELP\r\n",
                     MAIL_LIMIT);
            r = sm_send(c, reply);
        }
        else if (_stricmp(cmd, "HELO") == 0)
        {
            mail = 0;
            nrcpt = 0;
            from[0] = 0;
            r = sm_send(c, "250 localhost\r\n");
        }
        else if (_stricmp(cmd, "MAIL") == 0)
        {
            const char *rest = NULL, *sz;
            if (mail)
                r = sm_send(c, "503 5.5.1 Sender already given\r\n");
            else if (sm_addr(arg, "FROM:", from, sizeof(from), &rest) != 0)
                r = sm_send(c, "501 5.5.4 Syntax: MAIL FROM:<address>\r\n");
            else if (rest && (sz = strstr(rest, "SIZE=")) != NULL && _atoi64(sz + 5) > MAIL_LIMIT)
            {
                from[0] = 0;
                r = sm_send(c, "552 5.3.4 Message size exceeds fixed limit\r\n");
            }
            else
            {
                mail = 1;
                nrcpt = 0;
                r = sm_send(c, "250 2.1.0 Ok\r\n");
            }
        }
        else if (_stricmp(cmd, "RCPT") == 0)
        {
            char to[256];
            if (!mail)
                r = sm_send(c, "503 5.5.1 Need MAIL before RCPT\r\n");
            else if (sm_addr(arg, "TO:", to, sizeof(to), NULL) != 0 || !to[0])
                r = sm_send(c, "501 5.5.4 Syntax: RCPT TO:<address>\r\n");
            else if (nrcpt >= MAIL_RCPTS)
                r = sm_send(c, "452 4.5.3 Too many recipients\r\n");
            else
            {
                strcpy(rcpt[nrcpt++], to);
                r = sm_send(c, "250 2.1.5 Ok\r\n");
            }
        }
        else if (_stricmp(cmd, "DATA") == 0)
        {
            char *msg = NULL;
            size_t ml = 0;
            int big = 0;
            if (!mail || !nrcpt)
            {
                r = sm_send(c, !mail ? "503 5.5.1 Need MAIL command\r\n" : "503 5.5.1 Need RCPT command\r\n");
                continue;
            }
            if (sm_send(c, "354 End data with <CR><LF>.<CR><LF>\r\n") != 0 || (r = sm_data(c, &msg, &ml, &big)) != 0)
            {
                if (r == -2)
                    sm_send(c, "421 4.4.2 Idle timeout, closing connection\r\n");
                break;
            }
            if (big)
                r = sm_send(c, "552 5.3.4 Message size exceeds fixed limit\r\n");
            else if (ml_store(c->root, msg, ml, from, rcpt, nrcpt, id) != 0)
                r = sm_send(c, "451 4.3.0 The message could not be stored\r\n");
            else
            {
                snprintf(reply, sizeof(reply), "250 2.0.0 Ok: queued as %s\r\n", id);
                r = sm_send(c, reply);
            }
            free(msg);
            mail = 0;
            nrcpt = 0;
            from[0] = 0;
        }
        else if (_stricmp(cmd, "RSET") == 0)
        {
            mail = 0;
            nrcpt = 0;
            from[0] = 0;
            r = sm_send(c, "250 2.0.0 Ok\r\n");
        }
        else if (_stricmp(cmd, "NOOP") == 0)
            r = sm_send(c, "250 2.0.0 Ok\r\n");
        else if (_stricmp(cmd, "VRFY") == 0)
            r = sm_send(c, "252 2.0.0 Cannot VRFY user, but will accept message\r\n");
        else if (_stricmp(cmd, "HELP") == 0)
            r = sm_send(c, "214 2.0.0 EHLO HELO MAIL RCPT DATA RSET NOOP VRFY AUTH QUIT\r\n");
        else if (_stricmp(cmd, "AUTH") == 0)
        {
            char *init = strchr(arg, ' ');
            int cancel = 0;
            if (init)
                *init++ = 0;
            if (authed)
                r = sm_send(c, "503 5.5.1 Already authenticated\r\n");
            else if (_stricmp(arg, "PLAIN") == 0)
            {
                if (!init || !*init)
                    cancel = sm_auth_line(c, "334 \r\n");
                if (cancel < 0)
                    break;
                r = sm_send(c, cancel ? "501 5.7.0 Authentication canceled\r\n" : "235 2.7.0 Authentication successful\r\n");
                authed = !cancel;
            }
            else if (_stricmp(arg, "LOGIN") == 0)
            {
                if (!init || !*init)
                    cancel = sm_auth_line(c, "334 VXNlcm5hbWU6\r\n");
                if (cancel == 0)
                    cancel = sm_auth_line(c, "334 UGFzc3dvcmQ6\r\n");
                if (cancel < 0)
                    break;
                r = sm_send(c, cancel ? "501 5.7.0 Authentication canceled\r\n" : "235 2.7.0 Authentication successful\r\n");
                authed = !cancel;
            }
            else
                r = sm_send(c, "504 5.5.4 Unrecognized authentication type\r\n");
            if (init)
                SecureZeroMemory(init, strlen(init));
        }
        else if (_stricmp(cmd, "QUIT") == 0)
        {
            sm_send(c, "221 2.0.0 Bye\r\n");
            break;
        }
        else if (_stricmp(cmd, "STARTTLS") == 0)
            r = sm_send(c, "502 5.5.1 Command not implemented\r\n");
        else
            r = sm_send(c, "500 5.5.2 Command unrecognized\r\n");
        if (r != 0)
            break;
    }
done:
    shutdown(c->s, SD_BOTH);
    closesocket(c->s);
    free(c->line);
    free(rcpt);
    free(c);
    InterlockedDecrement(&s_conns);
    return 0;
}

static SOCKET ml_listen(int port)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a;
    BOOL on = TRUE;
    if (s == INVALID_SOCKET)
        return s;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof(on));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(s, 64) != 0)
    {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static DWORD ml_pid(const wchar_t *root)
{
    wchar_t f[MAX_PATH];
    char *t = NULL;
    size_t n;
    DWORD pid = 0;
    ml_tmpfile(root, L"mail.pid", f);
    if (ols_read_wfile(f, &t, &n) == 0)
    {
        pid = (DWORD)strtoul(t, NULL, 10);
        free(t);
    }
    return pid && ols_pid_is_ours((pid_t)pid) ? pid : 0;
}

static int ml_run(const wchar_t *root)
{
    wchar_t ini[MAX_PATH], pidf[MAX_PATH], stopf[MAX_PATH];
    WSADATA wd;
    SOCKET ls;
    char buf[32];
    int n;
    ml_ini(root, ini);
    ml_tmpfile(root, L"mail.pid", pidf);
    ml_tmpfile(root, L"mail.stop", stopf);
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0 || ml_pid(root) != 0)
        return 1;
    if ((ls = ml_listen(ml_port(ini))) == INVALID_SOCKET)
    {
        fprintf(stderr, "port %d is busy\n", ml_port(ini));
        return 1;
    }
    DeleteFileW(stopf);
    n = snprintf(buf, sizeof(buf), "%lu\n", GetCurrentProcessId());
    if (ols_write_wfile(pidf, buf, (size_t)n, 0) != 0)
        return 1;
    for (;;)
    {
        fd_set rs;
        struct timeval tv = { 0, 500000 };
        SOCKET cs;
        FD_ZERO(&rs);
        FD_SET(ls, &rs);
        if (GetFileAttributesW(stopf) != INVALID_FILE_ATTRIBUTES)
            break;
        if (select(0, &rs, NULL, NULL, &tv) <= 0)
            continue;
        if ((cs = accept(ls, NULL, NULL)) == INVALID_SOCKET)
            continue;
        if (InterlockedIncrement(&s_conns) > MAIL_CONNS)
        {
            static const char busy[] = "421 4.7.0 Too many connections, try again later\r\n";
            send(cs, busy, (int)sizeof(busy) - 1, 0);
            closesocket(cs);
            InterlockedDecrement(&s_conns);
        }
        else
        {
            smtp_t *c = (smtp_t *)calloc(1, sizeof(smtp_t));
            HANDLE th = NULL;
            if (c)
            {
                c->s = cs;
                wcscpy(c->root, root);
                th = CreateThread(NULL, 262144, sm_session, c, 0, NULL);
            }
            if (th)
                CloseHandle(th);
            else
            {
                closesocket(cs);
                free(c);
                InterlockedDecrement(&s_conns);
            }
        }
    }
    closesocket(ls);
    DeleteFileW(stopf);
    DeleteFileW(pidf);
    return 0;
}

static int ml_sendmail(const wchar_t *root, int argc, char **argv)
{
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    char from[256] = "", (*rcpt)[256] = (char (*)[256])calloc(MAIL_RCPTS, 256), *buf;
    size_t cap = 65536, len = 0;
    DWORD got;
    int i, nrcpt = 0, r;
    if (!rcpt || (buf = (char *)malloc(cap)) == NULL)
        return 1;
    for (i = 0; i < argc; ++i)
    {
        const char *a = argv[i];
        if ((strcmp(a, "-f") == 0 || strcmp(a, "-F") == 0) && i + 1 < argc)
        {
            if (a[1] == 'f')
                snprintf(from, sizeof(from), "%s", argv[i + 1]);
            ++i;
        }
        else if (strncmp(a, "-f", 2) == 0)
            snprintf(from, sizeof(from), "%s", a + 2);
        else if (a[0] != '-' && nrcpt < MAIL_RCPTS)
            snprintf(rcpt[nrcpt++], 256, "%s", a);
    }
    while (ReadFile(in, buf + len, (DWORD)(cap - len), &got, NULL) && got)
    {
        len += got;
        if (len > MAIL_LIMIT)
        {
            fprintf(stderr, "the message is larger than 25 MB\n");
            free(buf);
            free(rcpt);
            return 1;
        }
        if (len == cap)
        {
            char *g = (char *)realloc(buf, cap * 2);
            if (!g)
            {
                free(buf);
                free(rcpt);
                return 1;
            }
            buf = g;
            cap *= 2;
        }
    }
    r = ml_store(root, buf, len, from, rcpt, nrcpt, NULL);
    free(buf);
    free(rcpt);
    if (r != 0)
    {
        fprintf(stderr, "the message could not be stored\n");
        return 1;
    }
    return 0;
}

static int ml_read(const wchar_t *path, char **out, size_t *len)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    LARGE_INTEGER sz;
    DWORD got = 0;
    *out = NULL;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > MAIL_LIMIT + 65536LL || (*out = (char *)malloc((size_t)sz.QuadPart + 1)) == NULL
        || !ReadFile(h, *out, (DWORD)sz.QuadPart, &got, NULL))
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

static const char *hdr_body(const char *p, const char *e, const char **he)
{
    const char *q;
    if (p < e && (*p == '\n' || (*p == '\r' && p + 1 < e && p[1] == '\n')))
    {
        *he = p;
        return p + (*p == '\n' ? 1 : 2);
    }
    for (q = p; q < e; ++q)
    {
        if (*q != '\n')
            continue;
        if (q + 1 < e && q[1] == '\n')
        {
            *he = q + 1;
            return q + 2;
        }
        if (q + 2 < e && q[1] == '\r' && q[2] == '\n')
        {
            *he = q + 1;
            return q + 3;
        }
    }
    *he = e;
    return e;
}

static int hdr_get(const char *h, const char *he, const char *name, char *out, size_t n)
{
    size_t nl = strlen(name), i = 0;
    const char *p, *eol, *v;
    out[0] = 0;
    for (p = h; p < he; p = eol + 1)
    {
        eol = (const char *)memchr(p, '\n', (size_t)(he - p));
        if (!eol)
            eol = he;
        if ((size_t)(eol - p) <= nl || _strnicmp(p, name, nl) != 0 || p[nl] != ':')
            continue;
        v = p + nl + 1;
        for (;;)
        {
            while (v < eol && (*v == ' ' || *v == '\t'))
                ++v;
            if (i && i + 1 < n && v < eol && *v != '\r')
                out[i++] = ' ';
            while (v < eol && *v != '\r' && i + 1 < n)
                out[i++] = *v++;
            if (eol + 1 >= he || (eol[1] != ' ' && eol[1] != '\t'))
                break;
            v = eol + 1;
            eol = (const char *)memchr(v, '\n', (size_t)(he - v));
            if (!eol)
                eol = he;
        }
        out[i] = 0;
        return 1;
    }
    return 0;
}

static void param_get(const char *v, const char *key, char *out, size_t n)
{
    size_t kl = strlen(key), i = 0;
    const char *p = v;
    out[0] = 0;
    while ((p = strchr(p, ';')) != NULL)
    {
        ++p;
        while (*p == ' ' || *p == '\t')
            ++p;
        if (_strnicmp(p, key, kl) == 0 && (p[kl] == '=' || p[kl] == '*'))
        {
            p += kl;
            while (*p && *p != '=')
                ++p;
            if (*p != '=')
                return;
            ++p;
            if (*p == '"')
            {
                ++p;
                while (*p && *p != '"' && i + 1 < n)
                    out[i++] = *p++;
            }
            else
                while (*p && *p != ';' && *p != ' ' && *p != '\t' && i + 1 < n)
                    out[i++] = *p++;
            out[i] = 0;
            if (!out[0])
                strcpy(out, "-");
            return;
        }
    }
}

static void mime_type(const char *v, char *out, size_t n)
{
    size_t i = 0;
    while (*v == ' ')
        ++v;
    while (*v && *v != ';' && *v != ' ' && i + 1 < n)
        out[i++] = (char)tolower((unsigned char)*v++);
    out[i] = 0;
}

static void mime_walk(const char *p, const char *e, int *html, int *att, int depth)
{
    const char *he, *body = hdr_body(p, e, &he);
    char ct[512], disp[512], type[96], dtype[32], name[256], bnd[200];
    hdr_get(p, he, "Content-Type", ct, sizeof(ct));
    hdr_get(p, he, "Content-Disposition", disp, sizeof(disp));
    mime_type(ct[0] ? ct : "text/plain", type, sizeof(type));
    mime_type(disp, dtype, sizeof(dtype));
    if (strncmp(type, "multipart/", 10) == 0)
    {
        const char *q = body, *start = NULL;
        size_t bl;
        char delim[210];
        param_get(ct, "boundary", bnd, sizeof(bnd));
        if (!bnd[0] || depth > 8)
            return;
        bl = (size_t)snprintf(delim, sizeof(delim), "--%s", bnd);
        while (q < e)
        {
            const char *eol = (const char *)memchr(q, '\n', (size_t)(e - q));
            if (!eol)
                eol = e;
            if ((size_t)(eol - q) >= bl && memcmp(q, delim, bl) == 0)
            {
                if (start)
                    mime_walk(start, q, html, att, depth + 1);
                if ((size_t)(eol - q) >= bl + 2 && q[bl] == '-' && q[bl + 1] == '-')
                    return;
                start = eol < e ? eol + 1 : e;
            }
            q = eol + 1;
        }
        return;
    }
    param_get(disp, "filename", name, sizeof(name));
    if (!name[0])
        param_get(ct, "name", name, sizeof(name));
    if (strcmp(dtype, "attachment") == 0 || strcmp(type, "message/rfc822") == 0
        || (strcmp(dtype, "inline") != 0 && name[0] && strncmp(type, "text/", 5) != 0))
        ++*att;
    else if (strcmp(type, "text/html") == 0)
        *html = 1;
}

static int utf8_ok(const unsigned char *s, size_t n)
{
    size_t i = 0;
    while (i < n)
    {
        unsigned char c = s[i++];
        int k = c < 0x80 ? 0 : (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : -1;
        if (k < 0 || c == 0xc0 || c == 0xc1)
            return 0;
        for (; k > 0; --k, ++i)
            if (i >= n || (s[i] & 0xc0) != 0x80)
                return 0;
    }
    return 1;
}

static UINT cs_page(const char *cs)
{
    static const struct
    {
        const char *name;
        UINT cp;
    } map[] = { { "iso-8859-1", 28591 }, { "latin1", 28591 }, { "iso-8859-2", 28592 }, { "iso-8859-3", 28593 },
                { "iso-8859-4", 28594 }, { "iso-8859-5", 28595 }, { "iso-8859-6", 28596 }, { "iso-8859-7", 28597 },
                { "iso-8859-8", 28598 }, { "iso-8859-9", 28599 }, { "iso-8859-13", 28603 }, { "iso-8859-15", 28605 },
                { "windows-1250", 1250 }, { "windows-1251", 1251 }, { "windows-1252", 1252 }, { "cp1252", 1252 },
                { "windows-1253", 1253 }, { "windows-1254", 1254 }, { "windows-1255", 1255 }, { "windows-1256", 1256 },
                { "windows-1257", 1257 }, { "windows-1258", 1258 }, { "koi8-r", 20866 }, { "koi8-u", 21866 },
                { "shift_jis", 932 }, { "iso-2022-jp", 50220 }, { "euc-jp", 51932 }, { "gb2312", 936 }, { "gbk", 936 },
                { "gb18030", 54936 }, { "big5", 950 }, { "euc-kr", 949 }, { "ks_c_5601-1987", 949 },
                { "us-ascii", 20127 } };
    size_t i;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
        if (_stricmp(cs, map[i].name) == 0)
            return map[i].cp;
    return 0;
}

static size_t cs_utf8(const char *cs, const unsigned char *b, size_t n, char *out, size_t on)
{
    UINT cp = _stricmp(cs, "utf-8") == 0 || _stricmp(cs, "utf8") == 0 ? CP_UTF8 : cs_page(cs);
    wchar_t *w;
    int wl, ol;
    if (on == 0)
        return 0;
    if (n == 0)
        return 0;
    if ((cp == CP_UTF8 || cp == 20127 || cp == 0) && utf8_ok(b, n))
    {
        if (n > on)
            n = on;
        memcpy(out, b, n);
        return n;
    }
    if (cp == CP_UTF8 || cp == 20127 || cp == 0)
        cp = 1252;
    if ((wl = MultiByteToWideChar(cp, 0, (const char *)b, (int)n, NULL, 0)) <= 0
        || (w = (wchar_t *)malloc((size_t)wl * sizeof(wchar_t))) == NULL)
        return 0;
    MultiByteToWideChar(cp, 0, (const char *)b, (int)n, w, wl);
    ol = WideCharToMultiByte(CP_UTF8, 0, w, wl, out, (int)on, NULL, NULL);
    free(w);
    return ol > 0 ? (size_t)ol : 0;
}

static int hexv(int c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static size_t b64_dec(const char *s, size_t n, unsigned char *out)
{
    size_t o = 0, i;
    unsigned v = 0;
    int bits = 0;
    for (i = 0; i < n; ++i)
    {
        int c = (unsigned char)s[i], d;
        d = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
            : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (d < 0)
            continue;
        v = (v << 6) | (unsigned)d;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out[o++] = (unsigned char)(v >> bits);
        }
    }
    return o;
}

static void hdr_decode(const char *in, char *out, size_t n)
{
    size_t o = 0;
    const char *p = in, *pend = NULL;
    while (*p && o + 1 < n)
    {
        const char *q1, *q2, *q3;
        if (p[0] == '=' && p[1] == '?' && (q1 = strchr(p + 2, '?')) != NULL && q1[1] && q1[2] == '?'
            && (q3 = strstr(q1 + 3, "?=")) != NULL)
        {
            char cs[48];
            unsigned char raw[1024];
            size_t cl = (size_t)(q1 - p - 2), rl = 0, tl = (size_t)(q3 - q1 - 3);
            const char *t = q1 + 3, *star;
            char enc = (char)toupper((unsigned char)q1[1]);
            if (cl < sizeof(cs) && tl < sizeof(raw) && (enc == 'B' || enc == 'Q'))
            {
                if (pend)
                    o = (size_t)(pend - out);
                memcpy(cs, p + 2, cl);
                cs[cl] = 0;
                if ((star = strchr(cs, '*')) != NULL)
                    cs[star - cs] = 0;
                if (enc == 'B')
                    rl = b64_dec(t, tl, raw);
                else
                {
                    size_t i;
                    for (i = 0; i < tl; ++i)
                    {
                        if (t[i] == '_')
                            raw[rl++] = ' ';
                        else if (t[i] == '=' && i + 2 < tl && hexv(t[i + 1]) >= 0 && hexv(t[i + 2]) >= 0)
                        {
                            raw[rl++] = (unsigned char)(hexv(t[i + 1]) * 16 + hexv(t[i + 2]));
                            i += 2;
                        }
                        else
                            raw[rl++] = (unsigned char)t[i];
                    }
                }
                o += cs_utf8(cs, raw, rl, out + o, n - 1 - o);
                p = q3 + 2;
                q2 = p;
                while (*q2 == ' ' || *q2 == '\t')
                    ++q2;
                pend = q2[0] == '=' && q2[1] == '?' ? out + o : NULL;
                if (pend)
                {
                    size_t ws = (size_t)(q2 - p);
                    if (o + ws < n - 1)
                    {
                        memcpy(out + o, p, ws);
                        o += ws;
                    }
                    p = q2;
                }
                continue;
            }
        }
        pend = NULL;
        {
            const char *s = p;
            while (*p && !(p[0] == '=' && p[1] == '?' && p != s))
                ++p;
            o += cs_utf8("utf-8", (const unsigned char *)s, (size_t)(p - s), out + o, n - 1 - o);
        }
    }
    out[o] = 0;
}

static int addr_split(const char *v, char (*out)[512], int max)
{
    int n = 0, q = 0, ang = 0, par = 0;
    const char *s = v, *p;
    for (p = v;; ++p)
    {
        if (*p == '"' && (p == v || p[-1] != '\\'))
            q = !q;
        else if (!q && *p == '<')
            ang = 1;
        else if (!q && *p == '>')
            ang = 0;
        else if (!q && *p == '(')
            ++par;
        else if (!q && *p == ')' && par)
            --par;
        if (*p == 0 || (*p == ',' && !q && !ang && !par))
        {
            char raw[512];
            size_t l;
            while (s < p && (*s == ' ' || *s == '\t'))
                ++s;
            l = (size_t)(p - s);
            while (l && (s[l - 1] == ' ' || s[l - 1] == '\t'))
                --l;
            if (l && n < max)
            {
                if (l >= sizeof(raw))
                    l = sizeof(raw) - 1;
                memcpy(raw, s, l);
                raw[l] = 0;
                hdr_decode(raw, out[n++], 512);
            }
            if (*p == 0)
                break;
            s = p + 1;
        }
    }
    return n;
}

static void ml_item(const wchar_t *dir, const char *id, int first)
{
    wchar_t path[MAX_PATH];
    char *t = NULL, raw[4096], dec[4096], date[32];
    static char to[MAIL_RCPTS][512];
    size_t len = 0;
    const char *he;
    int html = 0, att = 0, n, i;
    ml_path(dir, id, L".eml", path);
    if (ml_read(path, &t, &len) != 0)
        return;
    hdr_body(t, t + len, &he);
    snprintf(date, sizeof(date), "%.4s-%.2s-%.2sT%.2s:%.2s:%.2sZ", id, id + 4, id + 6, id + 9, id + 11, id + 13);
    printf("%s{\"id\":", first ? "" : ",");
    ols_jstr(id);
    printf(",\"file\":");
    ols_jwstr(path);
    printf(",\"date\":\"%s\",\"from\":", date);
    hdr_get(t, he, "From", raw, sizeof(raw));
    hdr_decode(raw, dec, sizeof(dec));
    ols_jstr(dec);
    printf(",\"to\":[");
    if (!hdr_get(t, he, "To", raw, sizeof(raw)) || !raw[0])
        hdr_get(t, he, "X-Original-To", raw, sizeof(raw));
    n = addr_split(raw, to, MAIL_RCPTS);
    for (i = 0; i < n; ++i)
    {
        printf("%s", i ? "," : "");
        ols_jstr(to[i]);
    }
    printf("],\"subject\":");
    hdr_get(t, he, "Subject", raw, sizeof(raw));
    hdr_decode(raw, dec, sizeof(dec));
    ols_jstr(dec);
    mime_walk(t, t + len, &html, &att, 0);
    printf(",\"size\":%llu,\"html\":%s,\"attachments\":%d}", (unsigned long long)len, html ? "true" : "false", att);
    free(t);
}

static int ml_list(const wchar_t *root)
{
    wchar_t dir[MAX_PATH];
    char **names;
    int n, i;
    ml_dir(root, dir);
    n = ml_names(dir, &names);
    printf("[");
    for (i = 0; i < n; ++i)
        ml_item(dir, names[i], i == 0);
    printf("]\n");
    ml_names_free(names, n);
    return 0;
}

static int ml_remove(const wchar_t *root, const char *id)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    if (!ml_id_ok(id))
    {
        fprintf(stderr, "use a mail id from openlitespeed --mail list\n");
        return 2;
    }
    ml_dir(root, dir);
    ml_path(dir, id, L".eml", path);
    if (!DeleteFileW(path))
    {
        fprintf(stderr, GetLastError() == ERROR_FILE_NOT_FOUND ? "there is no mail %s\n" : "mail %s could not be removed\n",
                id);
        return GetLastError() == ERROR_FILE_NOT_FOUND ? 2 : 1;
    }
    printf("removed %s\n", id);
    return 0;
}

static int ml_clear(const wchar_t *root)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    char **names;
    int n, i, gone = 0;
    ml_dir(root, dir);
    n = ml_names(dir, &names);
    for (i = 0; i < n; ++i)
    {
        ml_path(dir, names[i], L".eml", path);
        if (DeleteFileW(path))
            ++gone;
    }
    ml_names_free(names, n);
    printf("removed %d message%s\n", gone, gone == 1 ? "" : "s");
    return gone == n ? 0 : 1;
}

static int ml_port_free(int port)
{
    WSADATA wd;
    SOCKET s;
    WSAStartup(MAKEWORD(2, 2), &wd);
    if ((s = ml_listen(port)) == INVALID_SOCKET)
        return 0;
    closesocket(s);
    return 1;
}

static int ml_spawn(void)
{
    return ols_helper_spawn(L"--mail run");
}

static int ml_wait(const wchar_t *root, int up, int ms)
{
    int i;
    for (i = 0; i < ms / 100; ++i)
    {
        if ((ml_pid(root) != 0) == up)
            return 0;
        Sleep(100);
    }
    return (ml_pid(root) != 0) == up ? 0 : -1;
}

int ols_mail_ensure(const wchar_t *root)
{
    wchar_t ini[MAX_PATH];
    ml_ini(root, ini);
    if (!GetPrivateProfileIntW(L"mail", L"on", 0, ini) || ml_pid(root) != 0)
        return 0;
    return ml_spawn();
}

int ols_mail_halt(const wchar_t *root)
{
    wchar_t stopf[MAX_PATH], pidf[MAX_PATH];
    DWORD pid = ml_pid(root);
    HANDLE h;
    if (!pid)
        return 0;
    ml_tmpfile(root, L"mail.stop", stopf);
    ml_tmpfile(root, L"mail.pid", pidf);
    ols_write_wfile(stopf, "1\n", 2, 0);
    if (ml_wait(root, 0, 5000) == 0)
        return 0;
    if ((h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid)) != NULL)
    {
        TerminateProcess(h, 1);
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
    }
    DeleteFileW(stopf);
    if (ml_pid(root) != 0)
        return -1;
    DeleteFileW(pidf);
    return 0;
}

void ols_mail_summary(const wchar_t *root, const wchar_t *ini)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int count = 0;
    ml_dir(root, dir);
    swprintf(pat, MAX_PATH, L"%s\\*.eml", dir);
    f = FindFirstFileW(pat, &fd);
    if (f != INVALID_HANDLE_VALUE)
    {
        do
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcslen(fd.cFileName) == 28)
                ++count;
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    printf(",\"mail\":{\"on\":%s,\"running\":%s,\"port\":%d,\"count\":%d}",
           GetPrivateProfileIntW(L"mail", L"on", 0, ini) ? "true" : "false", ml_pid(root) ? "true" : "false", ml_port(ini),
           count);
}

int ols_mail_laravel_env(const wchar_t *root, const wchar_t *dir)
{
    static const char *const keys[] = { "MAIL_MAILER=", "MAIL_HOST=", "MAIL_PORT=" };
    wchar_t ini[MAX_PATH], env[MAX_PATH];
    char *t = NULL, *out, *p, vals[3][32];
    size_t len, cap, o = 0;
    int seen[3] = { 0, 0, 0 }, k, r;
    ml_ini(root, ini);
    if (!GetPrivateProfileIntW(L"mail", L"on", 0, ini))
        return 0;
    swprintf(env, MAX_PATH, L"%s\\.env", dir);
    if (ml_read(env, &t, &len) != 0)
        return -1;
    strcpy(vals[0], "smtp");
    strcpy(vals[1], "127.0.0.1");
    snprintf(vals[2], sizeof(vals[2]), "%d", ml_port(ini));
    cap = len + 256;
    if ((out = (char *)malloc(cap)) == NULL)
    {
        free(t);
        return -1;
    }
    for (p = t; *p;)
    {
        char *e = strchr(p, '\n');
        size_t l = e ? (size_t)(e - p + 1) : strlen(p);
        for (k = 0; k < 3; ++k)
            if (strncmp(p, keys[k], strlen(keys[k])) == 0)
                break;
        if (k < 3)
        {
            o += (size_t)snprintf(out + o, cap - o, "%s%s%s", keys[k], vals[k], e && e > p && e[-1] == '\r' ? "\r\n" : "\n");
            seen[k] = 1;
        }
        else
        {
            memcpy(out + o, p, l);
            o += l;
        }
        p += l;
    }
    for (k = 0; k < 3; ++k)
        if (!seen[k])
            o += (size_t)snprintf(out + o, cap - o, "%s%s%s\n", o && out[o - 1] != '\n' ? "\n" : "", keys[k], vals[k]);
    r = ols_write_wfile(env, out, o, 0);
    free(out);
    free(t);
    return r;
}

static int ml_apply(const wchar_t *root, int on)
{
    int changed = 0;
    if (ols_php_mail_sync(root, on, &changed) != 0)
        return -1;
    if (changed && ols_server_running() && ols_win_reload_running() < 0)
        return -1;
    return changed;
}

int ols_mail_main(int argc, char **argv)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH], v[16];
    const char *act = argc >= 1 ? argv[0] : "";
    int port, svc, r;
    if (ml_root(root) != 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    ml_ini(root, ini);
    port = ml_port(ini);
    if (strcmp(act, "sendmail") == 0)
        return ml_sendmail(root, argc - 1, argv + 1);
    if (strcmp(act, "run") == 0)
        return ml_run(root);
    if (strcmp(act, "list") == 0)
        return ml_list(root);
    if (strcmp(act, "remove") == 0 && argc >= 2)
        return ml_remove(root, argv[1]);
    if (strcmp(act, "clear") == 0)
        return ml_clear(root);
    if (strcmp(act, "status") == 0)
    {
        if (!GetPrivateProfileIntW(L"mail", L"on", 0, ini))
            printf("off\n");
        else
            printf("on %d %s\n", port, ml_pid(root) ? "running" : "stopped");
        return 0;
    }
    if (strcmp(act, "on") == 0)
    {
        swprintf(v, 16, L"1");
        if (!WritePrivateProfileStringW(L"mail", L"on", v, ini))
        {
            fprintf(stderr, "cannot write %ls\n", ini);
            return 1;
        }
        svc = ols_service_mode();
        if (ml_pid(root) == 0 && !ml_port_free(port))
        {
            ml_apply(root, 1);
            fprintf(stderr, "port %d is busy, so the mail catcher is not running; PHP mail is still caught\n", port);
            return 1;
        }
        if ((r = ml_apply(root, 1)) < 0)
        {
            fprintf(stderr, "PHP could not be set up to deliver mail to the catcher\n");
            return 1;
        }
        if (ml_pid(root) == 0)
        {
            if (svc == 0)
                ml_spawn();
            else if (svc == 2 && r == 0)
                ols_win_reload_running();
            if (svc != 1 && ml_wait(root, 1, 10000) != 0)
            {
                fprintf(stderr, "the mail catcher did not start\n");
                return 1;
            }
        }
        printf("Mail catcher on, SMTP 127.0.0.1:%d\n", port);
        return 0;
    }
    if (strcmp(act, "off") == 0)
    {
        if (!WritePrivateProfileStringW(L"mail", L"on", L"0", ini))
        {
            fprintf(stderr, "cannot write %ls\n", ini);
            return 1;
        }
        if (ols_mail_halt(root) != 0)
        {
            fprintf(stderr, "the mail catcher did not stop\n");
            return 1;
        }
        if (ml_apply(root, 0) < 0)
        {
            fprintf(stderr, "php.ini could not be changed back\n");
            return 1;
        }
        printf("Mail catcher off\n");
        return 0;
    }
    fprintf(stderr, "usage: openlitespeed --mail on | off | status | list | remove <id> | clear |\n"
                    "       sendmail [-t] [-i] [-f <from>] [<recipient>...]\n");
    return 2;
}
