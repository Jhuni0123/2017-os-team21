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
static inline bool range_valid(int range);
static inline bool degree_valid(int degree);
inline bool rot_in_range(int rot, int degree, int range);
void rotlock_assign(struct range_desc *lock, struct range_desc *assigned_list);

static void print_range_desc_list(struct range_desc *list){
	struct range_desc *pos;
	printk("DEBUG: head");
	list_for_each_entry(pos, &list->node, node) {
		printk("->(%s,%d,%d)", pos->type == READ_FLAG ? "READ" : "WRITE", pos->degree, pos->range);
	}
	printk("\n");
}

int reassign_rotlock() /* always use with lock x */
{
	struct range_desc *pos, *tmp;
	int read_left, read_right;
	int write_left, write_right;
	bool write_assignable = true;

	/* calculate range of assigned write lock */
	write_left = device_rot - 360;
	write_right = device_rot + 360;

	list_for_each_entry(pos, &assigned_writes.node, node){
		if(rot_in_range(device_rot, pos->degree, pos->range)) {
			return 0;
		}
		int lo = pos->degree - pos->range;
		int hi = pos->degree + pos->range;
		if(lo < device_rot) lo += 360;
		if(hi > device_rot) hi -= 360;
		write_left = max(write_left, hi);
		write_right = min(write_right, lo);
	}

	/* calculate range of assigned read lock */
	read_left = device_rot - 360;
	read_right = device_rot + 360;

	list_for_each_entry(pos, &assigned_reads.node, node){
		if(rot_in_range(device_rot, pos->degree, pos->range)){
			write_assignable = false;
		}
		int lo = pos->degree - pos->range;
		int hi = pos->degree + pos->range;
		if(lo < device_rot) lo += 360;
		if(hi > device_rot) hi -= 360;
		read_left = max(read_left, hi);
		read_right = min(read_right, lo);
	}

	/* assign write lock */
	if(write_assignable) {
		list_for_each_entry_safe(pos, tmp, &waiting_writes.node, node) {
			if(rot_in_range(device_rot, pos->degree, pos->range)) {
				int lo = pos->degree - pos->range;
				int hi = pos->degree + pos->range;
				if(lo > device_rot)
					lo -= 360;
				if(hi < device_rot)
					hi += 360;
				if(read_left < lo && hi < read_right && write_left < lo && hi < write_right) { /* assignable */
					rotlock_assign(pos, &assigned_writes);
					return 1;
				}
				return 0;
			}
		}
	}

	/* assign read lock */
	int count = 0;

	list_for_each_entry_safe(pos, tmp, &waiting_reads.node, node) {
		if(rot_in_range(device_rot, pos->degree, pos->range)) {
			int lo = pos->degree - pos->range;
			int hi = pos->degree + pos->range;
			if(lo > device_rot)
				lo -= 360;
			if(hi < device_rot)
				hi += 360;
			if(write_left < lo && hi < write_right) {
				rotlock_assign(pos, &assigned_reads);
				count++;
			}
		}
	}

	return count;
}

int do_rotlock(int degree, int range, enum rw_flag flag, struct range_desc *head)
{
	if(!degree_valid(degree) || !range_valid(range))
		return -EINVAL;

	struct range_desc* newitem =
		(struct range_desc*) kmalloc(sizeof(struct range_desc), GFP_KERNEL);

	if(newitem == NULL) {
		printk("DEBUG: kernel has no memory\n");
		return -ENOMEM;
	}

	newitem->degree = degree;
	newitem->range = range;
	newitem->tid = task_pid_vnr(current);
	newitem->task = current;
	newitem->type = flag;
	newitem->assigned = 0;
	INIT_LIST_HEAD(&newitem->node);

	mutex_lock(&rotlock_mutex);

	list_add_tail(&newitem->node, &head->node);

	if(range_in_rotation(newitem))
		reassign_rotlock();

	mutex_unlock(&rotlock_mutex);

	/* this sleep implementation prevents concurrency issues
	 * see: http://www.linuxjournal.com/article/8144 */
	set_current_state(TASK_INTERRUPTIBLE);
	while(!newitem->assigned) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);

	return 0;

}

int do_rotunlock(int degree, int range, enum rw_flag flag, struct range_desc* head)
{
	if(!degree_valid(degree) || !range_valid(range))
		return -EINVAL;

	/* find that exact lock */
	pid_t tid = task_pid_vnr(current);
	struct range_desc *curr;

	mutex_lock(&rotlock_mutex);

	list_for_each_entry(curr, &head->node, node) {
		if(curr->tid == tid && curr->degree == degree && curr->range == range)
			break;
	}

	if(curr == head) {	/* this is true only if iteration is finished */
		printk("DEBUG: error: no match for rotunlock_read\n");
		mutex_unlock(&rotlock_mutex);
		return -EINVAL;
	}

	/* delete it from list. */
	list_del(&curr->node);
	kfree(curr);

	reassign_rotlock();

	mutex_unlock(&rotlock_mutex);

	return 0;

}

int do_set_rotation(int degree)
{
	int result;

	if(!degree_valid(degree))
		return -EINVAL;

	mutex_lock(&rotlock_mutex);

	device_rot = degree;

	result = reassign_rotlock();

	mutex_unlock(&rotlock_mutex);

	return result;
}


SYSCALL_DEFINE1(set_rotation, int, degree)
{
	return do_set_rotation(degree);
}

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
	return do_rotlock(degree, range, READ_FLAG, &waiting_reads);
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
	return do_rotlock(degree, range, WRITE_FLAG, &waiting_writes);
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
	return do_rotunlock(degree, range, READ_FLAG, &assigned_reads);
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
	return do_rotunlock(degree, range, WRITE_FLAG, &assigned_writes);
}

static inline bool range_valid(int range)
{
	return 0 < range && range < 180;
}

static inline bool degree_valid(int degree)
{
	return 0 <= degree && degree < 360;
}

void exit_rotlock(){
	pid_t tid = task_pid_vnr(current);
	struct range_desc *pos, *tmp;

	mutex_lock(&rotlock_mutex);

	list_for_each_entry_safe(pos, tmp, &waiting_reads.node, node){
		if(pos->tid == tid){
			list_del(&pos->node);
			kfree(pos);
		}
	}
	list_for_each_entry_safe(pos, tmp, &waiting_writes.node, node){
		if(pos->tid == tid){
			list_del(&pos->node);
			kfree(pos);
		}
	}
	list_for_each_entry_safe(pos, tmp, &assigned_reads.node, node){
		if(pos->tid == tid){
			list_del(&pos->node);
			kfree(pos);
		}
	}
	list_for_each_entry_safe(pos, tmp, &assigned_reads.node, node){
		if(pos->tid == tid){
			list_del(&pos->node);
			kfree(pos);
		}
	}

	mutex_unlock(&rotlock_mutex);
}

inline bool rot_in_range(int rot, int degree, int range)
{
	int dist = abs(rot - degree);
	return (dist < 360 - dist ? dist : 360 - dist) <= range;
}

void rotlock_assign(struct range_desc *lock, struct range_desc *assigned_list)
{
	list_del(&lock->node);
	list_add_tail(&lock->node, &assigned_list->node);
	lock->assigned = 1;
	if(lock->task != NULL)
		wake_up_process(lock->task);
}
