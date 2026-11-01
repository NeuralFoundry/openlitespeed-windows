#include "compat_internal.h"
#include <pthread.h>

typedef struct thread_rec
{
    DWORD id;
    HANDLE h;
    void *(*fn)(void *);
    void *arg;
    void *ret;
    int detached;
    struct thread_rec *next;
} thread_rec;

static SRWLOCK g_thr_lock = SRWLOCK_INIT;
static thread_rec *g_threads;

static thread_rec *find_thread(DWORD id, int unlink)
{
    thread_rec **pp, *r;
    for (pp = &g_threads; (r = *pp) != NULL; pp = &r->next)
        if (r->id == id)
        {
            if (unlink)
                *pp = r->next;
            return r;
        }
    return NULL;
}

static unsigned __stdcall thread_start(void *p)
{
    thread_rec *r = (thread_rec *)p;
    void *ret = r->fn(r->arg);
    AcquireSRWLockExclusive(&g_thr_lock);
    r->ret = ret;
    if (r->detached)
    {
        find_thread(r->id, 1);
        CloseHandle(r->h);
        free(r);
    }
    ReleaseSRWLockExclusive(&g_thr_lock);
    return 0;
}

static void apply_affinity(HANDLE h, const cpu_set_t *set)
{
    GROUP_AFFINITY ga;
    memset(&ga, 0, sizeof(ga));
    ga.Mask = (KAFFINITY)set->bits[0];
    if (ga.Mask)
        SetThreadGroupAffinity(h, &ga, NULL);
}

int pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg)
{
    thread_rec *r = (thread_rec *)calloc(1, sizeof(*r));
    unsigned id;
    size_t stack = attr && attr->stacksize ? attr->stacksize : 0;
    if (!r)
        return ENOMEM;
    r->fn = fn;
    r->arg = arg;
    r->detached = attr && attr->detach == PTHREAD_CREATE_DETACHED;
    AcquireSRWLockExclusive(&g_thr_lock);
    r->h = (HANDLE)_beginthreadex(NULL, (unsigned)stack, thread_start, r,
                                  CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION, &id);
    if (!r->h)
    {
        ReleaseSRWLockExclusive(&g_thr_lock);
        free(r);
        return EAGAIN;
    }
    r->id = id;
    r->next = g_threads;
    g_threads = r;
    ReleaseSRWLockExclusive(&g_thr_lock);
    if (attr && attr->has_cpus)
        apply_affinity(r->h, &attr->cpus);
    *t = (pthread_t)id;
    ResumeThread(r->h);
    return 0;
}

static int join_wait(pthread_t t, void **ret, DWORD ms)
{
    thread_rec *r;
    HANDLE h;
    DWORD w;
    AcquireSRWLockShared(&g_thr_lock);
    r = find_thread((DWORD)t, 0);
    h = r ? r->h : NULL;
    ReleaseSRWLockShared(&g_thr_lock);
    if (!r)
        return ESRCH;
    if (r->detached)
        return EINVAL;
    w = WaitForSingleObject(h, ms);
    if (w == WAIT_TIMEOUT)
        return ms == 0 ? EBUSY : ETIMEDOUT;
    AcquireSRWLockExclusive(&g_thr_lock);
    find_thread((DWORD)t, 1);
    ReleaseSRWLockExclusive(&g_thr_lock);
    if (ret)
        *ret = r->ret;
    CloseHandle(r->h);
    free(r);
    return 0;
}

int pthread_join(pthread_t t, void **ret)
{
    return join_wait(t, ret, INFINITE);
}

int pthread_tryjoin_np(pthread_t t, void **ret)
{
    return join_wait(t, ret, 0);
}

int pthread_timedjoin_np(pthread_t t, void **ret, const struct timespec *abstime)
{
    struct timespec now;
    int64_t ms;
    clock_gettime(CLOCK_REALTIME, &now);
    ms = (int64_t)(abstime->tv_sec - now.tv_sec) * 1000 + (abstime->tv_nsec - now.tv_nsec) / 1000000;
    if (ms < 0)
        ms = 0;
    return join_wait(t, ret, (DWORD)ms);
}

int pthread_detach(pthread_t t)
{
    thread_rec *r;
    AcquireSRWLockExclusive(&g_thr_lock);
    r = find_thread((DWORD)t, 0);
    if (!r)
    {
        ReleaseSRWLockExclusive(&g_thr_lock);
        return ESRCH;
    }
    if (WaitForSingleObject(r->h, 0) == WAIT_OBJECT_0)
    {
        find_thread((DWORD)t, 1);
        CloseHandle(r->h);
        free(r);
    }
    else
        r->detached = 1;
    ReleaseSRWLockExclusive(&g_thr_lock);
    return 0;
}

pthread_t pthread_self(void)
{
    return (pthread_t)GetCurrentThreadId();
}

int pthread_equal(pthread_t a, pthread_t b)
{
    return a == b;
}

void pthread_exit(void *ret)
{
    thread_rec *r;
    AcquireSRWLockExclusive(&g_thr_lock);
    r = find_thread(GetCurrentThreadId(), 0);
    if (r)
    {
        r->ret = ret;
        if (r->detached)
        {
            find_thread(r->id, 1);
            CloseHandle(r->h);
            free(r);
        }
    }
    ReleaseSRWLockExclusive(&g_thr_lock);
    _endthreadex(0);
}

int pthread_cancel(pthread_t t)
{
    return ENOSYS;
}

int pthread_kill(pthread_t t, int sig)
{
    if (sig == 0)
        return 0;
    return ols_raise(sig) == 0 ? 0 : errno;
}

int pthread_setcancelstate(int state, int *old)
{
    if (old)
        *old = PTHREAD_CANCEL_ENABLE;
    return 0;
}

int pthread_setcanceltype(int type, int *old)
{
    if (old)
        *old = PTHREAD_CANCEL_DEFERRED;
    return 0;
}

void pthread_testcancel(void)
{
}

int pthread_setname_np(pthread_t t, const char *name)
{
    wchar_t w[64];
    HANDLE h = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, (DWORD)t);
    if (!h)
        return ESRCH;
    MultiByteToWideChar(CP_UTF8, 0, name, -1, w, 64);
    w[63] = 0;
    SetThreadDescription(h, w);
    CloseHandle(h);
    return 0;
}

int pthread_getname_np(pthread_t t, char *name, size_t len)
{
    if (len)
        name[0] = 0;
    return 0;
}

int pthread_yield(void)
{
    SwitchToThread();
    return 0;
}

int pthread_setaffinity_np(pthread_t t, size_t size, const cpu_set_t *set)
{
    HANDLE h = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, (DWORD)t);
    if (!h)
        return ESRCH;
    apply_affinity(h, set);
    CloseHandle(h);
    return 0;
}

int pthread_getaffinity_np(pthread_t t, size_t size, cpu_set_t *set)
{
    DWORD_PTR pm, sm;
    memset(set, 0, sizeof(*set));
    if (GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm))
        set->bits[0] = pm;
    return 0;
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *old)
{
    return sigprocmask(how, set, old) == 0 ? 0 : errno;
}

int pthread_setschedparam(pthread_t t, int policy, const struct sched_param *param)
{
    return 0;
}

int pthread_getschedparam(pthread_t t, int *policy, struct sched_param *param)
{
    *policy = SCHED_OTHER;
    param->sched_priority = 0;
    return 0;
}

int pthread_attr_init(pthread_attr_t *a)
{
    memset(a, 0, sizeof(*a));
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *a)
{
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *a, int v) { a->detach = v; return 0; }
int pthread_attr_getdetachstate(const pthread_attr_t *a, int *v) { *v = a->detach; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t v) { a->stacksize = v; return 0; }
int pthread_attr_getstacksize(const pthread_attr_t *a, size_t *v) { *v = a->stacksize ? a->stacksize : 1024 * 1024; return 0; }
int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size) { a->stackaddr = addr; a->stacksize = size; return 0; }
int pthread_attr_getstack(const pthread_attr_t *a, void **addr, size_t *size) { *addr = a->stackaddr; *size = a->stacksize; return 0; }
int pthread_attr_setguardsize(pthread_attr_t *a, size_t v) { a->guardsize = v; return 0; }
int pthread_attr_getguardsize(const pthread_attr_t *a, size_t *v) { *v = a->guardsize; return 0; }
int pthread_attr_setscope(pthread_attr_t *a, int v) { a->scope = v; return 0; }
int pthread_attr_getscope(const pthread_attr_t *a, int *v) { *v = a->scope; return 0; }
int pthread_attr_setschedpolicy(pthread_attr_t *a, int v) { a->policy = v; return 0; }
int pthread_attr_getschedpolicy(const pthread_attr_t *a, int *v) { *v = a->policy; return 0; }
int pthread_attr_setschedparam(pthread_attr_t *a, const struct sched_param *v) { a->param = *v; return 0; }
int pthread_attr_getschedparam(const pthread_attr_t *a, struct sched_param *v) { *v = a->param; return 0; }
int pthread_attr_setinheritsched(pthread_attr_t *a, int v) { a->inherit = v; return 0; }
int pthread_attr_getinheritsched(const pthread_attr_t *a, int *v) { *v = a->inherit; return 0; }

int pthread_attr_setaffinity_np(pthread_attr_t *a, size_t size, const cpu_set_t *set)
{
    a->cpus = *set;
    a->has_cpus = 1;
    return 0;
}

int pthread_attr_getaffinity_np(const pthread_attr_t *a, size_t size, cpu_set_t *set)
{
    if (a->has_cpus)
        *set = a->cpus;
    else
        memset(set, 0, sizeof(*set));
    return 0;
}

int pthread_getattr_np(pthread_t t, pthread_attr_t *a)
{
    ULONG_PTR lo, hi;
    pthread_attr_init(a);
    if ((DWORD)t == GetCurrentThreadId())
    {
        GetCurrentThreadStackLimits(&lo, &hi);
        a->stackaddr = (void *)lo;
        a->stacksize = hi - lo;
    }
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
    InitializeSRWLock(&m->lock);
    m->type = a ? a->type : PTHREAD_MUTEX_NORMAL;
    m->owner = 0;
    m->count = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *m)
{
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
    DWORD self = GetCurrentThreadId();
    if (m->type != PTHREAD_MUTEX_NORMAL && m->owner == self)
    {
        if (m->type == PTHREAD_MUTEX_ERRORCHECK)
            return EDEADLK;
        ++m->count;
        return 0;
    }
    AcquireSRWLockExclusive(&m->lock);
    m->owner = self;
    m->count = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *m)
{
    DWORD self = GetCurrentThreadId();
    if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == self)
    {
        ++m->count;
        return 0;
    }
    if (!TryAcquireSRWLockExclusive(&m->lock))
        return EBUSY;
    m->owner = self;
    m->count = 1;
    return 0;
}

int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abstime)
{
    struct timespec now;
    for (;;)
    {
        if (pthread_mutex_trylock(m) == 0)
            return 0;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abstime->tv_sec
            || (now.tv_sec == abstime->tv_sec && now.tv_nsec >= abstime->tv_nsec))
            return ETIMEDOUT;
        Sleep(1);
    }
}

int pthread_mutex_unlock(pthread_mutex_t *m)
{
    if (m->type != PTHREAD_MUTEX_NORMAL)
    {
        if (m->owner != GetCurrentThreadId())
            return EPERM;
        if (--m->count > 0)
            return 0;
    }
    m->owner = 0;
    m->count = 0;
    ReleaseSRWLockExclusive(&m->lock);
    return 0;
}

int pthread_mutex_consistent(pthread_mutex_t *m)
{
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *a)
{
    a->type = PTHREAD_MUTEX_NORMAL;
    a->pshared = 0;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { a->type = type; return 0; }
int pthread_mutexattr_gettype(const pthread_mutexattr_t *a, int *type) { *type = a->type; return 0; }

int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int v)
{
    if (v == PTHREAD_PROCESS_SHARED)
        return ENOTSUP;
    a->pshared = v;
    return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *a, int *v) { *v = a->pshared; return 0; }
int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int v) { return 0; }

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a)
{
    InitializeConditionVariable(&c->cv);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *c)
{
    return 0;
}

static int cond_wait_ms(pthread_cond_t *c, pthread_mutex_t *m, DWORD ms)
{
    int count = m->count;
    BOOL ok;
    m->owner = 0;
    m->count = 0;
    ok = SleepConditionVariableSRW(&c->cv, &m->lock, ms, 0);
    m->owner = GetCurrentThreadId();
    m->count = count;
    if (!ok)
        return GetLastError() == ERROR_TIMEOUT ? ETIMEDOUT : EINVAL;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    return cond_wait_ms(c, m, INFINITE);
}

int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime)
{
    struct timespec now;
    int64_t ms;
    clock_gettime(CLOCK_REALTIME, &now);
    ms = (int64_t)(abstime->tv_sec - now.tv_sec) * 1000 + (abstime->tv_nsec - now.tv_nsec) / 1000000;
    if (ms < 0)
        ms = 0;
    return cond_wait_ms(c, m, (DWORD)ms);
}

int pthread_cond_signal(pthread_cond_t *c)
{
    WakeConditionVariable(&c->cv);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *c)
{
    WakeAllConditionVariable(&c->cv);
    return 0;
}

int pthread_condattr_init(pthread_condattr_t *a) { memset(a, 0, sizeof(*a)); return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { return 0; }
int pthread_condattr_setpshared(pthread_condattr_t *a, int v) { return v ? ENOTSUP : 0; }
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t c) { a->clock = c; return 0; }

int pthread_rwlock_init(pthread_rwlock_t *l, const pthread_rwlockattr_t *a)
{
    InitializeSRWLock(&l->lock);
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *l) { return 0; }

static __declspec(thread) int t_rw_excl;

int pthread_rwlock_rdlock(pthread_rwlock_t *l)
{
    AcquireSRWLockShared(&l->lock);
    t_rw_excl = 0;
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *l)
{
    AcquireSRWLockExclusive(&l->lock);
    t_rw_excl = 1;
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *l)
{
    if (!TryAcquireSRWLockShared(&l->lock))
        return EBUSY;
    t_rw_excl = 0;
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *l)
{
    if (!TryAcquireSRWLockExclusive(&l->lock))
        return EBUSY;
    t_rw_excl = 1;
    return 0;
}

int pthread_rwlock_unlock(pthread_rwlock_t *l)
{
    if (t_rw_excl)
        ReleaseSRWLockExclusive(&l->lock);
    else
        ReleaseSRWLockShared(&l->lock);
    return 0;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *a) { a->pshared = 0; return 0; }
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a) { return 0; }
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *a, int v) { return v ? ENOTSUP : 0; }

int pthread_spin_init(pthread_spinlock_t *l, int pshared)
{
    *l = 0;
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *l)
{
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *l)
{
    unsigned spins = 0;
    while (_InterlockedExchange(l, 1))
    {
        while (*l)
        {
            if (++spins < 64)
                _mm_pause();
            else
            {
                SwitchToThread();
                spins = 0;
            }
        }
    }
    return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *l)
{
    return _InterlockedExchange(l, 1) ? EBUSY : 0;
}

int pthread_spin_unlock(pthread_spinlock_t *l)
{
    _InterlockedExchange(l, 0);
    return 0;
}

int pthread_key_create(pthread_key_t *k, void (*dtor)(void *))
{
    DWORD idx = FlsAlloc((PFLS_CALLBACK_FUNCTION)dtor);
    if (idx == FLS_OUT_OF_INDEXES)
        return EAGAIN;
    *k = idx;
    return 0;
}

int pthread_key_delete(pthread_key_t k)
{
    FlsFree(k);
    return 0;
}

void *pthread_getspecific(pthread_key_t k)
{
    return FlsGetValue(k);
}

int pthread_setspecific(pthread_key_t k, const void *v)
{
    return FlsSetValue(k, (PVOID)v) ? 0 : EINVAL;
}

static BOOL CALLBACK once_cb(PINIT_ONCE o, PVOID p, PVOID *ctx)
{
    ((void (*)(void))p)();
    return TRUE;
}

int pthread_once(pthread_once_t *o, void (*fn)(void))
{
    InitOnceExecuteOnce(o, once_cb, (PVOID)fn, NULL);
    return 0;
}

int ols_cpu_count(const cpu_set_t *s)
{
    int i, n = 0;
    for (i = 0; i < CPU_SETSIZE / 64; ++i)
        n += (int)__popcnt64(s->bits[i]);
    return n;
}

int sched_yield(void)
{
    SwitchToThread();
    return 0;
}

int sched_getcpu(void)
{
    return (int)GetCurrentProcessorNumber();
}

int sched_setaffinity(pid_t pid, size_t size, const cpu_set_t *set)
{
    if (!SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)set->bits[0]))
        return ols_set_errno_win();
    return 0;
}

int sched_getaffinity(pid_t pid, size_t size, cpu_set_t *set)
{
    DWORD_PTR pm, sm;
    memset(set, 0, sizeof(*set));
    if (!GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm))
        return ols_set_errno_win();
    set->bits[0] = pm;
    return 0;
}

int sched_get_priority_min(int policy) { return 0; }
int sched_get_priority_max(int policy) { return 0; }
int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param) { return 0; }
int sched_getscheduler(pid_t pid) { return SCHED_OTHER; }
