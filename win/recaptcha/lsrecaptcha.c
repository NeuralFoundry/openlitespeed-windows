#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

#define MAX_BODY 65536

static char *read_body(void)
{
    const char *cl = getenv("CONTENT_LENGTH");
    long n = cl ? strtol(cl, NULL, 10) : 0;
    if (n <= 0 || n > MAX_BODY)
        return NULL;
    char *buf = (char *)malloc(n + 1);
    if (!buf)
        return NULL;
    long got = 0;
    while (got < n)
    {
        int r = _read(0, buf + got, (unsigned)(n - got));
        if (r <= 0)
            break;
        got += r;
    }
    buf[got] = 0;
    return buf;
}

static int hex(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s)
{
    char *o = s;
    for (; *s; ++s)
    {
        if (*s == '+')
            *o++ = ' ';
        else if (*s == '%' && hex(s[1]) >= 0 && hex(s[2]) >= 0)
        {
            *o++ = (char)(hex(s[1]) * 16 + hex(s[2]));
            s += 2;
        }
        else
            *o++ = *s;
    }
    *o = 0;
}

static char *form_value(char *body, const char *name)
{
    size_t nl = strlen(name);
    char *p = body;
    while (p && *p)
    {
        char *end = strchr(p, '&');
        if (end)
            *end = 0;
        if (strncmp(p, name, nl) == 0 && p[nl] == '=')
        {
            char *v = _strdup(p + nl + 1);
            if (v)
                url_decode(v);
            return v;
        }
        p = end ? end + 1 : NULL;
    }
    return NULL;
}

static void url_encode(const char *s, char *out, size_t max)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t n = 0;
    for (; *s && n + 4 < max; ++s)
    {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'
            || c == '.' || c == '~')
            out[n++] = (char)c;
        else
        {
            out[n++] = '%';
            out[n++] = digits[c >> 4];
            out[n++] = digits[c & 15];
        }
    }
    out[n] = 0;
}

static int json_success(const char *body)
{
    const char *p = strstr(body, "\"success\"");
    if (!p)
        return 0;
    p += 9;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    if (*p++ != ':')
        return 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    return strncmp(p, "true", 4) == 0;
}

static int verify(const wchar_t *host, const wchar_t *path, const char *data, char *resp, DWORD max)
{
    int ok = 0;
    DWORD got = 0;
    HINTERNET ses = WinHttpOpen(L"OpenLiteSpeed", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses)
        ses = WinHttpOpen(L"OpenLiteSpeed", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses)
        return 0;
    WinHttpSetTimeouts(ses, 10000, 10000, 10000, 10000);
    HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"POST", path, NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
    DWORD len = (DWORD)strlen(data);
    if (req && WinHttpSendRequest(req, L"Content-Type: application/x-www-form-urlencoded\r\n", (DWORD)-1L,
                                  (LPVOID)data, len, len, 0)
        && WinHttpReceiveResponse(req, NULL))
    {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &sz, WINHTTP_NO_HEADER_INDEX);
        DWORD n;
        while (got + 1 < max && WinHttpReadData(req, resp + got, max - 1 - got, &n) && n)
            got += n;
        resp[got] = 0;
        if (status == 200)
            ok = json_success(resp);
        else
            fprintf(stderr, "[RECAPTCHA] verification server answered %lu\n", status);
    }
    else
        fprintf(stderr, "[RECAPTCHA] cannot reach the verification server (error %lu)\n", GetLastError());
    if (req)
        WinHttpCloseHandle(req);
    if (con)
        WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

static int url_chars(const char *p, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        unsigned char c = (unsigned char)p[i];
        if (c < 0x21 || c > 0x7e || c == '"' || c == '<' || c == '>' || c == '\\')
            return 0;
    }
    return 1;
}

static int safe_path(const char *p, size_t n)
{
    return n > 0 && p[0] == '/' && !(n > 1 && (p[1] == '/' || p[1] == '\\')) && url_chars(p, n);
}

static void location(char *out, size_t max)
{
    const char *ref = getenv("HTTP_REFERER");
    const char *qs = getenv("QUERY_STRING");
    const char *p = NULL;
    size_t n = 0;
    if (ref)
    {
        const char *s = strstr(ref, "://");
        p = s ? strchr(s + 3, '/') : (ref[0] == '/' ? ref : NULL);
        if (p)
            n = strcspn(p, "?#");
    }
    if (!p || !safe_path(p, n) || n + 1 >= max)
    {
        p = "/";
        n = 1;
    }
    memcpy(out, p, n);
    out[n] = 0;
    size_t ql = qs ? strlen(qs) : 0;
    if (ql && n + ql + 2 < max && url_chars(qs, ql))
    {
        out[n] = '?';
        memcpy(out + n + 1, qs, ql + 1);
    }
}

int main(void)
{
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    int passed = 0;
    const char *method = getenv("REQUEST_METHOD");
    if (method && strcmp(method, "POST") == 0)
    {
        const char *obj = getenv("LSRECAPTCHA_JS_OBJ_NAME");
        int hcaptcha = obj && strcmp(obj, "hcaptcha") == 0;
        const char *secret = getenv("LSRECAPTCHA_SECRET_KEY");
        const char *ip = getenv("REMOTE_ADDR");
        char *body = read_body();
        char *token = body ? form_value(body, hcaptcha ? "h-captcha-response" : "g-recaptcha-response") : NULL;
        if (!token || !*token)
            fprintf(stderr, "[RECAPTCHA] POST without a %s value\n", hcaptcha ? "h-captcha-response" : "g-recaptcha-response");
        else if (!secret || !*secret)
            fprintf(stderr, "[RECAPTCHA] no secret key for this request\n");
        else
        {
            size_t cap = strlen(token) * 3 + strlen(secret) * 3 + 128;
            char *data = (char *)malloc(cap), *e1 = (char *)malloc(cap), *e2 = (char *)malloc(cap),
                 *e3 = (char *)malloc(cap), *resp = (char *)malloc(MAX_BODY);
            if (data && e1 && e2 && e3 && resp)
            {
                url_encode(secret, e1, cap);
                url_encode(ip ? ip : "", e2, cap);
                url_encode(token, e3, cap);
                snprintf(data, cap, "secret=%s&remoteip=%s&response=%s", e1, e2, e3);
                passed = verify(hcaptcha ? L"api.hcaptcha.com" : L"www.google.com",
                                hcaptcha ? L"/siteverify" : L"/recaptcha/api/siteverify", data, resp, MAX_BODY);
                if (!passed && *resp)
                {
                    for (char *c = resp; *c; ++c)
                        if (*c == '\r' || *c == '\n')
                            *c = ' ';
                    fprintf(stderr, "[RECAPTCHA] not passed: %.300s\n", resp);
                }
            }
            free(data);
            free(e1);
            free(e2);
            free(e3);
            free(resp);
        }
        free(token);
        free(body);
    }
    char loc[4096];
    location(loc, sizeof(loc));
    printf("Status: 301 Moved Permanently\r\n");
    if (passed)
        printf("Lsrecaptcha: 1\r\n");
    printf("Location: %s\r\nCache-Control: no-store\r\n\r\n", loc);
    fflush(stdout);
    return 0;
}
