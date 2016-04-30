#include "timespec.h"
#include <sys/time.h>
#include "ylog/ylog.h"
#include <stdio.h>

bool margin_cmp(const uint64_t value, const uint64_t ref, const uint64_t margin)
{
    if (value > ref) return (value - margin < ref);
    else return ( value + margin > ref );
}

//struct timespec nanosec_add(struct timespec start, struct timespec end)
//{
//    struct timespec temp;
//    temp.tv_nsec = end.tv_nsec + start.tv_nsec;
//    if ( temp.tv_nsec > 1000000000 )

//    temp.tv_sec = end.tv_sec-start.tv_sec-1;

//    return temp;
//}

bool timed_out(uint64_t timestamp, uint64_t curtime, uint64_t timeout)
{
        return timestamp + timeout <= curtime;
}

bool nanosec_timeout(const struct timespec *timestamp, const struct timespec *curtime, uint64_t timeout)
{
        struct timespec diff; // = nanosec_diff(timestamp, curtime);
        timespec_subtract(&diff, curtime, timestamp);
        uint64_t elasped = timespec_to_ns(&diff);
        return elasped >= timeout;
}

void timespec_now(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC_RAW, ts);
}

void timespec_addms(struct timespec *ts, long ms)
{
    int sec=ms/MSEC_PER_SEC;
    ms=ms-sec*MSEC_PER_SEC;

    // perform the addition
    ts->tv_nsec+=ms*NSEC_PER_MSEC;

    // adjust the time
    ts->tv_sec+=ts->tv_nsec/NSEC_PER_SEC + sec;
    ts->tv_nsec=ts->tv_nsec%NSEC_PER_SEC;
}

void timespec_addmicros(struct timespec *ts, long micro)
{
    int sec=micro/USEC_PER_SEC;
    micro=micro - sec*USEC_PER_SEC;

    // perform the addition
    ts->tv_nsec+=micro*NSEC_PER_USEC;

    // adjust the time
    ts->tv_sec+=ts->tv_nsec/NSEC_PER_SEC + sec;
    ts->tv_nsec=ts->tv_nsec%NSEC_PER_SEC;

}

void timespec_addns(struct timespec *ts, long ns)
{
    int sec=ns/NSEC_PER_SEC;
    ns=ns - sec*NSEC_PER_SEC;

    // perform the addition
    ts->tv_nsec+=ns;

    // adjust the time
    ts->tv_sec+=ts->tv_nsec/NSEC_PER_SEC + sec;
    ts->tv_nsec=ts->tv_nsec%NSEC_PER_SEC;

}

/*
 * https://www.guyrutenberg.com/2007/09/22/profiling-code-using-clock_gettime/
 *
 * TODO : manage err if start>end ?
 */
struct timespec nanosec_diff(const struct timespec *start, const struct timespec *end)
{
        struct timespec temp;

        if ((end->tv_nsec - start->tv_nsec) < 0) {
                temp.tv_sec = end->tv_sec - start->tv_sec - 1;
                temp.tv_nsec = NSEC_PER_SEC + end->tv_nsec - start->tv_nsec;
        } else {
                temp.tv_sec = end->tv_sec - start->tv_sec;
                temp.tv_nsec = end->tv_nsec - start->tv_nsec;
        }
        return temp;
}

void timespec_subtract(struct timespec *r, const struct timespec *a, const struct timespec *b)
{
    r->tv_sec = a->tv_sec;
    r->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (r->tv_nsec < 0) {
        // borrow.
        r->tv_nsec += NSEC_PER_SEC;
        r->tv_sec --;
    }
    r->tv_sec = r->tv_sec - b->tv_sec;
}

long timespec_milliseconds(struct timespec *a) 
{
    return a->tv_sec*MSEC_PER_SEC + a->tv_nsec/NSEC_PER_MSEC;
}

long timespec_microseconds(struct timespec *a) 
{
    return a->tv_sec*USEC_PER_SEC + a->tv_nsec/NSEC_PER_USEC;
}

/*
#ifndef div_long_long_rem

static inline unsigned long do_div_llr(const long long dividend,
                                        const long divisor, long *remainder)
{
         uint64_t result = dividend;

         *(remainder) = do_div(result, divisor);
         return (unsigned long) result;
}

#define div_long_long_rem(dividend, divisor, remainder) \
         do_div_llr((dividend), divisor, remainder)


#endif

static inline long div_long_long_rem_signed(const long long dividend,
                                             const long divisor, long *remainder)
{
         long res;

         if (unlikely(dividend < 0)) {
                 res = -div_long_long_rem(-dividend, divisor, remainder);
                 *remainder = -(*remainder);
         } else
                 res = div_long_long_rem(dividend, divisor, remainder);

         return res;
 }


void set_normalized_timespec(struct timespec *ts, time_t sec, long nsec)
{
         while (nsec >= NSEC_PER_SEC) {
                 nsec -= NSEC_PER_SEC;
                 ++sec;
         }
         while (nsec < 0) {
                 nsec += NSEC_PER_SEC;
                 --sec;
        }
         ts->tv_sec = sec;
         ts->tv_nsec = nsec;
}

struct timespec ns_to_timespec(const int64_t nsec)
{
    struct timespec ts;

    if (!nsec)
        return (struct timespec) {0, 0};

    ts.tv_sec = div_long_long_rem_signed(nsec, NSEC_PER_SEC, &ts.tv_nsec);
    if (unlikely(nsec < 0))
        set_normalized_timespec(&ts, ts.tv_sec, ts.tv_nsec);

    return ts;
}

*/
