#ifndef _timespec_h
#define _timespec_h

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs);

/**
 * @brief margin_cmp
 * @param value
 * @param ref
 * @param margin
 * @return true if value is [ref-margin,ref+marging]
 */
bool margin_cmp(const uint64_t value, const uint64_t ref, const uint64_t margin);

/**
 * @brief
 *  return end - start
 * @param start
 * @param end
 * @return
 */
struct timespec nanosec_diff(const struct timespec *start, const struct timespec *end);
bool nanosec_timeout(const struct timespec *timestamp, const struct timespec *curtime, uint64_t timeout);
bool timed_out(uint64_t timestamp, uint64_t curtime, uint64_t timeout);

/* Parameters used to convert the timespec values: */
#define HOUR_PER_DAY    24L
#define SEC_PER_HOUR    3600L
#define MSEC_PER_SEC	1000L /* millisecond per second */
#define USEC_PER_MSEC	1000L
#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L
#define USEC_PER_SEC	1000000L
#define NSEC_PER_SEC	1000000000L
#define FSEC_PER_SEC	1000000000000000LL

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define MAX_NS  999999999L

void timespec_now(struct timespec *ts);
void timespec_addms(struct timespec *ts, long ms);
void timespec_addmicros(struct timespec *ts, long micro);
void timespec_addns(struct timespec *ts, long ns);

static inline int timespec_equal(const struct timespec *a,
                                 const struct timespec *b)
{
    return (a->tv_sec == b->tv_sec) && (a->tv_nsec == b->tv_nsec);
}


/*
 * lhs < rhs:  return <0
 * lhs == rhs: return 0
 * lhs > rhs:  return >0
 */
static inline int timespec_compare(const struct timespec *lhs, const struct timespec *rhs)
{
    if (lhs->tv_sec < rhs->tv_sec)
        return -1;
    if (lhs->tv_sec > rhs->tv_sec)
        return 1;
    return lhs->tv_nsec - rhs->tv_nsec;
}

/*
 * Returns true if the timespec is norm, false if denorm:
 */
static inline bool timespec_valid(const struct timespec *ts)
{
    /* Dates before 1970 are bogus */
    if (ts->tv_sec < 0)
        return false;
    /* Can't have more nanoseconds then a second */
    if ((unsigned long)ts->tv_nsec >= NSEC_PER_SEC)
        return false;
    return true;
}

/**
 * timespec_to_ns - Convert timespec to nanoseconds
 * @ts:		pointer to the timespec variable to be converted
 *
 * Returns the scalar nanosecond representation of the timespec
 * parameter.
 */
static inline int64_t timespec_to_ns(const struct timespec *ts)
{
    return (ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

/** computes r = a - b */
void timespec_subtract(struct timespec *r, const struct timespec *a, const struct timespec *b);

/** convert the timespec into milliseconds (may overflow) */
long timespec_milliseconds(struct timespec *a);

/** convert the timespec into microseconds (may overflow) */
long timespec_microseconds(struct timespec *a);

static inline uint32_t __iter_div_u64_rem(uint64_t dividend, uint32_t divisor, uint64_t *remainder)
{
    uint32_t ret = 0;

    while (dividend >= divisor) {
        /* The following asm() prevents the compiler from
       optimising this loop into a modulo operation.  */
        asm("" : "+rm"(dividend));

        dividend -= divisor;
        ret++;
    }

    *remainder = dividend;

    return ret;
}

/**
 * timespec_add_ns - Adds nanoseconds to a timespec64
 * @a:		pointer to timespec to be incremented
 * @ns:		unsigned nanoseconds value to be added
 *
 * This must always be inlined because its used from the x86-64 vdso,
 * which cannot call other kernel functions.
 */
static __always_inline void timespec_add_ns(struct timespec *a, uint64_t ns)
{
    a->tv_sec += __iter_div_u64_rem(a->tv_nsec + ns, NSEC_PER_SEC, &ns);
    a->tv_nsec = ns;
}

//me
static __always_inline void ns_to_timespecme(struct timespec *a, uint64_t ns)
{
    a->tv_sec = 0 ; a->tv_sec = 0;
    a->tv_sec += __iter_div_u64_rem(a->tv_nsec + ns, NSEC_PER_SEC, &ns);
    a->tv_nsec = ns;
}

#endif /* _timespec_h */
