# Project 3: team 21
## Syscall number

| System Call | number |
| -------- | :--------: |
| set_weight | 380 |
| get_weight | 381 |

`set_weight` syscall and `get_weight` syscall are handled by `do_sched_setweight` and `do_sched_getweight` functions in core.c, respectively. `SYSCALL_DEFINE`s also in core.c.

## struct sched_wrr_entity
Node of wrr runqueue. Included in task_struct.
```
struct sched_wrr_entity
{
  int weight; 
  int on_wrr_rq;
  struct list_head queue_node;
  unsigned int time_slice;
};
```
- `unsigned int time_slice`: track how long the task has been run for preemtion purpose. managed by task_tick. 

## struct wrr_rq
Dummy header for wrr runqueue. Included in rq.
```
struct wrr_rq {
  unsigned int wrr_nr_running;
  struct sched_wrr_entity *curr;
  struct list_head queue_head;
  struct rq *rq;
  int weight_sum;
  u64 next_balancing;
};
```
- `unsigned int wrr_nr_running`: number of tasks on runqueue
- `sched_wrr_entity *curr`: 
- `int weight_sum`: sum of weights of all tasks on runqueue
- `u64 next_balancing`:

## Functionality of implemented sched_class interface functions

These functions are called by functions in core.c. It is guaranteed that these functions are called with `rq.lock` locked.

1. `void enqueue_task_wrr()` : Add `wrr_se` node of given `task_struct` to the end of `wrr_rq`. Update `wrr_nr_running` and `weight_sum` of `wrr_rq`, and `on_wrr_rq` of `wrr_se`.

1. `void dequeue_task_wrr()` : Delete `wrr_se` node of given `task_struct` from `wrr_rq`. Update `wrr_nr_running` and `weight_sum` of `wrr_rq`, and `on_wrr_rq` of `wrr_se`.

1. `void yield_task_wrr()` : Delete `wrr_se` node of given `task_struct` from `wrr_rq` and put it to the end of `wrr_rq` again. No need to update parameters.

1. `struct task_struct *pick_next_task_wrr()` : Return the next task to be run. Basically pick the first entry of `wrr_rq`. If it is already running, since there can be only one running process in `wrr_rq`, it is safe to pick the next one. Update `time_slice` of `wrr_se` of that selected task to represent its weight correctly.

1. `int select_task_rq_wrr()` : Only definced under multi process condition. Find cpu with minimum weight_sum and return its number so that new task can be added to that cpu, under load balancing purpose.

1. `void task_tick_wrr()` : Timer code calls task_tick with HZ frequency if a task of this class is currently running on the cpu. Update schedstat and `time_slice` of `wrr_se` to reflect time the task has been runnnig. If running time has expired for the task, reset time_slice, requeue it on `wrr_rq`, and call `set_tsk_need_resched` to reschedule it.

1. `void task_fork_wrr()` : Called for the child process newly created by fork. Reset `on_wrr_rq` to reflect the fact that child process is yet to be scheduled.

## Usage of locks in `load_balance` implementation

- Used `rcu_read_lock()` and to safely acces `weight_sum` values of all usable cpus. `weight_sum` values are read to find out which cpu has maximum `weight_sum` and which has minimum one. Unlock with `rcu_read_unlock()` right after traversal.
- Hold doble lock for rq's of both `max_cpu` and `min_cpu` before entering critical section of traversing all `wrr_se` entries of `max_cpu`'s `wrr_rq` and moving appropriate task from `max_cpu` to `min_cpu`. Need to call save irq flags before holding double lock. Right after critical section, release double lock and restore irq flags.


## How sched_class interface functions are used to implement task managing functionalities
1. task is made with `fork()` and need to be put to `wrr_rq`

1. load balancing : Timer code calls scheduler_tick with HZ frequency. This functions calls trigger_load_balance_wrr(rq, cpu), and trigger_load_balance 

## How to set wrr as basic scheme
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
- set policy as SCHED_WRR and initiate sched_wrr_entity wrr of task_struct

in kernel/kthread.c
```
struct task_struct *kthread_create_on_node( ... )
{
  ...
  scehd_setscheduler_noched(create.result, SCHED_WRR, &param);
  ...
}
```
- substitute SCHED_NORMAL with SCHED_WRR

in kernel/sched/core.c
- substitute fair with wrr in `sched_fork`, `__sched_setscheduler`, `sched_init`

in rt.c
- substitue `&fair_sched_class` with `&wrr_sched_class` in `const struct sched_class rt_sched_class`

## debug.c
Add call to `print_wrr_stats` in `print_cpu` function. `print_wrr_stats` implemented in wrr.c. It calls `print_wrr_rq` implemented in debug.c, which prints out cpu number, `wrr_nr_running`, and `weight_sum` for given `wrr_rq`.

## Plot 
