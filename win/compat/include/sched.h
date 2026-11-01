#ifndef OLS_SCHED_H
#define OLS_SCHED_H

#include "ols_win.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sched_param
{
    int sched_priority;
};

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_BATCH 3
#define SCHED_IDLE 5

#define CPU_SETSIZE 1024
typedef struct
{
    uint64_t bits[CPU_SETSIZE / 64];
} cpu_set_t;

#define CPU_ZERO(s) memset((s), 0, sizeof(cpu_set_t))
#define CPU_SET(c, s) ((s)->bits[(c) / 64] |= (1ULL << ((c) % 64)))
#define CPU_CLR(c, s) ((s)->bits[(c) / 64] &= ~(1ULL << ((c) % 64)))
#define CPU_ISSET(c, s) (((s)->bits[(c) / 64] >> ((c) % 64)) & 1)
#define CPU_COUNT(s) ols_cpu_count(s)
#define CPU_ALLOC_SIZE(n) sizeof(cpu_set_t)
#define CPU_ALLOC(n) ((cpu_set_t *)calloc(1, sizeof(cpu_set_t)))
#define CPU_FREE(s) free(s)
#define CPU_ZERO_S(sz, s) CPU_ZERO(s)
#define CPU_SET_S(c, sz, s) CPU_SET(c, s)
#define CPU_ISSET_S(c, sz, s) CPU_ISSET(c, s)

int ols_cpu_count(const cpu_set_t *s);
int sched_yield(void);
int sched_getcpu(void);
int sched_setaffinity(pid_t pid, size_t size, const cpu_set_t *set);
int sched_getaffinity(pid_t pid, size_t size, cpu_set_t *set);
int sched_get_priority_min(int policy);
int sched_get_priority_max(int policy);
int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);
int sched_getscheduler(pid_t pid);

#ifdef __cplusplus
}
#endif

#endif
