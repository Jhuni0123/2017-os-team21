# Synopsis
proj1 implements a syscall 'ptree' at syscall number 380 that gets a buffer and size of that buffer from user to return number of processes now running with buffer filled with information of the processes. Int value to specify size of the buffer would also be updated.

# Files Changed
## Preparation
- `include/linux/prinfo.h` 
  - define `struct prinfo`

## Define Syscall of ptree

- `include/linux/syscalls.h`
  - define `sys_ptree`
  - include `linux/prinfo.h`
    
- `arch/arm/include/asm/unistd.h`
  - enlarge number of syscalls from 380 to 384, in alignment of 4 bytes

- `arch/arm/include/uapi/asm/unistd.h`
  - define ptree as `syscall_base+380`

- `arch/arm/kernel/calls.S`
  - include call of `sys_ptree`

## Make ptree Logic

- `kernel/ptree.c`
  - to be explained.

## Test Code
Every test codes are placed in `artik/`.

- `artik/main.c`
  - print out process tree

- `artik/test.c`
  - test for developers to check if `ptree` catch errors correctly.

# How ptree Is Implemented

`sys_ptree` gets two arguments: `struct prinfo * buf` and `int * nr`.

ptree checks errors of
1. `EINVAL`: `buf`, `nr` null pointers 
2. `EFAULT`: `buf`, `nr` not in user address space
3. `ENOMEM`: unable to allocate kernel heap memory

It locks `tasklist_lock` and runs dfs starting from `init_task`.
While running dfs, it copies process information into kernel buffer `kbuf`.
After dfs, it unlocks `tasklist_lock` and copy kernel buffer entries into user buffer `buf`.
Then it updates `nr` to match the number of entries copied from `kbuf` to `buf` and returns process count.

## Note: How Children and Sibling Tasks are connected in linked list

In `task_struct` there are two `list_head`s: `children` and `sibling`, that make one `task_struct` to be a member of two circular doubly linked lists.
For `children` linked list, next points to `list_head sibling` of its child task. Then it flows into siblings of that child task, enabling user to surf through all children processes. If there is no child task, next points to itself.
For `sibling` linked list, next points to `list_head sibling`of its sibling task. If there is no more sibling, it points to `list_head children` of its parent task.
Exceptionally for init_task, which has no parent nor sibling, `sibling` linked list's next points to itself.
After all, `sibling` list_head forms the main linked list and `children` functions as a dummy head of that main list.
Because it's a linked list with dummy head, next pointing to itself means an empty list. That's why list_empty can be used to check if a task has a child or not.


## Functions and Fuctionalities
- `first_child_task(&struct task_struct)`
  - macro: exploits `list_first_entry` macro. only used when child exists.

- `next_sibling_task(&struct task_struct)`
  - macro: exploits `list_first_entry` macro. only used when next sibling exists.

- `pid_t child_pid(struct task_struct *)`
  - function: exploits `list_empty` and `first_child_task` macro. Used for `prinfo`.

- `pid_t sibling_pid(struct task_struct *)`
  - function: exploits `list_is_last` and `next_sibling_task` macro. Used for `prinfo`.

- `void print_prinfo(struct task_struct *)`
  - function: debugging purpose

- `void __write_prinfo (struct prinfo *, struct task_struct *)`
  - function: writes task infomation to `prinfo`
  
- `void dfs_task_rec(struct task_struct *)`
  - function: recursive dfs. Implemented but not used.

- `int dfs_init_task(struct prinfo *, int, int *)`
  - function: runs dfs starting from `init_task`, without stack but with if. 
  
1. If current task has child task (`!list_empty(&task->children)`) it moves on to that child.
1. If current task has no child but next sibling task(`!list_is_last(&task->sibling, &task->real_parent->children)`), then it moves to that sibling.
1. If current task has no child and no sibling, it moves to its ancestors who have next sibling.
1. If current task comes all the way back to `init_task`, then it breaks.
1. While doing so, for each task current task has moved through, task information is copied to kbuf until buffer is full or it dfs through all processes.
1. Count of copied entries of buffer is updated to `int *`.
1. Returns count of all processes running.
  
- `int do_ptree(struct prinfo *, int *)`
  - function: checks for errors, define and malloc `kbuf`, lock `tasklist_lock`, call `dfs_init_task`, unlock `tasklist_lock`, copy from `kbuf` to `buf`, `knr` to `nr`, free `kbuf` and return result.
- `SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr)`
  - syscall: calls `do_ptree`.
