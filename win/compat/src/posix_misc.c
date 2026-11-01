#include "compat_internal.h"
#include <aio.h>
#include <wordexp.h>
#include <sys/sem.h>

int aio_read(struct aiocb *cb) { errno = ENOSYS; return -1; }
int aio_write(struct aiocb *cb) { errno = ENOSYS; return -1; }
int aio_error(const struct aiocb *cb) { return cb->__error; }
ssize_t aio_return(struct aiocb *cb) { return cb->__return; }
int aio_cancel(int fd, struct aiocb *cb) { return AIO_ALLDONE; }
int aio_suspend(const struct aiocb *const list[], int n, const struct timespec *timeout) { errno = ENOSYS; return -1; }
int aio_fsync(int op, struct aiocb *cb) { errno = ENOSYS; return -1; }

int wordexp(const char *words, wordexp_t *we, int flags)
{
    const char *p = words;
    size_t n = 0, cap = 8;
    char **v = (char **)calloc(cap + 1, sizeof(char *));
    if (!v)
        return WRDE_NOSPACE;
    while (*p)
    {
        const char *s;
        char *w, *o;
        char q = 0;
        while (*p == ' ' || *p == '\t' || *p == '\n')
            ++p;
        if (!*p)
            break;
        s = p;
        w = o = (char *)malloc(strlen(s) + 1);
        if (!w)
            return WRDE_NOSPACE;
        while (*p && (q || (*p != ' ' && *p != '\t' && *p != '\n')))
        {
            if (!q && (*p == '"' || *p == '\''))
                q = *p++;
            else if (q && *p == q)
            {
                q = 0;
                ++p;
            }
            else if (*p == '\\' && p[1] && q != '\'')
            {
                *o++ = p[1];
                p += 2;
            }
            else
                *o++ = *p++;
        }
        *o = 0;
        if (n == cap)
        {
            char **nv = (char **)realloc(v, (cap * 2 + 1) * sizeof(char *));
            if (!nv)
                return WRDE_NOSPACE;
            v = nv;
            cap *= 2;
        }
        v[n++] = w;
        v[n] = NULL;
    }
    we->we_wordc = n;
    we->we_wordv = v;
    we->we_offs = 0;
    return 0;
}

void wordfree(wordexp_t *we)
{
    size_t i;
    if (!we->we_wordv)
        return;
    for (i = 0; i < we->we_wordc; ++i)
        free(we->we_wordv[i]);
    free(we->we_wordv);
    we->we_wordv = NULL;
    we->we_wordc = 0;
}

key_t ftok(const char *path, int id)
{
    struct stat st;
    if (stat(path, &st) < 0)
        return -1;
    return (key_t)((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xff) << 24));
}

int semget(key_t key, int nsems, int flags) { errno = ENOSYS; return -1; }
int semop(int id, struct sembuf *ops, size_t n) { errno = ENOSYS; return -1; }
int semtimedop(int id, struct sembuf *ops, size_t n, const struct timespec *timeout) { errno = ENOSYS; return -1; }
int semctl(int id, int num, int cmd, ...) { errno = ENOSYS; return -1; }
