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
- `time_slice`:

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

