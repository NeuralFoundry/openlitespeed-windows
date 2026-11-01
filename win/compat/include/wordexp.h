#ifndef OLS_WORDEXP_H
#define OLS_WORDEXP_H
#include "ols_win.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
    size_t we_wordc;
    char **we_wordv;
    size_t we_offs;
} wordexp_t;
#define WRDE_DOOFFS 1
#define WRDE_APPEND 2
#define WRDE_NOCMD 4
#define WRDE_REUSE 8
#define WRDE_SHOWERR 16
#define WRDE_UNDEF 32
#define WRDE_BADCHAR 1
#define WRDE_BADVAL 2
#define WRDE_CMDSUB 3
#define WRDE_NOSPACE 4
#define WRDE_SYNTAX 5
int wordexp(const char *words, wordexp_t *we, int flags);
void wordfree(wordexp_t *we);
#ifdef __cplusplus
}
#endif
#endif
