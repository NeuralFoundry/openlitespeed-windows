#include "htaccessacl.h"

#include <http/htauth.h>
#include <http/httpcontext.h>
#include <http/userdir.h>
#include <util/pcregex.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{

typedef std::string S;

enum { I_GRANT, I_DENY, I_IP, I_AUTH, I_UNKNOWN, I_ALL, I_ANY, I_NONE };

struct Net
{
    int v6;
    unsigned char a[16];
    int bits;
};

struct Item
{
    int type;
    int neg;
    std::vector<Net> nets;
    S auth;
    std::vector<Item> kids;
    Item() : type(I_UNKNOWN), neg(0) {}
};

struct Compat
{
    int present, order, allowAll, denyAll;
    std::vector<Net> allow, deny;
    Compat() : present(0), order(0), allowAll(0), denyAll(0) {}
};

struct Sec
{
    int hasReq, satisfy, authMerge, forceDeny, noScript, indexes;
    Item req;
    Compat compat;
    S authType, authName, userFile, groupFile, provider;
    int hasType, hasName, hasUser, hasGroup, hasProvider;
    Sec()
        : hasReq(0), satisfy(-1), authMerge(0), forceDeny(0), noScript(0), indexes(-1), hasType(0), hasName(0),
          hasUser(0), hasGroup(0), hasProvider(0)
    {
        req.type = I_ANY;
    }
};

struct FilesRule
{
    S pat;
    int regex;
    std::shared_ptr<Pcregex> re;
    Sec sec;
};

struct Node
{
    S key;
    std::vector<S> args;
    std::vector<Node> kids;
    int block;
    Node() : block(0) {}
};

S lower(S s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

bool ieq(const S &a, const char *b) { return strcasecmp(a.c_str(), b) == 0; }

S trim(const S &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == S::npos)
        return S();
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

std::vector<S> split_args(const S &s)
{
    std::vector<S> out;
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

int module_present(S a)
{
    static const char *mods[] = {"core", "so", "http", "authz_core", "authz_host", "authz_user", "authz_groupfile",
                                 "authn_core", "authn_file", "auth_basic", "access_compat", "rewrite", "headers",
                                 "mime", "dir", "alias", "setenvif", "expires", "deflate", "filter", "env",
                                 "version", "log_config", "autoindex", "negotiation", "include", "actions", "ssl",
                                 "socache_shmcb", "litespeed", NULL};
    a = lower(trim(a));
    if (a.size() > 7 && a.compare(a.size() - 7, 7, "_module") == 0)
        a.erase(a.size() - 7);
    if (a.size() > 2 && a.compare(a.size() - 2, 2, ".c") == 0)
        a.erase(a.size() - 2);
    if (a.compare(0, 4, "mod_") == 0)
        a.erase(0, 4);
    if (a.compare(0, 3, "php") == 0)
        return 1;
    for (int i = 0; mods[i]; ++i)
        if (a == mods[i])
            return 1;
    return 0;
}

void parse_tree(const char *text, int len, Node &root)
{
    std::vector<Node *> stack(1, &root);
    std::vector<int> active(1, 1);
    S all(text, len), buf;
    size_t i = 0;
    while (i <= all.size())
    {
        size_t j = all.find('\n', i);
        if (j == S::npos)
            j = all.size();
        S raw = all.substr(i, j - i);
        i = j + 1;
        while (!raw.empty() && (raw.back() == '\r' || raw.back() == ' ' || raw.back() == '\t'))
            raw.pop_back();
        if (!raw.empty() && raw.back() == '\\')
        {
            buf += raw.substr(0, raw.size() - 1) + " ";
            continue;
        }
        S line = trim(buf + raw);
        buf.clear();
        if (line.empty() || line[0] == '#')
            continue;
        if (line.compare(0, 2, "</") == 0)
        {
            if (stack.size() > 1)
            {
                stack.pop_back();
                active.pop_back();
            }
            continue;
        }
        if (line[0] == '<' && line.back() == '>')
        {
            S inner = trim(line.substr(1, line.size() - 2));
            size_t sp = inner.find_first_of(" \t");
            S key = lower(inner.substr(0, sp));
            std::vector<S> args = sp == S::npos ? std::vector<S>() : split_args(inner.substr(sp + 1));
            if (!active.back())
            {
                stack.push_back(stack.back());
                active.push_back(0);
                continue;
            }
            if (key == "ifmodule" || key == "ifdefine")
            {
                S a = args.empty() ? S() : args[0];
                int neg = !a.empty() && a[0] == '!';
                if (neg)
                    a.erase(0, 1);
                int ok = key == "ifmodule" ? module_present(a) : 0;
                stack.push_back(stack.back());
                active.push_back(ok != neg);
                continue;
            }
            if (key == "ifversion" || key == "iffile" || key == "ifdirective" || key == "ifsection")
            {
                stack.push_back(stack.back());
                active.push_back(1);
                continue;
            }
            Node n;
            n.key = key;
            n.args = args;
            n.block = 1;
            stack.back()->kids.push_back(n);
            stack.push_back(&stack.back()->kids.back());
            active.push_back(1);
            continue;
        }
        if (!active.back())
            continue;
        size_t sp = line.find_first_of(" \t");
        Node n;
        n.key = lower(line.substr(0, sp));
        if (sp != S::npos)
            n.args = split_args(line.substr(sp + 1));
        stack.back()->kids.push_back(n);
    }
}

int parse_net(S t, Net &n)
{
    memset(&n, 0, sizeof(n));
    if (!t.empty() && t[0] == '[')
    {
        size_t e = t.find(']');
        if (e == S::npos)
            return 0;
        t = t.substr(1, e - 1) + t.substr(e + 1);
    }
    S mask;
    size_t sl = t.find('/');
    if (sl != S::npos)
    {
        mask = t.substr(sl + 1);
        t.erase(sl);
    }
    if (t.find(':') != S::npos)
    {
        if (inet_pton(AF_INET6, t.c_str(), n.a) != 1)
            return 0;
        n.v6 = 1;
        n.bits = mask.empty() ? 128 : atoi(mask.c_str());
        return n.bits >= 0 && n.bits <= 128;
    }
    while (!t.empty() && t.back() == '.')
        t.pop_back();
    int parts = 0;
    size_t i = 0;
    while (i < t.size() && parts < 4)
    {
        if (t[i] == '*')
            break;
        if (!isdigit((unsigned char)t[i]))
            return 0;
        unsigned v = 0;
        while (i < t.size() && isdigit((unsigned char)t[i]))
            v = v * 10 + (unsigned)(t[i++] - '0');
        if (v > 255)
            return 0;
        n.a[parts++] = (unsigned char)v;
        if (i < t.size())
        {
            if (t[i] != '.')
                return 0;
            ++i;
        }
    }
    if (parts == 0)
        return 0;
    n.bits = parts * 8;
    if (!mask.empty())
    {
        if (mask.find('.') != S::npos)
        {
            Net m;
            if (!parse_net(mask, m) || m.v6)
                return 0;
            n.bits = 0;
            while (n.bits < 32 && (m.a[n.bits / 8] & (0x80 >> (n.bits % 8))))
                ++n.bits;
        }
        else
            n.bits = atoi(mask.c_str());
    }
    return n.bits >= 0 && n.bits <= 32;
}

int net_match(const Net &n, int v6, const unsigned char *addr)
{
    if (n.v6 != v6)
        return 0;
    int bits = n.bits, i = 0;
    for (; bits >= 8; bits -= 8, ++i)
        if (n.a[i] != addr[i])
            return 0;
    if (bits)
    {
        unsigned char m = (unsigned char)(0xff << (8 - bits));
        if ((n.a[i] & m) != (addr[i] & m))
            return 0;
    }
    return 1;
}

struct Peer
{
    int ok, v6;
    unsigned char a[16];
};

int in_nets(const std::vector<Net> &v, const Peer &p)
{
    if (!p.ok)
        return 0;
    for (size_t i = 0; i < v.size(); ++i)
        if (net_match(v[i], p.v6, p.a))
            return 1;
    return 0;
}

void local_nets(std::vector<Net> &out)
{
    Net n;
    parse_net("127.0.0.0/8", n);
    out.push_back(n);
    parse_net("::1", n);
    out.push_back(n);
}

Item parse_require(const std::vector<S> &args)
{
    Item it;
    std::vector<S> a = args;
    if (!a.empty() && ieq(a[0], "not"))
    {
        it.neg = 1;
        a.erase(a.begin());
    }
    S t = a.empty() ? S() : lower(a[0]);
    if (t == "all" && a.size() > 1)
        it.type = ieq(a[1], "granted") ? I_GRANT : ieq(a[1], "denied") ? I_DENY : I_UNKNOWN;
    else if (t == "ip" && a.size() > 1)
    {
        it.type = I_IP;
        for (size_t i = 1; i < a.size(); ++i)
        {
            Net n;
            if (!parse_net(a[i], n))
            {
                it.type = I_UNKNOWN;
                break;
            }
            it.nets.push_back(n);
        }
    }
    else if (t == "local")
    {
        it.type = I_IP;
        local_nets(it.nets);
    }
    else if (t == "valid-user")
    {
        it.type = I_AUTH;
        it.auth = "valid-user";
    }
    else if ((t == "user" || t == "group") && a.size() > 1)
    {
        it.type = I_AUTH;
        it.auth = t;
        for (size_t i = 1; i < a.size(); ++i)
            it.auth += " " + a[i];
    }
    return it;
}

void collect_items(const Node &n, std::vector<Item> &out)
{
    for (size_t i = 0; i < n.kids.size(); ++i)
    {
        const Node &k = n.kids[i];
        if (k.key == "require")
            out.push_back(parse_require(k.args));
        else if (k.key == "requireall" || k.key == "requireany" || k.key == "requirenone")
        {
            Item it;
            it.type = k.key == "requireall" ? I_ALL : k.key == "requireany" ? I_ANY : I_NONE;
            collect_items(k, it.kids);
            out.push_back(it);
        }
        else if (k.key == "limit" || k.key == "limitexcept")
            collect_items(k, out);
    }
}

int disables_php(const Node &n)
{
    for (size_t i = 0; i < n.kids.size(); ++i)
    {
        const Node &k = n.kids[i];
        S a0 = k.args.empty() ? S() : lower(k.args[0]);
        S a1 = k.args.size() > 1 ? lower(k.args[1]) : S();
        if (k.key == "sethandler" && (a0 == "none" || a0 == "default-handler"))
            return 1;
        if (k.key == "removehandler" || k.key == "removetype")
            for (size_t j = 0; j < k.args.size(); ++j)
                if (lower(k.args[j]).find("php") != S::npos)
                    return 1;
        if ((k.key == "php_flag" || k.key == "php_admin_flag" || k.key == "php_value" || k.key == "php_admin_value")
            && a0 == "engine" && (a1 == "off" || a1 == "0"))
            return 1;
        if (k.block && (k.key == "files" || k.key == "filesmatch") && disables_php(k))
            return 1;
    }
    return 0;
}

int has_access(const Node &n)
{
    for (size_t i = 0; i < n.kids.size(); ++i)
    {
        const S &k = n.kids[i].key;
        if (k == "require" || k == "requireall" || k == "requireany" || k == "requirenone" || k == "order"
            || k == "allow" || k == "deny" || k == "satisfy" || k == "authtype" || k == "authuserfile")
            return 1;
        if (n.kids[i].block && has_access(n.kids[i]))
            return 1;
    }
    return 0;
}

void add_compat(const Node &k, Compat &c, int &satisfy)
{
    if (k.key == "order" && !k.args.empty())
    {
        S o;
        for (size_t i = 0; i < k.args.size(); ++i)
            o += lower(k.args[i]);
        c.order = o == "deny,allow" ? 0 : o == "allow,deny" ? 1 : 2;
        c.present = 1;
    }
    else if ((k.key == "allow" || k.key == "deny") && k.args.size() >= 2 && ieq(k.args[0], "from"))
    {
        c.present = 1;
        int al = k.key == "allow";
        for (size_t i = 1; i < k.args.size(); ++i)
        {
            Net n;
            if (ieq(k.args[i], "all"))
                (al ? c.allowAll : c.denyAll) = 1;
            else if (parse_net(k.args[i], n))
                (al ? c.allow : c.deny).push_back(n);
        }
    }
    else if (k.key == "satisfy" && !k.args.empty())
        satisfy = ieq(k.args[0], "any") ? 1 : 0;
}

void parse_sec(const Node &n, Sec &s, std::vector<FilesRule> *files)
{
    std::vector<Item> items;
    collect_items(n, items);
    if (!items.empty())
    {
        s.hasReq = 1;
        s.req.type = I_ANY;
        s.req.kids = items;
    }
    if (disables_php(n))
        s.noScript = 1;
    for (size_t i = 0; i < n.kids.size(); ++i)
    {
        const Node &k = n.kids[i];
        const std::vector<S> &a = k.args;
        if (k.key == "limit" || k.key == "limitexcept")
            for (size_t j = 0; j < k.kids.size(); ++j)
                add_compat(k.kids[j], s.compat, s.satisfy);
        add_compat(k, s.compat, s.satisfy);
        if (a.empty())
            continue;
        if (k.key == "authtype")
            s.authType = a[0], s.hasType = 1;
        else if (k.key == "authname")
            s.authName = a[0], s.hasName = 1;
        else if (k.key == "authuserfile")
            s.userFile = a[0], s.hasUser = 1;
        else if (k.key == "authgroupfile")
            s.groupFile = a[0], s.hasGroup = 1;
        else if (k.key == "authbasicprovider")
            s.provider = a[0], s.hasProvider = 1;
        else if (k.key == "authmerging")
            s.authMerge = ieq(a[0], "and") ? 1 : 0;
        else if (k.key == "options")
        {
            int absolute = 0;
            for (size_t j = 0; j < a.size(); ++j)
                if (a[j][0] != '+' && a[j][0] != '-')
                    absolute = 1;
            if (absolute)
            {
                s.indexes = 0;
                for (size_t j = 0; j < a.size(); ++j)
                    if (ieq(a[j], "indexes") || ieq(a[j], "all"))
                        s.indexes = 1;
            }
            else
                for (size_t j = 0; j < a.size(); ++j)
                {
                    if (ieq(a[j], "+indexes"))
                        s.indexes = 1;
                    else if (ieq(a[j], "-indexes"))
                        s.indexes = 0;
                }
        }
        else if ((k.key == "files" || k.key == "filesmatch") && k.block && files)
        {
            FilesRule r;
            r.regex = k.key == "filesmatch" || (a[0] == "~" && a.size() > 1);
            r.pat = a[0] == "~" && a.size() > 1 ? a[1] : a[0];
            if (r.regex)
            {
                r.re = std::make_shared<Pcregex>();
                if (r.re->compile(("(?i)" + r.pat).c_str(), 0) != 0)
                {
                    r.re.reset();
                    if (has_access(k))
                        s.forceDeny = 1;
                    continue;
                }
            }
            parse_sec(k, r.sec, NULL);
            files->push_back(r);
        }
    }
    for (size_t i = 0; i < n.kids.size(); ++i)
    {
        const Node &k = n.kids[i];
        if ((k.key == "if" || k.key == "elseif" || k.key == "else") && has_access(k))
            s.forceDeny = 1;
    }
}

int wild(const char *p, const char *s)
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
                    return 1;
                if (!*t)
                    return 0;
            }
        }
        if (!*s)
            return 0;
        if (*p == '?')
            continue;
        if (*p == '[')
        {
            const char *q = p + 1;
            int neg = *q == '!' || *q == '^', ok = 0;
            if (neg)
                ++q;
            for (; *q && *q != ']'; ++q)
            {
                if (q[1] == '-' && q[2] && q[2] != ']')
                {
                    if (tolower((unsigned char)*s) >= tolower((unsigned char)*q)
                        && tolower((unsigned char)*s) <= tolower((unsigned char)q[2]))
                        ok = 1;
                    q += 2;
                }
                else if (tolower((unsigned char)*q) == tolower((unsigned char)*s))
                    ok = 1;
            }
            if (!*q || ok == neg)
                return 0;
            p = q;
            continue;
        }
        if (*p == '\\' && p[1])
            ++p;
        if (tolower((unsigned char)*p) != tolower((unsigned char)*s))
            return 0;
    }
    return !*s;
}

int files_match(const FilesRule &r, const char *base)
{
    if (!r.regex)
        return wild(r.pat.c_str(), base);
    if (!r.re)
        return 0;
    int vec[30];
    return r.re->exec(base, (int)strlen(base), 0, 0, vec, 30) > 0;
}

void merge(Sec &d, const Sec &s)
{
    if (s.hasReq)
    {
        if (s.authMerge == 1 && d.hasReq)
        {
            Item all;
            all.type = I_ALL;
            all.kids.push_back(d.req);
            all.kids.push_back(s.req);
            Item any;
            any.type = I_ANY;
            any.kids.push_back(all);
            d.req = any;
        }
        else
            d.req = s.req;
        d.hasReq = 1;
    }
    if (s.compat.present)
        d.compat = s.compat;
    if (s.satisfy >= 0)
        d.satisfy = s.satisfy;
    if (s.hasType)
        d.authType = s.authType, d.hasType = 1;
    if (s.hasName)
        d.authName = s.authName, d.hasName = 1;
    if (s.hasUser)
        d.userFile = s.userFile, d.hasUser = 1;
    if (s.hasGroup)
        d.groupFile = s.groupFile, d.hasGroup = 1;
    if (s.hasProvider)
        d.provider = s.provider, d.hasProvider = 1;
    if (s.indexes >= 0)
        d.indexes = s.indexes;
    d.forceDeny = d.forceDeny || s.forceDeny;
    d.noScript = d.noScript || s.noScript;
}

int eval(const Item &it, const Peer &p, int authed, const S &authStr);

int eval_neg(const Item &k, const Peer &p)
{
    if (k.type == I_IP)
        return !in_nets(k.nets, p);
    if (k.type == I_GRANT)
        return 0;
    return 1;
}

int eval(const Item &it, const Peer &p, int authed, const S &authStr)
{
    switch (it.type)
    {
    case I_GRANT:
        return 1;
    case I_IP:
        return in_nets(it.nets, p);
    case I_AUTH:
        return authed && it.auth == authStr;
    case I_ALL:
        for (size_t i = 0; i < it.kids.size(); ++i)
            if (it.kids[i].neg ? !eval_neg(it.kids[i], p) : !eval(it.kids[i], p, authed, authStr))
                return 0;
        return !it.kids.empty();
    case I_ANY:
        for (size_t i = 0; i < it.kids.size(); ++i)
            if (!it.kids[i].neg && it.kids[i].type != I_NONE && eval(it.kids[i], p, authed, authStr))
                return 1;
        return 0;
    case I_NONE:
        for (size_t i = 0; i < it.kids.size(); ++i)
            if (!it.kids[i].neg && eval(it.kids[i], p, authed, authStr))
                return 0;
        return 1;
    default:
        return 0;
    }
}

void first_auth(const Item &it, S &out)
{
    if (!out.empty())
        return;
    if (it.type == I_AUTH && !it.neg)
    {
        out = it.auth;
        return;
    }
    for (size_t i = 0; i < it.kids.size(); ++i)
        first_auth(it.kids[i], out);
}

int compat_ok(const Compat &c, const Peer &p)
{
    if (!c.present)
        return 1;
    int a = c.allowAll || in_nets(c.allow, p);
    int d = c.denyAll || in_nets(c.deny, p);
    if (c.order == 0)
        return !d || a;
    return a && !d;
}

int peer_addr(const struct sockaddr *sa, Peer &p)
{
    p.ok = 0;
    p.v6 = 0;
    if (!sa)
        return 0;
    if (sa->sa_family == AF_INET)
    {
        memcpy(p.a, &((const struct sockaddr_in *)sa)->sin_addr, 4);
        p.ok = 1;
    }
    else if (sa->sa_family == AF_INET6)
    {
        const unsigned char *b = (const unsigned char *)&((const struct sockaddr_in6 *)sa)->sin6_addr;
        static const unsigned char mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        if (memcmp(b, mapped, 12) == 0)
            memcpy(p.a, b + 12, 4);
        else
        {
            memcpy(p.a, b, 16);
            p.v6 = 1;
        }
        p.ok = 1;
    }
    return p.ok;
}

int is_script(const char *base)
{
    const char *dot = strrchr(base, '.');
    if (!dot)
        return 0;
    S e = lower(dot + 1);
    if (e == "phtml" || e == "phar" || e == "pht" || e == "phps" || e == "php")
        return 1;
    return e.size() == 4 && e.compare(0, 3, "php") == 0 && isdigit((unsigned char)e[3]);
}

const HTAuth *get_auth(const S &user, const S &group, const S &name)
{
    static std::map<S, HTAuth *> cache;
    S key = user + "\n" + group + "\n" + name;
    std::map<S, HTAuth *>::iterator it = cache.find(key);
    if (it != cache.end())
        return it->second;
    PlainFileUserDir *pDir = new PlainFileUserDir();
    pDir->setName(name.c_str());
    if (pDir->setDataStore(user.c_str(), group.empty() ? NULL : group.c_str()) != 0)
    {
        delete pDir;
        return NULL;
    }
    HTAuth *pAuth = new HTAuth(name.c_str());
    pAuth->setUserDir(pDir);
    cache[key] = pAuth;
    return pAuth;
}

const AuthRequired *get_required(const S &req)
{
    static std::map<S, AuthRequired *> cache;
    std::map<S, AuthRequired *>::iterator it = cache.find(req);
    if (it != cache.end())
        return it->second;
    AuthRequired *p = new AuthRequired();
    if (p->parse(req.c_str()) != 0)
    {
        delete p;
        return NULL;
    }
    cache[req] = p;
    return p;
}

void chain_of(const HttpContext *pContext, std::vector<const HtAclData *> &chain)
{
    for (const HttpContext *c = pContext; c; c = c->getParent())
    {
        const HtAcl *a = c->getHtAcl();
        if (a && a->getData())
            chain.push_back(a->getData());
    }
    std::reverse(chain.begin(), chain.end());
}

}

struct HtAclData
{
    Sec top;
    std::vector<FilesRule> files;
};


HtAcl::HtAcl()
    : m_pData(NULL)
{
}


HtAcl::~HtAcl()
{
    delete m_pData;
}


void HtAcl::parse(const char *pText, int len)
{
    Node root;
    parse_tree(pText, len, root);
    HtAclData *d = new HtAclData();
    parse_sec(root, d->top, &d->files);
    delete m_pData;
    m_pData = d;
}


int htAclCheck(const HttpContext *pContext, const char *pRealPath,
               const struct sockaddr *pPeer, const HTAuth **pAuth,
               const AuthRequired **pRequired)
{
    std::vector<const HtAclData *> chain;
    chain_of(pContext, chain);
    if (chain.empty())
        return HTACL_ALLOW;
    Sec m;
    for (size_t i = 0; i < chain.size(); ++i)
        merge(m, chain[i]->top);
    const char *base = NULL;
    if (pRealPath && *pRealPath)
    {
        const char *s1 = strrchr(pRealPath, '/'), *s2 = strrchr(pRealPath, '\\');
        base = s1 > s2 ? s1 + 1 : s2 ? s2 + 1 : pRealPath;
    }
    if (base && *base)
        for (size_t i = 0; i < chain.size(); ++i)
            for (size_t j = 0; j < chain[i]->files.size(); ++j)
                if (files_match(chain[i]->files[j], base))
                    merge(m, chain[i]->files[j].sec);
    if (m.noScript && base && is_script(base))
        return HTACL_DENY;
    if (m.forceDeny)
        return HTACL_DENY;
    if (!m.hasReq && !m.compat.present)
        return HTACL_ALLOW;
    Peer p;
    peer_addr(pPeer, p);
    S authStr;
    first_auth(m.req, authStr);
    int compat = compat_ok(m.compat, p);
    int reqNoAuth = m.hasReq ? eval(m.req, p, 0, authStr) : 1;
    if (m.satisfy == 1 && m.hasReq)
    {
        if (compat || reqNoAuth)
            return HTACL_ALLOW;
    }
    else
    {
        if (!compat)
            return HTACL_DENY;
        if (reqNoAuth)
            return HTACL_ALLOW;
    }
    if (authStr.empty() || !eval(m.req, p, 1, authStr))
        return HTACL_DENY;
    struct stat st;
    if (!ieq(m.authType, "basic") || (!m.provider.empty() && !ieq(m.provider, "file")) || m.userFile.empty()
        || stat(m.userFile.c_str(), &st) != 0)
        return HTACL_DENY;
    *pAuth = get_auth(m.userFile, m.groupFile, m.authName.empty() ? S("Restricted") : m.authName);
    *pRequired = get_required(authStr);
    return *pAuth && *pRequired ? HTACL_AUTH : HTACL_DENY;
}


int htAclIndexes(const HttpContext *pContext)
{
    std::vector<const HtAclData *> chain;
    chain_of(pContext, chain);
    int v = -1;
    for (size_t i = 0; i < chain.size(); ++i)
        if (chain[i]->top.indexes >= 0)
            v = chain[i]->top.indexes;
    return v;
}
