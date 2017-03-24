/*
 *  linux/fs/ptree.c
 */

#include <linux/syscalls.h>
#include <linux/prinfo.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/string.h>

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

void print_prinfo(struct  task_struct *task)
{
	printk("DEBUG: state : %ld\n", task->state);
	printk("DEBUG: pid:    %d\n", task->pid);
	printk("DEBUG: parent: %d\n", task->real_parent->pid);
	printk("DEBUG: child:  %d\n", child_pid(task));
	printk("DEBUG: sibling:%d\n", sibling_pid(task));
	printk("DEBUG: uid:    %ld\n", (long)task->real_cred->uid);
	printk("DEBUG: comm:   %s\n", task->comm);
}

int do_ptree(struct prinfo *buf, int *nr)
{
	printk("DEBUG: ptree is called\n");

	/* copy content of nr from user space */
	int num;
	copy_from_user(&num, nr, sizeof(int));

	/* error checking */
	/* check if buf and nr are null pointers */
	if(buf == NULL || nr == NULL || num < 0)
	{
		printk("DEBUG: null pointer error\n");
		return -EINVAL;
	}
	/* check if buf is not in right address space */
	if(!access_ok(struct prinfo *, buf, num))
	{
		printk("DEBUG: buf address space error\n");
		return -EFAULT;
	}

	/* assign memory to save things to write on buf */
	struct prinfo *kBuf = (struct prinfo *) kmalloc(sizeof(struct prinfo) * num, GFP_ATOMIC);
	if(kBuf == NULL)
	{
		printk("DEBUG: kmalloc failure for kBuf");
		//todo: errno 
		return -1;
	}
	int nextBIndex = 0; 

	/* define a stack for dfs */
	int MAX_ENTRY = 10000; // arbitrary number todo: optimization
	struct task_struct **dfsStack = kmalloc(sizeof(struct task_struct *) * MAX_ENTRY, GFP_ATOMIC) ; 
	if(dfsStack == NULL)
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
	printk("DEBUG: lock tasklist\n");
	read_lock(&tasklist_lock);

	/* get root process, that is init */
	struct task_struct *root = kmalloc(sizeof(struct task_struct), GFP_ATOMIC);
	if(root == NULL)
	{
		printk("DEBUG: kmalloc failure for root");
		//todo: errno
		return -1;
	}

	root = &init_task;
	
	printk("DEBUG: got init_task with pid %ld\n", root->pid);
	dfsStack[nextSIndex++] = root; // push it to stack. kBuf will be handled inside while loop

	/* define and initiate return result */
	int proc_num = 0;

	/* run dfs */
	while(nextSIndex > 0 )
	{
		/* pop stack */
		struct task_struct *curr = kmalloc(sizeof(struct task_struct), GFP_ATOMIC);
		curr = (dfsStack[--nextSIndex]);
		

		/* save info to kBuf if kBuf index < nr */
		if(nextBIndex < num)
		{
			struct prinfo new;
			new.state = curr->state;
			new.pid = curr->pid;
			new.parent_pid = curr->real_parent->pid;
			new.first_child_pid = child_pid(curr);
			new.next_sibling_pid = sibling_pid(curr);
			new.uid = (long) curr->real_cred->uid;
			strncpy(new.comm, curr->comm, 64);
			kBuf[nextBIndex++] = new;
		}
		/* increment process num */
		proc_num++;
		
		/* put sibling to the stack */
		if(!list_empty(&(curr->sibling)))
		//if(sibling->pid != curr->pid) // assuming circularly linked list
		{
			struct task_struct *sibling = kmalloc(sizeof(struct task_struct), GFP_ATOMIC);
			sibling = list_first_entry(&(curr->sibling), struct task_struct, sibling);
			/*			
			if((nextSIndex<500) && (sibling->pid != 0))
			{
				dfsStack[nextSIndex++] = sibling;
			}
			*/
			
		}	

		/* put children to the stack */
		if(!list_empty(&(curr->children)))
		{
			struct task_struct *child = kmalloc(sizeof(struct task_struct), GFP_ATOMIC);
			child = list_first_entry(&(curr->children), struct task_struct, sibling);
			dfsStack[nextSIndex++] = child;
		}

	}

	/* unlock tasklist */
	printk("DEBUG: unlock tasklist\n");
	read_unlock(&tasklist_lock);

	/* copy kBuf into buf */
	/*
	if(copy_to_user((char *)buf, (char *)kBuf, num*sizeof(struct task_struct))==0)
	{
		printk("DEBUG: copy_to_user to buf failure\n");
		return -EFAULT;
	}
	*/

	/* free all malloced things */
	kfree(kBuf);
	kfree(dfsStack);
	kfree(root);
	//todo: what to do with curr's, child's and sibling's?

	/* end */
	printk("DEBUG: end of ptree\n");
	return proc_num;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr)
{
	return do_ptree(buf, nr);
}
