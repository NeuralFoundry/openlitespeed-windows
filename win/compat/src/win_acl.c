#include "compat_internal.h"
#include <sddl.h>
#include <aclapi.h>

static INIT_ONCE s_sid_once = INIT_ONCE_STATIC_INIT;
static wchar_t s_user_sid[192];

static BOOL CALLBACK sid_cb(PINIT_ONCE once, void *param, void **ctx)
{
    HANDLE tok;
    BYTE buf[512];
    DWORD n;
    wchar_t *str;
    (void)once; (void)param; (void)ctx;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
        return TRUE;
    if (GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &n)
        && ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &str))
    {
        if (wcslen(str) < sizeof(s_user_sid) / sizeof(wchar_t))
            wcscpy(s_user_sid, str);
        LocalFree(str);
    }
    CloseHandle(tok);
    return TRUE;
}

const wchar_t *ols_user_sid(void)
{
    InitOnceExecuteOnce(&s_sid_once, sid_cb, NULL, NULL);
    return s_user_sid[0] ? s_user_sid : NULL;
}

PSECURITY_DESCRIPTOR ols_private_sd(int inherit)
{
    wchar_t sddl[512];
    PSECURITY_DESCRIPTOR sd = NULL;
    const wchar_t *sid = ols_user_sid();
    const wchar_t *f = inherit ? L"OICI" : L"";
    if (!sid)
        return NULL;
    swprintf(sddl, 512, L"D:P(A;%s;GA;;;SY)(A;%s;GA;;;BA)(A;%s;GA;;;%s)", f, f, f, sid);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL))
        return NULL;
    return sd;
}

int ols_secure_handle(HANDLE h, int inherit)
{
    PSECURITY_DESCRIPTOR sd = ols_private_sd(inherit);
    PACL dacl = NULL;
    BOOL present = FALSE, def = FALSE;
    DWORD r = ERROR_INVALID_SECURITY_DESCR;
    if (!sd)
        return -1;
    if (GetSecurityDescriptorDacl(sd, &present, &dacl, &def) && present)
        r = SetSecurityInfo(h, SE_FILE_OBJECT,
                            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                            NULL, NULL, dacl, NULL);
    LocalFree(sd);
    if (r != ERROR_SUCCESS)
    {
        SetLastError(r);
        return -1;
    }
    return 0;
}

int ols_secure_wpath(const wchar_t *path, int inherit)
{
    int r;
    HANDLE h = CreateFileW(path, WRITE_DAC | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    r = ols_secure_handle(h, inherit);
    CloseHandle(h);
    return r;
}

int ols_is_private_handle(HANDLE h)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    PSID owner = NULL;
    PACL dacl = NULL;
    BYTE buf[SECURITY_MAX_SID_SIZE];
    DWORD n = sizeof(buf);
    PSID sys = (PSID)buf;
    int ok = 0;
    ACL_SIZE_INFORMATION ai;
    DWORD i;
    BYTE ubuf[512];
    HANDLE tok;
    PSID me = NULL;
    BYTE abuf[SECURITY_MAX_SID_SIZE];
    DWORD an = sizeof(abuf);
    PSID admins = (PSID)abuf;
    if (GetSecurityInfo(h, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                        &owner, NULL, &dacl, NULL, &sd) != ERROR_SUCCESS)
        return 0;
    if (!CreateWellKnownSid(WinLocalSystemSid, NULL, sys, &n)
        || !CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, admins, &an))
        goto out;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
    {
        if (GetTokenInformation(tok, TokenUser, ubuf, sizeof(ubuf), &n))
            me = ((TOKEN_USER *)ubuf)->User.Sid;
        CloseHandle(tok);
    }
    if (!me || !owner)
        goto out;
    if (!EqualSid(owner, me) && !EqualSid(owner, sys) && !EqualSid(owner, admins))
        goto out;
    if (!dacl || !GetAclInformation(dacl, &ai, sizeof(ai), AclSizeInformation))
        goto out;
    for (i = 0; i < ai.AceCount; ++i)
    {
        ACE_HEADER *ace;
        PSID sid;
        if (!GetAce(dacl, i, (void **)&ace))
            goto out;
        if (ace->AceType != ACCESS_ALLOWED_ACE_TYPE)
            continue;
        sid = (PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart;
        if (!EqualSid(sid, me) && !EqualSid(sid, sys) && !EqualSid(sid, admins))
            goto out;
    }
    ok = 1;
out:
    if (sd)
        LocalFree(sd);
    return ok;
}
