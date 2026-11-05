#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <winsock2.h>
#include <windows.h>
#include <winsvc.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
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
extern "C" int panel_php_info(int i, int *major, int *minor, int *patch, int *ts, int *lsapi, wchar_t *source,
                              size_t n);

static HINSTANCE g_inst;
static HWND g_wnd;
static wchar_t g_root[MAX_PATH], g_exe[MAX_PATH], g_ini[MAX_PATH];
static ComPtr<ICoreWebView2Controller> g_ctrl;
static ComPtr<ICoreWebView2> g_view;
static bool g_loaded, g_devtools;
static std::mutex g_lock, g_phplock, g_dllock;
static std::vector<std::wstring> g_php, g_node;

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

static bool parse_at(const std::string &s, size_t &i, Msg &m)
{
    skip_ws(s, i);
    if (i >= s.size() || s[i++] != '{')
        return false;
    for (;;)
    {
        std::string key;
        skip_ws(s, i);
        if (i < s.size() && s[i] == '}')
        {
            ++i;
            return true;
        }
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
        if (i >= s.size() || s[i] != '}')
            return false;
        ++i;
        return true;
    }
}

static bool parse(const std::string &s, Msg &m)
{
    size_t i = 0;
    return parse_at(s, i, m);
}

static std::vector<Msg> parse_list(const std::string &s)
{
    std::vector<Msg> v;
    size_t i = 0;
    skip_ws(s, i);
    if (i >= s.size() || s[i++] != '[')
        return v;
    for (;;)
    {
        Msg m;
        skip_ws(s, i);
        if (i >= s.size() || s[i] == ']' || !parse_at(s, i, m))
            return v;
        v.push_back(m);
        skip_ws(s, i);
        if (i >= s.size() || s[i++] != ',')
            return v;
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

static bool port_open(int port)
{
    static bool ready = false;
    WSADATA wd;
    sockaddr_in a = {};
    SOCKET s;
    bool ok;
    if (!ready)
        ready = WSAStartup(MAKEWORD(2, 2), &wd) == 0;
    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
        return false;
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ok = connect(s, (sockaddr *)&a, sizeof(a)) == 0;
    closesocket(s);
    return ok;
}

static bool wait_up(int port, int seconds)
{
    for (int i = 0; i < seconds * 4; ++i)
    {
        if (is_running() && port_open(port))
        {
            Sleep(1500);
            if (is_running() && port_open(port))
                return true;
        }
        Sleep(250);
    }
    return false;
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

static std::wstring www_dir()
{
    return ini_str(L"sites", L"www", (std::wstring(g_root) + L"\\www").c_str());
}

static bool plain_dir(const std::wstring &p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}

static bool site_dir(const std::string &name, std::wstring &dir)
{
    if (!name_ok(name))
        return false;
    dir = www_dir() + L"\\" + wide(name);
    return plain_dir(dir);
}

static bool db_ok(const std::string &n)
{
    if (n.empty() || n.size() > 64)
        return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

static bool arg_ok(const std::string &v, const char *extra)
{
    if (v.empty() || v.size() > 64)
        return false;
    for (char c : v)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr(extra, c)))
            return false;
    return true;
}

static bool quoted_ok(const std::wstring &p)
{
    return !p.empty() && p.size() < MAX_PATH && p.find(L'"') == std::wstring::npos;
}

static std::wstring q(const std::wstring &p)
{
    return L"\"" + p + (!p.empty() && p.back() == L'\\' ? L"." : L"") + L"\"";
}

static bool pick_php(const Msg &m, std::wstring &dir)
{
    std::lock_guard<std::mutex> pg(g_phplock);
    long long i = m.n("index");
    if (i < 0 || i >= (long long)g_php.size())
        return false;
    dir = g_php[(size_t)i];
    return quoted_ok(dir);
}

static bool pick_node(const Msg &m, std::wstring &exe)
{
    std::lock_guard<std::mutex> pg(g_phplock);
    long long i = m.n("index");
    if (i < 0 || i >= (long long)g_node.size())
        return false;
    exe = g_node[(size_t)i];
    return quoted_ok(exe);
}

static std::string json_part(const std::string &out)
{
    size_t b = out.find_first_of("[{");
    if (b == std::string::npos)
        return std::string();
    std::string j = out.substr(b);
    while (!j.empty() && (j.back() == '\n' || j.back() == '\r' || j.back() == ' '))
        j.pop_back();
    return j;
}

static bool pick_file(bool save, const std::wstring &name, std::wstring &out)
{
    ComPtr<IFileDialog> d;
    ComPtr<IShellItem> it;
    COMDLG_FILTERSPEC f[] = { { L"SQL files", L"*.sql" } };
    DWORD opt = 0;
    PWSTR p = nullptr;
    if (FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&d))))
        return false;
    d->SetFileTypes(1, f);
    d->SetDefaultExtension(L"sql");
    if (save)
        d->SetFileName(name.c_str());
    d->GetOptions(&opt);
    d->SetOptions(opt | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
    if (FAILED(d->Show(g_wnd)) || FAILED(d->GetResult(&it)) || FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) || !p)
        return false;
    out = p;
    CoTaskMemFree(p);
    return true;
}

static bool save_as(const std::wstring &name, std::wstring &out)
{
    ComPtr<IFileDialog> d;
    ComPtr<IShellItem> it;
    COMDLG_FILTERSPEC f[] = { { L"All files", L"*.*" } };
    DWORD opt = 0;
    PWSTR p = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d))))
        return false;
    d->SetFileTypes(1, f);
    d->SetFileName(name.c_str());
    d->GetOptions(&opt);
    d->SetOptions(opt | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_OVERWRITEPROMPT);
    if (FAILED(d->Show(g_wnd)) || FAILED(d->GetResult(&it)) || FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) || !p)
        return false;
    out = p;
    CoTaskMemFree(p);
    return true;
}

static const char s_b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string b64_encode(const std::string &in)
{
    std::string o;
    size_t i;
    o.reserve((in.size() + 2) / 3 * 4);
    for (i = 0; i + 2 < in.size(); i += 3)
    {
        unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
        o += s_b64[v >> 18];
        o += s_b64[(v >> 12) & 63];
        o += s_b64[(v >> 6) & 63];
        o += s_b64[v & 63];
    }
    if (i < in.size())
    {
        unsigned v = (unsigned char)in[i] << 16;
        if (i + 1 < in.size())
            v |= (unsigned char)in[i + 1] << 8;
        o += s_b64[v >> 18];
        o += s_b64[(v >> 12) & 63];
        o += i + 1 < in.size() ? s_b64[(v >> 6) & 63] : '=';
        o += '=';
    }
    return o;
}

static bool b64_decode(const std::string &in, std::string &out)
{
    unsigned v = 0;
    int bits = 0;
    out.clear();
    out.reserve(in.size() / 4 * 3);
    for (char c : in)
    {
        const char *f = c ? strchr(s_b64, c) : NULL;
        if (c == '=')
            break;
        if (!f)
            return false;
        v = (v << 6) | (unsigned)(f - s_b64);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out += (char)((v >> bits) & 0xff);
        }
    }
    return true;
}

static std::wstring attach_name(std::string n)
{
    for (char &c : n)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 0x20)
            c = '_';
    if (n.empty() || n.size() > 200)
        n = "attachment";
    std::wstring w = wide(n);
    for (wchar_t &c : w)
        if (c == 0x200e || c == 0x200f || (c >= 0x202a && c <= 0x202e) || (c >= 0x2066 && c <= 0x2069))
            c = L'_';
    return w;
}

static bool save_marked(const std::wstring &file, const std::string &bytes)
{
    static const char zone[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w = 0;
    bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, bytes.data(), (DWORD)bytes.size(), &w, NULL) && w == bytes.size();
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (!ok)
        return false;
    h = CreateFileW((file + L":Zone.Identifier").c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        WriteFile(h, zone, sizeof(zone) - 1, &w, NULL);
        CloseHandle(h);
    }
    return true;
}

static std::wstring argq(const std::wstring &a)
{
    std::wstring o = L"\"";
    size_t bs = 0;
    for (wchar_t c : a)
    {
        if (c == L'\\')
        {
            ++bs;
            continue;
        }
        o.append(c == L'"' ? bs * 2 + 1 : bs, L'\\');
        bs = 0;
        o += c;
    }
    o.append(bs * 2, L'\\');
    return o + L"\"";
}

static bool mail_id_ok(const std::string &id)
{
    if (id.size() != 24 || id[8] != 'T' || id[15] != '-')
        return false;
    for (size_t i = 0; i < id.size(); ++i)
    {
        if (i == 8 || i == 15)
            continue;
        char c = id[i];
        if (i < 15 ? !(c >= '0' && c <= '9') : !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    }
    return true;
}

static bool job_name_ok(const std::string &n)
{
    if (n.empty() || n.size() > 32)
        return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    return true;
}

static bool site_runtime(const std::string &name, std::wstring &php, std::wstring &node)
{
    std::string out;
    if (run(L"--status", &out, NULL) != 0)
        return false;
    size_t p = out.find("\"sites\":[");
    if (p == std::string::npos)
        return false;
    for (const auto &m : parse_list(out.substr(p + 8)))
        if (m.s("name") == name)
        {
            php = wide(m.s("php"));
            node = wide(m.s("node"));
            return true;
        }
    return false;
}

static bool pick_folder(const std::wstring &start, std::wstring &out)
{
    ComPtr<IFileOpenDialog> d;
    ComPtr<IShellItem> it, at;
    DWORD opt = 0;
    PWSTR p = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d))))
        return false;
    d->GetOptions(&opt);
    d->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
    if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), NULL, IID_PPV_ARGS(&at))))
        d->SetFolder(at.Get());
    if (FAILED(d->Show(g_wnd)) || FAILED(d->GetResult(&it)) || FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) || !p)
        return false;
    out = p;
    CoTaskMemFree(p);
    return true;
}

static int recycle(const std::wstring &dir)
{
    std::wstring from = dir;
    SHFILEOPSTRUCTW op = {};
    from.push_back(L'\0');
    op.hwnd = g_wnd;
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING | FOF_SILENT | FOF_NOERRORUI;
    int r = SHFileOperationW(&op);
    if (op.fAnyOperationsAborted)
        return -1;
    return r == 0 && GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES ? 1 : 0;
}

static bool open_terminal(const std::wstring &cwd, std::wstring php = std::wstring(), std::wstring node = std::wstring())
{
    wchar_t sys[MAX_PATH];
    std::wstring path, env, exe, cmd;
    std::vector<std::wstring> add;
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    wchar_t title[] = L"OpenLiteSpeed";
    if (GetSystemDirectoryW(sys, MAX_PATH) == 0)
        return false;
    if (php.empty())
        php = current_php();
    if (node.empty())
        node = ini_str(L"node", L"bin", L"");
    if (!php.empty())
        add.push_back(php);
    if (!node.empty() && node.size() > 9)
        add.push_back(node.substr(0, node.find_last_of(L'\\')));
    add.push_back(std::wstring(g_root) + L"\\tools\\composer");
    add.push_back(std::wstring(g_root) + L"\\mariadb\\bin");
    for (const auto &d : add)
        if (plain_dir(d))
            path += d + L";";
    if (wchar_t *blk = GetEnvironmentStringsW())
    {
        for (wchar_t *e = blk; *e; e += wcslen(e) + 1)
        {
            if (_wcsnicmp(e, L"PATH=", 5) == 0)
                path += e + 5;
            else if (_wcsnicmp(e, L"PHPRC=", 6) != 0 && _wcsnicmp(e, L"NoDefaultCurrentDirectoryInExePath=", 35) != 0)
            {
                env += e;
                env.push_back(L'\0');
            }
        }
        FreeEnvironmentStringsW(blk);
    }
    env += L"PATH=" + path;
    env.push_back(L'\0');
    env += L"NoDefaultCurrentDirectoryInExePath=1";
    env.push_back(L'\0');
    if (!php.empty())
    {
        env += L"PHPRC=" + php;
        env.push_back(L'\0');
    }
    env.push_back(L'\0');
    exe = std::wstring(sys) + L"\\cmd.exe";
    cmd = q(exe);
    si.cb = sizeof(si);
    si.lpTitle = title;
    if (!CreateProcessW(exe.c_str(), &cmd[0], NULL, NULL, FALSE, CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
                        &env[0], cwd.c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static std::wstring vscode_exe()
{
    std::vector<std::wstring> c;
    wchar_t buf[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH))
        c.push_back(std::wstring(buf) + L"\\Programs\\Microsoft VS Code\\Code.exe");
    if (GetEnvironmentVariableW(L"ProgramFiles", buf, MAX_PATH))
        c.push_back(std::wstring(buf) + L"\\Microsoft VS Code\\Code.exe");
    for (HKEY h : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        DWORD size = sizeof(buf);
        if (RegGetValueW(h, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Code.exe", NULL,
                         RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, buf, &size) == ERROR_SUCCESS)
        {
            std::wstring p = buf;
            if (p.size() > 2 && p.front() == L'"' && p.back() == L'"')
                p = p.substr(1, p.size() - 2);
            c.push_back(p);
        }
    }
    for (const auto &p : c)
    {
        DWORD a = GetFileAttributesW(p.c_str());
        if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY) && p.size() > 9
            && _wcsicmp(p.c_str() + p.size() - 9, L"\\Code.exe") == 0)
            return p;
    }
    return std::wstring();
}

static bool open_vscode(const std::wstring &dir)
{
    std::wstring exe = vscode_exe(), cmd;
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    if (exe.empty() || !quoted_ok(exe) || !quoted_ok(dir))
        return false;
    cmd = q(exe) + L" " + q(dir);
    si.cb = sizeof(si);
    if (!CreateProcessW(exe.c_str(), &cmd[0], NULL, NULL, FALSE, 0, NULL, dir.c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static bool autorun_on()
{
    HKEY k;
    DWORD type, n = 0;
    LONG r;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &k)
        != ERROR_SUCCESS)
        return false;
    r = RegQueryValueExW(k, L"OpenLiteSpeed", NULL, &type, NULL, &n);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static bool autorun_set(bool on)
{
    HKEY k;
    LONG r;
    std::wstring v = q(std::wstring(g_root) + L"\\bin\\olstray.exe");
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &k)
        != ERROR_SUCCESS)
        return false;
    if (on)
        r = RegSetValueExW(k, L"OpenLiteSpeed", 0, REG_SZ, (const BYTE *)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    else
    {
        r = RegDeleteValueW(k, L"OpenLiteSpeed");
        if (r == ERROR_FILE_NOT_FOUND)
            r = ERROR_SUCCESS;
    }
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

struct LogFile
{
    std::string id, label;
    std::wstring path;
};

static std::vector<LogFile> log_files()
{
    std::vector<LogFile> v;
    std::wstring r = g_root, www = www_dir();
    wchar_t sect[16384];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    v.push_back({ "error", "Web server errors", r + L"\\logs\\error.log" });
    v.push_back({ "access", "Web server requests", r + L"\\logs\\access.log" });
    v.push_back({ "stderr", "Web server output", r + L"\\logs\\stderr.log" });
    v.push_back({ "mariadb", "MariaDB", r + L"\\logs\\mariadb.err" });
    v.push_back({ "webadmin", "WebAdmin", r + L"\\admin\\logs\\error.log" });
    f = FindFirstFileW((r + L"\\logs\\jobs\\*.log").c_str(), &fd);
    if (f != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::wstring fn = fd.cFileName;
            std::string n = utf8(fn.substr(0, fn.size() - 4));
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n.size() <= 100
                && n.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.") == std::string::npos)
                v.push_back({ "job:" + n, n + " job", r + L"\\logs\\jobs\\" + fn });
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    if (GetPrivateProfileSectionW(L"apps", sect, 16384, g_ini) > 0)
        for (wchar_t *e = sect; *e; e += wcslen(e) + 1)
        {
            wchar_t *eq = wcschr(e, L'=');
            std::string n;
            if (!eq)
                continue;
            n = utf8(std::wstring(e, eq));
            if (!name_ok(n))
                continue;
            bool py = _wcsicmp(eq + 1, L"python") == 0;
            v.push_back({ "app:" + n, n + " app", www + L"\\" + wide(n) + (py ? L"\\stderr.log" : L"\\log\\stderr.log") });
        }
    f = FindFirstFileW((r + L"\\tmp\\tunnel\\*.log").c_str(), &fd);
    if (f != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::wstring fn = fd.cFileName;
            std::string n = utf8(fn.substr(0, fn.size() - 4));
            if (n == "_named")
                v.push_back({ "tunnel:_named", "Tunnel for your domain", r + L"\\tmp\\tunnel\\" + fn });
            else if (name_ok(n))
                v.push_back({ "tunnel:" + n, "Tunnel for " + n, r + L"\\tmp\\tunnel\\" + fn });
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    return v;
}

static bool log_path(const std::string &id, std::wstring &path)
{
    for (const auto &l : log_files())
        if (l.id == id)
        {
            path = l.path;
            return true;
        }
    return false;
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
        out.insert(b + 1, "\"version\":" + jesc(OLS_RELEASE) + ",\"vscode\":" + (vscode_exe().empty() ? "false," : "true,"));
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        Result r;
        r.data = out;
        return r;
    }
    if (a == "php.list")
    {
        static wchar_t labels[MAX_PHP][MAX_PATH + 96], dirs[MAX_PHP][MAX_PATH];
        std::vector<Msg> xd;
        if (run(L"--php list", &out, NULL) == 0)
            xd = parse_list(json_part(out));
        std::lock_guard<std::mutex> g(g_phplock);
        std::wstring cur = current_php();
        int n = panel_php_list(g_root, g_ini, cur.c_str(), labels, dirs, MAX_PHP);
        std::string j = "[";
        std::wstring ours = std::wstring(g_root) + L"\\php\\";
        g_php.clear();
        for (int i = 0; i < n; ++i)
        {
            int ma = 0, mi = 0, pa = 0, ts = 0, ls = 0;
            wchar_t src[24] = L"";
            char ver[32];
            panel_php_info(i, &ma, &mi, &pa, &ts, &ls, src, 24);
            snprintf(ver, sizeof(ver), "%d.%d.%d", ma, mi, pa);
            g_php.push_back(dirs[i]);
            std::string xs, xv;
            for (const auto &e : xd)
                if (_wcsicmp(wide(e.s("dir")).c_str(), dirs[i]) == 0)
                {
                    xs = e.s("xdebug");
                    xv = e.s("xdebugVersion");
                    break;
                }
            j += (i ? "," : "") + std::string("{\"label\":") + jesc(utf8(labels[i])) + ",\"dir\":" + jesc(utf8(dirs[i]))
                 + ",\"current\":" + (_wcsicmp(dirs[i], cur.c_str()) == 0 ? "true" : "false") + ",\"version\":" + jesc(ver)
                 + ",\"ts\":" + (ts ? "true" : "false") + ",\"lsapi\":" + (ls ? "true" : "false")
                 + ",\"source\":" + jesc(utf8(src)) + ",\"ours\":"
                 + (_wcsnicmp(dirs[i], ours.c_str(), ours.size()) == 0 ? "true" : "false") + ",\"xdebug\":" + jesc(xs)
                 + ",\"xdebugVersion\":" + jesc(xv) + "}";
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
    if (a == "log.list")
    {
        std::string j = "[";
        bool first = true;
        for (const auto &l : log_files())
        {
            WIN32_FILE_ATTRIBUTE_DATA fa;
            if (!GetFileAttributesExW(l.path.c_str(), GetFileExInfoStandard, &fa))
                continue;
            unsigned long long size = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            j += (first ? "" : ",") + std::string("{\"id\":") + jesc(l.id) + ",\"label\":" + jesc(l.label)
                 + ",\"size\":" + std::to_string(size) + "}";
            first = false;
        }
        Result r;
        r.data = j + "]";
        return r;
    }
    if (a == "log.read" || a == "log.open")
    {
        std::wstring path;
        if (!log_path(m.s("log"), path))
            return fail("That log is not available.");
        if (a == "log.open")
        {
            wchar_t sys[MAX_PATH];
            if (!GetSystemDirectoryW(sys, MAX_PATH) || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
                return fail("That log is not available.");
            ShellExecuteW(NULL, L"open", (std::wstring(sys) + L"\\notepad.exe").c_str(), q(path).c_str(), NULL,
                          SW_SHOWNORMAL);
            return Result();
        }
        Result r;
        r.data = jesc(read_shared(path, 262144));
        return r;
    }
    if (a == "php.available")
    {
        if ((code = run(L"--php available", &out, NULL)) != 0 || json_part(out).empty())
            return fail(last_line(out, "The PHP versions could not be listed."));
        Result r;
        r.data = json_part(out);
        return r;
    }
    if (a == "php.ext" || a == "php.ini")
    {
        std::wstring dir;
        if (!pick_php(m, dir))
            return fail("Pick a PHP version from the list.");
        if ((code = run((a == "php.ext" ? L"--php ext " : L"--php ini ") + q(dir), &out, NULL)) != 0
            || json_part(out).empty())
            return fail(last_line(out, "The PHP settings could not be read."));
        Result r;
        r.data = json_part(out);
        return r;
    }
    if (a == "php.folder" || a == "php.editini")
    {
        std::wstring dir;
        wchar_t sys[MAX_PATH];
        if (!pick_php(m, dir))
            return fail("Pick a PHP version from the list.");
        if (a == "php.folder")
        {
            ShellExecuteW(NULL, L"explore", dir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return Result();
        }
        if (GetFileAttributesW((dir + L"\\php.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
            run(L"--php ini-create " + q(dir), NULL, NULL);
        if (!GetSystemDirectoryW(sys, MAX_PATH) || GetFileAttributesW((dir + L"\\php.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
            return fail("This PHP has no php.ini.");
        ShellExecuteW(NULL, L"open", (std::wstring(sys) + L"\\notepad.exe").c_str(), q(dir + L"\\php.ini").c_str(), NULL,
                      SW_SHOWNORMAL);
        return Result();
    }
    if (a == "node.list")
    {
        if ((code = run(L"--node list", &out, NULL)) != 0 || json_part(out).empty())
            return fail(last_line(out, "Node.js could not be listed."));
        std::string j = json_part(out);
        {
            std::lock_guard<std::mutex> pg(g_phplock);
            size_t p = 0;
            g_node.clear();
            while ((p = j.find("\"exe\":", p)) != std::string::npos)
            {
                std::string v;
                size_t i = p + 6;
                if (!parse_string(j, i, v))
                    break;
                g_node.push_back(wide(v));
                p = i;
            }
        }
        Result r;
        r.data = j;
        return r;
    }
    if (a == "node.available")
    {
        if ((code = run(L"--node available", &out, NULL)) != 0 || json_part(out).empty())
            return fail(last_line(out, "The Node.js versions could not be listed."));
        Result r;
        r.data = json_part(out);
        return r;
    }
    if (a == "db.list")
    {
        if ((code = run(L"--db list", &out, NULL)) != 0 || json_part(out).empty())
            return fail(last_line(out, "The databases could not be listed."));
        Result r;
        r.data = json_part(out);
        return r;
    }
    if (a == "site.folder" || a == "site.terminal" || a == "site.vscode")
    {
        std::wstring dir;
        if (!site_dir(m.s("name"), dir))
            return fail("That site has no folder.");
        if (a == "site.folder")
            ShellExecuteW(NULL, L"explore", dir.c_str(), NULL, NULL, SW_SHOWNORMAL);
        else if (a == "site.vscode")
        {
            if (!open_vscode(dir))
                return fail("VS Code could not be opened.");
        }
        else
        {
            std::wstring php, node;
            site_runtime(m.s("name"), php, node);
            if (!open_terminal(dir, php, node))
                return fail("The terminal could not be opened.");
        }
        return Result();
    }
    if (a == "mail.list" || a == "job.list")
    {
        if ((code = run(a == "mail.list" ? L"--mail list" : L"--job list", &out, NULL)) != 0 || json_part(out).empty())
            return fail(last_line(out, a == "mail.list" ? "The mail could not be listed." : "The jobs could not be listed."));
        Result r;
        r.data = json_part(out);
        return r;
    }
    if (a == "mail.read")
    {
        std::string id = m.s("mail");
        if (!mail_id_ok(id))
            return fail("That mail is not available.");
        std::wstring f = std::wstring(g_root) + L"\\data\\mail\\" + wide(id) + L".eml";
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (!GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &fa) || fa.nFileSizeHigh || fa.nFileSizeLow > (27u << 20))
            return fail("That mail is not available.");
        Result r;
        r.data = jesc(b64_encode(read_shared(f, 0)));
        return r;
    }
    if (a == "mail.save")
    {
        std::string bytes;
        std::wstring file;
        if (!b64_decode(m.s("data"), bytes))
            return fail("The attachment could not be read.");
        if (!save_as(attach_name(m.s("name")), file))
            return Result();
        return save_marked(file, bytes) ? Result() : fail("The attachment could not be saved.");
    }
    if (a == "terminal")
    {
        std::wstring www = www_dir();
        if (!open_terminal(plain_dir(www) ? www : std::wstring(g_root)))
            return fail("The terminal could not be opened.");
        return Result();
    }
    if (a == "settings")
    {
        Result r;
        r.data = std::string("{\"autostart\":") + (autorun_on() ? "true" : "false") + "}";
        return r;
    }
    if (a == "settings.autostart")
    {
        if (!autorun_set(m.b("on")))
            return fail("The Windows startup setting could not be changed.");
        return Result();
    }
    if (a == "app.restart")
    {
        std::wstring dir;
        if (!site_dir(m.s("name"), dir))
            return fail("That app has no folder.");
        CreateDirectoryW((dir + L"\\tmp").c_str(), NULL);
        HANDLE h = CreateFileW((dir + L"\\tmp\\restart.txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return fail("The app could not be restarted.");
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        SetFileTime(h, NULL, NULL, &ft);
        CloseHandle(h);
        return Result();
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
    if (a == "php.install" || a == "node.install" || a == "composer.install")
    {
        std::unique_lock<std::mutex> dl(g_dllock, std::try_to_lock);
        if (!dl.owns_lock())
            return fail("Another download is still running. Try again when it has finished.");
        if (a == "php.install")
        {
            std::string b = m.s("branch");
            if (!arg_ok(b, ".") || b.find('.') == std::string::npos)
                return fail("Pick a PHP version from the list.");
            code = run(L"--php install " + wide(b) + (m.b("ts") ? L" ts" : L""), &out, NULL);
            return ran(code, out, "PHP could not be installed.");
        }
        if (a == "node.install")
        {
            long long mj = m.n("major");
            if (mj < 18 || mj > 999)
                return fail("Pick a Node.js version from the list.");
            code = run(L"--node install " + std::to_wstring(mj), &out, NULL);
            if (code == 0)
            {
                std::lock_guard<std::mutex> g(g_lock);
                reload_if_running();
            }
            return ran(code, out, "Node.js could not be installed.");
        }
        return ran(run(L"--composer install", &out, NULL), out, "Composer could not be installed.");
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
        std::string n = m.s("name"), k = m.s("kind");
        if (!name_ok(n))
            return fail("Use lowercase letters, digits and '-' for the site name.");
        if (m.b("wp"))
            k = "wordpress";
        std::wstring flag = k == "wordpress" ? L" --wordpress"
                            : k == "node"    ? L" --node"
                            : k == "python"  ? L" --python"
                            : k == "laravel" ? L" --laravel"
                                             : L"";
        code = run(L"--new-site " + wide(n) + flag, &out, NULL);
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
        {
            run(L"--site resync", NULL, NULL);
            reload_if_running();
        }
        return ran(code, out, "PHP could not be switched.");
    }
    if (a == "php.remove")
    {
        std::wstring dir;
        if (!pick_php(m, dir))
            return fail("Pick a PHP version from the list.");
        reload_if_running();
        return ran(run(L"--php remove " + q(dir), &out, NULL), out, "PHP could not be removed.");
    }
    if (a == "php.extset")
    {
        std::wstring dir;
        std::string n = m.s("name");
        if (!pick_php(m, dir) || !arg_ok(n, "_"))
            return fail("Pick an extension from the list.");
        code = run(L"--php ext-set " + q(dir) + L" " + wide(n) + (m.b("on") ? L" on" : L" off"), &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, "The extension could not be changed.");
    }
    if (a == "mail.set")
    {
        code = run(m.b("on") ? L"--mail on" : L"--mail off", &out, NULL);
        if (code != 0)
            return fail(last_line(out, "The mail catcher could not be changed."));
        Result r;
        r.data = jesc(last_line(out, ""));
        return r;
    }
    if (a == "mail.remove" || a == "mail.clear")
    {
        std::string id = m.s("mail");
        if (a == "mail.remove" && !mail_id_ok(id))
            return fail("That mail is not available.");
        return ran(run(a == "mail.remove" ? L"--mail remove " + wide(id) : L"--mail clear", &out, NULL), out,
                   "The mail could not be removed.");
    }
    if (a == "job.add" || a == "job.do" || a == "job.autostart")
    {
        std::string site = m.s("site"), n = m.s("name"), op = m.s("op"), cmd = m.s("command");
        std::wstring args;
        if (!name_ok(site) || !job_name_ok(n))
            return fail("Use lowercase letters, digits and '-' for the job name.");
        if (a == "job.add")
        {
            if (cmd.empty() || cmd.size() > 4000 || cmd.find_first_of("\r\n") != std::string::npos)
                return fail("Enter the command on one line.");
            args = L"--job add " + wide(site) + L" " + wide(n) + L" " + argq(wide(cmd));
        }
        else if (a == "job.autostart")
            args = L"--job autostart " + wide(site) + L" " + wide(n) + (m.b("on") ? L" on" : L" off");
        else if (op == "start" || op == "stop" || op == "restart" || op == "remove")
            args = L"--job " + wide(op) + L" " + wide(site) + L" " + wide(n);
        else
            return fail("That is not a job action.");
        code = run(args, &out, NULL);
        if (code != 0)
            return fail(last_line(out, "The job could not be changed."));
        Result r;
        r.data = jesc(last_line(out, ""));
        return r;
    }
    if (a == "site.node")
    {
        std::wstring exe, sd;
        std::string n = m.s("name");
        long long i = m.n("index");
        if (!site_dir(n, sd))
            return fail("That site has no folder.");
        if (i >= 0 && !pick_node(m, exe))
            return fail("Pick a Node.js version from the list.");
        code = run(L"--site node " + wide(n) + L" " + (i >= 0 ? q(exe) : std::wstring(L"default")), &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, "The Node.js version of the site could not be changed.");
    }
    if (a == "php.xdebug")
    {
        std::wstring dir;
        if (!pick_php(m, dir))
            return fail("Pick a PHP version from the list.");
        code = run(L"--php xdebug " + q(dir) + (m.b("on") ? L" on" : L" off"), &out, NULL);
        if (code != 0)
            return fail(last_line(out, "Xdebug could not be changed."));
        Result r;
        r.data = jesc(last_line(out, ""));
        return r;
    }
    if (a == "php.iniset")
    {
        std::wstring dir;
        std::string k = m.s("key"), v = m.s("value");
        if (!pick_php(m, dir) || !arg_ok(k, "._") || !arg_ok(v, "/_+-"))
            return fail("That value is not allowed.");
        code = run(L"--php ini-set " + q(dir) + L" " + wide(k) + L" " + wide(v), &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, "The setting could not be saved.");
    }
    if (a == "site.php")
    {
        std::wstring dir, sd;
        std::string n = m.s("name");
        long long i = m.n("index");
        if (!site_dir(n, sd))
            return fail("That site has no folder.");
        if (i >= 0 && !pick_php(m, dir))
            return fail("Pick a PHP version from the list.");
        code = run(L"--site php " + wide(n) + L" " + (i >= 0 ? q(dir) : std::wstring(L"default")), &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, "The PHP version of the site could not be changed.");
    }
    if (a == "site.app")
    {
        std::wstring sd;
        std::string n = m.s("name"), k = m.s("kind");
        if (!site_dir(n, sd) || (k != "node" && k != "python" && k != "off"))
            return fail("That site cannot run as an app.");
        code = run(L"--site app " + wide(n) + L" " + wide(k), &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, "The site could not be changed.");
    }
    if (a == "site.remove")
    {
        std::wstring dir;
        std::string n = m.s("name");
        if (!name_ok(n))
            return fail("That site cannot be removed.");
        dir = www_dir() + L"\\" + wide(n);
        DWORD at = GetFileAttributesW(dir.c_str());
        if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_REPARSE_POINT))
            return fail("This site folder is a link. Remove it in File Explorer.");
        std::wstring wn = wide(n), kind = ini_str(L"apps", wn.c_str(), L""), php = ini_str(L"sitephp", wn.c_str(), L""),
                     node = ini_str(L"sitenode", wn.c_str(), L"");
        run(L"--tunnel stop " + wn, NULL, NULL);
        code = run(L"--site forget " + wn, &out, NULL);
        if (code != 0)
            return fail(last_line(out, "The site list could not be updated."));
        reload_if_running();
        bool gone = at == INVALID_FILE_ATTRIBUTES;
        for (int i = 0; i < 12 && !gone; ++i)
        {
            Sleep(i ? 1000 : 1500);
            int rc = recycle(dir);
            gone = rc == 1;
            if (rc < 0)
                break;
        }
        if (!gone)
        {
            if (!kind.empty())
                WritePrivateProfileStringW(L"apps", wn.c_str(), kind.c_str(), g_ini);
            if (!php.empty())
                WritePrivateProfileStringW(L"sitephp", wn.c_str(), php.c_str(), g_ini);
            if (!node.empty())
                WritePrivateProfileStringW(L"sitenode", wn.c_str(), node.c_str(), g_ini);
            run(L"--site resync", NULL, NULL);
            reload_if_running();
            return fail("The site folder could not be moved to the Recycle Bin. Close programs that use it and try again.");
        }
        run(L"--site resync", NULL, NULL);
        hosts_update();
        reload_if_running();
        return Result();
    }
    if (a == "node.default" || a == "node.remove")
    {
        std::wstring exe;
        if (!pick_node(m, exe))
            return fail("Pick a Node.js version from the list.");
        code = run((a == "node.default" ? L"--node default " : L"--node remove ") + q(exe), &out, NULL);
        if (code == 0)
            reload_if_running();
        return ran(code, out, a == "node.default" ? "Node.js could not be switched." : "Node.js could not be removed.");
    }
    if (a == "db.create")
    {
        std::string n = m.s("name");
        if (!db_ok(n))
            return fail("Use lowercase letters, digits and '_' for the database name.");
        code = run(L"--db create " + wide(n) + (m.b("user") ? L"" : L" --no-user"), &out, NULL);
        if (code != 0)
            return fail(last_line(out, "The database could not be created."));
        Result r;
        size_t p = out.find("\npassword ");
        if (p != std::string::npos)
        {
            p += 10;
            std::string pw = out.substr(p, out.find_first_of("\r\n", p) - p);
            r.data = "{\"user\":" + jesc(n) + ",\"password\":" + jesc(pw) + "}";
            SecureZeroMemory(&pw[0], pw.size());
        }
        SecureZeroMemory(&out[0], out.size());
        return r;
    }
    if (a == "db.copytext")
    {
        std::string t = m.s("text");
        bool ok = !t.empty() && t.size() < 128 && clip_set(wide(t), true);
        SecureZeroMemory(&t[0], t.size());
        return ok ? Result() : fail("The text could not be copied.");
    }
    if (a == "db.drop")
    {
        std::string n = m.s("name");
        if (!db_ok(n))
            return fail("That database cannot be removed.");
        return ran(run(L"--db drop " + wide(n) + (m.b("user") ? L" --user" : L""), &out, NULL), out,
                   "The database could not be removed.");
    }
    if (a == "db.export" || a == "db.import")
    {
        std::string n = m.s("name");
        std::wstring file;
        SYSTEMTIME st;
        wchar_t stamp[32];
        if (!db_ok(n))
            return fail("Pick a database from the list.");
        GetLocalTime(&st);
        swprintf(stamp, 32, L"-%04d%02d%02d-%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        if (!pick_file(a == "db.export", wide(n) + stamp + L".sql", file))
        {
            Result r;
            r.data = "\"cancelled\"";
            return r;
        }
        if (!quoted_ok(file))
            return fail("That file cannot be used.");
        code = run((a == "db.export" ? L"--db export " : L"--db import ") + wide(n) + L" " + q(file), &out, NULL);
        return ran(code, out, a == "db.export" ? "The database could not be exported." : "The import stopped with an error.");
    }
    if (a == "settings.www")
    {
        std::wstring cur = www_dir(), dir, tld = ini_str(L"sites", L"tld", L"test");
        if (!pick_folder(cur, dir))
        {
            Result r;
            r.data = "\"cancelled\"";
            return r;
        }
        if (!quoted_ok(dir) || !plain_dir(dir) || dir.size() <= 3)
            return fail("That folder cannot be the projects folder.");
        if (!WritePrivateProfileStringW(L"sites", L"www", dir.c_str(), g_ini))
            return fail("The projects folder could not be saved.");
        code = run(L"--dev-sites " + q(dir) + L" " + tld
                       + (GetPrivateProfileIntW(L"sites", L"root", 0, g_ini) ? L" --root" : L""),
                   &out, NULL);
        if (code == 0)
        {
            hosts_update();
            reload_if_running();
        }
        return ran(code, out, "The sites in that folder could not be set up.");
    }
    if (a == "settings.port")
    {
        std::string w = m.s("which");
        long long port = m.n("port");
        bool was;
        if ((w != "http" && w != "admin") || port <= 0 || port > 65535)
            return fail("Use a port between 1 and 65535.");
        code = run(L"--set-port " + wide(w) + L" " + std::to_wstring(port), &out, NULL);
        if (code != 0)
            return fail(last_line(out, "The port could not be changed."));
        if ((was = is_running()))
        {
            code = run(L"-s restart", &out, NULL);
            if (code != 0 && service_installed())
                code = run_elevated(L"-s restart");
            if (code != 0)
                return fail(last_line(out, "The port was saved, but the web server did not restart."));
            if (!wait_up((int)port, 20))
                return fail("The port was saved, but the web server is not answering on port " + std::to_string(port)
                            + ". See the error log.");
        }
        return Result();
    }
    if (a == "settings.adminpw")
    {
        std::string pw = m.s("password");
        bool ok = pw.size() >= 8 && pw.size() <= 128;
        for (char c : pw)
            if ((unsigned char)c < 0x20 || c == 0x7f)
                ok = false;
        if (!ok)
        {
            SecureZeroMemory(&pw[0], pw.size());
            return fail("Use 8 to 128 characters for the WebAdmin password.");
        }
        std::string in = pw + "\n";
        code = run(L"--admin-password " + q(std::wstring(g_root) + L"\\admin\\conf\\htpasswd") + L" - admin", &out, &in);
        SecureZeroMemory(&in[0], in.size());
        SecureZeroMemory(&pw[0], pw.size());
        return ran(code, out, "The WebAdmin password could not be changed.");
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
        HRESULT co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        try
        {
            r = handle(m);
        }
        catch (...)
        {
            r = fail("Something went wrong.");
        }
        if (SUCCEEDED(co))
            CoUninitialize();
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
                                              LPWSTR uri = nullptr;
                                              bool doc = SUCCEEDED(e->get_Uri(&uri)) && uri && wcscmp(uri, L"about:srcdoc") == 0;
                                              if (uri)
                                                  CoTaskMemFree(uri);
                                              if (!doc)
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
