#include "compat_internal.h"
#include <openssl/md5.h>
#include <openssl/sha.h>

char *_crypt_blowfish_rn(const char *key, const char *setting, char *output, int size);

#define OLS_CRYPT_MAX_KEY 1024

static const char itoa64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static __declspec(thread) char t_out[256];

static char *b64_24(char *p, unsigned b2, unsigned b1, unsigned b0, int n)
{
    unsigned w = (b2 << 16) | (b1 << 8) | b0;
    while (n-- > 0)
    {
        *p++ = itoa64[w & 0x3f];
        w >>= 6;
    }
    return p;
}

static char *md5_crypt(const char *pw, const char *setting, const char *magic)
{
    size_t ml = strlen(magic), pl = strlen(pw), sl;
    const char *salt = setting + ml, *e;
    unsigned char fin[16];
    MD5_CTX ctx, ctx1;
    size_t i;
    char *p;
    for (e = salt; *e && *e != '$' && e < salt + 8; ++e)
        ;
    sl = e - salt;
    MD5_Init(&ctx);
    MD5_Update(&ctx, pw, pl);
    MD5_Update(&ctx, magic, ml);
    MD5_Update(&ctx, salt, sl);
    MD5_Init(&ctx1);
    MD5_Update(&ctx1, pw, pl);
    MD5_Update(&ctx1, salt, sl);
    MD5_Update(&ctx1, pw, pl);
    MD5_Final(fin, &ctx1);
    for (i = pl; i > 0; i -= (i > 16 ? 16 : i))
        MD5_Update(&ctx, fin, i > 16 ? 16 : i);
    memset(fin, 0, sizeof(fin));
    for (i = pl; i; i >>= 1)
    {
        if (i & 1)
            MD5_Update(&ctx, fin, 1);
        else
            MD5_Update(&ctx, pw, 1);
    }
    MD5_Final(fin, &ctx);
    for (i = 0; i < 1000; ++i)
    {
        MD5_Init(&ctx1);
        if (i & 1)
            MD5_Update(&ctx1, pw, pl);
        else
            MD5_Update(&ctx1, fin, 16);
        if (i % 3)
            MD5_Update(&ctx1, salt, sl);
        if (i % 7)
            MD5_Update(&ctx1, pw, pl);
        if (i & 1)
            MD5_Update(&ctx1, fin, 16);
        else
            MD5_Update(&ctx1, pw, pl);
        MD5_Final(fin, &ctx1);
    }
    p = t_out;
    memcpy(p, magic, ml);
    p += ml;
    memcpy(p, salt, sl);
    p += sl;
    *p++ = '$';
    p = b64_24(p, fin[0], fin[6], fin[12], 4);
    p = b64_24(p, fin[1], fin[7], fin[13], 4);
    p = b64_24(p, fin[2], fin[8], fin[14], 4);
    p = b64_24(p, fin[3], fin[9], fin[15], 4);
    p = b64_24(p, fin[4], fin[10], fin[5], 4);
    p = b64_24(p, 0, 0, fin[11], 2);
    *p = 0;
    return t_out;
}

typedef struct
{
    int is512;
    union
    {
        SHA256_CTX c256;
        SHA512_CTX c512;
    } u;
} shactx;

static void sh_init(shactx *c, int is512)
{
    c->is512 = is512;
    if (is512)
        SHA512_Init(&c->u.c512);
    else
        SHA256_Init(&c->u.c256);
}

static void sh_update(shactx *c, const void *d, size_t n)
{
    if (c->is512)
        SHA512_Update(&c->u.c512, d, n);
    else
        SHA256_Update(&c->u.c256, d, n);
}

static void sh_final(shactx *c, unsigned char *out)
{
    if (c->is512)
        SHA512_Final(out, &c->u.c512);
    else
        SHA256_Final(out, &c->u.c256);
}

static char *sha_crypt(const char *key, const char *setting, int is512)
{
    const char *magic = is512 ? "$6$" : "$5$";
    const char *salt = setting + 3, *e;
    size_t hl = is512 ? 64 : 32, kl = strlen(key), sl, cnt;
    unsigned long rounds = 5000;
    int custom = 0;
    unsigned char A[64], B[64], DP[64], DS[64], P[OLS_CRYPT_MAX_KEY + 1], S[17];
    shactx ctx;
    char *p;
    if (strncmp(salt, "rounds=", 7) == 0)
    {
        char *end;
        unsigned long r = strtoul(salt + 7, &end, 10);
        if (*end == '$')
        {
            salt = end + 1;
            rounds = r < 1000 ? 1000 : (r > 999999999 ? 999999999 : r);
            custom = 1;
        }
    }
    for (e = salt; *e && *e != '$' && e < salt + 16; ++e)
        ;
    sl = e - salt;

    sh_init(&ctx, is512);
    sh_update(&ctx, key, kl);
    sh_update(&ctx, salt, sl);
    sh_update(&ctx, key, kl);
    sh_final(&ctx, B);

    sh_init(&ctx, is512);
    sh_update(&ctx, key, kl);
    sh_update(&ctx, salt, sl);
    for (cnt = kl; cnt > hl; cnt -= hl)
        sh_update(&ctx, B, hl);
    sh_update(&ctx, B, cnt);
    for (cnt = kl; cnt > 0; cnt >>= 1)
    {
        if (cnt & 1)
            sh_update(&ctx, B, hl);
        else
            sh_update(&ctx, key, kl);
    }
    sh_final(&ctx, A);

    sh_init(&ctx, is512);
    for (cnt = 0; cnt < kl; ++cnt)
        sh_update(&ctx, key, kl);
    sh_final(&ctx, DP);
    for (cnt = 0; cnt + hl <= kl; cnt += hl)
        memcpy(P + cnt, DP, hl);
    memcpy(P + cnt, DP, kl - cnt);

    sh_init(&ctx, is512);
    for (cnt = 0; cnt < 16u + A[0]; ++cnt)
        sh_update(&ctx, salt, sl);
    sh_final(&ctx, DS);
    for (cnt = 0; cnt + hl <= sl; cnt += hl)
        memcpy(S + cnt, DS, hl);
    memcpy(S + cnt, DS, sl - cnt);

    for (cnt = 0; cnt < rounds; ++cnt)
    {
        sh_init(&ctx, is512);
        if (cnt & 1)
            sh_update(&ctx, P, kl);
        else
            sh_update(&ctx, A, hl);
        if (cnt % 3)
            sh_update(&ctx, S, sl);
        if (cnt % 7)
            sh_update(&ctx, P, kl);
        if (cnt & 1)
            sh_update(&ctx, A, hl);
        else
            sh_update(&ctx, P, kl);
        sh_final(&ctx, A);
    }

    p = t_out;
    memcpy(p, magic, 3);
    p += 3;
    if (custom)
        p += sprintf(p, "rounds=%lu$", rounds);
    memcpy(p, salt, sl);
    p += sl;
    *p++ = '$';
    if (is512)
    {
        p = b64_24(p, A[0], A[21], A[42], 4);
        p = b64_24(p, A[22], A[43], A[1], 4);
        p = b64_24(p, A[44], A[2], A[23], 4);
        p = b64_24(p, A[3], A[24], A[45], 4);
        p = b64_24(p, A[25], A[46], A[4], 4);
        p = b64_24(p, A[47], A[5], A[26], 4);
        p = b64_24(p, A[6], A[27], A[48], 4);
        p = b64_24(p, A[28], A[49], A[7], 4);
        p = b64_24(p, A[50], A[8], A[29], 4);
        p = b64_24(p, A[9], A[30], A[51], 4);
        p = b64_24(p, A[31], A[52], A[10], 4);
        p = b64_24(p, A[53], A[11], A[32], 4);
        p = b64_24(p, A[12], A[33], A[54], 4);
        p = b64_24(p, A[34], A[55], A[13], 4);
        p = b64_24(p, A[56], A[14], A[35], 4);
        p = b64_24(p, A[15], A[36], A[57], 4);
        p = b64_24(p, A[37], A[58], A[16], 4);
        p = b64_24(p, A[59], A[17], A[38], 4);
        p = b64_24(p, A[18], A[39], A[60], 4);
        p = b64_24(p, A[40], A[61], A[19], 4);
        p = b64_24(p, A[62], A[20], A[41], 4);
        p = b64_24(p, 0, 0, A[63], 2);
    }
    else
    {
        p = b64_24(p, A[0], A[10], A[20], 4);
        p = b64_24(p, A[21], A[1], A[11], 4);
        p = b64_24(p, A[12], A[22], A[2], 4);
        p = b64_24(p, A[3], A[13], A[23], 4);
        p = b64_24(p, A[24], A[4], A[14], 4);
        p = b64_24(p, A[15], A[25], A[5], 4);
        p = b64_24(p, A[6], A[16], A[26], 4);
        p = b64_24(p, A[27], A[7], A[17], 4);
        p = b64_24(p, A[18], A[28], A[8], 4);
        p = b64_24(p, A[9], A[19], A[29], 4);
        p = b64_24(p, 0, A[31], A[30], 3);
    }
    *p = 0;
    return t_out;
}

char *crypt(const char *key, const char *salt)
{
    if (!key || !salt)
    {
        strcpy(t_out, "*0");
        return t_out;
    }
    if (strlen(key) > OLS_CRYPT_MAX_KEY)
    {
        strcpy(t_out, salt[0] == '*' && salt[1] == '0' ? "*1" : "*0");
        return t_out;
    }
    if (strncmp(salt, "$1$", 3) == 0)
        return md5_crypt(key, salt, "$1$");
    if (strncmp(salt, "$apr1$", 6) == 0)
        return md5_crypt(key, salt, "$apr1$");
    if (strncmp(salt, "$5$", 3) == 0)
        return sha_crypt(key, salt, 0);
    if (strncmp(salt, "$6$", 3) == 0)
        return sha_crypt(key, salt, 1);
    if (salt[0] == '$' && salt[1] == '2')
    {
        if (_crypt_blowfish_rn(key, salt, t_out, sizeof(t_out)))
            return t_out;
    }
    strcpy(t_out, salt[0] == '*' && salt[1] == '0' ? "*1" : "*0");
    return t_out;
}
