# Project 3: team 21
## Syscall number

| System Call | number |
| -------- | :--------: |
| set_weight | 380 |
| get_weight | 381 |

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

## functionality of sched_class interface functions
1. enqueue_task_wrr
1. dequeue_task_wrr


## how to set wrr as basic scheme
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


## Plot 
