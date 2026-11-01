#include "lscgiddef.h"
#include "blake2b.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wctype.h>

#include <string>
#include <vector>

#undef recv
#undef send
#undef shutdown
#undef setsockopt

namespace
{

unsigned char s_secret[LSCGID_SECRET_LEN];
HANDLE s_log = INVALID_HANDLE_VALUE;
CRITICAL_SECTION s_logLock;

std::wstring W(const char *s, int len = -1)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, len, NULL, 0);
    if (n <= 0)
        return std::wstring();
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, len, &w[0], n);
    if (len < 0 && !w.empty() && w.back() == L'\0')
        w.pop_back();
    return w;
}

void log_msg(const char *fmt, ...)
{
    char buf[2048];
    SYSTEMTIME t;
    va_list ap;
    int n;
    if (s_log == INVALID_HANDLE_VALUE)
        return;
    GetLocalTime(&t);
    n = snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d [lscgid] ", t.wYear, t.wMonth, t.wDay,
                 t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n - 2, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(buf) - 2)
        n = (int)sizeof(buf) - 2;
    buf[n++] = '\n';
    EnterCriticalSection(&s_logLock);
    DWORD w;
    WriteFile(s_log, buf, n, &w, NULL);
    LeaveCriticalSection(&s_logLock);
}

int recv_all(SOCKET s, char *p, int n)
{
    while (n > 0)
    {
        int r = ::recv(s, p, n, 0);
        if (r <= 0)
            return -1;
        p += r;
        n -= r;
    }
    return 0;
}

int send_all(SOCKET s, const char *p, int n)
{
    while (n > 0)
    {
        int r = ::send(s, p, n, 0);
        if (r <= 0)
            return -1;
        p += r;
        n -= r;
    }
    return 0;
}

void cgi_error(SOCKET s, int status)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "Status:%d\n\nInternal error.\n", status);
    send_all(s, buf, n);
}

std::wstring native(const char *path)
{
    char buf[PATH_MAX * 3];
    if (ols_native_path(path, (int)strlen(path), buf, sizeof(buf)) < 0)
        return std::wstring();
    std::wstring w = W(buf);
    for (size_t i = 0; i < w.size(); ++i)
        if (w[i] == L'/')
            w[i] = L'\\';
    return w;
}

std::wstring quote(const std::wstring &a)
{
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        return a;
    std::wstring out = L"\"";
    for (size_t i = 0;; ++i)
    {
        size_t bs = 0;
        while (i < a.size() && a[i] == L'\\')
        {
            ++i;
            ++bs;
        }
        if (i == a.size())
        {
            out.append(bs * 2, L'\\');
            break;
        }
        if (a[i] == L'"')
            out.append(bs * 2 + 1, L'\\');
        else
            out.append(bs, L'\\');
        out += a[i];
    }
    return out + L"\"";
}

std::wstring lower(std::wstring s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (wchar_t)towlower(s[i]);
    return s;
}

std::wstring search_exe(const std::wstring &name)
{
    wchar_t buf[MAX_PATH];
    DWORD n = SearchPathW(NULL, name.c_str(), L".exe", MAX_PATH, buf, NULL);
    return n > 0 && n < MAX_PATH ? std::wstring(buf) : std::wstring();
}

int file_exists(const std::wstring &p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::wstring> split_line(const std::wstring &s)
{
    std::vector<std::wstring> out;
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && iswspace(s[i]))
            ++i;
        if (i >= s.size())
            break;
        std::wstring t;
        if (s[i] == L'"')
        {
            ++i;
            while (i < s.size() && s[i] != L'"')
                t += s[i++];
            ++i;
        }
        else
            while (i < s.size() && !iswspace(s[i]))
                t += s[i++];
        out.push_back(t);
    }
    return out;
}

int command_for(const std::wstring &script, const std::vector<std::wstring> &args, std::wstring &app,
                std::wstring &cmd)
{
    size_t dot = script.find_last_of(L'.');
    size_t slash = script.find_last_of(L'\\');
    std::wstring ext = dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)
                       ? lower(script.substr(dot)) : std::wstring();
    if (ext == L".exe" || ext == L".com")
    {
        app = script;
        cmd = quote(script);
        for (size_t i = 0; i < args.size(); ++i)
            cmd += L" " + quote(args[i]);
        return 0;
    }
    if (ext == L".bat" || ext == L".cmd")
    {
        wchar_t sys[MAX_PATH];
        UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        if (n == 0 || n >= MAX_PATH)
            return 500;
        app = std::wstring(sys) + L"\\cmd.exe";
        cmd = L"cmd.exe /d /c \"" + quote(script) + L"\"";
        return 0;
    }
    HANDLE h = CreateFileW(script.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_ACCESS_DENIED ? 403 : 404;
    char buf[1024];
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(h);
    if (!ok)
        return 500;
    buf[got] = 0;
    char *p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    if (p[0] != '#' || p[1] != '!')
    {
        log_msg("%ls has no #! line naming its interpreter", script.c_str());
        return 500;
    }
    p += 2;
    char *e = strpbrk(p, "\r\n");
    if (e)
        *e = 0;
    std::vector<std::wstring> t = split_line(W(p));
    if (t.empty())
        return 500;
    std::wstring interp = t[0];
    t.erase(t.begin());
    for (size_t i = 0; i < interp.size(); ++i)
        if (interp[i] == L'/')
            interp[i] = L'\\';
    std::wstring base = interp.substr(interp.find_last_of(L'\\') == std::wstring::npos ? 0
                                      : interp.find_last_of(L'\\') + 1);
    if (lower(base) == L"env" || lower(base) == L"env.exe")
    {
        if (t.empty())
            return 500;
        app = search_exe(t[0]);
        t.erase(t.begin());
    }
    else if (file_exists(interp))
        app = interp;
    else if (file_exists(interp + L".exe"))
        app = interp + L".exe";
    else
        app = search_exe(base);
    if (app.empty())
    {
        log_msg("interpreter %ls for %ls not found", interp.c_str(), script.c_str());
        return 500;
    }
    cmd = quote(app);
    for (size_t i = 0; i < t.size(); ++i)
        cmd += L" " + quote(t[i]);
    cmd += L" " + quote(script);
    for (size_t i = 0; i < args.size(); ++i)
        cmd += L" " + quote(args[i]);
    return 0;
}

struct Req
{
    lscgid_req hdr;
    std::vector<char> data;
    std::wstring exe, cwd;
    std::vector<std::wstring> args;
    std::vector<std::wstring> env;
    std::string stderrPath;
};

int read_req(SOCKET s, Req &r)
{
    DWORD tmo = 10000;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof(tmo));
    if (recv_all(s, (char *)&r.hdr, sizeof(r.hdr)) != 0)
        return 500;
    if (r.hdr.m_version != LSCGID_VERSION_2 || r.hdr.m_szData < 0 || r.hdr.m_szData > 65536
        || r.hdr.m_nenv < 1 || r.hdr.m_nargv < 2)
    {
        log_msg("invalid request header");
        return 500;
    }
    r.data.resize(r.hdr.m_szData + 2);
    if (r.hdr.m_szData && recv_all(s, &r.data[0], r.hdr.m_szData) != 0)
        return 500;
    tmo = 0;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof(tmo));

    unsigned char supplied[LSCGID_MAC_LEN], expected[LSCGID_MAC_LEN];
    blake2b_ctx ctx;
    memcpy(supplied, r.hdr.m_md5, sizeof(supplied));
    memset(r.hdr.m_md5, 0, sizeof(r.hdr.m_md5));
    int bad = blake2b_init_key(&ctx, sizeof(expected), s_secret, sizeof(s_secret)) != 0
              || blake2b_update(&ctx, &r.hdr, sizeof(r.hdr)) != 0
              || blake2b_update(&ctx, r.data.data(), (size_t)r.hdr.m_szData) != 0
              || blake2b_final(&ctx, expected, sizeof(expected)) != 0;
    unsigned char diff = 0;
    for (size_t i = 0; i < sizeof(expected); ++i)
        diff |= expected[i] ^ supplied[i];
    if (bad || diff)
    {
        log_msg("request validation failed");
        return 500;
    }
    if (r.hdr.m_type != LSCGID_TYPE_CGI)
    {
        log_msg("request type %d is not supported on Windows", r.hdr.m_type);
        return 500;
    }

    const char *p = r.data.data(), *end = p + r.hdr.m_szData;
    p += r.hdr.m_chrootPathLen;
    const char *exe = p;
    if (p + r.hdr.m_exePathLen + r.hdr.m_exeNameLen + 1 > end || exe[r.hdr.m_exePathLen + r.hdr.m_exeNameLen] != 0)
        return 500;
    if (exe[r.hdr.m_exePathLen] == '&')
    {
        log_msg("detached commands are not supported on Windows");
        return 403;
    }
    r.exe = native(exe);
    if (r.exe.empty())
        return 500;
    r.cwd = r.exe.substr(0, r.exe.find_last_of(L'\\'));
    p += r.hdr.m_exePathLen + r.hdr.m_exeNameLen + 1;
    for (int i = 1; i < r.hdr.m_nargv - 1; ++i)
    {
        if (p + 2 > end)
            return 500;
        unsigned short len = *(const unsigned short *)p;
        p += 2;
        if (len == 0 || p + len > end || p[len - 1] != 0)
            return 500;
        r.args.push_back(W(p));
        p += len;
    }
    if (p + 2 > end || p[0] || p[1])
        return 500;
    p += 2;
    for (int i = 0; i < r.hdr.m_nenv - 1; ++i)
    {
        if (p + 2 > end)
            return 500;
        unsigned short len = *(const unsigned short *)p;
        p += 2;
        if (len == 0 || p + len > end || p[len - 1] != 0)
            return 500;
        if (strncmp(p, "LS_STDERR_LOG=", 14) == 0)
            r.stderrPath = p + 14;
        else if (strncmp(p, "LS_CWD=", 7) == 0)
            r.cwd = native(p + 7);
        else if (strncmp(p, "LS_", 3) != 0 && strncmp(p, "PATH=", 5) != 0)
            r.env.push_back(W(p));
        p += len;
    }
    if (p + 2 != end || p[0] || p[1])
        return 500;
    return 0;
}

std::vector<wchar_t> env_block(const std::vector<std::wstring> &env)
{
    static const wchar_t *inherit[] = {L"SystemRoot", L"SystemDrive", L"windir", L"TEMP", L"TMP", L"PATH",
                                       L"PATHEXT", L"COMSPEC", L"ProgramData", L"ALLUSERSPROFILE",
                                       L"PROGRAMFILES", L"NUMBER_OF_PROCESSORS", L"PROCESSOR_ARCHITECTURE", NULL};
    std::vector<std::wstring> all(env);
    for (int i = 0; inherit[i]; ++i)
    {
        std::wstring name = std::wstring(inherit[i]) + L"=";
        bool have = false;
        for (size_t j = 0; j < all.size() && !have; ++j)
            have = _wcsnicmp(all[j].c_str(), name.c_str(), name.size()) == 0;
        if (have)
            continue;
        wchar_t buf[32767];
        DWORD n = GetEnvironmentVariableW(inherit[i], buf, 32767);
        if (n > 0 && n < 32767)
            all.push_back(name + buf);
    }
    std::vector<wchar_t> block;
    for (size_t i = 0; i < all.size(); ++i)
    {
        block.insert(block.end(), all[i].begin(), all[i].end());
        block.push_back(0);
    }
    block.push_back(0);
    return block;
}

struct Pump
{
    SOCKET s;
    HANDLE in;
};

DWORD WINAPI pump_input(LPVOID arg)
{
    Pump *p = (Pump *)arg;
    char buf[16384];
    for (;;)
    {
        int r = ::recv(p->s, buf, sizeof(buf), 0);
        if (r <= 0)
            break;
        DWORD w;
        if (!WriteFile(p->in, buf, r, &w, NULL))
            break;
    }
    CloseHandle(p->in);
    return 0;
}

void handle(int fd)
{
    SOCKET s = ols_socket_from_fd(fd);
    u_long nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    Req r;
    int status = read_req(s, r);
    std::wstring app, cmd;
    if (!status)
        status = command_for(r.exe, r.args, app, cmd);
    if (status)
    {
        cgi_error(s, status);
        close(fd);
        return;
    }

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE inR = NULL, inW = NULL, outR = NULL, outW = NULL, errH = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&inR, &inW, &sa, 0) || !CreatePipe(&outR, &outW, &sa, 0))
    {
        cgi_error(s, 500);
        close(fd);
        return;
    }
    SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
    if (!r.stderrPath.empty())
        errH = CreateFileW(native(r.stderrPath.c_str()).c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (errH == INVALID_HANDLE_VALUE && s_log != INVALID_HANDLE_VALUE)
        DuplicateHandle(GetCurrentProcess(), s_log, GetCurrentProcess(), &errH, 0, TRUE, DUPLICATE_SAME_ACCESS);
    if (errH == INVALID_HANDLE_VALUE)
        errH = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    HANDLE list[3] = {inR, outW, errH};
    SIZE_T attrsz = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrsz);
    std::vector<char> attrbuf(attrsz);
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)&attrbuf[0];
    InitializeProcThreadAttributeList(attrs, 1, 0, &attrsz);
    UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list, sizeof(list), NULL, NULL);
    STARTUPINFOEXW si;
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = inR;
    si.StartupInfo.hStdOutput = outW;
    si.StartupInfo.hStdError = errH;
    si.lpAttributeList = attrs;
    std::vector<wchar_t> env = env_block(r.env);
    std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back(0);
    PROCESS_INFORMATION pi;
    BOOL ok = CreateProcessW(app.c_str(), &cmdline[0], NULL, NULL, TRUE,
                             CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW
                             | EXTENDED_STARTUPINFO_PRESENT,
                             &env[0], r.cwd.c_str(), &si.StartupInfo, &pi);
    DWORD err = GetLastError();
    DeleteProcThreadAttributeList(attrs);
    CloseHandle(inR);
    CloseHandle(outW);
    CloseHandle(errH);
    if (!ok)
    {
        log_msg("cannot start %ls (error %lu)", app.c_str(), err);
        CloseHandle(inW);
        CloseHandle(outR);
        cgi_error(s, err == ERROR_ACCESS_DENIED ? 403 : 500);
        close(fd);
        return;
    }
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
        memset(&li, 0, sizeof(li));
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
        AssignProcessToJobObject(job, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    Pump pump = {s, inW};
    HANDLE th = CreateThread(NULL, 0, pump_input, &pump, 0, NULL);
    if (!th)
        CloseHandle(inW);
    char buf[16384];
    DWORD got;
    while (ReadFile(outR, buf, sizeof(buf), &got, NULL) && got)
        if (send_all(s, buf, (int)got) != 0)
            break;
    CloseHandle(outR);
    WaitForSingleObject(pi.hProcess, 5000);
    if (job)
        CloseHandle(job);
    else
        TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    ::shutdown(s, SD_SEND);
    close(fd);
    if (th)
    {
        WaitForSingleObject(th, 5000);
        CloseHandle(th);
    }
}

DWORD WINAPI conn_thread(LPVOID arg)
{
    handle((int)(intptr_t)arg);
    return 0;
}

}


extern "C" int ols_lscgid_main(int argc, char **argv)
{
    InitializeCriticalSection(&s_logLock);
    if (argc < 3)
        return 2;
    const char *sock = argv[2];
    if (argc > 3 && argv[3][0])
        s_log = CreateFileW(native(argv[3]).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE
                            | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD got = 0, total = 0;
    while (total < sizeof(s_secret)
           && ReadFile(in, s_secret + total, (DWORD)(sizeof(s_secret) - total), &got, NULL) && got)
        total += got;
    if (total != sizeof(s_secret))
    {
        log_msg("secret not received");
        return 1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path) - 1);
    unlink(sock);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd, 100) != 0)
    {
        log_msg("cannot listen on %s: %s", sock, strerror(errno));
        return 1;
    }
    u_long nb = 0;
    ioctlsocket(ols_socket_from_fd(fd), FIONBIO, &nb);
    log_msg("listening on %s", sock);
    for (;;)
    {
        int c = accept(fd, NULL, NULL);
        if (c == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            Sleep(10);
            continue;
        }
        HANDLE t = CreateThread(NULL, 0, conn_thread, (LPVOID)(intptr_t)c, 0, NULL);
        if (t)
            CloseHandle(t);
        else
            close(c);
    }
}
