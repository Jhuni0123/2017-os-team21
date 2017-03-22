/*
 * linux/kernel/ptree.c
 */

#include <linux/syscalls.h>
#include <linux/prinfo.h>
#include <linux/sched.h>
#include <linux/list.h>

inline pid_t child_pid(struct task_struct *task){
    struct list_head *children_list = &(task->children);
    if (list_empty(children_list))return 0;
    else return list_first_entry(children_list, struct task_struct, sibling)->pid;
}

inline pid_t sibling_pid(struct task_struct *task){
    return list_first_entry(&(task->sibling), struct task_struct, sibling)->pid;
}

void print_prinfo(struct task_struct *task){
    printk("DEBUG: state:  %ld\n", task->state);
    printk("DEBUG: pid:    %d\n", task->pid);
    printk("DEBUG: parent  %d\n", task->real_parent->pid);
    printk("DEBUG: child   %d\n", child_pid(task));
    printk("DEBUG: sibling %d\n", sibling_pid(task));
    printk("DEBUG: uid:    %ld\n", (long)task->real_cred->uid);
    printk("DEBUG: comm:   %s\n", task->comm);
}

int do_ptree(struct prinfo *buf, int *nr)
{
	printk("DEBUG: ptree is called\n");

    printk("DEBUG: lock tasklist\n");
    read_lock(&tasklist_lock);

    printk("DEBUG: ==INIT TASK==\n");
    print_prinfo(&init_task);

    printk("DEBUG: ==TASK PID 1==\n");
    struct task_struct *task_pid1 = find_task_by_vpid(1);
    print_prinfo(task_pid1);

    printk("DEBUG: ==TASK CHILD OF PID 1==\n");
    print_prinfo(find_task_by_vpid(child_pid(task_pid1)));

    printk("DEBUG: ==TASK PID 2==\n");
    print_prinfo(find_task_by_vpid(2));

    printk("DEBUG: unlock tasklist\n");
    read_unlock(&tasklist_lock);

    printk("DEBUG: end of ptree\n");
	return 0;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr)
{
	return do_ptree(buf, nr);
}
