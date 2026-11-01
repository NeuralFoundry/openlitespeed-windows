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
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "phpdetect.h"

#define WM_TRAY (WM_APP + 1)
#define TIMER_STATUS 1
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

static void import_apache(void)
{
    wchar_t conf[MAX_PATH] = L"", args[MAX_PATH + 64], log[MAX_PATH], *msg;
    char *out = NULL;
    DWORD code = run_capture(L"--import-apache --dry-run", &out);
    HANDLE h;
    if (code == 2)
    {
        OPENFILENAMEW ofn;
        free(out);
        out = NULL;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_wnd;
        ofn.lpstrFilter = L"Apache configuration\0httpd.conf;*.conf\0All files\0*.*\0";
        ofn.lpstrFile = conf;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Select httpd.conf of Apache";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn))
            return;
        swprintf(args, MAX_PATH + 64, L"--import-apache \"%s\" --dry-run", conf);
        code = run_capture(args, &out);
    }
    if (!out)
        return;
    if (code != 0 || strstr(out, "\n0 site(s) would be imported"))
    {
        msg = report_text(out, L"");
        if (msg)
            MessageBoxW(g_wnd, msg, L"Import Apache sites", MB_OK | (code ? MB_ICONWARNING : MB_ICONINFORMATION));
        free(msg);
        free(out);
        return;
    }
    msg = report_text(out, L"\n\nImport these sites into OpenLiteSpeed?");
    free(out);
    if (!msg || MessageBoxW(g_wnd, msg, L"Import Apache sites", MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        free(msg);
        return;
    }
    free(msg);
    if (conf[0])
        swprintf(args, MAX_PATH + 64, L"--import-apache \"%s\"", conf);
    else
        wcscpy(args, L"--import-apache");
    code = run_capture(args, &out);
    swprintf(log, MAX_PATH, L"%s\\logs\\apache-import.log", g_root);
    h = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD n;
        WriteFile(h, out, (DWORD)strlen(out), &n, NULL);
        CloseHandle(h);
    }
    if (code == 0)
    {
        if (g_running)
            run_tool(L"-s reload", 1);
        notify(L"Apache sites imported", L"The report is in logs\\apache-import.log.");
    }
    else if ((msg = report_text(out, L"")) != NULL)
    {
        MessageBoxW(g_wnd, msg, L"Import Apache sites", MB_OK | MB_ICONWARNING);
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

static void show_menu(void)
{
    HMENU m = CreatePopupMenu(), php = CreatePopupMenu(), sites = CreatePopupMenu(), logs = CreatePopupMenu();
    wchar_t cur[MAX_PATH], label[MAX_PATH + 32];
    POINT pt;
    int i, cmd;
    update_status();
    find_php();
    load_sites();
    current_php(cur, MAX_PATH);
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, g_running ? L"OpenLiteSpeed is running" : L"OpenLiteSpeed is stopped");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (g_running ? MF_GRAYED : 0), ID_START, L"Start");
    AppendMenuW(m, MF_STRING | (g_running ? 0 : MF_GRAYED), ID_STOP, L"Stop");
    AppendMenuW(m, MF_STRING | (g_running ? 0 : MF_GRAYED), ID_RELOAD, L"Reload");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_SITE, L"Open website");
    AppendMenuW(m, MF_STRING, ID_ADMIN, L"Open WebAdmin");
    for (i = 0; i < g_nsites; ++i)
        AppendMenuW(sites, MF_STRING, ID_SITE_BASE + i, g_sites[i]);
    if (g_nsites)
        AppendMenuW(sites, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sites, MF_STRING, ID_REFRESH, L"Refresh sites");
    AppendMenuW(sites, MF_STRING, ID_WWW, L"Open projects folder");
    AppendMenuW(sites, MF_STRING, ID_SETWWW, L"Change projects folder...");
    AppendMenuW(sites, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sites, MF_STRING, ID_IMPORT, L"Import Apache sites...");
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
    AppendMenuW(logs, MF_STRING, ID_ERRLOG, L"Error log");
    AppendMenuW(logs, MF_STRING, ID_ACCLOG, L"Access log");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)logs, L"Logs");
    AppendMenuW(m, MF_STRING, ID_FOLDER, L"Open install folder");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (autorun_enabled() ? MF_CHECKED : 0), ID_AUTORUN, L"Start with Windows");
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
        int port = http_port();
        if (port == 80)
            swprintf(buf, MAX_PATH, L"http://%s/", g_sites[id - ID_SITE_BASE]);
        else
            swprintf(buf, MAX_PATH, L"http://%s:%d/", g_sites[id - ID_SITE_BASE], port);
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
        import_apache();
    else if (id == ID_ERRLOG || id == ID_ACCLOG)
    {
        swprintf(buf, MAX_PATH, L"%s\\logs\\%s", g_root, id == ID_ERRLOG ? L"error.log" : L"access.log");
        ShellExecuteW(NULL, L"open", L"notepad.exe", buf, NULL, SW_SHOWNORMAL);
    }
    else if (id == ID_FOLDER)
        ShellExecuteW(NULL, L"open", g_root, NULL, NULL, SW_SHOWNORMAL);
    else if (id == ID_AUTORUN)
        toggle_autorun();
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
        return 0;
    case WM_COMMAND:
        on_command(LOWORD(wp));
        return 0;
    case WM_TIMER:
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
    SetTimer(g_wnd, TIMER_STATUS, 2000, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (mutex)
        CloseHandle(mutex);
    return 0;
}
