# Project 2: team 21
## Syscall number

| System Call | number |
| -------- | :--------: |
| set_rotation | 380 |
| rotlock_read | 381 |
| rotlock_write | 382 |
| rotunlock_read | 383 |
| rotunlock_write | 385 |

## struct range_desc

Range descriptor to use as a list entry
```C
struct range_desc
{
	int degree;
	int range;
	pid_t tid;
	struct task_struct* task;
	bool assigned;
	struct list_head node;
};
```
- `struct task_struct* task`: to wake up task immediately
- `bool assigned`: condition for sleeping task to check if lock is assigned

## range_desc lists

`waiting_reads`, `waiting_writes`, `assigned_reads`, `assigned_writes`

We manage 4 lists in the project, for
- performance
- minimalizing `struct range_desc` data (divide entries by their attribute)

## Lock assigning policy
`find_assign_rotlock()` is a main function of the whole project, which is called in almost every syscalls.
The function does:
- lookup assigned write/read lock range
- iterate waiting write locks, assign it if possible
- if not, iterate waiting read locks, assigning all locks in current rotation

So our assigning policy is:
- assign waiting write locks first if any in current rotation (to prevent writer starvation)
- if not, assign all waiting read locks in current rotation
- FIFO order among writers/readers

## Exit handler

In `kernel/exit.c`, we added `exit_rotlock()` function to handle unreleased locks.

## Optimizations
-------
### List range lookup
In `find_assign_rotlock()`, we made code like 
``` C++
for(entry e1 : waiting locks)
  for(entry e2 : assigned_locks)
    if(no_overlap_with_assigned)
      assign_lock(e1);
```
We made pre-calculation of assigned locks, which made double for-loop into single for-loop.

### rotlock_count
We added `rotlock_count` in `struct task_struct`, to significantly reduce overhead of doing `rotlock_exit()`.
`rotlock_count` is incremented when they grab a lock, and decremented when they release it.
So if `rotlock_count` is 0, the process can exit without iterating all the rotlock entries.

## Concurrency issues
-------
We considered many lock styles, but all had potential hazards.

### RCU
Since list accesses had high chance to write, the performance would not be good.
We considered pseudo-RCU locks with no writer garbage collection, but there was a chance to panic when the entry is `kfree`d.

### Fine grained locks (per list entry)
If entry is `kfree`d, there's little chance that next lock grabber would segfault.
Grabbing contiguous 2 locks would solve the problem, but implemention was complex.
Also, we concluded this approach had huge overhead with grabbing locks.

### Conclusion
Considering arbitary removal in list, we had no way but to make a lock to the entire list to prevent potential hazards.
There would be a better way to solve the issue if we change the whole data structure, but the process would take so much time.
Thus we ended up a state like this.

## Selector & Trial
---------

TODO: 쥬니가 쓰기

how to execute?

## Test cases
- invalid unlock range
- double unlock
- assigning policy check
- 2 thread having same rotation range
- process exit handler check
