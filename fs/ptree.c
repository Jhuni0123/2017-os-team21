/*
 *  linux/fs/ptree.c
 */

#include <linux/syscalls.h>
#include <linux/prinfo.h>

int do_ptree(struct prinfo *buf, int *nr)
{
	printk("ptree is called\n");
	// do implementation here
	return 0;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr)
{
	return do_ptree(buf, nr);
}
