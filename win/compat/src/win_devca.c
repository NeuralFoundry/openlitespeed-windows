#include "compat_internal.h"
#include <iphlpapi.h>
#include <openssl/bn.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#define CA_DAYS 3650L
#define LEAF_DAYS 825L
#define MAX_LEAF_SITES 500
#define CA_FRIENDLY L"OpenLiteSpeed Development CA"
#define CA_CONSTRAINTS                                                                                   \
    "critical,permitted;DNS:test,permitted;DNS:localhost,permitted;DNS:internal,"                       \
    "permitted;IP:127.0.0.0/255.0.0.0,permitted;IP:::1/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"

static const char *const s_tlds[] = { "test", "localhost", "internal", NULL };

typedef struct
{
    EVP_PKEY *key;
    X509 *cert;
} devca_t;

static int tld_ok(const char *t)
{
    int i;
    for (i = 0; s_tlds[i]; ++i)
        if (strcmp(t, s_tlds[i]) == 0)
            return 1;
    return 0;
}

static int label_ok(const char *s)
{
    size_t n = strlen(s), i;
    if (n == 0 || n > 63 || s[0] == '-' || s[n - 1] == '-')
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-'))
            return 0;
    return 1;
}

static int get_root(wchar_t *buf, DWORD len)
{
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", buf, len);
    if (n == 0 || n >= len)
        return -1;
    while (n > 3 && (buf[n - 1] == L'\\' || buf[n - 1] == L'/'))
        buf[--n] = 0;
    return 0;
}

static void sb_add(char **b, size_t *len, size_t *cap, const char *s)
{
    size_t n = strlen(s);
    if (!*b && *cap)
        return;
    if (*len + n + 1 > *cap)
    {
        size_t nc = (*cap ? *cap * 2 : 1024) + n;
        char *p = (char *)realloc(*b, nc);
        if (!p)
        {
            free(*b);
            *b = NULL;
            *cap = 1;
            return;
        }
        *b = p;
        *cap = nc;
    }
    memcpy(*b + *len, s, n + 1);
    *len += n;
}

int ols_read_wfile(const wchar_t *path, char **out, size_t *len)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, got = 0;
    *out = NULL;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > (1u << 20) || (*out = (char *)malloc(size + 1)) == NULL
        || !ReadFile(h, *out, size, &got, NULL) || got != size)
    {
        CloseHandle(h);
        free(*out);
        *out = NULL;
        return -1;
    }
    CloseHandle(h);
    (*out)[size] = 0;
    *len = size;
    return 0;
}

int ols_write_wfile(const wchar_t *path, const void *data, size_t len, int priv)
{
    SECURITY_ATTRIBUTES sa;
    PSECURITY_DESCRIPTOR sd = NULL;
    HANDLE h;
    DWORD w = 0, e;
    int rc = -1;
    if (priv)
    {
        if ((sd = ols_private_sd(0)) == NULL)
            return -1;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = sd;
        sa.bInheritHandle = FALSE;
    }
    h = CreateFileW(path, GENERIC_WRITE, 0, priv ? &sa : NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    e = GetLastError();
    if (sd)
        LocalFree(sd);
    if (h == INVALID_HANDLE_VALUE)
    {
        SetLastError(e);
        return -1;
    }
    if (WriteFile(h, data, (DWORD)len, &w, NULL) && w == len)
        rc = 0;
    CloseHandle(h);
    if (rc == 0 && priv && ols_secure_wpath(path, 0) != 0)
        rc = -1;
    if (rc != 0)
        DeleteFileW(path);
    return rc;
}

int ols_private_dir(const wchar_t *dir)
{
    SECURITY_ATTRIBUTES sa;
    PSECURITY_DESCRIPTOR sd = ols_private_sd(1);
    DWORD attr;
    HANDLE h;
    int ok;
    if (!sd)
        return -1;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    ok = CreateDirectoryW(dir, &sa) || GetLastError() == ERROR_ALREADY_EXISTS;
    LocalFree(sd);
    attr = GetFileAttributesW(dir);
    if (!ok || attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)
        || (attr & FILE_ATTRIBUTE_REPARSE_POINT))
        return -1;
    if (ols_secure_wpath(dir, 1) != 0)
        return -1;
    h = CreateFileW(dir, READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    ok = ols_is_private_handle(h);
    CloseHandle(h);
    return ok ? 0 : -1;
}

static void ca_paths(const wchar_t *root, wchar_t *dir, wchar_t *crt, wchar_t *key)
{
    swprintf(dir, MAX_PATH, L"%s\\conf\\cert\\devca", root);
    swprintf(crt, MAX_PATH, L"%s\\ca.crt", dir);
    swprintf(key, MAX_PATH, L"%s\\ca.key", dir);
}

static void leaf_paths(const wchar_t *root, wchar_t *crt, wchar_t *key)
{
    swprintf(crt, MAX_PATH, L"%s\\conf\\cert\\devsites.crt", root);
    swprintf(key, MAX_PATH, L"%s\\conf\\cert\\devsites.key", root);
}

static EVP_PKEY *new_key(void)
{
    EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    EVP_PKEY *p = NULL;
    if (ec && EC_KEY_generate_key(ec) && (p = EVP_PKEY_new()) != NULL && EVP_PKEY_assign_EC_KEY(p, ec))
        return p;
    EVP_PKEY_free(p);
    EC_KEY_free(ec);
    return NULL;
}

static int set_serial(X509 *x)
{
    BIGNUM *bn = BN_new();
    int ok = bn && BN_rand(bn, 120, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY)
             && BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(x)) != NULL;
    BN_free(bn);
    return ok;
}

static int add_ext(X509 *x, X509V3_CTX *ctx, int nid, const char *value)
{
    X509_EXTENSION *ext = X509V3_EXT_nconf_nid(NULL, ctx, nid, value);
    int ok;
    if (!ext)
        return -1;
    ok = X509_add_ext(x, ext, -1);
    X509_EXTENSION_free(ext);
    return ok ? 0 : -1;
}

static int add_name(X509_NAME *n, const char *field, const char *value)
{
    return X509_NAME_add_entry_by_txt(n, field, MBSTRING_UTF8, (const uint8_t *)value, -1, -1, 0) ? 0 : -1;
}

static void ascii_tag(const wchar_t *in, char *out, size_t n)
{
    size_t i = 0;
    for (; *in && i + 1 < n; ++in)
        out[i++] = ((*in >= L'a' && *in <= L'z') || (*in >= L'A' && *in <= L'Z') || (*in >= L'0' && *in <= L'9')
                    || *in == L'-' || *in == L'.')
                       ? (char)*in
                       : '_';
    out[i] = 0;
}

static void ca_free(devca_t *ca)
{
    X509_free(ca->cert);
    EVP_PKEY_free(ca->key);
    ca->cert = NULL;
    ca->key = NULL;
}

static int ca_create(devca_t *ca)
{
    X509 *x = X509_new();
    EVP_PKEY *k = new_key();
    X509_NAME *n;
    X509V3_CTX v3;
    wchar_t wu[128], wc[64];
    DWORD ul = 128, cl = 64;
    char user[48] = "user", comp[48] = "pc", ou[100];
    if (GetUserNameW(wu, &ul))
        ascii_tag(wu, user, sizeof(user));
    if (GetComputerNameW(wc, &cl))
        ascii_tag(wc, comp, sizeof(comp));
    snprintf(ou, sizeof(ou), "%s@%s", user, comp);
    if (!x || !k || !X509_set_version(x, 2) || !set_serial(x)
        || !X509_gmtime_adj(X509_getm_notBefore(x), -86400) || !X509_gmtime_adj(X509_getm_notAfter(x), CA_DAYS * 86400L))
        goto fail;
    n = X509_get_subject_name(x);
    if (add_name(n, "O", "OpenLiteSpeed development") || add_name(n, "OU", ou)
        || add_name(n, "CN", "OpenLiteSpeed Development CA") || !X509_set_issuer_name(x, n) || !X509_set_pubkey(x, k))
        goto fail;
    X509V3_set_ctx(&v3, x, x, NULL, NULL, 0);
    if (add_ext(x, &v3, NID_basic_constraints, "critical,CA:TRUE,pathlen:0")
        || add_ext(x, &v3, NID_key_usage, "critical,keyCertSign,cRLSign")
        || add_ext(x, &v3, NID_ext_key_usage, "serverAuth")
        || add_ext(x, &v3, NID_subject_key_identifier, "hash")
        || add_ext(x, &v3, NID_name_constraints, CA_CONSTRAINTS))
        goto fail;
    if (!X509_sign(x, k, EVP_sha256()))
        goto fail;
    ca->cert = x;
    ca->key = k;
    return 0;
fail:
    X509_free(x);
    EVP_PKEY_free(k);
    return -1;
}

static int pem_cert(X509 *x, char **out, size_t *len)
{
    BIO *b = BIO_new(BIO_s_mem());
    const uint8_t *d;
    size_t n;
    int rc = -1;
    *out = NULL;
    if (b && PEM_write_bio_X509(b, x) && BIO_mem_contents(b, &d, &n) && (*out = (char *)malloc(n)) != NULL)
    {
        memcpy(*out, d, n);
        *len = n;
        rc = 0;
    }
    BIO_free(b);
    return rc;
}

static int pem_key(EVP_PKEY *k, char **out, size_t *len)
{
    BIO *b = BIO_new(BIO_s_mem());
    const uint8_t *d;
    size_t n;
    int rc = -1;
    *out = NULL;
    if (b && PEM_write_bio_PrivateKey(b, k, NULL, NULL, 0, NULL, NULL) && BIO_mem_contents(b, &d, &n)
        && (*out = (char *)malloc(n)) != NULL)
    {
        memcpy(*out, d, n);
        *len = n;
        rc = 0;
    }
    BIO_free(b);
    return rc;
}

static int save_pair(const wchar_t *crt, const wchar_t *key, X509 *x, EVP_PKEY *k)
{
    char *c = NULL, *p = NULL;
    size_t cl = 0, pl = 0;
    int rc = -1;
    if (pem_cert(x, &c, &cl) == 0 && pem_key(k, &p, &pl) == 0 && ols_write_wfile(key, p, pl, 1) == 0
        && ols_write_wfile(crt, c, cl, 0) == 0)
        rc = 0;
    if (p)
    {
        OPENSSL_cleanse(p, pl);
        free(p);
    }
    free(c);
    return rc;
}

static int ca_load(const wchar_t *root, devca_t *ca)
{
    wchar_t dir[MAX_PATH], crt[MAX_PATH], key[MAX_PATH];
    char *a = NULL, *b = NULL;
    size_t al = 0, bl = 0;
    BIO *bio;
    int rc = -1;
    ca->key = NULL;
    ca->cert = NULL;
    ca_paths(root, dir, crt, key);
    if (ols_read_wfile(crt, &a, &al) != 0 || ols_read_wfile(key, &b, &bl) != 0)
        goto done;
    if ((bio = BIO_new_mem_buf(a, (ossl_ssize_t)al)) != NULL)
    {
        ca->cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        BIO_free(bio);
    }
    if ((bio = BIO_new_mem_buf(b, (ossl_ssize_t)bl)) != NULL)
    {
        ca->key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        BIO_free(bio);
    }
    if (ca->cert && ca->key && X509_check_private_key(ca->cert, ca->key)
        && X509_cmp_current_time(X509_get0_notAfter(ca->cert)) > 0)
        rc = 0;
done:
    if (b)
    {
        OPENSSL_cleanse(b, bl);
        free(b);
    }
    free(a);
    if (rc != 0)
        ca_free(ca);
    return rc;
}

static int ca_der(X509 *x, unsigned char **der, size_t *len)
{
    uint8_t *p = NULL;
    int n = i2d_X509(x, &p);
    if (n <= 0)
        return -1;
    *der = p;
    *len = (size_t)n;
    return 0;
}

static int leaf_issue(devca_t *ca, const char *san, EVP_PKEY **ko, X509 **xo)
{
    X509 *x = X509_new();
    EVP_PKEY *k = new_key();
    X509_NAME *n;
    X509V3_CTX v3;
    if (!x || !k || !X509_set_version(x, 2) || !set_serial(x)
        || !X509_gmtime_adj(X509_getm_notBefore(x), -3600) || !X509_gmtime_adj(X509_getm_notAfter(x), LEAF_DAYS * 86400L))
        goto fail;
    n = X509_get_subject_name(x);
    if (add_name(n, "O", "OpenLiteSpeed development") || add_name(n, "CN", "localhost")
        || !X509_set_issuer_name(x, X509_get_subject_name(ca->cert)) || !X509_set_pubkey(x, k))
        goto fail;
    X509V3_set_ctx(&v3, ca->cert, x, NULL, NULL, 0);
    if (add_ext(x, &v3, NID_basic_constraints, "critical,CA:FALSE")
        || add_ext(x, &v3, NID_key_usage, "critical,digitalSignature")
        || add_ext(x, &v3, NID_ext_key_usage, "serverAuth")
        || add_ext(x, &v3, NID_subject_key_identifier, "hash")
        || add_ext(x, &v3, NID_authority_key_identifier, "keyid:always")
        || add_ext(x, &v3, NID_subject_alt_name, san))
        goto fail;
    if (!X509_sign(x, ca->key, EVP_sha256()))
        goto fail;
    *ko = k;
    *xo = x;
    return 0;
fail:
    X509_free(x);
    EVP_PKEY_free(k);
    return -1;
}

int ols_devca_issue(const wchar_t *root, const char *tld, char **names, int count)
{
    devca_t ca;
    wchar_t crt[MAX_PATH], key[MAX_PATH], dir[MAX_PATH];
    char *san = NULL, item[300];
    size_t sl = 0, sc = 0;
    EVP_PKEY *k = NULL;
    X509 *x = NULL;
    int i, rc = 1;
    if (!tld || !tld_ok(tld))
        return 3;
    if (ca_load(root, &ca) != 0)
        return 2;
    sb_add(&san, &sl, &sc, "DNS:localhost,IP:127.0.0.1,IP:::1");
    for (i = 0; i < count && i < MAX_LEAF_SITES; ++i)
    {
        if (!names[i] || !label_ok(names[i]))
            continue;
        snprintf(item, sizeof(item), ",DNS:%s.%s,DNS:www.%s.%s", names[i], tld, names[i], tld);
        sb_add(&san, &sl, &sc, item);
    }
    if (count > MAX_LEAF_SITES)
        printf("only the first %d sites get HTTPS\n", MAX_LEAF_SITES);
    swprintf(dir, MAX_PATH, L"%s\\conf\\cert", root);
    CreateDirectoryW(dir, NULL);
    leaf_paths(root, crt, key);
    if (san && leaf_issue(&ca, san, &k, &x) == 0 && save_pair(crt, key, x, k) == 0)
        rc = 0;
    X509_free(x);
    EVP_PKEY_free(k);
    free(san);
    ca_free(&ca);
    return rc;
}

int ols_port_listening(int port)
{
    DWORD size = 0, i;
    int found = 0;
    MIB_TCPTABLE_OWNER_PID *t4 = NULL;
    MIB_TCP6TABLE_OWNER_PID *t6 = NULL;
    if (GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == ERROR_INSUFFICIENT_BUFFER
        && (t4 = (MIB_TCPTABLE_OWNER_PID *)malloc(size)) != NULL
        && GetExtendedTcpTable(t4, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR)
        for (i = 0; i < t4->dwNumEntries && !found; ++i)
            found = ntohs((u_short)t4->table[i].dwLocalPort) == port;
    free(t4);
    size = 0;
    if (!found
        && GetExtendedTcpTable(NULL, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_LISTENER, 0) == ERROR_INSUFFICIENT_BUFFER
        && (t6 = (MIB_TCP6TABLE_OWNER_PID *)malloc(size)) != NULL
        && GetExtendedTcpTable(t6, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR)
        for (i = 0; i < t6->dwNumEntries && !found; ++i)
            found = ntohs((u_short)t6->table[i].dwLocalPort) == port;
    free(t6);
    return found;
}

static int http_port(const wchar_t *root)
{
    wchar_t conf[MAX_PATH];
    char *t = NULL, *p, *q, *c = NULL;
    size_t len;
    int port = 80;
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    if (ols_read_wfile(conf, &t, &len) != 0)
        return 80;
    if ((p = strstr(t, "listener Default")) != NULL && (p = strstr(p, "address")) != NULL)
    {
        for (q = p; *q && *q != '\r' && *q != '\n'; ++q)
            if (*q == ':')
                c = q;
        if (c)
            port = atoi(c + 1);
    }
    free(t);
    return port > 0 && port < 65536 ? port : 80;
}

int ols_dev_resync(const wchar_t *root, const wchar_t *ini)
{
    wchar_t www[MAX_PATH], def[MAX_PATH], tld[32];
    char wmb[MAX_PATH * 3], tmb[32];
    swprintf(def, MAX_PATH, L"%s\\www", root);
    GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
    GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, ini);
    CreateDirectoryW(www, NULL);
    if (WideCharToMultiByte(CP_UTF8, 0, www, -1, wmb, sizeof(wmb), NULL, NULL) <= 0
        || WideCharToMultiByte(CP_UTF8, 0, tld, -1, tmb, sizeof(tmb), NULL, NULL) <= 0)
        return 1;
    return ols_dev_sites(wmb, tmb, GetPrivateProfileIntW(L"sites", L"root", 0, ini));
}

static int https_on(const wchar_t *root, const wchar_t *ini, const char *portarg)
{
    wchar_t dir[MAX_PATH], crt[MAX_PATH], key[MAX_PATH], wtld[32], val[16], cdir[MAX_PATH];
    char tld[32];
    devca_t ca;
    unsigned char *der = NULL;
    size_t dl = 0;
    int http = http_port(root), port, cur, r;
    GetPrivateProfileStringW(L"sites", L"tld", L"test", wtld, 32, ini);
    if (WideCharToMultiByte(CP_UTF8, 0, wtld, -1, tld, sizeof(tld), NULL, NULL) <= 0 || !tld_ok(tld))
    {
        fprintf(stderr, "trusted HTTPS works with the .test, .localhost and .internal suffixes only\n");
        return 2;
    }
    cur = GetPrivateProfileIntW(L"sites", L"https", 0, ini);
    if (portarg)
    {
        port = atoi(portarg);
        if (port <= 0 || port > 65535 || port == http)
        {
            fprintf(stderr, "invalid HTTPS port\n");
            return 2;
        }
        if (cur != port && ols_port_listening(port))
        {
            fprintf(stderr, "port %d is already in use\n", port);
            return 2;
        }
    }
    else
    {
        static const int tries[] = { 443, 8443, 9443, 0 };
        int i;
        port = 0;
        for (i = 0; tries[i] && !port; ++i)
            if (tries[i] != http && (tries[i] != 443 || http == 80) && (tries[i] == cur || !ols_port_listening(tries[i])))
                port = tries[i];
        if (!port)
        {
            fprintf(stderr, "ports 443, 8443 and 9443 are in use; pass a free port: --dev-https on <port>\n");
            return 2;
        }
    }
    swprintf(cdir, MAX_PATH, L"%s\\conf\\cert", root);
    CreateDirectoryW(cdir, NULL);
    ca_paths(root, dir, crt, key);
    if (ols_private_dir(dir) != 0)
    {
        DWORD e = GetLastError();
        fprintf(stderr, "cannot prepare a private folder for the local CA: %ls\n", dir);
        return e == ERROR_ACCESS_DENIED ? 5 : 1;
    }
    if (ca_load(root, &ca) != 0)
    {
        if (ca_create(&ca) != 0)
        {
            fprintf(stderr, "cannot create the local CA\n");
            return 1;
        }
        if (save_pair(crt, key, ca.cert, ca.key) != 0)
        {
            DWORD e = GetLastError();
            ca_free(&ca);
            fprintf(stderr, "cannot write the local CA\n");
            return e == ERROR_ACCESS_DENIED ? 5 : 1;
        }
    }
    if (ca_der(ca.cert, &der, &dl) != 0)
    {
        ca_free(&ca);
        return 1;
    }
    ca_free(&ca);
    r = ols_store_has_root(der, dl) ? 0 : ols_store_add_root(der, dl, CA_FRIENDLY);
    OPENSSL_free(der);
    if (r == 3)
    {
        fprintf(stderr, "the local CA was not trusted, so trusted HTTPS stays off\n");
        return 3;
    }
    if (r != 0)
    {
        fprintf(stderr, "cannot add the local CA to the Windows certificate store\n");
        return 1;
    }
    swprintf(val, 16, L"%d", port);
    if (!WritePrivateProfileStringW(L"sites", L"https", val, ini))
        return GetLastError() == ERROR_ACCESS_DENIED ? 5 : 1;
    if (ols_dev_resync(root, ini) != 0)
        return 1;
    printf("trusted HTTPS is on, port %d\n", port);
    return 0;
}

static int https_off(const wchar_t *root, const wchar_t *ini, int sites, int store)
{
    wchar_t dir[MAX_PATH], crt[MAX_PATH], key[MAX_PATH], lc[MAX_PATH], lk[MAX_PATH];
    char *a = NULL;
    size_t al = 0;
    int removed = 1;
    WritePrivateProfileStringW(L"sites", L"https", NULL, ini);
    if (sites)
        ols_dev_resync(root, ini);
    leaf_paths(root, lc, lk);
    DeleteFileW(lk);
    DeleteFileW(lc);
    ca_paths(root, dir, crt, key);
    DeleteFileW(key);
    if (store && ols_read_wfile(crt, &a, &al) == 0)
    {
        BIO *bio = BIO_new_mem_buf(a, (ossl_ssize_t)al);
        X509 *x = bio ? PEM_read_bio_X509(bio, NULL, NULL, NULL) : NULL;
        unsigned char *der = NULL;
        size_t dl = 0;
        BIO_free(bio);
        if (x && ca_der(x, &der, &dl) == 0)
        {
            removed = ols_store_remove_root(der, dl) == 0;
            OPENSSL_free(der);
        }
        X509_free(x);
        free(a);
    }
    if (!store)
        removed = 0;
    if (removed)
    {
        DeleteFileW(crt);
        RemoveDirectoryW(dir);
        printf("trusted HTTPS is off\n");
    }
    else
        printf("trusted HTTPS is off; the local CA is still listed in Windows, but its key was deleted\n");
    return 0;
}

static int https_status(const wchar_t *root, const wchar_t *ini)
{
    devca_t ca;
    unsigned char *der = NULL;
    size_t dl = 0;
    int port = GetPrivateProfileIntW(L"sites", L"https", 0, ini), trusted = 0, have = ca_load(root, &ca) == 0;
    if (have)
    {
        if (ca_der(ca.cert, &der, &dl) == 0)
        {
            trusted = ols_store_has_root(der, dl);
            OPENSSL_free(der);
        }
        ca_free(&ca);
    }
    if (port > 0)
        printf("on, port %d\n", port);
    else
        printf("off\n");
    printf("local CA: %s\n", have ? (trusted ? "trusted" : "present, not trusted") : "none");
    return 0;
}

int ols_dev_https(int argc, char **argv)
{
    wchar_t root[MAX_PATH], ini[MAX_PATH];
    const char *act = argc >= 1 ? argv[0] : "status";
    int i, sites = 1, store = 1;
    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--no-sites") == 0)
            sites = 0;
        else if (strcmp(argv[i], "--keep-store") == 0)
            store = 0;
    }
    if (get_root(root, MAX_PATH) < 0)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", root);
    if (strcmp(act, "on") == 0)
        return https_on(root, ini, argc >= 2 && argv[1][0] != '-' ? argv[1] : NULL);
    if (strcmp(act, "off") == 0)
        return https_off(root, ini, sites, store);
    if (strcmp(act, "status") == 0)
        return https_status(root, ini);
    fprintf(stderr, "usage: openlitespeed --dev-https on [port] | off | status\n");
    return 2;
}
