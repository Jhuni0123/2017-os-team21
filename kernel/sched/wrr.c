
const struct sched_class wrr_sched_class;

/*
 *********************************
 * Scheduler interface functions *
 *********************************
 */

void enqueue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{ printk("DEBUG: enqueue\n"); }

void dequeue_task_wrr (struct rq *rq, struct task_struct *p, int flags)
{ printk("DEBUG: dequeue\n"); }

void yield_task_wrr (struct rq *rq)
{ printk("DEBUG: yield_task\n"); }

// not in rt
bool yield_to_task_wrr (struct rq *rq, struct task_struct *p, bool preempt)
{ printk("DEBUG: yield_to_task\n"); }

void check_preempt_curr_wrr (struct rq *rq, struct task_struct *p, int flags)
{ printk("DEBUG: check_preempt_curr\n"); }

struct task_struct * pick_next_task_wrr (struct rq *rq)
{ printk("DEBUG: pick_next_task\n"); return NULL; }

void put_prev_task_wrr (struct rq *rq, struct task_struct *p)
{ printk("DEBUG: put_prev_task\n"); }

#ifdef CONFIG_SMP
int  select_task_rq_wrr (struct task_struct *p, int sd_flag, int flags)
{ printk("DEBUG: select_task_rq\n"); return 0; }

// not in rt
void migrate_task_rq_wrr (struct task_struct *p, int next_cpu)
{ printk("DEBUG: migrate_task_rq\n"); }

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
{ printk("DEBUG: set_cpus_allowed\n"); }

void rq_online_wrr (struct rq *rq)
{ printk("DEBUG: rq_online\n"); }

void rq_offline_wrr (struct rq *rq)
{ printk("DEBUG: rq_offline\n"); }
#endif

void set_curr_task_wrr (struct rq *rq)
{ printk("DEBUG: set_curr_task_wrr\n"); }

void task_tick_wrr (struct rq *rq, struct task_struct *p, int queued)
{ printk("DEBUG: task_tick\n"); }

// not in rt
void task_fork_wrr (struct task_struct *p)
{ printk("DEBUG: task_fork\n"); }

void switched_from_wrr (struct rq *this_rq, struct task_struct *task)
{ printk("DEBUG: switched_from\n"); }

void switched_to_wrr (struct rq *this_rq, struct task_struct *task)
{ printk("DEBUG: switched_to\n"); }

void prio_changed_wrr (struct rq *this_rq, struct task_struct *task, int oldprio)
{ printk("DEBUG: prio_changed\n"); }

unsigned int get_rr_interval_wrr (struct rq *rq, struct task_struct *task)
{ printk("DEBUG: get_rr_interval\n"); }


/*
 * Scheduling class
 */
const struct sched_class wrr_sched_class = {
	.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	.yield_to_task		= yield_to_task_wrr,

	.check_preempt_curr	= check_preempt_wakeup,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
#ifdef CONFIG_FAIR_GROUP_SCHED
	.migrate_task_rq	= migrate_task_rq_wrr,
#endif
	.rq_online		= rq_online_wrr,
	.rq_offline		= rq_offline_wrr,

	.task_waking		= task_waking_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.task_fork		= task_fork_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_from		= switched_from_wrr,
	.switched_to		= switched_to_wrr,

	.get_rr_interval	= get_rr_interval_wrr,
	
	.pre_schedule	= pre_schedule_wrr,
	.post_schedule	= post_schedule_wrr,
	.task_woken		= task_woken_wrr,
	.set_cpus_allowed	= set_cpus_allowed_wrr
};

