#ifndef OLS_POCO_GLOB_H
#define OLS_POCO_GLOB_H

#include <set>
#include <string>
#include <windows.h>

namespace Poco
{

class Glob
{
public:
    static void glob(const std::string &pattern, std::set<std::string> &files, int = 0)
    {
        std::wstring w = widen(pattern);
        for (auto &c : w)
            if (c == L'/')
                c = L'\\';
        size_t lead = w.find_first_not_of(L'\\');
        if (lead == std::wstring::npos)
            return;
        expand(w.substr(0, lead), w.substr(lead), files);
    }

private:
    static std::wstring widen(const std::string &s)
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
        std::wstring w(n, L'\0');
        if (n > 0)
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
        return w;
    }

    static std::string narrow(const std::wstring &w)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
        std::string s(n, '\0');
        if (n > 0)
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
        return s;
    }

    static bool match(const wchar_t *p, const wchar_t *s)
    {
        for (; *p; ++p, ++s)
        {
            if (*p == L'*')
            {
                while (p[1] == L'*')
                    ++p;
                for (const wchar_t *t = s;; ++t)
                {
                    if (match(p + 1, t))
                        return true;
                    if (!*t)
                        return false;
                }
            }
            if (!*s)
                return false;
            if (*p != L'?' && towlower(*p) != towlower(*s))
                return false;
        }
        return *s == 0;
    }

    static std::wstring join(const std::wstring &base, const std::wstring &name)
    {
        if (base.empty() || base.back() == L'\\')
            return base + name;
        return base + L"\\" + name;
    }

    static void expand(const std::wstring &base, const std::wstring &rest, std::set<std::string> &out)
    {
        size_t p = rest.find(L'\\');
        std::wstring comp = rest.substr(0, p);
        std::wstring tail = p == std::wstring::npos ? L"" : rest.substr(p + 1);
        bool last = tail.find_first_not_of(L'\\') == std::wstring::npos;
        if (comp.empty())
        {
            if (!last)
                expand(base, tail, out);
            return;
        }
        std::wstring path = join(base, comp);
        if (comp.find_first_of(L"*?") == std::wstring::npos)
        {
            if (!last)
                expand(path, tail, out);
            else if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                out.insert(narrow(path));
            return;
        }
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(path.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            if (fd.cFileName[0] == L'.' && comp[0] != L'.')
                continue;
            if (!match(comp.c_str(), fd.cFileName))
                continue;
            std::wstring m = join(base, fd.cFileName);
            if (last)
                out.insert(narrow(m));
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                expand(m, tail, out);
        }
        while (FindNextFileW(h, &fd));
        FindClose(h);
    }
};

}

#endif
