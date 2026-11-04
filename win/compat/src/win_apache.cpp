#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <regex>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <cstring>
#include <cctype>
#include "phpdetect.h"

extern "C" int ols_import_apache(int argc, char **argv);
extern "C" int ols_import_nginx(int argc, char **argv);
extern "C" PSECURITY_DESCRIPTOR ols_private_sd(int inherit);

namespace
{

typedef std::string S;
typedef std::vector<S> SV;

S lower(S s)
{
    for (char &c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

bool ieq(const S &a, const S &b) { return _stricmp(a.c_str(), b.c_str()) == 0; }
bool starts(const S &s, const S &p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool istarts(const S &s, const S &p) { return s.size() >= p.size() && _strnicmp(s.c_str(), p.c_str(), p.size()) == 0; }
bool ends(const S &s, const S &p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }

S trim(const S &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == S::npos)
        return S();
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

S join(const SV &v, const char *sep)
{
    S out;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            out += sep;
        out += v[i];
    }
    return out;
}

bool has(const SV &v, const S &s) { return std::find(v.begin(), v.end(), s) != v.end(); }

void add_unique(SV &v, const S &s)
{
    if (!has(v, s))
        v.push_back(s);
}

bool clean(const S &v, bool tabs = false)
{
    if (v.size() > 7800 || v.find("<<<") != S::npos)
        return false;
    for (char c : v)
        if (((unsigned char)c < 0x20 && !(tabs && c == '\t')) || c == 0x7f)
            return false;
    return v.empty() || (v[0] != '}' && v.back() != '{' && v.back() != '\\');
}

bool clean_all(const SV &v)
{
    for (auto &x : v)
        if (!clean(x))
            return false;
    return true;
}

S shown(const S &v)
{
    S out;
    for (char c : v)
    {
        unsigned char u = (unsigned char)c;
        if (u < 0x20 || u == 0x7f)
        {
            char b[8];
            snprintf(b, sizeof(b), "\\x%02x", u);
            out += b;
        }
        else
            out += c;
    }
    return out;
}

S brief(const S &v) { return shown(v.size() > 120 ? v.substr(0, 120) + "..." : v); }

S cert_blocks(const S &pem)
{
    static const S b = "-----BEGIN CERTIFICATE-----", e = "-----END CERTIFICATE-----";
    S out;
    size_t p = 0;
    while ((p = pem.find(b, p)) != S::npos)
    {
        size_t q = pem.find(e, p + b.size());
        if (q == S::npos)
            break;
        S blk = pem.substr(p, q + e.size() - p);
        if (blk.find("-----", b.size()) == q - p)
            out += blk + "\n";
        p = q + e.size();
    }
    return out;
}

bool valid_host(const S &h)
{
    if (h == "*")
        return true;
    if (h.empty() || h.size() > 255)
        return false;
    for (char c : h)
        if (!(isalnum((unsigned char)c) || c == '.' || c == ':' || c == '-' || c == '_'))
            return false;
    return true;
}

std::wstring W(const S &s)
{
    if (s.empty())
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

S U(const std::wstring &w)
{
    if (w.empty())
        return S();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    S s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

bool read_file(const S &path, S &out)
{
    HANDLE h = CreateFileW(W(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    DWORD got = 0;
    if (h == INVALID_HANDLE_VALUE)
        return false;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > 16 * 1024 * 1024)
    {
        CloseHandle(h);
        return false;
    }
    out.assign((size_t)sz.QuadPart, '\0');
    BOOL ok = out.empty() || ReadFile(h, &out[0], (DWORD)out.size(), &got, NULL);
    CloseHandle(h);
    if (!ok)
        return false;
    out.resize(got);
    if (starts(out, "\xEF\xBB\xBF"))
        out.erase(0, 3);
    return true;
}

bool write_file(const S &path, const S &data, bool priv = false)
{
    std::wstring w = W(path), tmp = w + L"." + std::to_wstring(GetCurrentProcessId());
    SECURITY_ATTRIBUTES sa = {sizeof(sa), priv ? ols_private_sd(0) : NULL, FALSE};
    if (priv && !sa.lpSecurityDescriptor)
        return false;
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, priv ? &sa : NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n = 0;
    if (sa.lpSecurityDescriptor)
        LocalFree(sa.lpSecurityDescriptor);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    BOOL ok = data.empty() || (WriteFile(h, data.data(), (DWORD)data.size(), &n, NULL) && n == data.size());
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmp.c_str(), w.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

DWORD attrs(const S &p) { return GetFileAttributesW(W(p).c_str()); }

bool is_dir(const S &p)
{
    DWORD a = attrs(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool is_file(const S &p)
{
    DWORD a = attrs(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool make_dirs(const S &p)
{
    if (p.empty() || is_dir(p))
        return true;
    size_t s = p.find_last_of('/');
    if (s != S::npos && s > 2)
        make_dirs(p.substr(0, s));
    return CreateDirectoryW(W(p).c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool is_drive(const S &p) { return p.size() >= 2 && isalpha((unsigned char)p[0]) && p[1] == ':'; }

S norm(S p)
{
    S prefix;
    SV parts;
    for (char &c : p)
        if (c == '\\')
            c = '/';
    if (starts(p, "//"))
    {
        prefix = "//";
        p = p.substr(2);
    }
    else if (is_drive(p))
    {
        prefix = S(1, (char)toupper((unsigned char)p[0])) + ":/";
        p = p.substr(2);
    }
    else if (starts(p, "/"))
        prefix = "/";
    size_t i = 0;
    while (i <= p.size())
    {
        size_t j = p.find('/', i);
        if (j == S::npos)
            j = p.size();
        S c = p.substr(i, j - i);
        i = j + 1;
        if (c.empty() || c == ".")
            continue;
        if (c == "..")
        {
            if (!parts.empty())
                parts.pop_back();
            continue;
        }
        parts.push_back(c);
    }
    return prefix + join(parts, "/");
}

S resolve(const S &root, const S &p)
{
    S q = p;
    for (char &c : q)
        if (c == '\\')
            c = '/';
    if (is_drive(q) || starts(q, "//"))
        return norm(q);
    if (starts(q, "/"))
        return norm((is_drive(root) ? root.substr(0, 2) : S()) + q);
    return norm(root + "/" + q);
}

S real_path(const S &p)
{
    HANDLE h = CreateFileW(W(p).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    wchar_t buf[4096];
    if (h == INVALID_HANDLE_VALUE)
        return p;
    DWORD n = GetFinalPathNameByHandleW(h, buf, 4096, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (n == 0 || n >= 4096)
        return p;
    std::wstring w(buf, n);
    if (w.compare(0, 8, L"\\\\?\\UNC\\") == 0)
        w = L"\\\\" + w.substr(8);
    else if (w.compare(0, 4, L"\\\\?\\") == 0)
        w = w.substr(4);
    return norm(U(w));
}

S parent_dir(const S &p);
S base_name(const S &p);

S real_file(const S &p)
{
    if (GetFileAttributesW(W(p).c_str()) != INVALID_FILE_ATTRIBUTES)
        return real_path(p);
    S d = parent_dir(p);
    return d.empty() ? p : real_path(d) + "/" + base_name(p);
}

S parent_dir(const S &p)
{
    size_t s = p.find_last_of('/');
    if (s == S::npos)
        return S();
    if (s == 2 && is_drive(p))
        return p.substr(0, 3);
    return p.substr(0, s);
}

S base_name(const S &p)
{
    size_t s = p.find_last_of('/');
    return s == S::npos ? p : p.substr(s + 1);
}

SV comps(const S &p)
{
    SV out;
    size_t i = 0;
    while (i < p.size())
    {
        size_t j = p.find('/', i);
        if (j == S::npos)
            j = p.size();
        if (j > i)
            out.push_back(p.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

bool under(const S &dir, const S &base)
{
    S d = lower(dir), b = lower(base);
    if (d == b)
        return true;
    if (!b.empty() && b.back() != '/')
        b += '/';
    return starts(d, b);
}

S rel(const S &dir, const S &base)
{
    if (dir.size() <= base.size())
        return S();
    S r = dir.substr(base.size());
    while (!r.empty() && r[0] == '/')
        r.erase(0, 1);
    return r;
}

bool wild(const char *p, const char *s)
{
    for (; *p; ++p, ++s)
    {
        if (*p == '*')
        {
            while (p[1] == '*')
                ++p;
            for (const char *t = s;; ++t)
            {
                if (wild(p + 1, t))
                    return true;
                if (!*t || *t == '/')
                    return false;
            }
        }
        if (!*s || *s == '/')
            return false;
        if (*p == '?')
            continue;
        if (*p == '[')
        {
            const char *q = p + 1;
            bool neg = *q == '!' || *q == '^', ok = false;
            if (neg)
                ++q;
            for (; *q && *q != ']'; ++q)
            {
                if (q[1] == '-' && q[2] && q[2] != ']')
                {
                    if (tolower((unsigned char)*s) >= tolower((unsigned char)*q)
                        && tolower((unsigned char)*s) <= tolower((unsigned char)q[2]))
                        ok = true;
                    q += 2;
                }
                else if (tolower((unsigned char)*q) == tolower((unsigned char)*s))
                    ok = true;
            }
            if (!*q || ok == neg)
                return false;
            p = q;
            continue;
        }
        if (*p == '\\' && p[1])
            ++p;
        if (tolower((unsigned char)*p) != tolower((unsigned char)*s))
            return false;
    }
    return !*s;
}

bool has_wild(const S &s) { return s.find_first_of("*?[") != S::npos; }

void list_dir(const S &dir, SV &names, bool dirs)
{
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(W(dir + "/*").c_str(), &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do
    {
        S n = U(fd.cFileName);
        if (n == "." || n == "..")
            continue;
        if (((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == dirs)
            names.push_back(n);
    }
    while (FindNextFileW(f, &fd));
    FindClose(f);
    std::sort(names.begin(), names.end());
}

void files_under(const S &dir, SV &out, int depth)
{
    SV f, d;
    if (depth > 8)
        return;
    list_dir(dir, f, false);
    for (auto &n : f)
        out.push_back(dir + "/" + n);
    list_dir(dir, d, true);
    for (auto &n : d)
        files_under(dir + "/" + n, out, depth + 1);
}

void glob(const S &pattern, SV &out)
{
    S p = norm(pattern);
    size_t w = p.find_first_of("*?[");
    if (w == S::npos)
    {
        if (is_dir(p))
            files_under(p, out, 0);
        else if (is_file(p))
            out.push_back(p);
        return;
    }
    size_t a = p.rfind('/', w);
    if (a == S::npos)
        return;
    size_t b = p.find('/', w);
    S base = p.substr(0, a), comp = p.substr(a + 1, b == S::npos ? S::npos : b - a - 1);
    S rest = b == S::npos ? S() : p.substr(b + 1);
    if (base.size() == 2 && is_drive(base))
        base += "/";
    SV names;
    list_dir(base, names, false);
    if (!rest.empty())
    {
        names.clear();
        list_dir(base, names, true);
    }
    for (auto &n : names)
    {
        if (!wild(comp.c_str(), n.c_str()))
            continue;
        S full = (ends(base, "/") ? base : base + "/") + n;
        if (rest.empty())
            out.push_back(full);
        else
            glob(full + "/" + rest, out);
    }
}

S re_escape(const S &s)
{
    S out;
    for (char c : s)
    {
        if (strchr("\\^$.|?*+()[]{}", c))
            out += '\\';
        out += c;
    }
    return out;
}

S glob_re(const S &g)
{
    S out;
    for (size_t i = 0; i < g.size(); ++i)
    {
        char c = g[i];
        if (c == '*')
            out += "[^/]*";
        else if (c == '?')
            out += "[^/]";
        else if (c == '[')
        {
            size_t e = g.find(']', i + 1);
            if (e == S::npos)
                out += "\\[";
            else
            {
                S cls = g.substr(i + 1, e - i - 1);
                if (!cls.empty() && cls[0] == '!')
                    cls[0] = '^';
                out += "[" + cls + "]";
                i = e;
            }
        }
        else
            out += re_escape(S(1, c));
    }
    return out;
}

int groups(const S &re)
{
    int n = 0;
    bool cls = false;
    for (size_t i = 0; i < re.size(); ++i)
    {
        if (re[i] == '\\')
        {
            ++i;
            continue;
        }
        if (cls)
        {
            if (re[i] == ']')
                cls = false;
            continue;
        }
        if (re[i] == '[')
            cls = true;
        else if (re[i] == '(')
        {
            S nx = re.substr(i + 1, 3);
            if (nx.empty() || nx[0] != '?' || starts(nx, "?P<") || starts(nx, "?'")
                || (starts(nx, "?<") && nx.size() > 2 && nx[2] != '=' && nx[2] != '!'))
                ++n;
        }
    }
    return n;
}

S quote_arg(const S &a)
{
    if (!a.empty() && a.find_first_of(" \t\"") == S::npos)
        return a;
    S out = "\"";
    for (char c : a)
    {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out + "\"";
}

SV split_args(const S &s)
{
    SV out;
    size_t i = 0, n = s.size();
    while (i < n)
    {
        while (i < n && isspace((unsigned char)s[i]))
            ++i;
        if (i >= n)
            break;
        char q = s[i];
        if (q == '"' || q == '\'')
        {
            S buf;
            ++i;
            while (i < n && s[i] != q)
            {
                if (s[i] == '\\' && i + 1 < n && (s[i + 1] == q || s[i + 1] == '\\'))
                    ++i;
                buf += s[i++];
            }
            ++i;
            out.push_back(buf);
        }
        else
        {
            size_t j = i;
            while (j < n && !isspace((unsigned char)s[j]))
                ++j;
            out.push_back(s.substr(i, j - i));
            i = j;
        }
    }
    return out;
}

struct Node
{
    S name, key, file, raw;
    SV args;
    int line = 0;
    bool block = false, unsafe = false;
    S unsafeWhy;
    SV dropped;
    Node *parent = nullptr;
    std::vector<std::unique_ptr<Node>> kids;

    int depth() const
    {
        int d = 0;
        for (const Node *n = parent; n; n = n->parent)
            ++d;
        return d;
    }

    Node *owner()
    {
        Node *n = this;
        while (n->parent && !(n->block && (n->key == "virtualhost" || n->key == "server")))
            n = n->parent;
        return n;
    }

    void taint(const S &why, bool harmless)
    {
        Node *o = owner();
        if (harmless)
            o->dropped.push_back(why + ", not converted");
        else if (!o->unsafe)
        {
            o->unsafe = true;
            o->unsafeWhy = why;
        }
    }

    Node *add(const S &n, const SV &a, const S &r, const S &f, int l, bool b)
    {
        std::unique_ptr<Node> c(new Node());
        c->name = n;
        c->key = lower(n);
        c->args = a;
        c->raw = r;
        c->file = f;
        c->line = l;
        c->block = b;
        c->parent = this;
        kids.push_back(std::move(c));
        return kids.back().get();
    }
    S arg(size_t i) const { return i < args.size() ? args[i] : S(); }
    S where() const { return base_name(file) + ":" + std::to_string(line); }
};

SV module_ids(S a)
{
    a = lower(trim(a));
    if (ends(a, "_module"))
        return SV{a};
    if (ends(a, ".c"))
        a.erase(a.size() - 2);
    if (starts(a, "mod_"))
        a.erase(0, 4);
    return SV{a + "_module", "mod_" + a, a};
}

std::vector<int> version_tuple(const S &v)
{
    std::vector<int> out;
    int cur = -1;
    for (char c : v)
    {
        if (isdigit((unsigned char)c))
            cur = (cur < 0 ? 0 : cur * 10) + (c - '0');
        else if (cur >= 0)
        {
            out.push_back(cur);
            cur = -1;
        }
    }
    if (cur >= 0)
        out.push_back(cur);
    if (out.size() > 3)
        out.resize(3);
    return out;
}

bool is_cond(const S &k)
{
    return k == "ifmodule" || k == "ifdefine" || k == "ifversion" || k == "iffile" || k == "ifdirective"
        || k == "ifsection";
}

struct Parser
{
    S root, version = "2.4.62";
    std::map<S, S> defines;
    std::set<S> mods;
    SV files, missing, badSites;
    std::set<S> chain;
    int budget = 4000;

    Parser()
    {
        for (const char *m : {"core_module", "so_module", "http_module", "win32_module", "mpm_winnt_module", "mod_win32",
                              "mpm_winnt", "mod_so", "core", "http_core", "mod_core"})
            mods.insert(m);
    }

    S expand(const S &s) const
    {
        S out;
        size_t i = 0;
        while (i < s.size())
        {
            if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{')
            {
                size_t e = s.find('}', i + 2);
                if (e != S::npos)
                {
                    S name = s.substr(i + 2, e - i - 2);
                    auto it = defines.find(name);
                    wchar_t buf[2048];
                    DWORD got = 0;
                    if (it != defines.end())
                        out += it->second;
                    else if ((got = GetEnvironmentVariableW(W(name).c_str(), buf, 2048)) > 0 && got < 2048)
                        out += U(buf);
                    else
                        out += s.substr(i, e - i + 1);
                    i = e + 1;
                    continue;
                }
            }
            out += s[i++];
        }
        return out;
    }

    void add_module(const S &name, const S &file)
    {
        S n = lower(name), b = lower(base_name(norm(file)));
        mods.insert(n);
        if (b.find('.') != S::npos)
            b.erase(b.find('.'));
        mods.insert(b);
        if (starts(n, "php"))
            for (const char *m : {"php_module", "php5_module", "php7_module", "php8_module", "mod_php", "mod_php5",
                                  "mod_php7", "mod_php8", "php", "php5", "php7", "php8"})
                mods.insert(m);
    }

    bool cond(const S &key, const SV &args) const
    {
        S a = args.empty() ? S() : args[0];
        bool neg = starts(a, "!");
        if (neg)
            a.erase(0, 1);
        if (key == "ifmodule")
        {
            bool ok = false;
            for (auto &id : module_ids(a))
                ok = ok || mods.count(id);
            return ok != neg;
        }
        if (key == "ifdefine")
            return (defines.count(a) != 0) != neg;
        if (key == "iffile")
            return (attrs(resolve(root, a)) != INVALID_FILE_ATTRIBUTES) != neg;
        if (key == "ifversion")
        {
            SV v = args;
            bool n2 = false;
            if (!v.empty() && starts(v[0], "!"))
            {
                n2 = true;
                v[0].erase(0, 1);
                if (v[0].empty())
                    v.erase(v.begin());
            }
            if (v.empty())
                return true;
            S op = v.size() > 1 ? v[0] : "=", val = v.size() > 1 ? v[1] : v[0];
            if (op == "~" || op == "=~" || (starts(val, "/") && ends(val, "/") && val.size() > 1))
            {
                if (starts(val, "/") && ends(val, "/") && val.size() > 1)
                    val = val.substr(1, val.size() - 2);
                bool r = false;
                try
                {
                    r = std::regex_search(version, std::regex(val));
                }
                catch (...)
                {
                }
                return r != n2;
            }
            std::vector<int> want = version_tuple(val), cur = version_tuple(version);
            cur.resize(want.size(), 0);
            bool r = op == ">" ? cur > want : op == ">=" ? cur >= want : op == "<" ? cur < want
                   : op == "<=" ? cur <= want : cur == want;
            return r != n2;
        }
        return !neg;
    }

    void include(const S &arg, bool optional, Node *parent, int depth)
    {
        if (depth > 16)
        {
            parent->taint(shown(arg) + "  includes nested too deeply", false);
            return;
        }
        S p = resolve(root, arg);
        SV list;
        glob(p, list);
        if (list.empty() && !optional && !has_wild(p))
            missing.push_back(p);
        for (auto &f : list)
            parse(f, parent, depth);
    }

    void parse(const S &path, Node *parent, int depth)
    {
        S key = lower(norm(path)), text;
        if (chain.count(key))
            return;
        if (budget <= 0)
        {
            parent->taint(shown(path) + "  too many included files", false);
            return;
        }
        --budget;
        if (!read_file(path, text))
        {
            missing.push_back(path);
            return;
        }
        files.push_back(norm(path));
        chain.insert(key);
        parse_text(text, norm(path), parent, depth);
        chain.erase(key);
    }

    static bool harmless(const S &k)
    {
        static const char *list[] = {
            "header", "requestheader", "errordocument", "directoryindex", "servername", "serveralias", "serveradmin",
            "customlog", "transferlog", "errorlog", "loglevel", "logformat", "adddefaultcharset", "addcharset",
            "expiresactive", "expiresbytype", "expiresdefault", "fileetag", "browsermatch", "browsermatchnocase",
            "setenvif", "setenvifnocase", "keepalive", "keepalivetimeout", "maxkeepaliverequests", "timeout",
            "servertokens", "serversignature", "protocols", "authname", "define", "addoutputfilterbytype",
            "sslprotocol", "sslciphersuite", "sslhonorcipherorder", "sslsessiontickets", "sslusestapling",
            "sslcompression", "ssloptions", "usecanonicalname", "hostnamelookups", "enablesendfile", "enablemmap",
        };
        for (const char *x : list)
            if (k == x)
                return true;
        return false;
    }

    void parse_text(const S &text, const S &path, Node *parent, int depth)
    {
        struct Frame
        {
            Node *node;
            bool active;
        };
        std::vector<Frame> st{{parent, true}};
        S buf;
        int no = 0, start = 0;
        size_t i = 0;
        while (i <= text.size())
        {
            size_t j = text.find('\n', i);
            if (j == S::npos)
                j = text.size();
            S raw = text.substr(i, j - i);
            i = j + 1;
            ++no;
            while (!raw.empty() && (raw.back() == '\r' || raw.back() == ' ' || raw.back() == '\t'))
                raw.pop_back();
            if (!raw.empty() && raw.back() == '\\')
            {
                if (buf.empty())
                    start = no;
                buf += raw.substr(0, raw.size() - 1) + " ";
                continue;
            }
            S line;
            if (!buf.empty())
            {
                line = buf + raw;
                buf.clear();
            }
            else
            {
                line = raw;
                start = no;
            }
            line = trim(line);
            if (line.empty() || line[0] == '#')
                continue;
            line = expand(line);
            Frame top = st.back();
            S where = base_name(path) + ":" + std::to_string(start);
            if (starts(line, "</"))
            {
                if (st.size() > 1)
                    st.pop_back();
                continue;
            }
            if (line[0] == '<' && line.back() == '>')
            {
                S inner = trim(line.substr(1, line.size() - 2));
                size_t sp = inner.find_first_of(" \t");
                S name = inner.substr(0, sp);
                SV args = sp == S::npos ? SV() : split_args(inner.substr(sp + 1));
                S key = lower(name);
                if (!top.active)
                    st.push_back({nullptr, false});
                else if (is_cond(key))
                    st.push_back({top.node, cond(key, args)});
                else if (!clean(line, true) || !clean_all(args) || top.node->depth() > 48)
                {
                    st.push_back({nullptr, false});
                    if (key == "virtualhost")
                        badSites.push_back(where + "  " + brief(line) + "  contains control characters or <<<");
                    else
                        top.node->taint(where + "  " + brief(line) + "  contains control characters or <<<, or is nested too deeply", false);
                }
                else
                    st.push_back({top.node->add(name, args, line, path, start, true), true});
                continue;
            }
            if (!top.active)
                continue;
            size_t sp = line.find_first_of(" \t");
            S name = line.substr(0, sp);
            SV args = sp == S::npos ? SV() : split_args(line.substr(sp + 1));
            S key = lower(name);
            if (!clean(line, true) || !clean_all(args))
            {
                top.node->taint(where + "  " + brief(line) + "  contains control characters or <<<", harmless(key));
                continue;
            }
            if (key == "include" || key == "includeoptional")
            {
                for (auto &a : args)
                    include(a, key == "includeoptional", top.node, depth + 1);
                continue;
            }
            if (key == "define" && !args.empty())
                defines[args[0]] = args.size() > 1 ? args[1] : S();
            else if (key == "undefine" && !args.empty())
                defines.erase(args[0]);
            else if (key == "serverroot" && !args.empty())
                root = resolve(root, args[0]);
            else if (key == "loadmodule" && args.size() >= 2)
                add_module(args[0], args[1]);
            top.node->add(name, args, line, path, start, false);
        }
    }
};

struct Access
{
    bool denied = false;
    SV allow, deny, except;
    S required;

    bool open() const { return !denied && allow.empty() && deny.empty() && required.empty(); }
    bool operator==(const Access &o) const
    {
        return denied == o.denied && allow == o.allow && deny == o.deny && except == o.except && required == o.required;
    }
};

struct Item
{
    enum { GRANT, DENY, IP, AUTH, UNKNOWN, ALL, ANY, NONE } type = UNKNOWN;
    bool neg = false;
    SV vals;
    S text;
    std::vector<Item> kids;
};

bool ip_token(const S &t)
{
    if (t.empty())
        return false;
    for (char c : t)
        if (!isxdigit((unsigned char)c) && !strchr(":./*[]", c))
            return false;
    return t.find_first_of("0123456789") != S::npos || t.find(':') != S::npos;
}

S ols_ip(S t)
{
    if (starts(t, "[") && t.find(']') != S::npos)
        t = t.substr(1, t.find(']') - 1) + t.substr(t.find(']') + 1);
    if (ends(t, "/32") && t.find(':') == S::npos)
        t.erase(t.size() - 3);
    if (ends(t, "/128"))
        t.erase(t.size() - 4);
    while (ends(t, ".") && t.find(':') == S::npos)
        t.pop_back();
    return t;
}

struct Conf
{
    bool reqSet = false, compatSet = false;
    Access req, compat;
    int authMerge = 0;
    int satisfy = -1;
    SV accNotes, accWarn;
    S authType, authName, userFile, groupFile, provider;
    bool hasAuthType = false, hasAuthName = false, hasUserFile = false, hasGroupFile = false, hasProvider = false;
    std::vector<SV> options;
    int override_ = -1;
    SV index;
    bool indexSet = false;
    SV headers, php, rewrite;
    int rewriteOn = -1;
    std::map<S, S> errors;
    SV notes;
    bool noScript = false, sslRequire = false, cgiAll = false;

    bool access_rules() const { return reqSet || compatSet || satisfy >= 0 || hasAuthType || hasUserFile; }
};

struct Sec
{
    enum { DIR, DIRRE, FILES, FILESRE, LOC, LOCRE, HT } kind;
    const Node *n = nullptr;
    S path;
    bool vh = false;
    int order = 0;
    const Sec *in = nullptr;
    Conf conf;
};

struct Ctx
{
    S uri, location;
    bool regex = false, cgi = false;
    Access acc;
    S realm, authName;
    int autoIndex = -1;
    SV index, headers, php, rewrite;
    S why;
};

struct Proxy
{
    S path, url;
    bool exclude = false;
};

struct Bind
{
    S host = "*";
    int port = 0;
    bool operator==(const Bind &o) const { return host == o.host && port == o.port; }
};

struct Member
{
    const Node *n = nullptr;
    bool ssl = false;
    std::vector<Bind> binds;
    S name;
    SV aliases;
    S docroot, cert, key, chain, admin;
    bool clientCert = false;
    S skip;
};

struct Site
{
    S name, vhname, docroot, file;
    SV domains;
    std::vector<Member> members;
    std::vector<Bind> httpBinds, httpsBinds;
    S cert, key, chain, admin;
    SV converted, notes, warnings, skipped;
    S conf;
    bool imported = false, httpPart = false;
    S skipWhy;
};

struct Listener
{
    S name, host = "*";
    int port = 0;
    bool secure = false;
    bool ours = false;
    S cert, key;
    SV maps;
};

const char *KNOWN[] = {
    "servername", "serveralias", "documentroot", "serveradmin", "errorlog", "customlog", "transferlog", "loglevel",
    "logformat", "sslengine", "sslcertificatefile", "sslcertificatekeyfile", "sslcertificatechainfile", "sslprotocol",
    "sslciphersuite", "sslhonorcipherorder", "sslcompression", "sslsessiontickets", "ssloptions", "sslopensslconfcmd",
    "sslproxyengine", "sslproxyverify", "sslproxycheckpeercn", "sslproxychecckpeername", "sslproxycheckpeername",
    "sslproxycheckpeerexpire", "sslstrictsnivhostcheck", "sslusestapling", "directoryindex", "options",
    "allowoverride", "allowoverridelist", "require", "order", "allow", "deny", "satisfy", "authtype", "authname",
    "authuserfile", "authgroupfile", "authbasicprovider", "alias", "scriptalias", "redirect", "redirectmatch",
    "redirectpermanent", "redirecttemp", "rewriteengine", "rewritecond", "rewriterule", "rewritebase",
    "rewriteoptions", "header", "php_value", "php_flag", "php_admin_value", "php_admin_flag", "errordocument",
    "proxypass", "proxypassreverse", "proxypassreversecookiedomain", "proxypassreversecookiepath",
    "proxypreservehost", "proxyrequests", "keepalive", "keepalivetimeout", "maxkeepaliverequests", "timeout",
    "serversignature", "servertokens", "usecanonicalname", "hostnamelookups", "enablesendfile", "enablemmap",
    "acceptfilter", "adddefaultcharset", "addtype", "addhandler", "sethandler", "fcgidwrapper", "protocols",
    "browsermatch", "setenvif", "setenvifnocase", "requestheader", "phpinidir", "directory", "directorymatch",
    "files", "filesmatch", "location", "locationmatch", "ifmodule", "requireall", "requireany", "requirenone",
    "limitrequestbody", "defaulttype", "fileetag", "mergeslashes", "addoutputfilterbytype", "expiresactive",
    "expiresbytype", "expiresdefault", "addcharset", "proxytimeout", "proxyaddheaders", "limit", "limitexcept",
};

struct Importer
{
    S olsRoot, httpdConf, apacheConf;
    bool dryRun = false;
    Parser parser;
    Node top;
    std::vector<std::unique_ptr<Sec>> mainSecs;
    SV listenPorts;
    std::map<int, SV> listenHosts;
    std::map<int, bool> listenHttps;
    std::map<S, S> logFormats;
    S phpHandler = "lsapi:lsphp", olsPhpDir, apachePhpDir, apachePhpHow;
    SV report, bad;
    int realmSeq = 0, procSeq = 0;
    std::set<S> htSeen;
    S kind = "apache", label = "Apache", listenerBase = "Apache";

    S mark() const { return "# Imported from " + label; }
    S import_begin() const { return "# " + kind + "-import begin"; }
    S import_end() const { return "# " + kind + "-import end"; }
    S maps_begin() const { return "# " + kind + "-import maps begin"; }
    S maps_end() const { return "# " + kind + "-import maps end"; }

    static bool is_known(const S &k)
    {
        for (const char *x : KNOWN)
            if (k == x)
                return true;
        return false;
    }

    Item parse_require(const Node *n)
    {
        Item it;
        SV a = n->args;
        it.text = n->raw;
        if (!a.empty() && ieq(a[0], "not"))
        {
            it.neg = true;
            a.erase(a.begin());
        }
        S t = a.empty() ? S() : lower(a[0]);
        if (t == "all" && a.size() > 1)
            it.type = ieq(a[1], "granted") ? Item::GRANT : ieq(a[1], "denied") ? Item::DENY : Item::UNKNOWN;
        else if (t == "ip" && a.size() > 1)
        {
            it.type = Item::IP;
            for (size_t i = 1; i < a.size(); ++i)
            {
                if (!ip_token(a[i]))
                {
                    it.type = Item::UNKNOWN;
                    break;
                }
                it.vals.push_back(ols_ip(a[i]));
            }
        }
        else if (t == "local")
        {
            it.type = Item::IP;
            it.vals = SV{"127.0.0.1", "::1"};
        }
        else if (t == "valid-user")
        {
            it.type = Item::AUTH;
            it.vals = SV{"valid-user"};
        }
        else if ((t == "user" || t == "group") && a.size() > 1)
        {
            it.type = Item::AUTH;
            SV v(a.begin() + 1, a.end());
            it.vals = SV{t + " " + join(v, " ")};
        }
        return it;
    }

    void collect_items(const Node *n, std::vector<Item> &out, Conf &c)
    {
        for (auto &k : n->kids)
        {
            if (k->key == "require")
                out.push_back(parse_require(k.get()));
            else if (k->key == "requireall" || k->key == "requireany" || k->key == "requirenone")
            {
                Item it;
                it.type = k->key == "requireall" ? Item::ALL : k->key == "requireany" ? Item::ANY : Item::NONE;
                it.text = k->raw;
                collect_items(k.get(), it.kids, c);
                out.push_back(it);
            }
            else if (k->key == "limit" || k->key == "limitexcept")
            {
                size_t before = out.size();
                collect_items(k.get(), out, c);
                if (out.size() != before)
                    add_unique(c.accNotes, k->where() + "  " + k->raw + "  applied to every request method");
            }
        }
    }

    static bool v4net(const S &t, unsigned &net, int &bits)
    {
        S s = t, mask;
        size_t sl = s.find('/');
        if (sl != S::npos)
        {
            mask = s.substr(sl + 1);
            s.erase(sl);
        }
        if (s.empty() || s.find(':') != S::npos)
            return false;
        unsigned parts[4] = {0, 0, 0, 0};
        int n = 0;
        size_t i = 0;
        while (i < s.size() && n < 4)
        {
            if (s[i] == '*')
                break;
            if (!isdigit((unsigned char)s[i]))
                return false;
            unsigned v = 0;
            while (i < s.size() && isdigit((unsigned char)s[i]))
                v = v * 10 + (unsigned)(s[i++] - '0');
            if (v > 255)
                return false;
            parts[n++] = v;
            if (i < s.size())
            {
                if (s[i] != '.')
                    return false;
                ++i;
            }
        }
        if (n == 0)
            return false;
        net = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
        bits = n * 8;
        if (!mask.empty())
        {
            if (mask.find('.') != S::npos)
            {
                unsigned m;
                int mb;
                if (!v4net(mask, m, mb))
                    return false;
                bits = 0;
                while (bits < 32 && (m & (0x80000000u >> bits)))
                    ++bits;
            }
            else
                bits = atoi(mask.c_str());
            if (bits < 0 || bits > 32)
                return false;
        }
        if (bits < 32)
            net &= bits ? ~0u << (32 - bits) : 0u;
        return true;
    }

    static bool covers(const S &outer, const S &inner, bool assumeV6)
    {
        if (ieq(outer, "all") || outer == "*")
            return true;
        unsigned a, b;
        int ab, bb;
        if (v4net(outer, a, ab) && v4net(inner, b, bb))
            return ab <= bb && (ab == 0 || (b & (~0u << (32 - ab))) == a);
        if (outer.find(':') != S::npos && inner.find(':') != S::npos)
            return assumeV6 || lower(outer) == lower(inner);
        return lower(outer) == lower(inner);
    }

    static void drop_covered(Access &r)
    {
        if (r.allow.empty() || r.deny.empty())
            return;
        SV keep;
        for (auto &a : r.allow)
        {
            bool cov = false;
            for (auto &d : r.deny)
                cov = cov || covers(d, a, true);
            if (!cov)
                keep.push_back(a);
        }
        if (keep.empty())
            r.denied = true;
        r.allow = keep;
    }

    static Access and_access(const Access &a, const Access &b, SV *notes)
    {
        Access r;
        r.denied = a.denied || b.denied;
        if (a.allow.empty())
            r.allow = b.allow;
        else if (b.allow.empty())
            r.allow = a.allow;
        else
        {
            for (auto &x : a.allow)
                for (auto &y : b.allow)
                {
                    if (covers(y, x, false))
                        add_unique(r.allow, x);
                    else if (covers(x, y, false))
                        add_unique(r.allow, y);
                }
            if (r.allow.empty())
                r.denied = true;
        }
        r.deny = a.deny;
        for (auto &x : b.deny)
            add_unique(r.deny, x);
        if (r.allow.empty())
            for (int side = 0; side < 2; ++side)
            {
                const Access &own = side ? b : a, &other = side ? a : b;
                for (auto &e : own.except)
                {
                    bool blocked = false;
                    for (auto &d : other.deny)
                        blocked = blocked || covers(d, e, true);
                    if (!blocked)
                        add_unique(r.except, e);
                }
            }
        if (!a.required.empty() && !b.required.empty() && a.required != b.required)
        {
            if (a.required == "valid-user")
                r.required = b.required;
            else if (b.required == "valid-user")
                r.required = a.required;
            else
            {
                r.denied = true;
                if (notes)
                    add_unique(*notes, "two different user requirements must both match, access denied");
            }
        }
        else
            r.required = a.required.empty() ? b.required : a.required;
        drop_covered(r);
        return r;
    }

    Access eval(const Item &grp, SV &notes, SV &warns)
    {
        Access r;
        if (grp.type == Item::NONE)
        {
            add_unique(notes, "RequireNone is not supported, access denied");
            r.denied = true;
            return r;
        }
        if (grp.type == Item::ALL)
        {
            for (auto &it : grp.kids)
            {
                if (it.type == Item::ALL || it.type == Item::ANY || it.type == Item::NONE)
                {
                    r = and_access(r, eval(it, notes, warns), &notes);
                    continue;
                }
                if (it.neg)
                {
                    if (it.type == Item::IP)
                        for (auto &v : it.vals)
                            add_unique(r.deny, v);
                    else if (it.type == Item::GRANT)
                        r.denied = true;
                    else if (it.type != Item::DENY)
                        add_unique(warns, it.text + "  not converted, these clients are not blocked");
                    continue;
                }
                Access t;
                if (it.type == Item::DENY)
                    t.denied = true;
                else if (it.type == Item::IP)
                    t.allow = it.vals;
                else if (it.type == Item::AUTH)
                    t.required = it.vals[0];
                else if (it.type == Item::UNKNOWN)
                {
                    add_unique(notes, it.text + "  not supported, access denied");
                    t.denied = true;
                }
                r = and_access(r, t, &notes);
            }
            drop_covered(r);
            return r;
        }
        if (grp.kids.size() == 1 && !grp.kids[0].neg
            && (grp.kids[0].type == Item::ALL || grp.kids[0].type == Item::ANY || grp.kids[0].type == Item::NONE))
            return eval(grp.kids[0], notes, warns);
        SV ips;
        S auth;
        for (auto &it : grp.kids)
        {
            if (it.neg)
                continue;
            if (it.type == Item::GRANT)
                return Access();
            if (it.type == Item::IP)
                for (auto &v : it.vals)
                    add_unique(ips, v);
            else if (it.type == Item::AUTH)
            {
                if (auth.empty())
                    auth = it.vals[0];
                else if (auth != it.vals[0])
                    add_unique(notes, it.text + "  only the first user requirement is used");
            }
            else if (it.type == Item::ALL || it.type == Item::ANY || it.type == Item::NONE)
            {
                Access a = eval(it, notes, warns);
                if (a.open())
                    return a;
                if (a.denied)
                    continue;
                if (!a.allow.empty() && a.deny.empty() && a.required.empty())
                    for (auto &v : a.allow)
                        add_unique(ips, v);
                else if (a.allow.empty() && a.deny.empty() && !a.required.empty() && auth.empty())
                    auth = a.required;
                else
                    add_unique(notes, it.text + "  not converted, the rule is stricter");
            }
            else if (it.type == Item::UNKNOWN)
                add_unique(notes, it.text + "  not supported, ignored (stricter)");
        }
        if (!ips.empty() && !auth.empty())
        {
            add_unique(notes, "IP address OR password access became password only");
            r.required = auth;
        }
        else if (!ips.empty())
            r.allow = ips;
        else if (!auth.empty())
            r.required = auth;
        else
            r.denied = true;
        return r;
    }

    Access eval_compat(const Node *n, SV &notes, SV &warns, int &satisfy, bool &present)
    {
        S order = "deny,allow";
        bool allowAll = false, denyAll = false;
        SV allowIps, denyIps;
        std::vector<const Node *> nodes;
        for (auto &k : n->kids)
        {
            if (k->key == "limit" || k->key == "limitexcept")
            {
                bool used = false;
                for (auto &x : k->kids)
                    if (x->key == "order" || x->key == "allow" || x->key == "deny" || x->key == "satisfy")
                    {
                        nodes.push_back(x.get());
                        used = true;
                    }
                if (used)
                    add_unique(notes, k->where() + "  " + k->raw + "  applied to every request method");
            }
            else
                nodes.push_back(k.get());
        }
        for (const Node *k : nodes)
        {
            if (k->key == "order" && !k->args.empty())
            {
                order = lower(join(k->args, ""));
                present = true;
            }
            else if ((k->key == "allow" || k->key == "deny") && k->args.size() >= 2 && ieq(k->args[0], "from"))
            {
                present = true;
                bool al = k->key == "allow";
                for (size_t i = 1; i < k->args.size(); ++i)
                {
                    S v = k->args[i];
                    if (ieq(v, "all"))
                        (al ? allowAll : denyAll) = true;
                    else if (ip_token(v))
                        add_unique(al ? allowIps : denyIps, ols_ip(v));
                    else if (al)
                        add_unique(notes, k->where() + "  " + k->raw + "  not converted (stricter)");
                    else
                        add_unique(warns, k->where() + "  " + k->raw + "  not converted, these clients are not blocked");
                }
            }
            else if (k->key == "satisfy" && !k->args.empty())
                satisfy = ieq(k->args[0], "any") ? 1 : 0;
        }
        Access r;
        if (!present)
            return r;
        if (order == "deny,allow")
        {
            if (denyAll)
            {
                if (allowAll)
                    return r;
                if (allowIps.empty())
                    r.denied = true;
                else
                    r.allow = allowIps;
            }
            else
            {
                r.deny = denyIps;
                for (auto &a : allowIps)
                    for (auto &d : denyIps)
                        if (covers(d, a, false))
                            add_unique(r.except, a);
            }
        }
        else if (allowAll)
        {
            if (denyAll)
                r.denied = true;
            else
                r.deny = denyIps;
        }
        else if (allowIps.empty() || denyAll)
            r.denied = true;
        else
        {
            r.allow = allowIps;
            r.deny = denyIps;
            drop_covered(r);
        }
        return r;
    }

    static bool disables_php(const Node *n)
    {
        for (auto &k : n->kids)
        {
            const S &key = k->key;
            S a0 = lower(k->arg(0)), a1 = lower(k->arg(1));
            if (key == "sethandler" && (a0 == "none" || a0 == "default-handler"))
                return true;
            if ((key == "removehandler" || key == "removetype") && lower(join(k->args, " ")).find("php") != S::npos)
                return true;
            if ((key == "php_flag" || key == "php_admin_flag" || key == "php_value" || key == "php_admin_value")
                && a0 == "engine" && (a1 == "off" || a1 == "0"))
                return true;
            if (k->block && (key == "files" || key == "filesmatch") && disables_php(k.get()))
                return true;
        }
        return false;
    }

    static bool risky(const Node *n)
    {
        for (auto &k : n->kids)
        {
            const S &key = k->key;
            if (key == "require" || key == "requireall" || key == "requireany" || key == "requirenone" || key == "order"
                || key == "allow" || key == "deny" || key == "satisfy" || key == "authtype" || key == "authuserfile"
                || key == "sslrequiressl" || key == "sslverifyclient" || key == "sslrequire")
                return true;
            if (k->block && risky(k.get()))
                return true;
        }
        return false;
    }

    void section_conf(const Node *n, Conf &c, bool dirLevel)
    {
        if (dirLevel && disables_php(n))
            c.noScript = true;
        std::vector<Item> items;
        collect_items(n, items, c);
        if (!items.empty())
        {
            Item any;
            any.type = Item::ANY;
            any.kids = items;
            c.req = eval(any, c.accNotes, c.accWarn);
            c.reqSet = true;
        }
        bool present = false;
        int sat = -1;
        Access old = eval_compat(n, c.accNotes, c.accWarn, sat, present);
        if (present)
        {
            c.compat = old;
            c.compatSet = true;
        }
        if (sat >= 0)
            c.satisfy = sat;
        for (auto &k : n->kids)
        {
            const S &key = k->key;
            const SV &a = k->args;
            if (key == "authtype" && !a.empty())
                c.authType = lower(a[0]), c.hasAuthType = true;
            else if (key == "authname" && !a.empty())
                c.authName = a[0], c.hasAuthName = true;
            else if (key == "authuserfile" && !a.empty())
                c.userFile = real_file(resolve(parser.root, a[0])), c.hasUserFile = true;
            else if (key == "authgroupfile" && !a.empty())
                c.groupFile = real_file(resolve(parser.root, a[0])), c.hasGroupFile = true;
            else if (key == "authbasicprovider" && !a.empty())
                c.provider = lower(a[0]), c.hasProvider = true;
            else if (key == "authmerging" && !a.empty())
                c.authMerge = ieq(a[0], "and") ? 1 : ieq(a[0], "or") ? 2 : 0;
            else if (key == "sslrequiressl")
                c.sslRequire = true;
            else if (key == "sethandler" && !a.empty() && ieq(a[0], "cgi-script"))
            {
                c.cgiAll = true;
                if (n->key == "files" || n->key == "filesmatch")
                {
                    c.reqSet = true;
                    c.req = Access();
                    c.req.denied = true;
                    add_unique(c.accNotes, k->where() + "  " + k->raw + "  CGI by file name is not converted, access denied");
                }
            }
            else if (key == "options")
                c.options.push_back(a);
            else if (key == "allowoverride" && dirLevel && !a.empty())
                c.override_ = ieq(a[0], "none") ? 0 : 1;
            else if (key == "directoryindex")
            {
                c.index.clear();
                for (auto &x : a)
                    if (!ieq(x, "disabled"))
                        c.index.push_back(x);
                c.indexSet = true;
            }
            else if (key == "header")
                header_line(k.get(), c);
            else if (key == "php_value" || key == "php_flag" || key == "php_admin_value" || key == "php_admin_flag")
            {
                if (a.size() >= 2)
                    c.php.push_back(k->name + " " + a[0] + " " + quote_arg(join(SV(a.begin() + 1, a.end()), " ")));
            }
            else if (key == "rewriteengine" && !a.empty())
                c.rewriteOn = ieq(a[0], "on") ? 1 : 0;
            else if (key == "rewritecond" || key == "rewriterule")
                c.rewrite.push_back(k->raw);
            else if (key == "errordocument" && a.size() >= 2)
            {
                if (a[0].size() == 3 && a[0].find_first_not_of("0123456789") == S::npos)
                    c.errors[a[0]] = a[1];
                else
                    c.notes.push_back(k->where() + "  " + k->raw + "  not converted");
            }
            else if (key == "if" || key == "elseif" || key == "else")
            {
                if (risky(k.get()))
                {
                    c.reqSet = true;
                    c.req = Access();
                    c.req.denied = true;
                    add_unique(c.accNotes, k->where() + "  " + k->raw + "  access rules inside <If> are not supported, access denied");
                }
                else
                    c.notes.push_back(k->where() + "  " + k->raw + "  not converted");
            }
        }
    }

    void header_line(const Node *k, Conf &c)
    {
        SV a = k->args;
        if (!a.empty() && (ieq(a[0], "always") || ieq(a[0], "onsuccess")))
            a.erase(a.begin());
        if (a.size() < 2)
            return;
        S act = lower(a[0]);
        bool cond = false;
        for (auto &x : a)
            if (istarts(x, "env=") || istarts(x, "expr=") || ieq(x, "early"))
                cond = true;
        if (cond || !(act == "set" || act == "append" || act == "add" || act == "merge" || act == "unset"))
        {
            c.notes.push_back(k->where() + "  " + k->raw + "  not converted");
            return;
        }
        S line = "Header " + act + " " + a[1];
        if (act != "unset" && a.size() >= 3)
            line += " " + quote_arg(a[2]);
        c.headers.push_back(line);
    }

    static void merge(Conf &d, const Conf &s)
    {
        if (s.reqSet)
        {
            if (s.authMerge == 1 && d.reqSet)
                d.req = and_access(d.req, s.req, &d.accNotes);
            else
                d.req = s.req;
            d.reqSet = true;
        }
        if (s.compatSet)
        {
            d.compat = s.compat;
            d.compatSet = true;
        }
        if (s.satisfy >= 0)
            d.satisfy = s.satisfy;
        for (auto &n : s.accNotes)
            add_unique(d.accNotes, n);
        for (auto &n : s.accWarn)
            add_unique(d.accWarn, n);
        if (s.hasAuthType)
            d.authType = s.authType, d.hasAuthType = true;
        if (s.hasAuthName)
            d.authName = s.authName, d.hasAuthName = true;
        if (s.hasUserFile)
            d.userFile = s.userFile, d.hasUserFile = true;
        if (s.hasGroupFile)
            d.groupFile = s.groupFile, d.hasGroupFile = true;
        if (s.hasProvider)
            d.provider = s.provider, d.hasProvider = true;
        for (auto &o : s.options)
            d.options.push_back(o);
        if (s.override_ >= 0)
            d.override_ = s.override_;
        if (s.indexSet)
            d.index = s.index, d.indexSet = true;
        for (auto &h : s.headers)
            d.headers.push_back(h);
        for (auto &p : s.php)
        {
            S key = p.substr(0, p.find(' ', p.find(' ') + 1));
            d.php.erase(std::remove_if(d.php.begin(), d.php.end(),
                                       [&](const S &x) { return x.substr(0, x.find(' ', x.find(' ') + 1)) == key; }),
                        d.php.end());
            d.php.push_back(p);
        }
        if (s.rewriteOn >= 0 || !s.rewrite.empty())
        {
            d.rewriteOn = s.rewriteOn >= 0 ? s.rewriteOn : d.rewriteOn;
            if (!s.rewrite.empty())
                d.rewrite = s.rewrite;
        }
        for (auto &e : s.errors)
            d.errors[e.first] = e.second;
        for (auto &n : s.notes)
            d.notes.push_back(n);
        d.noScript = d.noScript || s.noScript;
        d.sslRequire = d.sslRequire || s.sslRequire;
        d.cgiAll = d.cgiAll || s.cgiAll;
    }

    static int indexes(const Conf &c) { return option(c, "indexes"); }

    static int option(const Conf &c, const S &name)
    {
        int on = 0;
        for (auto &list : c.options)
        {
            bool absolute = false;
            for (auto &t : list)
                if (!starts(t, "+") && !starts(t, "-"))
                    absolute = true;
            if (absolute)
            {
                on = 0;
                for (auto &t : list)
                    if (ieq(t, name) || ieq(t, "all"))
                        on = 1;
            }
            else
                for (auto &t : list)
                {
                    if (ieq(t, "+" + name))
                        on = 1;
                    else if (ieq(t, "-" + name))
                        on = 0;
                }
        }
        return on;
    }

    std::regex make_re(const S &re, bool icase, bool &ok)
    {
        ok = true;
        try
        {
            return std::regex(re, icase ? std::regex::ECMAScript | std::regex::icase : std::regex::ECMAScript);
        }
        catch (...)
        {
            ok = false;
            return std::regex();
        }
    }

    bool dir_match(const S &pat, const S &dir)
    {
        if (pat == "/")
            return true;
        SV p = comps(lower(pat)), d = comps(lower(dir));
        if (p.size() > d.size())
            return false;
        for (size_t i = 0; i < p.size(); ++i)
            if (!wild(p[i].c_str(), d[i].c_str()))
                return false;
        return true;
    }

    bool dirre_match(const Sec *s, const S &dir)
    {
        bool ok;
        std::regex re = make_re(s->path, true, ok);
        return ok && (std::regex_search(dir, re) || std::regex_search(dir + "/", re));
    }

    bool loc_match(const S &pat, const S &uri)
    {
        if (pat == "/" || pat.empty())
            return true;
        if (!starts(uri, pat))
            return false;
        return ends(pat, "/") || uri.size() == pat.size() || uri[pat.size()] == '/';
    }

    void collect_secs(const Node *scope, bool vh, std::vector<std::unique_ptr<Sec>> &out, int &order, const Sec *in)
    {
        for (auto &k : scope->kids)
        {
            const S &key = k->key;
            if (key == "virtualhost" || key == "ifmodule" || key == "if" || key == "else" || key == "elseif")
                continue;
            std::unique_ptr<Sec> s(new Sec());
            s->n = k.get();
            s->vh = vh;
            s->order = order++;
            s->in = in;
            S a0 = k->arg(0);
            bool re = a0 == "~" && k->args.size() > 1;
            S pat = re ? k->arg(1) : a0;
            if (key == "directory")
                s->kind = re ? Sec::DIRRE : Sec::DIR;
            else if (key == "directorymatch")
                s->kind = Sec::DIRRE;
            else if (key == "files")
                s->kind = re ? Sec::FILESRE : Sec::FILES;
            else if (key == "filesmatch")
                s->kind = Sec::FILESRE;
            else if (key == "location")
                s->kind = re ? Sec::LOCRE : Sec::LOC;
            else if (key == "locationmatch")
                s->kind = Sec::LOCRE;
            else
                continue;
            if (s->kind == Sec::DIR)
                s->path = pat == "/" ? S("/") : has_wild(pat) ? resolve(parser.root, pat) : real_path(resolve(parser.root, pat));
            else if (s->kind == Sec::LOC)
            {
                s->path = pat;
                while (s->path.size() > 1 && ends(s->path, "//"))
                    s->path.pop_back();
            }
            else
                s->path = pat;
            section_conf(k.get(), s->conf, s->kind == Sec::DIR || s->kind == Sec::DIRRE);
            Sec *raw = s.get();
            out.push_back(std::move(s));
            if (raw->kind == Sec::DIR || raw->kind == Sec::DIRRE)
                collect_secs(k.get(), vh, out, order, raw);
        }
    }

    struct VhState
    {
        const Node *vnode = nullptr;
        Conf base;
        std::vector<Sec *> secs;
        std::vector<std::unique_ptr<Sec>> own, ht;
        std::set<S> htLoaded;
    };

    Conf server_conf(const Node *scope)
    {
        Node holder;
        for (auto &k : scope->kids)
        {
            const S &key = k->key;
            if (key == "options" || key == "directoryindex" || key == "header" || istarts(key, "php_")
                || key == "errordocument" || key == "if" || key == "elseif" || key == "else")
                holder.kids.push_back(clone(k.get()));
        }
        Conf c;
        section_conf(&holder, c, false);
        return c;
    }

    static std::unique_ptr<Node> clone(const Node *n)
    {
        std::unique_ptr<Node> c(new Node());
        c->name = n->name;
        c->key = n->key;
        c->file = n->file;
        c->raw = n->raw;
        c->args = n->args;
        c->line = n->line;
        c->block = n->block;
        for (auto &k : n->kids)
        {
            c->kids.push_back(clone(k.get()));
            c->kids.back()->parent = c.get();
        }
        return c;
    }

    Conf dir_conf(VhState &st, const S &dir)
    {
        Conf c = st.base;
        std::vector<Sec *> plain, regex, hts;
        for (auto *s : st.secs)
        {
            if (s->in || dir.empty())
                continue;
            if (s->kind == Sec::DIR && dir_match(s->path, dir))
                plain.push_back(s);
            else if (s->kind == Sec::DIRRE)
                regex.push_back(s);
        }
        for (auto &h : st.ht)
            if (h->kind == Sec::HT && !dir.empty() && under(dir, h->path))
                hts.push_back(h.get());
        auto depth = [](const Sec *s) { return s->path == "/" ? 0 : (int)comps(s->path).size(); };
        std::stable_sort(plain.begin(), plain.end(), [&](const Sec *a, const Sec *b) {
            return depth(a) != depth(b) ? depth(a) < depth(b) : a->order < b->order;
        });
        std::stable_sort(hts.begin(), hts.end(), [&](const Sec *a, const Sec *b) { return depth(a) < depth(b); });
        size_t hi = 0;
        for (size_t i = 0; i <= plain.size(); ++i)
        {
            int d = i < plain.size() ? depth(plain[i]) : 1 << 30;
            while (hi < hts.size() && depth(hts[hi]) < d)
            {
                if (c.override_ == 1)
                    merge(c, hts[hi]->conf);
                ++hi;
            }
            if (i < plain.size())
                merge(c, plain[i]->conf);
        }
        for (auto *s : regex)
            if (dirre_match(s, dir))
                merge(c, s->conf);
        return c;
    }

    void apply_locations(VhState &st, Conf &c, const S &uri)
    {
        for (auto *s : st.secs)
        {
            if (s->kind == Sec::LOC && loc_match(s->path, uri))
                merge(c, s->conf);
            else if (s->kind == Sec::LOCRE)
            {
                bool ok;
                std::regex re = make_re(s->path, false, ok);
                if (ok && std::regex_search(uri, re))
                    merge(c, s->conf);
            }
        }
    }

    Conf effective(VhState &st, const S &dir, const S &uri, bool withLoc)
    {
        Conf c = dir_conf(st, dir);
        if (withLoc)
            apply_locations(st, c, uri);
        return c;
    }

    Access final_access(const Conf &c, SV &notes, SV &warns)
    {
        Access r = c.reqSet ? c.req : Access(), k = c.compatSet ? c.compat : Access(), a;
        if (c.satisfy == 1 && c.reqSet)
        {
            if (r.open() || k.open())
                a = Access();
            else if (!r.denied && !r.required.empty() && r.allow.empty() && r.deny.empty())
            {
                a = r;
                add_unique(notes, "Satisfy Any: IP address OR password access became password only");
            }
            else
            {
                a = and_access(r, k, &notes);
                add_unique(notes, "Satisfy Any: both rules are required");
            }
        }
        else
            a = and_access(r, k, &notes);
        for (auto &n : c.accNotes)
            add_unique(notes, n);
        for (auto &n : c.accWarn)
            add_unique(warns, n);
        if (!a.required.empty() && !a.denied)
        {
            if (c.authType != "basic")
            {
                add_unique(notes, "authentication type '" + c.authType + "' is not supported, access denied");
                a.denied = true;
            }
            else if (!c.provider.empty() && c.provider != "file")
            {
                add_unique(notes, "AuthBasicProvider " + c.provider + " is not supported, access denied");
                a.denied = true;
            }
            else if (c.userFile.empty() || !is_file(c.userFile))
            {
                add_unique(notes, "password file " + (c.userFile.empty() ? S("(none)") : c.userFile)
                                      + " not found, access denied");
                a.denied = true;
            }
        }
        return a;
    }

    S realm_for(const Conf &c, std::map<S, SV> &realms, Site &site)
    {
        S key = lower(c.userFile) + "|" + lower(c.groupFile);
        auto it = realms.find(key);
        if (it != realms.end())
            return it->second[0];
        S name = kind + std::to_string(++realmSeq);
        realms[key] = SV{name, c.userFile, c.groupFile};
        S text;
        if (read_file(c.userFile, text))
        {
            size_t i = 0;
            while (i < text.size())
            {
                size_t j = text.find('\n', i);
                if (j == S::npos)
                    j = text.size();
                S line = trim(text.substr(i, j - i));
                i = j + 1;
                size_t colon = line.find(':');
                if (line.empty() || line[0] == '#' || colon == S::npos)
                    continue;
                S h = line.substr(colon + 1);
                if (!(starts(h, "$apr1$") || istarts(h, "{sha}") || starts(h, "$2y$") || starts(h, "$2a$")
                      || starts(h, "$2b$") || h.size() == 13))
                {
                    add_unique(site.warnings, c.userFile + " has passwords stored as plain text; "
                                              "those users cannot log in until they get a hashed password");
                    break;
                }
            }
        }
        return name;
    }

    S indent_block(const S &head, const SV &lines, int ind)
    {
        S pad(ind, ' ');
        S out = pad + head + " {\n";
        for (auto &l : lines)
            out += pad + "  " + l + "\n";
        return out + pad + "}\n";
    }

    static S line(const S &k, const S &v, int ind)
    {
        S pad(ind, ' ');
        S key = k;
        if (key.size() < 24)
            key += S(24 - key.size(), ' ');
        return pad + key + v + "\n";
    }

    S kv(const S &k, const S &v, int ind)
    {
        if (!clean(v))
        {
            add_unique(bad, k + " " + brief(v));
            return S();
        }
        return line(k, v, ind);
    }

    S head(const S &k, const S &v)
    {
        if (!clean(v))
        {
            add_unique(bad, k + " " + brief(v));
            return k + " {\n";
        }
        return k + " " + v + " {\n";
    }

    S heredoc(const S &k, const SV &lines, int ind)
    {
        S pad(ind, ' ');
        S tag = "END_" + k;
        S out = line(k, "<<<" + tag, ind);
        for (auto &l : lines)
        {
            if (!clean(l, true) || ieq(trim(l).substr(0, tag.size()), tag))
            {
                add_unique(bad, k + " " + brief(l));
                continue;
            }
            out += l + "\n";
        }
        return out + pad + tag + "\n";
    }

    S render_access(const Access &a, const S &realm, const S &authName, int ind)
    {
        S out;
        if (a.denied)
            return kv("allowBrowse", "0", ind);
        out += kv("allowBrowse", "1", ind);
        S pad(ind, ' ');
        out += pad + "accessControl {\n";
        if (a.allow.empty())
        {
            out += kv("allow", a.except.empty() ? S("ALL") : "ALL, " + join(a.except, ", "), ind + 2);
            if (!a.deny.empty())
                out += kv("deny", join(a.deny, ", "), ind + 2);
        }
        else
        {
            out += kv("allow", join(a.allow, ", "), ind + 2);
            SV d = a.deny;
            d.push_back("ALL");
            out += kv("deny", join(d, ", "), ind + 2);
        }
        out += pad + "}\n";
        if (!a.required.empty() && !realm.empty())
        {
            out += kv("realm", realm, ind);
            if (!authName.empty())
                out += kv("authName", authName, ind);
            out += kv("required", a.required, ind);
        }
        return out;
    }

    S render_ctx(const Ctx &c)
    {
        S out = head("context", c.uri);
        if (c.cgi)
            out += kv("type", "cgi", 2);
        if (!c.location.empty())
            out += kv("location", c.location, 2);
        out += render_access(c.acc, c.realm, c.authName, 2);
        if (c.autoIndex >= 0)
            out += kv("autoIndex", std::to_string(c.autoIndex), 2);
        if (!c.index.empty())
            out += kv("indexFiles", join(c.index, ", "), 2);
        if (!c.headers.empty())
            out += heredoc("extraHeaders", c.headers, 2);
        if (!c.php.empty())
        {
            out += "  phpIniOverride  {\n";
            for (auto &p : c.php)
                if (clean(p) && !istarts(p, "include"))
                    out += p + "\n";
                else
                    add_unique(bad, "phpIniOverride " + brief(p));
            out += "  }\n";
        }
        if (!c.rewrite.empty())
        {
            out += "  rewrite  {\n";
            out += kv("enable", "1", 4);
            out += heredoc("rules", c.rewrite, 4);
            out += "  }\n";
        }
        return out + "}\n";
    }

    bool sample_name(const S &n)
    {
        return n == "www.example.com" || n == "example.com" || n == "dummy-host.example.com"
            || n == "dummy-host2.example.com";
    }

    S clean_domain(S d)
    {
        d = lower(trim(d));
        size_t p = d.find("://");
        if (p != S::npos)
            d = d.substr(p + 3);
        if (!d.empty() && d[0] != '[')
        {
            size_t c = d.find(':');
            if (c != S::npos)
                d.erase(c);
        }
        while (ends(d, "."))
            d.pop_back();
        return d;
    }

    bool valid_domain(const S &d)
    {
        if (d.empty() || d.size() > 253)
            return false;
        S body = starts(d, "*.") ? d.substr(2) : d;
        for (char c : body)
            if (!(isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_'))
                return false;
        return !body.empty();
    }

    static S bind_host(S h)
    {
        if (starts(h, "[") && ends(h, "]"))
            h = h.substr(1, h.size() - 2);
        h = lower(h);
        if (h.empty() || h == "*" || h == "_default_" || h == "0.0.0.0" || h == "::")
            return "*";
        return h;
    }

    static void split_addr(const S &a, S &host, int &port)
    {
        host = "*";
        port = 0;
        if (starts(a, "["))
        {
            size_t e = a.find(']');
            if (e == S::npos)
                return;
            host = bind_host(a.substr(0, e + 1));
            if (e + 1 < a.size() && a[e + 1] == ':')
                port = atoi(a.c_str() + e + 2);
            return;
        }
        size_t c = a.rfind(':');
        if (c != S::npos && a.find(':') == c)
        {
            host = bind_host(a.substr(0, c));
            port = atoi(a.c_str() + c + 1);
        }
        else if (c == S::npos && !a.empty() && isdigit((unsigned char)a[0]) && a.find('.') == S::npos)
            port = atoi(a.c_str());
        else
            host = bind_host(a);
    }

    void member_from(const Node *v, Member &m)
    {
        m.n = v;
        for (auto &addr : v->args)
        {
            S host;
            int port;
            split_addr(addr, host, port);
            if (!valid_host(host) || port < 0 || port > 65535)
            {
                m.skip = "address " + brief(addr) + " is not valid";
                continue;
            }
            std::vector<int> ports;
            if (port > 0)
                ports.push_back(port);
            else
                for (auto &l : listenHosts)
                    ports.push_back(l.first);
            for (int p : ports)
            {
                SV hosts;
                if (host != "*")
                    hosts.push_back(host);
                else if (listenHosts.count(p))
                    hosts = listenHosts[p];
                else
                    hosts.push_back("*");
                for (auto &h : hosts)
                {
                    Bind b;
                    b.host = h;
                    b.port = p;
                    if (std::find(m.binds.begin(), m.binds.end(), b) == m.binds.end())
                        m.binds.push_back(b);
                }
            }
        }
        for (auto &k : v->kids)
        {
            const S &key = k->key;
            const SV &a = k->args;
            if (key == "servername" && !a.empty())
                m.name = clean_domain(a[0]);
            else if (key == "serveralias")
                for (auto &x : a)
                    m.aliases.push_back(clean_domain(x));
            else if (key == "documentroot" && !a.empty())
                m.docroot = real_path(resolve(parser.root, a[0]));
            else if (key == "sslengine" && !a.empty())
                m.ssl = ieq(a[0], "on");
            else if (key == "sslcertificatefile" && !a.empty())
                m.cert = real_file(resolve(parser.root, a[0]));
            else if (key == "sslcertificatekeyfile" && !a.empty())
                m.key = real_file(resolve(parser.root, a[0]));
            else if (key == "sslcertificatechainfile" && !a.empty())
                m.chain = real_file(resolve(parser.root, a[0]));
            else if (key == "serveradmin" && !a.empty())
                m.admin = a[0];
        }
        m.clientCert = client_cert(v);
    }

    static bool client_cert(const Node *scope)
    {
        for (auto &k : scope->kids)
        {
            if (k->key == "virtualhost")
                continue;
            if ((k->key == "sslverifyclient" && !k->args.empty() && !ieq(k->args[0], "none")) || k->key == "sslrequire")
                return true;
            if (k->block && client_cert(k.get()))
                return true;
        }
        return false;
    }

    S main_value(const char *key)
    {
        S v;
        for (auto &k : top.kids)
            if (k->key == key && !k->args.empty())
                v = k->args[0];
        return v;
    }

    void walk(const S &dir, int depth, VhState &st, int &count, SV &dirs)
    {
        if (depth > 16 || count > 30000)
            return;
        ++count;
        dirs.push_back(dir);
        WIN32_FIND_DATAW fd;
        HANDLE f = FindFirstFileW(W(dir + "/*").c_str(), &fd);
        if (f == INVALID_HANDLE_VALUE)
            return;
        SV subs;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                continue;
            S n = U(fd.cFileName);
            if (n == "." || n == "..")
                continue;
            if (n == ".git" || n == "node_modules" || n == ".svn")
            {
                dirs.push_back(dir + "/" + n);
                continue;
            }
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && !htSeen.insert(lower(real_path(dir + "/" + n))).second)
                continue;
            subs.push_back(n);
        }
        while (FindNextFileW(f, &fd));
        FindClose(f);
        std::sort(subs.begin(), subs.end());
        for (auto &n : subs)
            walk(dir + "/" + n, depth + 1, st, count, dirs);
    }

    bool redirects_everything(const Node *v)
    {
        for (auto &k : v->kids)
        {
            if ((k->key == "redirect" || k->key == "redirectpermanent" || k->key == "redirecttemp"))
            {
                SV a = k->args;
                if (k->key == "redirect" && a.size() == 3)
                    a.erase(a.begin());
                if (a.size() == 2 && a[0] == "/")
                    return true;
            }
            if (k->key == "redirectmatch")
            {
                SV a = k->args;
                if (a.size() == 3)
                    a.erase(a.begin());
                if (!a.empty() && (a[0] == "^" || a[0] == "^/" || a[0] == "^(.*)$" || a[0] == "(.*)" || a[0] == "^/(.*)$" || a[0] == ".*"))
                    return true;
            }
            if (k->key == "rewriterule" && !k->args.empty())
            {
                S p = k->args[0];
                S f = k->args.size() > 2 ? lower(k->args[2]) : S();
                if ((p == "^" || p == "^(.*)$" || p == "(.*)" || p == "^/?(.*)$" || p == "^/(.*)$" || p == ".*"
                     || p == "^(.*)" || p == "^/?(.*)")
                    && f.find("r") != S::npos)
                    return true;
            }
        }
        return false;
    }

    SV redirect_rules(const Node *scope, SV &notes)
    {
        SV out;
        for (auto &k : scope->kids)
        {
            const S &key = k->key;
            if (key != "redirect" && key != "redirectmatch" && key != "redirectpermanent" && key != "redirecttemp")
                continue;
            SV a = k->args;
            S status = key == "redirectpermanent" ? "301" : "302";
            if (key == "redirect" || key == "redirectmatch")
            {
                if (!a.empty())
                {
                    S s = lower(a[0]);
                    if (s == "permanent")
                        status = "301", a.erase(a.begin());
                    else if (s == "temp")
                        status = "302", a.erase(a.begin());
                    else if (s == "seeother")
                        status = "303", a.erase(a.begin());
                    else if (s == "gone")
                        status = "410", a.erase(a.begin());
                    else if (s.size() == 3 && isdigit((unsigned char)s[0]))
                        status = s, a.erase(a.begin());
                }
            }
            if (a.empty() || (status != "410" && a.size() < 2))
            {
                notes.push_back(k->where() + "  " + k->raw + "  not converted");
                continue;
            }
            S target = a.size() > 1 ? a[1] : S();
            S flags = status == "410" ? "[G,L]" : "[R=" + status + ",L" + (target.find('%') != S::npos ? ",NE" : "") + "]";
            if (key == "redirectmatch")
            {
                out.push_back("RewriteRule " + quote_arg(a[0]) + " " + (status == "410" ? S("-") : quote_arg(target)) + " " + flags);
                continue;
            }
            S path = a[0];
            S re = "^" + re_escape(path) + (ends(path, "/") ? "(.*)$" : "(/.*)?$");
            if (status == "410")
                out.push_back("RewriteRule " + quote_arg(re) + " - " + flags);
            else
            {
                S t;
                for (char ch : target)
                {
                    if (ch == '$' || ch == '%')
                        t += '\\';
                    t += ch;
                }
                out.push_back("RewriteRule " + quote_arg(re) + " " + quote_arg(t + "$1") + " " + flags);
            }
        }
        return out;
    }

    SV rewrite_rules(const Node *scope, bool &inherit, bool &before, SV &notes)
    {
        SV out;
        bool on = false;
        inherit = before = false;
        for (auto &k : scope->kids)
        {
            if (k->key == "rewriteengine" && !k->args.empty())
                on = ieq(k->args[0], "on");
            else if (k->key == "rewriteoptions")
                for (auto &x : k->args)
                {
                    if (ieq(x, "inherit"))
                        inherit = true;
                    else if (ieq(x, "inheritbefore"))
                        inherit = before = true;
                }
            else if (k->key == "rewritemap")
                notes.push_back(k->where() + "  " + k->raw + "  not converted");
            else if (k->key == "rewritecond" || k->key == "rewriterule")
            {
                if (k->raw.find("${") != S::npos)
                    notes.push_back(k->where() + "  " + k->raw + "  uses a RewriteMap, not converted");
                out.push_back(k->raw);
            }
        }
        if (!on)
            out.clear();
        return out;
    }

    SV with_cond(const SV &rules, const S &cond)
    {
        SV out;
        bool start = true;
        for (auto &r : rules)
        {
            bool isRule = istarts(trim(r), "rewriterule");
            if (start)
                out.push_back(cond);
            out.push_back(r);
            start = isRule;
        }
        return out;
    }

    void proxies(const Node *scope, std::vector<Proxy> &out, SV &notes)
    {
        for (auto &k : scope->kids)
        {
            if (k->key == "proxypassreverse" || k->key == "proxypassreversecookiedomain"
                || k->key == "proxypassreversecookiepath")
                notes.push_back(k->where() + "  " + k->raw + "  not converted, the backend must send correct redirects");
            if (k->key != "proxypass" || k->args.size() < 2)
                continue;
            Proxy p;
            p.path = k->args[0];
            p.url = k->args[1];
            if (p.url == "!")
                p.exclude = true;
            else if (!istarts(p.url, "http://"))
            {
                notes.push_back(k->where() + "  " + k->raw + "  not converted (only http:// backends are supported)");
                continue;
            }
            out.push_back(p);
        }
    }

    S log_block(const S &kind, const Node *n, const S &level, Site &site)
    {
        S path = real_file(resolve(parser.root, n->arg(0)));
        if (starts(n->arg(0), "|") || istarts(n->arg(0), "syslog"))
        {
            site.notes.push_back(n->where() + "  " + n->raw + "  piped or syslog logs are not supported, "
                                                              "using the server log");
            return S();
        }
        if (!is_dir(parent_dir(path)))
        {
            site.notes.push_back(n->where() + "  " + n->raw + "  folder does not exist, using the server log");
            return S();
        }
        S out = head(kind, path) + kv("useServer", "0", 2);
        if (kind == "errorlog")
            out += kv("logLevel", level, 2);
        else
        {
            S fmt = n->arg(1);
            auto it = logFormats.find(fmt);
            if (it != logFormats.end())
                fmt = it->second;
            S combined = "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"";
            S common = "%h %l %u %t \"%r\" %>s %b";
            if (fmt == combined || fmt.empty())
                out += kv("logReferer", "1", 2) + kv("logUserAgent", "1", 2);
            else if (fmt == common)
                out += kv("logReferer", "0", 2) + kv("logUserAgent", "0", 2);
            else
                out += kv("logFormat", fmt, 2);
            if (n->args.size() > 2)
                site.notes.push_back(n->where() + "  " + n->raw + "  log condition ignored");
        }
        out += kv("rollingSize", "10M", 2);
        return out + "}\n";
    }

    static S ols_level(const S &lv)
    {
        S l = lower(lv);
        if (l == "emerg" || l == "alert" || l == "crit" || l == "error")
            return "ERROR";
        if (l == "warn")
            return "WARN";
        if (l == "notice")
            return "NOTICE";
        if (l == "info")
            return "INFO";
        if (starts(l, "debug") || starts(l, "trace"))
            return "DEBUG";
        return "ERROR";
    }

    void php_suffixes(const Node *scope, SV &exts, S &fcgi, bool deep)
    {
        for (auto &k : scope->kids)
        {
            const S &key = k->key;
            if ((key == "addtype" || key == "addhandler") && k->args.size() >= 2 && lower(k->args[0]).find("php") != S::npos
                && lower(k->args[0]).find("source") == S::npos)
                for (size_t i = 1; i < k->args.size(); ++i)
                {
                    S e = lower(k->args[i]);
                    while (starts(e, "."))
                        e.erase(0, 1);
                    if (!e.empty() && e != "php")
                        add_unique(exts, e);
                }
            if (key == "sethandler" && !k->args.empty() && istarts(k->args[0], "proxy:fcgi://"))
            {
                S a = k->args[0].substr(13);
                size_t sl = a.find('/');
                if (sl != S::npos)
                    a.erase(sl);
                fcgi = a;
            }
            if (deep && k->block && (key == "filesmatch" || key == "files" || key == "ifmodule"))
                php_suffixes(k.get(), exts, fcgi, false);
        }
    }

    static void cgi_suffixes(const Node *scope, SV &exts)
    {
        for (auto &k : scope->kids)
        {
            if (k->key == "addhandler" && k->args.size() >= 2 && ieq(k->args[0], "cgi-script"))
                for (size_t i = 1; i < k->args.size(); ++i)
                {
                    S e = lower(k->args[i]);
                    while (starts(e, "."))
                        e.erase(0, 1);
                    if (!e.empty())
                        add_unique(exts, e);
                }
            if (k->block && k->key != "virtualhost")
                cgi_suffixes(k.get(), exts);
        }
    }

    void detect_apache_php()
    {
        std::vector<const Node *> stack{&top};
        while (!stack.empty())
        {
            const Node *x = stack.back();
            stack.pop_back();
            for (auto &k : x->kids)
            {
                if (k->key == "loadmodule" && k->args.size() >= 2 && istarts(k->args[0], "php"))
                {
                    apachePhpDir = parent_dir(resolve(parser.root, k->args[1]));
                    apachePhpHow = "mod_php";
                }
                else if (k->key == "phpinidir" && !k->args.empty() && apachePhpDir.empty())
                {
                    apachePhpDir = resolve(parser.root, k->args[0]);
                    apachePhpHow = "mod_php";
                }
                else if (k->key == "fcgidwrapper" && !k->args.empty() && apachePhpDir.empty())
                {
                    apachePhpDir = parent_dir(resolve(parser.root, k->args[0]));
                    apachePhpHow = "mod_fcgid";
                }
                if (k->key != "virtualhost")
                    stack.push_back(k.get());
            }
        }
    }

    void read_listen()
    {
        std::vector<const Node *> stack{&top};
        while (!stack.empty())
        {
            const Node *x = stack.back();
            stack.pop_back();
            for (auto &k : x->kids)
            {
                if (x == &top && k->key == "listen" && !k->args.empty())
                {
                    S host;
                    int port;
                    split_addr(k->args[0], host, port);
                    if (port <= 0 || port > 65535 || !valid_host(host))
                        continue;
                    add_unique(listenHosts[port], host);
                    listenHttps[port] = k->args.size() > 1 ? ieq(k->args[1], "https") : port == 443;
                }
                else if (k->key == "logformat" && k->args.size() >= 2)
                    logFormats[k->args[1]] = k->args[0];
                if (k->block && k->key != "virtualhost")
                    stack.push_back(k.get());
            }
        }
        for (auto &l : listenHosts)
            if (has(l.second, "*"))
                l.second = SV{"*"};
    }

    static S anchor_basename(const S &re)
    {
        S out;
        bool cls = false;
        for (size_t i = 0; i < re.size(); ++i)
        {
            char c = re[i];
            if (c == '\\' && i + 1 < re.size())
            {
                out += c;
                out += re[++i];
                continue;
            }
            if (cls)
            {
                if (c == ']')
                    cls = false;
                out += c;
                continue;
            }
            if (c == '[')
            {
                cls = true;
                out += c;
                if (i + 1 < re.size() && re[i + 1] == '^')
                    out += re[++i];
                continue;
            }
            out += c == '^' ? S("(?<=/)") : S(1, c);
        }
        return out;
    }

    bool same_config(const Node *a, const Node *b)
    {
        auto text = [&](const Node *n) {
            S out;
            std::function<void(const Node *, int)> rec = [&](const Node *x, int d) {
                for (auto &k : x->kids)
                {
                    if (starts(k->key, "ssl") || k->key == "errorlog" || k->key == "customlog" || k->key == "transferlog"
                        || k->key == "loglevel" || k->key == "header" || k->key == "protocols")
                        continue;
                    out += S(d, ' ') + k->raw + "\n";
                    if (k->block)
                        rec(k.get(), d + 1);
                }
            };
            rec(n, 0);
            return out;
        };
        return text(a) == text(b);
    }

    void build_site(Site &site)
    {
        const Member *prim = &site.members[0];
        for (auto &m : site.members)
            if (m.ssl)
            {
                site.cert = m.cert;
                site.key = m.key;
                site.chain = m.chain;
                if (!m.docroot.empty())
                    prim = &m;
                break;
            }
        site.docroot = prim->docroot.empty() ? real_path(resolve(parser.root, main_value("documentroot"))) : prim->docroot;
        site.file = prim->n->where();
        site.admin = prim->admin;
        bad.clear();
        if (!is_dir(site.docroot))
        {
            site.skipWhy = "document root " + site.docroot + " does not exist";
            return;
        }
        if (!clean(site.docroot))
        {
            site.skipWhy = "the document root path contains control characters or <<<";
            return;
        }
        bool hasHttp = false;
        for (auto &m : site.members)
            hasHttp = hasHttp || !m.ssl;

        VhState st;
        st.vnode = prim->n;
        st.base = server_conf(&top);
        merge(st.base, server_conf(prim->n));
        for (auto &s : mainSecs)
            st.secs.push_back(s.get());
        int order = 1 << 16;
        collect_secs(prim->n, true, st.own, order, nullptr);
        for (auto &s : st.own)
            st.secs.push_back(s.get());

        struct Root
        {
            S uri, dir;
            bool cgi;
            SV dirs;
        };
        std::vector<Root> roots{{"/", site.docroot, false, SV()}};
        auto add_aliases = [&](const Node *scope) {
            for (auto &k : scope->kids)
            {
                if (k->key == "aliasmatch" || k->key == "scriptaliasmatch")
                    site.notes.push_back(k->where() + "  " + k->raw + "  not converted");
                if ((k->key != "alias" && k->key != "scriptalias") || k->args.size() < 2)
                    continue;
                S u = k->args[0], d = real_path(resolve(parser.root, k->args[1]));
                if (!starts(u, "/"))
                    continue;
                if (!ends(u, "/"))
                    u += "/";
                bool dup = false;
                for (auto &r : roots)
                    dup = dup || r.uri == u;
                if (dup)
                    continue;
                if (!is_dir(d))
                {
                    site.notes.push_back(k->where() + "  " + k->raw + "  target is not a folder, not converted");
                    continue;
                }
                if (!clean(d) || !clean(u))
                {
                    site.notes.push_back(k->where() + "  " + k->raw + "  target path contains control characters or <<<, not converted");
                    continue;
                }
                roots.push_back({u, d, k->key == "scriptalias", SV()});
                if (k->key == "scriptalias")
                    add_unique(site.converted, "CGI programs in " + u);
            }
        };
        add_aliases(prim->n);
        add_aliases(&top);

        for (auto &r : roots)
        {
            int count = 0;
            htSeen.clear();
            walk(r.dir, 0, st, count, r.dirs);
            if (count > 30000)
                site.notes.push_back(r.dir + " has more than 30000 folders, only the first 30000 were checked");
        }
        S aliasExcl;
        for (auto &r : roots)
            if (r.uri != "/")
                aliasExcl += (aliasExcl.empty() ? "" : "|") + re_escape(r.uri.substr(1));
        if (!aliasExcl.empty())
            aliasExcl = "(?!(?:" + aliasExcl + "))";
        auto uri_of = [&](const Root &r, const S &dir) { return ieq(dir, r.dir) ? r.uri : r.uri + rel(dir, r.dir) + "/"; };
        auto ht_allowed = [&](const Sec *h) { return dir_conf(st, h->path).override_ == 1; };

        std::map<S, SV> realms;
        std::vector<Ctx> prefix;
        std::vector<std::pair<long long, Ctx>> ordered;
        std::vector<std::pair<S, S>> noScriptDirs, cgiAllDirs;
        std::vector<std::pair<S, int>> execDirs;
        SV sslUris;
        std::map<S, std::vector<std::pair<S, S>>> restrictedDirs;
        Conf rootConf = effective(st, site.docroot, "/", true);
        SV n0, w0;
        Access rootAcc = final_access(rootConf, n0, w0);
        int rootIdx = indexes(rootConf);
        SV rootIndex = rootConf.indexSet ? rootConf.index : SV{"index.html"};
        bool autoUsed = rootIdx == 1, anyOverride = rootConf.override_ == 1, cgiAny = false;
        for (auto *s : st.secs)
            cgiAny = cgiAny || s->conf.cgiAll;

        auto report = [&](const S &where, const SV &notes, const SV &warns) {
            for (auto &n : notes)
                add_unique(site.notes, where + ": " + n);
            for (auto &w : warns)
                add_unique(site.warnings, where + ": " + w);
        };
        auto realm_key = [&](const Conf &c, const Access &a) {
            return a.required.empty() || a.denied ? S() : lower(c.userFile) + "|" + lower(c.groupFile) + "|" + c.authName;
        };

        for (auto &r : roots)
        {
            std::map<S, S> dirs;
            dirs[lower(r.dir)] = r.dir;
            for (auto *s : st.secs)
                if (s->kind == Sec::DIR && !s->in && !has_wild(s->path) && s->path != "/" && under(s->path, r.dir)
                    && is_dir(s->path))
                    dirs[lower(s->path)] = s->path;
            for (auto &d : r.dirs)
                for (auto *s : st.secs)
                    if (!s->in && ((s->kind == Sec::DIR && has_wild(s->path) && dir_match(s->path, d))
                                   || (s->kind == Sec::DIRRE && dirre_match(s, d))))
                    {
                        dirs[lower(d)] = d;
                        break;
                    }
            for (auto &h : st.ht)
                if (h->kind == Sec::HT && under(h->path, r.dir))
                    dirs[lower(h->path)] = h->path;
            for (auto &d : r.dirs)
                if (!ends(lower(d), "/.git") && !ends(lower(d), "/node_modules") && !ends(lower(d), "/.svn")
                    && (r.cgi || (cgiAny && !dirs.count(lower(d)) && effective(st, d, uri_of(r, d), true).cgiAll)))
                    dirs[lower(d)] = d;
            for (auto *s : st.secs)
                if (s->kind == Sec::LOC && s->path != "/" && starts(s->path + "/", r.uri))
                {
                    S d = norm(r.dir + "/" + rel(s->path, r.uri.substr(0, r.uri.size() - 1)));
                    if (is_dir(d))
                        d = real_path(d), dirs[lower(d)] = d;
                }
            for (auto &d : dirs)
            {
                const S &dir = d.second;
                S u = uri_of(r, dir);
                bool isRoot = ieq(dir, r.dir);
                Conf c = effective(st, dir, u, true);
                S pdir = parent_dir(dir);
                S puri = u.substr(0, u.size() - 1);
                puri = puri.substr(0, puri.find_last_of('/') + 1);
                Conf pc = isRoot ? Conf() : effective(st, pdir, puri, true);
                if (c.override_ == 1)
                    anyOverride = true;
                if (c.noScript && (isRoot || !pc.noScript))
                    noScriptDirs.push_back({u, dir});
                int exec = option(c, "execcgi"), pexec = isRoot ? 0 : option(pc, "execcgi");
                bool runCgi = r.cgi || (c.cgiAll && exec);
                if (!r.cgi)
                {
                    if (isRoot || exec != pexec)
                        execDirs.push_back({u, exec});
                    if (c.cgiAll && !exec && (isRoot || !pc.cgiAll || pexec))
                        cgiAllDirs.push_back({u, dir});
                    if (runCgi && (isRoot || !pc.cgiAll || !pexec))
                        add_unique(site.converted, "CGI programs in " + u);
                }
                if (c.sslRequire && (isRoot || !pc.sslRequire))
                    sslUris.push_back(u);
                SV notes, warns, pn, pw;
                Access a = final_access(c, notes, warns);
                Access pa = isRoot ? Access() : final_access(pc, pn, pw);
                Ctx x;
                x.uri = u;
                x.location = dir + "/";
                x.cgi = runCgi;
                x.acc = a;
                if (!a.required.empty() && !a.denied)
                {
                    x.realm = realm_for(c, realms, site);
                    x.authName = c.authName.empty() ? S("Restricted") : c.authName;
                }
                x.autoIndex = indexes(c);
                if (c.indexSet && c.index != rootIndex)
                    x.index = c.index;
                x.headers = c.headers;
                x.php = c.php;
                if (c.rewriteOn == 1)
                    x.rewrite = c.rewrite;
                bool differs;
                if (isRoot && r.uri == "/")
                    differs = !a.open() || !c.headers.empty() || !c.php.empty() || !x.rewrite.empty();
                else if (isRoot || runCgi)
                    differs = true;
                else
                    differs = !(pa == a) || realm_key(c, a) != realm_key(pc, pa) || indexes(pc) != x.autoIndex
                           || pc.headers != c.headers || pc.php != c.php || (c.indexSet && c.index != pc.index)
                           || (!x.rewrite.empty() && x.rewrite != pc.rewrite);
                if (!a.open())
                    restrictedDirs[r.uri].push_back({dir, u});
                if (!differs)
                    continue;
                report(u, notes, warns);
                if (x.autoIndex == 1)
                    autoUsed = true;
                prefix.push_back(x);
            }
        }

        auto restricted_ctx = [&](const S &head, const S &tail, const S &dir, int n, const Conf &c, const Access &a,
                                  const S &why) {
            Ctx x;
            x.regex = true;
            x.acc = a;
            x.why = why;
            if (a.denied || n > 9)
            {
                if (!a.denied)
                    add_unique(site.notes, why + ": rule too complex, access denied");
                x.acc = Access();
                x.acc.denied = true;
                x.uri = "exp:" + head;
                return x;
            }
            x.uri = "exp:" + head + tail;
            x.location = dir + "/$" + std::to_string(n);
            if (!a.required.empty())
            {
                x.realm = realm_for(c, realms, site);
                x.authName = c.authName.empty() ? S("Restricted") : c.authName;
            }
            return x;
        };

        std::vector<const Sec *> fileSecs;
        for (auto *s : st.secs)
            if (s->kind == Sec::FILES || s->kind == Sec::FILESRE)
                fileSecs.push_back(s);
        for (auto &h : st.ht)
            if ((h->kind == Sec::FILES || h->kind == Sec::FILESRE) && h->in && ht_allowed(h->in))
                fileSecs.push_back(h.get());
        for (const Sec *s : fileSecs)
        {
            if ((s->kind == Sec::FILES && s->path == ".ht*")
                || (s->kind == Sec::FILESRE && (s->path == "^\\.ht" || s->path == "^\\.ht.*")))
                continue;
            if (!s->conf.headers.empty() || !s->conf.php.empty())
                add_unique(site.notes, s->n->where() + "  " + s->n->raw + "  only access rules inside are converted");
            if (!s->conf.access_rules())
                continue;
            S base = s->kind == Sec::FILES ? "(?<=/)" + glob_re(s->path) + "$" : anchor_basename(s->path);
            S cond = ".*(?:" + base + ")";
            std::vector<std::pair<S, S>> scopes;
            for (auto &r : roots)
            {
                const Sec *in = s->in;
                if (!in)
                    scopes.push_back({r.dir, r.uri});
                else if (in->kind == Sec::HT || (in->kind == Sec::DIR && !has_wild(in->path)))
                {
                    if (in->path == "/" || dir_match(in->path, r.dir))
                        scopes.push_back({r.dir, r.uri});
                    else if (under(in->path, r.dir) && is_dir(in->path))
                        scopes.push_back({in->path, uri_of(r, in->path)});
                }
                else
                {
                    bool whole = in->kind == Sec::DIR ? dir_match(in->path, r.dir) : dirre_match(in, r.dir);
                    if (whole)
                        scopes.push_back({r.dir, r.uri});
                    else
                    {
                        SV top;
                        for (auto &d : r.dirs)
                        {
                            bool m = in->kind == Sec::DIR ? dir_match(in->path, d) : dirre_match(in, d);
                            bool covered = false;
                            for (auto &t : top)
                                covered = covered || under(d, t);
                            if (m && !covered)
                                top.push_back(d);
                        }
                        for (auto &t : top)
                            scopes.push_back({t, uri_of(r, t)});
                    }
                }
            }
            size_t n0s = scopes.size();
            for (size_t i = 0; i < n0s; ++i)
                for (auto &rd : restrictedDirs)
                    for (auto &d : rd.second)
                        if (under(d.first, scopes[i].first) && !ieq(d.first, scopes[i].first))
                            scopes.push_back(d);
            for (auto &sc : scopes)
            {
                Conf c = dir_conf(st, sc.first);
                merge(c, s->conf);
                apply_locations(st, c, sc.second);
                SV notes, warns;
                Access a = final_access(c, notes, warns);
                report(s->n->raw, notes, warns);
                if (a.open())
                    continue;
                S head = "(?i)^" + re_escape(sc.second) + (sc.second == "/" ? aliasExcl : S()) + "(?=" + cond + ")";
                ordered.push_back({100000000000LL + (long long)comps(sc.first).size() * 10000000LL + s->order,
                                   restricted_ctx(head, "(.*)$", sc.first, groups(cond) + 1, c, a, s->n->raw)});
            }
        }

        for (auto *s : st.secs)
        {
            if (!s->conf.access_rules() || s->in)
                continue;
            if (s->kind == Sec::LOC && s->path != "/")
            {
                const Root *best = nullptr;
                for (auto &r : roots)
                    if (r.uri == "/" || s->path == r.uri.substr(0, r.uri.size() - 1) || starts(s->path, r.uri))
                        if (!best || r.uri.size() > best->uri.size())
                            best = &r;
                if (!best)
                    continue;
                S relp = s->path.size() > best->uri.size() ? s->path.substr(best->uri.size()) : S();
                S dir = norm(best->dir + (relp.empty() ? S() : "/" + relp));
                Conf c = dir_conf(st, is_dir(dir) ? dir : parent_dir(dir));
                apply_locations(st, c, s->path);
                SV notes, warns;
                Access a = final_access(c, notes, warns);
                report(s->n->raw, notes, warns);
                if (a.open())
                    continue;
                S p = s->path, head, tail, ldir;
                if (ends(p, "/"))
                    head = "^" + re_escape(p), tail = "(.*)$", ldir = dir;
                else
                    head = "^" + re_escape(p) + "(?=/|$)", tail = "(.*)$", ldir = dir;
                Ctx x = restricted_ctx(head, tail, ldir, 1, c, a, s->n->raw);
                if (!x.acc.denied && !ends(p, "/"))
                    x.location = ldir + "$1";
                ordered.push_back({200000000000LL + s->order, x});
            }
            else if (s->kind == Sec::LOCRE)
            {
                Conf c = dir_conf(st, site.docroot);
                merge(c, s->conf);
                SV notes, warns;
                Access a = final_access(c, notes, warns);
                report(s->n->raw, notes, warns);
                if (a.open())
                    continue;
                if (a.denied || groups(s->path) + 1 > 9)
                {
                    Ctx x = restricted_ctx("(?:" + s->path + ")", "", site.docroot, 10, c, a, s->n->raw);
                    ordered.push_back({200000000000LL + s->order, x});
                    continue;
                }
                for (size_t i = roots.size(); i-- > 0;)
                {
                    const Root &r = roots[i];
                    S head = "^(?=.*?(?:" + s->path + "))" + re_escape(r.uri) + (r.uri == "/" ? aliasExcl : S());
                    ordered.push_back({200000000000LL + s->order,
                                       restricted_ctx(head, "(.*)$", r.dir, groups(s->path) + 1, c, a, s->n->raw)});
                }
            }
            else if (s->kind == Sec::DIRRE && !starts(s->path, "^"))
            {
                Conf c = st.base;
                merge(c, s->conf);
                SV notes, warns;
                if (!final_access(c, notes, warns).denied)
                    continue;
                for (auto &r : roots)
                {
                    Ctx x;
                    x.regex = true;
                    x.acc.denied = true;
                    x.uri = "exp:(?i)^" + re_escape(r.uri.substr(0, r.uri.size() - 1)) + (r.uri == "/" ? aliasExcl : S())
                          + "(?=.*?(?:" + s->path + "))";
                    ordered.push_back({(long long)s->order, x});
                }
            }
        }
        for (auto *s : st.secs)
            if (!s->in && s->conf.access_rules() && (s->kind == Sec::DIRRE || (s->kind == Sec::DIR && has_wild(s->path))))
            {
                bool hit = false;
                for (auto &r : roots)
                {
                    if (s->kind == Sec::DIR ? dir_match(s->path, r.dir) : dirre_match(s, r.dir))
                        hit = true;
                    for (auto &d : r.dirs)
                        hit = hit || (s->kind == Sec::DIR ? dir_match(s->path, d) : dirre_match(s, d));
                }
                if (!hit)
                    add_unique(site.notes, s->n->where() + "  " + s->n->raw + "  matches no folder of this site yet");
            }

        SV exts;
        S fcgi;
        php_suffixes(&top, exts, fcgi, true);
        SV vexts;
        S vfcgi;
        php_suffixes(prim->n, vexts, vfcgi, true);
        for (auto &e : vexts)
            add_unique(exts, e);
        if (!vfcgi.empty())
            fcgi = vfcgi;
        S scriptExt = "php[0-9]?|phtml|phar|pht|phps";
        for (auto &e : exts)
            scriptExt += "|" + re_escape(e);
        std::set<S> nsSeen;
        for (auto &ns : noScriptDirs)
        {
            if (!nsSeen.insert(ns.first).second)
                continue;
            Ctx x;
            x.regex = true;
            x.acc.denied = true;
            x.uri = "exp:(?i)^" + re_escape(ns.first) + (ns.first == "/" ? aliasExcl : S()) + "(?=(?:.*/)?[^/]*\\.(?:"
                  + scriptExt + ")(?:/|$))";
            x.why = "noscript";
            ordered.push_back({400000000000LL + (long long)comps(ns.second).size(), x});
            add_unique(site.converted, "PHP turned off in " + ns.first + " (PHP files there are blocked)");
        }
        SV cgiExts;
        cgi_suffixes(&top, cgiExts);
        cgi_suffixes(prim->n, cgiExts);
        cgiExts.erase(std::remove_if(cgiExts.begin(), cgiExts.end(),
                                     [&](const S &e) {
                                         return e == "php" || std::find(exts.begin(), exts.end(), e) != exts.end();
                                     }),
                      cgiExts.end());
        bool cgiRun = false;
        if (!cgiExts.empty())
        {
            S cgiExt, cgiList;
            for (auto &e : cgiExts)
            {
                cgiExt += (cgiExt.empty() ? "" : "|") + re_escape(e);
                cgiList += (cgiList.empty() ? "." : ", .") + e;
            }
            for (auto &d : execDirs)
            {
                if (d.second)
                {
                    cgiRun = true;
                    continue;
                }
                S excl;
                for (auto &o : execDirs)
                    if (o.second && o.first.size() > d.first.size() && starts(o.first, d.first))
                        excl += (excl.empty() ? "" : "|") + re_escape(o.first.substr(d.first.size()));
                Ctx x;
                x.regex = true;
                x.acc.denied = true;
                x.uri = "exp:(?i)^" + re_escape(d.first) + (d.first == "/" ? aliasExcl : S())
                      + (excl.empty() ? S() : "(?!(?:" + excl + "))") + "(?=(?:.*/)?[^/]*\\.(?:" + cgiExt + ")(?:/|$))";
                x.why = "noscript";
                ordered.push_back({400000000000LL + (long long)comps(d.first).size(), x});
            }
            if (cgiRun)
                add_unique(site.converted, "CGI programs (" + cgiList + ") in folders with Options ExecCGI");
            else
                add_unique(site.converted, "CGI files (" + cgiList + ") blocked, Options ExecCGI is off");
        }
        for (auto &d : cgiAllDirs)
        {
            Ctx x;
            x.regex = true;
            x.acc.denied = true;
            x.uri = "exp:(?i)^" + re_escape(d.first) + (d.first == "/" ? aliasExcl : S());
            x.why = "noscript";
            ordered.push_back({400000000000LL + (long long)comps(d.second).size(), x});
        }
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const std::pair<long long, Ctx> &a, const std::pair<long long, Ctx> &b) {
                             return a.first > b.first;
                         });
        std::vector<Ctx> regex;
        std::set<S> seenUri;
        for (auto &o : ordered)
            if (seenUri.insert(o.second.uri).second)
            {
                char rank[16];
                snprintf(rank, sizeof(rank), "(?#%04d)", (int)regex.size());
                regex.push_back(o.second);
                regex.back().uri = "exp:" + S(rank) + o.second.uri.substr(4);
            }

        std::vector<Proxy> prox;
        SV pnotes;
        proxies(prim->n, prox, pnotes);
        proxies(&top, prox, pnotes);
        for (auto &n : pnotes)
            add_unique(site.notes, n);

        S conf;
        conf += mark() + ": " + shown(site.file) + "\n";
        conf += kv("docRoot", site.docroot + "/", 0);
        if (!site.admin.empty())
            conf += kv("adminEmails", site.admin, 0);
        conf += kv("enableGzip", "1", 0);
        conf += kv("enableBr", "1", 0);
        conf += "\nindex  {\n" + kv("useServer", "0", 2) + kv("indexFiles", join(rootIndex, ", "), 2)
              + kv("autoIndex", std::to_string(rootIdx), 2);
        if (autoUsed)
            conf += kv("autoIndexURI", "/_autoindex/default.php", 2);
        conf += "}\n";

        S level = ols_level(main_value("loglevel"));
        for (auto &k : prim->n->kids)
            if (k->key == "loglevel" && !k->args.empty())
                level = ols_level(k->args[0]);
        bool elog = false, alog = false;
        for (auto &k : prim->n->kids)
        {
            if (k->key == "errorlog" && !elog && !k->args.empty())
            {
                S b = log_block("errorlog", k.get(), level, site);
                if (!b.empty())
                    conf += "\n" + b, elog = true;
            }
            else if ((k->key == "customlog" || k->key == "transferlog") && !k->args.empty())
            {
                if (alog)
                {
                    site.notes.push_back(k->where() + "  " + k->raw + "  only the first access log is used");
                    continue;
                }
                S b = log_block("accesslog", k.get(), level, site);
                if (!b.empty())
                    conf += "\n" + b, alog = true;
            }
        }

        for (auto &e : rootConf.errors)
        {
            S target = e.second;
            if (starts(target, "/") || istarts(target, "http://") || istarts(target, "https://"))
                conf += "\n" + head("errorPage", e.first) + kv("url", target, 2) + "}\n";
            else
                site.notes.push_back("ErrorDocument " + e.first + " with a text message is not converted");
        }

        for (auto &r : realms)
        {
            conf += "\n" + head("realm", r.second[0]) + "  userDB  {\n" + kv("location", r.second[1], 4) + "  }\n";
            if (!r.second[2].empty())
                conf += "  groupDB  {\n" + kv("location", r.second[2], 4) + "  }\n";
            conf += "}\n";
        }

        S handler = phpHandler;
        if (!fcgi.empty())
        {
            S name = "apache-php-" + std::to_string(++procSeq);
            conf += "\n" + head("extprocessor", name) + kv("type", "fcgi", 2) + kv("address", fcgi, 2)
                  + kv("maxConns", "35", 2) + kv("initTimeout", "60", 2) + kv("retryTimeout", "0", 2)
                  + kv("persistConn", "1", 2) + kv("respBuffer", "0", 2) + kv("autoStart", "0", 2) + "}\n";
            handler = "fcgi:" + name;
            site.converted.push_back("PHP through the FastCGI server at " + fcgi);
            exts.insert(exts.begin(), "php");
        }
        if (!exts.empty() || cgiRun)
        {
            conf += "\nscriptHandler  {\n";
            for (auto &e : exts)
                conf += kv("add", handler + " " + e, 2);
            if (cgiRun)
                for (auto &e : cgiExts)
                    conf += kv("add", "cgi:lscgid " + e, 2);
            conf += "}\n";
        }

        SV procRules;
        std::map<S, S> procNames;
        for (auto &p : prox)
        {
            if (p.exclude)
            {
                procRules.push_back("RewriteRule " + quote_arg("^" + re_escape(p.path)) + " - [L]");
                continue;
            }
            S rest = p.url.substr(7);
            size_t sl = rest.find('/');
            S hostport = sl == S::npos ? rest : rest.substr(0, sl);
            S path = sl == S::npos ? S("/") : rest.substr(sl);
            if (hostport.find(':') == S::npos)
                hostport += ":80";
            S &nm = procNames[lower(hostport)];
            if (nm.empty())
            {
                nm = "apache-proxy-" + std::to_string(++procSeq);
                conf += "\n" + head("extprocessor", nm) + kv("type", "proxy", 2) + kv("address", hostport, 2)
                      + kv("maxConns", "100", 2) + kv("initTimeout", "60", 2) + kv("retryTimeout", "0", 2)
                      + kv("respBuffer", "0", 2) + "}\n";
            }
            procRules.push_back("RewriteRule " + quote_arg("^" + re_escape(p.path) + "(.*)$") + " "
                                + quote_arg("http://" + nm + path + "$1") + " [P,L]");
            site.converted.push_back("proxy " + p.path + " -> " + p.url);
        }

        for (auto &x : regex)
            conf += "\n" + render_ctx(x);
        std::stable_sort(prefix.begin(), prefix.end(), [](const Ctx &a, const Ctx &b) { return a.uri < b.uri; });
        for (auto &x : prefix)
            conf += "\n" + render_ctx(x);

        SV allRules;
        if (hasHttp)
            for (auto &u : sslUris)
            {
                allRules.push_back("RewriteCond %{HTTPS} !on");
                allRules.push_back("RewriteRule " + quote_arg("^" + re_escape(u)) + " - [F,L]");
                add_unique(site.converted, "HTTPS required for " + u);
            }
        allRules.insert(allRules.end(), procRules.begin(), procRules.end());
        std::vector<SV> memberRules;
        for (auto &m : site.members)
        {
            bool inh, before;
            SV rn;
            SV rr = rewrite_rules(m.n, inh, before, rn);
            if (inh)
            {
                bool i2, b2;
                SV mainRules = rewrite_rules(&top, i2, b2, rn);
                if (before)
                    rr.insert(rr.begin(), mainRules.begin(), mainRules.end());
                else
                    rr.insert(rr.end(), mainRules.begin(), mainRules.end());
            }
            SV red = redirect_rules(m.n, rn);
            SV redMain = redirect_rules(&top, rn);
            rr.insert(rr.end(), red.begin(), red.end());
            rr.insert(rr.end(), redMain.begin(), redMain.end());
            for (auto &n : rn)
                add_unique(site.notes, n);
            memberRules.push_back(rr);
        }
        if (memberRules.size() == 1 || memberRules[0] == memberRules[1])
            allRules.insert(allRules.end(), memberRules[0].begin(), memberRules[0].end());
        else
            for (size_t i = 0; i < site.members.size(); ++i)
            {
                SV c = with_cond(memberRules[i], site.members[i].ssl ? "RewriteCond %{HTTPS} on" : "RewriteCond %{HTTPS} !on");
                allRules.insert(allRules.end(), c.begin(), c.end());
            }
        int nRules = 0;
        for (auto &r : allRules)
            if (istarts(trim(r), "rewriterule"))
                ++nRules;
        if (anyOverride && rootConf.override_ != 1)
            site.notes.push_back(".htaccess files are read for the whole site, Apache reads them only in some folders");
        conf += "\nrewrite  {\n" + kv("enable", "1", 2) + kv("autoLoadHtaccess", anyOverride ? "1" : "0", 2);
        if (!allRules.empty())
            conf += heredoc("rules", allRules, 2);
        conf += "}\n";

        if (!site.cert.empty())
        {
            S certFile = site.cert, keyFile = site.key.empty() ? site.cert : site.key;
            if (!site.chain.empty())
            {
                S a, b, chainFile = site.chain;
                site.chain.clear();
                if (read_file(site.cert, a) && read_file(chainFile, b) && !(a = cert_blocks(a)).empty()
                    && !(b = cert_blocks(b)).empty())
                {
                    site.converted.push_back("certificate chain from " + chainFile);
                    site.warnings.push_back("certificate and chain were combined into conf/vhosts/" + site.vhname
                                            + "/cert-chain.pem; import again after renewing the certificate");
                    certFile = "$SERVER_ROOT/conf/vhosts/" + site.vhname + "/cert-chain.pem";
                    site.chain = a + b;
                }
                else
                    site.warnings.push_back("certificate chain " + chainFile + " has no certificates, not used");
            }
            conf += "\nvhssl  {\n" + kv("keyFile", keyFile, 2) + kv("certFile", certFile, 2) + kv("certChain", "1", 2)
                  + "}\n";
            site.converted.push_back("SSL certificate");
        }

        if (nRules)
            site.converted.push_back(std::to_string(nRules) + " rewrite and redirect rule" + (nRules > 1 ? "s" : ""));
        if (roots.size() > 1)
            site.converted.push_back(std::to_string(roots.size() - 1) + " alias" + (roots.size() > 2 ? "es" : ""));
        int restricted = 0;
        for (auto &x : prefix)
            if (!x.acc.open())
                ++restricted;
        for (auto &x : regex)
            if (!x.acc.open() && x.why != "noscript")
                ++restricted;
        if (restricted)
            site.converted.push_back(std::to_string(restricted) + " access rule" + (restricted > 1 ? "s" : ""));
        if (!realms.empty())
            site.converted.push_back("password protection");
        if (rootIdx == 1)
            site.converted.push_back("directory listing");
        if (anyOverride)
            site.converted.push_back(".htaccess files");
        if (rootAcc.denied)
            site.warnings.push_back("Apache denies access to " + site.docroot + ", so does OpenLiteSpeed");

        std::vector<const Node *> stack{prim->n};
        while (!stack.empty())
        {
            const Node *x = stack.back();
            stack.pop_back();
            for (auto &k : x->kids)
            {
                if (!is_known(k->key) && k->key != "ifmodule")
                    site.skipped.push_back(k->where() + "  " + k->raw);
                if (k->block && (k->key == "directory" || k->key == "location" || k->key == "files"
                                 || k->key == "filesmatch" || k->key == "directorymatch" || k->key == "locationmatch"))
                    stack.push_back(k.get());
            }
        }
        if (!bad.empty())
        {
            site.skipWhy = "unsafe text in its configuration: " + bad[0];
            return;
        }
        site.conf = conf;
    }

    void load_php_info()
    {
        S text;
        if (!read_file(httpdConf, text))
            return;
        size_t b = text.find("# php begin"), e = text.find("# php end");
        if (b == S::npos || e == S::npos)
            return;
        S blk = text.substr(b, e - b);
        size_t p = blk.find("\n    path ");
        if (p == S::npos)
            p = blk.find("path ");
        size_t a = blk.find("add ");
        if (a != S::npos)
        {
            SV t = split_args(blk.substr(a + 4, blk.find('\n', a) - a - 4));
            if (!t.empty())
                phpHandler = t[0];
        }
        size_t q = blk.find("PHPRC=");
        if (q != S::npos)
            olsPhpDir = trim(blk.substr(q + 6, blk.find('\n', q) - q - 6));
        (void)p;
    }

    std::vector<Listener> existing_listeners(const S &text, std::set<S> &domains, std::set<S> &vhnames)
    {
        std::vector<Listener> out;
        const S &body = text;
        SV lines;
        size_t i = 0;
        while (i <= body.size())
        {
            size_t j = body.find('\n', i);
            if (j == S::npos)
                j = body.size();
            lines.push_back(trim(body.substr(i, j - i)));
            i = j + 1;
        }
        int depth = 0;
        Listener *cur = nullptr;
        bool inTpl = false;
        for (auto &l : lines)
        {
            if (l.empty() || l[0] == '#')
                continue;
            SV t = split_args(l);
            if (t.empty())
                continue;
            if (ends(l, "{"))
            {
                if (depth == 0 && ieq(t[0], "listener") && t.size() >= 2)
                {
                    out.push_back(Listener());
                    cur = &out.back();
                    cur->name = t[1];
                }
                if (depth == 0 && (ieq(t[0], "virtualhost") || ieq(t[0], "vhTemplate")) && t.size() >= 2)
                    vhnames.insert(lower(t[1]));
                if (depth == 0 && ieq(t[0], "vhTemplate"))
                    inTpl = true;
                if (inTpl && depth == 1 && ieq(t[0], "member") && t.size() >= 2)
                    vhnames.insert(lower(t[1]));
                ++depth;
                continue;
            }
            if (l == "}")
            {
                --depth;
                if (depth == 0)
                {
                    cur = nullptr;
                    inTpl = false;
                }
                continue;
            }
            if (cur && depth == 1)
            {
                if (ieq(t[0], "address") && t.size() >= 2)
                    split_addr(t[1], cur->host, cur->port);
                else if (ieq(t[0], "secure") && t.size() >= 2)
                    cur->secure = t[1] == "1";
                else if (ieq(t[0], "map") && t.size() >= 3)
                {
                    S rest = l.substr(l.find(t[1]) + t[1].size());
                    for (auto &d : split_args(std::regex_replace(rest, std::regex(","), " ")))
                        if (d != "*")
                            domains.insert(lower(d));
                }
            }
            if (inTpl && depth == 2 && (ieq(t[0], "vhDomain") || ieq(t[0], "vhAliases")))
            {
                S rest = l.substr(t[0].size());
                for (auto &d : split_args(std::regex_replace(rest, std::regex(","), " ")))
                    if (d != "*")
                    {
                        domains.insert(lower(d));
                        if (!starts(d, "*."))
                            domains.insert("www." + lower(d));
                    }
            }
        }
        return out;
    }

    S state_path() const { return olsRoot + "/conf/" + kind + "-import.state"; }

    void read_state(SV &vhs, SV &lss)
    {
        S text;
        vhs.clear();
        lss.clear();
        if (!read_file(state_path(), text))
            return;
        size_t i = 0;
        while (i < text.size())
        {
            size_t j = text.find('\n', i);
            if (j == S::npos)
                j = text.size();
            SV t = split_args(text.substr(i, j - i));
            i = j + 1;
            if (t.size() == 2 && t[0] == "virtualhost")
                vhs.push_back(t[1]);
            else if (t.size() == 2 && t[0] == "listener")
                lss.push_back(t[1]);
        }
    }

    bool write_state(const SV &vhs, const SV &lss)
    {
        if (vhs.empty() && lss.empty())
            return DeleteFileW(W(state_path()).c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND;
        S out = "# Written by openlitespeed --import-" + kind + "\n";
        for (auto &v : vhs)
            out += "virtualhost " + v + "\n";
        for (auto &l : lss)
            out += "listener " + l + "\n";
        return write_file(state_path(), out);
    }

    S drop_objects(const S &text, const SV &vhs, const SV &lss)
    {
        SV lines;
        size_t i = 0;
        while (i < text.size())
        {
            size_t j = text.find('\n', i);
            j = j == S::npos ? text.size() : j + 1;
            lines.push_back(text.substr(i, j - i));
            i = j;
        }
        std::vector<bool> drop(lines.size(), false);
        int depth = 0;
        S heredoc;
        bool dropping = false, inListener = false, inImport = false;
        for (size_t k = 0; k < lines.size(); ++k)
        {
            S t = trim(lines[k]);
            if (heredoc.empty() && !t.empty() && t[0] != '#')
            {
                size_t h = t.find_first_of("#");
                while (h != S::npos && !(h > 0 && (t[h - 1] == ' ' || t[h - 1] == '\t')))
                    h = t.find('#', h + 1);
                if (h != S::npos)
                    t = trim(t.substr(0, h));
            }
            if (!heredoc.empty())
            {
                if (t == heredoc)
                    heredoc.clear();
                if (dropping)
                    drop[k] = true;
                continue;
            }
            if (t == import_begin() || t == import_end() || t == maps_begin() || t == maps_end())
            {
                drop[k] = true;
                if (t == import_begin() && k > 0 && trim(lines[k - 1]).empty())
                    drop[k - 1] = true;
                inImport = t == import_begin() || t == maps_begin();
                continue;
            }
            SV tok = t.empty() || t[0] == '#' ? SV() : split_args(t);
            bool opens = !t.empty() && t.back() == '{' && t[0] != '#';
            if (depth == 0 && opens && tok.size() >= 2)
            {
                S kind = lower(tok[0]);
                dropping = (kind == "virtualhost" && has(vhs, tok[1])) || (kind == "listener" && has(lss, tok[1]));
                inListener = kind == "listener";
            }
            if (dropping)
                drop[k] = true;
            else if (inListener && depth == 1 && tok.size() >= 2 && ieq(tok[0], "map") && has(vhs, tok[1]))
                drop[k] = true;
            size_t hd = t.find("<<<");
            if (hd != S::npos && t[0] != '#' && !inImport && !dropping)
                heredoc = trim(t.substr(hd + 3));
            if (opens)
                ++depth;
            else if (t == "}" && depth > 0 && --depth == 0)
            {
                if (dropping && k + 1 < lines.size() && trim(lines[k + 1]).empty())
                    drop[k + 1] = true;
                dropping = inListener = false;
            }
        }
        S out;
        for (size_t k = 0; k < lines.size(); ++k)
            if (!drop[k])
                out += lines[k];
        for (;;)
        {
            if (ends(out, "\r\n\r\n"))
                out.erase(out.size() - 2);
            else if (ends(out, "\n\n"))
                out.pop_back();
            else
                break;
        }
        return out;
    }

    void remove_dirs(const SV &vhs)
    {
        for (auto &v : vhs)
        {
            S dir = olsRoot + "/conf/vhosts/" + v, text;
            if (!read_file(dir + "/vhconf.conf", text) || !starts(text, mark()))
                continue;
            DeleteFileW(W(dir + "/vhconf.conf").c_str());
            DeleteFileW(W(dir + "/vhconf.conf0").c_str());
            DeleteFileW(W(dir + "/cert-chain.pem").c_str());
            RemoveDirectoryW(W(dir).c_str());
        }
    }

    int remove()
    {
        S text;
        SV vhs, lss;
        if (!read_file(httpdConf, text))
        {
            fprintf(stderr, "cannot read %s\n", httpdConf.c_str());
            return 1;
        }
        read_state(vhs, lss);
        S out = drop_objects(text, vhs, lss);
        if (vhs.empty() && out == text)
        {
            printf("nothing to remove\n");
            return 0;
        }
        if (out != text && !write_file(httpdConf, out))
        {
            fprintf(stderr, "cannot write %s\n", httpdConf.c_str());
            return 1;
        }
        remove_dirs(vhs);
        write_state(SV(), SV());
        printf("removed %d imported site(s). Reload OpenLiteSpeed to apply.\n", (int)vhs.size());
        return 0;
    }

    S find_conf(SV &found)
    {
        wchar_t d[MAX_PATH];
        auto try_add = [&](const S &p) {
            S n = norm(p);
            if (is_file(n))
                for (auto &f : found)
                    if (ieq(f, n))
                        return;
            if (is_file(n))
                found.push_back(n);
        };
        if (phpd_find_laragon(d))
        {
            S lr = norm(U(d));
            S ini, ver;
            if (read_file(lr + "/usr/laragon.ini", ini))
            {
                size_t a = ini.find("[apache]");
                if (a != S::npos)
                {
                    size_t v = ini.find("Version=", a);
                    size_t nx = ini.find("[", a + 1);
                    if (v != S::npos && (nx == S::npos || v < nx))
                        ver = trim(ini.substr(v + 8, ini.find('\n', v) - v - 8));
                }
            }
            if (!ver.empty())
                try_add(lr + "/bin/apache/" + ver + "/conf/httpd.conf");
            else
            {
                SV names;
                list_dir(lr + "/bin/apache", names, true);
                for (auto &n : names)
                    try_add(lr + "/bin/apache/" + n + "/conf/httpd.conf");
            }
        }
        if (phpd_find_xampp(d))
            try_add(norm(U(d)) + "/apache/conf/httpd.conf");
        if (phpd_running_exe_dir(L"httpd.exe", d))
            try_add(parent_dir(norm(U(d))) + "/conf/httpd.conf");
        wchar_t drives[128];
        if (GetLogicalDriveStringsW(128, drives))
            for (wchar_t *p = drives; *p; p += wcslen(p) + 1)
            {
                if (GetDriveTypeW(p) != DRIVE_FIXED)
                    continue;
                S r = norm(U(p));
                try_add(r + "/Apache24/conf/httpd.conf");
                try_add(r + "/Apache2/conf/httpd.conf");
                SV names;
                for (const char *w : {"wamp64", "wamp"})
                {
                    names.clear();
                    list_dir(r + "/" + w + "/bin/apache", names, true);
                    for (auto &n : names)
                        try_add(r + "/" + w + "/bin/apache/" + n + "/conf/httpd.conf");
                }
            }
        return found.size() == 1 ? found[0] : S();
    }

    void apache_version()
    {
        S exe = parser.root + "/bin/httpd.exe";
        DWORD h = 0, n = GetFileVersionInfoSizeW(W(exe).c_str(), &h);
        if (!n)
            return;
        std::vector<char> buf(n);
        VS_FIXEDFILEINFO *fi;
        UINT fl;
        if (GetFileVersionInfoW(W(exe).c_str(), 0, n, buf.data()) && VerQueryValueW(buf.data(), L"\\", (void **)&fi, &fl)
            && fl)
            parser.version = std::to_string(HIWORD(fi->dwFileVersionMS)) + "." + std::to_string(LOWORD(fi->dwFileVersionMS))
                           + "." + std::to_string(HIWORD(fi->dwFileVersionLS));
    }

    int run(const S &confArg)
    {
        httpdConf = olsRoot + "/conf/httpd_config.conf";
        S text;
        if (!read_file(httpdConf, text))
        {
            fprintf(stderr, "cannot read %s\n", httpdConf.c_str());
            return 1;
        }
        if (confArg.empty())
        {
            SV found;
            apacheConf = find_conf(found);
            if (apacheConf.empty())
            {
                if (found.empty())
                    fprintf(stderr, "no Apache configuration found, pass the path to httpd.conf\n");
                else
                {
                    fprintf(stderr, "several Apache configurations found, pass one of them:\n");
                    for (auto &f : found)
                        fprintf(stderr, "  %s\n", shown(f).c_str());
                }
                return 2;
            }
        }
        else
            apacheConf = norm(confArg);
        if (!is_file(apacheConf))
        {
            fprintf(stderr, "%s not found\n", shown(apacheConf).c_str());
            return 2;
        }
        parser.root = parent_dir(parent_dir(apacheConf));
        apache_version();
        parser.parse(apacheConf, &top, 0);
        if (parser.files.empty())
        {
            fprintf(stderr, "cannot read %s\n", shown(apacheConf).c_str());
            return 1;
        }
        load_php_info();
        read_listen();
        detect_apache_php();
        int order = 0;
        collect_secs(&top, false, mainSecs, order, nullptr);

        std::set<S> existingDomains, vhnames;
        SV oldVh, oldLs;
        read_state(oldVh, oldLs);
        S base = drop_objects(text, oldVh, oldLs);
        std::vector<Listener> listeners = existing_listeners(base, existingDomains, vhnames);
        int adminPort = 0;
        {
            S at;
            if (read_file(olsRoot + "/admin/conf/admin_config.conf", at))
            {
                size_t p = at.find("address");
                if (p != S::npos)
                {
                    S l = at.substr(p, at.find('\n', p) - p);
                    size_t c = l.rfind(':');
                    if (c != S::npos)
                        adminPort = atoi(l.c_str() + c + 1);
                }
            }
        }

        std::vector<const Node *> vnodes;
        std::vector<const Node *> stack{&top};
        while (!stack.empty())
        {
            const Node *x = stack.back();
            stack.pop_back();
            for (auto it = x->kids.rbegin(); it != x->kids.rend(); ++it)
            {
                if ((*it)->key == "virtualhost")
                    vnodes.push_back(it->get());
            }
        }
        std::reverse(vnodes.begin(), vnodes.end());
        bool mainClient = client_cert(&top);

        std::vector<Site> sites;
        SV skippedTop = parser.badSites;
        for (auto *v : vnodes)
        {
            if (v->unsafe || top.unsafe)
            {
                skippedTop.push_back(v->where() + "  not imported: " + (v->unsafe ? v->unsafeWhy : top.unsafeWhy));
                continue;
            }
            Member m;
            member_from(v, m);
            if (!m.skip.empty())
            {
                skippedTop.push_back(v->where() + "  " + m.skip);
                continue;
            }
            if (m.name.empty())
            {
                skippedTop.push_back(v->where() + "  <VirtualHost " + join(v->args, " ") + ">  has no ServerName");
                continue;
            }
            if (sample_name(m.name))
            {
                skippedTop.push_back(v->where() + "  " + m.name + "  Apache sample site");
                continue;
            }
            if (m.clientCert || (m.ssl && mainClient))
            {
                skippedTop.push_back(v->where() + "  " + m.name + "  requires client certificates, not supported");
                continue;
            }
            Site *site = nullptr;
            for (auto &s : sites)
                if (s.name == m.name)
                    site = &s;
            if (!site)
            {
                sites.push_back(Site());
                site = &sites.back();
                site->name = m.name;
            }
            else
            {
                bool clash = false;
                for (auto &o : site->members)
                    if (o.ssl == m.ssl)
                        clash = true;
                if (clash)
                {
                    skippedTop.push_back(v->where() + "  " + m.name + "  duplicate of an earlier <VirtualHost>, Apache uses the first");
                    continue;
                }
            }
            if (m.ssl)
            {
                if (m.cert.empty())
                {
                    for (auto &k : top.kids)
                        if (k->key == "sslcertificatefile" && !k->args.empty())
                            m.cert = real_file(resolve(parser.root, k->args[0]));
                        else if (k->key == "sslcertificatekeyfile" && !k->args.empty())
                            m.key = real_file(resolve(parser.root, k->args[0]));
                }
                if (m.cert.empty() || !is_file(m.cert) || (!m.key.empty() && !is_file(m.key)))
                {
                    site->warnings.push_back("HTTPS not imported: certificate file "
                                             + (m.cert.empty() ? S("(none)") : (is_file(m.cert) ? m.key : m.cert))
                                             + " not found");
                    continue;
                }
            }
            for (auto &d : v->dropped)
                add_unique(site->notes, d);
            site->members.push_back(m);
        }
        size_t nsites = sites.size();
        for (size_t i = 0; i < nsites; ++i)
        {
            if (sites[i].members.size() != 2)
                continue;
            Member &a = sites[i].members[0], &b = sites[i].members[1];
            const Member &http = a.ssl ? b : a, &https = a.ssl ? a : b;
            if (redirects_everything(http.n) || same_config(http.n, https.n))
                continue;
            Site split;
            split.name = sites[i].name;
            split.httpPart = true;
            split.members.push_back(http);
            Member keep = https;
            sites[i].members.clear();
            sites[i].members.push_back(keep);
            sites.push_back(split);
        }
        return emit(sites, skippedTop, text, base, listeners, existingDomains, vhnames, oldVh, adminPort,
                    [&](Site &s) { build_site(s); }, [&]() { print_header(); });
    }

    void print_header()
    {
        printf("Apache configuration: %s (%d file%s)\n", shown(apacheConf).c_str(), (int)parser.files.size(),
               parser.files.size() == 1 ? "" : "s");
        for (auto &m : parser.missing)
            printf("  missing: %s\n", shown(m).c_str());
        for (auto &d : top.dropped)
            printf("  note: %s\n", shown(d).c_str());
        if (top.unsafe)
            printf("  not imported: %s\n", shown(top.unsafeWhy).c_str());
        if (!apachePhpDir.empty())
        {
            printf("Apache PHP: %s (%s)\n", shown(apachePhpDir).c_str(), apachePhpHow.c_str());
            if (!olsPhpDir.empty() && !ieq(norm(apachePhpDir), norm(olsPhpDir)))
                printf("  OpenLiteSpeed uses %s. To switch: openlitespeed --set-php \"%s\"\n", olsPhpDir.c_str(),
                       apachePhpDir.c_str());
        }
    }

    int emit(std::vector<Site> &sites, SV &skippedTop, const S &text, const S &base,
             std::vector<Listener> &listeners, std::set<S> &existingDomains, std::set<S> &vhnames,
             const SV &oldVh, int adminPort, const std::function<void(Site &)> &build,
             const std::function<void()> &header)
    {
        std::map<S, S> used;
        std::vector<Listener> created;
        for (auto &s : sites)
        {
            if (s.members.empty())
            {
                s.skipWhy = s.warnings.empty() ? "nothing to import" : s.warnings[0];
                continue;
            }
            S vh = s.name + (s.httpPart ? "-http" : "");
            for (char &c : vh)
                if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_'))
                    c = '-';
            auto taken = [&](const S &n) {
                return vhnames.count(lower(n)) || (is_dir(olsRoot + "/conf/vhosts/" + n) && !has(oldVh, n));
            };
            if (taken(vh))
                vh = kind + "-" + vh;
            S candidate = vh;
            for (int i = 2; taken(candidate); ++i)
                candidate = vh + "-" + std::to_string(i);
            s.vhname = candidate;
            SV doms;
            for (auto &m : s.members)
            {
                add_unique(doms, m.name);
                for (auto &a : m.aliases)
                    add_unique(doms, a);
            }
            if (existingDomains.count(s.name))
            {
                s.skipWhy = "already served by OpenLiteSpeed";
                continue;
            }
            S scheme = s.members.size() == 2 ? "*" : s.members[0].ssl ? "https" : "http";
            for (auto &d : doms)
            {
                if (d == "*")
                {
                    s.notes.push_back("ServerAlias * not imported, OpenLiteSpeed keeps its own default site");
                    continue;
                }
                if (!valid_domain(d) || d.find('*', 1) != S::npos)
                {
                    s.notes.push_back("domain pattern " + d + " not supported");
                    continue;
                }
                if (existingDomains.count(d))
                {
                    s.notes.push_back(d + " is already served by another OpenLiteSpeed site");
                    continue;
                }
                for (auto &e : existingDomains)
                    if (starts(e, "*.") && ends(d, e.substr(1)) && d != e)
                        s.notes.push_back(d + " is also matched by " + e + " in OpenLiteSpeed, the imported site takes precedence");
                bool clash = false;
                for (const char *sc : {"http", "https"})
                    if ((scheme == "*" || scheme == sc) && used.count(S(sc) + ":" + d))
                    {
                        s.notes.push_back(d + " is already used by " + used[S(sc) + ":" + d] + ", " + label + " uses the first");
                        clash = true;
                        break;
                    }
                if (!clash)
                    s.domains.push_back(d);
            }
            if (s.domains.empty())
            {
                s.skipWhy = "all domains are already served by OpenLiteSpeed";
                continue;
            }
            build(s);
            if (!s.skipWhy.empty())
                continue;
            for (auto &d : s.domains)
                for (const char *sc : {"http", "https"})
                    if (scheme == "*" || scheme == sc)
                        used[S(sc) + ":" + d] = s.vhname;
            vhnames.insert(lower(s.vhname));
            for (auto &m : s.members)
                for (auto &b : m.binds)
                {
                    if (b.port == adminPort && (b.host == "*" || b.host == "127.0.0.1"))
                    {
                        s.notes.push_back("port " + std::to_string(b.port) + " is the WebAdmin port, skipped");
                        continue;
                    }
                    auto &list = m.ssl ? s.httpsBinds : s.httpBinds;
                    if (std::find(list.begin(), list.end(), b) == list.end())
                        list.push_back(b);
                }
            s.imported = true;
        }

        auto bind_text = [](const Bind &b) {
            if (b.host == "*")
                return "*:" + std::to_string(b.port);
            return (b.host.find(':') != S::npos ? "[" + b.host + "]" : b.host) + ":" + std::to_string(b.port);
        };
        auto name_used = [&](const S &n) {
            for (auto &l : listeners)
                if (ieq(l.name, n))
                    return true;
            for (auto &l : created)
                if (ieq(l.name, n))
                    return true;
            return false;
        };
        created.reserve(256);
        auto listener_for = [&](const Bind &b, bool secure, const Site &s) -> Listener * {
            for (auto &l : listeners)
                if (l.port == b.port && l.secure == secure && l.host == b.host)
                    return &l;
            if (!secure && b.port == 80 && b.host == "*")
                for (auto &l : listeners)
                    if (ieq(l.name, "Default") && !l.secure && l.host == "*")
                        return &l;
            for (auto &l : created)
                if (l.port == b.port && l.secure == secure && l.host == b.host)
                    return &l;
            Listener l;
            S base = listenerBase + (secure ? "SSL" : "") + std::to_string(b.port);
            if (b.host != "*")
            {
                S h = b.host;
                for (char &c : h)
                    if (!isalnum((unsigned char)c))
                        c = '-';
                base += "-" + h;
            }
            l.name = base;
            for (int i = 2; name_used(l.name); ++i)
                l.name = base + "-" + std::to_string(i);
            l.host = b.host;
            l.port = b.port;
            l.secure = secure;
            l.ours = true;
            if (secure)
            {
                l.cert = s.cert;
                l.key = s.key.empty() ? s.cert : s.key;
            }
            created.push_back(l);
            return &created.back();
        };
        for (auto &s : sites)
        {
            if (!s.imported)
                continue;
            S line = s.vhname + " " + join(s.domains, ", ");
            for (int secure = 0; secure < 2; ++secure)
                for (auto &b : secure ? s.httpsBinds : s.httpBinds)
                {
                    Listener *l = listener_for(b, secure != 0, s);
                    l->maps.push_back(l->ours && l->maps.empty() ? line + ", *" : line);
                }
        }
        int defaultPort = 0;
        for (auto &l : listeners)
            if (ieq(l.name, "Default"))
                defaultPort = l.port;

        header();
        printf("\n");
        int count = 0;
        for (auto &s : sites)
            if (!s.imported)
                skippedTop.push_back(s.name + (s.httpPart ? " (HTTP)" : "") + "  " + s.skipWhy);
        bool port80 = false;
        for (auto &s : sites)
        {
            if (!s.imported)
                continue;
            ++count;
            SV ports;
            for (auto &b : s.httpBinds)
            {
                Bind shown = b;
                if (b.port == 80 && b.host == "*" && defaultPort)
                {
                    shown.port = defaultPort;
                    port80 = true;
                }
                ports.push_back("http " + (shown.host == "*" ? std::to_string(shown.port) : bind_text(shown)));
            }
            for (auto &b : s.httpsBinds)
                ports.push_back("https " + (b.host == "*" ? std::to_string(b.port) : bind_text(b)));
            printf("%s%s  ->  %s  [%s]\n", shown(s.name).c_str(), s.httpPart ? " (HTTP)" : "", shown(s.docroot).c_str(),
                   join(ports, ", ").c_str());
            SV extra(s.domains.begin() + (s.domains.empty() ? 0 : 1), s.domains.end());
            if (!extra.empty())
                printf("  also: %s\n", shown(join(extra, ", ")).c_str());
            if (!s.converted.empty())
                printf("  converted: %s\n", shown(join(s.converted, ", ")).c_str());
            for (auto &w : s.warnings)
                printf("  warning: %s\n", shown(w).c_str());
            for (auto &n : s.notes)
                printf("  note: %s\n", shown(n).c_str());
            size_t listed = 0;
            for (auto &k : s.skipped)
            {
                if (listed++ == 0)
                    printf("  not converted:\n");
                if (listed > 12)
                {
                    printf("    ... and %d more\n", (int)(s.skipped.size() - 12));
                    break;
                }
                printf("    %s\n", shown(k).c_str());
            }
            printf("\n");
        }
        if (!skippedTop.empty())
            printf("Skipped:\n");
        for (auto &k : skippedTop)
            printf("  %s\n", shown(k).c_str());
        if (!skippedTop.empty())
            printf("\n");
        if (port80 && defaultPort != 80)
            printf("HTTP sites answer on port %d, the port of the Default listener. Change it to 80 in WebAdmin "
                   "after Apache is stopped.\n", defaultPort);

        if (dryRun)
        {
            printf("%d site(s) would be imported. Nothing was changed (--dry-run).\n", count);
            return 0;
        }

        S out = base;
        SV gone;
        for (auto &v : oldVh)
        {
            bool kept = false;
            for (auto &s : sites)
                kept = kept || (s.imported && s.vhname == v);
            if (!kept)
                gone.push_back(v);
        }
        remove_dirs(gone);
        if (count == 0)
        {
            if (out != text && !write_file(httpdConf, out))
            {
                fprintf(stderr, "cannot write %s\n", httpdConf.c_str());
                return 1;
            }
            write_state(SV(), SV());
            printf("No sites imported.\n");
            return 0;
        }
        S nl = text.find("\r\n") != S::npos ? "\r\n" : "\n";
        auto fix_nl = [&](S s) {
            if (nl == "\n")
                return s;
            S r;
            for (char c : s)
            {
                if (c == '\n')
                    r += '\r';
                r += c;
            }
            return r;
        };
        bad.clear();
        S block = import_begin() + "\n";
        for (auto &s : sites)
        {
            if (!s.imported)
                continue;
            block += head("virtualhost", s.vhname) + kv("vhRoot", s.docroot + "/", 4)
                   + kv("configFile", "conf/vhosts/" + s.vhname + "/vhconf.conf", 4) + kv("allowSymbolLink", "1", 4)
                   + kv("enableScript", "1", 4) + kv("restrained", "0", 4) + "}\n\n";
        }
        for (auto &l : created)
        {
            S addr = l.host == "*" ? "*:" + std::to_string(l.port)
                                   : (l.host.find(':') != S::npos ? "[" + l.host + "]" : l.host) + ":" + std::to_string(l.port);
            block += head("listener", l.name) + kv("address", addr, 4)
                   + kv("secure", l.secure ? "1" : "0", 4);
            if (l.secure)
                block += kv("keyFile", l.key, 4) + kv("certFile", l.cert, 4) + kv("certChain", "1", 4);
            for (auto &m : l.maps)
                block += kv("map", m, 4);
            block += "}\n\n";
        }
        while (ends(block, "\n\n"))
            block.pop_back();
        block += import_end() + "\n";
        for (auto &l : listeners)
        {
            if (l.maps.empty())
                continue;
            size_t p = 0;
            S want = "listener " + l.name;
            while ((p = out.find(want, p)) != S::npos)
            {
                size_t le = out.find('\n', p);
                S head = trim(out.substr(p, le - p));
                if (head == want + " {" || (starts(head, want) && ends(head, "{") && trim(head.substr(want.size(), head.size() - want.size() - 1)).empty()))
                    break;
                p = le;
            }
            if (p == S::npos)
                continue;
            int depth = 0;
            size_t i = out.find('{', p), close = S::npos;
            for (; i < out.size(); ++i)
            {
                if (out[i] == '{')
                    ++depth;
                else if (out[i] == '}' && --depth == 0)
                {
                    close = i;
                    break;
                }
            }
            if (close == S::npos)
                continue;
            size_t ls = out.rfind('\n', close);
            ls = ls == S::npos ? 0 : ls + 1;
            S ins = "    " + maps_begin() + "\n";
            for (auto &m : l.maps)
                ins += kv("map", m, 4);
            ins += "    " + maps_end() + "\n";
            out.insert(ls, fix_nl(ins));
        }
        if (!out.empty() && !ends(out, "\n"))
            out += nl;
        out += nl + fix_nl(block);
        if (!bad.empty())
        {
            fprintf(stderr, "not imported, unsafe text in the server configuration: %s\n", bad[0].c_str());
            return 1;
        }
        for (auto &s : sites)
        {
            if (!s.imported)
                continue;
            S dir = olsRoot + "/conf/vhosts/" + s.vhname, prev, conf = fix_nl(s.conf);
            if (read_file(dir + "/vhconf.conf", prev) && prev != conf)
            {
                write_file(dir + "/vhconf.conf.bak", prev);
                printf("%s: the previous vhconf.conf was saved as vhconf.conf.bak\n", s.vhname.c_str());
            }
            if (!make_dirs(dir) || !write_file(dir + "/vhconf.conf", conf))
            {
                fprintf(stderr, "cannot write %s/vhconf.conf\n", dir.c_str());
                return 1;
            }
            if (!s.chain.empty() && !write_file(dir + "/cert-chain.pem", s.chain, true))
            {
                fprintf(stderr, "cannot write %s/cert-chain.pem\n", dir.c_str());
                return 1;
            }
        }
        write_file(httpdConf + ".bak", text);
        if (!write_file(httpdConf, out))
        {
            fprintf(stderr, "cannot write %s\n", httpdConf.c_str());
            return 1;
        }
        SV newVh, newLs;
        for (auto &s : sites)
            if (s.imported)
                newVh.push_back(s.vhname);
        for (auto &l : created)
            newLs.push_back(l.name);
        write_state(newVh, newLs);
        printf("%d site(s) imported into conf/vhosts. The previous configuration is in conf/httpd_config.conf.bak.\n",
               count);
        printf("Reload OpenLiteSpeed to apply: openlitespeed -s reload\n");
        return 0;
    }
};

struct NgxParser
{
    S confDir;
    SV files, missing;

    static bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

    static void unescape(char e, S &tok)
    {
        if (e == '"' || e == '\'' || e == '\\')
            tok += e;
        else if (e == 't')
            tok += '\t';
        else if (e == 'n')
            tok += '\n';
        else if (e == 'r')
            tok += '\r';
        else
        {
            tok += '\\';
            tok += e;
        }
    }

    std::set<S> chain;
    SV badSites;
    int budget = 4000;

    static bool harmless(const S &k)
    {
        static const char *list[] = {
            "add_header", "index", "error_page", "server_name", "access_log", "error_log", "log_format", "charset",
            "expires", "gzip", "gzip_types", "gzip_vary", "gzip_comp_level", "gzip_proxied", "gzip_min_length",
            "gzip_disable", "proxy_set_header", "proxy_hide_header", "fastcgi_param", "fastcgi_index",
            "fastcgi_split_path_info", "client_max_body_size", "keepalive_timeout", "sendfile", "tcp_nopush",
            "tcp_nodelay", "ssl_protocols", "ssl_ciphers", "ssl_prefer_server_ciphers", "ssl_session_cache",
            "ssl_session_timeout", "ssl_session_tickets", "ssl_ecdh_curve", "server_tokens", "default_type", "etag",
            "log_not_found", "limit_rate", "real_ip_header", "set_real_ip_from", "map", "types", "geo",
            "split_clients", "upstream",
        };
        for (const char *x : list)
            if (k == x)
                return true;
        return false;
    }

    void parse(const S &path, Node *parent, int depth)
    {
        S text, key = lower(real_file(path));
        if (depth > 16 || budget <= 0)
        {
            parent->taint(shown(path) + "  includes nested too deeply or too many included files", false);
            return;
        }
        if (chain.count(key))
        {
            missing.push_back(path + " (included by itself)");
            return;
        }
        --budget;
        if (!read_file(path, text))
        {
            missing.push_back(path);
            return;
        }
        files.push_back(path);
        chain.insert(key);
        parse_text(text, path, parent, depth);
        chain.erase(key);
    }

    void parse_text(const S &text, const S &path, Node *parent, int depth)
    {
        std::vector<Node *> stack{parent};
        SV toks;
        int line = 1, tokLine = 1;
        size_t i = 0, n = text.size();
        while (i < n)
        {
            char c = text[i];
            if (c == '\n')
            {
                ++line;
                ++i;
                continue;
            }
            if (space(c))
            {
                ++i;
                continue;
            }
            if (c == '#')
            {
                while (i < n && text[i] != '\n')
                    ++i;
                continue;
            }
            if (c == ';' || c == '{' || c == '}')
            {
                ++i;
                if (c == '}')
                {
                    toks.clear();
                    if (stack.size() > 1)
                        stack.pop_back();
                    continue;
                }
                if (toks.empty())
                    continue;
                Node *cur = stack.back();
                SV args(toks.begin() + 1, toks.end());
                S head = lower(toks[0]), where = base_name(path) + ":" + std::to_string(tokLine);
                if (!cur)
                {
                    if (c == '{')
                        stack.push_back(nullptr);
                    toks.clear();
                    continue;
                }
                if (c == ';' && head == "return" && !args.empty() && args[0].size() == 3
                    && args[0].find_first_not_of("0123456789") == S::npos && !clean_all(toks))
                {
                    if (atoi(args[0].c_str()) >= 400)
                        cur->add(toks[0], SV{args[0]}, toks[0] + " " + args[0], path, tokLine, false);
                    else if (atoi(args[0].c_str()) < 300)
                        cur->taint(where + "  " + brief(join(toks, " ")) + "  contains control characters or <<<", true);
                    else
                        cur->taint(where + "  " + brief(join(toks, " ")) + "  contains control characters or <<<", false);
                    toks.clear();
                    continue;
                }
                if (!clean_all(toks) || (c == '{' && cur->depth() > 48))
                {
                    if (c == '{')
                    {
                        stack.push_back(nullptr);
                        if (head == "server")
                            badSites.push_back(where + "  " + brief(join(toks, " ")) + "  contains control characters or <<<, or is nested too deeply");
                        else
                            cur->taint(where + "  " + brief(join(toks, " ")) + "  contains control characters or <<<, or is nested too deeply", harmless(head));
                    }
                    else
                        cur->taint(where + "  " + brief(join(toks, " ")) + "  contains control characters or <<<", harmless(head));
                    toks.clear();
                    continue;
                }
                if (c == ';' && head == "include" && !args.empty())
                {
                    S pat = resolve(confDir, args[0]);
                    SV got;
                    if (has_wild(pat))
                        glob(pat, got);
                    else
                        got.push_back(pat);
                    std::sort(got.begin(), got.end());
                    for (auto &f : got)
                        parse(f, cur, depth + 1);
                }
                else
                {
                    Node *k = cur->add(toks[0], args, join(toks, " "), path, tokLine, c == '{');
                    if (c == '{')
                        stack.push_back(k);
                }
                toks.clear();
                continue;
            }
            if (toks.empty())
                tokLine = line;
            S tok;
            if (c == '"' || c == '\'')
            {
                char q = c;
                ++i;
                while (i < n && text[i] != q)
                {
                    if (text[i] == '\\' && i + 1 < n)
                    {
                        unescape(text[i + 1], tok);
                        i += 2;
                        continue;
                    }
                    if (text[i] == '\n')
                        ++line;
                    tok += text[i++];
                }
                ++i;
            }
            else
            {
                while (i < n && !space(text[i]) && text[i] != ';' && text[i] != '}')
                {
                    if (text[i] == '{')
                    {
                        if (tok.empty() || tok.back() != '$')
                            break;
                        while (i < n && text[i] != '}')
                            tok += text[i++];
                        if (i < n)
                            tok += text[i++];
                        continue;
                    }
                    if (text[i] == '\\' && i + 1 < n)
                    {
                        unescape(text[i + 1], tok);
                        i += 2;
                        continue;
                    }
                    tok += text[i++];
                }
            }
            toks.push_back(tok);
        }
    }
};

struct NginxImporter : Importer
{
    S nginxConf, prefix, confDir;
    NgxParser np;
    std::map<S, S> upstreams;

    NginxImporter()
    {
        kind = "nginx";
        label = "nginx";
        listenerBase = "Nginx";
    }

    static bool ngx_known(const S &k)
    {
        static const char *known[] = {
            "listen", "server_name", "root", "alias", "index", "autoindex", "location", "allow", "deny",
            "auth_basic", "auth_basic_user_file", "return", "rewrite", "try_files", "error_page", "add_header",
            "proxy_pass", "fastcgi_pass", "fastcgi_param", "fastcgi_index", "fastcgi_split_path_info",
            "fastcgi_read_timeout", "fastcgi_buffers", "fastcgi_buffer_size", "fastcgi_intercept_errors",
            "fastcgi_hide_header", "fastcgi_keep_conn", "include", "ssl", "ssl_certificate", "ssl_certificate_key",
            "ssl_protocols", "ssl_ciphers", "ssl_prefer_server_ciphers", "ssl_session_cache", "ssl_session_timeout",
            "ssl_session_tickets", "ssl_dhparam", "ssl_stapling", "ssl_stapling_verify", "ssl_trusted_certificate",
            "ssl_ecdh_curve", "http2", "http3", "access_log", "error_log", "log_not_found", "charset",
            "client_max_body_size", "client_body_timeout", "client_header_timeout", "keepalive_timeout",
            "send_timeout", "sendfile", "tcp_nopush", "tcp_nodelay", "gzip", "gzip_types", "gzip_vary",
            "gzip_comp_level", "gzip_proxied", "gzip_min_length", "gzip_disable", "gzip_static", "expires", "etag",
            "server_tokens", "if", "proxy_set_header", "proxy_http_version", "proxy_read_timeout",
            "proxy_connect_timeout", "proxy_send_timeout", "proxy_buffering", "proxy_redirect", "proxy_cache_bypass",
            "proxy_buffers", "proxy_buffer_size", "proxy_hide_header", "open_file_cache", "open_file_cache_valid",
            "open_file_cache_errors", "open_file_cache_min_uses", "default_type", "types", "limit_rate",
            "internal", "set_real_ip_from", "real_ip_header",
        };
        for (const char *x : known)
            if (k == x)
                return true;
        return false;
    }

    S find_nginx(SV &found)
    {
        wchar_t d[MAX_PATH];
        auto try_add = [&](const S &p) {
            S n = norm(p);
            if (!is_file(n))
                return;
            for (auto &f : found)
                if (ieq(f, n))
                    return;
            found.push_back(n);
        };
        if (phpd_find_laragon(d))
        {
            S lr = norm(U(d)), ini, ver;
            if (read_file(lr + "/usr/laragon.ini", ini))
            {
                size_t a = ini.find("[nginx]");
                if (a != S::npos)
                {
                    size_t v = ini.find("Version=", a), nx = ini.find("[", a + 1);
                    if (v != S::npos && (nx == S::npos || v < nx))
                        ver = trim(ini.substr(v + 8, ini.find('\n', v) - v - 8));
                }
            }
            if (!ver.empty())
                try_add(lr + "/bin/nginx/" + ver + "/conf/nginx.conf");
            else
            {
                SV names;
                list_dir(lr + "/bin/nginx", names, true);
                for (auto &n : names)
                    try_add(lr + "/bin/nginx/" + n + "/conf/nginx.conf");
            }
        }
        if (phpd_running_exe_dir(L"nginx.exe", d))
            try_add(norm(U(d)) + "/conf/nginx.conf");
        wchar_t drives[128];
        if (GetLogicalDriveStringsW(128, drives))
            for (wchar_t *p = drives; *p; p += wcslen(p) + 1)
            {
                if (GetDriveTypeW(p) != DRIVE_FIXED)
                    continue;
                S r = norm(U(p));
                SV names;
                list_dir(r, names, true);
                for (auto &n : names)
                    if (istarts(n, "nginx"))
                        try_add(r + "/" + n + "/conf/nginx.conf");
                try_add(r + "/WinNMP/conf/nginx.conf");
            }
        return found.size() == 1 ? found[0] : S();
    }

    void member_of(const Node *s, Member &m, SV &notes)
    {
        m.n = s;
        for (auto &k : s->kids)
        {
            if (k->key == "listen" && !k->args.empty())
            {
                S a = k->args[0];
                if (istarts(a, "unix:"))
                    continue;
                Bind b;
                if (starts(a, "["))
                {
                    size_t e = a.find(']');
                    if (e == S::npos)
                        continue;
                    b.host = a.substr(1, e - 1);
                    b.port = e + 1 < a.size() && a[e + 1] == ':' ? atoi(a.c_str() + e + 2) : 80;
                }
                else if (a.find(':') != S::npos)
                {
                    size_t c = a.rfind(':');
                    b.host = a.substr(0, c);
                    b.port = atoi(a.c_str() + c + 1);
                }
                else if (a.find_first_not_of("0123456789") == S::npos)
                    b.port = atoi(a.c_str());
                else
                {
                    b.host = a;
                    b.port = 80;
                }
                if (b.host == "*" || b.host == "0.0.0.0" || b.host == "::")
                    b.host = "*";
                if (ieq(b.host, "localhost"))
                    b.host = "127.0.0.1";
                if (!valid_host(b.host) || b.port < 0 || b.port > 65535)
                {
                    m.skip = "listen " + brief(a) + " is not a valid address";
                    continue;
                }
                for (size_t i = 1; i < k->args.size(); ++i)
                    if (k->args[i] == "ssl")
                        m.ssl = true;
                if (b.port > 0 && std::find(m.binds.begin(), m.binds.end(), b) == m.binds.end())
                    m.binds.push_back(b);
            }
            else if (k->key == "server_name")
                for (auto &a : k->args)
                {
                    S d = lower(a);
                    if (d.empty() || d == "_")
                        continue;
                    if (starts(d, "~"))
                    {
                        notes.push_back(k->where() + "  server name " + a + " is a regular expression, not converted");
                        continue;
                    }
                    if (starts(d, "."))
                    {
                        d = d.substr(1);
                        add_unique(m.aliases, "*." + d);
                    }
                    if (ends(d, ".*"))
                    {
                        notes.push_back(k->where() + "  server name " + a + " is not supported");
                        continue;
                    }
                    if (m.name.empty() && d.find('*') == S::npos)
                        m.name = d;
                    else if (d != m.name)
                        add_unique(m.aliases, d);
                }
            else if (k->key == "root" && !k->args.empty())
                m.docroot = real_path(resolve(prefix, k->args[0]));
            else if (k->key == "ssl_certificate" && !k->args.empty())
                m.cert = real_file(resolve(confDir, k->args[0]));
            else if (k->key == "ssl_certificate_key" && !k->args.empty())
                m.key = real_file(resolve(confDir, k->args[0]));
            else if (k->key == "ssl" && !k->args.empty() && ieq(k->args[0], "on"))
                m.ssl = true;
        }
        if (m.name.empty() && !m.aliases.empty())
        {
            m.name = m.aliases[0];
            m.aliases.erase(m.aliases.begin());
        }
        if (m.binds.empty())
            m.binds.push_back(Bind{"*", m.ssl ? 443 : 80});
    }

    bool redirect_only(const Node *s) const
    {
        bool redirect = false;
        for (auto &k : s->kids)
        {
            if (k->key == "listen" || k->key == "server_name" || k->key == "access_log" || k->key == "error_log"
                || k->key == "server_tokens")
                continue;
            if (k->key == "return" && k->args.size() >= 2 && (k->args[0] == "301" || k->args[0] == "302"
                                                              || k->args[0] == "307" || k->args[0] == "308")
                && istarts(k->args[1], "https://"))
            {
                redirect = true;
                continue;
            }
            if (k->key == "rewrite" && k->args.size() >= 2 && istarts(k->args[1], "https://"))
            {
                redirect = true;
                continue;
            }
            return false;
        }
        return redirect;
    }

    static S canonical(const Node *s)
    {
        S out;
        for (auto &k : s->kids)
        {
            if (k->key == "listen" || istarts(k->key, "ssl") || k->key == "http2" || k->key == "server_name")
                continue;
            out += k->raw + (k->block ? "{" + canonical(k.get()) + "}" : ";");
        }
        return out;
    }

    S var_map(S v, const Site &site, bool &ok, SV &notes, const Node *where)
    {
        static const char *drop[] = { "$is_args$args", "?$query_string", "?$args", "$is_args$query_string" };
        for (const char *d : drop)
        {
            size_t p;
            while ((p = v.find(d)) != S::npos)
                v.erase(p, strlen(d));
        }
        S out;
        for (size_t i = 0; i < v.size();)
        {
            if (v[i] != '$')
            {
                out += v[i++];
                continue;
            }
            size_t j = i + 1;
            bool brace = j < v.size() && v[j] == '{';
            if (brace)
                ++j;
            size_t s = j;
            while (j < v.size() && (isalnum((unsigned char)v[j]) || v[j] == '_'))
                ++j;
            S name = v.substr(s, j - s);
            if (brace && j < v.size() && v[j] == '}')
                ++j;
            if (!name.empty() && isdigit((unsigned char)name[0]))
                out += "$" + name;
            else if (name == "host")
                out += "%{SERVER_NAME}";
            else if (name == "http_host")
                out += "%{HTTP_HOST}";
            else if (name == "server_name")
                out += site.name;
            else if (name == "request_uri" || name == "uri" || name == "document_uri")
                out += "%{REQUEST_URI}";
            else if (name == "args" || name == "query_string")
                out += "%{QUERY_STRING}";
            else
            {
                ok = false;
                notes.push_back(where->where() + "  " + where->raw + "  uses $" + name + ", not converted");
                return S();
            }
            i = j;
        }
        return out;
    }

    static S redirect_flag(const S &code) { return "[R=" + code + ",L]"; }

    SV return_rules(const Node *k, const Site &site, SV &notes)
    {
        SV out;
        if (k->args.empty())
            return out;
        S code = k->args[0], url = k->args.size() > 1 ? k->args[1] : S();
        if (code.find_first_not_of("0123456789") != S::npos)
        {
            url = code;
            code = "302";
        }
        bool ok = true;
        if (code == "301" || code == "302" || code == "303" || code == "307" || code == "308")
        {
            S target = var_map(url, site, ok, notes, k);
            if (ok && !target.empty())
                out.push_back("RewriteRule ^ " + quote_arg(target) + " " + redirect_flag(code));
        }
        else if (code == "403" && url.empty())
            out.push_back("RewriteRule ^ - [F,L]");
        else if (code == "410" && url.empty())
            out.push_back("RewriteRule ^ - [G,L]");
        else
            notes.push_back(k->where() + "  " + k->raw + "  not converted");
        return out;
    }

    SV rewrite_rule(const Node *k, const Site &site, SV &notes)
    {
        SV out;
        if (k->args.size() < 2)
            return out;
        bool ok = true;
        S target = var_map(k->args[1], site, ok, notes, k);
        if (!ok)
            return out;
        S flag = k->args.size() > 2 ? k->args[2] : S();
        SV fl;
        bool absolute = istarts(target, "http://") || istarts(target, "https://");
        if (flag == "permanent")
            fl = {"R=301", "L"};
        else if (flag == "redirect" || (flag.empty() && absolute))
            fl = {"R=302", "L"};
        else if (flag == "last" || flag == "break")
            fl = {"L"};
        if (target.find('?') != S::npos && !ends(target, "?"))
            fl.push_back("QSA");
        out.push_back("RewriteRule " + quote_arg(k->args[0]) + " " + quote_arg(target)
                      + (fl.empty() ? S() : " [" + join(fl, ",") + "]"));
        return out;
    }

    SV if_rules(const Node *k, const Site &site, SV &notes)
    {
        SV out, conds, t;
        S expr = trim(join(k->args, " "));
        if (starts(expr, "("))
            expr = expr.substr(1);
        if (ends(expr, ")"))
            expr.pop_back();
        for (size_t i = 0; i < expr.size();)
        {
            size_t j = expr.find_first_of(" \t", i);
            if (j == S::npos)
                j = expr.size();
            if (j > i)
                t.push_back(expr.substr(i, j - i));
            i = j + 1;
        }
        S a0 = t.size() > 0 ? t[0] : S(), a1 = t.size() > 1 ? t[1] : S(), a2 = t.size() > 2 ? t[2] : S();
        if (a0 == "$scheme" && ((a1 == "=" && a2 == "http") || (a1 == "!=" && a2 == "https")))
            conds.push_back("RewriteCond %{HTTPS} !on");
        else if ((a0 == "!-e" || a0 == "!-f") && a1 == "$request_filename")
        {
            conds.push_back("RewriteCond %{REQUEST_FILENAME} !-f");
            if (a0 == "!-e")
                conds.push_back("RewriteCond %{REQUEST_FILENAME} !-d");
        }
        else if (a0 == "$host" && (a1 == "=" || a1 == "!=") && !a2.empty())
            conds.push_back("RewriteCond %{HTTP_HOST} " + S(a1 == "!=" ? "!" : "") + "^" + re_escape(a2) + "(:[0-9]+)?$ [NC]");
        else
        {
            notes.push_back(k->where() + "  " + k->raw + "  not converted");
            return out;
        }
        for (auto &x : k->kids)
        {
            SV r;
            if (x->key == "return")
                r = return_rules(x.get(), site, notes);
            else if (x->key == "rewrite")
                r = rewrite_rule(x.get(), site, notes);
            else
            {
                notes.push_back(x->where() + "  " + x->raw + "  inside if, not converted");
                continue;
            }
            for (auto &rule : r)
            {
                out.insert(out.end(), conds.begin(), conds.end());
                out.push_back(rule);
            }
        }
        return out;
    }

    SV try_rules(const Node *k, const Site &site, SV &notes, const std::map<S, const Node *> &named,
                 std::vector<Proxy> &prox)
    {
        SV out, a = k->args;
        if (a.size() < 2)
            return out;
        S fallback = a.back();
        a.pop_back();
        bool file = false, dir = false;
        for (auto &x : a)
        {
            if (x == "$uri")
                file = true;
            else if (x == "$uri/")
                dir = true;
            else
            {
                notes.push_back(k->where() + "  " + k->raw + "  not converted");
                return out;
            }
        }
        if (starts(fallback, "="))
            return out;
        SV conds;
        if (file)
            conds.push_back("RewriteCond %{REQUEST_FILENAME} !-f");
        if (dir)
            conds.push_back("RewriteCond %{REQUEST_FILENAME} !-d");
        if (starts(fallback, "@"))
        {
            auto it = named.find(fallback);
            if (it != named.end())
                for (auto &x : it->second->kids)
                    if (x->key == "proxy_pass" && !x->args.empty())
                    {
                        S url = upstream_url(x->args[0]);
                        if (istarts(url, "http://"))
                        {
                            out = conds;
                            out.push_back("__PROXY__ " + url);
                            return out;
                        }
                    }
            notes.push_back(k->where() + "  " + k->raw + "  not converted");
            return out;
        }
        bool ok = true;
        S target = var_map(fallback, site, ok, notes, k);
        if (!ok)
            return out;
        out = conds;
        out.push_back("RewriteRule ^ " + quote_arg(target) + " [L]");
        return out;
    }

    S upstream_url(const S &u)
    {
        S scheme = istarts(u, "https://") ? "https://" : istarts(u, "http://") ? "http://" : S();
        if (scheme.empty())
            return u;
        S rest = u.substr(scheme.size());
        size_t sl = rest.find('/');
        S host = sl == S::npos ? rest : rest.substr(0, sl), path = sl == S::npos ? S() : rest.substr(sl);
        auto it = upstreams.find(lower(host));
        if (it != upstreams.end())
            host = it->second;
        return scheme + host + path;
    }

    static bool ip4(const S &s, unsigned &addr, int &bits)
    {
        unsigned a[4];
        int n = 0;
        char tail[8] = { 0 };
        bits = 32;
        S x = s;
        size_t sl = x.find('/');
        if (sl != S::npos)
        {
            bits = atoi(x.c_str() + sl + 1);
            x = x.substr(0, sl);
        }
        if (sscanf(x.c_str(), "%u.%u.%u.%u%1s%n", &a[0], &a[1], &a[2], &a[3], tail, &n) != 4 || bits < 0 || bits > 32)
            return false;
        for (unsigned v : a)
            if (v > 255)
                return false;
        addr = (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
        return true;
    }

    static bool covers(const S &net, const S &ip)
    {
        unsigned na, ia;
        int nb, ib;
        if (!ip4(net, na, nb) || !ip4(ip, ia, ib))
            return ieq(net, ip);
        if (ib < nb)
            return false;
        unsigned mask = nb == 0 ? 0 : 0xFFFFFFFFu << (32 - nb);
        return (na & mask) == (ia & mask);
    }

    Access access_of(const std::vector<std::pair<bool, S>> &rules, SV &warns, const S &where)
    {
        Access a;
        SV allows, denies;
        bool endAllow = true;
        for (auto &r : rules)
        {
            if (ieq(r.second, "all"))
            {
                endAllow = r.first;
                break;
            }
            if (!ip_token(r.second))
            {
                warns.push_back(where + "  " + S(r.first ? "allow " : "deny ") + brief(r.second)
                                + (r.first ? "  not converted" : "  not converted, access denied"));
                if (!r.first)
                {
                    a.denied = true;
                    return a;
                }
                continue;
            }
            S ip = ols_ip(r.second);
            bool shadowed = false;
            for (auto &o : r.first ? denies : allows)
                if (covers(o, ip))
                    shadowed = true;
            if (shadowed)
            {
                warns.push_back(where + "  " + S(r.first ? "allow " : "deny ") + r.second
                                + " follows a rule that already matches it, nginx ignores it, so does OpenLiteSpeed");
                continue;
            }
            (r.first ? allows : denies).push_back(ip);
        }
        if (endAllow)
            a.deny = denies;
        else if (allows.empty())
            a.denied = true;
        else
        {
            a.allow = allows;
            a.deny = denies;
        }
        return a;
    }

    struct Loc
    {
        const Node *n;
        S mod, path, root;
        int depth, parent;
    };

    struct Rules
    {
        std::vector<std::pair<bool, S>> acc;
        S authName, userFile;
        bool authOff = false, internal = false;
    };

    static void scan_rules(const Node *scope, const Rules &base, Rules &r)
    {
        bool accSet = false;
        r = base;
        r.internal = false;
        for (auto &k : scope->kids)
        {
            if ((k->key == "allow" || k->key == "deny") && !k->args.empty())
            {
                if (!accSet)
                    r.acc.clear();
                accSet = true;
                r.acc.push_back({k->key == "allow", k->args[0]});
            }
            else if (k->key == "auth_basic" && !k->args.empty())
            {
                r.authOff = ieq(k->args[0], "off");
                r.authName = r.authOff ? S() : k->args[0];
            }
            else if (k->key == "auth_basic_user_file" && !k->args.empty())
                r.userFile = k->args[0];
            else if (k->key == "internal")
                r.internal = true;
        }
    }

    void collect_locs(const Node *scope, const S &root, int depth, std::vector<Loc> &out,
                      std::map<S, const Node *> &named, int parent)
    {
        for (auto &k : scope->kids)
        {
            if (k->key != "location" || !k->block || k->args.empty())
                continue;
            Loc l{k.get(), "", "", root, depth, parent};
            if (k->args.size() >= 2)
            {
                l.mod = k->args[0];
                l.path = k->args[1];
            }
            else
            {
                l.path = k->args[0];
                for (const char *m : {"^~", "~*", "~", "="})
                    if (starts(l.path, m) && l.path.size() > strlen(m))
                    {
                        l.mod = m;
                        l.path = l.path.substr(strlen(m));
                        break;
                    }
            }
            if (starts(l.path, "@"))
            {
                named[l.path] = k.get();
                continue;
            }
            for (auto &x : k->kids)
                if (x->key == "root" && !x->args.empty())
                    l.root = real_path(resolve(prefix, x->args[0]));
            out.push_back(l);
            collect_locs(k.get(), l.root, depth + 1, out, named, (int)out.size() - 1);
        }
    }

    SV select_conds(size_t i, const std::vector<Loc> &locs) const
    {
        SV out;
        const Loc &l = locs[i];
        bool lre = l.mod == "~" || l.mod == "~*";
        if (l.mod == "=")
            return out;
        for (size_t j = 0; j < locs.size(); ++j)
        {
            if (j == i)
                continue;
            const Loc &o = locs[j];
            bool ore = o.mod == "~" || o.mod == "~*";
            if (o.mod == "=")
            {
                if (lre || starts(o.path, l.path))
                    out.push_back("RewriteCond %{REQUEST_URI} !^" + re_escape(o.path) + "$");
            }
            else if (ore)
            {
                if ((lre && j < i) || (!lre && l.mod != "^~"))
                    out.push_back("RewriteCond %{REQUEST_URI} " + quote_arg("!" + o.path) + (o.mod == "~*" ? " [NC]" : ""));
            }
            else if (lre)
            {
                if (o.mod == "^~")
                    out.push_back("RewriteCond %{REQUEST_URI} !^" + re_escape(o.path));
            }
            else if (o.path.size() > l.path.size() && starts(o.path, l.path))
                out.push_back("RewriteCond %{REQUEST_URI} !^" + re_escape(o.path));
        }
        return out;
    }

    S loc_cond(const Loc &l) const
    {
        if (l.mod == "=")
            return "RewriteCond %{REQUEST_URI} ^" + re_escape(l.path) + "$";
        if (l.mod == "~")
            return "RewriteCond %{REQUEST_URI} " + quote_arg(l.path);
        if (l.mod == "~*")
            return "RewriteCond %{REQUEST_URI} " + quote_arg(l.path) + " [NC]";
        if (l.path == "/")
            return S();
        return "RewriteCond %{REQUEST_URI} ^" + re_escape(l.path);
    }

    void build(Site &site)
    {
        const Member *prim = &site.members[0];
        for (auto &m : site.members)
            if (m.ssl)
            {
                site.cert = m.cert;
                site.key = m.key;
                prim = &m;
                break;
            }
        const Node *srv = prim->n;
        site.file = srv->where();
        std::vector<Loc> locs;
        std::map<S, const Node *> named;
        S serverRoot = prim->docroot;
        collect_locs(srv, serverRoot, 0, locs, named, -1);
        S docroot = serverRoot;
        if (docroot.empty())
            for (auto &l : locs)
                if (l.mod.empty() && l.path == "/" && !l.root.empty())
                    docroot = l.root;
        if (docroot.empty())
            docroot = real_path(prefix + "/html");
        site.docroot = docroot;
        if (!is_dir(docroot))
        {
            site.skipWhy = "document root " + docroot + " does not exist";
            return;
        }
        if (!clean(docroot))
        {
            site.skipWhy = "the document root path contains control characters or <<<";
            return;
        }
        bad.clear();
        for (auto &l : locs)
            if (l.root.empty())
                l.root = docroot;

        SV rootIndex{"index.html"};
        int rootIdx = 0;
        std::map<S, S> errors;
        SV srvRules, notes, warns;
        bool php = false;
        SV fcgiAddrs;
        std::vector<Proxy> prox;
        std::map<S, SV> realms;
        std::vector<Ctx> exact, regex, prefixes;
        SV caret;
        std::vector<std::pair<int, SV>> locRules;

        auto scan_common = [&](const Node *k, Ctx &c, bool &need) {
            if (k->key == "allow" || k->key == "deny" || k->key == "auth_basic" || k->key == "auth_basic_user_file"
                || k->key == "internal")
                return true;
            if (k->key == "add_header" && k->args.size() >= 2)
            {
                c.headers.push_back("Header add " + k->args[0] + " " + quote_arg(k->args[1]));
                need = true;
                return true;
            }
            return false;
        };
        auto apply_rules = [&](Ctx &c, const Rules &r, const S &where) {
            c.acc = r.acc.empty() ? Access() : access_of(r.acc, warns, where);
            c.realm.clear();
            c.authName.clear();
            if (!r.authOff && !r.authName.empty() && !r.userFile.empty())
            {
                Conf cf;
                cf.userFile = real_file(resolve(confDir, r.userFile));
                if (!clean(cf.userFile) || !is_file(cf.userFile))
                {
                    add_unique(notes, where + "  password file " + brief(cf.userFile) + " not found, access denied");
                    c.acc.denied = true;
                }
                else
                {
                    c.realm = realm_for(cf, realms, site);
                    c.authName = clean(r.authName) ? r.authName : S("Restricted");
                    c.acc.required = "valid-user";
                }
            }
            if (r.internal)
            {
                c.acc = Access();
                c.acc.denied = true;
                c.realm.clear();
            }
        };
        auto same_access = [](const Ctx &a, const Ctx &b) {
            return a.acc == b.acc && a.realm == b.realm && a.authName == b.authName;
        };

        Rules srvAcc;
        scan_rules(srv, Rules(), srvAcc);
        std::vector<Rules> locEff(locs.size());
        for (size_t i = 0; i < locs.size(); ++i)
            scan_rules(locs[i].n, locs[i].parent >= 0 ? locEff[locs[i].parent] : srvAcc, locEff[i]);
        Ctx rootCtx, rootEff;
        rootCtx.uri = "/";
        apply_rules(rootCtx, srvAcc, srv->where());
        rootEff = rootCtx;
        for (size_t i = 0; i < locs.size(); ++i)
            if (locs[i].mod.empty() && locs[i].path == "/" && locs[i].parent < 0)
                apply_rules(rootEff, locEff[i], locs[i].n->where());
        bool rootNeed = !rootCtx.acc.open() || !rootCtx.realm.empty();
        {
            for (auto &k : srv->kids)
            {
                if (scan_common(k.get(), rootCtx, rootNeed))
                    continue;
                if (k->key == "index" && !k->args.empty())
                    rootIndex = k->args;
                else if (k->key == "autoindex" && !k->args.empty())
                    rootIdx = ieq(k->args[0], "on") ? 1 : 0;
                else if (k->key == "error_page" && k->args.size() >= 2)
                {
                    S target = k->args.back();
                    if (starts(target, "=") || starts(target, "@") || (k->args.size() >= 3 && starts(k->args[k->args.size() - 2], "=")))
                        notes.push_back(k->where() + "  " + k->raw + "  not converted");
                    else
                        for (size_t i = 0; i + 1 < k->args.size(); ++i)
                            if (k->args[i].size() == 3 && k->args[i].find_first_not_of("0123456789") == S::npos)
                                errors[k->args[i]] = target;
                }
                else if (k->key == "return")
                {
                    SV r = return_rules(k.get(), site, notes);
                    srvRules.insert(srvRules.end(), r.begin(), r.end());
                }
                else if (k->key == "rewrite")
                {
                    SV r = rewrite_rule(k.get(), site, notes);
                    srvRules.insert(srvRules.end(), r.begin(), r.end());
                }
                else if (k->key == "if")
                {
                    SV r = if_rules(k.get(), site, notes);
                    srvRules.insert(srvRules.end(), r.begin(), r.end());
                }
                else if (k->key == "client_max_body_size")
                    notes.push_back(k->where() + "  " + k->raw + "  OpenLiteSpeed sets the request body limit for the server");
                else if (!ngx_known(k->key))
                    site.skipped.push_back(k->where() + "  " + k->raw);
            }
        }

        int seq = 0;
        for (auto &l : locs)
        {
            const Node *n = l.n;
            if (l.depth > 0)
                notes.push_back(n->where() + "  nested location " + l.path + " is converted as a location of its own");
            Ctx c;
            bool need = false, locPhp = false, isRegex = l.mod == "~" || l.mod == "~*";
            S alias;
            SV rules;
            for (auto &k : n->kids)
            {
                if (scan_common(k.get(), c, need))
                    continue;
                if (k->key == "alias" && !k->args.empty())
                {
                    alias = resolve(prefix, k->args[0]);
                    if (ends(k->args[0], "/") && !ends(alias, "/"))
                        alias += "/";
                    need = true;
                }
                else if (k->key == "autoindex" && !k->args.empty())
                {
                    c.autoIndex = ieq(k->args[0], "on") ? 1 : 0;
                    need = true;
                }
                else if (k->key == "index" && !k->args.empty())
                {
                    c.index = k->args;
                    need = true;
                }
                else if (k->key == "return")
                {
                    SV r = return_rules(k.get(), site, notes);
                    rules.insert(rules.end(), r.begin(), r.end());
                }
                else if (k->key == "rewrite")
                {
                    SV r = rewrite_rule(k.get(), site, notes);
                    rules.insert(rules.end(), r.begin(), r.end());
                }
                else if (k->key == "try_files")
                {
                    SV r = try_rules(k.get(), site, notes, named, prox);
                    if (!r.empty() && starts(r.back(), "__PROXY__ "))
                    {
                        S url = r.back().substr(10);
                        r.pop_back();
                        Proxy p{l.path, url, false};
                        rules.insert(rules.end(), r.begin(), r.end());
                        rules.push_back("__PROXYRULE__ " + std::to_string(prox.size()));
                        prox.push_back(p);
                    }
                    else
                        rules.insert(rules.end(), r.begin(), r.end());
                }
                else if (k->key == "proxy_pass" && !k->args.empty())
                {
                    S url = upstream_url(k->args[0]);
                    if (isRegex || l.mod == "=" || !istarts(url, "http://"))
                        notes.push_back(k->where() + "  " + k->raw + "  not converted");
                    else
                    {
                        rules.push_back("__PROXYRULE__ " + std::to_string(prox.size()));
                        prox.push_back(Proxy{l.path, url, false});
                    }
                }
                else if (k->key == "fastcgi_pass" && !k->args.empty())
                {
                    locPhp = php = true;
                    add_unique(fcgiAddrs, upstream_url(k->args[0]));
                }
                else if (k->key == "if")
                {
                    SV r = if_rules(k.get(), site, notes);
                    rules.insert(rules.end(), r.begin(), r.end());
                }
                else if (k->key == "root" || k->key == "location")
                    ;
                else if (k->key == "client_max_body_size")
                    notes.push_back(k->where() + "  " + k->raw + "  OpenLiteSpeed sets the request body limit for the server");
                else if (!ngx_known(k->key))
                    site.skipped.push_back(k->where() + "  " + k->raw);
            }
            apply_rules(c, locEff[seq], n->where());
            need = need || !same_access(c, rootEff);
            if (!alias.empty())
                c.location = alias;
            else if (!ieq(l.root, docroot))
            {
                if (isRegex)
                    notes.push_back(n->where() + "  root " + l.root + " in a regex location is not converted");
                else
                    c.location = l.root + (l.mod == "=" ? l.path : (ends(l.path, "/") ? l.path : l.path + "/"));
                need = need || !isRegex;
            }
            if (!rules.empty())
            {
                S cond = loc_cond(l);
                SV sel = select_conds(seq, locs), withCond, pending;
                for (auto &r : rules)
                {
                    if (istarts(r, "RewriteCond"))
                    {
                        pending.push_back(r);
                        continue;
                    }
                    if (!cond.empty())
                        withCond.push_back(cond);
                    withCond.insert(withCond.end(), sel.begin(), sel.end());
                    withCond.insert(withCond.end(), pending.begin(), pending.end());
                    pending.clear();
                    withCond.push_back(r);
                }
                int rank = l.mod == "=" ? 0 : isRegex ? 1000 + seq : 1000000 - (int)l.path.size();
                locRules.push_back({rank, withCond});
            }
            ++seq;
            if (l.mod.empty() && l.path == "/")
            {
                if (c.autoIndex >= 0)
                    rootIdx = c.autoIndex;
                if (!c.index.empty())
                    rootIndex = c.index;
                c.autoIndex = -1;
                c.index.clear();
                rootCtx.acc = c.acc;
                rootCtx.realm = c.realm;
                rootCtx.authName = c.authName;
                if (!c.headers.empty())
                    rootCtx.headers = c.headers;
                if (!c.location.empty())
                    rootCtx.location = c.location;
                rootNeed = !rootCtx.acc.open() || !rootCtx.realm.empty() || !rootCtx.headers.empty()
                        || !rootCtx.location.empty();
                continue;
            }
            if (!need)
            {
                if (locPhp && isRegex)
                    site.converted.push_back("PHP location " + l.path);
                continue;
            }
            if (l.mod == "^~")
                caret.push_back(l.path);
            if (l.mod == "=")
            {
                c.uri = "exp:^" + re_escape(l.path) + "$";
                exact.push_back(c);
            }
            else if (isRegex)
            {
                c.uri = "exp:" + S(l.mod == "~*" ? "(?i)" : "") + l.path;
                regex.push_back(c);
            }
            else
            {
                c.uri = l.path;
                prefixes.push_back(c);
            }
        }

        S conf;
        conf += mark() + ": " + shown(site.file) + "\n";
        conf += kv("docRoot", site.docroot + "/", 0);
        conf += kv("enableGzip", "1", 0);
        conf += kv("enableBr", "1", 0);
        conf += "\nindex  {\n" + kv("useServer", "0", 2) + kv("indexFiles", join(rootIndex, ", "), 2)
              + kv("autoIndex", std::to_string(rootIdx), 2);
        if (rootIdx == 1)
            conf += kv("autoIndexURI", "/_autoindex/default.php", 2);
        conf += "}\n";
        for (auto &e : errors)
        {
            if (starts(e.second, "/") || istarts(e.second, "http://") || istarts(e.second, "https://"))
                conf += "\n" + head("errorPage", e.first) + kv("url", e.second, 2) + "}\n";
        }
        for (auto &r : realms)
            conf += "\n" + head("realm", r.second[0]) + "  userDB  {\n" + kv("location", r.second[1], 4) + "  }\n}\n";
        if (php)
        {
            conf += "\nscriptHandler  {\n" + kv("add", phpHandler + " php", 2) + "}\n";
            site.converted.push_back("PHP through OpenLiteSpeed (" + phpHandler + ")");
            if (!fcgiAddrs.empty())
                site.notes.push_back("nginx passed PHP to the FastCGI server at " + join(fcgiAddrs, ", ")
                                     + ", OpenLiteSpeed runs PHP itself");
        }

        std::map<S, S> procNames;
        SV procLines;
        for (auto &p : prox)
        {
            S rest = p.url.substr(7);
            size_t sl = rest.find('/');
            S hostport = sl == S::npos ? rest : rest.substr(0, sl);
            S path = sl == S::npos ? p.path : rest.substr(sl);
            if (hostport.find(':') == S::npos)
                hostport += ":80";
            S &nm = procNames[lower(hostport)];
            if (nm.empty())
            {
                nm = "nginx-proxy-" + std::to_string(++procSeq);
                conf += "\n" + head("extprocessor", nm) + kv("type", "proxy", 2) + kv("address", hostport, 2)
                      + kv("maxConns", "100", 2) + kv("initTimeout", "60", 2) + kv("retryTimeout", "0", 2)
                      + kv("respBuffer", "0", 2) + "}\n";
            }
            procLines.push_back("RewriteRule " + quote_arg("^" + re_escape(p.path) + "(.*)$") + " "
                                + quote_arg("http://" + nm + path + "$1") + " [P,L]");
            site.converted.push_back("proxy " + p.path + " -> " + p.url);
        }

        std::vector<Ctx> ranked;
        for (auto &c : exact)
            ranked.push_back(c);
        for (auto &c : regex)
        {
            S pat = c.uri.substr(4);
            S ci;
            if (starts(pat, "(?i)"))
            {
                ci = "(?i)";
                pat = pat.substr(4);
            }
            if (!caret.empty())
            {
                SV esc;
                for (auto &p : caret)
                    esc.push_back(re_escape(p));
                pat = "^(?!" + join(esc, "|") + ")" + (starts(pat, "^") ? pat.substr(1) : ".*?(?:" + pat + ")");
            }
            else if (!starts(pat, "^"))
                pat = "^.*?(?:" + pat + ")";
            c.uri = "exp:" + ci + pat;
            ranked.push_back(c);
        }
        for (size_t i = 0; i < ranked.size(); ++i)
        {
            char rank[16];
            snprintf(rank, sizeof(rank), "(?#%04d)", (int)i);
            ranked[i].uri = "exp:" + S(rank) + ranked[i].uri.substr(4);
            conf += "\n" + render_ctx(ranked[i]);
        }
        if (rootNeed)
            prefixes.push_back(rootCtx);
        std::stable_sort(prefixes.begin(), prefixes.end(), [](const Ctx &a, const Ctx &b) { return a.uri < b.uri; });
        for (auto &c : prefixes)
            conf += "\n" + render_ctx(c);

        SV allRules;
        if (site.members.size() == 2)
            for (auto &m : site.members)
                if (!m.ssl && redirect_only(m.n))
                {
                    allRules.push_back("RewriteCond %{HTTPS} !on");
                    SV r;
                    for (auto &k : m.n->kids)
                        if (k->key == "return")
                            r = return_rules(k.get(), site, notes);
                        else if (k->key == "rewrite")
                            r = rewrite_rule(k.get(), site, notes);
                    if (r.empty())
                        allRules.pop_back();
                    else
                    {
                        allRules.insert(allRules.end(), r.begin(), r.end());
                        site.converted.push_back("HTTP to HTTPS redirect");
                    }
                }
        allRules.insert(allRules.end(), srvRules.begin(), srvRules.end());
        std::stable_sort(locRules.begin(), locRules.end(),
                         [](const std::pair<int, SV> &a, const std::pair<int, SV> &b) { return a.first < b.first; });
        for (auto &lr : locRules)
            for (auto &r : lr.second)
            {
                if (starts(r, "__PROXYRULE__ "))
                    allRules.push_back(procLines[atoi(r.c_str() + 14)]);
                else
                    allRules.push_back(r);
            }
        int nRules = 0;
        for (auto &r : allRules)
            if (istarts(r, "RewriteRule"))
                ++nRules;
        conf += "\nrewrite  {\n" + kv("enable", "1", 2) + kv("autoLoadHtaccess", "0", 2);
        if (!allRules.empty())
            conf += heredoc("rules", allRules, 2);
        conf += "}\n";

        if (!site.cert.empty())
        {
            conf += "\nvhssl  {\n" + kv("keyFile", site.key.empty() ? site.cert : site.key, 2)
                  + kv("certFile", site.cert, 2) + kv("certChain", "1", 2) + "}\n";
            site.converted.push_back("SSL certificate");
        }
        if (nRules)
            site.converted.push_back(std::to_string(nRules) + " rewrite and redirect rule" + (nRules > 1 ? "s" : ""));
        int restricted = 0, aliases = 0, headers = 0;
        for (auto *v : {&exact, &regex, &prefixes})
            for (auto &c : *v)
            {
                if (!c.acc.open())
                    ++restricted;
                if (!c.location.empty())
                    ++aliases;
                if (!c.headers.empty())
                    ++headers;
            }
        if (restricted)
            site.converted.push_back(std::to_string(restricted) + " access rule" + (restricted > 1 ? "s" : ""));
        if (aliases)
            site.converted.push_back(std::to_string(aliases) + " alias" + (aliases > 1 ? "es" : ""));
        if (headers)
            site.converted.push_back(std::to_string(headers) + " location" + (headers > 1 ? "s" : "") + " with headers");
        if (!realms.empty())
            site.converted.push_back("password protection");
        if (rootIdx == 1)
            site.converted.push_back("directory listing");
        for (auto &n : notes)
            add_unique(site.notes, n);
        for (auto &w : warns)
            add_unique(site.warnings, w);
        if (!bad.empty())
        {
            site.skipWhy = "unsafe text in its configuration: " + bad[0];
            return;
        }
        site.conf = conf;
    }

    void servers(const Node *scope, std::vector<const Node *> &out)
    {
        for (auto &k : scope->kids)
        {
            if (k->key == "server" && k->block)
                out.push_back(k.get());
            else if (k->key == "upstream" && k->block && !k->args.empty())
            {
                for (auto &x : k->kids)
                    if (x->key == "server" && !x->args.empty())
                    {
                        upstreams[lower(k->args[0])] = x->args[0];
                        break;
                    }
            }
            else if (k->block && (k->key == "http" || k->key == "include"))
                servers(k.get(), out);
        }
    }

    int run(const S &confArg)
    {
        httpdConf = olsRoot + "/conf/httpd_config.conf";
        S text;
        if (!read_file(httpdConf, text))
        {
            fprintf(stderr, "cannot read %s\n", httpdConf.c_str());
            return 1;
        }
        if (confArg.empty())
        {
            SV found;
            nginxConf = find_nginx(found);
            if (nginxConf.empty())
            {
                if (found.empty())
                    fprintf(stderr, "no nginx configuration found, pass the path to nginx.conf\n");
                else
                {
                    fprintf(stderr, "several nginx configurations found, pass one of them:\n");
                    for (auto &f : found)
                        fprintf(stderr, "  %s\n", shown(f).c_str());
                }
                return 2;
            }
        }
        else
            nginxConf = norm(confArg);
        if (!is_file(nginxConf))
        {
            fprintf(stderr, "%s not found\n", shown(nginxConf).c_str());
            return 2;
        }
        confDir = parent_dir(nginxConf);
        prefix = parent_dir(confDir);
        np.confDir = confDir;
        np.parse(nginxConf, &top, 0);
        if (np.files.empty())
        {
            fprintf(stderr, "cannot read %s\n", shown(nginxConf).c_str());
            return 1;
        }
        load_php_info();

        std::set<S> existingDomains, vhnames;
        SV oldVh, oldLs;
        read_state(oldVh, oldLs);
        S base = drop_objects(text, oldVh, oldLs);
        std::vector<Listener> listeners = existing_listeners(base, existingDomains, vhnames);
        int adminPort = 0;
        {
            S at;
            if (read_file(olsRoot + "/admin/conf/admin_config.conf", at))
            {
                size_t p = at.find("address");
                if (p != S::npos)
                {
                    S l = at.substr(p, at.find('\n', p) - p);
                    size_t c = l.rfind(':');
                    if (c != S::npos)
                        adminPort = atoi(l.c_str() + c + 1);
                }
            }
        }

        std::vector<const Node *> snodes;
        servers(&top, snodes);
        std::vector<Site> sites;
        SV skippedTop = np.badSites;
        for (auto *v : snodes)
        {
            if (v->unsafe || top.unsafe)
            {
                skippedTop.push_back(v->where() + "  not imported: " + (v->unsafe ? v->unsafeWhy : top.unsafeWhy));
                continue;
            }
            Member m;
            SV notes;
            member_of(v, m, notes);
            if (!m.skip.empty())
            {
                skippedTop.push_back(v->where() + "  " + m.skip);
                continue;
            }
            if (m.name.empty())
            {
                skippedTop.push_back(v->where() + "  server block without a server_name");
                continue;
            }
            if (sample_name(m.name))
            {
                skippedTop.push_back(v->where() + "  " + m.name + "  sample site");
                continue;
            }
            Site *site = nullptr;
            for (auto &s : sites)
                if (s.name == m.name)
                    site = &s;
            if (!site)
            {
                sites.push_back(Site());
                site = &sites.back();
                site->name = m.name;
            }
            else
            {
                bool clash = false;
                for (auto &o : site->members)
                    if (o.ssl == m.ssl)
                        clash = true;
                if (clash)
                {
                    skippedTop.push_back(v->where() + "  " + m.name + "  duplicate of an earlier server block, nginx uses the first");
                    continue;
                }
            }
            for (auto &n : notes)
                add_unique(site->notes, n);
            for (auto &d : v->dropped)
                add_unique(site->notes, d);
            if (m.ssl && (m.cert.empty() || !is_file(m.cert) || (!m.key.empty() && !is_file(m.key))))
            {
                site->warnings.push_back("HTTPS not imported: certificate file "
                                         + (m.cert.empty() ? S("(none)") : (is_file(m.cert) ? m.key : m.cert))
                                         + " not found");
                continue;
            }
            site->members.push_back(m);
        }
        size_t nsites = sites.size();
        for (size_t i = 0; i < nsites; ++i)
        {
            if (sites[i].members.size() != 2)
                continue;
            Member &a = sites[i].members[0], &b = sites[i].members[1];
            const Member &http = a.ssl ? b : a, &https = a.ssl ? a : b;
            if (redirect_only(http.n) || canonical(http.n) == canonical(https.n))
                continue;
            Site split;
            split.name = sites[i].name;
            split.httpPart = true;
            split.members.push_back(http);
            Member keep = https;
            sites[i].members.clear();
            sites[i].members.push_back(keep);
            sites.push_back(split);
        }
        return emit(sites, skippedTop, text, base, listeners, existingDomains, vhnames, oldVh, adminPort,
                    [&](Site &s) { build(s); }, [&]() {
                        printf("nginx configuration: %s (%d file%s)\n", shown(nginxConf).c_str(), (int)np.files.size(),
                               np.files.size() == 1 ? "" : "s");
                        for (auto &m : np.missing)
                            printf("  missing: %s\n", shown(m).c_str());
                        for (auto &d : top.dropped)
                            printf("  note: %s\n", shown(d).c_str());
                        if (top.unsafe)
                            printf("  not imported: %s\n", shown(top.unsafeWhy).c_str());
                    });
    }
};

}

extern "C" int ols_import_nginx(int argc, char **argv)
{
    NginxImporter imp;
    S conf;
    bool removeMode = false;
    wchar_t root[MAX_PATH];
    SetConsoleOutputCP(CP_UTF8);
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "--dry-run") == 0)
            imp.dryRun = true;
        else if (strcmp(argv[i], "--remove") == 0)
            removeMode = true;
        else if (argv[i][0] == '-' && argv[i][1] == '-')
        {
            fprintf(stderr, "unknown option %s\n", shown(argv[i]).c_str());
            return 2;
        }
        else
            conf = argv[i];
    }
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    imp.olsRoot = norm(U(root));
    if (removeMode)
    {
        imp.httpdConf = imp.olsRoot + "/conf/httpd_config.conf";
        return imp.remove();
    }
    return imp.run(conf);
}

extern "C" int ols_import_apache(int argc, char **argv)
{
    Importer imp;
    S conf;
    bool removeMode = false;
    wchar_t root[MAX_PATH];
    SetConsoleOutputCP(CP_UTF8);
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "--dry-run") == 0)
            imp.dryRun = true;
        else if (strcmp(argv[i], "--remove") == 0)
            removeMode = true;
        else if (argv[i][0] == '-' && argv[i][1] == '-')
        {
            fprintf(stderr, "unknown option %s\n", shown(argv[i]).c_str());
            return 2;
        }
        else
            conf = argv[i];
    }
    DWORD n = GetEnvironmentVariableW(L"OLS_ROOT", root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
    {
        fprintf(stderr, "OLS_ROOT is not set\n");
        return 2;
    }
    imp.olsRoot = norm(U(root));
    if (removeMode)
    {
        imp.httpdConf = imp.olsRoot + "/conf/httpd_config.conf";
        return imp.remove();
    }
    return imp.run(conf);
}
