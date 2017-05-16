#ifndef _SCHED_WRR_H
#define _SCHED_WRR_H

#include <linux/sched.h>

#define WRR_MIN_WEIGHT 1
#define WRR_MAX_WEIGHT 20
#define WRR_DEFAULT_WEIGHT 10

/* 10 msec */
#define WRR_TIMESLICE (10 * HZ / 1000)

/* 2 sec */
#define WRR_BALANCE_PERIOD 2

/* 1 sec */
#define WRR_AGING_TIME 200

#endif /* _SCHED_WRR_H */
