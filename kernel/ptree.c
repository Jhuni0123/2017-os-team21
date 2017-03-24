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


inline struct task_struct* first_child_task(struct task_struct *task){
    return list_first_entry(&task->children, struct task_struct, sibling);
}

inline struct task_struct* next_sibling_task(struct task_struct *task){
    return list_first_entry(&task->sibling, struct task_struct, sibling);
}

inline pid_t child_pid(struct task_struct *task){
    if(list_empty(&task->children))return 0;
    else return first_child_task(task)->pid;
}

inline pid_t sibling_pid(struct task_struct *task){
    if(list_is_last(&task->sibling, &task->real_parent->children))return 0;
    return next_sibling_task(task)->pid;
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

// only use after validate memory access
void __write_prinfo(struct prinfo *info, struct task_struct *task){
    info->state = task->state;
    info->pid = task->pid;
    info->parent_pid = task->real_parent->pid;
    info->first_child_pid = child_pid(task);
    info->next_sibling_pid = sibling_pid(task);
    info->uid = (long)(task->real_cred->uid);
    strncpy(info->comm, task->comm, TASK_COMM_LEN);
}

void dfs_task_rec(struct task_struct *task){
    struct task_struct *child;
    struct list_head *head = &task->children;
    list_for_each_entry(child, head, sibling){
        // do something
        printk("DEBUG: pid: %d\n", child->pid);
        dfs_task_rec(child);
    }
}

int dfs_task(struct task_struct *init, struct prinfo *buf, int buflen, int *nr){
    int copied = 0;
    int count = 0;
    struct task_struct *task = init;
    while(true){
        if(list_empty(&task->children)){
            while(task != init && list_is_last(&task->sibling, &task->real_parent->children)){
                task = task->real_parent;
            }
            if(task == init)break;
            task = next_sibling_task(task);
        }
        else {
            task = first_child_task(task);
        }
        // do something
        printk("DEBUG: pid: %d\n", task->pid);
        if(copied < buflen){
            __write_prinfo(buf + copied, task);
            copied++;
        }
        count++;
    }
    (*nr) = copied;
    return count;
}

int do_ptree(struct prinfo *buf, int *nr)
{
    printk("DEBUG: ptree is called\n");

    if(buf == NULL || nr == NULL){
        printk("DEBUG: ERROR: buf or nr is null pointer \n");
        return -EINVAL;
    }

    if(!access_ok(int *, nr, 1)){
        return -EFAULT;
    }

    int buflen;
    int knr;
    int total;

    buflen = *nr;

    if(buflen < 0){
        return -EINVAL;
    }

    struct prinfo *kbuf = (struct prinfo *) kmalloc(sizeof(struct prinfo) * buflen, GFP_ATOMIC);

    if(kbuf == NULL){
        printk("DEBUG: kmalloc failure for kbuf");
        //todo: errno 
        return -1;
    }

    printk("DEBUG: lock tasklist\n");
    read_lock(&tasklist_lock);

    total = dfs_task(&init_task, kbuf, buflen, &knr);

    printk("DEBUG: unlock tasklist\n");
    read_unlock(&tasklist_lock);

    if(copy_to_user(buf, kbuf, sizeof(struct prinfo) * buflen)){
        printk("DEBUG: copy_to_user to buf failure\n");
        return -EFAULT;
    }
    
    *nr = knr;
    kfree(kbuf);

    printk("DEBUG: end of ptree\n");
    return total;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr){
    return do_ptree(buf, nr);
}
