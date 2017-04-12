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
    .degree = -1,   \
    .range = -1,    \
    .tid = -1,      \
    .task = NULL,   \
    .type = -1,     \
    .assigned = -1, \
    .node = { &(name.node), &(name.node) } \
}

extern int device_rot;

struct range_desc
{
	int degree;
	int range;
	pid_t tid;
	struct task_struct* task;
	enum rw_flag type;
	char assigned;
	struct list_head node;
};

/* circular list with dummy node */
extern struct range_desc waiting_reads;
extern struct range_desc waiting_writes;
extern struct range_desc assigned_reads;
extern struct range_desc assigned_writes;

extern void exit_rotlock();

/* rotation utility functions */
static int range_in_rotation(struct range_desc* rd)
{
	int low = rd->degree - rd->range;
	int high = rd->degree + rd->range;
	
	int rot;
	for(rot = device_rot - 360; rot <= device_rot + 360; rot += 360)
		if(low <= rot && rot <= high)
			return 1;
	
	return 0;
}

static int range_overlap(struct range_desc* rd1, struct range_desc* rd2)
{
	int low1 = rd1->degree - rd1->range - 360;
	int high1 = rd1->degree + rd1->range - 360;
	int low2 = rd2->degree - rd2->range;
	int high2 = rd2->degree + rd2->range;

	int cnt;
	for(cnt = 0; cnt < 3; cnt++) {
		if(low1 < high2 && low2 < high1)
			return 1;
		low1 += 360;
		high1 += 360;
	}
	return 0;
}

#endif

