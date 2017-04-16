#include <linux/rotation.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>

#define INITIAL_ROT 0

int device_rot = INITIAL_ROT;

DEFINE_MUTEX(rotlock_mutex);

struct range_desc waiting_reads = RANGE_DESC_INIT(waiting_reads);
struct range_desc waiting_writes = RANGE_DESC_INIT(waiting_writes);
struct range_desc assigned_reads = RANGE_DESC_INIT(assigned_reads);
struct range_desc assigned_writes = RANGE_DESC_INIT(assigned_writes);

/* common functions used by syscalls */
int remove_rotlocks_by_tid(struct range_desc *list, int tid);
void assign_rotlock(struct range_desc *lock, struct range_desc *assigned_list);
int find_assign_rotlock(void);

/* should use inside mutex lock */
int find_assign_rotlock(void)
{
	struct range_desc *pos, *tmp;
	int read_left, read_right;
	int write_left, write_right;
	bool write_assignable = true;

	int lo, hi;
	int count = 0;

	/* calculate range of assigned write lock */
	write_left = device_rot - 360;
	write_right = device_rot + 360;

	list_for_each_entry(pos, &assigned_writes.node, node){
		if(rot_in_range(pos)) {
			return count;
		}
		lo = pos->degree - pos->range;
		hi = pos->degree + pos->range;
		if(lo < device_rot)
			lo += 360;
		if(hi > device_rot)
			hi -= 360;
		write_left = max(write_left, hi);
		write_right = min(write_right, lo);
	}

	/* calculate range of assigned read lock */
	read_left = device_rot - 360;
	read_right = device_rot + 360;

	list_for_each_entry(pos, &assigned_reads.node, node){
		if(rot_in_range(pos)){
			write_assignable = false;
		}
		lo = pos->degree - pos->range;
		hi = pos->degree + pos->range;
		if(lo < device_rot)
			lo += 360;
		if(hi > device_rot)
			hi -= 360;
		read_left = max(read_left, hi);
		read_right = min(read_right, lo);
	}

	/* assign write lock */
	list_for_each_entry(pos, &waiting_writes.node, node) {
		if(rot_in_range(pos)) {
			if(!write_assignable) {
				return count;
			}
			lo = pos->degree - pos->range;
			hi = pos->degree + pos->range;
			if(lo > device_rot)
				lo -= 360;
			if(hi < device_rot)
				hi += 360;
			if(read_left < lo && hi < read_right && write_left < lo && hi < write_right) { /* assignable */
				assign_rotlock(pos, &assigned_writes);
				count++;
				return count;
			}
		}
	}

	/* assign read lock */
	list_for_each_entry_safe(pos, tmp, &waiting_reads.node, node) {
		if(rot_in_range(pos)) {
			lo = pos->degree - pos->range;
			hi = pos->degree + pos->range;
			if(lo > device_rot)
				lo -= 360;
			if(hi < device_rot)
				hi += 360;
			if(write_left < lo && hi < write_right) {
				assign_rotlock(pos, &assigned_reads);
				count++;
			}
		}
	}

	return count;
}

/* should use inside mutex lock */
void assign_rotlock(struct range_desc *lock, struct range_desc *assigned_list)
{
	list_del(&lock->node);
	list_add_tail(&lock->node, &assigned_list->node);
	lock->assigned = true;
	if(lock->task != NULL)
		wake_up_process(lock->task);
}

int do_rotlock(int degree, int range, struct range_desc *head)
{
	struct range_desc *newitem;

	if(!degree_valid(degree) || !range_valid(range))
		return -EINVAL;

	newitem = (struct range_desc*) kmalloc(sizeof(struct range_desc), GFP_KERNEL);

	if(newitem == NULL)
		return -ENOMEM;

	newitem->degree = degree;
	newitem->range = range;
	newitem->tid = task_pid_vnr(current);
	newitem->task = current;
	newitem->assigned = false;
	INIT_LIST_HEAD(&newitem->node);

	mutex_lock(&rotlock_mutex);
	list_add_tail(&newitem->node, &head->node);

	if(rot_in_range(newitem))
		find_assign_rotlock();

	mutex_unlock(&rotlock_mutex);

	/* this sleep implementation prevents concurrency issues
	 * see: http://www.linuxjournal.com/article/8144 */
	set_current_state(TASK_INTERRUPTIBLE);
	while(!newitem->assigned) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);

	current->rotlock_count++;
	return 0;
}

int do_rotunlock(int degree, int range, struct range_desc* head)
{
	pid_t tid;
	struct range_desc *curr;

	if(!degree_valid(degree) || !range_valid(range))
		return -EINVAL;

	/* find that exact lock */
	tid = task_pid_vnr(current);

	mutex_lock(&rotlock_mutex);

	list_for_each_entry(curr, &head->node, node) {
		if(curr->tid == tid && curr->degree == degree && curr->range == range)
			break;
	}

	if(curr == head) {	/* this is true only if iteration is finished */
		mutex_unlock(&rotlock_mutex);
		return -EINVAL;
	}

	list_del(&curr->node);
	kfree(curr);

	find_assign_rotlock();

	mutex_unlock(&rotlock_mutex);

	current->rotlock_count--;
	return 0;
}

int do_set_rotation(int degree)
{
	int result;

	if(!degree_valid(degree))
		return -EINVAL;

	mutex_lock(&rotlock_mutex);
	device_rot = degree;
	result = find_assign_rotlock();
	printk("DEBUG: SET ROTATION %d\n",device_rot);
	mutex_unlock(&rotlock_mutex);

	return result;
}

void exit_rotlock(void)
{
	pid_t tid;
	int removed = 0;

	if(current->rotlock_count == 0)
		return;

	tid  = task_pid_vnr(current);

	mutex_lock(&rotlock_mutex);

	removed += remove_rotlocks_by_tid(&waiting_reads, tid);
	removed += remove_rotlocks_by_tid(&waiting_writes, tid);
	removed += remove_rotlocks_by_tid(&assigned_reads, tid);
	removed += remove_rotlocks_by_tid(&assigned_writes, tid);

	if(removed)
		find_assign_rotlock();

	mutex_unlock(&rotlock_mutex);
}

/* should use inside mutex lock */
int remove_rotlocks_by_tid(struct range_desc *list, int tid)
{
	struct range_desc *pos, *tmp;
	int count = 0;

	list_for_each_entry_safe(pos, tmp, &list->node, node){
		if(pos->tid == tid) {
			pos->task = NULL;
			list_del(&pos->node);
			kfree(pos);
			count++;
		}
	}
	return count;
}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
	return do_set_rotation(degree);
}

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
	return do_rotlock(degree, range, &waiting_reads);
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
	return do_rotlock(degree, range, &waiting_writes);
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
	return do_rotunlock(degree, range, &assigned_reads);
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
	return do_rotunlock(degree, range, &assigned_writes);
}
