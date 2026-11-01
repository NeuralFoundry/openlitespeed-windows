#include "compat_internal.h"
#include <openssl/evp.h>
#include <openssl/ec_key.h>
#include <openssl/obj.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>

char *_crypt_blowfish_rn(const char *key, const char *setting, char *output, int size);
char *_crypt_gensalt_blowfish_rn(const char *prefix, unsigned long count,
                                 const char *input, int size, char *output, int output_size);

static int write_text(const char *path, const char *data, size_t len, int mode)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
        return -1;
    if (write(fd, data, len) != (ssize_t)len)
    {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int write_bio(const char *path, BIO *bio, int mode)
{
    const uint8_t *data;
    size_t len;
    if (!BIO_mem_contents(bio, &data, &len))
        return -1;
    return write_text(path, (const char *)data, len, mode);
}

int ols_gen_cert(const char *keyPath, const char *certPath, const char *cn)
{
    EVP_PKEY *pkey = NULL;
    EC_KEY *ec = NULL;
    X509 *x = NULL;
    X509_NAME *name;
    BIO *kb = NULL, *cb = NULL;
    BIGNUM *serial = NULL;
    X509V3_CTX v3;
    X509_EXTENSION *ext;
    char san[300];
    int ret = 1;

    ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec || !EC_KEY_generate_key(ec))
        goto done;
    pkey = EVP_PKEY_new();
    if (!pkey || !EVP_PKEY_assign_EC_KEY(pkey, ec))
        goto done;
    ec = NULL;
    x = X509_new();
    if (!x)
        goto done;
    X509_set_version(x, 2);
    serial = BN_new();
    if (!serial || !BN_rand(serial, 120, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY)
        || !BN_to_ASN1_INTEGER(serial, X509_get_serialNumber(x)))
        goto done;
    X509_gmtime_adj(X509_getm_notBefore(x), -3600);
    X509_gmtime_adj(X509_getm_notAfter(x), 3650L * 24 * 3600);
    X509_set_pubkey(x, pkey);
    name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (const uint8_t *)"OpenLiteSpeed", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const uint8_t *)cn, -1, -1, 0);
    X509_set_issuer_name(x, name);
    X509V3_set_ctx(&v3, x, x, NULL, NULL, 0);
    snprintf(san, sizeof(san), "DNS:%s,DNS:localhost,IP:127.0.0.1", cn);
    ext = X509V3_EXT_nconf_nid(NULL, &v3, NID_subject_alt_name, san);
    if (ext)
    {
        X509_add_ext(x, ext, -1);
        X509_EXTENSION_free(ext);
    }
    ext = X509V3_EXT_nconf_nid(NULL, &v3, NID_basic_constraints, "critical,CA:FALSE");
    if (ext)
    {
        X509_add_ext(x, ext, -1);
        X509_EXTENSION_free(ext);
    }
    if (!X509_sign(x, pkey, EVP_sha256()))
        goto done;
    kb = BIO_new(BIO_s_mem());
    cb = BIO_new(BIO_s_mem());
    if (!kb || !cb || !PEM_write_bio_PrivateKey(kb, pkey, NULL, NULL, 0, NULL, NULL)
        || !PEM_write_bio_X509(cb, x))
        goto done;
    if (write_bio(keyPath, kb, 0600) != 0 || write_bio(certPath, cb, 0644) != 0)
        goto done;
    ret = 0;
done:
    if (ret)
        fprintf(stderr, "failed to create certificate\n");
    BIO_free(kb);
    BIO_free(cb);
    BN_free(serial);
    X509_free(x);
    EVP_PKEY_free(pkey);
    EC_KEY_free(ec);
    return ret;
}

int ols_set_admin_password(const char *htpasswd, const char *user, const char *pass)
{
    unsigned char rnd[16];
    char salt[64], hash[128], line[256];
    if (!user || !*user || strchr(user, ':') || !pass || strlen(pass) < 8)
    {
        fprintf(stderr, "user name must not contain ':' and password must be at least 8 characters\n");
        return 2;
    }
    if (!RAND_bytes(rnd, sizeof(rnd))
        || !_crypt_gensalt_blowfish_rn("$2y$", 12, (const char *)rnd, sizeof(rnd), salt, sizeof(salt))
        || !_crypt_blowfish_rn(pass, salt, hash, sizeof(hash)))
    {
        fprintf(stderr, "failed to hash password\n");
        return 1;
    }
    snprintf(line, sizeof(line), "%s:%s\n", user, hash);
    if (write_text(htpasswd, line, strlen(line), 0600) != 0)
    {
        fprintf(stderr, "failed to write %s: %s\n", htpasswd, strerror(errno));
        return 1;
    }
    return 0;
}
