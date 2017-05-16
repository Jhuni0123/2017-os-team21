#include "sched.h"
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sched/wrr.h>

#define for_each_sched_wrr_entity(pos, wrr_rq)	\
	list_for_each_entry(pos, &(wrr_rq)->queue_head, queue_node)

const struct sched_class wrr_sched_class;
static int load_balance(void);

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *rq)
{
	wrr_rq->wrr_nr_running = 0;
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

static inline struct sched_wrr_entity *__pick_next_entity(struct wrr_rq *wrr_rq)
{
	return list_first_entry_or_null(&wrr_rq->queue_head, struct sched_wrr_entity, queue_node);
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

#ifdef CONFIG_WRR_AGING
static void check_aging(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se)
{
	int aging;

	if(wrr_se->weight > 19)
		return;
	
	aging = ++(wrr_se->aging_time_slice);
	if(aging > WRR_AGING_TIME){
		wrr_se->weight++;
		wrr_se->aging_weight++;
		wrr_rq->weight_sum++;
		wrr_se->aging_time_slice = 0;
	}
}
#endif

static void check_load_balance_wrr(struct wrr_rq *wrr_rq)
{
	u64 now = wrr_rq->rq->clock_task;
	
	if(wrr_rq->next_balancing == 0) {
		wrr_rq->next_balancing = now + WRR_BALANCE_PERIOD;
		return;
	}

	if(now > wrr_rq->next_balancing) {
		printk("DEBUG: now : %lld, next_balancing : %lld\n", now, wrr_rq->next_balancing);
		load_balance();
		/* unit of now, next_balancing is nsec */
		wrr_rq->next_balancing = now + WRR_BALANCE_PERIOD * 1000000000LL;
	}
}

void trigger_load_balance_wrr(struct rq *rq, int cpu)
{
	/* only the first cpu call load balance */
	unsigned int first_cpu = cpumask_first(cpu_online_mask);
	if(cpu == first_cpu)
		check_load_balance_wrr(&rq->wrr);
}

void inline move_task_wrr(struct rq *src_rq, struct rq *dst_rq, struct task_struct *p)
{
	deactivate_task(src_rq, p, 0);
	set_task_cpu(p, cpu_of(dst_rq));
	activate_task(dst_rq, p, 0);
}

bool inline can_move_task_wrr(struct rq *src_rq, struct rq *dst_rq, struct task_struct *p)
{
	return p != src_rq->curr && cpumask_test_cpu(cpu_of(dst_rq), tsk_cpus_allowed(p));
}
/*
 *********************************
 * Scheduler interface functions *
 *********************************
 */

void enqueue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if(wrr_se->on_wrr_rq)
		return;

#ifdef CONFIG_WRR_AGING
	wrr_se->aging_weight = 0;
	wrr_se->aging_time_slice = 0;
#endif
	__enqueue_wrr_entity(wrr_rq, wrr_se);

	inc_nr_running(rq);
	wrr_rq->wrr_nr_running++;
	wrr_rq->weight_sum += wrr_se->weight;
	wrr_se->on_wrr_rq = 1; 
}

void dequeue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if(!(wrr_se->on_wrr_rq))
		return;
	
#ifdef CONFIG_WRR_AGING
	if(wrr_se->aging_weight) {
		int next_weight = wrr_se->weight - wrr_se->aging_weight;
		if(next_weight <= 0)
			next_weight = 1;
		wrr_rq->weight_sum -= wrr_se->weight - next_weight;
		wrr_se->weight = next_weight;
		wrr_se->aging_weight = 0;
	}
#endif
	__dequeue_wrr_entity(wrr_se);

	dec_nr_running(rq);
	wrr_rq->wrr_nr_running--;
	wrr_rq->weight_sum -= wrr_se->weight;
	wrr_se->on_wrr_rq = 0;
}

void yield_task_wrr (struct rq *rq)
{
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct task_struct *curr = rq->curr;
	struct sched_wrr_entity *wrr_se = &curr->wrr;

	if(!(wrr_se->on_wrr_rq))
		return;

	__requeue_wrr_entity(wrr_se, wrr_rq);
}

bool yield_to_task_wrr (struct rq *rq, struct task_struct *p, bool preempt)
{
	return false;
}

void check_preempt_curr_wrr (struct rq *rq, struct task_struct *p, int flags)
{
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

	wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;

	return p;
}

void put_prev_task_wrr (struct rq *rq, struct task_struct *p)
{
}

#ifdef CONFIG_SMP
int  select_task_rq_wrr (struct task_struct *p, int sd_flag, int flags)
{
	int i;
	int min_weight_sum = 1000000000;
	int min_cpu = -1;

	/* find minimum weight_sum cpu */
	rcu_read_lock();
	for_each_possible_cpu(i){
		struct rq *rq = cpu_rq(i);
		struct wrr_rq *wrr_rq = &rq->wrr;
		if (!cpumask_test_cpu(i, tsk_cpus_allowed(p)))
			continue;
		if (min_weight_sum > wrr_rq->weight_sum){
			min_cpu = i;
			min_weight_sum = wrr_rq->weight_sum;
		}
	}
	rcu_read_unlock();
	return min_cpu;
}

void migrate_task_rq_wrr (struct task_struct *p, int next_cpu)
{
}

void pre_schedule_wrr (struct rq *this_rq, struct task_struct *task)
{
}

void post_schedule_wrr (struct rq *this_rq)
{
}

void task_waking_wrr (struct task_struct *task)
{
}

void task_woken_wrr (struct rq *this_rq, struct task_struct *task)
{
}

void set_cpus_allowed_wrr (struct task_struct *p, const struct cpumask *newmask)
{
}

void rq_online_wrr (struct rq *rq)
{
}

void rq_offline_wrr (struct rq *rq)
{
}
#endif

void set_curr_task_wrr (struct rq *rq)
{
}

void task_tick_wrr (struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	update_curr_wrr(rq);

#ifdef CONFIG_WRR_AGING
	check_aging(wrr_rq, wrr_se);
#endif

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
	p->wrr.on_wrr_rq = 0;
}

void switched_from_wrr (struct rq *this_rq, struct task_struct *task)
{
}

void switched_to_wrr (struct rq *this_rq, struct task_struct *task)
{
}

void prio_changed_wrr (struct rq *this_rq, struct task_struct *task, int oldprio)
{
}

unsigned int get_rr_interval_wrr (struct rq *rq, struct task_struct *task)
{
	return 0;
}

/* no runqueues are locked when call load_balance */
static int load_balance(void)
{
	int i;
	int max_cpu = 0, min_cpu = 0;
	int max_weight = -1, min_weight = 1000000000;
	unsigned long flags;
	struct rq *max_rq, *min_rq;
	struct sched_wrr_entity *pos, *to_move;
	int movable_weight, move_weight = 0;
	int moved = 0;

	rcu_read_lock();
	for_each_possible_cpu(i){
		struct rq *rq = cpu_rq(i);
		int weight_sum = rq->wrr.weight_sum;
		if(max_weight < weight_sum){
			max_cpu = i;
			max_weight = weight_sum;
		}
		if(min_weight > weight_sum){
			min_cpu = i;
			min_weight = weight_sum;
		}
	}
	rcu_read_unlock();

	if(max_weight == 0 || max_cpu == min_cpu){
		return 0;
	}
	
	movable_weight = (max_weight - min_weight - 1) / 2;
	max_rq = cpu_rq(max_cpu);
	min_rq = cpu_rq(min_cpu);

	if(max_rq->wrr.wrr_nr_running < 2){
		return 0;
	}

	/* hold lock for both max and min cpus at the same time
	 * before entering critical section */
	local_irq_save(flags);
	double_rq_lock(cpu_rq(max_cpu), cpu_rq(min_cpu));

	to_move = NULL;

	for_each_sched_wrr_entity(pos, &max_rq->wrr) {
		int weight = pos->weight;
		if (!can_move_task_wrr(max_rq, min_rq, wrr_task_of(pos)))
			continue;
		if(weight <= movable_weight && weight > move_weight) {
			to_move = pos;
			move_weight = weight;
		}
	}

	if (to_move) {
		move_task_wrr(max_rq, min_rq, wrr_task_of(to_move));
		moved++;
	}

	/* release lock for cpu max_cpu, min_cpu */
	double_rq_unlock(max_rq, min_rq);
	local_irq_restore(flags);

	return moved;
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
