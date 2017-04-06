#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>

int do_rotlock_read(int degree, int range)
{
	struct range_desc* newitem =
		(struct range_desc*) kmalloc(sizeof(struct range_desc), GFP_KERNEL);
	if(newitem == NULL)
		return -ENOMEM;

	newitem->degree = degree;
	newitem->range = range;
	newitem->tid = task_pid_vnr(current);
	newitem->node = LIST_HEAD_INIT(newitem->node);

	list_add_tail(newitem, wait_read);

	/* make thread sleep */
	

}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
	return do_set_rotation(degree);
}

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
	return do_rotlock_read(degree, range);
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
	return 0;
	//return do_rotlock_write(degree, range);
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
	return 0;
	//return do_rotunlock_read(degree, range);
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
	return 0;
	//return do_rotunlock_write(degree, range);
}

