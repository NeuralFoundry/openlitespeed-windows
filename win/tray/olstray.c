#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <time.h>
#include "phpdetect.h"

#define WM_TRAY (WM_APP + 1)
#define WM_UPDATE (WM_APP + 2)
#define WM_NEWSITE (WM_APP + 3)
#define WM_DBDONE (WM_APP + 4)
#define TIMER_STATUS 1
#define TIMER_UPDATE 2
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#ifndef OLS_RELEASE
#define OLS_RELEASE "0"
#endif
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#define RELEASES_URL L"https://github.com/NeuralFoundry/openlitespeed-windows/releases/tag/"
#define UPDATE_EVERY (24 * 3600)
#define ID_START 100
#define ID_STOP 101
#define ID_RELOAD 102
#define ID_SITE 103
#define ID_ADMIN 104
#define ID_WWW 105
#define ID_REFRESH 106
#define ID_FOLDER 107
#define ID_ERRLOG 108
#define ID_ACCLOG 109
#define ID_AUTORUN 110
#define ID_EXIT 111
#define ID_SETWWW 112
#define ID_IMPORT 113
#define ID_UPDATE 114
#define ID_CHECK 115
#define ID_IMPORT_NGINX 116
#define ID_NEWSITE 117
#define ID_HTTPS 118
#define ID_DB_INSTALL 119
#define ID_DB_START 120
#define ID_DB_STOP 121
#define ID_DB_PMA 122
#define ID_DB_COPY 123
#define IDD_NEWSITE 101
#define IDC_NS_NAME 1001
#define IDC_NS_PREVIEW 1002
#define IDC_NS_WP 1003
#define ID_PHP_BASE 1000
#define ID_SITE_BASE 2000
#define MAX_SITES 256

static HINSTANCE g_inst;
static HWND g_wnd;
static NOTIFYICONDATAW g_nid;
static HICON g_icon_on, g_icon_off;
static int g_running = -1;
static wchar_t g_root[MAX_PATH], g_exe[MAX_PATH], g_ini[MAX_PATH];
static wchar_t g_sites[MAX_SITES][128];
static int g_nsites;
static wchar_t g_latest[64];
static volatile LONG g_checking;
static volatile LONG g_creating;
static volatile LONG g_dbbusy;
static int g_update_balloon;

typedef struct
{
    wchar_t name[64];
    int wp;
    DWORD code;
    char *out;
} newsite_t;

static HICON make_icon(COLORREF fill)
{
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, 32, 32), mask = CreateBitmap(32, 32, 1, 1, NULL);
    HBITMAP old = (HBITMAP)SelectObject(dc, color);
    HBRUSH br = CreateSolidBrush(fill), bg = CreateSolidBrush(RGB(0, 0, 0));
    HFONT font = CreateFontW(26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0,
                             L"Segoe UI");
    RECT r = { 0, 0, 32, 32 };
    ICONINFO ii;
    HICON icon;
    FillRect(dc, &r, bg);
    SelectObject(dc, br);
    SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, 1, 1, 31, 31, 10, 10);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, font);
    DrawTextW(dc, L"L", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, mask);
    r.right = r.bottom = 32;
    FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SelectObject(dc, GetStockObject(BLACK_BRUSH));
    RoundRect(dc, 1, 1, 31, 31, 10, 10);
    SelectObject(dc, old);
    ii.fIcon = TRUE;
    ii.xHotspot = ii.yHotspot = 0;
    ii.hbmMask = mask;
    ii.hbmColor = color;
    icon = CreateIconIndirect(&ii);
    DeleteObject(font);
    DeleteObject(br);
    DeleteObject(bg);
    DeleteObject(color);
    DeleteObject(mask);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    return icon;
}

static char *read_all(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, 0, NULL);
    DWORD n, got;
    char *buf;
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    n = GetFileSize(h, NULL);
    buf = (char *)malloc(n + 1);
    if (buf && ReadFile(h, buf, n, &got, NULL))
        buf[got] = 0;
    else
    {
        free(buf);
        buf = NULL;
    }
    CloseHandle(h);
    return buf;
}

static int listener_port(const wchar_t *file, const char *anchor, int def)
{
    char *text = read_all(file), *p;
    int port = def;
    if (!text)
        return def;
    p = anchor ? strstr(text, anchor) : text;
    if (p && (p = strstr(p, "address")) != NULL)
    {
        char *colon = NULL;
        p += 7;
        while (*p && *p != '\n')
        {
            if (*p == ':')
                colon = p;
            ++p;
        }
        if (colon)
            port = atoi(colon + 1);
    }
    free(text);
    return port > 0 ? port : def;
}

static int http_port(void)
{
    wchar_t f[MAX_PATH];
    swprintf(f, MAX_PATH, L"%s\\conf\\httpd_config.conf", g_root);
    return listener_port(f, "listener Default", 80);
}

static int admin_port(void)
{
    wchar_t f[MAX_PATH];
    swprintf(f, MAX_PATH, L"%s\\admin\\conf\\admin_config.conf", g_root);
    return listener_port(f, "listener", 7080);
}

static void current_php(wchar_t *out, size_t len)
{
    wchar_t f[MAX_PATH];
    char *text, *p, *e;
    out[0] = 0;
    swprintf(f, MAX_PATH, L"%s\\conf\\httpd_config.conf", g_root);
    text = read_all(f);
    if (!text)
        return;
    p = strstr(text, "# php begin");
    if (p && (p = strstr(p, "PHPRC=")) != NULL)
    {
        p += 6;
        for (e = p; *e && *e != '\r' && *e != '\n'; ++e)
            ;
        *e = 0;
        MultiByteToWideChar(CP_UTF8, 0, p, -1, out, (int)len);
        for (wchar_t *q = out; *q; ++q)
            if (*q == L'/')
                *q = L'\\';
    }
    free(text);
}

static void load_sites(void)
{
    wchar_t f[MAX_PATH];
    char *text, *p, *end;
    g_nsites = 0;
    swprintf(f, MAX_PATH, L"%s\\conf\\httpd_config.conf", g_root);
    text = read_all(f);
    if (!text)
        return;
    p = strstr(text, "# dev-sites begin");
    end = p ? strstr(p, "# dev-sites end") : NULL;
    while (p && end && g_nsites < MAX_SITES && (p = strstr(p, "vhDomain")) != NULL && p < end)
    {
        char dom[128];
        int i = 0;
        p += 8;
        while (*p == ' ' || *p == '\t')
            ++p;
        while (*p && *p != '\r' && *p != '\n' && *p != ' ' && i < 127)
            dom[i++] = *p++;
        dom[i] = 0;
        if (i)
            MultiByteToWideChar(CP_UTF8, 0, dom, -1, g_sites[g_nsites++], 128);
    }
    free(text);
}

static void find_php(void)
{
    static DWORD last;
    wchar_t laragon[MAX_PATH], xampp[MAX_PATH], extra[MAX_PATH];
    if (last && GetTickCount() - last < 15000)
        return;
    last = GetTickCount();
    if (!phpd_find_laragon(laragon))
        laragon[0] = 0;
    if (!phpd_find_xampp(xampp))
        xampp[0] = 0;
    phpd_detect(laragon, xampp, g_root);
    if (GetPrivateProfileStringW(L"php", L"extra", L"", extra, MAX_PATH, g_ini) && extra[0])
        phpd_scan(extra, L"*", L"Custom", g_root);
    current_php(extra, MAX_PATH);
    if (extra[0])
        phpd_add(extra, L"Current", g_root);
    qsort(phpd_list, phpd_count, sizeof(phpd_t), phpd_cmp);
}

static DWORD run_tool(const wchar_t *args, int wait)
{
    wchar_t cmd[2048];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    swprintf(cmd, 2048, L"\"%s\" %s", g_exe, args);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(g_exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_root, &si, &pi))
        return 1;
    if (wait)
    {
        WaitForSingleObject(pi.hProcess, 120000);
        GetExitCodeProcess(pi.hProcess, &code);
    }
    else
        code = 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

static DWORD run_capture(const wchar_t *args, char **out)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd, wr;
    wchar_t cmd[2048];
    size_t len = 0, cap = 65536;
    DWORD got, code = 1;
    *out = (char *)calloc(cap, 1);
    if (!*out || !CreatePipe(&rd, &wr, &sa, 0))
        return 1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    swprintf(cmd, 2048, L"\"%s\" %s", g_exe, args);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    if (!CreateProcessW(g_exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, g_root, &si, &pi))
    {
        CloseHandle(rd);
        CloseHandle(wr);
        return 1;
    }
    CloseHandle(wr);
    while (ReadFile(rd, *out + len, (DWORD)(cap - len - 1), &got, NULL) && got)
    {
        len += got;
        if (cap - len < 4096)
        {
            char *n = (char *)realloc(*out, cap * 2);
            if (!n)
                break;
            *out = n;
            cap *= 2;
        }
    }
    (*out)[len] = 0;
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, 120000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

static wchar_t *report_text(const char *utf8, const wchar_t *tail)
{
    size_t max = 2400, tl = wcslen(tail);
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t *w = (wchar_t *)calloc((size_t)n + tl + 64, sizeof(wchar_t));
    if (!w)
        return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    if (wcslen(w) > max)
        wcscpy(w + max, L"\n...");
    wcscat(w, tail);
    return w;
}

static void notify(const wchar_t *title, const wchar_t *msg);

static void import_sites(int nginx)
{
    const wchar_t *kind = nginx ? L"nginx" : L"apache", *label = nginx ? L"nginx" : L"Apache";
    wchar_t conf[MAX_PATH] = L"", args[MAX_PATH + 64], log[MAX_PATH], title[64], *msg;
    char *out = NULL;
    DWORD code;
    HANDLE h;
    swprintf(title, 64, L"Import %s sites", label);
    swprintf(args, MAX_PATH + 64, L"--import-%s --dry-run", kind);
    code = run_capture(args, &out);
    if (code == 2)
    {
        OPENFILENAMEW ofn;
        wchar_t pick[64];
        free(out);
        out = NULL;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_wnd;
        ofn.lpstrFilter = nginx ? L"nginx configuration\0nginx.conf;*.conf\0All files\0*.*\0"
                                : L"Apache configuration\0httpd.conf;*.conf\0All files\0*.*\0";
        ofn.lpstrFile = conf;
        ofn.nMaxFile = MAX_PATH;
        swprintf(pick, 64, L"Select %s of %s", nginx ? L"nginx.conf" : L"httpd.conf", label);
        ofn.lpstrTitle = pick;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn))
            return;
        swprintf(args, MAX_PATH + 64, L"--import-%s \"%s\" --dry-run", kind, conf);
        code = run_capture(args, &out);
    }
    if (!out)
        return;
    if (code != 0 || strstr(out, "\n0 site(s) would be imported"))
    {
        msg = report_text(out, L"");
        if (msg)
            MessageBoxW(g_wnd, msg, title, MB_OK | (code ? MB_ICONWARNING : MB_ICONINFORMATION));
        free(msg);
        free(out);
        return;
    }
    msg = report_text(out, L"\n\nImport these sites into OpenLiteSpeed?");
    free(out);
    if (!msg || MessageBoxW(g_wnd, msg, title, MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        free(msg);
        return;
    }
    free(msg);
    if (conf[0])
        swprintf(args, MAX_PATH + 64, L"--import-%s \"%s\"", kind, conf);
    else
        swprintf(args, MAX_PATH + 64, L"--import-%s", kind);
    code = run_capture(args, &out);
    swprintf(log, MAX_PATH, L"%s\\logs\\%s-import.log", g_root, kind);
    h = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD n;
        WriteFile(h, out, (DWORD)strlen(out), &n, NULL);
        CloseHandle(h);
    }
    if (code == 0)
    {
        wchar_t done[64], where[96];
        if (g_running)
            run_tool(L"-s reload", 1);
        swprintf(done, 64, L"%s sites imported", label);
        swprintf(where, 96, L"The report is in logs\\%s-import.log.", kind);
        notify(done, where);
    }
    else if ((msg = report_text(out, L"")) != NULL)
    {
        MessageBoxW(g_wnd, msg, title, MB_OK | MB_ICONWARNING);
        free(msg);
    }
    free(out);
}

static int run_elevated(const wchar_t *args)
{
    SHELLEXECUTEINFOW sei;
    DWORD code = 1;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = g_exe;
    sei.lpParameters = args;
    sei.lpDirectory = g_root;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
        return 1;
    WaitForSingleObject(sei.hProcess, 120000);
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return (int)code;
}

static void notify(const wchar_t *title, const wchar_t *msg)
{
    g_update_balloon = 0;
    g_nid.uFlags = NIF_INFO;
    wcsncpy(g_nid.szInfoTitle, title, 63);
    wcsncpy(g_nid.szInfo, msg, 255);
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static int is_running(void)
{
    wchar_t pidfile[MAX_PATH];
    char *text;
    DWORD pid, code;
    HANDLE h;
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT), svc;
    if (scm)
    {
        svc = OpenServiceW(scm, L"OpenLiteSpeed", SERVICE_QUERY_STATUS);
        if (svc)
        {
            SERVICE_STATUS st;
            int r = QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_RUNNING;
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return r;
        }
        CloseServiceHandle(scm);
    }
    swprintf(pidfile, MAX_PATH, L"%s\\tmp\\lshttpd\\lshttpd.pid", g_root);
    text = read_all(pidfile);
    if (!text)
        return 0;
    pid = (DWORD)atoi(text);
    free(text);
    if (!pid || !(h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)))
        return 0;
    code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return code == STILL_ACTIVE;
}

static void update_status(void)
{
    int r = is_running();
    if (r == g_running)
        return;
    g_running = r;
    g_nid.uFlags = NIF_ICON | NIF_TIP;
    g_nid.hIcon = r ? g_icon_on : g_icon_off;
    swprintf(g_nid.szTip, 128, L"OpenLiteSpeed: %s", r ? L"running" : L"stopped");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void open_url(const wchar_t *url)
{
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

static int version_cmp(const wchar_t *a, const wchar_t *b)
{
    for (;;)
    {
        unsigned long x, y;
        while (*a && !iswdigit(*a))
            ++a;
        while (*b && !iswdigit(*b))
            ++b;
        if (!*a || !*b)
            return *a ? 1 : *b ? -1 : 0;
        x = wcstoul(a, (wchar_t **)&a, 10);
        y = wcstoul(b, (wchar_t **)&b, 10);
        if (x != y)
            return x < y ? -1 : 1;
    }
}

static int parse_tag(const char *json, wchar_t *out, size_t len)
{
    const char *p = strstr(json, "\"tag_name\"");
    size_t n = 0;
    if (!p)
        return 0;
    p += 10;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')
        ++p;
    if (*p++ != '"')
        return 0;
    for (; *p && *p != '"'; ++p)
    {
        if (!(isalnum((unsigned char)*p) || *p == '.' || *p == '-' || *p == '_') || n + 1 >= len)
            return 0;
        out[n++] = (wchar_t)*p;
    }
    out[n] = 0;
    return n > 0 && *p == '"';
}

static int fetch_latest(wchar_t *tag, size_t len)
{
    const DWORD max = 262144;
    int ok = 0;
    DWORD got = 0, n;
    char *buf = (char *)malloc(max);
    HINTERNET ses = WinHttpOpen(L"OpenLiteSpeed-tray/" WIDEN(OLS_RELEASE), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET con = NULL, req = NULL;
    if (!ses)
        ses = WinHttpOpen(L"OpenLiteSpeed-tray/" WIDEN(OLS_RELEASE), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (ses)
    {
        WinHttpSetTimeouts(ses, 10000, 10000, 10000, 10000);
        con = WinHttpConnect(ses, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    }
    if (con)
        req = WinHttpOpenRequest(con, L"GET", L"/repos/NeuralFoundry/openlitespeed-windows/releases/latest", NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (buf && req
        && WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0,
                              0, 0)
        && WinHttpReceiveResponse(req, NULL))
    {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &sz, WINHTTP_NO_HEADER_INDEX);
        while (got + 1 < max && WinHttpReadData(req, buf + got, max - 1 - got, &n) && n)
            got += n;
        buf[got] = 0;
        ok = status == 200 && parse_tag(buf, tag, len);
    }
    if (req)
        WinHttpCloseHandle(req);
    if (con)
        WinHttpCloseHandle(con);
    if (ses)
        WinHttpCloseHandle(ses);
    free(buf);
    return ok;
}

static DWORD WINAPI update_thread(LPVOID arg)
{
    wchar_t *tag = (wchar_t *)calloc(64, sizeof(wchar_t));
    if (tag && !fetch_latest(tag, 64))
    {
        free(tag);
        tag = NULL;
    }
    if (!PostMessageW(g_wnd, WM_UPDATE, (WPARAM)(arg != NULL), (LPARAM)tag))
        free(tag);
    InterlockedExchange(&g_checking, 0);
    return 0;
}

static void check_updates(int manual)
{
    HANDLE th;
    if (InterlockedExchange(&g_checking, 1))
        return;
    th = CreateThread(NULL, 0, update_thread, manual ? (LPVOID)1 : NULL, 0, NULL);
    if (th)
        CloseHandle(th);
    else
        InterlockedExchange(&g_checking, 0);
}

static void schedule_update(void)
{
    wchar_t buf[32];
    long long now = (long long)time(NULL), last, wait;
    KillTimer(g_wnd, TIMER_UPDATE);
    if (!GetPrivateProfileIntW(L"update", L"check", 1, g_ini))
        return;
    GetPrivateProfileStringW(L"update", L"last", L"0", buf, 32, g_ini);
    last = _wtoi64(buf);
    wait = last + UPDATE_EVERY - now;
    if (wait < 30 || last > now)
        wait = 30;
    SetTimer(g_wnd, TIMER_UPDATE, (UINT)(wait * 1000), NULL);
}

static void on_update(int manual, wchar_t *tag)
{
    wchar_t buf[192], seen[64];
    swprintf(buf, 32, L"%lld", (long long)time(NULL));
    WritePrivateProfileStringW(L"update", L"last", buf, g_ini);
    schedule_update();
    if (!tag)
    {
        if (manual)
            notify(L"OpenLiteSpeed", L"Could not check for updates.");
        return;
    }
    if (version_cmp(tag, WIDEN(OLS_RELEASE)) > 0)
    {
        wcsncpy(g_latest, tag, 63);
        GetPrivateProfileStringW(L"update", L"notified", L"", seen, 64, g_ini);
        if (manual || wcscmp(seen, tag) != 0)
        {
            swprintf(buf, 192, L"OpenLiteSpeed %s is available. Click to open the download page.",
                     tag[0] == L'v' ? tag + 1 : tag);
            notify(L"OpenLiteSpeed update", buf);
            g_update_balloon = 1;
            WritePrivateProfileStringW(L"update", L"notified", tag, g_ini);
        }
    }
    else
    {
        g_latest[0] = 0;
        if (manual)
        {
            swprintf(buf, 192, L"OpenLiteSpeed %s is the latest version.", WIDEN(OLS_RELEASE));
            notify(L"OpenLiteSpeed", buf);
        }
    }
    free(tag);
}

static void www_dir(wchar_t *out, size_t len)
{
    wchar_t def[MAX_PATH];
    swprintf(def, MAX_PATH, L"%s\\www", g_root);
    GetPrivateProfileStringW(L"sites", L"www", def, out, (DWORD)len, g_ini);
}

static void refresh_sites(int quiet)
{
    wchar_t www[MAX_PATH], tld[32], args[MAX_PATH + 64];
    DWORD r;
    www_dir(www, MAX_PATH);
    GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, g_ini);
    CreateDirectoryW(www, NULL);
    swprintf(args, MAX_PATH + 64, L"--dev-sites \"%s\" %s%s", www, tld,
             GetPrivateProfileIntW(L"sites", L"root", 0, g_ini) ? L" --root" : L"");
    if (run_tool(args, 1) != 0)
    {
        notify(L"OpenLiteSpeed", L"Could not update the site list.");
        return;
    }
    r = run_tool(L"--dev-hosts", 1);
    if (r == 5)
        r = run_elevated(L"--dev-hosts");
    if (g_running)
        run_tool(L"-s reload", 1);
    load_sites();
    if (!quiet)
    {
        wchar_t msg[128];
        swprintf(msg, 128, r == 0 ? L"%d site(s) ready." : L"%d site(s); hosts file not updated.", g_nsites);
        notify(L"OpenLiteSpeed", msg);
    }
}

static int autorun_enabled(void)
{
    HKEY k;
    DWORD type, n = 0;
    LONG r;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return 0;
    r = RegQueryValueExW(k, L"OpenLiteSpeed", NULL, &type, NULL, &n);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static void toggle_autorun(void)
{
    HKEY k;
    wchar_t self[MAX_PATH], val[MAX_PATH + 4];
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_SET_VALUE, &k) != ERROR_SUCCESS)
        return;
    if (autorun_enabled())
        RegDeleteValueW(k, L"OpenLiteSpeed");
    else
    {
        GetModuleFileNameW(NULL, self, MAX_PATH);
        swprintf(val, MAX_PATH + 4, L"\"%s\"", self);
        RegSetValueExW(k, L"OpenLiteSpeed", 0, REG_SZ, (const BYTE *)val,
                       (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(k);
}

static void choose_www(void)
{
    BROWSEINFOW bi;
    PIDLIST_ABSOLUTE pidl;
    wchar_t path[MAX_PATH];
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_wnd;
    bi.lpszTitle = L"Projects folder (each subfolder becomes name.test)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    pidl = SHBrowseForFolderW(&bi);
    if (!pidl)
        return;
    if (SHGetPathFromIDListW(pidl, path))
    {
        WritePrivateProfileStringW(L"sites", L"www", path, g_ini);
        refresh_sites(0);
    }
    CoTaskMemFree(pidl);
}

static int https_port(void)
{
    return GetPrivateProfileIntW(L"sites", L"https", 0, g_ini);
}

static void site_url(wchar_t *buf, size_t n, const wchar_t *host)
{
    int https = https_port(), port;
    if (https > 0)
    {
        if (https == 443)
            swprintf(buf, n, L"https://%s/", host);
        else
            swprintf(buf, n, L"https://%s:%d/", host, https);
        return;
    }
    port = http_port();
    if (port == 80)
        swprintf(buf, n, L"http://%s/", host);
    else
        swprintf(buf, n, L"http://%s:%d/", host, port);
}

static void toggle_https(void)
{
    int on = https_port() > 0, p;
    const wchar_t *args = on ? L"--dev-https off" : L"--dev-https on";
    char *out = NULL;
    wchar_t *text, msg[128];
    DWORD code;
    if (!on
        && MessageBoxW(g_wnd,
                       L"Windows will ask you to trust a certificate authority made on this computer. "
                       L"It can only sign certificates for .test, .localhost and .internal sites and for "
                       L"this computer's own address.\n\nTurn on trusted HTTPS?",
                       L"Trusted HTTPS", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    AllowSetForegroundWindow(ASFW_ANY);
    code = run_capture(args, &out);
    if (code == 5)
    {
        free(out);
        out = NULL;
        code = (DWORD)run_elevated(args);
    }
    if (code == 3)
    {
        free(out);
        notify(L"OpenLiteSpeed", L"Trusted HTTPS stays off.");
        return;
    }
    if (code != 0)
    {
        text = report_text(out ? out : "", L"");
        MessageBoxW(g_wnd, text && text[0] ? text : L"Trusted HTTPS could not be changed.", L"Trusted HTTPS",
                    MB_OK | MB_ICONWARNING);
        free(text);
        free(out);
        return;
    }
    free(out);
    if (g_running > 0)
        run_tool(L"-s reload", 1);
    load_sites();
    p = https_port();
    if (on)
        wcscpy(msg, L"Trusted HTTPS is off.");
    else if (p == 443)
        wcscpy(msg, L"Trusted HTTPS is on.");
    else
        swprintf(msg, 128, L"Trusted HTTPS is on, port %d.", p);
    notify(L"OpenLiteSpeed", msg);
}

static int db_port(void)
{
    return GetPrivateProfileIntW(L"db", L"port", 0, g_ini);
}

static int db_installed(void)
{
    wchar_t exe[MAX_PATH], cnf[MAX_PATH];
    swprintf(exe, MAX_PATH, L"%s\\mariadb\\bin\\mariadbd.exe", g_root);
    swprintf(cnf, MAX_PATH, L"%s\\data\\client.cnf", g_root);
    return db_port() > 0 && GetFileAttributesW(exe) != INVALID_FILE_ATTRIBUTES
           && GetFileAttributesW(cnf) != INVALID_FILE_ATTRIBUTES;
}

static int db_running(void)
{
    wchar_t pidf[MAX_PATH], img[MAX_PATH];
    DWORD pid, n = MAX_PATH;
    HANDLE h;
    char *t;
    int ok = 0;
    swprintf(pidf, MAX_PATH, L"%s\\data\\mariadb.pid", g_root);
    if ((t = read_all(pidf)) == NULL)
        return 0;
    pid = (DWORD)strtoul(t, NULL, 10);
    free(t);
    if (!pid || (h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)) == NULL)
        return 0;
    if (WaitForSingleObject(h, 0) == WAIT_TIMEOUT && QueryFullProcessImageNameW(h, 0, img, &n))
    {
        size_t l = wcslen(img);
        ok = l >= 12 && _wcsicmp(img + l - 12, L"mariadbd.exe") == 0;
    }
    CloseHandle(h);
    return ok;
}

static void clip_flag(UINT fmt)
{
    HGLOBAL z = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    DWORD *d;
    if (!z || !fmt)
        return;
    if ((d = (DWORD *)GlobalLock(z)) != NULL)
    {
        *d = 0;
        GlobalUnlock(z);
        if (SetClipboardData(fmt, z))
            return;
    }
    GlobalFree(z);
}

static int copy_secret(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    HGLOBAL g;
    wchar_t *w;
    if (n <= 0 || (g = GlobalAlloc(GMEM_MOVEABLE, (size_t)n * sizeof(wchar_t))) == NULL)
        return -1;
    if ((w = (wchar_t *)GlobalLock(g)) == NULL)
    {
        GlobalFree(g);
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    GlobalUnlock(g);
    if (!OpenClipboard(g_wnd))
    {
        GlobalFree(g);
        return -1;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, g))
        GlobalFree(g);
    clip_flag(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"));
    clip_flag(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"));
    clip_flag(RegisterClipboardFormatW(L"CanUploadToCloudClipboard"));
    CloseClipboard();
    return 0;
}

static void db_copy_password(void)
{
    wchar_t cnf[MAX_PATH];
    char *t, *s;
    size_t n;
    swprintf(cnf, MAX_PATH, L"%s\\data\\client.cnf", g_root);
    if ((t = read_all(cnf)) == NULL)
    {
        notify(L"OpenLiteSpeed", L"The database password file was not found.");
        return;
    }
    n = strlen(t);
    if ((s = strstr(t, "\npassword=")) != NULL)
    {
        s += 10;
        s[strcspn(s, "\r\n")] = 0;
        if (copy_secret(s) == 0)
            notify(L"OpenLiteSpeed", L"The root password is on the clipboard. It is not kept in the clipboard history.");
    }
    SecureZeroMemory(t, n);
    free(t);
}

static DWORD WINAPI db_thread(LPVOID arg)
{
    char *out = NULL;
    DWORD code;
    (void)arg;
    code = run_capture(L"--db install", &out);
    if (!PostMessageW(g_wnd, WM_DBDONE, code, (LPARAM)out))
        free(out);
    return 0;
}

static void db_install(void)
{
    HANDLE t;
    if (g_dbbusy
        || MessageBoxW(g_wnd,
                       L"Download MariaDB 11.8.9 (97 MB) and phpMyAdmin 5.2.3 (16 MB) and run them on this computer?\n\n"
                       L"MariaDB listens on 127.0.0.1 only and phpMyAdmin opens only from this computer.",
                       L"Database", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    InterlockedExchange(&g_dbbusy, 1);
    notify(L"OpenLiteSpeed", L"Downloading MariaDB and phpMyAdmin...");
    t = CreateThread(NULL, 0, db_thread, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        InterlockedExchange(&g_dbbusy, 0);
}

static void open_pma(void)
{
    wchar_t tld[32], host[64], url[256];
    GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, g_ini);
    swprintf(host, 64, L"phpmyadmin.%s", tld);
    site_url(url, 256, host);
    open_url(url);
}

static void db_done(DWORD code, char *out)
{
    wchar_t *text;
    DWORD r;
    InterlockedExchange(&g_dbbusy, 0);
    if (code != 0)
    {
        text = report_text(out ? out : "", L"");
        MessageBoxW(g_wnd, text && text[0] ? text : L"The database could not be installed.", L"Database",
                    MB_OK | MB_ICONWARNING);
        free(text);
        free(out);
        return;
    }
    free(out);
    r = run_tool(L"--dev-hosts", 1);
    if (r == 5)
        r = run_elevated(L"--dev-hosts");
    if (g_running > 0)
        run_tool(L"-s reload", 1);
    load_sites();
    notify(L"OpenLiteSpeed", L"MariaDB is running. Sign in to phpMyAdmin as root; Database > Copy root password.");
    if (r == 0 && g_running > 0)
        open_pma();
}

static int site_label_ok(const wchar_t *s)
{
    size_t n = wcslen(s), i;
    if (n == 0 || n > 63 || s[0] == L'-' || s[n - 1] == L'-' || wcscmp(s, L"localhost") == 0)
        return 0;
    for (i = 0; i < n; ++i)
        if (!((s[i] >= L'a' && s[i] <= L'z') || (s[i] >= L'0' && s[i] <= L'9') || s[i] == L'-'))
            return 0;
    return 1;
}

static INT_PTR CALLBACK newsite_proc(HWND d, UINT msg, WPARAM wp, LPARAM lp)
{
    newsite_t *ns = (newsite_t *)GetWindowLongPtrW(d, DWLP_USER);
    if (msg == WM_INITDIALOG)
    {
        SetWindowLongPtrW(d, DWLP_USER, lp);
        SendDlgItemMessageW(d, IDC_NS_NAME, EM_LIMITTEXT, 63, 0);
        EnableWindow(GetDlgItem(d, IDOK), FALSE);
        SetForegroundWindow(d);
        return TRUE;
    }
    if (msg != WM_COMMAND)
        return FALSE;
    if (LOWORD(wp) == IDC_NS_NAME && HIWORD(wp) == EN_CHANGE)
    {
        wchar_t name[64], tld[32], www[MAX_PATH], path[MAX_PATH], prev[160];
        int ok;
        GetDlgItemTextW(d, IDC_NS_NAME, name, 64);
        GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, g_ini);
        ok = site_label_ok(name);
        prev[0] = 0;
        if (ok)
        {
            www_dir(www, MAX_PATH);
            swprintf(path, MAX_PATH, L"%s\\%s", www, name);
            if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
            {
                ok = 0;
                wcscpy(prev, L"A folder with this name already exists");
            }
            else
            {
                wchar_t host[128];
                swprintf(host, 128, L"%s.%s", name, tld);
                site_url(prev, 160, host);
            }
        }
        else if (name[0])
            wcscpy(prev, L"Use lowercase letters, digits and '-'");
        SetDlgItemTextW(d, IDC_NS_PREVIEW, prev);
        EnableWindow(GetDlgItem(d, IDOK), ok);
        return TRUE;
    }
    if (LOWORD(wp) == IDOK && ns)
    {
        GetDlgItemTextW(d, IDC_NS_NAME, ns->name, 64);
        if (!site_label_ok(ns->name))
            return TRUE;
        ns->wp = IsDlgButtonChecked(d, IDC_NS_WP) == BST_CHECKED;
        EndDialog(d, IDOK);
        return TRUE;
    }
    if (LOWORD(wp) == IDCANCEL)
    {
        EndDialog(d, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static DWORD WINAPI newsite_thread(LPVOID arg)
{
    newsite_t *ns = (newsite_t *)arg;
    wchar_t args[128];
    swprintf(args, 128, L"--new-site %s%s", ns->name, ns->wp ? L" --wordpress" : L"");
    ns->code = run_capture(args, &ns->out);
    if (!PostMessageW(g_wnd, WM_NEWSITE, 0, (LPARAM)ns))
    {
        free(ns->out);
        free(ns);
    }
    return 0;
}

static void new_site(void)
{
    newsite_t *ns;
    HANDLE t;
    if (g_creating)
        return;
    ns = (newsite_t *)calloc(1, sizeof(*ns));
    if (!ns)
        return;
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_NEWSITE), g_wnd, newsite_proc, (LPARAM)ns) != IDOK)
    {
        free(ns);
        return;
    }
    InterlockedExchange(&g_creating, 1);
    notify(L"OpenLiteSpeed", ns->wp ? L"Creating the site and downloading WordPress..." : L"Creating the site...");
    t = CreateThread(NULL, 0, newsite_thread, ns, 0, NULL);
    if (t)
        CloseHandle(t);
    else
    {
        InterlockedExchange(&g_creating, 0);
        free(ns);
    }
}

static void newsite_done(newsite_t *ns)
{
    wchar_t tld[32], url[256], msg[256], host[128], *text;
    DWORD r;
    InterlockedExchange(&g_creating, 0);
    if (ns->code != 0)
    {
        text = report_text(ns->out ? ns->out : "", L"");
        MessageBoxW(g_wnd, text ? text : L"The site could not be created.", L"New site", MB_OK | MB_ICONWARNING);
        free(text);
        free(ns->out);
        free(ns);
        return;
    }
    free(ns->out);
    r = run_tool(L"--dev-hosts", 1);
    if (r == 5)
        r = run_elevated(L"--dev-hosts");
    if (g_running > 0)
        run_tool(L"-s reload", 1);
    load_sites();
    GetPrivateProfileStringW(L"sites", L"tld", L"test", tld, 32, g_ini);
    swprintf(host, 128, L"%s.%s", ns->name, tld);
    site_url(url, 256, host);
    if (r != 0)
        swprintf(msg, 256, L"%s.%s was created, but the hosts file was not updated.", ns->name, tld);
    else if (ns->wp)
        swprintf(msg, 256, L"%s.%s is ready. Finish the WordPress setup in the browser.", ns->name, tld);
    else
        swprintf(msg, 256, L"%s.%s is ready.", ns->name, tld);
    notify(L"OpenLiteSpeed", msg);
    if (r == 0 && g_running > 0)
        open_url(url);
    free(ns);
}

static void show_menu(void)
{
    HMENU m = CreatePopupMenu(), php = CreatePopupMenu(), sites = CreatePopupMenu(), logs = CreatePopupMenu(),
          db = CreatePopupMenu();
    wchar_t cur[MAX_PATH], label[MAX_PATH + 32];
    POINT pt;
    int i, cmd;
    update_status();
    find_php();
    load_sites();
    current_php(cur, MAX_PATH);
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, g_running ? L"OpenLiteSpeed is running" : L"OpenLiteSpeed is stopped");
    if (g_latest[0])
    {
        swprintf(label, MAX_PATH + 32, L"Update available: %s", g_latest[0] == L'v' ? g_latest + 1 : g_latest);
        AppendMenuW(m, MF_STRING, ID_UPDATE, label);
    }
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (g_running ? MF_GRAYED : 0), ID_START, L"Start");
    AppendMenuW(m, MF_STRING | (g_running ? 0 : MF_GRAYED), ID_STOP, L"Stop");
    AppendMenuW(m, MF_STRING | (g_running ? 0 : MF_GRAYED), ID_RELOAD, L"Reload");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_SITE, L"Open website");
    AppendMenuW(m, MF_STRING, ID_ADMIN, L"Open WebAdmin");
    AppendMenuW(sites, MF_STRING | (g_creating ? MF_GRAYED : 0), ID_NEWSITE, L"New site...");
    AppendMenuW(sites, MF_SEPARATOR, 0, NULL);
    for (i = 0; i < g_nsites; ++i)
        AppendMenuW(sites, MF_STRING, ID_SITE_BASE + i, g_sites[i]);
    if (g_nsites)
        AppendMenuW(sites, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sites, MF_STRING, ID_REFRESH, L"Refresh sites");
    AppendMenuW(sites, MF_STRING, ID_WWW, L"Open projects folder");
    AppendMenuW(sites, MF_STRING, ID_SETWWW, L"Change projects folder...");
    AppendMenuW(sites, MF_STRING | (https_port() > 0 ? MF_CHECKED : 0), ID_HTTPS, L"Trusted HTTPS");
    AppendMenuW(sites, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sites, MF_STRING, ID_IMPORT, L"Import Apache sites...");
    AppendMenuW(sites, MF_STRING, ID_IMPORT_NGINX, L"Import nginx sites...");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sites, L"Sites");
    for (i = 0; i < phpd_count; ++i)
    {
        const phpd_t *p = &phpd_list[i];
        swprintf(label, MAX_PATH + 32, L"PHP %d.%d.%d %s%s\t%s", p->major, p->minor, p->patch, p->ts ? L"TS" : L"NTS",
                 p->lsapi ? L" LSAPI" : L"", p->source);
        AppendMenuW(php, MF_STRING | (_wcsicmp(cur, p->dir) == 0 ? MF_CHECKED : 0), ID_PHP_BASE + i, label);
    }
    if (!phpd_count)
        AppendMenuW(php, MF_STRING | MF_GRAYED, 0, L"No PHP found");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)php, L"PHP");
    if (db_installed())
    {
        int run = db_running();
        if (run)
            swprintf(label, MAX_PATH + 32, L"MariaDB is running on port %d", db_port());
        else
            wcscpy(label, L"MariaDB is stopped");
        AppendMenuW(db, MF_STRING | MF_GRAYED, 0, label);
        AppendMenuW(db, MF_SEPARATOR, 0, NULL);
        AppendMenuW(db, MF_STRING | (run ? MF_GRAYED : 0), ID_DB_START, L"Start");
        AppendMenuW(db, MF_STRING | (run ? 0 : MF_GRAYED), ID_DB_STOP, L"Stop");
        AppendMenuW(db, MF_STRING, ID_DB_PMA, L"Open phpMyAdmin");
        AppendMenuW(db, MF_STRING, ID_DB_COPY, L"Copy root password");
    }
    else
        AppendMenuW(db, MF_STRING | (g_dbbusy ? MF_GRAYED : 0), ID_DB_INSTALL,
                    g_dbbusy ? L"Installing..." : L"Install MariaDB and phpMyAdmin...");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)db, L"Database");
    AppendMenuW(logs, MF_STRING, ID_ERRLOG, L"Error log");
    AppendMenuW(logs, MF_STRING, ID_ACCLOG, L"Access log");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)logs, L"Logs");
    AppendMenuW(m, MF_STRING, ID_FOLDER, L"Open install folder");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (autorun_enabled() ? MF_CHECKED : 0), ID_AUTORUN, L"Start with Windows");
    AppendMenuW(m, MF_STRING | (g_checking ? MF_GRAYED : 0), ID_CHECK, L"Check for updates");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit");
    GetCursorPos(&pt);
    SetForegroundWindow(g_wnd);
    cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_wnd, NULL);
    DestroyMenu(m);
    if (cmd)
        PostMessageW(g_wnd, WM_COMMAND, cmd, 0);
}

static void on_command(int id)
{
    wchar_t buf[MAX_PATH * 2];
    if (id == ID_START || id == ID_STOP || id == ID_RELOAD)
    {
        DWORD r = run_tool(id == ID_START ? L"-s start" : id == ID_STOP ? L"-s stop" : L"-s reload", 1);
        if (r != 0 && id == ID_START)
            notify(L"OpenLiteSpeed", L"Start failed. Check the error log.");
        g_running = -1;
        update_status();
    }
    else if (id == ID_SITE)
    {
        int port = http_port();
        swprintf(buf, MAX_PATH, port == 80 ? L"http://localhost/" : L"http://localhost:%d/", port);
        open_url(buf);
    }
    else if (id == ID_ADMIN)
    {
        swprintf(buf, MAX_PATH, L"https://localhost:%d/", admin_port());
        open_url(buf);
    }
    else if (id >= ID_SITE_BASE && id < ID_SITE_BASE + g_nsites)
    {
        site_url(buf, MAX_PATH, g_sites[id - ID_SITE_BASE]);
        open_url(buf);
    }
    else if (id >= ID_PHP_BASE && id < ID_PHP_BASE + phpd_count)
    {
        const phpd_t *p = &phpd_list[id - ID_PHP_BASE];
        swprintf(buf, MAX_PATH * 2, L"--set-php \"%s\"", p->dir);
        if (run_tool(buf, 1) != 0)
            notify(L"OpenLiteSpeed", L"Could not switch PHP.");
        else
        {
            if (g_running)
                run_tool(L"-s reload", 1);
            swprintf(buf, MAX_PATH * 2, L"PHP %d.%d.%d %s (%s)", p->major, p->minor, p->patch,
                     p->lsapi ? L"LSAPI" : L"FastCGI", p->dir);
            notify(L"OpenLiteSpeed", buf);
        }
    }
    else if (id == ID_NEWSITE)
        new_site();
    else if (id == ID_HTTPS)
        toggle_https();
    else if (id == ID_DB_INSTALL)
        db_install();
    else if (id == ID_DB_START || id == ID_DB_STOP)
    {
        if (run_tool(id == ID_DB_START ? L"--db start" : L"--db stop", 1) != 0)
            notify(L"OpenLiteSpeed", id == ID_DB_START ? L"MariaDB did not start. See logs\\mariadb.err."
                                                       : L"MariaDB did not stop.");
    }
    else if (id == ID_DB_PMA)
        open_pma();
    else if (id == ID_DB_COPY)
        db_copy_password();
    else if (id == ID_REFRESH)
        refresh_sites(0);
    else if (id == ID_WWW)
    {
        www_dir(buf, MAX_PATH);
        CreateDirectoryW(buf, NULL);
        ShellExecuteW(NULL, L"open", buf, NULL, NULL, SW_SHOWNORMAL);
    }
    else if (id == ID_SETWWW)
        choose_www();
    else if (id == ID_IMPORT)
        import_sites(0);
    else if (id == ID_IMPORT_NGINX)
        import_sites(1);
    else if (id == ID_ERRLOG || id == ID_ACCLOG)
    {
        swprintf(buf, MAX_PATH, L"%s\\logs\\%s", g_root, id == ID_ERRLOG ? L"error.log" : L"access.log");
        ShellExecuteW(NULL, L"open", L"notepad.exe", buf, NULL, SW_SHOWNORMAL);
    }
    else if (id == ID_FOLDER)
        ShellExecuteW(NULL, L"open", g_root, NULL, NULL, SW_SHOWNORMAL);
    else if (id == ID_AUTORUN)
        toggle_autorun();
    else if (id == ID_UPDATE && g_latest[0])
    {
        swprintf(buf, MAX_PATH, L"%s%s", RELEASES_URL, g_latest);
        open_url(buf);
    }
    else if (id == ID_CHECK)
        check_updates(1);
    else if (id == ID_EXIT)
        DestroyWindow(g_wnd);
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    static UINT taskbar_created;
    switch (msg)
    {
    case WM_CREATE:
        taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
        return 0;
    case WM_TRAY:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
            show_menu();
        else if (LOWORD(lp) == WM_LBUTTONDBLCLK)
            on_command(ID_SITE);
        else if (LOWORD(lp) == NIN_BALLOONUSERCLICK && g_update_balloon)
            on_command(ID_UPDATE);
        return 0;
    case WM_COMMAND:
        on_command(LOWORD(wp));
        return 0;
    case WM_UPDATE:
        on_update((int)wp, (wchar_t *)lp);
        return 0;
    case WM_NEWSITE:
        newsite_done((newsite_t *)lp);
        return 0;
    case WM_DBDONE:
        db_done((DWORD)wp, (char *)lp);
        return 0;
    case WM_TIMER:
        if (wp == TIMER_UPDATE)
        {
            KillTimer(h, TIMER_UPDATE);
            check_updates(0);
        }
        else
            update_status();
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    default:
        if (msg == taskbar_created && taskbar_created)
        {
            Shell_NotifyIconW(NIM_ADD, &g_nid);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show)
{
    WNDCLASSW wc;
    MSG msg;
    wchar_t *slash;
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\OpenLiteSpeedTray");
    (void)prev; (void)show;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
    g_inst = inst;
    GetModuleFileNameW(NULL, g_root, MAX_PATH);
    if ((slash = wcsrchr(g_root, L'\\')) != NULL)
        *slash = 0;
    swprintf(g_exe, MAX_PATH, L"%s\\openlitespeed.exe", g_root);
    if ((slash = wcsrchr(g_root, L'\\')) != NULL)
        *slash = 0;
    swprintf(g_ini, MAX_PATH, L"%s\\conf\\olstray.ini", g_root);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_icon_on = make_icon(RGB(0, 150, 90));
    g_icon_off = make_icon(RGB(120, 120, 120));
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"OpenLiteSpeedTray";
    RegisterClassW(&wc);
    g_wnd = CreateWindowW(wc.lpszClassName, L"OpenLiteSpeed", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_wnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_icon_off;
    wcscpy(g_nid.szTip, L"OpenLiteSpeed");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    update_status();
    if (((cmdline && wcsstr(cmdline, L"--start")) || GetPrivateProfileIntW(L"tray", L"autostart", 1, g_ini)) && !g_running)
        on_command(ID_START);
    if (db_installed() && !db_running())
        run_tool(L"--db start", 0);
    SetTimer(g_wnd, TIMER_STATUS, 2000, NULL);
    schedule_update();
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (mutex)
        CloseHandle(mutex);
    return 0;
}
