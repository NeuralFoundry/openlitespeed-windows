#ifndef OLS_SYS_EPOLL_H
#define OLS_SYS_EPOLL_H

#include "ols_win.h"

#define epoll_create wep_epoll_create
#define epoll_create1 wep_epoll_create1
#define epoll_close wep_epoll_close
#define epoll_ctl wep_epoll_ctl
#define epoll_wait wep_epoll_wait
#include "../../wepoll/wepoll.h"
#undef epoll_create
#undef epoll_create1
#undef epoll_close
#undef epoll_ctl
#undef epoll_wait

#ifdef __cplusplus
extern "C" {
#endif

#define EPOLL_CLOEXEC O_CLOEXEC
#define EPOLLEXCLUSIVE (1U << 28)
#define EPOLLWAKEUP (1U << 29)

int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev);
int epoll_wait(int epfd, struct epoll_event *evs, int max, int timeout);
int epoll_pwait(int epfd, struct epoll_event *evs, int max, int timeout, const sigset_t *mask);

#ifdef __cplusplus
}
#endif

#endif
