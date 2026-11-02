#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#undef X509_NAME
#undef X509_EXTENSIONS
#undef X509_CERT_PAIR
#undef PKCS7_ISSUER_AND_SERIAL
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/obj.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>
#include <utility>
#include <vector>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

typedef void (*ols_acme_log_fn)(int error, const char *msg);

namespace
{

typedef std::string S;

ols_acme_log_fn g_log;

void say(int error, const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (g_log)
        g_log(error, buf);
    else
        fprintf(error ? stderr : stdout, "%s\n", buf);
}

std::wstring W(const S &s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, L'\0');
    if (n)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

S U(const std::wstring &w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    S s(n, '\0');
    if (n)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

bool read_file(const S &path, S &out)
{
    HANDLE h = CreateFileW(W(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER sz;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (1 << 24);
    if (ok)
    {
        out.assign((size_t)sz.QuadPart, '\0');
        DWORD got = 0;
        ok = sz.QuadPart == 0 || (ReadFile(h, &out[0], (DWORD)sz.QuadPart, &got, NULL) && got == sz.QuadPart);
    }
    CloseHandle(h);
    return ok;
}

bool write_file(const S &path, const S &data)
{
    std::wstring tmp = W(path + ".new");
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD put = 0;
    bool ok = data.empty() || (WriteFile(h, data.data(), (DWORD)data.size(), &put, NULL) && put == data.size());
    ok = FlushFileBuffers(h) && ok;
    CloseHandle(h);
    if (ok)
        ok = MoveFileExW(tmp.c_str(), W(path).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok)
        DeleteFileW(tmp.c_str());
    return ok;
}

bool exists(const S &path)
{
    return GetFileAttributesW(W(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool make_dirs(const S &path)
{
    std::wstring w = W(path);
    for (size_t i = 3; i <= w.size(); ++i)
        if (i == w.size() || w[i] == L'\\' || w[i] == L'/')
        {
            std::wstring part = w.substr(0, i);
            if (GetFileAttributesW(part.c_str()) == INVALID_FILE_ATTRIBUTES && !CreateDirectoryW(part.c_str(), NULL)
                && GetLastError() != ERROR_ALREADY_EXISTS)
                return false;
        }
    return true;
}

S b64u(const unsigned char *d, size_t n)
{
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    S o;
    for (size_t i = 0; i < n; i += 3)
    {
        unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o += t[v >> 18 & 63];
        o += t[v >> 12 & 63];
        if (i + 1 < n)
            o += t[v >> 6 & 63];
        if (i + 2 < n)
            o += t[v & 63];
    }
    return o;
}

S b64u(const S &s)
{
    return b64u((const unsigned char *)s.data(), s.size());
}

S jstr(const S &s)
{
    S o = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
        {
            o += '\\';
            o += (char)c;
        }
        else if (c < 0x20)
        {
            char b[8];
            snprintf(b, sizeof(b), "\\u%04x", c);
            o += b;
        }
        else
            o += (char)c;
    }
    return o + "\"";
}

struct J
{
    enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
    bool b = false;
    double n = 0;
    S s;
    std::vector<J> a;
    std::vector<std::pair<S, J>> o;

    const J *get(const char *k) const
    {
        for (auto &p : o)
            if (p.first == k)
                return &p.second;
        return nullptr;
    }
    S str(const char *k) const
    {
        const J *v = get(k);
        return v && v->t == STR ? v->s : S();
    }
};

int hexv(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

void utf8(S &out, unsigned v)
{
    if (v < 0x80)
        out += (char)v;
    else if (v < 0x800)
    {
        out += (char)(0xC0 | v >> 6);
        out += (char)(0x80 | (v & 63));
    }
    else if (v < 0x10000)
    {
        out += (char)(0xE0 | v >> 12);
        out += (char)(0x80 | (v >> 6 & 63));
        out += (char)(0x80 | (v & 63));
    }
    else
    {
        out += (char)(0xF0 | v >> 18);
        out += (char)(0x80 | (v >> 12 & 63));
        out += (char)(0x80 | (v >> 6 & 63));
        out += (char)(0x80 | (v & 63));
    }
}

struct JP
{
    const char *p, *e;

    void ws()
    {
        while (p < e && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            ++p;
    }
    bool lit(const char *w)
    {
        size_t n = strlen(w);
        if ((size_t)(e - p) >= n && memcmp(p, w, n) == 0)
        {
            p += n;
            return true;
        }
        return false;
    }
    bool hex4(unsigned &v)
    {
        if (e - p < 4)
            return false;
        v = 0;
        for (int i = 0; i < 4; ++i)
        {
            int h = hexv(p[i]);
            if (h < 0)
                return false;
            v = v * 16 + h;
        }
        p += 4;
        return true;
    }
    bool str(S &out)
    {
        if (p >= e || *p != '"')
            return false;
        ++p;
        while (p < e && *p != '"')
        {
            char c = *p++;
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (p >= e)
                return false;
            c = *p++;
            if (c == 'n')
                out += '\n';
            else if (c == 't')
                out += '\t';
            else if (c == 'r')
                out += '\r';
            else if (c == 'b')
                out += '\b';
            else if (c == 'f')
                out += '\f';
            else if (c == 'u')
            {
                unsigned v, w;
                if (!hex4(v))
                    return false;
                if (v >= 0xD800 && v < 0xDC00 && e - p >= 6 && p[0] == '\\' && p[1] == 'u')
                {
                    const char *save = p;
                    p += 2;
                    if (hex4(w) && w >= 0xDC00 && w < 0xE000)
                        v = 0x10000 + ((v - 0xD800) << 10) + (w - 0xDC00);
                    else
                        p = save;
                }
                utf8(out, v);
            }
            else
                out += c;
        }
        if (p >= e)
            return false;
        ++p;
        return true;
    }
    bool val(J &v, int depth)
    {
        if (depth > 32)
            return false;
        ws();
        if (p >= e)
            return false;
        if (*p == '{')
        {
            v.t = J::OBJ;
            ++p;
            ws();
            if (p < e && *p == '}')
            {
                ++p;
                return true;
            }
            for (;;)
            {
                ws();
                S k;
                J c;
                if (!str(k))
                    return false;
                ws();
                if (p >= e || *p != ':')
                    return false;
                ++p;
                if (!val(c, depth + 1))
                    return false;
                v.o.emplace_back(k, std::move(c));
                ws();
                if (p < e && *p == ',')
                {
                    ++p;
                    continue;
                }
                if (p < e && *p == '}')
                {
                    ++p;
                    return true;
                }
                return false;
            }
        }
        if (*p == '[')
        {
            v.t = J::ARR;
            ++p;
            ws();
            if (p < e && *p == ']')
            {
                ++p;
                return true;
            }
            for (;;)
            {
                J c;
                if (!val(c, depth + 1))
                    return false;
                v.a.push_back(std::move(c));
                ws();
                if (p < e && *p == ',')
                {
                    ++p;
                    continue;
                }
                if (p < e && *p == ']')
                {
                    ++p;
                    return true;
                }
                return false;
            }
        }
        if (*p == '"')
        {
            v.t = J::STR;
            return str(v.s);
        }
        if (lit("true"))
        {
            v.t = J::BOOL;
            v.b = true;
            return true;
        }
        if (lit("false"))
        {
            v.t = J::BOOL;
            return true;
        }
        if (lit("null"))
            return true;
        char *end;
        v.n = strtod(p, &end);
        if (end == p || end > e)
            return false;
        v.t = J::NUM;
        p = end;
        return true;
    }
};

bool parse_json(const S &s, J &out)
{
    out = J();
    JP jp = {s.c_str(), s.c_str() + s.size()};
    if (!jp.val(out, 0))
        return false;
    jp.ws();
    return jp.p == jp.e;
}

struct Resp
{
    DWORD status = 0;
    S body, location, nonce, ctype;
};

S header(HINTERNET req, DWORD query, const wchar_t *name)
{
    DWORD len = 0;
    WinHttpQueryHeaders(req, query, name, WINHTTP_NO_OUTPUT_BUFFER, &len, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0)
        return S();
    std::wstring w(len / sizeof(wchar_t) + 1, L'\0');
    if (!WinHttpQueryHeaders(req, query, name, &w[0], &len, WINHTTP_NO_HEADER_INDEX))
        return S();
    w.resize(len / sizeof(wchar_t));
    return U(w);
}

struct Http
{
    HINTERNET ses = NULL;
    HCERTSTORE roots = NULL;
    HCERTCHAINENGINE engine = NULL;

    ~Http()
    {
        if (engine)
            CertFreeCertificateChainEngine(engine);
        if (roots)
            CertCloseStore(roots, 0);
        if (ses)
            WinHttpCloseHandle(ses);
    }

    bool init(const S &caFile)
    {
        ses = WinHttpOpen(L"OpenLiteSpeed-ACME", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!ses)
            ses = WinHttpOpen(L"OpenLiteSpeed-ACME", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
        if (!ses)
            return false;
        WinHttpSetTimeouts(ses, 15000, 15000, 30000, 30000);
        S pem;
        if (caFile.empty() || !read_file(caFile, pem))
            return true;
        roots = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
        BIO *bio = BIO_new_mem_buf(pem.data(), (int)pem.size());
        X509 *x;
        int n = 0;
        while (roots && bio && (x = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL)
        {
            unsigned char *der = NULL;
            int len = i2d_X509(x, &der);
            if (len > 0 && CertAddEncodedCertificateToStore(roots, X509_ASN_ENCODING, der, len,
                                                             CERT_STORE_ADD_ALWAYS, NULL))
                ++n;
            OPENSSL_free(der);
            X509_free(x);
        }
        BIO_free(bio);
        if (!n)
        {
            say(1, "no certificate found in %s", caFile.c_str());
            return false;
        }
        CERT_CHAIN_ENGINE_CONFIG cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.cbSize = sizeof(cfg);
        cfg.hExclusiveRoot = roots;
        return CertCreateCertificateChainEngine(&cfg, &engine) != 0;
    }

    bool trusted(HINTERNET req)
    {
        PCCERT_CONTEXT cert = NULL;
        DWORD len = sizeof(cert);
        if (!WinHttpQueryOption(req, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &cert, &len) || !cert)
            return false;
        CERT_CHAIN_PARA para;
        memset(&para, 0, sizeof(para));
        para.cbSize = sizeof(para);
        PCCERT_CHAIN_CONTEXT chain = NULL;
        bool ok = CertGetCertificateChain(engine, cert, NULL, cert->hCertStore, &para, 0, NULL, &chain) && chain
                  && chain->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR;
        if (chain)
            CertFreeCertificateChain(chain);
        CertFreeCertificateContext(cert);
        return ok;
    }

    bool request(const char *method, const S &url, const S &body, const char *ctype, const char *accept, Resp &r)
    {
        std::wstring wurl = W(url);
        URL_COMPONENTS uc;
        memset(&uc, 0, sizeof(uc));
        uc.dwStructSize = sizeof(uc);
        uc.dwHostNameLength = (DWORD)-1;
        uc.dwUrlPathLength = (DWORD)-1;
        uc.dwExtraInfoLength = (DWORD)-1;
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS)
        {
            say(1, "not an https URL: %s", url.c_str());
            return false;
        }
        std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
        std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
        if (uc.lpszExtraInfo)
            path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        if (path.empty())
            path = L"/";
        bool ok = false;
        HINTERNET con = WinHttpConnect(ses, host.c_str(), uc.nPort, 0);
        HINTERNET req = con ? WinHttpOpenRequest(con, W(method).c_str(), path.c_str(), NULL, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                            : NULL;
        if (req && engine)
        {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA;
            WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
        }
        std::wstring hdrs;
        if (ctype)
            hdrs += L"Content-Type: " + W(ctype) + L"\r\n";
        if (accept)
            hdrs += L"Accept: " + W(accept) + L"\r\n";
        if (req
            && WinHttpSendRequest(req, hdrs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdrs.c_str(), (DWORD)-1L,
                                  body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(),
                                  (DWORD)body.size(), 0)
            && WinHttpReceiveResponse(req, NULL))
        {
            if (engine && !trusted(req))
                say(1, "the certificate of %s is not issued by the configured CA", U(host).c_str());
            else
            {
                DWORD sz = sizeof(r.status);
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &r.status, &sz, WINHTTP_NO_HEADER_INDEX);
                r.location = header(req, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX);
                r.nonce = header(req, WINHTTP_QUERY_CUSTOM, L"Replay-Nonce");
                r.ctype = header(req, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX);
                char buf[16384];
                DWORD n;
                ok = true;
                while (WinHttpReadData(req, buf, sizeof(buf), &n) && n)
                {
                    r.body.append(buf, n);
                    if (r.body.size() > (8 << 20))
                    {
                        ok = false;
                        break;
                    }
                }
            }
        }
        else
            say(1, "request to %s failed (error %lu)", url.c_str(), GetLastError());
        if (req)
            WinHttpCloseHandle(req);
        if (con)
            WinHttpCloseHandle(con);
        return ok;
    }
};

EC_KEY *new_key()
{
    EC_KEY *k = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (k && !EC_KEY_generate_key(k))
    {
        EC_KEY_free(k);
        return NULL;
    }
    return k;
}

S key_pem(EC_KEY *k)
{
    S out;
    BIO *bio = BIO_new(BIO_s_mem());
    if (bio && PEM_write_bio_ECPrivateKey(bio, k, NULL, NULL, 0, NULL, NULL))
    {
        const uint8_t *p;
        size_t n;
        if (BIO_mem_contents(bio, &p, &n))
            out.assign((const char *)p, n);
    }
    BIO_free(bio);
    return out;
}

EC_KEY *load_key(const S &path, bool create)
{
    S pem;
    if (read_file(path, pem))
    {
        BIO *bio = BIO_new_mem_buf(pem.data(), (int)pem.size());
        EC_KEY *k = bio ? PEM_read_bio_ECPrivateKey(bio, NULL, NULL, NULL) : NULL;
        BIO_free(bio);
        if (!k)
            say(1, "%s is not an EC private key", path.c_str());
        return k;
    }
    if (!create)
        return NULL;
    EC_KEY *k = new_key();
    if (k && !write_file(path, key_pem(k)))
    {
        say(1, "cannot write %s", path.c_str());
        EC_KEY_free(k);
        return NULL;
    }
    return k;
}

S sha256_b64u(const S &data)
{
    unsigned char h[SHA256_DIGEST_LENGTH];
    SHA256((const uint8_t *)data.data(), data.size(), h);
    return b64u(h, sizeof(h));
}

S iso_time(time_t t)
{
    struct tm tmv;
    char buf[32];
    gmtime_s(&tmv, &t);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

bool valid_token(const S &t)
{
    if (t.size() < 16)
        return false;
    for (char c : t)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    return true;
}

struct Acme
{
    Http http;
    EC_KEY *key = NULL;
    S jwk, thumb, kid, nonce;
    J dir;

    ~Acme()
    {
        EC_KEY_free(key);
    }

    bool use_key(EC_KEY *k)
    {
        key = k;
        unsigned char pt[65];
        if (EC_POINT_point2oct(EC_KEY_get0_group(k), EC_KEY_get0_public_key(k), POINT_CONVERSION_UNCOMPRESSED, pt,
                               sizeof(pt), NULL)
            != sizeof(pt))
            return false;
        jwk = "{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\"" + b64u(pt + 1, 32) + "\",\"y\":\"" + b64u(pt + 33, 32)
              + "\"}";
        thumb = sha256_b64u(jwk);
        return true;
    }

    S sign(const S &data)
    {
        unsigned char h[SHA256_DIGEST_LENGTH], rs[64];
        SHA256((const uint8_t *)data.data(), data.size(), h);
        ECDSA_SIG *sig = ECDSA_do_sign(h, sizeof(h), key);
        if (!sig)
            return S();
        const BIGNUM *r, *s;
        ECDSA_SIG_get0(sig, &r, &s);
        bool ok = BN_bn2bin_padded(rs, 32, r) && BN_bn2bin_padded(rs + 32, 32, s);
        ECDSA_SIG_free(sig);
        return ok ? b64u(rs, sizeof(rs)) : S();
    }

    static S problem(const Resp &r)
    {
        J p;
        if (parse_json(r.body, p) && p.t == J::OBJ)
        {
            S d = p.str("detail"), t = p.str("type");
            const J *sub = p.get("subproblems");
            if (sub && sub->t == J::ARR)
                for (auto &x : sub->a)
                    if (x.t == J::OBJ && !x.str("detail").empty())
                        d += "; " + x.str("detail");
            if (!d.empty() || !t.empty())
                return t + (d.empty() ? S() : ": " + d);
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "HTTP status %lu", r.status);
        return buf;
    }

    bool directory(const S &url)
    {
        Resp r;
        if (!http.request("GET", url, S(), NULL, NULL, r))
            return false;
        if (r.status != 200 || !parse_json(r.body, dir) || dir.t != J::OBJ || dir.str("newNonce").empty()
            || dir.str("newAccount").empty() || dir.str("newOrder").empty())
        {
            say(1, "%s is not an ACME directory (%s)", url.c_str(), problem(r).c_str());
            return false;
        }
        return true;
    }

    bool fresh_nonce()
    {
        Resp r;
        if (!http.request("HEAD", dir.str("newNonce"), S(), NULL, NULL, r) || r.nonce.empty())
        {
            say(1, "the ACME server did not send a nonce");
            return false;
        }
        nonce = r.nonce;
        return true;
    }

    bool post(const S &url, const S &payload, Resp &r, bool withJwk = false, const char *accept = NULL)
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            if (nonce.empty() && !fresh_nonce())
                return false;
            S prot = "{\"alg\":\"ES256\",\"nonce\":" + jstr(nonce) + ",\"url\":" + jstr(url) + ","
                     + (withJwk ? "\"jwk\":" + jwk : "\"kid\":" + jstr(kid)) + "}";
            S p64 = b64u(prot), b64 = payload.empty() ? S() : b64u(payload), sig = sign(p64 + "." + b64);
            if (sig.empty())
                return false;
            S body = "{\"protected\":\"" + p64 + "\",\"payload\":\"" + b64 + "\",\"signature\":\"" + sig + "\"}";
            nonce.clear();
            r = Resp();
            if (!http.request("POST", url, body, "application/jose+json", accept, r))
                return false;
            nonce = r.nonce;
            if (r.status == 400 && r.body.find("urn:ietf:params:acme:error:badNonce") != S::npos)
                continue;
            return true;
        }
        say(1, "the ACME server rejected every nonce");
        return false;
    }

    bool account(const S &email, bool create)
    {
        S payload = create ? "{\"termsOfServiceAgreed\":true" : "{\"onlyReturnExisting\":true";
        if (create && !email.empty())
            payload += ",\"contact\":[" + jstr("mailto:" + email) + "]";
        payload += "}";
        Resp r;
        if (!post(dir.str("newAccount"), payload, r, true))
            return false;
        if ((r.status != 200 && r.status != 201) || r.location.empty())
        {
            say(1, "account registration failed: %s", problem(r).c_str());
            return false;
        }
        kid = r.location;
        return true;
    }

    bool fetch(const S &url, J &out)
    {
        Resp r;
        if (!post(url, S(), r))
            return false;
        if (r.status != 200 || !parse_json(r.body, out) || out.t != J::OBJ)
        {
            say(1, "%s: %s", url.c_str(), problem(r).c_str());
            return false;
        }
        return true;
    }

    bool authorize(const S &url)
    {
        J a;
        if (!fetch(url, a))
            return false;
        S ident;
        if (const J *id = a.get("identifier"))
            ident = id->str("value");
        if (a.str("status") == "valid")
            return true;
        if (a.str("status") != "pending")
        {
            say(1, "authorization for %s is %s", ident.c_str(), a.str("status").c_str());
            return false;
        }
        S chUrl, token;
        if (const J *chs = a.get("challenges"))
            for (auto &c : chs->a)
                if (c.t == J::OBJ && c.str("type") == "http-01")
                {
                    chUrl = c.str("url");
                    token = c.str("token");
                }
        if (chUrl.empty() || !valid_token(token))
        {
            say(1, "the ACME server offers no http-01 challenge for %s", ident.c_str());
            return false;
        }
        say(0, "validating %s over HTTP", ident.c_str());
        Resp r;
        if (!post(chUrl, "{}", r))
            return false;
        if (r.status != 200)
        {
            say(1, "challenge for %s refused: %s", ident.c_str(), problem(r).c_str());
            return false;
        }
        for (int i = 0; i < 60; ++i)
        {
            Sleep(i < 5 ? 1000 : 3000);
            if (!fetch(url, a))
                return false;
            S st = a.str("status");
            if (st == "valid")
                return true;
            if (st != "pending" && st != "processing")
            {
                S why = st;
                if (const J *chs = a.get("challenges"))
                    for (auto &c : chs->a)
                        if (const J *err = c.get("error"))
                            why = err->str("type") + ": " + err->str("detail");
                say(1, "validation of %s failed: %s", ident.c_str(), why.c_str());
                return false;
            }
        }
        say(1, "validation of %s did not finish in time", ident.c_str());
        return false;
    }
};

S make_csr(EC_KEY *k, const std::vector<S> &names)
{
    S out;
    EVP_PKEY *pkey = EVP_PKEY_new();
    X509_REQ *req = X509_REQ_new();
    GENERAL_NAMES *gens = sk_GENERAL_NAME_new_null();
    STACK_OF(X509_EXTENSION) *exts = sk_X509_EXTENSION_new_null();
    bool ok = pkey && req && gens && exts && EC_KEY_up_ref(k) && EVP_PKEY_assign_EC_KEY(pkey, k);
    if (ok)
    {
        X509_NAME *name = X509_REQ_get_subject_name(req);
        ok = X509_REQ_set_version(req, 0) && X509_REQ_set_pubkey(req, pkey)
             && X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, (const unsigned char *)names[0].c_str(), -1,
                                           -1, 0);
    }
    for (size_t i = 0; ok && i < names.size(); ++i)
    {
        GENERAL_NAME *g = GENERAL_NAME_new();
        ASN1_IA5STRING *ia5 = ASN1_IA5STRING_new();
        ok = g && ia5 && ASN1_STRING_set(ia5, names[i].data(), (int)names[i].size());
        if (ok)
        {
            GENERAL_NAME_set0_value(g, GEN_DNS, ia5);
            ia5 = NULL;
            ok = sk_GENERAL_NAME_push(gens, g) != 0;
            if (ok)
                g = NULL;
        }
        ASN1_IA5STRING_free(ia5);
        GENERAL_NAME_free(g);
    }
    X509_EXTENSION *ext = ok ? X509V3_EXT_i2d(NID_subject_alt_name, 0, gens) : NULL;
    ok = ext && sk_X509_EXTENSION_push(exts, ext);
    if (!ok)
        X509_EXTENSION_free(ext);
    ok = ok && X509_REQ_add_extensions(req, exts) && X509_REQ_sign(req, pkey, EVP_sha256());
    if (ok)
    {
        unsigned char *der = NULL;
        int len = i2d_X509_REQ(req, &der);
        if (len > 0)
            out = b64u(der, len);
        OPENSSL_free(der);
    }
    sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
    GENERAL_NAMES_free(gens);
    X509_REQ_free(req);
    EVP_PKEY_free(pkey);
    return out;
}

std::vector<S> split_pem(const S &pem)
{
    std::vector<S> out;
    static const S b = "-----BEGIN CERTIFICATE-----", e = "-----END CERTIFICATE-----";
    size_t pos = 0;
    for (;;)
    {
        size_t s = pem.find(b, pos);
        if (s == S::npos)
            break;
        size_t t = pem.find(e, s);
        if (t == S::npos)
            break;
        t += e.size();
        out.push_back(pem.substr(s, t - s) + "\n");
        pos = t;
    }
    return out;
}

bool cert_times(const S &pem, EC_KEY *k, int64_t &nb, int64_t &na)
{
    BIO *bio = BIO_new_mem_buf(pem.data(), (int)pem.size());
    X509 *x = bio ? PEM_read_bio_X509(bio, NULL, NULL, NULL) : NULL;
    BIO_free(bio);
    if (!x)
        return false;
    EVP_PKEY *pub = X509_get_pubkey(x);
    EC_KEY *ek = pub ? EVP_PKEY_get1_EC_KEY(pub) : NULL;
    bool ok = ek && EC_POINT_cmp(EC_KEY_get0_group(k), EC_KEY_get0_public_key(ek), EC_KEY_get0_public_key(k), NULL) == 0
              && ASN1_TIME_to_posix(X509_get0_notBefore(x), &nb) && ASN1_TIME_to_posix(X509_get0_notAfter(x), &na)
              && na > nb;
    EC_KEY_free(ek);
    EVP_PKEY_free(pub);
    X509_free(x);
    return ok;
}

bool setup(Acme &acme, const S &configDir, const S &server, bool create)
{
    S ca = configDir + "\\server-ca.pem";
    if (!acme.http.init(exists(ca) ? ca : S()))
        return false;
    EC_KEY *k = load_key(configDir + "\\account.key", create);
    if (!k)
    {
        say(1, "no ACME account key in %s, run openlitespeed --acme-install", configDir.c_str());
        return false;
    }
    return acme.use_key(k) && acme.directory(server);
}

S trim(const S &s)
{
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == S::npos ? S() : s.substr(a, b - a + 1);
}

S server_url(const S &name)
{
    if (name.empty() || name == "letsencrypt")
        return "https://acme-v02.api.letsencrypt.org/directory";
    if (name == "letsencrypt_test")
        return "https://acme-staging-v02.api.letsencrypt.org/directory";
    return name;
}

S root_dir()
{
    wchar_t root[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return S();
    S r = U(root);
    while (!r.empty() && (r.back() == '\\' || r.back() == '/'))
        r.pop_back();
    for (auto &c : r)
        if (c == '/')
            c = '\\';
    return r;
}

S admin_email(const S &conf)
{
    S text;
    if (!read_file(conf, text))
        return S();
    size_t p = text.find("adminEmails");
    if (p == S::npos)
        return S();
    size_t e = text.find_first_of("\r\n", p);
    S v = trim(text.substr(p + 11, e == S::npos ? S::npos : e - p - 11));
    v = v.substr(0, v.find_first_of(", \t"));
    return v == "root@localhost" || v.find('@') == S::npos ? S() : v;
}

bool set_module(const S &conf, const S &thumb)
{
    S text;
    if (!read_file(conf, text))
        return false;
    S block = "module mod_acme {\nacmeEnable 1\nacmeThumbPrint " + thumb + "\n}\n";
    size_t p = text.find("module mod_acme");
    if (p != S::npos && (p == 0 || text[p - 1] == '\n'))
    {
        size_t e = text.find('}', p);
        if (e == S::npos)
            return false;
        e = text.find_first_not_of("\r\n", e + 1);
        text.replace(p, (e == S::npos ? text.size() : e) - p, block);
    }
    else
    {
        if (!text.empty() && text.back() != '\n')
            text += "\n";
        text += "\n" + block;
    }
    return write_file(conf, text);
}

bool set_acme_on(const S &conf)
{
    S text;
    if (!read_file(conf, text))
        return false;
    size_t t = 0;
    while ((t = text.find("tuning", t)) != S::npos && t > 0 && text[t - 1] != '\n')
        t += 6;
    if (t == S::npos)
    {
        if (!text.empty() && text.back() != '\n')
            text += "\n";
        text += "\ntuning {\n    acme                         2\n}\n";
        return write_file(conf, text);
    }
    size_t open = text.find('{', t), close = open == S::npos ? S::npos : text.find('}', open);
    if (close == S::npos)
        return false;
    size_t p = open + 1;
    while (p < close)
    {
        size_t e = text.find('\n', p);
        if (e == S::npos || e > close)
            e = close;
        S line = trim(text.substr(p, e - p));
        if (line.size() > 4 && line.compare(0, 4, "acme") == 0 && (line[4] == ' ' || line[4] == '\t'))
        {
            text.replace(p, e - p, "    acme                         2");
            return write_file(conf, text);
        }
        p = e + 1;
    }
    size_t lineStart = text.rfind('\n', close);
    text.insert(lineStart == S::npos ? close : lineStart + 1, "    acme                         2\n");
    return write_file(conf, text);
}

}

extern "C" int ols_acme_order(const char *configDir, const char *serverUrl, const char *domain,
                              const char *const *names, int nnames, const char *alts, ols_acme_log_fn log)
{
    g_log = log;
    S cfg = configDir, dom = domain;
    std::vector<S> all;
    all.push_back(dom);
    for (int i = 0; i < nnames; ++i)
        if (_stricmp(names[i], domain) != 0)
            all.push_back(names[i]);
    Acme acme;
    if (!setup(acme, cfg, server_url(serverUrl), false) || !acme.account(S(), true))
        return 1;
    S ids;
    for (auto &n : all)
        ids += (ids.empty() ? "" : ",") + S("{\"type\":\"dns\",\"value\":") + jstr(n) + "}";
    Resp r;
    J order;
    if (!acme.post(acme.dir.str("newOrder"), "{\"identifiers\":[" + ids + "]}", r))
        return 1;
    if (r.status != 201 || r.location.empty() || !parse_json(r.body, order) || order.t != J::OBJ)
    {
        say(1, "new order for %s failed: %s", domain, Acme::problem(r).c_str());
        return 1;
    }
    S orderUrl = r.location, finalize = order.str("finalize");
    if (const J *az = order.get("authorizations"))
        for (auto &u : az->a)
            if (u.t == J::STR && !acme.authorize(u.s))
                return 1;
    EC_KEY *ck = new_key();
    if (!ck)
        return 1;
    S csr = make_csr(ck, all), keyPem = key_pem(ck);
    if (csr.empty() || keyPem.empty() || finalize.empty())
    {
        say(1, "cannot create the certificate request for %s", domain);
        EC_KEY_free(ck);
        return 1;
    }
    if (!acme.post(finalize, "{\"csr\":\"" + csr + "\"}", r) || (r.status != 200 && r.status != 201))
    {
        say(1, "finalizing the order for %s failed: %s", domain, Acme::problem(r).c_str());
        EC_KEY_free(ck);
        return 1;
    }
    S certUrl;
    for (int i = 0; i < 60 && certUrl.empty(); ++i)
    {
        if (!acme.fetch(orderUrl, order))
            break;
        S st = order.str("status");
        if (st == "valid")
            certUrl = order.str("certificate");
        else if (st == "invalid")
        {
            say(1, "the order for %s became invalid", domain);
            break;
        }
        else
            Sleep(2000);
    }
    if (certUrl.empty())
    {
        EC_KEY_free(ck);
        return 1;
    }
    if (!acme.post(certUrl, S(), r, false, "application/pem-certificate-chain") || r.status != 200)
    {
        say(1, "downloading the certificate for %s failed: %s", domain, Acme::problem(r).c_str());
        EC_KEY_free(ck);
        return 1;
    }
    std::vector<S> certs = split_pem(r.body);
    int64_t nb = 0, na = 0;
    if (certs.empty() || !cert_times(certs[0], ck, nb, na))
    {
        say(1, "the ACME server returned an unusable certificate for %s", domain);
        EC_KEY_free(ck);
        return 1;
    }
    EC_KEY_free(ck);
    S ca, full;
    for (size_t i = 0; i < certs.size(); ++i)
    {
        full += certs[i];
        if (i)
            ca += certs[i];
    }
    time_t now = time(NULL), renew = (time_t)(nb + (na - nb) * 2 / 3);
    S dir = cfg + "\\certs\\" + dom + "_ecc", base = dir + "\\" + dom;
    S conf = "Le_Domain='" + dom + "'\nLe_Alt='" + S(alts && *alts ? alts : "no") + "'\nLe_Webroot='stateless'\n"
             + "Le_Keylength='ec-256'\nLe_OrderFinalize='" + finalize + "'\nLe_LinkOrder='" + orderUrl
             + "'\nLe_LinkCert='" + certUrl + "'\nLe_CertCreateTime='" + std::to_string((long long)now)
             + "'\nLe_CertCreateTimeStr='" + iso_time(now) + "'\nLe_NextRenewTimeStr='" + iso_time(renew)
             + "'\nLe_NextRenewTime='" + std::to_string((long long)renew) + "'\n";
    if (!make_dirs(dir) || !write_file(base + ".key", keyPem) || !write_file(base + ".cer", certs[0])
        || !write_file(dir + "\\ca.cer", ca) || !write_file(dir + "\\fullchain.cer", full)
        || !write_file(base + ".conf", conf))
    {
        say(1, "cannot write the certificate files in %s", dir.c_str());
        return 1;
    }
    say(0, "certificate for %s issued, valid until %s, renewal after %s", domain, iso_time((time_t)na).c_str(),
        iso_time(renew).c_str());
    return 0;
}

extern "C" int ols_acme_main(int argc, char **argv)
{
    S email, server;
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "--email") == 0 && i + 1 < argc)
            email = argv[++i];
        else if (strcmp(argv[i], "--server") == 0 && i + 1 < argc)
            server = argv[++i];
        else
        {
            fprintf(stderr, "usage: openlitespeed --acme-install [--email <address>] [--server <directory URL>]\n");
            return 2;
        }
    }
    S root = root_dir();
    if (root.empty())
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    S cfg = root + "\\conf\\cert\\acme", conf = root + "\\conf\\httpd_config.conf";
    S known;
    if (server.empty() && read_file(cfg + "\\server.conf", known))
        server = trim(known);
    server = server_url(server);
    if (email.empty())
        email = admin_email(conf);
    if (!make_dirs(root + "\\acme") || !make_dirs(cfg + "\\data") || !make_dirs(cfg + "\\certs")
        || !write_file(cfg + "\\server.conf", server + "\n"))
    {
        fprintf(stderr, "cannot create %s\n", cfg.c_str());
        return 1;
    }
    Acme acme;
    if (!setup(acme, cfg, server, true) || !acme.account(email, true))
        return 1;
    write_file(cfg + "\\data\\account.url", acme.kid + "\n");
    if (!set_module(conf, acme.thumb) || !set_acme_on(conf))
    {
        fprintf(stderr, "cannot update %s\n", conf.c_str());
        return 1;
    }
    printf("ACME account %s\nserver %s\n%s\nACME is on. After a reload, sites on HTTPS listeners get a certificate "
           "for their domains unless they have their own certificate files.\n",
           acme.kid.c_str(), server.c_str(), email.empty() ? "no contact email" : ("contact " + email).c_str());
    return 0;
}
