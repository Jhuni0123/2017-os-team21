/*
 * linux/kernel/ptree.c
 */

#include <linux/syscalls.h>
#include <linux/prinfo.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>

/*
 * task : &struct task_struct
 * only use when child exists
 */
#define first_child_task(task) \
    list_first_entry(&(task)->children, struct task_struct, sibling)

/* 
 * task : &struct task_struct
 * only use when next sibling exists
 */
#define next_sibling_task(task) \
    list_first_entry(&(task)->sibling, struct task_struct, sibling)

inline pid_t child_pid(struct task_struct *task)
{
    if(list_empty(&task->children))
        return 0;
    else 
        return first_child_task(task)->pid;
}

inline pid_t sibling_pid(struct task_struct *task)
{
    if(list_is_last(&task->sibling, &task->real_parent->children))
        return 0;
    else
        return next_sibling_task(task)->pid;
}

void print_prinfo(struct task_struct *task)
{
    printk("DEBUG: state:  %ld\n", task->state);
    printk("DEBUG: pid:    %d\n", task->pid);
    printk("DEBUG: parent  %d\n", task->real_parent->pid);
    printk("DEBUG: child   %d\n", child_pid(task));
    printk("DEBUG: sibling %d\n", sibling_pid(task));
    printk("DEBUG: uid:    %ld\n", (long)task->real_cred->uid);
    printk("DEBUG: comm:   %s\n", task->comm);
}

// only use after validate memory access
void __write_prinfo(struct prinfo *info, struct task_struct *task)
{
    info->state = task->state;
    info->pid = task->pid;
    info->parent_pid = task->real_parent->pid;
    info->first_child_pid = child_pid(task);
    info->next_sibling_pid = sibling_pid(task);
    info->uid = (long)(task->real_cred->uid);
    strncpy(info->comm, task->comm, TASK_COMM_LEN);
}

void dfs_task_rec(struct task_struct *task)
{
    struct task_struct *child;
    struct list_head *head = &task->children;
    list_for_each_entry(child, head, sibling) {
        // do something
        printk("DEBUG: pid: %d\n", child->pid);
        dfs_task_rec(child);
    }
}

int dfs_init_task(struct prinfo *kbuf, int buflen, int *knr)
{
    int copied = 0;
    int count = 0;
    struct task_struct *task = &init_task;

    while(true) {
        if(list_empty(&task->children)) {
            while(list_is_last(&task->sibling, &task->real_parent->children)) {
                task = task->real_parent;
            }
            task = next_sibling_task(task);
        } else
            task = first_child_task(task);

        if(task == &init_task)
            break;

        if(copied < buflen) {
            __write_prinfo(kbuf + copied, task);
            copied++;
        }
        count++;
    }
    (*knr) = copied;
    return count;
}

int do_ptree(struct prinfo *buf, int *nr)
{
    int buflen;
    int knr;
    int total;
    struct prinfo *kbuf;

    printk("DEBUG: ptree is called\n");

    if(buf == NULL || nr == NULL) {
        printk("DEBUG: ERROR: buf or nr is null pointer \n");
        return -EINVAL;
    }

    if(copy_from_user(&buflen, nr, sizeof(int))) {
        printk("DEBUG: ERROR: nr is not accessable\n");
        return -EFAULT;
    }

    if(buflen < 0) {
        printk("DEBUG: ERROR: nr less than 0\n");
        return -EINVAL;
    }

    kbuf = (struct prinfo *) kmalloc(sizeof(struct prinfo) * buflen, GFP_ATOMIC);

    if(kbuf == NULL) {
        printk("DEBUG: kmalloc failure for kbuf");
        return -ENOMEM;
    }

    printk("DEBUG: lock tasklist\n");
    read_lock(&tasklist_lock);

    total = dfs_init_task(kbuf, buflen, &knr);

    printk("DEBUG: unlock tasklist\n");
    read_unlock(&tasklist_lock);

    if(copy_to_user(buf, kbuf, sizeof(struct prinfo) * buflen)) {
        printk("DEBUG: copy_to_user to buf failure\n");
        return -EFAULT;
    }
    
    kfree(kbuf);

    if(copy_to_user(nr, &knr, sizeof(int))) {
        printk("DEBUG: copy_to_user to nr failure\n");
        return -EFAULT;
    }

    printk("DEBUG: end of ptree\n");
    return total;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr)
{
    return do_ptree(buf, nr);
}
