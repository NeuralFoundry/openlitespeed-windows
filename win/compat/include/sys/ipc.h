#ifndef OLS_SYS_IPC_H
#define OLS_SYS_IPC_H
#include "ols_win.h"
#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_NOWAIT 04000
#define IPC_PRIVATE ((key_t)0)
#define IPC_RMID 0
#define IPC_SET 1
#define IPC_STAT 2
#ifdef __cplusplus
extern "C" {
#endif
key_t ftok(const char *path, int id);
#ifdef __cplusplus
}
#endif
#endif
