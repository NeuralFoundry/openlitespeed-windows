#include "compat_internal.h"
#include <ctype.h>
#include <openssl/curve25519.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define RELEASE_BASE "https://github.com/NeuralFoundry/openlitespeed-windows/releases/download"
#define SIGN_PREFIX "openlitespeed-windows release "
#define SETUP_SUFFIX "-win64-setup.exe"

static const uint8_t s_release_pub[32] = { 0x77, 0x62, 0x3e, 0x72, 0xbb, 0xbe, 0x4d, 0x3b, 0x7d, 0xb2, 0xa2,
                                           0xdd, 0x66, 0x09, 0x88, 0x6c, 0x7f, 0xfa, 0x2c, 0xba, 0xa0, 0x79,
                                           0x97, 0xec, 0x53, 0x9f, 0x9d, 0x76, 0x53, 0xf3, 0x54, 0x7f };

static int safe_name(const char *s, size_t max)
{
    size_t n = strlen(s), i;
    if (n == 0 || n > max || s[0] == '.' || s[0] == '-')
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9')
              || s[i] == '.' || s[i] == '-' || s[i] == '_'))
            return 0;
    return strstr(s, "..") == NULL;
}

static void hex_out(const uint8_t *b, size_t n, char *out)
{
    size_t i;
    for (i = 0; i < n; ++i)
        snprintf(out + i * 2, 3, "%02x", b[i]);
}

static int hex_in(const char *s, uint8_t *out, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
    {
        unsigned v;
        if (!isxdigit((unsigned char)s[i * 2]) || !isxdigit((unsigned char)s[i * 2 + 1])
            || sscanf(s + i * 2, "%2x", &v) != 1)
            return -1;
        out[i] = (uint8_t)v;
    }
    return 0;
}

static int run_wait(wchar_t *cmd, DWORD timeout)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
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

static int download(const char *url, const wchar_t *out)
{
    wchar_t sys[MAX_PATH], wurl[1024], cmd[MAX_PATH * 6];
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0 || MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024) <= 0)
        return -1;
    DeleteFileW(out);
    swprintf(cmd, MAX_PATH * 6,
             L"\"%s\\curl.exe\" -q -fsS --proto =https --tlsv1.2 --ssl-revoke-best-effort --connect-timeout 20 "
             L"--max-time 900 -L -o \"%s\" \"%s\"",
             sys, out, wurl);
    return run_wait(cmd, 960000) == 0 ? 0 : -1;
}

static int sha256_hex(const wchar_t *path, char hex[65])
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    static uint8_t buf[1 << 16];
    uint8_t md[SHA256_DIGEST_LENGTH];
    SHA256_CTX c;
    DWORD got;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    SHA256_Init(&c);
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got)
        SHA256_Update(&c, buf, got);
    SHA256_Final(md, &c);
    CloseHandle(h);
    hex_out(md, sizeof(md), hex);
    return 0;
}

static int load_seed(const wchar_t *key, uint8_t seed[32])
{
    char *t = NULL;
    size_t n = 0;
    int rc = -1;
    if (ols_read_wfile(key, &t, &n) == 0 && n >= 64 && hex_in(t, seed, 32) == 0)
        rc = 0;
    if (t)
    {
        OPENSSL_cleanse(t, n);
        free(t);
    }
    return rc;
}

static char *signed_message(const char *tag, const char *sums, size_t slen, size_t *mlen)
{
    size_t pl = strlen(SIGN_PREFIX) + strlen(tag) + 1;
    char *m = (char *)malloc(pl + slen + 1);
    if (!m)
        return NULL;
    snprintf(m, pl + 1, "%s%s\n", SIGN_PREFIX, tag);
    memcpy(m + pl, sums, slen);
    m[pl + slen] = 0;
    *mlen = pl + slen;
    return m;
}

static int release_key(const char *path)
{
    wchar_t wpath[MAX_PATH], dir[MAX_PATH], *s;
    uint8_t seed[32], pub[32], priv[64];
    char hex[65];
    int i;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH) <= 0)
        return 2;
    if (load_seed(wpath, seed) != 0)
    {
        wcscpy(dir, wpath);
        if ((s = wcsrchr(dir, L'\\')) != NULL)
        {
            *s = 0;
            if (ols_private_dir(dir) != 0)
            {
                fprintf(stderr, "cannot prepare a private folder for the key\n");
                return 1;
            }
        }
        RAND_bytes(seed, sizeof(seed));
        hex_out(seed, sizeof(seed), hex);
        if (ols_write_wfile(wpath, hex, 64, 1) != 0)
        {
            OPENSSL_cleanse(hex, sizeof(hex));
            OPENSSL_cleanse(seed, sizeof(seed));
            fprintf(stderr, "cannot write %s\n", path);
            return 1;
        }
        OPENSSL_cleanse(hex, sizeof(hex));
        printf("new release key written to %s\n", path);
    }
    ED25519_keypair_from_seed(pub, priv, seed);
    OPENSSL_cleanse(seed, sizeof(seed));
    OPENSSL_cleanse(priv, sizeof(priv));
    printf("public key:");
    for (i = 0; i < 32; ++i)
        printf("%s0x%02x", i ? ", " : " { ", pub[i]);
    printf(" }\n");
    return 0;
}

static int sign_release(const char *key, const char *tag, const char *sums, const char *sig)
{
    wchar_t wkey[MAX_PATH], wsums[MAX_PATH], wsig[MAX_PATH];
    uint8_t seed[32], pub[32], priv[64], out[64];
    char *t = NULL, *m, hex[129];
    size_t tl = 0, ml = 0;
    int rc = 1;
    if (!safe_name(tag, 63) || MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, MAX_PATH) <= 0
        || MultiByteToWideChar(CP_UTF8, 0, sums, -1, wsums, MAX_PATH) <= 0
        || MultiByteToWideChar(CP_UTF8, 0, sig, -1, wsig, MAX_PATH) <= 0)
        return 2;
    if (load_seed(wkey, seed) != 0)
    {
        fprintf(stderr, "cannot read the release key\n");
        return 1;
    }
    ED25519_keypair_from_seed(pub, priv, seed);
    OPENSSL_cleanse(seed, sizeof(seed));
    if (ols_read_wfile(wsums, &t, &tl) == 0 && (m = signed_message(tag, t, tl, &ml)) != NULL)
    {
        if (ED25519_sign(out, (const uint8_t *)m, ml, priv))
        {
            hex_out(out, sizeof(out), hex);
            hex[128] = '\n';
            if (ols_write_wfile(wsig, hex, 129, 0) == 0)
                rc = 0;
        }
        free(m);
    }
    OPENSSL_cleanse(priv, sizeof(priv));
    free(t);
    if (rc != 0)
        fprintf(stderr, "signing failed\n");
    return rc;
}

static int verify_sums(const char *tag, const char *sums, size_t slen, const char *sigtext, size_t siglen)
{
    static const uint8_t zero[32] = { 0 };
    uint8_t sig[64];
    size_t ml = 0;
    char *m;
    int ok;
    if (memcmp(s_release_pub, zero, sizeof(zero)) == 0)
    {
        fprintf(stderr, "this build has no release key, so updates cannot be verified\n");
        return 0;
    }
    if (siglen < 128 || hex_in(sigtext, sig, 64) != 0 || (m = signed_message(tag, sums, slen, &ml)) == NULL)
        return 0;
    ok = ED25519_verify((const uint8_t *)m, ml, sig, s_release_pub);
    free(m);
    return ok;
}

static int pick_setup(char *sums, char name[128], char hash[65])
{
    char *line, *next;
    int found = 0;
    for (line = sums; line && *line; line = next)
    {
        char *e = strchr(line, '\n'), *n;
        size_t len;
        next = e ? e + 1 : NULL;
        if (e)
            *e = 0;
        len = strlen(line);
        if (len && line[len - 1] == '\r')
            line[--len] = 0;
        if (len < 67 || line[64] != ' ')
            continue;
        n = line + 65;
        if (*n == ' ' || *n == '*')
            ++n;
        len = strlen(n);
        if (len < sizeof(SETUP_SUFFIX) || len >= 128 || strcmp(n + len - (sizeof(SETUP_SUFFIX) - 1), SETUP_SUFFIX) != 0
            || !safe_name(n, 127))
            continue;
        if (found++)
            return -1;
        memcpy(hash, line, 64);
        hash[64] = 0;
        strcpy(name, n);
    }
    return found == 1 ? 0 : -1;
}

static int update(const char *tag, const char *dirarg)
{
    wchar_t dir[MAX_PATH], wsums[MAX_PATH], wsig[MAX_PATH], wsetup[MAX_PATH], wname[128];
    char base[512], url[1024], *sums = NULL, *sig = NULL, name[128], want[65], got[65], *env;
    size_t sl = 0, gl = 0;
    int rc = 1;
    if (!safe_name(tag, 63) || MultiByteToWideChar(CP_UTF8, 0, dirarg, -1, dir, MAX_PATH) <= 0)
    {
        fprintf(stderr, "invalid release name\n");
        return 2;
    }
    env = getenv("OLS_UPDATE_BASE");
    snprintf(base, sizeof(base), "%s", env && strncmp(env, "https://", 8) == 0 ? env : RELEASE_BASE);
    if (ols_private_dir(dir) != 0)
    {
        fprintf(stderr, "cannot prepare the download folder\n");
        return 1;
    }
    swprintf(wsums, MAX_PATH, L"%s\\SHA256SUMS.txt", dir);
    swprintf(wsig, MAX_PATH, L"%s\\SHA256SUMS.txt.sig", dir);
    snprintf(url, sizeof(url), "%s/%s/SHA256SUMS.txt", base, tag);
    if (download(url, wsums) != 0)
    {
        fprintf(stderr, "could not download the release checksums\n");
        return 1;
    }
    snprintf(url, sizeof(url), "%s/%s/SHA256SUMS.txt.sig", base, tag);
    if (download(url, wsig) != 0)
    {
        fprintf(stderr, "the release has no signature, so it is not installed automatically\n");
        return 1;
    }
    if (ols_read_wfile(wsums, &sums, &sl) != 0 || ols_read_wfile(wsig, &sig, &gl) != 0)
        goto done;
    if (!verify_sums(tag, sums, sl, sig, gl))
    {
        fprintf(stderr, "the release signature is not valid; the update was not installed\n");
        goto done;
    }
    if (pick_setup(sums, name, want) != 0)
    {
        fprintf(stderr, "the release does not list exactly one setup file\n");
        goto done;
    }
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 128);
    swprintf(wsetup, MAX_PATH, L"%s\\%s", dir, wname);
    snprintf(url, sizeof(url), "%s/%s/%s", base, tag, name);
    if (download(url, wsetup) != 0)
    {
        fprintf(stderr, "could not download %s\n", name);
        goto done;
    }
    if (sha256_hex(wsetup, got) != 0 || strcmp(got, want) != 0)
    {
        fprintf(stderr, "%s failed the checksum check\n", name);
        DeleteFileW(wsetup);
        goto done;
    }
    printf("OK %s %ls\n", want, wsetup);
    rc = 0;
done:
    free(sums);
    free(sig);
    return rc;
}

int ols_update_main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[0], "--update") == 0)
        return update(argv[1], argv[2]);
    if (argc >= 2 && strcmp(argv[0], "--release-key") == 0)
        return release_key(argv[1]);
    if (argc >= 5 && strcmp(argv[0], "--sign-release") == 0)
        return sign_release(argv[1], argv[2], argv[3], argv[4]);
    fprintf(stderr, "usage: openlitespeed --update <tag> <folder> | --release-key <file> | "
                    "--sign-release <key> <tag> <sums> <sig>\n");
    return 2;
}
