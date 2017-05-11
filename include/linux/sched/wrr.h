#ifndef _SCHED_WRR_H
#define _SCHED_WRR_H

#include <linux/sched.h>

#define WRR_DEFAULT_WEIGHT 10
#define WRR_TIMESLICE (10 * HZ / 1000)
#define WRR_BALANCE_PERIOD (2000 * HZ / 1000)

#endif /* _SCHED_WRR_H */
