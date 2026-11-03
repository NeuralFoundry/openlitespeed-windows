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
#include <winsvc.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <wrl.h>
#include <WebView2.h>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

#ifndef OLS_RELEASE
#define OLS_RELEASE "0"
#endif
#define WM_REPLY (WM_APP + 1)
#define PANEL_CLASS L"OpenLiteSpeedPanel"
#define RUNTIME_URL L"https://go.microsoft.com/fwlink/p/?LinkId=2124703"
#define MAX_PHP 64

extern "C" int panel_php_list(const wchar_t *root, const wchar_t *ini, const wchar_t *current,
                              wchar_t (*labels)[MAX_PATH + 96], wchar_t (*dirs)[MAX_PATH], int max);

static HINSTANCE g_inst;
static HWND g_wnd;
static wchar_t g_root[MAX_PATH], g_exe[MAX_PATH], g_ini[MAX_PATH];
static ComPtr<ICoreWebView2Controller> g_ctrl;
static ComPtr<ICoreWebView2> g_view;
static bool g_loaded, g_devtools;
static std::mutex g_lock, g_phplock;
static std::vector<std::wstring> g_php;

static std::string utf8(const std::wstring &w)
{
    if (w.empty())
        return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

static std::wstring wide(const std::string &s)
{
    if (s.empty())
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string jesc(const std::string &s)
{
    std::string o = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
        {
            o += '\\';
            o += (char)c;
        }
        else if (c < 0x20)
        {
            char b[8];
            snprintf(b, sizeof(b), "\\u%04x", c);
            o += b;
        }
        else
            o += (char)c;
    }
    return o + "\"";
}

struct Msg
{
    std::map<std::string, std::string> str;
    std::map<std::string, double> num;
    std::map<std::string, bool> flag;
    std::string s(const char *k) const
    {
        auto i = str.find(k);
        return i == str.end() ? std::string() : i->second;
    }
    long long n(const char *k, long long def = -1) const
    {
        auto i = num.find(k);
        return i == num.end() ? def : (long long)i->second;
    }
    bool b(const char *k) const
    {
        auto i = flag.find(k);
        return i != flag.end() && i->second;
    }
};

static void skip_ws(const std::string &s, size_t &i)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
        ++i;
}

static bool parse_string(const std::string &s, size_t &i, std::string &out)
{
    if (i >= s.size() || s[i] != '"')
        return false;
    for (++i; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '"')
        {
            ++i;
            return true;
        }
        if (c != '\\')
        {
            out += c;
            continue;
        }
        if (++i >= s.size())
            return false;
        switch (s[i])
        {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'u':
        {
            if (i + 4 >= s.size())
                return false;
            unsigned v = (unsigned)strtoul(s.substr(i + 1, 4).c_str(), NULL, 16);
            i += 4;
            wchar_t w[2] = { (wchar_t)v, 0 };
            out += utf8(w);
            break;
        }
        default: return false;
        }
    }
    return false;
}

static bool parse(const std::string &s, Msg &m)
{
    size_t i = 0;
    skip_ws(s, i);
    if (i >= s.size() || s[i++] != '{')
        return false;
    for (;;)
    {
        std::string key;
        skip_ws(s, i);
        if (i < s.size() && s[i] == '}')
            return true;
        if (!parse_string(s, i, key))
            return false;
        skip_ws(s, i);
        if (i >= s.size() || s[i++] != ':')
            return false;
        skip_ws(s, i);
        if (i >= s.size())
            return false;
        if (s[i] == '"')
        {
            std::string v;
            if (!parse_string(s, i, v))
                return false;
            m.str[key] = v;
        }
        else if (s.compare(i, 4, "true") == 0)
        {
            m.flag[key] = true;
            i += 4;
        }
        else if (s.compare(i, 5, "false") == 0)
        {
            m.flag[key] = false;
            i += 5;
        }
        else if (s.compare(i, 4, "null") == 0)
            i += 4;
        else
        {
            char *end = NULL;
            double v = strtod(s.c_str() + i, &end);
            if (end == s.c_str() + i)
                return false;
            m.num[key] = v;
            i = (size_t)(end - s.c_str());
        }
        skip_ws(s, i);
        if (i < s.size() && s[i] == ',')
        {
            ++i;
            continue;
        }
        return i < s.size() && s[i] == '}';
    }
}

static DWORD run(const std::wstring &args, std::string *out, const std::string *input)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    HANDLE ord = NULL, owr = NULL, ird = NULL, iwr = NULL;
    std::wstring cmd = L"\"" + std::wstring(g_exe) + L"\" " + args;
    DWORD code = 1, got;
    char buf[4096];
    if (!CreatePipe(&ord, &owr, &sa, 0))
        return 1;
    SetHandleInformation(ord, HANDLE_FLAG_INHERIT, 0);
    if (input && !CreatePipe(&ird, &iwr, &sa, 0))
    {
        CloseHandle(ord);
        CloseHandle(owr);
        return 1;
    }
    if (iwr)
        SetHandleInformation(iwr, HANDLE_FLAG_INHERIT, 0);
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = owr;
    si.hStdError = owr;
    si.hStdInput = ird ? ird : GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessW(g_exe, &cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, g_root, &si, &pi))
    {
        CloseHandle(ord);
        CloseHandle(owr);
        if (ird)
            CloseHandle(ird);
        if (iwr)
            CloseHandle(iwr);
        return 1;
    }
    CloseHandle(owr);
    if (ird)
        CloseHandle(ird);
    if (iwr)
    {
        DWORD w;
        WriteFile(iwr, input->data(), (DWORD)input->size(), &w, NULL);
        CloseHandle(iwr);
    }
    while (ReadFile(ord, buf, sizeof(buf), &got, NULL) && got)
        if (out)
            out->append(buf, got);
    CloseHandle(ord);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

static DWORD run_elevated(const std::wstring &args)
{
    SHELLEXECUTEINFOW sei = {};
    DWORD code = 1;
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = g_wnd;
    sei.lpVerb = L"runas";
    sei.lpFile = g_exe;
    sei.lpParameters = args.c_str();
    sei.lpDirectory = g_root;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
        return 1;
    WaitForSingleObject(sei.hProcess, INFINITE);
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return code;
}

static std::string last_line(const std::string &out, const char *fallback)
{
    size_t end = out.size();
    while (end > 0)
    {
        size_t start = out.rfind('\n', end - 1);
        start = start == std::string::npos ? 0 : start + 1;
        std::string line = out.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty() && line.compare(0, 7, "http://") != 0 && line.find("site(s)") == std::string::npos
            && line.find("Administrator rights") == std::string::npos)
            return line;
        if (start == 0)
            break;
        end = start - 1;
    }
    return fallback;
}

static std::string read_shared(const std::wstring &path, size_t tail)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    std::string s;
    LARGE_INTEGER size;
    DWORD got;
    if (h == INVALID_HANDLE_VALUE)
        return s;
    if (GetFileSizeEx(h, &size))
    {
        long long start = tail && size.QuadPart > (long long)tail ? size.QuadPart - (long long)tail : 0;
        LARGE_INTEGER pos;
        pos.QuadPart = start;
        SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
        s.resize((size_t)(size.QuadPart - start));
        if (!s.empty() && (!ReadFile(h, &s[0], (DWORD)s.size(), &got, NULL) || got != s.size()))
            s.clear();
    }
    CloseHandle(h);
    return s;
}

static std::wstring ini_str(const wchar_t *sect, const wchar_t *key, const wchar_t *def)
{
    wchar_t buf[MAX_PATH];
    GetPrivateProfileStringW(sect, key, def, buf, MAX_PATH, g_ini);
    return buf;
}

static int http_port()
{
    std::string t = read_shared(std::wstring(g_root) + L"\\conf\\httpd_config.conf", 0);
    size_t p = t.find("listener Default");
    if (p == std::string::npos || (p = t.find("address", p)) == std::string::npos)
        return 80;
    size_t e = t.find_first_of("\r\n", p), c = t.rfind(':', e);
    int port = c != std::string::npos && c > p ? atoi(t.c_str() + c + 1) : 80;
    return port > 0 && port < 65536 ? port : 80;
}

static std::wstring site_url(const std::wstring &host)
{
    int https = GetPrivateProfileIntW(L"sites", L"https", 0, g_ini), port;
    wchar_t buf[300];
    if (https > 0)
        swprintf(buf, 300, https == 443 ? L"https://%s/" : L"https://%s:%d/", host.c_str(), https);
    else
    {
        port = http_port();
        swprintf(buf, 300, port == 80 ? L"http://%s/" : L"http://%s:%d/", host.c_str(), port);
    }
    return buf;
}

static bool name_ok(const std::string &n)
{
    if (n.empty() || n.size() > 63 || n.front() == '-' || n.back() == '-' || n == "localhost" || n == "phpmyadmin")
        return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    return true;
}

static bool url_ok(const std::string &u)
{
    if (u.size() > 2048 || (u.compare(0, 7, "http://") != 0 && u.compare(0, 8, "https://") != 0))
        return false;
    for (unsigned char c : u)
        if (c <= 0x20 || c >= 0x7f || strchr("\"<>\\^`{|}", c))
            return false;
    return true;
}

static bool token_ok(const std::string &t)
{
    if (t.size() < 32 || t.size() > 4096)
        return false;
    for (char c : t)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '/'
              || c == '=' || c == '-' || c == '_'))
            return false;
    return true;
}

static bool is_running()
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT), svc;
    if (scm)
    {
        if ((svc = OpenServiceW(scm, L"OpenLiteSpeed", SERVICE_QUERY_STATUS)) != NULL)
        {
            SERVICE_STATUS st;
            bool r = QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_RUNNING;
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return r;
        }
        CloseServiceHandle(scm);
    }
    std::string t = read_shared(std::wstring(g_root) + L"\\tmp\\lshttpd\\lshttpd.pid", 0);
    DWORD pid = (DWORD)atoi(t.c_str()), code = 0;
    HANDLE h = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : NULL;
    if (!h)
        return false;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return code == STILL_ACTIVE;
}

static bool service_installed()
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

static DWORD hosts_update()
{
    DWORD r = run(L"--dev-hosts", NULL, NULL);
    if (r == 5)
        r = run_elevated(L"--dev-hosts");
    return r;
}

static void reload_if_running()
{
    if (is_running())
        run(L"-s reload", NULL, NULL);
}

static void clip_flag(UINT fmt)
{
    HGLOBAL z = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    if (!z || !fmt)
        return;
    if (DWORD *d = (DWORD *)GlobalLock(z))
    {
        *d = 0;
        GlobalUnlock(z);
        if (SetClipboardData(fmt, z))
            return;
    }
    GlobalFree(z);
}

static bool clip_set(const std::wstring &text, bool secret)
{
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool ok = false;
    if (!g)
        return false;
    if (void *p = GlobalLock(g))
    {
        memcpy(p, text.c_str(), bytes);
        GlobalUnlock(g);
    }
    for (int i = 0; i < 10 && !OpenClipboard(NULL); ++i)
        Sleep(50);
    if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, g))
        GlobalFree(g);
    else
        ok = true;
    if (secret)
    {
        clip_flag(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"));
        clip_flag(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"));
        clip_flag(RegisterClipboardFormatW(L"CanUploadToCloudClipboard"));
    }
    CloseClipboard();
    return ok;
}

static std::wstring current_php()
{
    std::string t = read_shared(std::wstring(g_root) + L"\\conf\\httpd_config.conf", 0);
    size_t p = t.find("# php begin");
    if (p == std::string::npos || (p = t.find("PHPRC=", p)) == std::string::npos)
        return std::wstring();
    p += 6;
    size_t e = t.find_first_of("\r\n\"", p);
    std::wstring w = wide(t.substr(p, e == std::string::npos ? std::string::npos : e - p));
    for (auto &c : w)
        if (c == L'/')
            c = L'\\';
    return w;
}

struct Result
{
    bool ok = true;
    std::string data = "null";
    std::string error;
};

static Result fail(const std::string &e)
{
    Result r;
    r.ok = false;
    r.error = e;
    return r;
}

static Result ran(DWORD code, const std::string &out, const char *fallback)
{
    if (code == 0)
        return Result();
    return fail(last_line(out, fallback));
}

static Result handle(const Msg &m)
{
    std::string a = m.s("action"), out;
    DWORD code;
    if (a == "status")
    {
        if ((code = run(L"--status", &out, NULL)) != 0)
            return fail(last_line(out, "The status could not be read."));
        size_t b = out.find('{');
        if (b == std::string::npos)
            return fail("The status could not be read.");
        out.insert(b + 1, "\"version\":" + jesc(OLS_RELEASE) + ",");
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        Result r;
        r.data = out;
        return r;
    }
    if (a == "php.list")
    {
        static wchar_t labels[MAX_PHP][MAX_PATH + 96], dirs[MAX_PHP][MAX_PATH];
        std::lock_guard<std::mutex> g(g_phplock);
        std::wstring cur = current_php();
        int n = panel_php_list(g_root, g_ini, cur.c_str(), labels, dirs, MAX_PHP);
        std::string j = "[";
        g_php.clear();
        for (int i = 0; i < n; ++i)
        {
            g_php.push_back(dirs[i]);
            j += (i ? "," : "") + std::string("{\"label\":") + jesc(utf8(labels[i])) + ",\"dir\":" + jesc(utf8(dirs[i]))
                 + ",\"current\":"
                 + (_wcsicmp(dirs[i], cur.c_str()) == 0 ? "true" : "false") + "}";
        }
        Result r;
        r.data = j + "]";
        return r;
    }
    if (a == "log")
    {
        Result r;
        r.data = jesc(read_shared(std::wstring(g_root) + L"\\logs\\error.log", 65536));
        return r;
    }
    if (a == "open")
    {
        std::string u = m.s("url");
        if (!url_ok(u))
            return fail("That address cannot be opened.");
        ShellExecuteW(NULL, L"open", wide(u).c_str(), NULL, NULL, SW_SHOWNORMAL);
        return Result();
    }
    if (a == "folder")
    {
        std::wstring www = ini_str(L"sites", L"www", (std::wstring(g_root) + L"\\www").c_str());
        CreateDirectoryW(www.c_str(), NULL);
        ShellExecuteW(NULL, L"explore", www.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return Result();
    }
    if (a == "copy")
    {
        std::string t = m.s("text");
        if (!url_ok(t) || !clip_set(wide(t), false))
            return fail("The link could not be copied.");
        return Result();
    }
    if (a == "pma.open")
    {
        ShellExecuteW(NULL, L"open", site_url(L"phpmyadmin." + ini_str(L"sites", L"tld", L"test")).c_str(), NULL, NULL,
                      SW_SHOWNORMAL);
        return Result();
    }
    if (a == "db.copypw")
    {
        std::string t = read_shared(std::wstring(g_root) + L"\\data\\client.cnf", 0);
        size_t p = t.find("\npassword=");
        bool ok = false;
        if (p != std::string::npos)
        {
            p += 10;
            std::wstring pw = wide(t.substr(p, t.find_first_of("\r\n", p) - p));
            ok = !pw.empty() && clip_set(pw, true);
            SecureZeroMemory(&pw[0], pw.size() * sizeof(wchar_t));
        }
        SecureZeroMemory(&t[0], t.size());
        return ok ? Result() : fail("The database password was not found.");
    }

    std::lock_guard<std::mutex> g(g_lock);
    if (a == "server.start" || a == "server.stop" || a == "server.reload")
    {
        std::wstring args = a == "server.start" ? L"-s start" : a == "server.stop" ? L"-s stop" : L"-s reload";
        code = run(args, &out, NULL);
        if (code != 0 && service_installed())
            code = run_elevated(args);
        return ran(code, out, "The web server did not respond.");
    }
    if (a == "db.install")
    {
        code = run(L"--db install", &out, NULL);
        if (code == 0)
        {
            hosts_update();
            reload_if_running();
        }
        return ran(code, out, "The database could not be installed.");
    }
    if (a == "db.start" || a == "db.stop")
        return ran(run(a == "db.start" ? L"--db start" : L"--db stop", &out, NULL), out, "MariaDB did not respond.");
    if (a == "site.new")
    {
        std::string n = m.s("name");
        if (!name_ok(n))
            return fail("Use lowercase letters, digits and '-' for the site name.");
        code = run(L"--new-site " + wide(n) + (m.b("wp") ? L" --wordpress" : L""), &out, NULL);
        if (code != 0)
            return fail(last_line(out, "The site could not be created."));
        hosts_update();
        reload_if_running();
        ShellExecuteW(NULL, L"open", site_url(wide(n) + L"." + ini_str(L"sites", L"tld", L"test")).c_str(), NULL, NULL,
                      SW_SHOWNORMAL);
        return Result();
    }
    if (a == "proxy.new" || a == "proxy.remove")
    {
        std::string n = m.s("name");
        long long port = m.n("port");
        if (!name_ok(n) || (a == "proxy.new" && (port <= 0 || port > 65535)))
            return fail("Check the site name and the port.");
        std::wstring args = a == "proxy.new" ? L"--proxy add " + wide(n) + L" " + std::to_wstring(port)
                                             : L"--proxy remove " + wide(n);
        code = run(args, &out, NULL);
        if (code == 0)
        {
            hosts_update();
            reload_if_running();
        }
        return ran(code, out, "The proxy site could not be changed.");
    }
    if (a == "https")
    {
        std::wstring args = m.b("on") ? L"--dev-https on" : L"--dev-https off";
        AllowSetForegroundWindow(ASFW_ANY);
        code = run(args, &out, NULL);
        if (code == 5)
            code = run_elevated(args);
        if (code == 3)
            return fail("The local certificate authority was not trusted, so trusted HTTPS stays off.");
        if (code == 0)
            reload_if_running();
        return ran(code, out, "Trusted HTTPS could not be changed.");
    }
    if (a == "tunnel.start")
    {
        std::string n = m.s("name");
        if (!name_ok(n))
            return fail("This site cannot be shared.");
        code = run(L"--tunnel start " + wide(n), &out, NULL);
        size_t p = out.find("URL https://");
        if (code != 0 || p == std::string::npos)
            return fail(last_line(out, "Cloudflare did not give the site an address."));
        std::string url = out.substr(p + 4, out.find_first_of("\r\n", p) - p - 4);
        if (!url_ok(url))
            return fail("Cloudflare returned an unexpected address.");
        Result r;
        r.data = jesc(url);
        return r;
    }
    if (a == "tunnel.stop")
    {
        std::string n = m.s("name");
        if (!name_ok(n))
            return fail("This site cannot be shared.");
        return ran(run(L"--tunnel stop " + wide(n), &out, NULL), out, "The tunnel did not stop.");
    }
    if (a == "tunnel.token")
    {
        std::string t = m.s("token");
        if (!token_ok(t))
            return fail("That does not look like a Cloudflare tunnel token.");
        std::string in = t + "\n";
        code = run(L"--tunnel token", &out, &in);
        SecureZeroMemory(&in[0], in.size());
        SecureZeroMemory(&t[0], t.size());
        return ran(code, out, "The token could not be saved.");
    }
    if (a == "tunnel.run")
        return ran(run(L"--tunnel run", &out, NULL), out, "The tunnel did not start.");
    if (a == "tunnel.runstop")
        return ran(run(L"--tunnel run-stop", &out, NULL), out, "The tunnel did not stop.");
    if (a == "tunnel.clear")
        return ran(run(L"--tunnel token clear", &out, NULL), out, "The token could not be removed.");
    if (a == "php.set")
    {
        std::wstring dir;
        {
            std::lock_guard<std::mutex> pg(g_phplock);
            long long i = m.n("index");
            if (i < 0 || i >= (long long)g_php.size())
                return fail("Pick a PHP version from the list.");
            dir = g_php[(size_t)i];
        }
        if (dir.find(L'"') != std::wstring::npos)
            return fail("That PHP folder cannot be used.");
        code = run(L"--set-php \"" + dir + L"\"", &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, "PHP could not be switched.");
    }
    return fail("Unknown action.");
}

static void post_reply(const std::string &json)
{
    std::wstring *w = new std::wstring(wide(json));
    if (!PostMessageW(g_wnd, WM_REPLY, 0, (LPARAM)w))
        delete w;
}

static void dispatch(const std::string &json)
{
    Msg m;
    if (json.size() > 16384 || !parse(json, m) || m.n("id") < 0)
        return;
    std::thread([m]() {
        long long id = m.n("id");
        Result r;
        try
        {
            r = handle(m);
        }
        catch (...)
        {
            r = fail("Something went wrong.");
        }
        std::string reply = "{\"id\":" + std::to_string(id) + ",\"ok\":" + (r.ok ? "true" : "false");
        reply += r.ok ? ",\"data\":" + r.data + "}" : ",\"error\":" + jesc(r.error) + "}";
        post_reply(reply);
    }).detach();
}

static std::wstring load_html()
{
    HRSRC res = FindResourceW(g_inst, MAKEINTRESOURCEW(2), MAKEINTRESOURCEW(10));
    HGLOBAL h = res ? LoadResource(g_inst, res) : NULL;
    const char *p = h ? (const char *)LockResource(h) : NULL;
    DWORD n = res ? SizeofResource(g_inst, res) : 0;
    return p && n ? wide(std::string(p, n)) : std::wstring(L"<p>The panel is damaged. Reinstall OpenLiteSpeed.</p>");
}

static bool dark_theme()
{
    DWORD v = 1, n = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &n);
    return v == 0;
}

static void no_runtime()
{
    if (MessageBoxW(g_wnd,
                    L"The panel needs the Microsoft Edge WebView2 Runtime, which is not installed.\n\nOpen the download page?",
                    L"OpenLiteSpeed", MB_YESNO | MB_ICONWARNING) == IDYES)
        ShellExecuteW(NULL, L"open", RUNTIME_URL, NULL, NULL, SW_SHOWNORMAL);
    DestroyWindow(g_wnd);
}

static void start_webview(HWND hwnd)
{
    wchar_t data[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", data, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 40)
    {
        no_runtime();
        return;
    }
    wcscat(data, L"\\OpenLiteSpeed\\Panel");
    SHCreateDirectoryExW(NULL, data, NULL);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, data, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT r, ICoreWebView2Environment *env) -> HRESULT {
                if (FAILED(r) || !env)
                {
                    no_runtime();
                    return S_OK;
                }
                env->CreateCoreWebView2Controller(
                    hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                              [hwnd](HRESULT r2, ICoreWebView2Controller *c) -> HRESULT {
                                  EventRegistrationToken tok;
                                  ComPtr<ICoreWebView2Settings> s;
                                  ComPtr<ICoreWebView2Settings3> s3;
                                  ComPtr<ICoreWebView2Controller2> c2;
                                  RECT rc;
                                  if (FAILED(r2) || !c)
                                  {
                                      no_runtime();
                                      return S_OK;
                                  }
                                  g_ctrl = c;
                                  g_ctrl->get_CoreWebView2(&g_view);
                                  if (SUCCEEDED(g_ctrl.As(&c2)))
                                  {
                                      COREWEBVIEW2_COLOR bg = dark_theme() ? COREWEBVIEW2_COLOR{ 255, 0x10, 0x16, 0x1c }
                                                                           : COREWEBVIEW2_COLOR{ 255, 0xe7, 0xeb, 0xef };
                                      c2->put_DefaultBackgroundColor(bg);
                                  }
                                  g_view->get_Settings(&s);
                                  s->put_AreDevToolsEnabled(g_devtools);
                                  s->put_IsStatusBarEnabled(FALSE);
                                  s->put_AreHostObjectsAllowed(FALSE);
                                  s->put_IsWebMessageEnabled(TRUE);
                                  s->put_IsScriptEnabled(TRUE);
                                  s->put_AreDefaultScriptDialogsEnabled(TRUE);
                                  if (SUCCEEDED(s.As(&s3)))
                                      s3->put_AreBrowserAcceleratorKeysEnabled(g_devtools);
                                  GetClientRect(hwnd, &rc);
                                  g_ctrl->put_Bounds(rc);
                                  g_view->add_NavigationStarting(
                                      Callback<ICoreWebView2NavigationStartingEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *e) -> HRESULT {
                                              LPWSTR uri = nullptr;
                                              bool own = !g_loaded && SUCCEEDED(e->get_Uri(&uri)) && uri
                                                         && wcsncmp(uri, L"data:text/html", 14) == 0;
                                              if (uri)
                                                  CoTaskMemFree(uri);
                                              if (!own)
                                                  e->put_Cancel(TRUE);
                                              return S_OK;
                                          })
                                          .Get(),
                                      &tok);
                                  g_view->add_FrameNavigationStarting(
                                      Callback<ICoreWebView2NavigationStartingEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *e) -> HRESULT {
                                              e->put_Cancel(TRUE);
                                              return S_OK;
                                          })
                                          .Get(),
                                      &tok);
                                  g_view->add_NavigationCompleted(
                                      Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *) -> HRESULT {
                                              g_loaded = true;
                                              return S_OK;
                                          })
                                          .Get(),
                                      &tok);
                                  g_view->add_NewWindowRequested(
                                      Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *e) -> HRESULT {
                                              e->put_Handled(TRUE);
                                              return S_OK;
                                          })
                                          .Get(),
                                      &tok);
                                  g_view->add_WebMessageReceived(
                                      Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *e) -> HRESULT {
                                              LPWSTR src = nullptr, msg = nullptr;
                                              bool same = SUCCEEDED(e->get_Source(&src)) && src && wcscmp(src, L"about:blank") == 0;
                                              if (src)
                                                  CoTaskMemFree(src);
                                              if (same && SUCCEEDED(e->TryGetWebMessageAsString(&msg)) && msg)
                                                  dispatch(utf8(msg));
                                              if (msg)
                                                  CoTaskMemFree(msg);
                                              return S_OK;
                                          })
                                          .Get(),
                                      &tok);
                                  g_view->add_ProcessFailed(
                                      Callback<ICoreWebView2ProcessFailedEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2ProcessFailedEventArgs *) -> HRESULT {
                                              MessageBoxW(g_wnd, L"The panel stopped unexpectedly. Open it again from the tray.",
                                                          L"OpenLiteSpeed", MB_OK | MB_ICONWARNING);
                                              DestroyWindow(g_wnd);
                                              return S_OK;
                                          })
                                          .Get(),
                                      &tok);
                                  g_view->NavigateToString(load_html().c_str());
                                  return S_OK;
                              })
                              .Get());
                return S_OK;
            })
            .Get());
    if (FAILED(hr))
        no_runtime();
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZE:
        if (g_ctrl)
        {
            RECT rc;
            GetClientRect(h, &rc);
            g_ctrl->put_Bounds(rc);
        }
        return 0;
    case WM_GETMINMAXINFO:
    {
        UINT dpi = GetDpiForWindow(h);
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = MulDiv(960, dpi ? dpi : 96, 96);
        mm->ptMinTrackSize.y = MulDiv(620, dpi ? dpi : 96, 96);
        return 0;
    }
    case WM_DPICHANGED:
    {
        RECT *r = (RECT *)lp;
        SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_REPLY:
    {
        std::wstring *w = (std::wstring *)lp;
        if (g_view && w)
            g_view->PostWebMessageAsJson(w->c_str());
        delete w;
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE && g_view && g_loaded)
            g_view->PostWebMessageAsJson(L"{\"event\":\"refresh\"}");
        break;
    case WM_DESTROY:
        g_ctrl.Reset();
        g_view.Reset();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show)
{
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\OpenLiteSpeedPanel");
    WNDCLASSW wc = {};
    MSG msg;
    wchar_t *slash;
    UINT dpi = GetDpiForSystem();
    BOOL dark;
    (void)prev;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND w = FindWindowW(PANEL_CLASS, NULL);
        if (w)
        {
            ShowWindow(w, IsIconic(w) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(w);
        }
        return 0;
    }
    g_inst = inst;
    g_devtools = cmdline && wcsstr(cmdline, L"--devtools") != NULL;
    GetModuleFileNameW(NULL, g_root, MAX_PATH);
    if ((slash = wcsrchr(g_root, L'\\')) != NULL)
        *slash = 0;
    swprintf(g_exe, MAX_PATH, L"%s\\openlitespeed.exe", g_root);
    if ((slash = wcsrchr(g_root, L'\\')) != NULL)
        *slash = 0;
    swprintf(g_ini, MAX_PATH, L"%s\\conf\\olstray.ini", g_root);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    dark = dark_theme();
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = PANEL_CLASS;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(dark ? RGB(0x10, 0x16, 0x1c) : RGB(0xe7, 0xeb, 0xef));
    RegisterClassW(&wc);
    g_wnd = CreateWindowExW(0, PANEL_CLASS, L"OpenLiteSpeed", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            MulDiv(1180, dpi, 96), MulDiv(760, dpi, 96), NULL, NULL, inst, NULL);
    if (!g_wnd)
        return 1;
    DwmSetWindowAttribute(g_wnd, 20, &dark, sizeof(dark));
    ShowWindow(g_wnd, show);
    start_webview(g_wnd);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (mutex)
        CloseHandle(mutex);
    return 0;
}
