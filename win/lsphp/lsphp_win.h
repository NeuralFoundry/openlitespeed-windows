#ifndef LSPHP_WIN_H
#define LSPHP_WIN_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <windows.h>
#include <intsafe.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;

#define LSW_SOCK_BIT 0x40000000

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct timezone;
__declspec(dllimport) int gettimeofday(struct timeval *tv, struct timezone *tz);
__declspec(dllimport) int usleep(unsigned int us);

#ifndef STDIN_FILENO
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#define F_GETFL 3
#define F_SETFL 4
#define F_SETFD 2
#define FD_CLOEXEC 1
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x4000
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGBUS
#define SIGBUS 7
#endif
#ifndef SIGCHLD
#define SIGCHLD 17
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGPROF
#define SIGPROF 27
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGHUP
#define SIGHUP 1
#endif

typedef int sigset_t;
struct sigaction
{
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};
#define SA_RESTART 0
#define SA_NODEFER 0
#define SA_SIGINFO 0
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#define sigemptyset(s) (*(s) = 0, 0)
#define sigfillset(s) (*(s) = ~0, 0)
#define sigaddset(s, n) (*(s) |= (1 << ((n) & 31)), 0)
#define sigdelset(s, n) (*(s) &= ~(1 << ((n) & 31)), 0)
#define sigismember(s, n) ((*(s) >> ((n) & 31)) & 1)

int lsw_sigaction(int sig, const struct sigaction *act, struct sigaction *old);
int lsw_sigprocmask(int how, const sigset_t *set, sigset_t *old);
#define sigaction(s, a, o) lsw_sigaction(s, a, o)
#define sigprocmask(h, s, o) lsw_sigprocmask(h, s, o)

#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) (s)
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0

struct itimerval { struct timeval it_interval, it_value; };
#define ITIMER_REAL 0
#define ITIMER_PROF 2
#define setitimer(w, n, o) 0

struct rlimit { unsigned long long rlim_cur, rlim_max; };
#define RLIMIT_CORE 4
#define RLIMIT_NOFILE 7
#define RLIM_INFINITY (~0ULL)

struct passwd { char *pw_name; uid_t pw_uid; gid_t pw_gid; char *pw_dir; char *pw_shell; };
struct group { char *gr_name; gid_t gr_gid; };

#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define MAP_ANON 0x20
#define MAP_ANONYMOUS MAP_ANON
#define MAP_FAILED ((void *)-1)

#define RTLD_LAZY 1
#define RTLD_NOW 2

#define __atomic_store_n(p, v, o) (*(p) = (v))
#define __atomic_load_n(p, o) (*(p))
#define __atomic_fetch_add(p, v, o) (*(p) += (v), *(p) - (v))
#define __atomic_fetch_sub(p, v, o) (*(p) -= (v), *(p) + (v))
#define __atomic_add_fetch(p, v, o) (*(p) += (v))
#define __atomic_compare_exchange_n(p, e, d, w, s, f) \
    ((*(p) == *(e)) ? (*(p) = (d), 1) : (*(e) = *(p), 0))
#define __ATOMIC_SEQ_CST 5

#ifndef __attribute__
#define __attribute__(x)
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define localtime_r(t, r) (localtime_s((r), &(time_t){ (time_t)*(t) }) ? NULL : (r))
#define setgroups(n, g) lsw_setgroups((int)(n), (const void *)(g))
#define wait(s) lsw_waitpid(-1, (s), 0)
int lsw_setgroups(int n, const void *g);
void lsw_set_stdin_socket(int fd);
__declspec(noreturn) void lsw_exit_now(int code);
#ifndef environ
#define environ _environ
#endif

pid_t lsw_fork(void);
pid_t lsw_getppid(void);
pid_t lsw_setsid(void);
pid_t lsw_getpgrp(void);
int lsw_setpgid(pid_t pid, pid_t pg);
int lsw_kill(pid_t pid, int sig);
pid_t lsw_waitpid(pid_t pid, int *status, int options);
unsigned int lsw_alarm(unsigned int s);
int lsw_sched_yield(void);
uid_t lsw_getuid(void);
int lsw_setuid(uid_t u);
int lsw_setgid(gid_t g);
int lsw_initgroups(const char *u, gid_t g);
int lsw_chroot(const char *p);
struct passwd *lsw_getpwnam(const char *n);
struct passwd *lsw_getpwuid(uid_t u);
struct group *lsw_getgrnam(const char *n);
int lsw_setrlimit(int r, const struct rlimit *l);
int lsw_getrlimit(int r, struct rlimit *l);
void *lsw_mmap(void *a, size_t len, int prot, int flags, int fd, long long off);
int lsw_munmap(void *a, size_t len);
void *lsw_dlopen(const char *f, int m);
void *lsw_dlsym(void *h, const char *s);

#define fork lsw_fork
#define getppid lsw_getppid
#define setsid lsw_setsid
#define getpgrp lsw_getpgrp
#define setpgid lsw_setpgid
#define kill lsw_kill
#define waitpid lsw_waitpid
#define alarm lsw_alarm
#define sched_yield lsw_sched_yield
#define getuid lsw_getuid
#define geteuid lsw_getuid
#define getgid lsw_getuid
#define getegid lsw_getuid
#define setuid lsw_setuid
#define seteuid lsw_setuid
#define setgid lsw_setgid
#define initgroups lsw_initgroups
#define chroot lsw_chroot
#define getpwnam lsw_getpwnam
#define getpwuid lsw_getpwuid
#define getgrnam lsw_getgrnam
#define setrlimit lsw_setrlimit
#define getrlimit lsw_getrlimit
#define mmap lsw_mmap
#define munmap lsw_munmap
#define dlopen lsw_dlopen
#define dlsym lsw_dlsym

#ifdef LSPHP_IO_SHIM
struct iovec { void *iov_base; size_t iov_len; };

ssize_t lsw_read(int fd, void *buf, size_t len);
ssize_t lsw_write(int fd, const void *buf, size_t len);
ssize_t lsw_writev(int fd, const struct iovec *iov, int n);
int lsw_close(int fd);
int lsw_dup(int fd);
int lsw_dup2(int fd, int fd2);
int lsw_fcntl(int fd, int cmd, ...);
int lsw_accept(int fd, struct sockaddr *a, int *len);
int lsw_socket(int d, int t, int p);
int lsw_bind(int fd, const struct sockaddr *a, int len);
int lsw_listen(int fd, int n);
int lsw_connect(int fd, const struct sockaddr *a, int len);
int lsw_setsockopt(int fd, int l, int n, const void *v, int len);
int lsw_getsockopt(int fd, int l, int n, void *v, int *len);
int lsw_getpeername(int fd, struct sockaddr *a, int *len);
int lsw_getsockname(int fd, struct sockaddr *a, int *len);
int lsw_shutdown(int fd, int how);
int lsw_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t);
ssize_t lsw_sendfile(int out, int in, long long *off, size_t size);
ssize_t gsendfile(int out, int in, off_t *off, size_t size);
int lsw_open(const char *path, int flags, ...);
void lsw_fd_set(int fd, fd_set *s);
int lsw_fd_isset(int fd, fd_set *s);

#define read lsw_read
#define write lsw_write
#define writev lsw_writev
#define close lsw_close
#define dup lsw_dup
#define dup2 lsw_dup2
#define fcntl lsw_fcntl
#define accept lsw_accept
#define socket lsw_socket
#define bind lsw_bind
#define listen lsw_listen
#define connect lsw_connect
#define setsockopt lsw_setsockopt
#define getsockopt lsw_getsockopt
#define getpeername lsw_getpeername
#define getsockname lsw_getsockname
#define shutdown lsw_shutdown
#define select lsw_select
#define sendfile lsw_sendfile
#define open lsw_open
#undef FD_SET
#define FD_SET(fd, s) lsw_fd_set(fd, s)
#undef FD_ISSET
#define FD_ISSET(fd, s) lsw_fd_isset(fd, s)
#include <direct.h>
#define getcwd _getcwd
#define realpath(p, r) _fullpath((r), (p), PATH_MAX)
#define unlink _unlink
#define getpid _getpid
#endif

#endif
