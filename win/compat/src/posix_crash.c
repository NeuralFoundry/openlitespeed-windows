#include "compat_internal.h"
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif
#include <dbghelp.h>
#include <shellapi.h>

static void crash_print(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    DWORD n;
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (h && h != INVALID_HANDLE_VALUE)
        WriteFile(h, buf, (DWORD)strlen(buf), &n, NULL);
    OutputDebugStringA(buf);
}

static void write_dump(EXCEPTION_POINTERS *ep)
{
    wchar_t path[MAX_PATH], root[MAX_PATH];
    MINIDUMP_EXCEPTION_INFORMATION mei;
    SECURITY_ATTRIBUTES sa;
    HANDLE f;
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 64)
        return;
    swprintf(path, MAX_PATH, L"%s\\logs\\crash-%lu.dmp", root, GetCurrentProcessId());
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = ols_private_sd(0);
    if (!sa.lpSecurityDescriptor)
        return;
    f = CreateFileW(path, GENERIC_WRITE, 0, &sa, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(sa.lpSecurityDescriptor);
    if (f == INVALID_HANDLE_VALUE)
        return;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                      MiniDumpNormal | MiniDumpWithThreadInfo, &mei, NULL, NULL);
    CloseHandle(f);
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
    static volatile LONG once;
    HANDLE proc = GetCurrentProcess(), thr = GetCurrentThread();
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf;
    char symbuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO *sym = (SYMBOL_INFO *)symbuf;
    IMAGEHLP_LINE64 line;
    int i;
    if (InterlockedExchange(&once, 1))
        return EXCEPTION_EXECUTE_HANDLER;
    crash_print("\n*** openlitespeed crash: code 0x%08lX at %p, thread %lu\n",
                ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress,
                GetCurrentThreadId());
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        && ep->ExceptionRecord->NumberParameters >= 2)
        crash_print("    %s address %p\n",
                    ep->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
                    (void *)ep->ExceptionRecord->ExceptionInformation[1]);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);
    memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset = ctx.Rip;
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rbp;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp;
    sf.AddrStack.Mode = AddrModeFlat;
    for (i = 0; i < 48; ++i)
    {
        DWORD64 disp = 0;
        DWORD ldisp = 0;
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, thr, &sf, &ctx, NULL,
                         SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;
        if (!sf.AddrPC.Offset)
            break;
        memset(symbuf, 0, sizeof(symbuf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        line.SizeOfStruct = sizeof(line);
        if (SymFromAddr(proc, sf.AddrPC.Offset, &disp, sym))
        {
            if (SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &ldisp, &line))
                crash_print("  #%02d %s+0x%llx  %s:%lu\n", i, sym->Name, disp,
                            line.FileName, line.LineNumber);
            else
                crash_print("  #%02d %s+0x%llx\n", i, sym->Name, disp);
        }
        else
            crash_print("  #%02d %p\n", i, (void *)sf.AddrPC.Offset);
    }
    write_dump(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

void ols_install_crash_handler(void)
{
    SetUnhandledExceptionFilter(crash_filter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
}

void ols_win_main_init(int *argc, char ***argv)
{
    int n, i;
    LPWSTR *wargv;
    char **out;
    ols_install_crash_handler();
    ols_compat_init();
    wargv = CommandLineToArgvW(GetCommandLineW(), &n);
    if (!wargv)
        return;
    out = (char **)calloc(n + 1, sizeof(char *));
    if (!out)
        return;
    for (i = 0; i < n; ++i)
    {
        int len;
        if (i == 0)
        {
            wchar_t full[MAX_PATH];
            char *buf = (char *)malloc(PATH_MAX * 3);
            if (buf && GetModuleFileNameW(NULL, full, MAX_PATH)
                && ols_w_to_path(full, buf, PATH_MAX * 3) == 0)
            {
                out[0] = buf;
                continue;
            }
            free(buf);
        }
        len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        out[i] = (char *)malloc(len > 0 ? len : 1);
        if (!out[i])
            return;
        out[i][0] = 0;
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, out[i], len, NULL, NULL);
    }
    LocalFree(wargv);
    *argc = n;
    *argv = out;
}
