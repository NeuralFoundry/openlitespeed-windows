#include "compat_internal.h"
#undef NOCRYPT
#include <wincrypt.h>

static char g_ca_file[PATH_MAX];
static INIT_ONCE g_ca_once = INIT_ONCE_STATIC_INIT;

static HCERTSTORE open_store(const wchar_t *name)
{
    return CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG
                         | CERT_STORE_OPEN_EXISTING_FLAG, name);
}

static int trusted_for_tls(PCCERT_CONTEXT c, HCERTSTORE disallowed)
{
    BYTE buf[2048];
    CERT_ENHKEY_USAGE *u = (CERT_ENHKEY_USAGE *)buf;
    DWORD n = sizeof(buf), i;
    PCCERT_CONTEXT bad;
    if (disallowed)
    {
        bad = CertFindCertificateInStore(disallowed, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, c, NULL);
        if (bad)
        {
            CertFreeCertificateContext(bad);
            return 0;
        }
    }
    if (!CertGetEnhancedKeyUsage(c, CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, u, &n))
        return GetLastError() == CRYPT_E_NOT_FOUND;
    for (i = 0; i < u->cUsageIdentifier; ++i)
        if (strcmp(u->rgpszUsageIdentifier[i], szOID_PKIX_KP_SERVER_AUTH) == 0)
            return 1;
    return 0;
}

static int dump_store(HANDLE f, const wchar_t *name)
{
    HCERTSTORE st = open_store(name), disallowed = open_store(L"Disallowed");
    PCCERT_CONTEXT c = NULL;
    int n = 0;
    if (!st)
    {
        if (disallowed)
            CertCloseStore(disallowed, 0);
        return 0;
    }
    while ((c = CertEnumCertificatesInStore(st, c)) != NULL)
    {
        DWORD len = 0, w;
        char *pem;
        if (!trusted_for_tls(c, disallowed))
            continue;
        if (!CryptBinaryToStringA(c->pbCertEncoded, c->cbCertEncoded,
                                  CRYPT_STRING_BASE64HEADER, NULL, &len))
            continue;
        pem = (char *)malloc(len);
        if (!pem)
            continue;
        if (CryptBinaryToStringA(c->pbCertEncoded, c->cbCertEncoded,
                                 CRYPT_STRING_BASE64HEADER, pem, &len))
        {
            WriteFile(f, pem, len, &w, NULL);
            ++n;
        }
        free(pem);
    }
    CertCloseStore(st, 0);
    if (disallowed)
        CertCloseStore(disallowed, 0);
    return n;
}

static BOOL CALLBACK ca_init(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    wchar_t w[PATH_MAX + 8], tmp[PATH_MAX + 16];
    HANDLE f;
    int n;
    if (ols_path_to_w("/tmp/ca-bundle.pem", w, PATH_MAX + 8, NULL) < 0)
        return TRUE;
    swprintf(tmp, PATH_MAX + 16, L"%s.%lu", w, GetCurrentProcessId());
    DeleteFileW(tmp);
    f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return TRUE;
    n = dump_store(f, L"ROOT");
    CloseHandle(f);
    if (n == 0 || !MoveFileExW(tmp, w, MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(tmp);
        return TRUE;
    }
    WideCharToMultiByte(CP_UTF8, 0, w, -1, g_ca_file, sizeof(g_ca_file), NULL, NULL);
    {
        char *p;
        for (p = g_ca_file; *p; ++p)
            if (*p == '\\')
                *p = '/';
    }
    return TRUE;
}

const char *ols_system_ca_file(void)
{
    InitOnceExecuteOnce(&g_ca_once, ca_init, NULL, NULL);
    return g_ca_file[0] ? g_ca_file : NULL;
}
