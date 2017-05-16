# Project 3: team 21
## Syscall number

| System Call | number |
| -------- | :--------: |
| set_weight | 380 |
| get_weight | 381 |

(SPECIFICATION MODIFICATION)`set_weight` syscall and `get_weight` syscall are handled by `do_sched_setweight` and `do_sched_getweight` functions in core.c, respectively. `SYSCALL_DEFINE`s also in core.c. Specification document dictated system calls be implemented in `kernel/sched.c`, but the file does not exist and syscalls whose name starts with sched are defined in kernel/sched/core.c. So this team decided to put new syscalls into core.c. 

## struct sched_wrr_entity
Node of wrr runqueue. Included in `task_struct`.
```
struct sched_wrr_entity
{
  int weight; 
  int on_wrr_rq;
  struct list_head queue_node;
  unsigned int time_slice;
};
```
- `unsigned int time_slice`: track how long the task has been run for preemtion purpose. managed by `task_tick`. 

## struct wrr_rq
Dummy header for wrr runqueue. Included in `rq`.
```
struct wrr_rq {
  unsigned int wrr_nr_running;
  struct list_head queue_head;
  struct rq *rq;
  int weight_sum;
  u64 next_balancing;
};
```
- `unsigned int wrr_nr_running`: number of tasks on wrr runqueue
- `int weight_sum`: sum of weights of all tasks on wrr runqueue
- `u64 next_balancing`: track how much time is left to call `load_balance`.

## Functionality of implemented sched_class interface functions

These functions are called by functions in `core.c`. It is guaranteed that these functions are called with `rq.lock` locked.

1. `void enqueue_task_wrr()` : Add `wrr_se` node of given `task_struct` to the end of `wrr_rq`. Update `wrr_nr_running` and `weight_sum` of `wrr_rq`, and `on_wrr_rq` of `wrr_se`.

1. `void dequeue_task_wrr()` : Delete `wrr_se` node of given `task_struct` from `wrr_rq`. Update `wrr_nr_running` and `weight_sum` of `wrr_rq`, and `on_wrr_rq` of `wrr_se`.

1. `void yield_task_wrr()` : Delete `wrr_se` node of given `task_struct` from `wrr_rq` and put it to the end of `wrr_rq` again. No need to update parameters.

1. `struct task_struct *pick_next_task_wrr()` : Return the next task to be run. It is called only after afore-running task has been dequeued or requeued, so it is safe to pick the first entry of `wrr_rq` following FIFO rule of wrr. Update `time_slice` of `wrr_se` of that selected task to represent its weight correctly. This function does not remove `wrr_se` from `wrr_rq` but just give pointer to the task. Thus, currently running process always locates in the first node of `wrr_rq`.

1. `int select_task_rq_wrr()` : Only defined under multi process condition. Find cpu with minimum weight_sum and return its number so that new task can be added to that cpu, under load balancing purpose.

1. `void task_tick_wrr()` : Timer code calls task_tick with HZ frequency if a task of this class is currently running on the cpu. Update schedstat and `time_slice` of `wrr_se` to reflect time the task has been runnnig. If running time has expired for the task, reset time_slice, requeue it on `wrr_rq`, and call `set_tsk_need_resched` to reschedule it.

1. `void task_fork_wrr()` : Called for the child process newly created by fork. Reset `on_wrr_rq` to reflect the fact that child process is yet to be scheduled.

## Usage of locks in `load_balance` implementation

- Used `rcu_read_lock()` and to safely acces `weight_sum` values of all usable cpus. `weight_sum` values are read to find out which cpu has maximum `weight_sum` and which has minimum one. Unlock with `rcu_read_unlock()` right after traversal.
- Hold double lock for rq's of both `max_cpu` and `min_cpu` before entering critical section of traversing all `wrr_se` entries of `max_cpu`'s `wrr_rq` and moving appropriate task from `max_cpu` to `min_cpu`. Need to call save irq flags before holding double lock. Right after critical section, release double lock and restore irq flags.


## How sched_class interface functions are used to implement task managing functionalities
- task is made with `fork()` and need to be put to `wrr_rq`

If `fork()` is executed, `sched_fork` of core.c is called. It refers to `policy` in `task_struct` of child process to find out what class it belongs to and update `sched_class` variable in `task_struct`. Then it calls `task_fork_wrr` to update variables. After this, the 'now inactive' child process is waken by `wake_up_new_task` in core.c. The function calls `select_task_rq_wrr` to decide which cpu to put this task into. Then call `enqueue_task_wrr` to put it into `wrr_rq`. 

- task has used up all its allocated `time_slice` but is not done yet.

`task_tick_wrr` checks if currently running task has used up all its allocated time. If it has, `task_tick_wrr` requeues the process and reschedule it.

- task is done or it sleeps.

`dequeue_task_wrr` is called to remove it from `wrr_rq`. 

- load balancing

Timer code calls `scheduler_tick` with HZ frequency. This functions calls `trigger_load_balance_wrr`. It checks if the caller is the first possible cpu, and if it is, calls `check_load_balance_wrr` to check if it's time for load balancing - that is, 2 seconds. If it's time for load balancing, `check_load_balance_wrr` calls `load_balance` to do the work. 

By making `scheduler_tick`, not `task_tick_wrr` to call `trigger_load_balance_wrr`, load balancing can be done regardless of first cpu currently running or not. Only one cpu is dedicated to do load balancing since load_balancing should be called only one in 2 seconds, and it is not guaranteed that all cpus share same time measures.


## How to set wrr as default scheme
in inclue/linux/init_task.h
```
#define INIT_TASK(tsk)
{
...
  .policy = SCHED_WRR,
...
  .wrr = {
    .weight = WRR_DEFAULT_WEIGHT;
    .queue_node = LIST_HEAD_INIT(tsk.wrr.queue_node),
    .time_slice = WRR_DEFAULT_WEIGHT * WRR_TIMESLICE,
  },
...
}
```
- set policy as `SCHED_WRR` and initiate `sched_wrr_entity wrr` of `task_struct`

in kernel/kthread.c
```
struct task_struct *kthread_create_on_node( ... )
{
  ...
  sched_setscheduler_noched(create.result, SCHED_WRR, &param);
  ...
}
```
- substitute `SCHED_NORMAL` with `SCHED_WRR`

in kernel/sched/core.c
- substitute fair with wrr in `sched_init`
- assign `wrr_sched_class` in `sched_fork`, `__sched_setscheduler` when the policy is `SCHED_WRR`.

in rt.c
- substitue `&fair_sched_class` with `&wrr_sched_class` in `rt_sched_class->next`

## Improvement

This team adopted aging policy to improve performance. Objective is to give long-running processes bigger weight so that they would not be stalled more but be done earlier. Variables of `aging_time_slice` and `aging_weight` are added to `sched_wrr_entity`. `aging_time_slice` checks times left to increment weight, and `aging_weight` tracks the amount of weight incremented by aging so that later it can be used to reset weight. `aging_time_slice` is incremented every `task_tick` and if it reaches 200, which means the task has been run for 1 second, `weight` and `aging_weight` are incremented. When a task is dequeued from `wrr_rq` its `weight` is resetted, with the memory of `aging_weight`. This policy is to prevent tasks that run intermittently from having high weight.

This policy will be especially helpful whent there comes many short-running tasks while small number of long-running processe are being executed. In that case, long-running processes need to wait for coming short-running tasks to run for their share fully, and will finish late. Aging helps it.

In order to run it, uncomment `#define CONFIG_WRR_AGING` in `include/linux/sched.h`. It is commented in proj3 branch.


## debug.c
Add call to `print_wrr_stats` in `print_cpu` function. `print_wrr_stats` implemented in `wrr.c`. It calls `print_wrr_rq` implemented in `debug.c`, which prints out cpu number, `wrr_nr_running`, and `weight_sum` for given `wrr_rq`.

## Plot 
refer to [plot.pdf](https://github.com/swsnu/os-team21/blob/proj3/plot.pdf) for explanation and results.

Every test codes are located in `artik/`.

## Demo Videos

1. [w/o aging](https://goo.gl/kOLCyS)
1. [w/ aging](https://goo.gl/5PGnfv)

Both are done on this environment: 16 infinite loops with weight 20 have been running. Execute `ageplot.c` that runs a task that starts with weight 1 that do prime factorization for n trials. When test with aging, it ages while preparing test, so trials are run with weight 20. Performance improved by 8 times.
