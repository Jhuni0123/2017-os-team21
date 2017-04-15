#include <linux/rotation.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>

#define INITIAL_ROT 0

int device_rot = INITIAL_ROT;

DEFINE_MUTEX(waiting_reads_mutex);
DEFINE_MUTEX(waiting_writes_mutex);
DEFINE_MUTEX(assigned_reads_mutex);
DEFINE_MUTEX(assigned_writes_mutex);

struct range_desc waiting_reads = RANGE_DESC_INIT(waiting_reads);
struct range_desc waiting_writes = RANGE_DESC_INIT(waiting_writes);
struct range_desc assigned_reads = RANGE_DESC_INIT(assigned_reads);
struct range_desc assigned_writes = RANGE_DESC_INIT(assigned_writes);

/* common functions used by syscalls */
void assign_rotlock(struct range_desc *lock, struct range_desc *assigned_list);
int find_assign_rotlock(void);

static void print_range_desc_list(struct range_desc *list){
	struct range_desc *pos;
	printk("head");
	list_for_each_entry(pos, &list->node, node) {
		printk("->(%d,%d)", pos->degree, pos->range);
	}
	printk("\n");
}

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

	mutex_lock(&assigned_writes_mutex);
	list_for_each_entry(pos, &assigned_writes.node, node){
		if(rot_in_range(pos)) {
			mutex_unlock(&assigned_writes_mutex);
			return count;
		}
		lo = pos->degree - pos->range;
		hi = pos->degree + pos->range;
		if(lo < device_rot) lo += 360;
		if(hi > device_rot) hi -= 360;
		write_left = max(write_left, hi);
		write_right = min(write_right, lo);
	}
	mutex_unlock(&assigned_writes_mutex);

	/* calculate range of assigned read lock */
	read_left = device_rot - 360;
	read_right = device_rot + 360;

	mutex_lock(&assigned_reads_mutex);
	list_for_each_entry(pos, &assigned_reads.node, node){
		if(rot_in_range(pos)){
			write_assignable = false;
		}
		lo = pos->degree - pos->range;
		hi = pos->degree + pos->range;
		if(lo < device_rot) lo += 360;
		if(hi > device_rot) hi -= 360;
		read_left = max(read_left, hi);
		read_right = min(read_right, lo);
	}
	mutex_unlock(&assigned_reads_mutex);

	/* assign write lock */
	if(write_assignable) {
		mutex_lock(&waiting_writes_mutex);
		list_for_each_entry(pos, &waiting_writes.node, node) {
			if(rot_in_range(pos)) {
				lo = pos->degree - pos->range;
				hi = pos->degree + pos->range;
				if(lo > device_rot)
					lo -= 360;
				if(hi < device_rot)
					hi += 360;
				if(read_left < lo && hi < read_right && write_left < lo && hi < write_right) { /* assignable */
					mutex_lock(&assigned_writes_mutex);
					assign_rotlock(pos, &assigned_writes);
					mutex_unlock(&assigned_writes_mutex);
					mutex_unlock(&waiting_writes_mutex);
					count++;
					return count;
				}
			}
		}
		mutex_unlock(&waiting_writes_mutex);
	}

	/* assign read lock */
	count = 0;

	mutex_lock(&waiting_reads_mutex);
	list_for_each_entry_safe(pos, tmp, &waiting_reads.node, node) {
		if(rot_in_range(pos)) {
			lo = pos->degree - pos->range;
			hi = pos->degree + pos->range;
			if(lo > device_rot)
				lo -= 360;
			if(hi < device_rot)
				hi += 360;
			if(write_left < lo && hi < write_right) {
				mutex_lock(&assigned_reads_mutex);
				assign_rotlock(pos, &assigned_reads);
				mutex_unlock(&assigned_reads_mutex);
				count++;
			}
		}
	}
	mutex_unlock(&waiting_reads_mutex);

	return count;
}

void assign_rotlock(struct range_desc *lock, struct range_desc *assigned_list)
{
	list_del(&lock->node);
	list_add_tail(&lock->node, &assigned_list->node);
	lock->assigned = true;
	if(lock->task != NULL)
		wake_up_process(lock->task);
}

int do_rotlock(int degree, int range, struct range_desc *head, struct mutex *mutex)
{
	struct range_desc *newitem;

	if(!degree_valid(degree) || !range_valid(range))
		return -EINVAL;

	newitem = (struct range_desc*) kmalloc(sizeof(struct range_desc), GFP_KERNEL);

	if(newitem == NULL) {
		printk("DEBUG: kernel has no memory\n");
		return -ENOMEM;
	}

	newitem->degree = degree;
	newitem->range = range;
	newitem->tid = task_pid_vnr(current);
	newitem->task = current;
	newitem->assigned = false;
	INIT_LIST_HEAD(&newitem->node);

	mutex_lock(mutex);
	list_add_tail(&newitem->node, &head->node);
	mutex_unlock(mutex);

	if(rot_in_range(newitem))
		find_assign_rotlock();

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

int do_rotunlock(int degree, int range, struct range_desc* head, struct mutex* mutex)
{
	pid_t tid;
	struct range_desc *curr;

	if(!degree_valid(degree) || !range_valid(range))
		return -EINVAL;

	/* find that exact lock */
	tid = task_pid_vnr(current);

	mutex_lock(mutex);
	list_for_each_entry(curr, &head->node, node) {
		if(curr->tid == tid && curr->degree == degree && curr->range == range)
			break;
	}

	if(curr == head) {	/* this is true only if iteration is finished */
		printk("DEBUG: error: no match for rotunlock_read\n");
		mutex_unlock(mutex);
		return -EINVAL;
	}

	list_del(&curr->node);
	kfree(curr);
	mutex_unlock(mutex);

	find_assign_rotlock();

	current->rotlock_count--;
	return 0;
}

int do_set_rotation(int degree)
{
	int result;

	if(!degree_valid(degree))
		return -EINVAL;

	device_rot = degree;
	result = find_assign_rotlock();
	printk("DEBUG: SET ROTATION %d\n", device_rot);
	printk("DEBUG: WAIT     READ : ");
	print_range_desc_list(&waiting_reads);
	printk("DEBUG: WAIT     WRITE: ");
	print_range_desc_list(&waiting_writes);
	printk("DEBUG: ASSIGNED READ : ");
	print_range_desc_list(&assigned_reads);
	printk("DEBUG: ASSIGNED WRITE: ");
	print_range_desc_list(&assigned_writes);

	return result;
}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
	return do_set_rotation(degree);
}

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
	return do_rotlock(degree, range, &waiting_reads, &waiting_reads_mutex);
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
	return do_rotlock(degree, range, &waiting_writes, &waiting_writes_mutex);
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
	return do_rotunlock(degree, range, &assigned_reads, &assigned_reads_mutex);
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
	return do_rotunlock(degree, range, &assigned_writes, &assigned_writes_mutex);
}

void exit_rotlock(){
	if(current->rotlock_count == 0)
		return;
	
	pid_t tid = task_pid_vnr(current);
	struct range_desc *pos, *tmp;
	
	/* make sure that lock order doesn't make circular dependency with find_assign_rotlock() */
	mutex_lock(&assigned_writes_mutex);
	list_for_each_entry_safe(pos, tmp, &assigned_writes.node, node){
		if(pos->tid == tid){
			printk("DEBUG: PID %d's LOCK REMOVED\n", tid);
			list_del(&pos->node);
			kfree(pos);
		}
	}
	mutex_unlock(&assigned_writes_mutex);
	mutex_lock(&assigned_reads_mutex);
	list_for_each_entry_safe(pos, tmp, &assigned_reads.node, node){
		if(pos->tid == tid){
			printk("DEBUG: PID %d's LOCK REMOVED\n", tid);
			list_del(&pos->node);
			kfree(pos);
		}
	}
	mutex_unlock(&assigned_reads_mutex);
	mutex_lock(&waiting_writes_mutex);
	list_for_each_entry_safe(pos, tmp, &waiting_writes.node, node){
		if(pos->tid == tid){
			printk("DEBUG: PID %d's LOCK REMOVED\n", tid);
			list_del(&pos->node);
			kfree(pos);
		}
	}
	mutex_unlock(&waiting_writes_mutex);
	mutex_lock(&waiting_reads_mutex);
	list_for_each_entry_safe(pos, tmp, &waiting_reads.node, node){
		if(pos->tid == tid){
			printk("DEBUG: PID %d's LOCK REMOVED\n", tid);
			list_del(&pos->node);
			kfree(pos);
		}
	}
	mutex_unlock(&waiting_reads_mutex);

	return;
}

