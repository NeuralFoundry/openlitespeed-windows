#ifndef OLS_WIN_H
#define OLS_WIN_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOSERVICE
#define NOSERVICE
#endif
#ifndef NOMCX
#define NOMCX
#endif
#ifndef NOIME
#define NOIME
#endif
#ifndef NOCRYPT
#define NOCRYPT
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <afunix.h>
#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#include <intrin.h>

#undef DELETE
#undef IN
#undef OUT
#undef OPTIONAL
#undef interface
#undef small
#undef ERROR
#undef MOUSE_MOVED
#undef ABSOLUTE
#undef RELATIVE

#ifdef __cplusplus
#include <type_traits>
extern "C" {
#endif

#define OLS_EXTERN extern

#ifndef H3_ALPN
#define H3_ALPN "\x02h3\x05h3-29"
#endif

typedef intptr_t ssize_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int mode_t;
typedef int64_t off_t;
typedef int64_t off64_t;
typedef uint64_t ino_t;
typedef uint64_t dev_t;
typedef unsigned int nlink_t;
typedef int blksize_t;
typedef int64_t blkcnt_t;
typedef unsigned int useconds_t;
typedef long suseconds_t;
typedef uint16_t in_port_t;
typedef unsigned long in_addr_t;
typedef ADDRESS_FAMILY sa_family_t;
typedef unsigned long nfds_t;
typedef uint64_t rlim_t;
typedef int clockid_t;
typedef char *caddr_t;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
typedef int key_t;
typedef unsigned int sigset_t_lo;
typedef uint64_t sigset_t;
typedef int sig_atomic_t_compat;

#define __attribute__(x)
#define __restrict__ __restrict
#define __inline__ __inline
#define __func__ __FUNCTION__
#define __thread __declspec(thread)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#define memccpy _memccpy
#define fseeko _fseeki64
#define ftello _ftelli64
#define likely(x) (x)
#define unlikely(x) (x)
#ifndef __builtin_expect
#define __builtin_expect(e, v) (e)
#endif
#define __builtin_prefetch(...) ((void)0)
#define __builtin_unreachable() __assume(0)
#define __builtin_trap() __debugbreak()
static __forceinline int ols_clz32(uint32_t v) { unsigned long i; return _BitScanReverse(&i, v) ? 31 - (int)i : 32; }
static __forceinline int ols_ctz32(uint32_t v) { unsigned long i; return _BitScanForward(&i, v) ? (int)i : 32; }
static __forceinline int ols_clz64(uint64_t v) { unsigned long i; return _BitScanReverse64(&i, v) ? 63 - (int)i : 64; }
static __forceinline int ols_ctz64(uint64_t v) { unsigned long i; return _BitScanForward64(&i, v) ? (int)i : 64; }
#define __builtin_clz(v) ols_clz32((uint32_t)(v))
#define __builtin_ctz(v) ols_ctz32((uint32_t)(v))
#define __builtin_clzl(v) ols_clz32((uint32_t)(v))
#define __builtin_ctzl(v) ols_ctz32((uint32_t)(v))
#define __builtin_clzll(v) ols_clz64((uint64_t)(v))
#define __builtin_ctzll(v) ols_ctz64((uint64_t)(v))
#define __builtin_popcount(v) ((int)__popcnt((unsigned)(v)))
#define __builtin_popcountll(v) ((int)__popcnt64((unsigned __int64)(v)))
#define __builtin_bswap16(v) _byteswap_ushort(v)
#define __builtin_bswap32(v) _byteswap_ulong(v)
#define __builtin_bswap64(v) _byteswap_uint64(v)
#define bswap_16(v) _byteswap_ushort(v)
#define bswap_32(v) _byteswap_ulong(v)
#define bswap_64(v) _byteswap_uint64(v)
#define __BYTE_ORDER 1234
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define BYTE_ORDER __BYTE_ORDER
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#define __x86_64__ 1
#define __x86_64 1

#define PATH_MAX 4096
#define MAXPATHLEN PATH_MAX
#define NAME_MAX 255
#define IOV_MAX 1024
#define UIO_MAXIOV IOV_MAX
#define HOST_NAME_MAX 255
#define MAXHOSTNAMELEN 256
#define SSIZE_MAX INTPTR_MAX
#ifndef LLONG_MAX
#define LLONG_MAX _I64_MAX
#endif

#undef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#ifndef ESHUTDOWN
#define ESHUTDOWN 201
#endif
#define ETOOMANYREFS 202
#define EHOSTDOWN 203
#define EUSERS 204
#define EDQUOT 205
#define ESTALE 206
#define EREMOTE 207
#define ESOCKTNOSUPPORT 208
#define EPFNOSUPPORT 209
#define ENONET 210
#define ENOTBLK 211
#define ENOMEDIUM 212
#define EREMOTEIO 213
#define ECOMM 214
#define EBADFD 215
#define ENOTUNIQ 216
#define EBADE 217
#define ENOKEY 218
#define EKEYEXPIRED 219
#define EKEYREVOKED 220
#define EKEYREJECTED 221
#define ERESTART 222
#define EPROCLIM 223
#define ENOPKG 224
#define EMULTIHOP 225
#define ENOLINK_COMPAT 226

#define O_ACCMODE 0003
#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_TRUNC
#undef O_EXCL
#undef O_APPEND
#undef O_BINARY
#undef O_TEXT
#undef O_NOINHERIT
#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_NDELAY O_NONBLOCK
#define O_DSYNC 010000
#define O_DIRECT 040000
#define O_LARGEFILE 0
#define O_DIRECTORY 0200000
#define O_NOFOLLOW 0400000
#define O_NOATIME 01000000
#define O_CLOEXEC 02000000
#define O_SYNC 04010000
#define O_RSYNC O_SYNC
#define O_PATH 010000000
#define O_TMPFILE 020200000
#define O_BINARY 0
#define O_TEXT 0
#define O_NOINHERIT O_CLOEXEC

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11
#define F_DUPFD_CLOEXEC 1030
#define F_SETPIPE_SZ 1031
#define F_GETPIPE_SZ 1032
#define FD_CLOEXEC 1
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define SEEK_DATA 3
#define SEEK_HOLE 4

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_EMPTY_PATH 0x1000

struct flock
{
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

#define S_IFMT 0170000
#define S_IFSOCK 0140000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFBLK 0060000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFIFO 0010000
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#define S_IREAD S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC S_IXUSR
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

struct stat
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
#define stat64 stat
#define fstat64 fstat
#define lstat64 lstat

int stat(const char *path, struct stat *st);
int lstat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);
int fstatat(int dirfd, const char *path, struct stat *st, int flags);

struct iovec
{
    void *iov_base;
    size_t iov_len;
};

struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7

int gettimeofday(struct timeval *tv, struct timezone *tz);
int clock_gettime(clockid_t id, struct timespec *ts);
int clock_getres(clockid_t id, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);
int usleep(useconds_t usec);
unsigned int sleep(unsigned int sec);
struct tm *gmtime_r(const time_t *t, struct tm *tm);
struct tm *localtime_r(const time_t *t, struct tm *tm);
char *asctime_r(const struct tm *tm, char *buf);
char *ctime_r(const time_t *t, char *buf);
time_t timegm(struct tm *tm);
char *strptime(const char *s, const char *fmt, struct tm *tm);
void tzset(void);
#define timezone_compat _timezone

struct itimerval
{
    struct timeval it_interval;
    struct timeval it_value;
};
#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF 2
int setitimer(int which, const struct itimerval *nv, struct itimerval *ov);
unsigned int alarm(unsigned int sec);

struct tms
{
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};
clock_t times(struct tms *buf);

ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
int close(int fd);
int open(const char *path, int flags, ...);
int openat(int dirfd, const char *path, int flags, ...);
int creat(const char *path, mode_t mode);
off_t lseek(int fd, off_t off, int whence);
ssize_t pread(int fd, void *buf, size_t n, off_t off);
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off);
ssize_t readv(int fd, const struct iovec *iov, int cnt);
ssize_t writev(int fd, const struct iovec *iov, int cnt);
int dup(int fd);
int dup2(int fd, int fd2);
int dup3(int fd, int fd2, int flags);
int pipe(int fds[2]);
int pipe2(int fds[2], int flags);
int fcntl(int fd, int cmd, ...);
int ioctl(int fd, unsigned long req, ...);
int ftruncate(int fd, off_t len);
int truncate(const char *path, off_t len);
int fsync(int fd);
int fdatasync(int fd);
int flock(int fd, int op);
int isatty(int fd);
int fchmod(int fd, mode_t mode);
int fchown(int fd, uid_t uid, gid_t gid);
int fchdir(int fd);
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
int posix_fadvise(int fd, off_t off, off_t len, int advice);
int posix_fallocate(int fd, off_t off, off_t len);
#define POSIX_FADV_NORMAL 0
#define POSIX_FADV_RANDOM 1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED 3
#define POSIX_FADV_DONTNEED 4
#define POSIX_FADV_NOREUSE 5

int unlink(const char *path);
int unlinkat(int dirfd, const char *path, int flags);
int ols_rename(const char *from, const char *to);
#define rename(a, b) ols_rename(a, b)
int renameat(int fd1, const char *from, int fd2, const char *to);
int mkdir(const char *path, mode_t mode);
int mkdirat(int dirfd, const char *path, mode_t mode);
int rmdir(const char *path);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int access(const char *path, int mode);
int faccessat(int dirfd, const char *path, int mode, int flags);
int chmod(const char *path, mode_t mode);
int chown(const char *path, uid_t uid, gid_t gid);
int lchown(const char *path, uid_t uid, gid_t gid);
int fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags);
int link(const char *from, const char *to);
int symlink(const char *target, const char *path);
int symlinkat(const char *target, int dirfd, const char *path);
ssize_t readlink(const char *path, char *buf, size_t size);
ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t size);
char *realpath(const char *path, char *resolved);
mode_t umask(mode_t mask);
int mkstemp(char *tmpl);
int mkostemp(char *tmpl, int flags);
char *mkdtemp(char *tmpl);
char *mktemp(char *tmpl);
int chroot(const char *path);
int ols_unlink_utf8(const char *path);

struct utimbuf
{
    time_t actime;
    time_t modtime;
};
int utime(const char *path, const struct utimbuf *times);
int utimes(const char *path, const struct timeval tv[2]);
int futimes(int fd, const struct timeval tv[2]);
int futimens(int fd, const struct timespec ts[2]);
int utimensat(int dirfd, const char *path, const struct timespec ts[2], int flags);
#define UTIME_NOW ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

struct statvfs
{
    unsigned long f_bsize;
    unsigned long f_frsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    uint64_t f_favail;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};
int statvfs(const char *path, struct statvfs *buf);
int fstatvfs(int fd, struct statvfs *buf);

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
struct dirent
{
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[NAME_MAX * 3 + 1];
};
typedef struct ols_dir DIR;
DIR *opendir(const char *path);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *dir);
int readdir_r(DIR *dir, struct dirent *entry, struct dirent **result);
int closedir(DIR *dir);
void rewinddir(DIR *dir);
int dirfd(DIR *dir);
int scandir(const char *dir, struct dirent ***list,
            int (*filter)(const struct dirent *),
            int (*cmp)(const struct dirent **, const struct dirent **));
int alphasort(const struct dirent **a, const struct dirent **b);

pid_t getpid(void);
pid_t getppid(void);
pid_t gettid(void);
pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);
pid_t setsid(void);
pid_t getsid(pid_t pid);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int seteuid(uid_t uid);
int setgid(gid_t gid);
int setegid(gid_t gid);
int setreuid(uid_t r, uid_t e);
int setregid(gid_t r, gid_t e);
int setresuid(uid_t r, uid_t e, uid_t s);
int setresgid(gid_t r, gid_t e, gid_t s);
int setfsuid(uid_t uid);
int setfsgid(gid_t gid);
int getgroups(int size, gid_t list[]);
int setgroups(size_t size, const gid_t *list);
int initgroups(const char *user, gid_t group);
int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);
pid_t fork(void);
pid_t vfork(void);
int daemon(int nochdir, int noclose);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int execl(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
void _exit_compat(int code);
int nice(int inc);
char *getlogin(void);
long gethostid(void);
int getdtablesize(void);
int getpagesize(void);

#define _SC_ARG_MAX 0
#define _SC_CHILD_MAX 1
#define _SC_CLK_TCK 2
#define _SC_NGROUPS_MAX 3
#define _SC_OPEN_MAX 4
#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_CONF 83
#define _SC_NPROCESSORS_ONLN 84
#define _SC_PHYS_PAGES 85
#define _SC_GETPW_R_SIZE_MAX 70
#define _SC_GETGR_R_SIZE_MAX 69
#define _SC_HOST_NAME_MAX 180
long sysconf(int name);
long pathconf(const char *path, int name);
#define _PC_NAME_MAX 3
#define _PC_PATH_MAX 4

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

struct passwd
{
    char *pw_name;
    char *pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};
struct group
{
    char *gr_name;
    char *gr_passwd;
    gid_t gr_gid;
    char **gr_mem;
};
struct passwd *getpwnam(const char *name);
struct passwd *getpwuid(uid_t uid);
int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t len, struct passwd **result);
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t len, struct passwd **result);
struct group *getgrnam(const char *name);
struct group *getgrgid(gid_t gid);
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t len, struct group **result);
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t len, struct group **result);
struct passwd *getpwent(void);
void setpwent(void);
void endpwent(void);
struct group *getgrent(void);
void setgrent(void);
void endgrent(void);

#undef NSIG
#define NSIG 65
#define _NSIG NSIG
#define SIGHUP 1
#define SIGQUIT 3
#define SIGTRAP 5
#define SIGBUS 7
#define SIGKILL 9
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCLD SIGCHLD
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 24
#define SIGTTOU 25
#define SIGURG 26
#define SIGXCPU 27
#define SIGXFSZ 28
#define SIGVTALRM 29
#define SIGPROF 30
#define SIGWINCH 31
#define SIGIO 32
#define SIGPOLL SIGIO
#define SIGSYS 33
#define SIGPWR 34
#define SIGIOT SIGABRT
#define SIGRTMIN 35
#define SIGRTMAX 64

typedef uint64_t sigset_t;
typedef union sigval
{
    int sival_int;
    void *sival_ptr;
} sigval_t;
typedef struct
{
    int si_signo;
    int si_errno;
    int si_code;
    pid_t si_pid;
    uid_t si_uid;
    int si_status;
    void *si_addr;
    long si_band;
    int si_fd;
    union sigval si_value;
} siginfo_t;
struct sigaction
{
    union
    {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    } __sa_u;
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};
#define sa_handler __sa_u.sa_handler
#define sa_sigaction __sa_u.sa_sigaction
#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO 4
#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000
#define SA_INTERRUPT 0x20000000
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#define SI_USER 0
#define SI_QUEUE -1
#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define BUS_ADRERR 2
#define SEGV_MAPERR 1
typedef void (*sighandler_t)(int);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
int sigdelset(sigset_t *set, int sig);
int sigismember(const sigset_t *set, int sig);
int sigaction(int sig, const struct sigaction *act, struct sigaction *old);
int sigprocmask(int how, const sigset_t *set, sigset_t *old);
int sigsuspend(const sigset_t *mask);
int sigpending(sigset_t *set);
int siginterrupt(int sig, int flag);
int sigqueue(pid_t pid, int sig, const union sigval value);
sighandler_t ols_signal(int sig, sighandler_t handler);
#define signal(...) ols_signal(__VA_ARGS__)
int ols_raise(int sig);
#define raise(...) ols_raise(__VA_ARGS__)
int kill(pid_t pid, int sig);
int killpg(pid_t pgrp, int sig);
int ols_signal_pending(void);
void ols_signal_dispatch(void);
const char *strsignal(int sig);
void psignal(int sig, const char *s);
typedef struct
{
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;
int sigaltstack(const stack_t *ss, stack_t *old);
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);
int sigwaitinfo(const sigset_t *set, siginfo_t *info);
int sigwait(const sigset_t *set, int *sig);
int pause(void);
#define SS_DISABLE 2
#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192

#define WNOHANG 1
#define WUNTRACED 2
#define WCONTINUED 8
#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s) ((s) & 0x7f)
#define WSTOPSIG(s) WEXITSTATUS(s)
#define WIFEXITED(s) (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (((signed char)(((s) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(s) (((s) & 0xff) == 0x7f)
#define WIFCONTINUED(s) ((s) == 0xffff)
#define WCOREDUMP(s) ((s) & 0x80)
#define W_EXITCODE(ret, sig) ((ret) << 8 | (sig))
pid_t waitpid(pid_t pid, int *status, int options);
pid_t wait(int *status);
struct rusage;
pid_t wait3(int *status, int options, struct rusage *ru);
pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru);

struct rlimit
{
    rlim_t rlim_cur;
    rlim_t rlim_max;
};
#define RLIM_INFINITY (~(rlim_t)0)
#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIMIT_CPU 0
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_RSS 5
#define RLIMIT_NPROC 6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS 9
#define RLIMIT_LOCKS 10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE 13
#define RLIMIT_RTPRIO 14
#define RLIMIT_RTTIME 15
#define RLIMIT_NLIMITS 16
#define RLIMIT_VMEM RLIMIT_AS
int getrlimit(int res, struct rlimit *rl);
int setrlimit(int res, const struct rlimit *rl);
struct rusage
{
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
};
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN -1
#define RUSAGE_THREAD 1
int getrusage(int who, struct rusage *ru);
#define PRIO_PROCESS 0
#define PRIO_PGRP 1
#define PRIO_USER 2
int getpriority(int which, int who);
int setpriority(int which, int who, int prio);

struct utsname
{
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};
int uname(struct utsname *buf);

#define PR_SET_PDEATHSIG 1
#define PR_GET_DUMPABLE 3
#define PR_SET_DUMPABLE 4
#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define PR_SET_KEEPCAPS 8
#define PR_SET_NO_NEW_PRIVS 38
#define PR_SET_CHILD_SUBREAPER 36
int prctl(int option, ...);
long syscall(long number, ...);
#define SYS_gettid 186
#define __NR_gettid SYS_gettid
#define SYS_getpid 39

#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
#define RTLD_NODELETE 0x1000
#define RTLD_NOLOAD 4
#define RTLD_DEFAULT ((void *)0)
#define RTLD_NEXT ((void *)-1)
void *dlopen(const char *file, int mode);
void *dlsym(void *handle, const char *name);
int dlclose(void *handle);
char *dlerror(void);
typedef struct
{
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;
int dladdr(const void *addr, Dl_info *info);

#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_KERN (0 << 3)
#define LOG_USER (1 << 3)
#define LOG_DAEMON (3 << 3)
#define LOG_AUTH (4 << 3)
#define LOG_LOCAL0 (16 << 3)
#define LOG_PID 0x01
#define LOG_CONS 0x02
#define LOG_NDELAY 0x08
#define LOG_NOWAIT 0x10
#define LOG_PERROR 0x20
#define LOG_UPTO(p) ((1 << ((p) + 1)) - 1)
void openlog(const char *ident, int opt, int facility);
void syslog(int prio, const char *fmt, ...);
void vsyslog(int prio, const char *fmt, va_list ap);
void closelog(void);
int setlogmask(int mask);

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE 0x8000
#define MAP_LOCKED 0x2000
#define MAP_FAILED ((void *)-1)
#define MS_ASYNC 1
#define MS_INVALIDATE 2
#define MS_SYNC 4
#define MADV_NORMAL 0
#define MADV_RANDOM 1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_REMOVE 9
#define MADV_DONTFORK 10
#define MADV_DOFORK 11
#define MADV_HUGEPAGE 14
#define MADV_NOHUGEPAGE 15
#define MADV_DONTDUMP 16
#define MREMAP_MAYMOVE 1
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *addr, size_t len);
int msync(void *addr, size_t len, int flags);
int madvise(void *addr, size_t len, int advice);
int posix_madvise(void *addr, size_t len, int advice);
int mprotect(void *addr, size_t len, int prot);
int mlock(const void *addr, size_t len);
int munlock(const void *addr, size_t len);
int mlockall(int flags);
#define MCL_CURRENT 1
#define MCL_FUTURE 2
int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);

int posix_memalign(void **ptr, size_t align, size_t size);
void *memalign(size_t align, size_t size);
void *ols_aligned_alloc(size_t align, size_t size);

int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strcasestr(const char *h, const char *n);
void *memmem(const void *h, size_t hl, const void *n, size_t nl);
void *memrchr(const void *s, int c, size_t n);
void *rawmemchr(const void *s, int c);
void *mempcpy(void *dst, const void *src, size_t n);
char *strndup(const char *s, size_t n);
char *strsep(char **sp, const char *delim);
char *stpcpy(char *dst, const char *src);
char *stpncpy(char *dst, const char *src, size_t n);
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
char *strchrnul(const char *s, int c);
char *index(const char *s, int c);
char *rindex(const char *s, int c);
void bzero(void *s, size_t n);
void bcopy(const void *src, void *dst, size_t n);
int bcmp(const void *a, const void *b, size_t n);
int ffs(int i);
char *strsignal_compat(int sig);
#define strtok_r strtok_s
#define strdup _strdup
#define fileno _fileno
#define fdopen ols_fdopen
FILE *ols_fdopen(int fd, const char *mode);
#define popen _popen
#define pclose _pclose
#define putenv _putenv
#define strtoll _strtoi64
#define strtoull _strtoui64
#define snprintf_compat snprintf
#define alloca _alloca
#define getc_unlocked _getc_nolock
#define putc_unlocked _putc_nolock
#define fwrite_unlocked _fwrite_nolock
#define fflush_unlocked _fflush_nolock
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int clearenv(void);
char *ols_basename(char *path);
char *ols_dirname(char *path);
long random(void);
void srandom(unsigned int seed);
int rand_r(unsigned int *seed);
char *crypt(const char *key, const char *salt);
int getloadavg(double loadavg[], int nelem);
int dprintf(int fd, const char *fmt, ...);
int vdprintf(int fd, const char *fmt, va_list ap);

#define FNM_NOMATCH 1
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD 0x04
#define FNM_LEADING_DIR 0x08
#define FNM_CASEFOLD 0x10
#define FNM_FILE_NAME FNM_PATHNAME
int fnmatch(const char *pattern, const char *string, int flags);

OLS_EXTERN char *optarg;
OLS_EXTERN int optind, opterr, optopt;
int getopt(int argc, char *const argv[], const char *optstring);
struct option
{
    const char *name;
    int has_arg;
    int *flag;
    int val;
};
#define no_argument 0
#define required_argument 1
#define optional_argument 2
int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex);

#define environ _environ

int ols_errno_from_wsa(int wsa);
int ols_errno_from_win(DWORD err);
int ols_fd_from_handle(HANDLE h);
HANDLE ols_handle_from_fd(int fd);
int ols_fd_from_socket(SOCKET s);
SOCKET ols_socket_from_fd(int fd);
int ols_fd_is_socket(int fd);
int ols_wchar_path(const char *path, wchar_t *out, size_t outlen);
int ols_utf8_path(const wchar_t *path, char *out, size_t outlen);
void ols_compat_init(void);
pid_t ols_handoff_restart(void);
int ols_handoff_next(void);
int ols_handoff_put(int fd, int slot);
int ols_handoff_count(int in);
int ols_worker_no(void);
pid_t ols_worker_spawn(int procNo, const int *fds, int nfds, void *job, void **phProcess);
#define OLS_SPAWN_FCGI 1
pid_t ols_spawn(const char *path, char *const argv[], char *const envp[], const char *cwd,
                int fd_in, int fd_out, int fd_err, int flags);
void ols_win_main_init(int *argc, char ***argv);
void ols_set_main_thread(void);
int ols_pid_is_ours(pid_t pid);
int ols_dev_sites(const char *www, const char *tld, int with_root);
int ols_dev_hosts(int clear);
int ols_set_php(const char *dir);
int ols_import_apache(int argc, char **argv);
int ols_lscgid_main(int argc, char **argv);
int ols_win_control(const char *cmd);
int ols_win_service_run(void);
int ols_win_service_install(int install);
int ols_gen_cert(const char *keyPath, const char *certPath, const char *cn);
int ols_set_admin_password(const char *htpasswd, const char *user, const char *pass);
void ols_install_crash_handler(void);
int ols_copy_file(const char *src, const char *dst);
int ols_main_thread(void);
const char *ols_server_drive(void);
static __forceinline int ols_is_drive_path(const char *p)
{
    return p && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z'))
           && p[1] == ':' && (p[2] == '/' || p[2] == '\\');
}
int ols_strip_server_drive(char *p);
const char *ols_system_ca_file(void);
int ols_native_path(const char *p, int len, char *buf, int buflen);
int ols_mapped_path(const char *p, char *buf, int buflen);

int ols_socket(int domain, int type, int proto);
int ols_accept(int fd, struct sockaddr *addr, socklen_t *len);
int accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags);
int ols_connect(int fd, const struct sockaddr *addr, socklen_t len);
int ols_bind(int fd, const struct sockaddr *addr, socklen_t len);
int ols_listen(int fd, int backlog);
int ols_shutdown(int fd, int how);
ssize_t ols_send(int fd, const void *buf, size_t len, int flags);
ssize_t ols_recv(int fd, void *buf, size_t len, int flags);
ssize_t ols_sendto(int fd, const void *buf, size_t len, int flags,
                   const struct sockaddr *to, socklen_t tolen);
ssize_t ols_recvfrom(int fd, void *buf, size_t len, int flags,
                     struct sockaddr *from, socklen_t *fromlen);
int ols_setsockopt(int fd, int level, int name, const void *val, socklen_t len);
int ols_getsockopt(int fd, int level, int name, void *val, socklen_t *len);
int ols_getsockname(int fd, struct sockaddr *addr, socklen_t *len);
int ols_getpeername(int fd, struct sockaddr *addr, socklen_t *len);
int socketpair(int domain, int type, int proto, int fds[2]);
int ols_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv);

struct msghdr
{
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
    int msg_flags;
};
struct mmsghdr
{
    struct msghdr msg_hdr;
    unsigned int msg_len;
};
#undef CMSG_ALIGN
#undef CMSG_SPACE
#undef CMSG_LEN
#undef CMSG_DATA
#undef CMSG_FIRSTHDR
#undef CMSG_NXTHDR
#define OLS_CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_ALIGN OLS_CMSG_ALIGN
#define CMSG_SPACE(len) (OLS_CMSG_ALIGN(sizeof(struct cmsghdr)) + OLS_CMSG_ALIGN(len))
#define CMSG_LEN(len) (OLS_CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_DATA(c) ((unsigned char *)(c) + OLS_CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(m) ((m)->msg_controllen >= sizeof(struct cmsghdr) ? (struct cmsghdr *)(m)->msg_control : (struct cmsghdr *)0)
#define CMSG_NXTHDR(m, c) ols_cmsg_nxthdr(m, c)
struct cmsghdr *ols_cmsg_nxthdr(struct msghdr *m, struct cmsghdr *c);
ssize_t sendmsg(int fd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int fd, struct msghdr *msg, int flags);
int sendmmsg(int fd, struct mmsghdr *vec, unsigned int vlen, int flags);
int recvmmsg(int fd, struct mmsghdr *vec, unsigned int vlen, int flags, struct timespec *timeout);
#define SCM_RIGHTS 1
#define SCM_CREDENTIALS 2
#define SOL_IP IPPROTO_IP
#define SOL_TCP IPPROTO_TCP
#define SOL_UDP IPPROTO_UDP
#define MSG_NOSIGNAL 0
#define MSG_DONTWAIT 0x40000000
#define MSG_MORE 0
#define MSG_CMSG_CLOEXEC 0
#define MSG_EOR 0
#define MSG_TRUNC_COMPAT 0x20
#define MSG_CTRUNC_COMPAT 0x08
#define SOCK_NONBLOCK 04000
#define SOCK_CLOEXEC 02000000
#define OLS_LINUX_OPT 0x7F00
#define SO_REUSEPORT (OLS_LINUX_OPT + 1)
#define SO_ATTACH_REUSEPORT_CBPF (OLS_LINUX_OPT + 2)
#define SO_PEERCRED (OLS_LINUX_OPT + 3)
#define SO_BINDTODEVICE (OLS_LINUX_OPT + 4)
#define SO_INCOMING_CPU (OLS_LINUX_OPT + 5)
#define TCP_DEFER_ACCEPT (OLS_LINUX_OPT + 6)
#define TCP_CORK (OLS_LINUX_OPT + 7)
#define TCP_QUICKACK (OLS_LINUX_OPT + 8)
#define TCP_USER_TIMEOUT (OLS_LINUX_OPT + 9)
#define TCP_CONGESTION (OLS_LINUX_OPT + 10)
#define TCP_INFO (OLS_LINUX_OPT + 11)
#define TCP_NOTSENT_LOWAT (OLS_LINUX_OPT + 12)
#define IP_FREEBIND (OLS_LINUX_OPT + 13)
#define IP_TRANSPARENT (OLS_LINUX_OPT + 14)
#define IPV6_RECVPKTINFO (OLS_LINUX_OPT + 15)
#define SO_RCVBUFFORCE (OLS_LINUX_OPT + 16)
#define SO_SNDBUFFORCE (OLS_LINUX_OPT + 17)
#define IP_SENDSRCADDR IP_PKTINFO
#define IPV6_PMTUDISC_DONT IP_PMTUDISC_DONT
#define IPV6_PMTUDISC_DO IP_PMTUDISC_DO
#define IPV6_PMTUDISC_PROBE IP_PMTUDISC_PROBE
#define AF_LOCAL AF_UNIX
#define PF_LOCAL AF_UNIX
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#define INADDR_NONE_COMPAT INADDR_NONE

struct ucred
{
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

#undef POLLIN
#undef POLLPRI
#undef POLLOUT
#undef POLLERR
#undef POLLHUP
#undef POLLNVAL
#undef POLLRDNORM
#undef POLLRDBAND
#undef POLLWRNORM
#undef POLLWRBAND
#define POLLIN 0x001
#define POLLPRI 0x002
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
#define POLLNVAL 0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#define POLLMSG 0x400
#define POLLREMOVE 0x1000
#define POLLRDHUP 0x2000
#define pollfd ols_pollfd
struct ols_pollfd
{
    int fd;
    short events;
    short revents;
};
int ols_poll(struct ols_pollfd *fds, nfds_t n, int timeout);

#define socket(...) ols_socket(__VA_ARGS__)
#define accept(...) ols_accept(__VA_ARGS__)
#define connect(...) ols_connect(__VA_ARGS__)
#define bind(...) ols_bind(__VA_ARGS__)
#define listen(...) ols_listen(__VA_ARGS__)
#define shutdown(...) ols_shutdown(__VA_ARGS__)
#define send(...) ols_send(__VA_ARGS__)
#define recv(...) ols_recv(__VA_ARGS__)
#define sendto(...) ols_sendto(__VA_ARGS__)
#define recvfrom(...) ols_recvfrom(__VA_ARGS__)
#define setsockopt(...) ols_setsockopt(__VA_ARGS__)
#define getsockopt(...) ols_getsockopt(__VA_ARGS__)
#define getsockname(...) ols_getsockname(__VA_ARGS__)
#define getpeername(...) ols_getpeername(__VA_ARGS__)
#define select(...) ols_select(__VA_ARGS__)
#define poll(...) ols_poll(__VA_ARGS__)

static __forceinline void ols_sync_barrier(void) { __faststorefence(); }
#define __sync_synchronize() ols_sync_barrier()

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define OLS_PTE(p) std::remove_cv_t<std::remove_reference_t<decltype(*(p))>>
#else
#define OLS_PTE(p) __typeof_unqual__(*(p))
#endif

static __forceinline int64_t ols_atomic_cas_n(volatile void *p, size_t sz, int64_t o, int64_t n)
{
    switch (sz)
    {
    case 1: return (int64_t)_InterlockedCompareExchange8((volatile char *)p, (char)n, (char)o);
    case 2: return (int64_t)_InterlockedCompareExchange16((volatile short *)p, (short)n, (short)o);
    case 4: return (int64_t)_InterlockedCompareExchange((volatile long *)p, (long)n, (long)o);
    default: return (int64_t)_InterlockedCompareExchange64((volatile __int64 *)p, (__int64)n, (__int64)o);
    }
}
static __forceinline int64_t ols_atomic_xchg_n(volatile void *p, size_t sz, int64_t v)
{
    switch (sz)
    {
    case 1: return (int64_t)_InterlockedExchange8((volatile char *)p, (char)v);
    case 2: return (int64_t)_InterlockedExchange16((volatile short *)p, (short)v);
    case 4: return (int64_t)_InterlockedExchange((volatile long *)p, (long)v);
    default: return (int64_t)_InterlockedExchange64((volatile __int64 *)p, (__int64)v);
    }
}
static __forceinline int64_t ols_atomic_xadd_n(volatile void *p, size_t sz, int64_t v)
{
    switch (sz)
    {
    case 1: return (int64_t)_InterlockedExchangeAdd8((volatile char *)p, (char)v);
    case 2: return (int64_t)_InterlockedExchangeAdd16((volatile short *)p, (short)v);
    case 4: return (int64_t)_InterlockedExchangeAdd((volatile long *)p, (long)v);
    default: return (int64_t)_InterlockedExchangeAdd64((volatile __int64 *)p, (__int64)v);
    }
}
static __forceinline int64_t ols_atomic_and_n(volatile void *p, size_t sz, int64_t v)
{
    switch (sz)
    {
    case 1: return (int64_t)_InterlockedAnd8((volatile char *)p, (char)v);
    case 2: return (int64_t)_InterlockedAnd16((volatile short *)p, (short)v);
    case 4: return (int64_t)_InterlockedAnd((volatile long *)p, (long)v);
    default: return (int64_t)_InterlockedAnd64((volatile __int64 *)p, (__int64)v);
    }
}
static __forceinline int64_t ols_atomic_or_n(volatile void *p, size_t sz, int64_t v)
{
    switch (sz)
    {
    case 1: return (int64_t)_InterlockedOr8((volatile char *)p, (char)v);
    case 2: return (int64_t)_InterlockedOr16((volatile short *)p, (short)v);
    case 4: return (int64_t)_InterlockedOr((volatile long *)p, (long)v);
    default: return (int64_t)_InterlockedOr64((volatile __int64 *)p, (__int64)v);
    }
}
static __forceinline int64_t ols_atomic_xor_n(volatile void *p, size_t sz, int64_t v)
{
    switch (sz)
    {
    case 1: return (int64_t)_InterlockedXor8((volatile char *)p, (char)v);
    case 2: return (int64_t)_InterlockedXor16((volatile short *)p, (short)v);
    case 4: return (int64_t)_InterlockedXor((volatile long *)p, (long)v);
    default: return (int64_t)_InterlockedXor64((volatile __int64 *)p, (__int64)v);
    }
}
static __forceinline int64_t ols_atomic_load_n(const volatile void *p, size_t sz)
{
    int64_t v;
    switch (sz)
    {
    case 1: v = *(const volatile int8_t *)p; break;
    case 2: v = *(const volatile int16_t *)p; break;
    case 4: v = *(const volatile int32_t *)p; break;
    default: v = *(const volatile int64_t *)p; break;
    }
    _ReadWriteBarrier();
    return v;
}
static __forceinline void ols_atomic_store_n(volatile void *p, size_t sz, int64_t v)
{
    _ReadWriteBarrier();
    switch (sz)
    {
    case 1: *(volatile int8_t *)p = (int8_t)v; break;
    case 2: *(volatile int16_t *)p = (int16_t)v; break;
    case 4: *(volatile int32_t *)p = (int32_t)v; break;
    default: *(volatile int64_t *)p = (int64_t)v; break;
    }
}
static __forceinline int64_t ols_atomic_nand_n(volatile void *p, size_t sz, int64_t v)
{
    int64_t o, n;
    do
    {
        o = ols_atomic_load_n(p, sz);
        n = ~(o & v);
    }
    while (ols_atomic_cas_n(p, sz, o, n) != o);
    return o;
}
static __forceinline int ols_atomic_cmpxchg16(volatile void *p, void *expected, const void *desired)
{
    const int64_t *d = (const int64_t *)desired;
    return _InterlockedCompareExchange128((volatile __int64 *)p, d[1], d[0], (__int64 *)expected);
}

static __forceinline int ols_atomic_cas_b(volatile void *p, size_t sz, int64_t o, int64_t n)
{
    switch (sz)
    {
    case 1: return _InterlockedCompareExchange8((volatile char *)p, (char)n, (char)o) == (char)o;
    case 2: return _InterlockedCompareExchange16((volatile short *)p, (short)n, (short)o) == (short)o;
    case 4: return _InterlockedCompareExchange((volatile long *)p, (long)n, (long)o) == (long)o;
    default: return _InterlockedCompareExchange64((volatile __int64 *)p, (__int64)n, (__int64)o) == (__int64)o;
    }
}

#define OLS_I64(v) ((int64_t)(intptr_t)(v))
#define OLS_AT(p, v) ((OLS_PTE(p))(intptr_t)(v))
#define __sync_val_compare_and_swap(p, o, n) OLS_AT(p, ols_atomic_cas_n((p), sizeof(*(p)), OLS_I64(o), OLS_I64(n)))
#define __sync_bool_compare_and_swap(p, o, n) ols_atomic_cas_b((p), sizeof(*(p)), OLS_I64(o), OLS_I64(n))
#define __sync_lock_test_and_set(p, v) OLS_AT(p, ols_atomic_xchg_n((p), sizeof(*(p)), OLS_I64(v)))
#define __sync_lock_release(p) ols_atomic_store_n((p), sizeof(*(p)), 0)
#define __sync_fetch_and_add(p, v) OLS_AT(p, ols_atomic_xadd_n((p), sizeof(*(p)), OLS_I64(v)))
#define __sync_fetch_and_sub(p, v) OLS_AT(p, ols_atomic_xadd_n((p), sizeof(*(p)), -OLS_I64(v)))
#define __sync_add_and_fetch(p, v) OLS_AT(p, ols_atomic_xadd_n((p), sizeof(*(p)), OLS_I64(v)) + OLS_I64(v))
#define __sync_sub_and_fetch(p, v) OLS_AT(p, ols_atomic_xadd_n((p), sizeof(*(p)), -OLS_I64(v)) - OLS_I64(v))
#define __sync_fetch_and_and(p, v) OLS_AT(p, ols_atomic_and_n((p), sizeof(*(p)), OLS_I64(v)))
#define __sync_fetch_and_or(p, v) OLS_AT(p, ols_atomic_or_n((p), sizeof(*(p)), OLS_I64(v)))
#define __sync_fetch_and_xor(p, v) OLS_AT(p, ols_atomic_xor_n((p), sizeof(*(p)), OLS_I64(v)))
#define __sync_fetch_and_nand(p, v) OLS_AT(p, ols_atomic_nand_n((p), sizeof(*(p)), OLS_I64(v)))
#define __sync_and_and_fetch(p, v) OLS_AT(p, ols_atomic_and_n((p), sizeof(*(p)), OLS_I64(v)) & OLS_I64(v))
#define __sync_or_and_fetch(p, v) OLS_AT(p, ols_atomic_or_n((p), sizeof(*(p)), OLS_I64(v)) | OLS_I64(v))
#define __sync_xor_and_fetch(p, v) OLS_AT(p, ols_atomic_xor_n((p), sizeof(*(p)), OLS_I64(v)) ^ OLS_I64(v))
#define __sync_nand_and_fetch(p, v) OLS_AT(p, ~(ols_atomic_nand_n((p), sizeof(*(p)), OLS_I64(v)) & OLS_I64(v)))
#define __ATOMIC_RELAXED 0
#define __ATOMIC_CONSUME 1
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#define __ATOMIC_SEQ_CST 5
#define __atomic_load_n(p, m) OLS_AT(p, ols_atomic_load_n((p), sizeof(*(p))))
#define __atomic_store_n(p, v, m) ols_atomic_store_n((p), sizeof(*(p)), OLS_I64(v))
#define __atomic_exchange_n(p, v, m) __sync_lock_test_and_set(p, v)
#define __atomic_fetch_add(p, v, m) __sync_fetch_and_add(p, v)
#define __atomic_fetch_sub(p, v, m) __sync_fetch_and_sub(p, v)
#define __atomic_add_fetch(p, v, m) __sync_add_and_fetch(p, v)
#define __atomic_sub_fetch(p, v, m) __sync_sub_and_fetch(p, v)
#define __atomic_thread_fence(m) ols_sync_barrier()
static __forceinline int ols_atomic_cmpxchg(volatile void *p, void *expected, const void *desired, size_t sz)
{
    int64_t e, d, o;
    if (sz == 16)
        return ols_atomic_cmpxchg16(p, expected, desired);
    e = 0;
    d = 0;
    memcpy(&e, expected, sz);
    memcpy(&d, desired, sz);
    o = ols_atomic_cas_n(p, sz, e, d);
    if (memcmp(&o, &e, sz) == 0)
        return 1;
    memcpy(expected, &o, sz);
    return 0;
}
#define __atomic_compare_exchange(p, e, d, w, s, f) ols_atomic_cmpxchg((p), (e), (d), sizeof(*(p)))

#endif
