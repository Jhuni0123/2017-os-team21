#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched.h>

enum rw_flag {
	READ_FLAG,
	WRITE_FLAG
};

#define RANGE_DESC_INIT(name) { \
	.degree = -1, \
	.range = -1, \
	.tid = -1, \
	.task = NULL, \
	.type = -1, \
	.assigned = false, \
	.node = LIST_HEAD_INIT(name.node) \
}

extern int device_rot;

struct range_desc
{
	int degree;
	int range;
	pid_t tid;
	struct task_struct* task;
	enum rw_flag type;
	bool assigned;
	struct list_head node;
};

/* circular list with dummy node */
extern struct range_desc waiting_reads;
extern struct range_desc waiting_writes;
extern struct range_desc assigned_reads;
extern struct range_desc assigned_writes;

extern void exit_rotlock(void);

/* rotation utility functions */

static inline bool rot_in_range(struct range_desc* rd)
{
	int dist = abs(device_rot - rd->degree);
	return min(dist, 360 - dist) <= rd->range;
}

static bool range_overlap(struct range_desc* rd1, struct range_desc* rd2)
{
	int dist = abs(rd1->degree - rd2->degree);
	return min(dist, 360 - dist) <= rd1->range + rd2->range;
}

static inline bool range_valid(int range)
{
	return 0 < range && range < 180;
}

static inline bool degree_valid(int degree)
{
	return 0 <= degree && degree < 360;
}

#endif

