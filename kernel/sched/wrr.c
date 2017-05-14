#include "sched.h"
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sched/wrr.h>

const struct sched_class wrr_sched_class;
static int load_balance(struct rq *this_rq);

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *rq)
{
	wrr_rq->wrr_nr_running = 0;
	wrr_rq->curr = NULL;
	INIT_LIST_HEAD(&wrr_rq->queue_head);
	wrr_rq->rq = rq;
	wrr_rq->next_balancing = 0;
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

static inline void __enqueue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se)
{
	list_add_tail(&wrr_se->queue_node, &wrr_rq->queue_head);
}

static inline void __dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
	list_del(&wrr_se->queue_node);
}

static inline void __requeue_wrr_entity(struct sched_wrr_entity *curr, struct wrr_rq *wrr_rq)
{
	list_move_tail(&curr->queue_node, &wrr_rq->queue_head);
}

static struct sched_wrr_entity *__pick_next_entity(struct wrr_rq *wrr_rq){
	
	struct sched_wrr_entity *next;

	next = list_first_entry_or_null(&wrr_rq->queue_head, struct sched_wrr_entity, queue_node);
	
	if(!next)
		return NULL;

	/* if there is only one entry in the queue,
	 * just return it regardless of it running right now */
	if(wrr_rq->wrr_nr_running == 1)
		return next;

	/* if it's already running, choose the next one */
	if(wrr_task_of(next) == wrr_rq->rq->curr)
		next = list_entry(wrr_rq->queue_head.next->next, struct sched_wrr_entity, queue_node);

	return next;
}

static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	if(curr->sched_class != &wrr_sched_class)
		return;

	delta_exec = rq->clock_task - curr->se.exec_start;
	if(unlikely((s64)delta_exec <= 0))
		return;

	/* update schedstat */
	schedstat_set(curr->se.statistics.exec_max,
			  max(curr->se.statistics.exec_max, delta_exec));
	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);
	curr->se.exec_start = rq->clock_task;
	cpuacct_charge(curr, delta_exec);
}

static void check_load_balance_wrr(struct wrr_rq *wrr_rq)
{
	u64 now = wrr_rq->rq->clock_task;
	
	if(wrr_rq->next_balancing == 0) {
		wrr_rq->next_balancing = now + WRR_BALANCE_PERIOD;
		return;
	}

	if(now > wrr_rq->next_balancing) {
		printk("DEBUG: now : %lld, next_balancing : %lld\n", now, wrr_rq->next_balancing);
		load_balance(wrr_rq->rq);
		/* unit of now, next_balancing is nsec */
		wrr_rq->next_balancing = now + WRR_BALANCE_PERIOD * 1000000000LL;
	}
}

void trigger_load_balance_wrr(struct rq *rq, int cpu)
{
	/* Check if load balancing is needed */
	unsigned int first_cpu = cpumask_first(cpu_online_mask);
	if(cpu == first_cpu)
		check_load_balance_wrr(&rq->wrr);
}
/*
 *********************************
 * Scheduler interface functions *
 *********************************
 */

void enqueue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{
	//printk("DEBUG: %d: enqueue\n", p->pid);
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if(wrr_se->on_wrr_rq)
		return;

	__enqueue_wrr_entity(wrr_rq, wrr_se);

	inc_nr_running(rq);
	wrr_rq->wrr_nr_running++;
	wrr_rq->weight_sum += wrr_se->weight;
	wrr_se->on_wrr_rq = 1; 
}

void dequeue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{
	//printk("DEBUG: %d: dequeue\n", p->pid);
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if(!(wrr_se->on_wrr_rq))
		return;
	
	__dequeue_wrr_entity(wrr_se);

	dec_nr_running(rq);
	wrr_rq->wrr_nr_running--;
	wrr_rq->weight_sum -= wrr_se->weight;
	wrr_se->on_wrr_rq = 0;
}

void yield_task_wrr (struct rq *rq)
{
	//printk("DEBUG: yield_task\n");
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct task_struct *curr = rq->curr;
	struct sched_wrr_entity *wrr_se = &curr->wrr;

	if(!(wrr_se->on_wrr_rq))
		return;

	__requeue_wrr_entity(wrr_se, wrr_rq);
}

bool yield_to_task_wrr (struct rq *rq, struct task_struct *p, bool preempt)
{
	//printk("DEBUG: %d: yield_to_task\n", p->pid);
	return false;
}

void check_preempt_curr_wrr (struct rq *rq, struct task_struct *p, int flags)
{
	//printk("DEBUG: %d: check_preempt_curr\n", p->pid);
}

struct task_struct *pick_next_task_wrr (struct rq *rq)
{
	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;
	struct wrr_rq *wrr_rq = &rq->wrr;

	wrr_se = __pick_next_entity(wrr_rq);

	if(!wrr_se)
		return NULL;
	p = wrr_task_of(wrr_se);

	//printk("DEBUG: pick_next_task %d\n", p->pid);

	wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;

	return p;
}

void put_prev_task_wrr (struct rq *rq, struct task_struct *p)
{
	//printk("DEBUG: %d: put_prev_task\n", p->pid);
}

#ifdef CONFIG_SMP
int  select_task_rq_wrr (struct task_struct *p, int sd_flag, int flags)
{
	//printk("DEBUG: %d: select_task_rq\n", p->pid);
	int i;
	int min_weight_sum = 1000000000;
	int min_cpu = -1;
	for_each_possible_cpu(i){
		struct rq *rq = cpu_rq(i);
		struct wrr_rq *wrr_rq = &rq->wrr;
		if(min_weight_sum > wrr_rq->weight_sum){
			min_cpu = i;
			min_weight_sum = wrr_rq->weight_sum;
		}
	}
	return min_cpu;
}

void migrate_task_rq_wrr (struct task_struct *p, int next_cpu)
{
	//printk("DEBUG: %d: migrate_task_rq\n", p->pid);
}

void pre_schedule_wrr (struct rq *this_rq, struct task_struct *task)
{
	//printk("DEBUG: pre_schedule\n");
}

void post_schedule_wrr (struct rq *this_rq)
{
	//printk("DEBUG: post_schedule\n");
}

void task_waking_wrr (struct task_struct *task)
{
	//printk("DEBUG: task_waking\n");
}

void task_woken_wrr (struct rq *this_rq, struct task_struct *task)
{
	//printk("DEBUG: task_woken\n");
}

void set_cpus_allowed_wrr (struct task_struct *p, const struct cpumask *newmask)
{
	//printk("DEBUG: %d: set_cpus_allowed\n", p->pid);
}

void rq_online_wrr (struct rq *rq)
{
	//printk("DEBUG: rq_online\n");
}

void rq_offline_wrr (struct rq *rq)
{
	//printk("DEBUG: rq_offline\n");
}
#endif

void set_curr_task_wrr (struct rq *rq)
{
	//printk("DEBUG: set_curr_task_wrr\n");
}

void task_tick_wrr (struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	update_curr_wrr(rq);
	
	if(p->policy != SCHED_WRR)
		return;
	
	/* Decrease time slice of task */
	if(--wrr_se->time_slice)
		return;

	wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;
	if(wrr_rq->queue_head.prev != wrr_rq->queue_head.next) { // If n_item > 2
		__requeue_wrr_entity(wrr_se, wrr_rq);
		set_tsk_need_resched(p);
	}
}

void task_fork_wrr (struct task_struct *p)
{
	//printk("DEBUG: p->pid = %d, current->pid = %d: task_fork on_rq = %d\n", p->pid, current->pid, p->on_rq);
	p->wrr.on_wrr_rq = 0;
}

void switched_from_wrr (struct rq *this_rq, struct task_struct *task)
{
	//printk("DEBUG: %d: twitched_from\n", task->pid);
}

void switched_to_wrr (struct rq *this_rq, struct task_struct *task)
{
	//printk("DEBUG: %d: switched_to\n", task->pid);
}

void prio_changed_wrr (struct rq *this_rq, struct task_struct *task, int oldprio)
{
	//printk("DEBUG: %d: prio_changed\n", task->pid);
}

unsigned int get_rr_interval_wrr (struct rq *rq, struct task_struct *task)
{
	//printk("DEBUG: %d: get_rr_interval\n", task->pid);
	return 0;
}

/* no runqueues are locked when call load_balance */
static int load_balance(struct rq *this_rq)
{
	int i;
	int max_cpu = -1;
	int min_cpu = -1;
	int max_weight = -1;
	int min_weight = 1000000000;
	unsigned long flags;
	struct wrr_rq *max_wrr_rq;
	struct wrr_rq *min_wrr_rq;
	struct sched_wrr_entity *pos;
	struct sched_wrr_entity *to_move;
	int movable_weight;
	int move_weight = -1;

	printk("DEBUG: rq %d, load_balance called\n", this_rq->cpu);
	for_each_possible_cpu(i){
		struct rq *rq = cpu_rq(i);
		struct wrr_rq *wrr_rq = &rq->wrr;
		if(max_weight < wrr_rq->weight_sum){
			max_cpu = i;
			max_weight = wrr_rq->weight_sum;
		}
		if(min_weight > wrr_rq->weight_sum){
			min_cpu = i;
			min_weight = wrr_rq->weight_sum;
		}
	}

	movable_weight = (max_weight - min_weight - 1) / 2;


	printk("DEBUG: max_cpu %d, max_weight %d, min_cpu %d, min_weight %d\n",
			max_cpu, max_weight, min_cpu, min_weight);

	if(max_weight == 0 || max_cpu == min_cpu){
		printk("DEBUG: load_balancing not needed\n");
		return -1;
	}
	
	max_wrr_rq = &cpu_rq(max_cpu)->wrr;
	min_wrr_rq = &cpu_rq(max_cpu)->wrr;

	if(max_wrr_rq->wrr_nr_running < 2){
		printk("DEBUG: 0 or 1 entries in max_wrr_rq\n");
		return -1;
	}

	/* hold lock for both max and min cpus at the same time
	 * before entering critical section */
	local_irq_save(flags);
	double_rq_lock(cpu_rq(max_cpu), cpu_rq(min_cpu));

	printk("DEBUG: load_balance successfully lock max:%d min:%d queue\n", max_cpu, min_cpu);

	to_move = NULL;

	list_for_each_entry(pos, &max_wrr_rq->queue_head, queue_node){
		if(pos->weight <= movable_weight && pos->weight > move_weight && wrr_task_of(pos) != max_wrr_rq->rq->curr && cpumask_test_cpu(min_cpu, tsk_cpus_allowed(wrr_task_of(pos)))) {
			to_move = pos;
			move_weight = pos->weight;
		}
	}

	if (to_move) {
		deactivate_task(max_wrr_rq->rq, wrr_task_of(to_move), 0);
		set_task_cpu(wrr_task_of(to_move), min_cpu);
		activate_task(min_wrr_rq->rq, wrr_task_of(to_move), 0);
		printk("DEBUG: load_balance successfully move task %d from:%d to:%d queue\n", wrr_task_of(to_move)->pid, max_cpu, min_cpu);

	}

	/* release lock for cpu max_cpu, min_cpu */
	double_rq_unlock(cpu_rq(max_cpu), cpu_rq(min_cpu));
	printk("DEBUG: load_balance successfully unlock max:%d min:%d queue\n", max_cpu, min_cpu);
	local_irq_restore(flags);

	return 0;
}

/*
 * Scheduling class
 */
const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	.yield_to_task		= yield_to_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
	.migrate_task_rq	= migrate_task_rq_wrr,

	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,
	.task_woken		= task_woken_wrr,
	.task_waking		= task_waking_wrr,

	.set_cpus_allowed	= set_cpus_allowed_wrr,
	.rq_online		= rq_online_wrr,
	.rq_offline		= rq_offline_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.task_fork		= task_fork_wrr,

	.switched_from		= switched_from_wrr,
	.switched_to		= switched_to_wrr,
	.prio_changed		= prio_changed_wrr,

	.get_rr_interval	= get_rr_interval_wrr
};

#ifdef CONFIG_SCHED_DEBUG
extern void print_wrr_rq(struct seq_file *m, int cpu, struct wrr_rq *wrr_rq);

void print_wrr_stats(struct seq_file *m, int cpu)
{
	struct rq *rq;

	rq = cpu_rq(cpu);
	print_wrr_rq(m, cpu, &rq->wrr);
}

#endif /* CONFIG_SCHED_DEBUG */
