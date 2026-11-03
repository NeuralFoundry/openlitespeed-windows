#ifndef OLS_COMPAT_INTERNAL_H
#define OLS_COMPAT_INTERNAL_H

#include "ols_win.h"

#undef socket
#undef accept
#undef connect
#undef bind
#undef listen
#undef shutdown
#undef send
#undef recv
#undef sendto
#undef recvfrom
#undef setsockopt
#undef getsockopt
#undef getsockname
#undef getpeername
#undef select
#undef poll
#undef signal
#undef raise
#undef rename
#undef pollfd

#ifndef DELETE
#define DELETE (0x00010000L)
#endif
#ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#endif
LONG WINAPI BCryptGenRandom(void *alg, PUCHAR buf, ULONG len, ULONG flags);
#ifndef FSCTL_GET_REPARSE_POINT
#define FSCTL_GET_REPARSE_POINT 0x000900a8
#endif
#ifndef MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE (16 * 1024)
#endif

#define OLS_T_UNKNOWN 0
#define OLS_T_FILE 1
#define OLS_T_SOCKET 2
#define OLS_T_EPOLL 3
#define OLS_T_CHAR 4
#define OLS_T_PIPE 5
#define OLS_T_RANDOM 6
#define OLS_T_NULL 7
#define OLS_T_MASK 0x0f
#define OLS_F_NONBLOCK 0x0010
#define OLS_F_APPEND 0x0020
#define OLS_F_CLOEXEC 0x0040
#define OLS_F_DIR 0x0080
#define OLS_F_ACC_SHIFT 8
#define OLS_F_ACC_MASK 0x0300
#define OLS_F_KNOWN 0x8000

#define WSA_POLLERR 0x0001
#define WSA_POLLHUP 0x0002
#define WSA_POLLNVAL 0x0004
#define WSA_POLLWRNORM 0x0010
#define WSA_POLLWRBAND 0x0020
#define WSA_POLLRDNORM 0x0100
#define WSA_POLLRDBAND 0x0200
#define WSA_POLLPRI 0x0400

uint16_t ols_fd_meta(int fd);
void ols_fd_set_meta(int fd, uint16_t meta);
void ols_fd_clear(int fd);
int ols_fd_type(int fd);
void ols_wsa_init(void);
int ols_set_errno_wsa(void);
int ols_set_errno_win(void);
HANDLE ols_open_path(const char *path, DWORD access, DWORD share, DWORD disp, DWORD flags, int *special);
HANDLE ols_open_path_sa(const char *path, DWORD access, DWORD share, DWORD disp, DWORD flags,
                        int *special, SECURITY_ATTRIBUTES *sa);
int ols_path_to_w(const char *path, wchar_t *out, size_t outlen, int *special);
int ols_w_to_path(const wchar_t *w, char *out, size_t outlen);
int ols_fill_stat(HANDLE h, struct stat *st, int lnk);
void ols_signal_wake(void);
int ols_signal_wake_fd(void);
void ols_signal_drain(void);
int ols_main_thread(void);
const wchar_t *ols_user_sid(void);
PSECURITY_DESCRIPTOR ols_private_sd(int inherit);
int ols_secure_handle(HANDLE h, int inherit);
int ols_secure_wpath(const wchar_t *path, int inherit);
int ols_is_private_handle(HANDLE h);
int ols_store_has_root(const unsigned char *der, size_t len);
int ols_store_add_root(const unsigned char *der, size_t len, const wchar_t *friendly);
int ols_store_remove_root(const unsigned char *der, size_t len);
int ols_read_wfile(const wchar_t *path, char **out, size_t *len);
int ols_write_wfile(const wchar_t *path, const void *data, size_t len, int priv);
int ols_private_dir(const wchar_t *dir);
int ols_port_listening(int port);
int ols_dev_resync(const wchar_t *root, const wchar_t *ini);
int ols_default_port(const wchar_t *root);
int ols_db_wordpress(const wchar_t *root, const char *site, const wchar_t *dir);
void ols_reserve_low_handles(void);
int ols_accept_lock(SOCKET s);
void ols_accept_unlock(void);

#define OLS_SPECIAL_NONE 0
#define OLS_SPECIAL_NULL 1
#define OLS_SPECIAL_RANDOM 2

#endif
