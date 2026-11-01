#ifndef HTACCESSACL_H
#define HTACCESSACL_H

struct sockaddr;
class HTAuth;
class AuthRequired;
class HttpContext;
struct HtAclData;

class HtAcl
{
public:
    HtAcl();
    ~HtAcl();
    void parse(const char *pText, int len);
    const HtAclData *getData() const   {   return m_pData;     }

private:
    HtAcl(const HtAcl &rhs);
    void operator=(const HtAcl &rhs);
    HtAclData *m_pData;
};

enum
{
    HTACL_ALLOW = 0,
    HTACL_DENY = 403,
    HTACL_AUTH = 401
};

int htAclCheck(const HttpContext *pContext, const char *pRealPath,
               const struct sockaddr *pPeer, const HTAuth **pAuth,
               const AuthRequired **pRequired);
int htAclIndexes(const HttpContext *pContext);

#endif
