#ifndef OLS_PHPDETECT_H
#define OLS_PHPDETECT_H

#include <windows.h>
#include <tlhelp32.h>
#include <stdlib.h>
#include <wchar.h>

#define PHPD_MAX 64

typedef struct
{
    wchar_t dir[MAX_PATH];
    wchar_t source[24];
    int major, minor, patch;
    int ts;
    int lsapi;
} phpd_t;

static phpd_t phpd_list[PHPD_MAX];
static int phpd_count;

static int phpd_exists(const wchar_t *p)
{
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static int phpd_is_dir(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int phpd_version(const wchar_t *exe, int *maj, int *min, int *pat)
{
    DWORD h = 0, n = GetFileVersionInfoSizeW(exe, &h);
    VS_FIXEDFILEINFO *fi;
    UINT fl;
    void *buf;
    int ok = 0;
    if (!n || !(buf = malloc(n)))
        return -1;
    if (GetFileVersionInfoW(exe, 0, n, buf) && VerQueryValueW(buf, L"\\", (void **)&fi, &fl) && fl)
    {
        *maj = HIWORD(fi->dwFileVersionMS);
        *min = LOWORD(fi->dwFileVersionMS);
        *pat = HIWORD(fi->dwFileVersionLS);
        ok = 1;
    }
    free(buf);
    return ok ? 0 : -1;
}

static int phpd_is_x64(const wchar_t *exe)
{
    HANDLE f = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    IMAGE_DOS_HEADER dos;
    DWORD got, sig;
    WORD machine = 0;
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    if (ReadFile(f, &dos, sizeof(dos), &got, NULL) && got == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE
        && SetFilePointer(f, dos.e_lfanew, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER
        && ReadFile(f, &sig, 4, &got, NULL) && sig == IMAGE_NT_SIGNATURE)
        ReadFile(f, &machine, 2, &got, NULL);
    CloseHandle(f);
    return machine == IMAGE_FILE_MACHINE_AMD64;
}

static void phpd_add(const wchar_t *dir, const wchar_t *source, const wchar_t *ols_root)
{
    wchar_t full[MAX_PATH], exe[MAX_PATH], dll[MAX_PATH], ls[MAX_PATH];
    phpd_t *p;
    int i;
    size_t n;
    if (phpd_count >= PHPD_MAX || !GetFullPathNameW(dir, MAX_PATH, full, NULL))
        return;
    n = wcslen(full);
    while (n > 3 && (full[n - 1] == L'\\' || full[n - 1] == L'/'))
        full[--n] = 0;
    swprintf(exe, MAX_PATH, L"%s\\php-cgi.exe", full);
    if (!phpd_exists(exe) || !phpd_is_x64(exe))
        return;
    for (i = 0; i < phpd_count; ++i)
        if (_wcsicmp(phpd_list[i].dir, full) == 0)
            return;
    p = &phpd_list[phpd_count];
    memset(p, 0, sizeof(*p));
    if (phpd_version(exe, &p->major, &p->minor, &p->patch) != 0 || p->major < 7)
        return;
    wcsncpy(p->dir, full, MAX_PATH - 1);
    wcsncpy(p->source, source, 23);
    swprintf(dll, MAX_PATH, L"%s\\php%dts.dll", full, p->major);
    p->ts = phpd_exists(dll);
    if (ols_root)
    {
        swprintf(ls, MAX_PATH, L"%s\\bin\\lsphp\\%d.%d-%s\\lsphp.exe", ols_root, p->major, p->minor,
                 p->ts ? L"ts" : L"nts");
        p->lsapi = phpd_exists(ls);
    }
    ++phpd_count;
}

static void phpd_scan(const wchar_t *base, const wchar_t *pattern, const wchar_t *source, const wchar_t *ols_root)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    if (!phpd_is_dir(base))
        return;
    phpd_add(base, source, ols_root);
    swprintf(pat, MAX_PATH, L"%s\\%s", base, pattern);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.')
        {
            swprintf(p, MAX_PATH, L"%s\\%s", base, fd.cFileName);
            phpd_add(p, source, ols_root);
        }
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
}

static void phpd_scan_roots(const wchar_t *parent, const wchar_t *pattern, const wchar_t *inner,
                            const wchar_t *source, const wchar_t *ols_root)
{
    wchar_t drives[128], *d, pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    if (!GetLogicalDriveStringsW(128, drives))
        return;
    for (d = drives; *d; d += wcslen(d) + 1)
    {
        if (GetDriveTypeW(d) != DRIVE_FIXED)
            continue;
        swprintf(pat, MAX_PATH, L"%s%s%s", d, parent, pattern);
        f = FindFirstFileW(pat, &fd);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.')
            {
                swprintf(p, MAX_PATH, L"%s%s%s%s", d, parent, fd.cFileName, inner);
                phpd_scan(p, L"*", source, ols_root);
            }
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
}

static int phpd_reg_string(HKEY root, const wchar_t *key, const wchar_t *name, wchar_t *out, DWORD len)
{
    DWORD type, size = len * sizeof(wchar_t);
    out[0] = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, out, &size) != ERROR_SUCCESS)
        return 0;
    return out[0] != 0;
}

static int phpd_uninstall_location(const wchar_t *display_prefix, wchar_t *out)
{
    static const wchar_t *keys[] = {
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };
    HKEY roots[2] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    int r, k;
    for (r = 0; r < 2; ++r)
        for (k = 0; k < 2; ++k)
        {
            HKEY h;
            DWORD i;
            wchar_t sub[256];
            if (RegOpenKeyExW(roots[r], keys[k], 0, KEY_READ, &h) != ERROR_SUCCESS)
                continue;
            for (i = 0;; ++i)
            {
                DWORD sl = 256;
                wchar_t path[512], name[256];
                if (RegEnumKeyExW(h, i, sub, &sl, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                    break;
                swprintf(path, 512, L"%s\\%s", keys[k], sub);
                if (phpd_reg_string(roots[r], path, L"DisplayName", name, 256)
                    && _wcsnicmp(name, display_prefix, wcslen(display_prefix)) == 0
                    && phpd_reg_string(roots[r], path, L"InstallLocation", out, MAX_PATH) && phpd_is_dir(out))
                {
                    size_t n = wcslen(out);
                    while (n > 3 && (out[n - 1] == L'\\' || out[n - 1] == L'/'))
                        out[--n] = 0;
                    RegCloseKey(h);
                    return 1;
                }
            }
            RegCloseKey(h);
        }
    return 0;
}

static int phpd_running_exe_dir(const wchar_t *exe_name, wchar_t *out)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    int found = 0;
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe))
        do
        {
            if (_wcsicmp(pe.szExeFile, exe_name) == 0)
            {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                DWORD n = MAX_PATH;
                if (h && QueryFullProcessImageNameW(h, 0, out, &n))
                {
                    wchar_t *s = wcsrchr(out, L'\\');
                    if (s)
                    {
                        *s = 0;
                        found = 1;
                    }
                }
                if (h)
                    CloseHandle(h);
            }
        }
        while (!found && Process32NextW(snap, &pe));
    CloseHandle(snap);
    return found;
}

static int phpd_find_on_drives(const wchar_t *rel, const wchar_t *marker, wchar_t *out)
{
    wchar_t drives[128], *d, p[MAX_PATH];
    if (!GetLogicalDriveStringsW(128, drives))
        return 0;
    for (d = drives; *d; d += wcslen(d) + 1)
    {
        if (GetDriveTypeW(d) != DRIVE_FIXED)
            continue;
        swprintf(p, MAX_PATH, L"%s%s\\%s", d, rel, marker);
        if (phpd_exists(p))
        {
            swprintf(out, MAX_PATH, L"%s%s", d, rel);
            return 1;
        }
    }
    return 0;
}

static int phpd_find_laragon(wchar_t *out)
{
    wchar_t p[MAX_PATH];
    if (phpd_running_exe_dir(L"laragon.exe", out))
        return 1;
    if (phpd_uninstall_location(L"Laragon", out))
    {
        swprintf(p, MAX_PATH, L"%s\\laragon.exe", out);
        if (phpd_exists(p))
            return 1;
    }
    return phpd_find_on_drives(L"laragon", L"laragon.exe", out);
}

static int phpd_find_xampp(wchar_t *out)
{
    wchar_t p[MAX_PATH];
    if ((phpd_reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\xampp", L"Install_Dir", out, MAX_PATH)
         || phpd_reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\xampp", L"Install_Dir", out, MAX_PATH)
         || phpd_uninstall_location(L"XAMPP", out)))
    {
        size_t n = wcslen(out);
        while (n > 3 && (out[n - 1] == L'\\' || out[n - 1] == L'/'))
            out[--n] = 0;
        swprintf(p, MAX_PATH, L"%s\\php\\php-cgi.exe", out);
        if (phpd_exists(p))
            return 1;
    }
    if (phpd_running_exe_dir(L"xampp-control.exe", out))
    {
        swprintf(p, MAX_PATH, L"%s\\php\\php-cgi.exe", out);
        if (phpd_exists(p))
            return 1;
    }
    return phpd_find_on_drives(L"xampp", L"php\\php-cgi.exe", out);
}

static int phpd_cmp(const void *a, const void *b)
{
    const phpd_t *x = (const phpd_t *)a, *y = (const phpd_t *)b;
    if (x->major != y->major)
        return y->major - x->major;
    if (x->minor != y->minor)
        return y->minor - x->minor;
    return y->patch - x->patch;
}

static void phpd_detect(const wchar_t *laragon, const wchar_t *xampp, const wchar_t *ols_root)
{
    wchar_t p[MAX_PATH], env[MAX_PATH], *path, *tok, *ctx;
    DWORD n;
    phpd_count = 0;
    if (laragon && laragon[0])
    {
        swprintf(p, MAX_PATH, L"%s\\bin\\php", laragon);
        phpd_scan(p, L"*", L"Laragon", ols_root);
    }
    if (xampp && xampp[0])
    {
        swprintf(p, MAX_PATH, L"%s\\php", xampp);
        phpd_add(p, L"XAMPP", ols_root);
    }
    if (ols_root && ols_root[0])
    {
        swprintf(p, MAX_PATH, L"%s\\php", ols_root);
        phpd_scan(p, L"*", L"OpenLiteSpeed", ols_root);
    }
    n = GetEnvironmentVariableW(L"PATH", NULL, 0);
    if (n && (path = (wchar_t *)malloc(n * sizeof(wchar_t))) != NULL)
    {
        GetEnvironmentVariableW(L"PATH", path, n);
        for (tok = wcstok(path, L";", &ctx); tok; tok = wcstok(NULL, L";", &ctx))
            if (tok[0])
                phpd_add(tok, L"PATH", ols_root);
        free(path);
    }
    phpd_scan_roots(L"", L"php*", L"", L"PHP", ols_root);
    phpd_scan_roots(L"", L"wamp*", L"\\bin\\php", L"WampServer", ols_root);
    phpd_scan_roots(L"tools\\", L"php*", L"", L"Chocolatey", ols_root);
    if (GetEnvironmentVariableW(L"ProgramFiles", env, MAX_PATH))
    {
        swprintf(p, MAX_PATH, L"%s\\PHP", env);
        phpd_scan(p, L"*", L"PHP", ols_root);
    }
    if (GetEnvironmentVariableW(L"USERPROFILE", env, MAX_PATH))
    {
        WIN32_FIND_DATAW fd;
        HANDLE f;
        wchar_t pat[MAX_PATH];
        swprintf(pat, MAX_PATH, L"%s\\scoop\\apps\\php*", env);
        f = FindFirstFileW(pat, &fd);
        if (f != INVALID_HANDLE_VALUE)
        {
            do
            {
                swprintf(p, MAX_PATH, L"%s\\scoop\\apps\\%s\\current", env, fd.cFileName);
                phpd_add(p, L"Scoop", ols_root);
            }
            while (FindNextFileW(f, &fd));
            FindClose(f);
        }
    }
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", env, MAX_PATH))
    {
        swprintf(p, MAX_PATH, L"%s\\Microsoft\\WinGet\\Packages", env);
        phpd_scan(p, L"PHP.PHP*", L"WinGet", ols_root);
    }
    qsort(phpd_list, phpd_count, sizeof(phpd_t), phpd_cmp);
}

static void phpd_label(const phpd_t *p, wchar_t *out, size_t len)
{
    swprintf(out, len, L"PHP %d.%d.%d %s%s  [%s]  %s", p->major, p->minor, p->patch, p->ts ? L"TS" : L"NTS",
             p->lsapi ? L"  LSAPI" : L"", p->source, p->dir);
}

#endif
