#include "compat_internal.h"

typedef struct
{
    const char *s, *end;
    int depth;
} jparse_t;

static void jp_ws(jparse_t *p)
{
    while (p->s < p->end && (*p->s == ' ' || *p->s == '\t' || *p->s == '\r' || *p->s == '\n'))
        ++p->s;
}

static void jp_utf8(char **o, unsigned v)
{
    char *q = *o;
    if (v < 0x80)
        *q++ = (char)v;
    else if (v < 0x800)
    {
        *q++ = (char)(0xc0 | (v >> 6));
        *q++ = (char)(0x80 | (v & 0x3f));
    }
    else if (v < 0x10000)
    {
        *q++ = (char)(0xe0 | (v >> 12));
        *q++ = (char)(0x80 | ((v >> 6) & 0x3f));
        *q++ = (char)(0x80 | (v & 0x3f));
    }
    else
    {
        *q++ = (char)(0xf0 | (v >> 18));
        *q++ = (char)(0x80 | ((v >> 12) & 0x3f));
        *q++ = (char)(0x80 | ((v >> 6) & 0x3f));
        *q++ = (char)(0x80 | (v & 0x3f));
    }
    *o = q;
}

static int jp_hex4(const char *s, unsigned *v)
{
    int i;
    *v = 0;
    for (i = 0; i < 4; ++i)
    {
        char c = s[i];
        *v <<= 4;
        if (c >= '0' && c <= '9')
            *v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f')
            *v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            *v |= (unsigned)(c - 'A' + 10);
        else
            return -1;
    }
    return 0;
}

static char *jp_string(jparse_t *p)
{
    const char *b;
    char *out, *o;
    if (p->s >= p->end || *p->s != '"')
        return NULL;
    b = ++p->s;
    while (p->s < p->end && *p->s != '"')
    {
        if (*p->s == '\\')
            ++p->s;
        ++p->s;
    }
    if (p->s >= p->end)
        return NULL;
    if ((out = (char *)malloc((size_t)(p->s - b) + 1)) == NULL)
        return NULL;
    for (o = out; b < p->s; ++b)
    {
        unsigned v, lo;
        if ((unsigned char)*b < 0x20)
            goto bad;
        if (*b != '\\')
        {
            *o++ = *b;
            continue;
        }
        switch (*++b)
        {
        case '"': *o++ = '"'; break;
        case '\\': *o++ = '\\'; break;
        case '/': *o++ = '/'; break;
        case 'b': *o++ = '\b'; break;
        case 'f': *o++ = '\f'; break;
        case 'n': *o++ = '\n'; break;
        case 'r': *o++ = '\r'; break;
        case 't': *o++ = '\t'; break;
        case 'u':
            if (p->s - b < 5 || jp_hex4(b + 1, &v) != 0)
                goto bad;
            b += 4;
            if (v >= 0xd800 && v < 0xdc00 && p->s - b >= 7 && b[1] == '\\' && b[2] == 'u'
                && jp_hex4(b + 3, &lo) == 0 && lo >= 0xdc00 && lo < 0xe000)
            {
                v = 0x10000 + ((v - 0xd800) << 10) + (lo - 0xdc00);
                b += 6;
            }
            else if (v >= 0xd800 && v < 0xe000)
                v = 0xfffd;
            jp_utf8(&o, v);
            break;
        default:
            goto bad;
        }
    }
    *o = 0;
    ++p->s;
    return out;
bad:
    free(out);
    return NULL;
}

static ols_json *jp_value(jparse_t *p);

static ols_json *jp_list(jparse_t *p, int obj)
{
    ols_json *j = (ols_json *)calloc(1, sizeof(ols_json)), **tail;
    char close = obj ? '}' : ']';
    if (!j)
        return NULL;
    j->type = obj ? OLS_JSON_OBJ : OLS_JSON_ARR;
    tail = &j->child;
    ++p->s;
    jp_ws(p);
    if (p->s < p->end && *p->s == close)
    {
        ++p->s;
        return j;
    }
    for (;;)
    {
        char *key = NULL;
        ols_json *v;
        jp_ws(p);
        if (obj)
        {
            if ((key = jp_string(p)) == NULL)
                goto bad;
            jp_ws(p);
            if (p->s >= p->end || *p->s != ':')
            {
                free(key);
                goto bad;
            }
            ++p->s;
        }
        if ((v = jp_value(p)) == NULL)
        {
            free(key);
            goto bad;
        }
        v->key = key;
        *tail = v;
        tail = &v->next;
        jp_ws(p);
        if (p->s < p->end && *p->s == ',')
        {
            ++p->s;
            continue;
        }
        if (p->s < p->end && *p->s == close)
        {
            ++p->s;
            return j;
        }
        goto bad;
    }
bad:
    ols_json_free(j);
    return NULL;
}

static ols_json *jp_value(jparse_t *p)
{
    ols_json *j;
    jp_ws(p);
    if (p->s >= p->end || ++p->depth > 64)
        return NULL;
    if (*p->s == '{' || *p->s == '[')
        j = jp_list(p, *p->s == '{');
    else if ((j = (ols_json *)calloc(1, sizeof(ols_json))) == NULL)
        return NULL;
    else if (*p->s == '"')
    {
        j->type = OLS_JSON_STR;
        if ((j->str = jp_string(p)) == NULL)
        {
            free(j);
            j = NULL;
        }
    }
    else if (p->end - p->s >= 4 && memcmp(p->s, "true", 4) == 0)
    {
        j->type = OLS_JSON_BOOL;
        j->b = 1;
        p->s += 4;
    }
    else if (p->end - p->s >= 5 && memcmp(p->s, "false", 5) == 0)
    {
        j->type = OLS_JSON_BOOL;
        p->s += 5;
    }
    else if (p->end - p->s >= 4 && memcmp(p->s, "null", 4) == 0)
    {
        j->type = OLS_JSON_NULL;
        p->s += 4;
    }
    else
    {
        char num[64], *e;
        size_t n = 0;
        while (p->s + n < p->end && n < sizeof(num) - 1 && strchr("+-0123456789.eE", p->s[n]))
            ++n;
        memcpy(num, p->s, n);
        num[n] = 0;
        j->type = OLS_JSON_NUM;
        j->num = strtod(num, &e);
        if (n == 0 || e != num + n)
        {
            free(j);
            j = NULL;
        }
        else
            p->s += n;
    }
    --p->depth;
    return j;
}

ols_json *ols_json_parse(const char *s, size_t n)
{
    jparse_t p;
    ols_json *j;
    p.s = s;
    p.end = s + n;
    p.depth = 0;
    if (n > 3 && (unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb && (unsigned char)s[2] == 0xbf)
        p.s += 3;
    if ((j = jp_value(&p)) == NULL)
        return NULL;
    jp_ws(&p);
    if (p.s != p.end)
    {
        ols_json_free(j);
        return NULL;
    }
    return j;
}

void ols_json_free(ols_json *j)
{
    while (j)
    {
        ols_json *n = j->next;
        ols_json_free(j->child);
        free(j->key);
        free(j->str);
        free(j);
        j = n;
    }
}

ols_json *ols_json_get(const ols_json *o, const char *key)
{
    ols_json *c;
    if (!o || o->type != OLS_JSON_OBJ)
        return NULL;
    for (c = o->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0)
            return c;
    return NULL;
}

const char *ols_json_str(const ols_json *o, const char *key)
{
    ols_json *c = ols_json_get(o, key);
    return c && c->type == OLS_JSON_STR ? c->str : NULL;
}
