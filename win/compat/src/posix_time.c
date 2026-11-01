#include "compat_internal.h"

static LARGE_INTEGER g_qpf;

static void ft_now(struct timespec *ts, int precise)
{
    FILETIME ft;
    uint64_t v;
    if (precise)
        GetSystemTimePreciseAsFileTime(&ft);
    else
        GetSystemTimeAsFileTime(&ft);
    v = (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) - 116444736000000000ULL;
    ts->tv_sec = (time_t)(v / 10000000ULL);
    ts->tv_nsec = (long)((v % 10000000ULL) * 100);
}

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    struct timespec ts;
    if (tv)
    {
        ft_now(&ts, 1);
        tv->tv_sec = (long)ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
    }
    if (tz)
    {
        TIME_ZONE_INFORMATION tzi;
        DWORD r = GetTimeZoneInformation(&tzi);
        tz->tz_minuteswest = tzi.Bias;
        tz->tz_dsttime = r == TIME_ZONE_ID_DAYLIGHT;
    }
    return 0;
}

int clock_gettime(clockid_t id, struct timespec *ts)
{
    LARGE_INTEGER c;
    FILETIME a, b, k, u;
    uint64_t v;
    switch (id)
    {
    case CLOCK_REALTIME:
        ft_now(ts, 1);
        return 0;
    case CLOCK_REALTIME_COARSE:
        ft_now(ts, 0);
        return 0;
    case CLOCK_MONOTONIC_COARSE:
        v = GetTickCount64();
        ts->tv_sec = (time_t)(v / 1000);
        ts->tv_nsec = (long)((v % 1000) * 1000000);
        return 0;
    case CLOCK_MONOTONIC:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_BOOTTIME:
        if (!g_qpf.QuadPart)
            QueryPerformanceFrequency(&g_qpf);
        QueryPerformanceCounter(&c);
        ts->tv_sec = (time_t)(c.QuadPart / g_qpf.QuadPart);
        ts->tv_nsec = (long)((c.QuadPart % g_qpf.QuadPart) * 1000000000LL / g_qpf.QuadPart);
        return 0;
    case CLOCK_PROCESS_CPUTIME_ID:
    case CLOCK_THREAD_CPUTIME_ID:
        if (id == CLOCK_PROCESS_CPUTIME_ID)
            GetProcessTimes(GetCurrentProcess(), &a, &b, &k, &u);
        else
            GetThreadTimes(GetCurrentThread(), &a, &b, &k, &u);
        v = (((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime)
            + (((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime);
        ts->tv_sec = (time_t)(v / 10000000ULL);
        ts->tv_nsec = (long)((v % 10000000ULL) * 100);
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

int clock_getres(clockid_t id, struct timespec *ts)
{
    ts->tv_sec = 0;
    ts->tv_nsec = (id == CLOCK_MONOTONIC_COARSE || id == CLOCK_REALTIME_COARSE) ? 15625000 : 100;
    return 0;
}

static void precise_sleep(int64_t ns)
{
    HANDLE t;
    LARGE_INTEGER due;
    if (ns <= 0)
    {
        SwitchToThread();
        return;
    }
    t = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!t)
    {
        Sleep((DWORD)((ns + 999999) / 1000000));
        return;
    }
    due.QuadPart = -(ns / 100 ? ns / 100 : 1);
    SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE);
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L)
    {
        errno = EINVAL;
        return -1;
    }
    precise_sleep((int64_t)req->tv_sec * 1000000000LL + req->tv_nsec);
    if (rem)
    {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

int usleep(useconds_t usec)
{
    precise_sleep((int64_t)usec * 1000);
    return 0;
}

unsigned int sleep(unsigned int sec)
{
    Sleep(sec * 1000);
    return 0;
}

struct tm *gmtime_r(const time_t *t, struct tm *tm)
{
    return gmtime_s(tm, t) == 0 ? tm : NULL;
}

struct tm *localtime_r(const time_t *t, struct tm *tm)
{
    return localtime_s(tm, t) == 0 ? tm : NULL;
}

char *asctime_r(const struct tm *tm, char *buf)
{
    return asctime_s(buf, 26, tm) == 0 ? buf : NULL;
}

char *ctime_r(const time_t *t, char *buf)
{
    return ctime_s(buf, 26, t) == 0 ? buf : NULL;
}

time_t timegm(struct tm *tm)
{
    return _mkgmtime(tm);
}

void tzset(void)
{
    _tzset();
}

static HANDLE g_alarm_timer;

static VOID CALLBACK alarm_cb(PVOID ctx, BOOLEAN fired)
{
    ols_raise(SIGALRM);
}

int setitimer(int which, const struct itimerval *nv, struct itimerval *ov)
{
    DWORD due, period;
    if (ov)
        memset(ov, 0, sizeof(*ov));
    if (which != ITIMER_REAL)
        return 0;
    if (g_alarm_timer)
    {
        DeleteTimerQueueTimer(NULL, g_alarm_timer, NULL);
        g_alarm_timer = NULL;
    }
    if (!nv || (nv->it_value.tv_sec == 0 && nv->it_value.tv_usec == 0))
        return 0;
    due = (DWORD)(nv->it_value.tv_sec * 1000 + nv->it_value.tv_usec / 1000);
    period = (DWORD)(nv->it_interval.tv_sec * 1000 + nv->it_interval.tv_usec / 1000);
    if (!CreateTimerQueueTimer(&g_alarm_timer, NULL, alarm_cb, NULL, due ? due : 1, period,
                               WT_EXECUTEDEFAULT))
        return ols_set_errno_win();
    return 0;
}

unsigned int alarm(unsigned int sec)
{
    struct itimerval v;
    memset(&v, 0, sizeof(v));
    v.it_value.tv_sec = sec;
    setitimer(ITIMER_REAL, &v, NULL);
    return 0;
}

clock_t times(struct tms *buf)
{
    FILETIME c, e, k, u;
    if (buf)
    {
        memset(buf, 0, sizeof(*buf));
        if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u))
        {
            buf->tms_utime = (clock_t)(((((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime)) / 100000);
            buf->tms_stime = (clock_t)(((((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime)) / 100000);
        }
    }
    return (clock_t)(GetTickCount64() / 10);
}

static int parse_num(const char **s, int min, int max, int digits, int *out)
{
    int v = 0, n = 0;
    while (**s == ' ')
        ++*s;
    while (n < digits && **s >= '0' && **s <= '9')
    {
        v = v * 10 + (**s - '0');
        ++*s;
        ++n;
    }
    if (n == 0 || v < min || v > max)
        return -1;
    *out = v;
    return 0;
}

static const char *g_mon[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
static const char *g_day[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };

char *strptime(const char *s, const char *fmt, struct tm *tm)
{
    int v, i;
    while (*fmt)
    {
        if (*fmt == ' ')
        {
            while (*s == ' ')
                ++s;
            ++fmt;
            continue;
        }
        if (*fmt != '%')
        {
            if (*s != *fmt)
                return NULL;
            ++s;
            ++fmt;
            continue;
        }
        ++fmt;
        switch (*fmt++)
        {
        case 'Y':
            if (parse_num(&s, 0, 9999, 4, &v))
                return NULL;
            tm->tm_year = v - 1900;
            break;
        case 'm':
            if (parse_num(&s, 1, 12, 2, &v))
                return NULL;
            tm->tm_mon = v - 1;
            break;
        case 'd':
        case 'e':
            if (parse_num(&s, 1, 31, 2, &v))
                return NULL;
            tm->tm_mday = v;
            break;
        case 'H':
            if (parse_num(&s, 0, 23, 2, &v))
                return NULL;
            tm->tm_hour = v;
            break;
        case 'M':
            if (parse_num(&s, 0, 59, 2, &v))
                return NULL;
            tm->tm_min = v;
            break;
        case 'S':
            if (parse_num(&s, 0, 61, 2, &v))
                return NULL;
            tm->tm_sec = v;
            break;
        case 'b':
        case 'B':
        case 'h':
            for (i = 0; i < 12; ++i)
                if (_strnicmp(s, g_mon[i], 3) == 0)
                    break;
            if (i == 12)
                return NULL;
            tm->tm_mon = i;
            s += 3;
            while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))
                ++s;
            break;
        case 'a':
        case 'A':
            for (i = 0; i < 7; ++i)
                if (_strnicmp(s, g_day[i], 3) == 0)
                    break;
            if (i == 7)
                return NULL;
            tm->tm_wday = i;
            s += 3;
            while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))
                ++s;
            break;
        case 'y':
            if (parse_num(&s, 0, 99, 2, &v))
                return NULL;
            tm->tm_year = v < 69 ? v + 100 : v;
            break;
        case 'Z':
            while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
                ++s;
            break;
        case 'z':
            if (*s == '+' || *s == '-')
                ++s;
            while (*s >= '0' && *s <= '9')
                ++s;
            break;
        case 'T':
            if (!(s = strptime(s, "%H:%M:%S", tm)))
                return NULL;
            break;
        case '%':
            if (*s++ != '%')
                return NULL;
            break;
        default:
            return NULL;
        }
    }
    return (char *)s;
}
