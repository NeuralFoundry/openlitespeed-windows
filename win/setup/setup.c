#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <initguid.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shellapi.h>
#include <compressapi.h>
#include <winsvc.h>
#include <sddl.h>
#include <aclapi.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "phpdetect.h"

#define PRODUCT       L"OpenLiteSpeed for Windows"
#define VERSION       L"1.9.4"
#ifndef OLS_RELEASE
#define OLS_RELEASE "1.9.4"
#endif
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OpenLiteSpeed"
#define MAGIC         "OLSPAYLD"

enum { ID_MODE = 100, ID_DIR, ID_DIR_BR, ID_PHP, ID_PHP_BR, ID_WWW, ID_WWW_BR, ID_PORT, ID_APORT,
       ID_SERVICE, ID_TRAY, ID_ED_SERVER, ID_ED_FULL, ID_INSTALL, ID_CANCEL, ID_LOG, ID_STATUS, ID_OPEN_SITE, ID_OPEN_ADMIN };

#define WM_DONE (WM_APP + 1)
#define WM_LOG  (WM_APP + 2)

typedef struct
{
    char magic[8];
    unsigned long long off, csize, usize;
} trailer_t;

typedef struct
{
    unsigned char *p;
    size_t len, cap;
} buf_t;

static HINSTANCE g_inst;
static HWND g_wnd, g_ctl[32];
static HFONT g_font, g_bold;
static int g_dpi = 96;
static unsigned char *g_payload;
static size_t g_plen;
static wchar_t g_dir[MAX_PATH], g_phpdir[MAX_PATH], g_www[MAX_PATH], g_port[16], g_aport[16];
static int g_mode, g_service, g_tray, g_upgrade, g_ok, g_full = -1;
static wchar_t g_password[64];

static void buf_add(buf_t *b, const void *d, size_t n)
{
    if (b->len + n > b->cap)
    {
        size_t nc = (b->cap + n) * 2 + 4096;
        unsigned char *np = (unsigned char *)realloc(b->p, nc);
        if (!np)
            ExitProcess(3);
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->len, d, n);
    b->len += n;
}

static unsigned char *read_file(const wchar_t *path, size_t *len)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER sz;
    unsigned char *d;
    DWORD got;
    size_t off = 0;
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > (1LL << 31) || !(d = (unsigned char *)malloc((size_t)sz.QuadPart + 1)))
    {
        CloseHandle(h);
        return NULL;
    }
    while (off < (size_t)sz.QuadPart)
    {
        if (!ReadFile(h, d + off, (DWORD)((size_t)sz.QuadPart - off), &got, NULL) || got == 0)
        {
            free(d);
            CloseHandle(h);
            return NULL;
        }
        off += got;
    }
    d[off] = 0;
    *len = off;
    CloseHandle(h);
    return d;
}

static int write_file(const wchar_t *path, const void *d, size_t len)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w;
    BOOL ok;
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    ok = len == 0 || (WriteFile(h, d, (DWORD)len, &w, NULL) && w == len);
    CloseHandle(h);
    return ok ? 0 : -1;
}

static void mkdirs(const wchar_t *path)
{
    wchar_t tmp[MAX_PATH], *p;
    wcsncpy(tmp, path, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = 0;
    for (p = tmp + 3; *p; ++p)
        if (*p == L'\\')
        {
            *p = 0;
            CreateDirectoryW(tmp, NULL);
            *p = L'\\';
        }
    CreateDirectoryW(tmp, NULL);
}

static int pack_dir(buf_t *b, const wchar_t *base, const wchar_t *rel)
{
    wchar_t pat[MAX_PATH], full[MAX_PATH], sub[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    swprintf(pat, MAX_PATH, rel[0] ? L"%s\\%s\\*" : L"%s%s\\*", base, rel);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return -1;
    do
    {
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
            continue;
        swprintf(sub, MAX_PATH, rel[0] ? L"%s\\%s" : L"%s%s", rel, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (pack_dir(b, base, sub) != 0)
            {
                FindClose(f);
                return -1;
            }
        }
        else
        {
            char name[MAX_PATH * 3];
            unsigned int nl;
            unsigned long long sz;
            size_t len;
            unsigned char *data;
            swprintf(full, MAX_PATH, L"%s\\%s", base, sub);
            data = read_file(full, &len);
            nl = (unsigned int)WideCharToMultiByte(CP_UTF8, 0, sub, -1, name, sizeof(name), NULL, NULL);
            if (!data || nl <= 1)
            {
                free(data);
                FindClose(f);
                return -1;
            }
            --nl;
            sz = len;
            buf_add(b, &nl, 4);
            buf_add(b, name, nl);
            buf_add(b, &sz, 8);
            buf_add(b, data, len);
            free(data);
        }
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
    return 0;
}

static unsigned char *image_of(const wchar_t *path, size_t *stub_len, trailer_t *tr)
{
    wchar_t self[MAX_PATH];
    size_t len;
    unsigned char *d;
    if (!path)
    {
        GetModuleFileNameW(NULL, self, MAX_PATH);
        path = self;
    }
    d = read_file(path, &len);
    if (!d)
        return NULL;
    *stub_len = len;
    if (len > sizeof(trailer_t))
    {
        trailer_t t;
        memcpy(&t, d + len - sizeof(t), sizeof(t));
        if (memcmp(t.magic, MAGIC, 8) == 0 && t.off + t.csize + sizeof(t) == len)
        {
            *stub_len = (size_t)t.off;
            if (tr)
                *tr = t;
        }
    }
    return d;
}

#define own_image(l, t) image_of(NULL, l, t)

static int do_pack(const wchar_t *stub, const wchar_t *dir, const wchar_t *out)
{
    buf_t s = { 0 }, o = { 0 };
    size_t stub_len, clen = 0;
    unsigned char *img = image_of(stub, &stub_len, NULL), *comp;
    unsigned int end = 0;
    COMPRESSOR_HANDLE c;
    trailer_t t;
    if (!img || pack_dir(&s, dir, L"") != 0)
    {
        fwprintf(stderr, L"cannot read %s\n", dir);
        return 1;
    }
    buf_add(&s, &end, 4);
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, NULL, &c))
        return 1;
    Compress(c, s.p, s.len, NULL, 0, &clen);
    comp = (unsigned char *)malloc(clen);
    if (!comp || !Compress(c, s.p, s.len, comp, clen, &clen))
    {
        fwprintf(stderr, L"compression failed (%lu)\n", GetLastError());
        return 1;
    }
    CloseCompressor(c);
    buf_add(&o, img, stub_len);
    memcpy(t.magic, MAGIC, 8);
    t.off = stub_len;
    t.csize = clen;
    t.usize = s.len;
    buf_add(&o, comp, clen);
    buf_add(&o, &t, sizeof(t));
    if (write_file(out, o.p, o.len) != 0)
    {
        fwprintf(stderr, L"cannot write %s\n", out);
        return 1;
    }
    wprintf(L"%s: %zu bytes (payload %zu -> %zu)\n", out, o.len, s.len, clen);
    return 0;
}

static int load_payload(void)
{
    size_t stub_len;
    trailer_t t;
    unsigned char *img;
    DECOMPRESSOR_HANDLE d;
    SIZE_T got = 0;
    memset(&t, 0, sizeof(t));
    img = own_image(&stub_len, &t);
    if (!img || memcmp(t.magic, MAGIC, 8) != 0)
        return -1;
    g_payload = (unsigned char *)malloc((size_t)t.usize);
    if (!g_payload || !CreateDecompressor(COMPRESS_ALGORITHM_LZMS, NULL, &d))
        return -1;
    if (!Decompress(d, img + t.off, (SIZE_T)t.csize, g_payload, (SIZE_T)t.usize, &got) || got != t.usize)
        return -1;
    CloseDecompressor(d);
    g_plen = (size_t)t.usize;
    free(img);
    return 0;
}

static int is_user_file(const char *rel)
{
    return _strnicmp(rel, "conf\\", 5) == 0 || _strnicmp(rel, "admin\\conf\\", 11) == 0
           || _strnicmp(rel, "Example\\", 8) == 0;
}

static int is_link(const wchar_t *path)
{
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(path, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    FindClose(f);
    return (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && IsReparseTagNameSurrogate(fd.dwReserved0);
}

static int no_reparse(const wchar_t *dir, const wchar_t *full)
{
    wchar_t tmp[MAX_PATH];
    size_t i, dl = wcslen(dir);
    int link;
    wcsncpy(tmp, full, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = 0;
    for (i = dl + 1; i < MAX_PATH && tmp[i]; ++i)
        if (tmp[i] == L'\\')
        {
            tmp[i] = 0;
            link = is_link(tmp);
            tmp[i] = L'\\';
            if (link)
                return 0;
        }
    return !is_link(full);
}

static int extract(const wchar_t *dir)
{
    size_t pos = 0;
    int n = 0;
    while (pos + 4 <= g_plen)
    {
        unsigned int nl;
        unsigned long long sz;
        char name[MAX_PATH * 3];
        wchar_t wname[MAX_PATH], full[MAX_PATH], *slash;
        memcpy(&nl, g_payload + pos, 4);
        pos += 4;
        if (nl == 0)
            return n;
        if (nl >= sizeof(name) || pos + nl + 8 > g_plen)
            return -1;
        memcpy(name, g_payload + pos, nl);
        name[nl] = 0;
        pos += nl;
        memcpy(&sz, g_payload + pos, 8);
        pos += 8;
        if (pos + sz > g_plen || strstr(name, "..") || name[0] == '\\' || strchr(name, ':'))
            return -1;
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, MAX_PATH);
        swprintf(full, MAX_PATH, L"%s\\%s", dir, wname);
        if (!(g_upgrade && is_user_file(name) && GetFileAttributesW(full) != INVALID_FILE_ATTRIBUTES))
        {
            if (!no_reparse(dir, full))
                return -1;
            if ((slash = wcsrchr(full, L'\\')) != NULL)
            {
                *slash = 0;
                mkdirs(full);
                *slash = L'\\';
            }
            if (!no_reparse(dir, full))
                return -1;
            DeleteFileW(full);
            if (write_file(full, g_payload + pos, (size_t)sz) != 0)
                return -1;
            ++n;
        }
        pos += (size_t)sz;
    }
    return -1;
}

static int exists(const wchar_t *p)
{
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

#define DL_BASE 1000
static const wchar_t *s_dl_branches[] = { L"8.5", L"8.4", L"8.3", L"8.2" };
static wchar_t g_laragon[MAX_PATH], g_xampp[MAX_PATH], g_download[8], g_existing[MAX_PATH];

static int conf_php_dir(const wchar_t *root, wchar_t *php);

static int payload_has(const char *want)
{
    size_t pos = 0;
    while (g_payload && pos + 4 <= g_plen)
    {
        unsigned int nl;
        unsigned long long sz;
        memcpy(&nl, g_payload + pos, 4);
        pos += 4;
        if (nl == 0 || pos + nl + 8 > g_plen)
            return 0;
        if (nl == strlen(want) && _strnicmp((const char *)g_payload + pos, want, nl) == 0)
            return 1;
        pos += nl;
        memcpy(&sz, g_payload + pos, 8);
        pos += 8 + (size_t)sz;
    }
    return 0;
}

static void mark_lsapi(phpd_t *p)
{
    char name[64];
    snprintf(name, sizeof(name), "bin\\lsphp\\%d.%d-%s\\lsphp.exe", p->major, p->minor, p->ts ? "ts" : "nts");
    p->lsapi = payload_has(name);
}

static void detect_all(void)
{
    int i;
    g_laragon[0] = g_xampp[0] = 0;
    if (!phpd_find_laragon(g_laragon))
        g_laragon[0] = 0;
    if (!phpd_find_xampp(g_xampp))
        g_xampp[0] = 0;
    phpd_detect(g_laragon, g_xampp, NULL);
    {
        DWORD type, size = sizeof(g_existing);
        wchar_t exe[MAX_PATH];
        g_existing[0] = 0;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, L"InstallLocation", RRF_RT_REG_SZ, &type, g_existing,
                         &size) == ERROR_SUCCESS)
        {
            swprintf(exe, MAX_PATH, L"%s\\bin\\openlitespeed.exe", g_existing);
            if (!exists(exe))
                g_existing[0] = 0;
            else if (conf_php_dir(g_existing, exe))
            {
                phpd_add(exe, L"Current", NULL);
                qsort(phpd_list, phpd_count, sizeof(phpd_t), phpd_cmp);
            }
        }
    }
    for (i = 0; i < phpd_count; ++i)
        mark_lsapi(&phpd_list[i]);
}

static int has_laragon(void) { return g_laragon[0] != 0; }
static int has_xampp(void)   { return g_xampp[0] != 0; }

static int existing_mode(void)
{
    wchar_t p[MAX_PATH];
    if (!g_existing[0])
        return -1;
    swprintf(p, MAX_PATH, L"%s\\bin\\openlitespeed", g_laragon);
    if (g_laragon[0] && _wcsicmp(p, g_existing) == 0)
        return 1;
    swprintf(p, MAX_PATH, L"%s\\openlitespeed", g_xampp);
    if (g_xampp[0] && _wcsicmp(p, g_existing) == 0)
        return 2;
    return 0;
}

static int default_mode(void)
{
    int m = existing_mode();
    return m >= 0 ? m : has_laragon() ? 1 : has_xampp() ? 2 : 0;
}

static int port_free(int port)
{
    WSADATA w;
    SOCKET s;
    struct sockaddr_in a;
    int ok;
    WSAStartup(MAKEWORD(2, 2), &w);
    s = socket(AF_INET, SOCK_STREAM, 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    ok = bind(s, (struct sockaddr *)&a, sizeof(a)) == 0;
    closesocket(s);
    return ok;
}

static int dpi(int v)
{
    return MulDiv(v, g_dpi, 96);
}

static HWND ctl(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(wcscmp(cls, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, cls, text,
                             WS_CHILD | WS_VISIBLE | style, dpi(x), dpi(y), dpi(w), dpi(h), g_wnd,
                             (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (id >= 100 && id < 132)
        g_ctl[id - 100] = c;
    return c;
}

static HWND item(int id)
{
    return g_ctl[id - 100];
}

static int mode_defaults(int m, wchar_t *dir, wchar_t *www)
{
    wchar_t v[MAX_PATH], want[MAX_PATH], ini[MAX_PATH], def[MAX_PATH];
    int i, best = -1;
    want[0] = 0;
    if (m == 1)
    {
        const wchar_t *lr = g_laragon[0] ? g_laragon : L"C:\\laragon";
        swprintf(dir, MAX_PATH, L"%s\\bin\\openlitespeed", lr);
        swprintf(ini, MAX_PATH, L"%s\\usr\\laragon.ini", lr);
        swprintf(def, MAX_PATH, L"%s\\www", lr);
        GetPrivateProfileStringW(L"apache", L"DocumentRoot", def, www, MAX_PATH, ini);
        if (GetPrivateProfileStringW(L"php", L"Version", L"", v, MAX_PATH, ini))
            swprintf(want, MAX_PATH, L"%s\\bin\\php\\%s", lr, v);
    }
    else if (m == 2)
    {
        const wchar_t *xr = g_xampp[0] ? g_xampp : L"C:\\xampp";
        swprintf(dir, MAX_PATH, L"%s\\openlitespeed", xr);
        swprintf(www, MAX_PATH, L"%s\\htdocs", xr);
        swprintf(want, MAX_PATH, L"%s\\php", xr);
    }
    else
    {
        wcscpy(dir, L"C:\\OpenLiteSpeed");
        wcscpy(www, L"C:\\OpenLiteSpeed\\www");
    }
    if (m == existing_mode())
    {
        swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", g_existing);
        wcscpy(dir, g_existing);
        wcscpy(def, www);
        GetPrivateProfileStringW(L"sites", L"www", def, www, MAX_PATH, ini);
        if (conf_php_dir(g_existing, v))
            wcscpy(want, v);
    }
    for (i = 0; i < phpd_count; ++i)
    {
        if (want[0] && _wcsicmp(phpd_list[i].dir, want) == 0)
            return i;
        if (best < 0 && phpd_list[i].lsapi)
            best = i;
    }
    if (best >= 0)
        return best;
    if (phpd_count)
        return 0;
    return DL_BASE + 1;
}

static void select_php_item(int data)
{
    int i, n = (int)SendMessageW(item(ID_PHP), CB_GETCOUNT, 0, 0);
    for (i = 0; i < n; ++i)
        if ((int)SendMessageW(item(ID_PHP), CB_GETITEMDATA, i, 0) == data)
        {
            SendMessageW(item(ID_PHP), CB_SETCURSEL, i, 0);
            return;
        }
}

static int conf_port(const wchar_t *file)
{
    size_t len;
    char *text = (char *)read_file(file, &len), *p, *e;
    int port = 0;
    if (!text)
        return 0;
    if ((p = strstr(text, "listener ")) != NULL && (p = strstr(p, "address")) != NULL)
    {
        for (e = p; *e && *e != '\r' && *e != '\n'; ++e)
            ;
        *e = 0;
        if ((p = strrchr(p, ':')) != NULL)
            port = atoi(p + 1);
    }
    free(text);
    return port;
}

static int existing_full(const wchar_t *dir)
{
    wchar_t ini[MAX_PATH], ed[16];
    swprintf(ini, MAX_PATH, L"%s\\conf\\olstray.ini", dir);
    GetPrivateProfileStringW(L"tray", L"edition", L"server", ed, 16, ini);
    return wcscmp(ed, L"full") == 0;
}

static void edition_state(void)
{
    int full = SendMessageW(item(ID_ED_FULL), BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (full)
        SendMessageW(item(ID_SERVICE), BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(item(ID_SERVICE), !full);
    EnableWindow(item(ID_TRAY), SendMessageW(item(ID_SERVICE), BM_GETCHECK, 0, 0) != BST_CHECKED);
}

static void apply_mode_defaults(void)
{
    int m = (int)SendMessageW(item(ID_MODE), CB_GETCURSEL, 0, 0);
    wchar_t dir[MAX_PATH], www[MAX_PATH], f[MAX_PATH], num[16];
    int sel = mode_defaults(m, dir, www);
    int up = m == existing_mode(), port, full = up ? existing_full(g_existing) : 1;
    SendMessageW(item(ID_ED_FULL), BM_SETCHECK, full ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(item(ID_ED_SERVER), BM_SETCHECK, full ? BST_UNCHECKED : BST_CHECKED, 0);
    SetWindowTextW(item(ID_DIR), dir);
    SetWindowTextW(item(ID_WWW), www);
    select_php_item(sel);
    swprintf(f, MAX_PATH, L"%s\\conf\\httpd_config.conf", g_existing);
    port = up ? conf_port(f) : 0;
    swprintf(num, 16, L"%d", port ? port : port_free(80) ? 80 : 8088);
    SetWindowTextW(item(ID_PORT), num);
    swprintf(f, MAX_PATH, L"%s\\admin\\conf\\admin_config.conf", g_existing);
    port = up ? conf_port(f) : 0;
    swprintf(num, 16, L"%d", port ? port : 7080);
    SetWindowTextW(item(ID_APORT), num);
    EnableWindow(item(ID_PORT), !up);
    EnableWindow(item(ID_APORT), !up);
    EnableWindow(item(ID_WWW), !up);
    EnableWindow(item(ID_WWW_BR), !up);
    edition_state();
    SetWindowTextW(item(ID_INSTALL), up ? L"Upgrade" : L"Install");
    SetWindowTextW(item(ID_STATUS), up ? L"OpenLiteSpeed is already installed here. Upgrading keeps your settings." : L"");
}

static void add_php_item(int idx)
{
    wchar_t label[MAX_PATH + 96];
    int pos;
    phpd_label(&phpd_list[idx], label, MAX_PATH + 96);
    pos = (int)SendMessageW(item(ID_PHP), CB_ADDSTRING, 0, (LPARAM)label);
    SendMessageW(item(ID_PHP), CB_SETITEMDATA, pos, idx);
}

static void browse_folder(int id, const wchar_t *title)
{
    BROWSEINFOW bi;
    PIDLIST_ABSOLUTE pidl;
    wchar_t path[MAX_PATH];
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_wnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    pidl = SHBrowseForFolderW(&bi);
    if (pidl)
    {
        if (SHGetPathFromIDListW(pidl, path))
        {
            if (id == ID_PHP)
            {
                int before = phpd_count, i;
                phpd_add(path, L"Custom", NULL);
                for (i = 0; i < phpd_count; ++i)
                    if (_wcsicmp(phpd_list[i].dir, path) == 0)
                        break;
                if (i == phpd_count)
                    MessageBoxW(g_wnd, L"No 64-bit php-cgi.exe was found in this folder.", PRODUCT, MB_ICONWARNING);
                else
                {
                    if (phpd_count > before)
                    {
                        mark_lsapi(&phpd_list[i]);
                        add_php_item(i);
                    }
                    select_php_item(i);
                }
            }
            else
                SetWindowTextW(item(id), path);
        }
        CoTaskMemFree(pidl);
    }
}

static FILE *g_log;
static int g_silent;

static void log_line(const wchar_t *s)
{
    wchar_t *copy;
    if (g_silent)
    {
        if (g_log)
        {
            fwprintf(g_log, L"%s\n", s);
            fflush(g_log);
        }
        return;
    }
    copy = _wcsdup(s);
    if (copy)
        PostMessageW(g_wnd, WM_LOG, 0, (LPARAM)copy);
}

static int unsafe_arg(const wchar_t *s)
{
    for (; *s; ++s)
        if (*s == L'"' || *s < 0x20)
            return 1;
    return 0;
}

static DWORD run_capture(const wchar_t *cmdline, const wchar_t *cwd)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd, wr;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t *cmd = _wcsdup(cmdline);
    char buf[4096], line[4096];
    size_t ll = 0;
    DWORD got, code = 1;
    if (!cmd || !CreatePipe(&rd, &wr, &sa, 0))
        return 1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd, &si, &pi))
    {
        CloseHandle(rd);
        CloseHandle(wr);
        free(cmd);
        return 1;
    }
    CloseHandle(wr);
    while (ReadFile(rd, buf, sizeof(buf), &got, NULL) && got)
    {
        DWORD i;
        for (i = 0; i < got; ++i)
        {
            if (buf[i] == '\n' || ll == sizeof(line) - 1)
            {
                wchar_t w[4096];
                while (ll && line[ll - 1] == '\r')
                    --ll;
                line[ll] = 0;
                MultiByteToWideChar(CP_OEMCP, 0, line, -1, w, 4096);
                if (wcsncmp(w, L"Admin password: ", 16) == 0)
                {
                    swscanf(w + 16, L"%63s", g_password);
                    if (!g_silent)
                        log_line(w);
                }
                else if (w[0])
                    log_line(w);
                ll = 0;
            }
            else
                line[ll++] = buf[i];
        }
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(rd);
    free(cmd);
    return code;
}

static void shortcut(const wchar_t *lnk, const wchar_t *target, const wchar_t *args, const wchar_t *icon)
{
    IShellLinkW *sl;
    IPersistFile *pf;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)))
        return;
    sl->lpVtbl->SetPath(sl, target);
    if (args)
        sl->lpVtbl->SetArguments(sl, args);
    if (icon)
        sl->lpVtbl->SetIconLocation(sl, icon, 0);
    if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf)))
    {
        pf->lpVtbl->Save(pf, lnk, TRUE);
        pf->lpVtbl->Release(pf);
    }
    sl->lpVtbl->Release(sl);
}

static void start_menu_dir(wchar_t *out)
{
    wchar_t *p = NULL;
    out[0] = 0;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_CommonPrograms, 0, NULL, &p)))
    {
        swprintf(out, MAX_PATH, L"%s\\OpenLiteSpeed", p);
        CoTaskMemFree(p);
    }
}

static void reg_set(HKEY k, const wchar_t *name, const wchar_t *val)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
}

static void stop_app(const wchar_t *dir, const wchar_t *exe, const wchar_t *cls, UINT msg, WPARAM wp)
{
    wchar_t want[MAX_PATH], path[MAX_PATH];
    HWND t = NULL;
    swprintf(want, MAX_PATH, L"%s\\bin\\%s", dir, exe);
    while ((t = FindWindowExW(NULL, t, cls, NULL)) != NULL)
    {
        DWORD pid = 0, n = MAX_PATH;
        HANDLE h;
        GetWindowThreadProcessId(t, &pid);
        if (!pid || (h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) == NULL)
            continue;
        if (QueryFullProcessImageNameW(h, 0, path, &n) && _wcsicmp(path, want) == 0)
        {
            PostMessageW(t, msg, wp, 0);
            WaitForSingleObject(h, 5000);
            CloseHandle(h);
            return;
        }
        CloseHandle(h);
    }
}

static int service_exists(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT), svc = NULL;
    if (scm)
    {
        if ((svc = OpenServiceW(scm, L"OpenLiteSpeed", SERVICE_QUERY_STATUS)) != NULL)
            CloseServiceHandle(svc);
        CloseServiceHandle(scm);
    }
    return svc != NULL;
}

static int desktop_user_sid(wchar_t *out, size_t n)
{
    HWND shell = GetShellWindow();
    DWORD pid = 0, len;
    HANDLE p = NULL, tok = NULL;
    BYTE buf[512];
    wchar_t *str = NULL;
    int rc = -1;
    if (shell && GetWindowThreadProcessId(shell, &pid) && pid)
        p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!OpenProcessToken(p ? p : GetCurrentProcess(), TOKEN_QUERY, &tok))
        tok = NULL;
    if (tok && GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len)
        && !IsWellKnownSid(((TOKEN_USER *)buf)->User.Sid, WinLocalSystemSid)
        && ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &str) && wcslen(str) < n)
    {
        wcscpy(out, str);
        rc = 0;
    }
    if (str)
        LocalFree(str);
    if (tok)
        CloseHandle(tok);
    if (p)
        CloseHandle(p);
    return rc;
}

static int enable_priv(const wchar_t *name)
{
    HANDLE t;
    TOKEN_PRIVILEGES tp;
    int ok = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &t))
        return 0;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueW(NULL, name, &tp.Privileges[0].Luid))
        ok = AdjustTokenPrivileges(t, FALSE, &tp, 0, NULL, NULL) && GetLastError() == ERROR_SUCCESS;
    CloseHandle(t);
    return ok;
}

static BYTE g_selfbuf[512];

static int is_self(PSID s)
{
    HANDLE t;
    DWORD n;
    static int got = -1;
    if (got < 0)
    {
        got = 0;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
        {
            got = GetTokenInformation(t, TokenUser, g_selfbuf, sizeof(g_selfbuf), &n) ? 1 : 0;
            CloseHandle(t);
        }
    }
    return got == 1 && EqualSid(s, ((TOKEN_USER *)g_selfbuf)->User.Sid);
}

static int known_sid(PSID s, PSID me)
{
    return s && (EqualSid(s, me) || is_self(s) || IsWellKnownSid(s, WinBuiltinAdministratorsSid)
                 || IsWellKnownSid(s, WinLocalSystemSid));
}

static int root_state(const wchar_t *dir, PSID me)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    PSID owner = NULL;
    PACL dacl = NULL;
    SECURITY_DESCRIPTOR_CONTROL ctl = 0;
    ACL_SIZE_INFORMATION ai;
    DWORD rev, i;
    int r = 0;
    if (GetNamedSecurityInfoW(dir, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, NULL,
                              &dacl, NULL, &sd) != ERROR_SUCCESS)
        return 0;
    if (!known_sid(owner, me))
        r = -1;
    else if (GetSecurityDescriptorControl(sd, &ctl, &rev) && (ctl & SE_DACL_PROTECTED) && dacl
             && GetAclInformation(dacl, &ai, sizeof(ai), AclSizeInformation))
    {
        r = 1;
        for (i = 0; i < ai.AceCount && r == 1; ++i)
        {
            ACE_HEADER *ace;
            if (!GetAce(dacl, i, (void **)&ace))
                r = 0;
            else if (ace->AceType == ACCESS_ALLOWED_ACE_TYPE
                     && !known_sid((PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart, me))
                r = 0;
        }
    }
    LocalFree(sd);
    return r;
}

static void drop_links(const wchar_t *dir, int depth)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    if (depth > 32 || swprintf(pat, MAX_PATH, L"%s\\*", dir) < 0)
        return;
    if ((f = FindFirstFileExW(pat, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0)) == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
            continue;
        if (swprintf(p, MAX_PATH, L"%s\\%s", dir, fd.cFileName) < 0)
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            if (IsReparseTagNameSurrogate(fd.dwReserved0))
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    RemoveDirectoryW(p);
                else
                    DeleteFileW(p);
            }
        }
        else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            drop_links(p, depth + 1);
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
}

static int reset_tree(const wchar_t *path, PSID admins, PACL empty, int depth)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    DWORD a = GetFileAttributesW(path);
    int rc = 0;
    if (depth > 32 || a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_REPARSE_POINT))
        return depth > 32 ? -1 : 0;
    if (SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT,
                              OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION
                                  | UNPROTECTED_DACL_SECURITY_INFORMATION,
                              admins, NULL, empty, NULL) != ERROR_SUCCESS)
        rc = -1;
    if (!(a & FILE_ATTRIBUTE_DIRECTORY) || swprintf(pat, MAX_PATH, L"%s\\*", path) < 0)
        return rc;
    if ((f = FindFirstFileExW(pat, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0)) == INVALID_HANDLE_VALUE)
        return rc;
    do
    {
        if ((fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
            || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        if (swprintf(p, MAX_PATH, L"%s\\%s", path, fd.cFileName) < 0 || reset_tree(p, admins, empty, depth + 1) != 0)
            rc = -1;
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
    return rc;
}

static const wchar_t *const s_prog_dirs[] = { L"bin", L"conf", L"admin", L"fcgi-bin", L"lsrecaptcha", L"share",
                                              L"modsec", L"Example", L"mariadb", L"tmp", L"logs", L"cachedata",
                                              L"cgid", L"autoupdate", NULL };

static int secure_root(const wchar_t *dir)
{
    wchar_t sids[192], sddl[400], p[MAX_PATH];
    BYTE abuf[SECURITY_MAX_SID_SIZE], ebuf[sizeof(ACL)];
    DWORD an = sizeof(abuf);
    PSID me = NULL, admins = (PSID)abuf;
    PACL empty = (PACL)ebuf, dacl = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    BOOL present = FALSE, def = FALSE;
    int st, i, rc = -1;
    if (is_link(dir))
        return -2;
    if (desktop_user_sid(sids, 192) != 0 || !ConvertStringSidToSidW(sids, &me))
        return -1;
    if ((st = root_state(dir, me)) != 0)
    {
        LocalFree(me);
        return st > 0 ? 0 : -2;
    }
    swprintf(sddl, 400, L"D:PAI(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;%s)", sids);
    if (CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, admins, &an)
        && InitializeAcl(empty, sizeof(ebuf), ACL_REVISION)
        && ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL)
        && GetSecurityDescriptorDacl(sd, &present, &dacl, &def) && present)
    {
        enable_priv(SE_TAKE_OWNERSHIP_NAME);
        enable_priv(SE_RESTORE_NAME);
        for (i = 0; s_prog_dirs[i]; ++i)
        {
            swprintf(p, MAX_PATH, L"%s\\%s", dir, s_prog_dirs[i]);
            if (is_link(p))
                RemoveDirectoryW(p);
            else
                drop_links(p, 0);
        }
        rc = SetNamedSecurityInfoW((LPWSTR)dir, SE_FILE_OBJECT,
                                   OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION
                                       | PROTECTED_DACL_SECURITY_INFORMATION,
                                   admins, NULL, dacl, NULL) == ERROR_SUCCESS ? 0 : -1;
        for (i = 0; rc == 0 && s_prog_dirs[i]; ++i)
        {
            swprintf(p, MAX_PATH, L"%s\\%s", dir, s_prog_dirs[i]);
            if (reset_tree(p, admins, empty, 0) != 0)
                rc = -1;
        }
    }
    if (sd)
        LocalFree(sd);
    LocalFree(me);
    return rc;
}

static void drop_dlls(const wchar_t *dir)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    swprintf(pat, MAX_PATH, L"%s\\bin\\*.dll", dir);
    if ((f = FindFirstFileW(pat, &fd)) == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        swprintf(p, MAX_PATH, L"%s\\bin\\%s", dir, fd.cFileName);
        SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(p);
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
}

static void kill_mariadb(const wchar_t *dir)
{
    wchar_t pidf[MAX_PATH], want[MAX_PATH], img[MAX_PATH];
    unsigned char *t;
    size_t len;
    DWORD pid, n = MAX_PATH;
    HANDLE h;
    swprintf(pidf, MAX_PATH, L"%s\\data\\mariadb.pid", dir);
    swprintf(want, MAX_PATH, L"%s\\mariadb\\bin\\mariadbd.exe", dir);
    if ((t = read_file(pidf, &len)) == NULL)
        return;
    pid = (DWORD)strtoul((const char *)t, NULL, 10);
    free(t);
    if (!pid || (h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid))
                    == NULL)
        return;
    if (QueryFullProcessImageNameW(h, 0, img, &n) && _wcsicmp(img, want) == 0 && TerminateProcess(h, 1))
        WaitForSingleObject(h, 10000);
    CloseHandle(h);
}

static void stop_tray(const wchar_t *dir)
{
    stop_app(dir, L"olspanel.exe", L"OpenLiteSpeedPanel", WM_CLOSE, 0);
    stop_app(dir, L"olstray.exe", L"OpenLiteSpeedTray", WM_COMMAND, 111);
}

static DWORD run_tool(const wchar_t *dir, const wchar_t *args)
{
    wchar_t cmd[MAX_PATH * 3];
    swprintf(cmd, MAX_PATH * 3, L"\"%s\\bin\\openlitespeed.exe\" %s", dir, args);
    return run_capture(cmd, dir);
}

static void delete_tree(const wchar_t *dir, const wchar_t *keep, const wchar_t *keep2);

static DWORD WINAPI install_thread(void *arg)
{
    wchar_t cmd[4096], path[MAX_PATH], sm[MAX_PATH], self[MAX_PATH];
    int r;
    static const wchar_t *modes[] = { L"standalone", L"laragon", L"xampp" };
    HKEY k;
    size_t stub_len;
    unsigned char *img;
    (void)arg;
    mkdirs(g_dir);
    if (!service_exists() && (r = secure_root(g_dir)) != 0)
    {
        if (r == -2)
        {
            log_line(L"Error: the install folder belongs to another account or is a link. Choose another folder.");
            PostMessageW(g_wnd, WM_DONE, 0, 0);
            return 0;
        }
        log_line(L"Warning: could not limit the install folder to your account. Install the Windows service on shared computers.");
    }
    drop_dlls(g_dir);
    swprintf(path, MAX_PATH, L"%s\\admin\\conf\\htpasswd", g_dir);
    g_upgrade = exists(path);
    if (g_upgrade)
    {
        log_line(L"Existing installation found, upgrading (settings are kept).");
        stop_tray(g_dir);
        run_tool(g_dir, L"-s stop");
        swprintf(path, MAX_PATH, L"%s\\modsec\\crs", g_dir);
        delete_tree(path, NULL, NULL);
        RemoveDirectoryW(path);
    }
    log_line(L"Copying files...");
    if (extract(g_dir) < 0)
    {
        log_line(L"Error: could not write the program files.");
        PostMessageW(g_wnd, WM_DONE, 0, 0);
        return 0;
    }
    if (g_upgrade)
    {
        static const wchar_t *old[] = { L"dbg-8.4-nts", L"exp-8.4-nts" };
        int i;
        for (i = 0; i < 2; ++i)
        {
            swprintf(path, MAX_PATH, L"%s\\bin\\lsphp\\%s\\lsphp.exe", g_dir, old[i]);
            DeleteFileW(path);
            *wcsrchr(path, L'\\') = 0;
            RemoveDirectoryW(path);
        }
    }
    {
        wchar_t php_arg[MAX_PATH + 32], cur[MAX_PATH], ps[MAX_PATH];
        UINT sl = GetSystemDirectoryW(ps, MAX_PATH);
        if (sl == 0 || sl > MAX_PATH - 48)
        {
            log_line(L"Error: the Windows folder was not found.");
            PostMessageW(g_wnd, WM_DONE, 0, 0);
            return 0;
        }
        wcscat(ps, L"\\WindowsPowerShell\\v1.0\\powershell.exe");
        if (unsafe_arg(g_www) || unsafe_arg(g_phpdir) || unsafe_arg(g_port) || unsafe_arg(g_aport))
        {
            log_line(L"Error: invalid characters in a setup value.");
            PostMessageW(g_wnd, WM_DONE, 0, 0);
            return 0;
        }
        if (g_upgrade && !g_download[0] && !g_phpdir[0] && conf_php_dir(g_dir, cur))
        {
            int i;
            for (i = 0; i < phpd_count; ++i)
                if (_wcsicmp(phpd_list[i].dir, cur) == 0)
                    wcscpy(g_phpdir, cur);
        }
        if (g_download[0])
            swprintf(php_arg, MAX_PATH + 32, L"-DownloadPhp %s", g_download);
        else
            swprintf(php_arg, MAX_PATH + 32, L"-PhpPath \"%s\"", g_phpdir);
        if (g_upgrade)
            swprintf(cmd, 4096,
                     L"\"%s\" -NoProfile -ExecutionPolicy Bypass -File \"%s\\install.ps1\" -Mode %s %s -PhpOnly",
                     ps, g_dir, modes[g_mode], php_arg);
        else
            swprintf(cmd, 4096,
                     L"\"%s\" -NoProfile -ExecutionPolicy Bypass -File \"%s\\install.ps1\" -Mode %s "
                     L"%s -Www \"%s\" -HttpPort %s -AdminPort %s%s",
                     ps, g_dir, modes[g_mode], php_arg, g_www, g_port, g_aport, g_service ? L" -Service" : L"");
        log_line(L"Configuring...");
        if ((g_download[0] || g_phpdir[0]) && run_capture(cmd, g_dir) != 0)
        {
            log_line(L"Error: configuration failed (see the messages above).");
            PostMessageW(g_wnd, WM_DONE, 0, 0);
            return 0;
        }
    }
    if (g_full < 0)
        g_full = g_upgrade && existing_full(g_dir);
    if (g_full && (g_service || service_exists()))
    {
        log_line(L"The full stack edition runs with the tray app, so this installation keeps the web server only edition.");
        g_full = 0;
    }
    swprintf(path, MAX_PATH, L"%s\\conf\\olstray.ini", g_dir);
    WritePrivateProfileStringW(L"tray", L"edition", g_full ? L"full" : L"server", path);
    if (g_full)
    {
        swprintf(path, MAX_PATH, L"%s\\data\\client.cnf", g_dir);
        if (!exists(path))
        {
            log_line(L"Downloading MariaDB and phpMyAdmin...");
            if (run_tool(g_dir, L"--db fetch") != 0)
                log_line(L"MariaDB was not downloaded. Install it later from the dashboard.");
        }
        swprintf(path, MAX_PATH, L"%s\\bin\\cloudflared.exe", g_dir);
        if (!exists(path))
        {
            log_line(L"Downloading Cloudflare Tunnel...");
            if (run_tool(g_dir, L"--tunnel install") != 0)
                log_line(L"Cloudflare Tunnel was not downloaded. It is downloaded again when you share a site.");
        }
        run_tool(g_dir, L"--dev-hosts");
    }
    if (g_upgrade && g_service)
        run_tool(g_dir, L"-s start");
    img = own_image(&stub_len, NULL);
    swprintf(self, MAX_PATH, L"%s\\uninstall.exe", g_dir);
    if (img)
    {
        write_file(self, img, stub_len);
        free(img);
    }
    start_menu_dir(sm);
    if (sm[0])
    {
        mkdirs(sm);
        swprintf(path, MAX_PATH, L"%s\\OpenLiteSpeed.lnk", sm);
        swprintf(cmd, 4096, L"%s\\bin\\olstray.exe", g_dir);
        shortcut(path, cmd, NULL, cmd);
        swprintf(path, MAX_PATH, L"%s\\Uninstall OpenLiteSpeed.lnk", sm);
        shortcut(path, self, L"--uninstall", self);
        swprintf(path, MAX_PATH, L"%s\\OpenLiteSpeed Dashboard.lnk", sm);
        if (g_full)
        {
            swprintf(cmd, 4096, L"%s\\bin\\olspanel.exe", g_dir);
            shortcut(path, cmd, NULL, cmd);
        }
        else
            DeleteFileW(path);
    }
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS)
    {
        DWORD one = 1;
        reg_set(k, L"DisplayName", PRODUCT);
        reg_set(k, L"DisplayVersion", WIDEN(OLS_RELEASE));
        reg_set(k, L"Publisher", L"OpenLiteSpeed for Windows");
        reg_set(k, L"InstallLocation", g_dir);
        swprintf(cmd, 4096, L"\"%s\" --uninstall", self);
        reg_set(k, L"UninstallString", cmd);
        reg_set(k, L"DisplayIcon", self);
        RegSetValueExW(k, L"NoModify", 0, REG_DWORD, (const BYTE *)&one, 4);
        RegSetValueExW(k, L"NoRepair", 0, REG_DWORD, (const BYTE *)&one, 4);
        RegCloseKey(k);
    }
    if (!g_service && g_tray && RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                              0, KEY_SET_VALUE, &k) == ERROR_SUCCESS)
    {
        swprintf(cmd, 4096, L"\"%s\\bin\\olstray.exe\"", g_dir);
        reg_set(k, L"OpenLiteSpeed", cmd);
        RegCloseKey(k);
    }
    g_ok = 1;
    log_line(L"Done.");
    PostMessageW(g_wnd, WM_DONE, 1, 0);
    return 0;
}

static void start_unelevated(const wchar_t *exe)
{
    wchar_t ex[MAX_PATH];
    UINT n = GetWindowsDirectoryW(ex, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 16)
        return;
    wcscat(ex, L"\\explorer.exe");
    ShellExecuteW(NULL, L"open", ex, exe, NULL, SW_SHOWNORMAL);
}

static void on_install(void)
{
    int i, sel;
    GetWindowTextW(item(ID_DIR), g_dir, MAX_PATH);
    GetWindowTextW(item(ID_WWW), g_www, MAX_PATH);
    GetWindowTextW(item(ID_PORT), g_port, 16);
    GetWindowTextW(item(ID_APORT), g_aport, 16);
    sel = (int)SendMessageW(item(ID_PHP), CB_GETCURSEL, 0, 0);
    g_phpdir[0] = 0;
    g_download[0] = 0;
    if (sel >= 0)
    {
        int data = (int)SendMessageW(item(ID_PHP), CB_GETITEMDATA, sel, 0);
        if (data >= DL_BASE && data < DL_BASE + 4)
            wcscpy(g_download, s_dl_branches[data - DL_BASE]);
        else if (data >= 0 && data < phpd_count)
            wcscpy(g_phpdir, phpd_list[data].dir);
    }
    g_mode = (int)SendMessageW(item(ID_MODE), CB_GETCURSEL, 0, 0);
    g_service = SendMessageW(item(ID_SERVICE), BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_tray = SendMessageW(item(ID_TRAY), BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_full = SendMessageW(item(ID_ED_FULL), BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!g_dir[0] || g_dir[1] != L':' || (!g_phpdir[0] && !g_download[0]) || !g_www[0] || _wtoi(g_port) <= 0
        || _wtoi(g_aport) <= 0)
    {
        MessageBoxW(g_wnd, L"Please fill in the install folder, PHP, projects folder and ports.", PRODUCT, MB_ICONWARNING);
        return;
    }
    for (i = ID_MODE; i <= ID_INSTALL; ++i)
        EnableWindow(item(i), FALSE);
    SetWindowTextW(item(ID_LOG), L"");
    CloseHandle(CreateThread(NULL, 0, install_thread, NULL, 0, NULL));
}

static void finish_page(int ok)
{
    wchar_t msg[1024];
    int port = _wtoi(g_port);
    EnableWindow(item(ID_CANCEL), TRUE);
    SetWindowTextW(item(ID_CANCEL), L"Finish");
    if (!ok)
    {
        SetWindowTextW(item(ID_STATUS), L"Setup did not complete.");
        return;
    }
    if (g_password[0])
        swprintf(msg, 1024, L"Installed. WebAdmin user: admin   password: %s", g_password);
    else
        swprintf(msg, 1024, L"Installed. Your existing settings were kept.");
    SetWindowTextW(item(ID_STATUS), msg);
    ShowWindow(item(ID_INSTALL), SW_HIDE);
    ShowWindow(item(ID_OPEN_SITE), SW_SHOW);
    ShowWindow(item(ID_OPEN_ADMIN), SW_SHOW);
    if (!g_service)
    {
        wchar_t tray[MAX_PATH];
        swprintf(tray, MAX_PATH, L"%s\\bin\\olstray.exe", g_dir);
        start_unelevated(tray);
    }
    if (g_full)
    {
        wchar_t panel[MAX_PATH];
        swprintf(panel, MAX_PATH, L"%s\\bin\\olspanel.exe", g_dir);
        start_unelevated(panel);
    }
    (void)port;
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_MODE:
            if (HIWORD(wp) == CBN_SELCHANGE)
                apply_mode_defaults();
            break;
        case ID_DIR_BR:
            browse_folder(ID_DIR, L"Install folder");
            break;
        case ID_PHP_BR:
            browse_folder(ID_PHP, L"PHP folder (with php-cgi.exe)");
            break;
        case ID_WWW_BR:
            browse_folder(ID_WWW, L"Projects folder");
            break;
        case ID_SERVICE:
        case ID_ED_FULL:
        case ID_ED_SERVER:
            edition_state();
            break;
        case ID_INSTALL:
            on_install();
            break;
        case ID_OPEN_SITE:
        {
            wchar_t u[64];
            int p = _wtoi(g_port);
            swprintf(u, 64, p == 80 ? L"http://localhost/" : L"http://localhost:%d/", p);
            ShellExecuteW(NULL, L"open", u, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case ID_OPEN_ADMIN:
        {
            wchar_t u[64];
            swprintf(u, 64, L"https://localhost:%s/", g_aport);
            ShellExecuteW(NULL, L"open", u, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case ID_CANCEL:
            DestroyWindow(h);
            break;
        }
        return 0;
    case WM_LOG:
    {
        wchar_t *s = (wchar_t *)lp;
        int n = GetWindowTextLengthW(item(ID_LOG));
        SendMessageW(item(ID_LOG), EM_SETSEL, n, n);
        SendMessageW(item(ID_LOG), EM_REPLACESEL, FALSE, (LPARAM)s);
        SendMessageW(item(ID_LOG), EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
        free(s);
        return 0;
    }
    case WM_DONE:
        finish_page((int)wp);
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void build_ui(void)
{
    HWND t;
    int i, y = 16;
    LOGFONTW lf;
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, g_dpi);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    lf = ncm.lfMessageFont;
    lf.lfWeight = FW_SEMIBOLD;
    lf.lfHeight = -MulDiv(15, g_dpi, 72);
    g_bold = CreateFontIndirectW(&lf);
    t = ctl(L"STATIC", PRODUCT L" " VERSION, 0, 20, y, 520, 30, 0);
    SendMessageW(t, WM_SETFONT, (WPARAM)g_bold, TRUE);
    y += 44;
    ctl(L"STATIC", L"Setup type", 0, 20, y + 3, 130, 20, 0);
    ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 160, y, 380, 200, ID_MODE);
    SendMessageW(item(ID_MODE), CB_ADDSTRING, 0, (LPARAM)L"Standalone server");
    {
        wchar_t label[MAX_PATH + 32];
        swprintf(label, MAX_PATH + 32, has_laragon() ? L"Inside Laragon (%s)" : L"Inside Laragon (not found)", g_laragon);
        SendMessageW(item(ID_MODE), CB_ADDSTRING, 0, (LPARAM)label);
        swprintf(label, MAX_PATH + 32, has_xampp() ? L"Inside XAMPP (%s)" : L"Inside XAMPP (not found)", g_xampp);
        SendMessageW(item(ID_MODE), CB_ADDSTRING, 0, (LPARAM)label);
    }
    SendMessageW(item(ID_MODE), CB_SETDROPPEDWIDTH, dpi(760), 0);
    y += 36;
    ctl(L"STATIC", L"Edition", 0, 20, y + 3, 130, 20, 0);
    ctl(L"BUTTON", L"Full stack: dashboard, MariaDB, phpMyAdmin, Cloudflare Tunnel",
        BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP, 160, y, 380, 22, ID_ED_FULL);
    y += 22;
    ctl(L"STATIC", L"Downloads about 170 MB during setup", 0, 178, y, 362, 20, 0);
    y += 24;
    ctl(L"BUTTON", L"Web server only: OpenLiteSpeed, PHP and the tray icon", BS_AUTORADIOBUTTON, 160, y, 380, 22,
        ID_ED_SERVER);
    y += 34;
    ctl(L"STATIC", L"Install folder", 0, 20, y + 3, 130, 20, 0);
    ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_GROUP, 160, y, 290, 24, ID_DIR);
    ctl(L"BUTTON", L"Browse...", WS_TABSTOP, 458, y, 82, 24, ID_DIR_BR);
    y += 36;
    ctl(L"STATIC", L"PHP", 0, 20, y + 3, 130, 20, 0);
    ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 160, y, 290, 300, ID_PHP);
    for (i = 0; i < phpd_count; ++i)
        add_php_item(i);
    for (i = 0; i < 4; ++i)
    {
        wchar_t label[96];
        int pos;
        swprintf(label, 96, L"Download official PHP %s (NTS, LSAPI)%s", s_dl_branches[i],
                 i == 1 ? L" - recommended" : L"");
        pos = (int)SendMessageW(item(ID_PHP), CB_ADDSTRING, 0, (LPARAM)label);
        SendMessageW(item(ID_PHP), CB_SETITEMDATA, pos, DL_BASE + i);
    }
    SendMessageW(item(ID_PHP), CB_SETDROPPEDWIDTH, dpi(760), 0);
    ctl(L"BUTTON", L"Browse...", WS_TABSTOP, 458, y, 82, 24, ID_PHP_BR);
    y += 36;
    ctl(L"STATIC", L"Projects folder", 0, 20, y + 3, 130, 20, 0);
    ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 160, y, 290, 24, ID_WWW);
    ctl(L"BUTTON", L"Browse...", WS_TABSTOP, 458, y, 82, 24, ID_WWW_BR);
    y += 28;
    ctl(L"STATIC", L"Each subfolder becomes http://folder.test/", 0, 160, y, 380, 20, 0);
    y += 30;
    ctl(L"STATIC", L"Website port", 0, 20, y + 3, 130, 20, 0);
    ctl(L"EDIT", port_free(80) ? L"80" : L"8088", ES_NUMBER | WS_TABSTOP, 160, y, 70, 24, ID_PORT);
    ctl(L"STATIC", L"WebAdmin port", 0, 260, y + 3, 110, 20, 0);
    ctl(L"EDIT", L"7080", ES_NUMBER | WS_TABSTOP, 370, y, 70, 24, ID_APORT);
    y += 38;
    ctl(L"BUTTON", L"Run as a Windows service (starts at boot, no tray icon)", BS_AUTOCHECKBOX | WS_TABSTOP,
        20, y, 520, 22, ID_SERVICE);
    y += 28;
    ctl(L"BUTTON", L"Start the tray icon with Windows", BS_AUTOCHECKBOX | WS_TABSTOP, 20, y, 520, 22, ID_TRAY);
    SendMessageW(item(ID_TRAY), BM_SETCHECK, BST_CHECKED, 0);
    y += 34;
    ctl(L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 20, y, 520, 120, ID_LOG);
    y += 128;
    ctl(L"STATIC", L"", SS_LEFT, 20, y + 6, 520, 40, ID_STATUS);
    y += 48;
    ctl(L"BUTTON", L"Install", BS_DEFPUSHBUTTON | WS_TABSTOP, 340, y, 96, 30, ID_INSTALL);
    ctl(L"BUTTON", L"Cancel", WS_TABSTOP, 444, y, 96, 30, ID_CANCEL);
    ctl(L"BUTTON", L"Open website", WS_TABSTOP, 20, y, 120, 30, ID_OPEN_SITE);
    ctl(L"BUTTON", L"Open WebAdmin", WS_TABSTOP, 148, y, 120, 30, ID_OPEN_ADMIN);
    ShowWindow(item(ID_OPEN_SITE), SW_HIDE);
    ShowWindow(item(ID_OPEN_ADMIN), SW_HIDE);
    SendMessageW(item(ID_MODE), CB_SETCURSEL, default_mode(), 0);
    apply_mode_defaults();
}

static void remove_procfile_line(const wchar_t *dir)
{
    wchar_t procfile[MAX_PATH], full[MAX_PATH];
    size_t len;
    char *text, *p, *e;
    swprintf(procfile, MAX_PATH, L"%s\\..\\..\\usr\\Procfile", dir);
    if (!GetFullPathNameW(procfile, MAX_PATH, full, NULL))
        return;
    text = (char *)read_file(full, &len);
    if (!text)
        return;
    if ((p = strstr(text, "OpenLiteSpeed:")) != NULL)
    {
        e = strchr(p, '\n');
        e = e ? e + 1 : p + strlen(p);
        memmove(p, e, strlen(e) + 1);
        write_file(full, text, strlen(text));
    }
    free(text);
}

static int conf_php_dir(const wchar_t *root, wchar_t *php)
{
    wchar_t conf[MAX_PATH], *q;
    size_t len;
    char *text, *p, *e;
    php[0] = 0;
    swprintf(conf, MAX_PATH, L"%s\\conf\\httpd_config.conf", root);
    text = (char *)read_file(conf, &len);
    if (!text)
        return 0;
    if ((p = strstr(text, "# php begin")) != NULL && (p = strstr(p, "PHPRC=")) != NULL)
    {
        p += 6;
        for (e = p; *e && *e != '\r' && *e != '\n'; ++e)
            ;
        *e = 0;
        MultiByteToWideChar(CP_UTF8, 0, p, -1, php, MAX_PATH);
        for (q = php; *q; ++q)
            if (*q == L'/')
                *q = L'\\';
    }
    free(text);
    return php[0] != 0;
}

static int same_content(const wchar_t *a, const wchar_t *b)
{
    size_t la, lb;
    unsigned char *x = read_file(a, &la), *y = read_file(b, &lb);
    int same = x && y && la == lb && memcmp(x, y, la) == 0;
    free(x);
    free(y);
    return same;
}

static int lsphp_is_ours(const wchar_t *dir, const wchar_t *target)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int ours = 0;
    swprintf(pat, MAX_PATH, L"%s\\bin\\lsphp\\*", dir);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.')
        {
            swprintf(p, MAX_PATH, L"%s\\bin\\lsphp\\%s\\lsphp.exe", dir, fd.cFileName);
            ours = same_content(p, target);
        }
    }
    while (!ours && FindNextFileW(f, &fd));
    FindClose(f);
    return ours;
}

static int php_used_elsewhere(const wchar_t *dir, const wchar_t *php)
{
    wchar_t roots[4][MAX_PATH], d[MAX_PATH], other[MAX_PATH], full[MAX_PATH];
    DWORD type, size = sizeof(d);
    int n = 0, i;
    if (phpd_find_laragon(d))
        swprintf(roots[n++], MAX_PATH, L"%s\\bin\\openlitespeed", d);
    if (phpd_find_xampp(d))
        swprintf(roots[n++], MAX_PATH, L"%s\\openlitespeed", d);
    wcscpy(roots[n++], L"C:\\OpenLiteSpeed");
    if (RegGetValueW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, L"InstallLocation", RRF_RT_REG_SZ, &type, d, &size)
        == ERROR_SUCCESS)
        wcscpy(roots[n++], d);
    if (!GetFullPathNameW(dir, MAX_PATH, full, NULL))
        wcscpy(full, dir);
    for (i = 0; i < n; ++i)
    {
        if (_wcsicmp(roots[i], full) == 0)
            continue;
        if (conf_php_dir(roots[i], other) && _wcsicmp(other, php) == 0)
            return 1;
    }
    return 0;
}

static void remove_lsphp(const wchar_t *dir)
{
    wchar_t php[MAX_PATH], target[MAX_PATH];
    if (!conf_php_dir(dir, php))
        return;
    swprintf(target, MAX_PATH, L"%s\\lsphp.exe", php);
    if (exists(target) && lsphp_is_ours(dir, target) && !php_used_elsewhere(dir, php))
        DeleteFileW(target);
}

static void delete_tree(const wchar_t *dir, const wchar_t *keep, const wchar_t *keep2)
{
    wchar_t pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    swprintf(pat, MAX_PATH, L"%s\\*", dir);
    f = FindFirstFileW(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
            continue;
        swprintf(p, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        if ((keep && _wcsicmp(p, keep) == 0) || (keep2 && _wcsicmp(p, keep2) == 0))
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            RemoveDirectoryW(p) || DeleteFileW(p);
        else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            delete_tree(p, keep, keep2);
            RemoveDirectoryW(p);
        }
        else
        {
            SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(p);
        }
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
}

static int uninstall(const wchar_t *dir)
{
    wchar_t sm[MAX_PATH], p[MAX_PATH], data[MAX_PATH];
    HKEY k;
    swprintf(p, MAX_PATH, L"%s\\bin\\openlitespeed.exe", dir);
    if (!exists(p))
    {
        if (!g_silent)
            MessageBoxW(NULL, L"OpenLiteSpeed is not installed in this folder.", PRODUCT, MB_ICONWARNING);
        return 1;
    }
    if (!g_silent
        && MessageBoxW(NULL, L"Remove OpenLiteSpeed from this computer?\n\nYour projects folder and databases are not touched.",
                       PRODUCT, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 1;
    stop_tray(dir);
    run_tool(dir, L"-s stop");
    kill_mariadb(dir);
    run_tool(dir, L"--tunnel stop --all");
    run_tool(dir, L"--uninstall-service");
    run_tool(dir, g_silent ? L"--dev-https off --no-sites --keep-store" : L"--dev-https off --no-sites");
    run_tool(dir, L"--dev-hosts-clear");
    remove_procfile_line(dir);
    remove_lsphp(dir);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE,
                      &k) == ERROR_SUCCESS)
    {
        RegDeleteValueW(k, L"OpenLiteSpeed");
        RegCloseKey(k);
    }
    start_menu_dir(sm);
    if (sm[0])
    {
        delete_tree(sm, NULL, NULL);
        RemoveDirectoryW(sm);
    }
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY);
    swprintf(p, MAX_PATH, L"%s\\www", dir);
    swprintf(data, MAX_PATH, L"%s\\data", dir);
    delete_tree(dir, exists(p) ? p : NULL, exists(data) ? data : NULL);
    if (!RemoveDirectoryW(dir))
    {
        wchar_t self[MAX_PATH];
        size_t dl = wcslen(dir);
        GetModuleFileNameW(NULL, self, MAX_PATH);
        if (_wcsnicmp(self, dir, dl) == 0 && self[dl] == L'\\')
        {
            MoveFileExW(self, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            MoveFileExW(dir, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    }
    if (!g_silent)
        MessageBoxW(NULL, L"OpenLiteSpeed was removed.", PRODUCT, MB_ICONINFORMATION);
    return 0;
}

static const wchar_t *arg_value(int argc, LPWSTR *argv, const wchar_t *name)
{
    int i;
    for (i = 1; i + 1 < argc; ++i)
        if (wcscmp(argv[i], name) == 0)
            return argv[i + 1];
    return NULL;
}

static int arg_flag(int argc, LPWSTR *argv, const wchar_t *name)
{
    int i;
    for (i = 1; i < argc; ++i)
        if (wcscmp(argv[i], name) == 0)
            return 1;
    return 0;
}

static int silent_install(int argc, LPWSTR *argv)
{
    const wchar_t *v;
    wchar_t logpath[MAX_PATH];
    int sel;
    v = arg_value(argc, argv, L"--log");
    if (v)
        wcsncpy(logpath, v, MAX_PATH - 1);
    else
    {
        GetTempPathW(MAX_PATH, logpath);
        wcscat(logpath, L"OpenLiteSpeed-setup.log");
    }
    g_log = _wfopen(logpath, L"w, ccs=UTF-8");
    if (AttachConsole(ATTACH_PARENT_PROCESS))
        _wfreopen(L"CONOUT$", L"w", stdout);
    detect_all();
    v = arg_value(argc, argv, L"--mode");
    g_mode = v ? (wcscmp(v, L"laragon") == 0 ? 1 : wcscmp(v, L"xampp") == 0 ? 2 : 0) : default_mode();
    sel = mode_defaults(g_mode, g_dir, g_www);
    g_phpdir[0] = g_download[0] = 0;
    if (sel >= DL_BASE)
        wcscpy(g_download, s_dl_branches[sel - DL_BASE]);
    else
        wcscpy(g_phpdir, phpd_list[sel].dir);
    if ((v = arg_value(argc, argv, L"--download-php")) != NULL)
    {
        int i;
        for (i = 0; i < 4; ++i)
            if (wcscmp(v, s_dl_branches[i]) == 0)
            {
                wcscpy(g_download, v);
                g_phpdir[0] = 0;
            }
    }
    {
        wchar_t label[MAX_PATH + 96], line[MAX_PATH + 128];
        for (sel = 0; sel < phpd_count; ++sel)
        {
            phpd_label(&phpd_list[sel], label, MAX_PATH + 96);
            swprintf(line, MAX_PATH + 128, L"Detected %s", label);
            log_line(line);
        }
        if (g_laragon[0])
        {
            swprintf(line, MAX_PATH + 128, L"Detected Laragon: %s", g_laragon);
            log_line(line);
        }
        if (g_xampp[0])
        {
            swprintf(line, MAX_PATH + 128, L"Detected XAMPP: %s", g_xampp);
            log_line(line);
        }
    }
    if ((v = arg_value(argc, argv, L"--dir")) != NULL)
        wcsncpy(g_dir, v, MAX_PATH - 1);
    if ((v = arg_value(argc, argv, L"--www")) != NULL)
        wcsncpy(g_www, v, MAX_PATH - 1);
    if ((v = arg_value(argc, argv, L"--php")) != NULL)
        wcsncpy(g_phpdir, v, MAX_PATH - 1);
    wcscpy(g_port, port_free(80) ? L"80" : L"8088");
    if ((v = arg_value(argc, argv, L"--port")) != NULL)
        wcsncpy(g_port, v, 15);
    wcscpy(g_aport, L"7080");
    if ((v = arg_value(argc, argv, L"--admin-port")) != NULL)
        wcsncpy(g_aport, v, 15);
    g_service = arg_flag(argc, argv, L"--service");
    g_tray = !arg_flag(argc, argv, L"--no-tray");
    if ((v = arg_value(argc, argv, L"--edition")) != NULL)
    {
        if (wcscmp(v, L"full") != 0 && wcscmp(v, L"server") != 0)
        {
            log_line(L"Error: --edition takes server or full");
            return 1;
        }
        g_full = wcscmp(v, L"full") == 0;
        if (g_full && g_service)
        {
            log_line(L"Error: --service works with --edition server only");
            return 1;
        }
    }
    if ((v = arg_value(argc, argv, L"--php")) != NULL)
        g_download[0] = 0;
    if (!g_phpdir[0] && !g_download[0])
    {
        log_line(L"Error: no PHP found; pass --php <folder> or --download-php <8.2|8.3|8.4|8.5>");
        return 1;
    }
    install_thread(NULL);
    if (g_password[0])
    {
        fwprintf(stdout, L"Admin password: %s\n", g_password);
        fflush(stdout);
        if (g_log)
            fwprintf(g_log, L"Admin password: ********\n");
    }
    if (g_log)
        fclose(g_log);
    return g_ok ? 0 : 1;
}

static int copy_private(const wchar_t *self, wchar_t *out)
{
    wchar_t dir[MAX_PATH];
    SECURITY_ATTRIBUTES sa;
    PSECURITY_DESCRIPTOR sd = NULL;
    unsigned char rnd[8];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    int ok;
    if (n == 0 || n > MAX_PATH - 48 || !ConvertStringSecurityDescriptorToSecurityDescriptorW(
                                           L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", SDDL_REVISION_1, &sd, NULL))
        return 0;
    BCryptGenRandom(NULL, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    swprintf(dir + n, MAX_PATH - n, L"ols-uninstall-%02x%02x%02x%02x%02x%02x%02x%02x", rnd[0], rnd[1], rnd[2], rnd[3],
             rnd[4], rnd[5], rnd[6], rnd[7]);
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    ok = CreateDirectoryW(dir, &sa);
    LocalFree(sd);
    if (!ok)
        return 0;
    swprintf(out, MAX_PATH, L"%s\\uninstall.exe", dir);
    return CopyFileW(self, out, TRUE);
}

#ifdef OLS_PACK_TOOL
int wmain(int argc, wchar_t **argv)
{
    if (argc < 4)
    {
        fwprintf(stderr, L"usage: olspack <setup-stub.exe> <package-dir> <out-setup.exe>\n");
        return 2;
    }
    return do_pack(argv[1], argv[2], argv[3]);
}
#else
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show)
{
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    WNDCLASSW wc;
    MSG msg;
    RECT r;
    (void)prev; (void)cmdline;
    g_inst = inst;
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_silent = arg_flag(argc, argv, L"--silent");
    if (argc >= 2 && wcscmp(argv[1], L"--uninstall") == 0)
    {
        wchar_t self[MAX_PATH], dir[MAX_PATH], tmp[MAX_PATH], *s;
        GetModuleFileNameW(NULL, self, MAX_PATH);
        if (argc >= 3 && wcscmp(argv[2], L"--silent") != 0)
            return uninstall(argv[2]);
        wcscpy(dir, self);
        if ((s = wcsrchr(dir, L'\\')) != NULL)
            *s = 0;
        if (copy_private(self, tmp))
        {
            wchar_t args[MAX_PATH + 32];
            SHELLEXECUTEINFOW sei;
            HANDLE lock = CreateFileW(tmp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            swprintf(args, MAX_PATH + 32, L"--uninstall \"%s\"%s", dir, g_silent ? L" --silent" : L"");
            memset(&sei, 0, sizeof(sei));
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"open";
            sei.lpFile = tmp;
            sei.lpParameters = args;
            sei.nShow = SW_SHOWNORMAL;
            if (lock != INVALID_HANDLE_VALUE && ShellExecuteExW(&sei))
            {
                if (sei.hProcess)
                {
                    WaitForInputIdle(sei.hProcess, 2000);
                    CloseHandle(sei.hProcess);
                }
                CloseHandle(lock);
                MoveFileExW(tmp, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                if ((s = wcsrchr(tmp, L'\\')) != NULL)
                {
                    *s = 0;
                    MoveFileExW(tmp, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
                return 0;
            }
            if (lock != INVALID_HANDLE_VALUE)
                CloseHandle(lock);
        }
        return uninstall(dir);
    }
    if (load_payload() != 0)
    {
        if (!g_silent)
            MessageBoxW(NULL, L"This setup file is damaged. Please download it again.", PRODUCT, MB_ICONERROR);
        return 1;
    }
    if (g_silent)
        return silent_install(argc, argv);
    detect_all();
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"OpenLiteSpeedSetup";
    RegisterClassW(&wc);
    g_dpi = (int)GetDpiForSystem();
    r.left = 0;
    r.top = 0;
    r.right = dpi(560);
    r.bottom = dpi(670);
    AdjustWindowRectExForDpi(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0, g_dpi);
    g_wnd = CreateWindowW(wc.lpszClassName, PRODUCT L" Setup", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    build_ui();
    ShowWindow(g_wnd, show);
    UpdateWindow(g_wnd);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageW(g_wnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
#endif
