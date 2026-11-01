#ifndef OLS_SYS_SEM_H
#define OLS_SYS_SEM_H
#include "sys/ipc.h"
#ifdef __cplusplus
extern "C" {
#endif
struct sembuf
{
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};
#define SEM_UNDO 0x1000
#define GETVAL 12
#define SETVAL 16
#define GETALL 13
#define SETALL 17
int semget(key_t key, int nsems, int flags);
int semop(int id, struct sembuf *ops, size_t n);
int semtimedop(int id, struct sembuf *ops, size_t n, const struct timespec *timeout);
int semctl(int id, int num, int cmd, ...);
#ifdef __cplusplus
}
#endif
#endif
