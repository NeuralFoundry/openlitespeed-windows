#include "compat_internal.h"
#undef NOCRYPT
#include <wincrypt.h>

static HCERTSTORE user_root(void)
{
    return CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"Root");
}

static int cancelled(void)
{
    DWORD e = GetLastError();
    return e == ERROR_CANCELLED || e == (DWORD)SCARD_W_CANCELLED_BY_USER || e == HRESULT_FROM_WIN32(ERROR_CANCELLED);
}

int ols_store_has_root(const unsigned char *der, size_t len)
{
    HCERTSTORE st;
    PCCERT_CONTEXT want, found = NULL;
    want = CertCreateCertificateContext(X509_ASN_ENCODING, der, (DWORD)len);
    if (!want)
        return 0;
    if ((st = user_root()) != NULL)
    {
        found = CertFindCertificateInStore(st, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, want, NULL);
        if (found)
            CertFreeCertificateContext(found);
        CertCloseStore(st, 0);
    }
    CertFreeCertificateContext(want);
    return found != NULL;
}

int ols_store_add_root(const unsigned char *der, size_t len, const wchar_t *friendly)
{
    HCERTSTORE st;
    PCCERT_CONTEXT ctx;
    CRYPT_DATA_BLOB name, usage;
    LPSTR oids[1] = { szOID_PKIX_KP_SERVER_AUTH };
    CERT_ENHKEY_USAGE eku = { 1, oids };
    BYTE buf[128];
    DWORD bl = sizeof(buf);
    int rc = 1;
    ctx = CertCreateCertificateContext(X509_ASN_ENCODING, der, (DWORD)len);
    if (!ctx)
        return 1;
    name.pbData = (BYTE *)friendly;
    name.cbData = (DWORD)((wcslen(friendly) + 1) * sizeof(wchar_t));
    CertSetCertificateContextProperty(ctx, CERT_FRIENDLY_NAME_PROP_ID, 0, &name);
    if (CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eku, buf, &bl))
    {
        usage.pbData = buf;
        usage.cbData = bl;
        CertSetCertificateContextProperty(ctx, CERT_ENHKEY_USAGE_PROP_ID, 0, &usage);
    }
    if ((st = user_root()) != NULL)
    {
        if (CertAddCertificateContextToStore(st, ctx, CERT_STORE_ADD_REPLACE_EXISTING, NULL))
            rc = 0;
        else if (cancelled())
            rc = 3;
        CertCloseStore(st, 0);
    }
    CertFreeCertificateContext(ctx);
    return rc;
}

int ols_store_remove_root(const unsigned char *der, size_t len)
{
    HCERTSTORE st;
    PCCERT_CONTEXT want, found;
    int rc = 0;
    want = CertCreateCertificateContext(X509_ASN_ENCODING, der, (DWORD)len);
    if (!want)
        return 1;
    if ((st = user_root()) == NULL)
    {
        CertFreeCertificateContext(want);
        return 1;
    }
    found = CertFindCertificateInStore(st, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, want, NULL);
    if (found && !CertDeleteCertificateFromStore(found))
        rc = cancelled() ? 3 : 1;
    CertCloseStore(st, 0);
    CertFreeCertificateContext(want);
    return rc;
}
