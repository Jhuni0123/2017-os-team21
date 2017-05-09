#include "sched.h"
#include <linux/slab.h>
#include <linux/list.h>

const struct sched_class wrr_sched_class;

/*
 *********************************
 * Scheduler interface functions *
 *********************************
 */

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
	wrr_rq->wrr_nr_running = 0;
	wrr_rq->curr = NULL;
	INIT_LIST_HEAD(&wrr_rq->queue_head);
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
	return wrr_se->wrr_rq;
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

	if(list_empty(&wrr_rq->queue_head)){
		printk("DEBUG: wrr_rq empty\n");
		return NULL;
	}

	next = list_entry(wrr_rq->queue_head.next, struct sched_wrr_entity, queue_node);
	
	/* if there is only one entry in the queue, 
	 * just return it regardless of it running right now */
	if(wrr_rq->wrr_nr_running == 1)
		return next;

	/* if it's already running, choose the next one */
	if(wrr_task_of(next) == wrr_rq->rq->curr)
		next = list_entry(wrr_rq->queue_head.next->next, struct sched_wrr_entity, queue_node);

	return next;

}

void enqueue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{
	printk("DEBUG: %d: enqueue\n", p->pid);

	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;
	//todo: how to handle flags?

	if(wrr_se->on_wrr_rq){
		printk("DEBUG: already on wrr rq\n");
		return;
	}

	__enqueue_wrr_entity(wrr_rq, wrr_se);

	wrr_rq->wrr_nr_running++;
	wrr_se->on_wrr_rq = 1; 
}

void dequeue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{ 
	printk("DEBUG: %d: dequeue\n", p->pid); 

	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se = &p->wrr;
	//todo: how to handle flags?

	if(!(wrr_se->on_wrr_rq)){
		printk("DEBUG: %d: not on wrr rq\n", p->pid);
		return;
	}

	__dequeue_wrr_entity(wrr_se);

	wrr_rq->wrr_nr_running--;
	wrr_se->on_wrr_rq = 0;
}

void yield_task_wrr (struct rq *rq)
{ 
	printk("DEBUG: yield_task\n"); 

	struct wrr_rq *wrr_rq = &rq->wrr;
	struct task_struct *curr = rq->curr;
	struct sched_wrr_entity *wrr_se = &curr->wrr;

	if(!(wrr_se->on_wrr_rq)){
		printk("DEBUG: not on wrr rq\n");
		return;
	}

	__requeue_wrr_entity(wrr_se, wrr_rq);

}

// not in rt
bool yield_to_task_wrr (struct rq *rq, struct task_struct *p, bool preempt)
{ printk("DEBUG: %d: yield_to_task\n", p->pid); return false; }

void check_preempt_curr_wrr (struct rq *rq, struct task_struct *p, int flags)
{ printk("DEBUG: %d: check_preempt_curr\n", p->pid); }

struct task_struct *pick_next_task_wrr (struct rq *rq)
{
	printk("DEBUG: pick_next_task\n");

	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;
	struct wrr_rq *wrr_rq = &rq->wrr; 

	wrr_se = __pick_next_entity(wrr_rq);

	if(!wrr_se)
		return NULL;

	p = wrr_task_of(wrr_se);

	if(hrtick_enabled(rq))
		hrtick_start(rq, wrr_se->weight * 10);

	return p;
}

void put_prev_task_wrr (struct rq *rq, struct task_struct *p)
{ printk("DEBUG: %d: put_prev_task\n", p->pid); }

#ifdef CONFIG_SMP
int  select_task_rq_wrr (struct task_struct *p, int sd_flag, int flags)
{ printk("DEBUG: %d: select_task_rq\n", p->pid); return 0; }

// not in rt
void migrate_task_rq_wrr (struct task_struct *p, int next_cpu)
{ printk("DEBUG: %d: migrate_task_rq\n", p->pid); }

// not in fair
void pre_schedule_wrr (struct rq *this_rq, struct task_struct *task)
{ printk("DEBUG: pre_schedule\n"); }

// not in fair
void post_schedule_wrr (struct rq *this_rq)
{ printk("DEBUG: post_schedule\n"); }

// not in rt
void task_waking_wrr (struct task_struct *task)
{ printk("DEBUG: task_waking\n"); }

// not in fair
void task_woken_wrr (struct rq *this_rq, struct task_struct *task)
{ printk("DEBUG: task_woken\n"); }

// not in fair
void set_cpus_allowed_wrr (struct task_struct *p, const struct cpumask *newmask)
{ printk("DEBUG: %d: set_cpus_allowed\n", p->pid); }

void rq_online_wrr (struct rq *rq)
{ printk("DEBUG: rq_online\n"); }

void rq_offline_wrr (struct rq *rq)
{ printk("DEBUG: rq_offline\n"); }
#endif

void set_curr_task_wrr (struct rq *rq)
{ printk("DEBUG: set_curr_task_wrr\n"); }

void task_tick_wrr (struct rq *rq, struct task_struct *p, int queued)
{ printk("DEBUG: %d: task_tick\n", p->pid); }

// not in rt
void task_fork_wrr (struct task_struct *p)
{ printk("DEBUG: %d: task_fork\n", p->pid); }

void switched_from_wrr (struct rq *this_rq, struct task_struct *task)
{ printk("DEBUG: %d: twitched_from\n", task->pid); }

void switched_to_wrr (struct rq *this_rq, struct task_struct *task)
{ printk("DEBUG: %d: switched_to\n", task->pid); }

void prio_changed_wrr (struct rq *this_rq, struct task_struct *task, int oldprio)
{ printk("DEBUG: %d: prio_changed\n", task->pid); }

unsigned int get_rr_interval_wrr (struct rq *rq, struct task_struct *task)
{ printk("DEBUG: %d: get_rr_interval\n", task->pid); return 0; }


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

void print_wrr_stats(struct seq_file *m, int cpu)
{
	//rt_rq_iter_t iter;
	struct wrr_rq *wrr_rq;

	rcu_read_lock();
	//for_each_rt_rq(rt_rq, iter, cpu_rq(cpu))
		//print_rt_rq(m, cpu, rt_rq);
	rcu_read_unlock();
}

