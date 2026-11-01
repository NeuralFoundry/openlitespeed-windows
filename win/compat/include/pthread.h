#ifndef OLS_PTHREAD_H
#define OLS_PTHREAD_H

#include "ols_win.h"
#include "sched.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t pthread_t;

typedef struct
{
    int detach;
    size_t stacksize;
    void *stackaddr;
    size_t guardsize;
    int scope;
    int policy;
    int inherit;
    struct sched_param param;
    cpu_set_t cpus;
    int has_cpus;
} pthread_attr_t;

typedef struct
{
    SRWLOCK lock;
    int type;
    DWORD owner;
    int count;
} pthread_mutex_t;

typedef struct
{
    int type;
    int pshared;
} pthread_mutexattr_t;

typedef struct
{
    CONDITION_VARIABLE cv;
} pthread_cond_t;

typedef struct
{
    int pshared;
    clockid_t clock;
} pthread_condattr_t;

typedef struct
{
    SRWLOCK lock;
} pthread_rwlock_t;

typedef struct
{
    int pshared;
} pthread_rwlockattr_t;

typedef volatile long pthread_spinlock_t;
typedef DWORD pthread_key_t;
typedef INIT_ONCE pthread_once_t;

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_ADAPTIVE_NP PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_FAST_NP PTHREAD_MUTEX_NORMAL
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1
#define PTHREAD_INHERIT_SCHED 0
#define PTHREAD_EXPLICIT_SCHED 1
#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED ((void *)-1)
#define PTHREAD_STACK_MIN 16384
#define PTHREAD_MUTEX_INITIALIZER { SRWLOCK_INIT, 0, 0, 0 }
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP { SRWLOCK_INIT, PTHREAD_MUTEX_RECURSIVE, 0, 0 }
#define PTHREAD_COND_INITIALIZER { CONDITION_VARIABLE_INIT }
#define PTHREAD_RWLOCK_INITIALIZER { SRWLOCK_INIT }
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

int pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg);
int pthread_join(pthread_t t, void **ret);
int pthread_tryjoin_np(pthread_t t, void **ret);
int pthread_timedjoin_np(pthread_t t, void **ret, const struct timespec *abstime);
int pthread_detach(pthread_t t);
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
void pthread_exit(void *ret);
int pthread_cancel(pthread_t t);
int pthread_kill(pthread_t t, int sig);
int pthread_setcancelstate(int state, int *old);
int pthread_setcanceltype(int type, int *old);
void pthread_testcancel(void);
int pthread_setname_np(pthread_t t, const char *name);
int pthread_getname_np(pthread_t t, char *name, size_t len);
int pthread_yield(void);
int pthread_setaffinity_np(pthread_t t, size_t size, const cpu_set_t *set);
int pthread_getaffinity_np(pthread_t t, size_t size, cpu_set_t *set);
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
int pthread_sigmask(int how, const sigset_t *set, sigset_t *old);
int pthread_setschedparam(pthread_t t, int policy, const struct sched_param *param);
int pthread_getschedparam(pthread_t t, int *policy, struct sched_param *param);

int pthread_attr_init(pthread_attr_t *a);
int pthread_attr_destroy(pthread_attr_t *a);
int pthread_attr_setdetachstate(pthread_attr_t *a, int v);
int pthread_attr_getdetachstate(const pthread_attr_t *a, int *v);
int pthread_attr_setstacksize(pthread_attr_t *a, size_t v);
int pthread_attr_getstacksize(const pthread_attr_t *a, size_t *v);
int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size);
int pthread_attr_getstack(const pthread_attr_t *a, void **addr, size_t *size);
int pthread_attr_setguardsize(pthread_attr_t *a, size_t v);
int pthread_attr_getguardsize(const pthread_attr_t *a, size_t *v);
int pthread_attr_setscope(pthread_attr_t *a, int v);
int pthread_attr_getscope(const pthread_attr_t *a, int *v);
int pthread_attr_setschedpolicy(pthread_attr_t *a, int v);
int pthread_attr_getschedpolicy(const pthread_attr_t *a, int *v);
int pthread_attr_setschedparam(pthread_attr_t *a, const struct sched_param *v);
int pthread_attr_getschedparam(const pthread_attr_t *a, struct sched_param *v);
int pthread_attr_setinheritsched(pthread_attr_t *a, int v);
int pthread_attr_getinheritsched(const pthread_attr_t *a, int *v);
int pthread_attr_setaffinity_np(pthread_attr_t *a, size_t size, const cpu_set_t *set);
int pthread_attr_getaffinity_np(const pthread_attr_t *a, size_t size, cpu_set_t *set);
int pthread_getattr_np(pthread_t t, pthread_attr_t *a);

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abstime);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_mutex_consistent(pthread_mutex_t *m);
int pthread_mutexattr_init(pthread_mutexattr_t *a);
int pthread_mutexattr_destroy(pthread_mutexattr_t *a);
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *a, int *type);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int v);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *a, int *v);
int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int v);
#define PTHREAD_MUTEX_ROBUST 1
#define PTHREAD_MUTEX_STALLED 0

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
int pthread_condattr_init(pthread_condattr_t *a);
int pthread_condattr_destroy(pthread_condattr_t *a);
int pthread_condattr_setpshared(pthread_condattr_t *a, int v);
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t c);

int pthread_rwlock_init(pthread_rwlock_t *l, const pthread_rwlockattr_t *a);
int pthread_rwlock_destroy(pthread_rwlock_t *l);
int pthread_rwlock_rdlock(pthread_rwlock_t *l);
int pthread_rwlock_wrlock(pthread_rwlock_t *l);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *l);
int pthread_rwlock_trywrlock(pthread_rwlock_t *l);
int pthread_rwlock_unlock(pthread_rwlock_t *l);
int pthread_rwlockattr_init(pthread_rwlockattr_t *a);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *a, int v);

int pthread_spin_init(pthread_spinlock_t *l, int pshared);
int pthread_spin_destroy(pthread_spinlock_t *l);
int pthread_spin_lock(pthread_spinlock_t *l);
int pthread_spin_trylock(pthread_spinlock_t *l);
int pthread_spin_unlock(pthread_spinlock_t *l);

int pthread_key_create(pthread_key_t *k, void (*dtor)(void *));
int pthread_key_delete(pthread_key_t k);
void *pthread_getspecific(pthread_key_t k);
int pthread_setspecific(pthread_key_t k, const void *v);
int pthread_once(pthread_once_t *o, void (*fn)(void));

#define pthread_cleanup_push(fn, arg) { void (*ols_cleanup_fn)(void *) = (void (*)(void *))(fn); void *ols_cleanup_arg = (arg);
#define pthread_cleanup_pop(exec) if (exec) ols_cleanup_fn(ols_cleanup_arg); }

#ifdef __cplusplus
}
#endif

#endif
