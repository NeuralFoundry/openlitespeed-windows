#ifndef OLS_AIO_H
#define OLS_AIO_H
#include "ols_win.h"
#ifdef __cplusplus
extern "C" {
#endif
struct sigevent
{
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
    void *sigev_notify_attributes;
};
#define SIGEV_NONE 1
#define SIGEV_SIGNAL 0
#define SIGEV_THREAD 2
struct aiocb
{
    int aio_fildes;
    int aio_lio_opcode;
    int aio_reqprio;
    volatile void *aio_buf;
    size_t aio_nbytes;
    struct sigevent aio_sigevent;
    off_t aio_offset;
    int __error;
    ssize_t __return;
};
#define AIO_CANCELED 0
#define AIO_NOTCANCELED 1
#define AIO_ALLDONE 2
#define LIO_READ 0
#define LIO_WRITE 1
#define LIO_NOP 2
#define LIO_WAIT 0
#define LIO_NOWAIT 1
int aio_read(struct aiocb *cb);
int aio_write(struct aiocb *cb);
int aio_error(const struct aiocb *cb);
ssize_t aio_return(struct aiocb *cb);
int aio_cancel(int fd, struct aiocb *cb);
int aio_suspend(const struct aiocb *const list[], int n, const struct timespec *timeout);
int aio_fsync(int op, struct aiocb *cb);
#ifdef __cplusplus
}
#endif
#endif
