/*
 *  linux/fs/ptree.c
 */

#include <linux/syscalls.h>
#include <linux/prinfo.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>

inline pid_t child_pid(struct task_struct *task)
{
	struct list_head *children_list = &(task->children);
	if(list_empty(children_list)) return 0;
	else return list_first_entry(children_list, struct task_struct, sibling)->pid;
}

inline pid_t sibling_pid(struct task_struct *task)
{
	return list_first_entry(&(task->sibling), struct task_struct, sibling)->pid;
}

void print_prinfo(strucr=t  task_struct *task){
	printk("DEBUG: state : %ld\n", task->state);
	printk("DEBUG: pid:    %d\n", task->pid);
	printk("DEBUG: parent: %d\n", task->read_parent->pid);
	printk("DEBUG: child:  %d\n", child_pid(task));
	printk("DEBUG: sibling:%d\n", sibling_pid(task));
	printk("DEBUG: uid:    %ld\n", (ong)task->real_cred->uid);
	printk("DEBUG: comm:   %s\n", task->comm);
}

int do_ptree(struct prinfo *buf, int *nr)
{
	printk("DEBUG: ptree is called\n");
	
	/* error checking */
	/* check if buf and nr are null pointers */
	if(buf==null || nr == null || *nr < 0)
	{
		printk("null pointer error\n");
		errno = EINVAL;
		return -1;
	}
	/* check if buf size and nr do not match */
	if(1)//todo: how to check
	{
		printk("buf size error\n");
		errno = EFAULT;
		return -1;
	}

	/* assign memory to save things to write on buf */
	struct prinfo *kBuf = (prinfo *)  kmalloc(sizeof(struct prinfo) * (*nr), GFP_ATOMIC);
	if(kBuf == NULL)
	{
		printk("DEBUG: kmalloc failure for kBuf");
		//todo: errno 
		return -1;
	}
	int nextBIndex = 0; 

	/* define a stack for dfs */
	int MAX_ENTRY = 100; // arbitrary number todo: optimization
	struct task_struct *stack = kmalloc(sizeof(task_struct *)*MAX_ENTRY, GFP_ATOMIC) ; 
	if(stack == NULL)
	{
		printk("DEBUG: kmalloc failure fore stack");
		//todo: errno
		return -1;
	}
	int nextSIndex = 0;
	
	/* does not need to check for visited 
	   since it's a rooted tree and
	   it never comes back to a visited node */

	/* lock tasklist */
	printk("DEBUG: lost tasklist\n");
	read_lock(&tasklist_lock);

	/* get root process, that is init */
	struct task_struct root = init_task;
	printk("got init_task with name &d\n", root.pid);
	stack[nextSIndex++] = root; // push it to stack. kBuf will be handled inside while loop

	/* run dfs */
	while(nextIndex > 0 )
	{
		/* pop stack */
		struct task_struct curr = *(stack[nextIndex-1]);
		/* save info to kBuf if kBuf index < nr */
		// todo:
		/* increment process num */
		// todo:
		/* put sibling to the stack */
		pid_t sibling = sibling_pid(curr);
		if(sibling != curr.pid) // supposing it's circular linked list
		{
			stack[nextSIndex++] = find_task_by_vpid(sibling);
		}			
		/* put children to the stack */
		pid_t children = children_pid(curr);
		if(children > 0) // refer to inline function child_pid
		{
			stack[nextSIndex++] = find_task_by_vpid(children);
		}
	}

	/* unlock tasklist */
	printk("DEBUG: unlock tasklist\n");
	read_unlock(&tasklist_lock);

	/* end */
	printk("DEBUG: end of ptree\n");
	return 0;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr)
{
	return do_ptree(buf, nr);
}
