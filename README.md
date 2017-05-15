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

## Functionality of sched_class interface functions
1. `enqueue_task_wrr`

Add `wrr_se` node of given `task_struct` to the end of `wrr_rq`. Update `wrr_nr_running` and `weight_sum` of `wrr_rq`, and `on_wrr_rq` of `wrr_se`.

1. `dequeue_task_wrr`

Delete `wrr_se` node of given `task_struct` from `wrr_rq`. Update `wrr_nr_running` and `weight_sum` of `wrr_rq`, and `on_wrr_rq` of `wrr_se`.

1. `yield_task_wrr`

Delete `wrr_se` node of given `task_struct` from `wrr_rq` and put it to the end of `wrr_rq` again. No need to update parameters.

1. `pick_next_task_wrr`



1. `load_balance`

## How sched_class interface functions are used to implement task managing functionalities
1. task is made with `fork()` and 
1. load balancing

Timer code calls scheduler_tick with HZ frequency. This functions calls trigger_load_balance_wrr(rq, cpu), and trigger_load_balance 

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
