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
	/* declare variable */
	int result = 0;
	/* check if there is a locked write lock
	 * if there is already a writer lock holding the rotation
	 * no more lock can be allocated: return 0 */
	struct range_desc *pos = NULL;
	list_for_each_entry(pos, &assigned_writes.node, node){
		if(range_in_rotation(pos))
			return 0;
	}
	/* check if there is a write lock waiting to be allocated.
	 * If there is, check if there is an allocated lock that overlaps with it.
	 * If there is no overlapping allocated lock, assign that waiting write lock. */
	list_for_each_entry(pos, &waiting_writes.node, node){
		if(range_in_rotation(pos)){
			int can_alloc_write = 1;
			struct range_desc *temp = NULL;
			list_for_each_entry(temp, &assigned_reads.node, node){
				if(range_overlap(pos, temp)){
					can_alloc_write = 0;
					break;
				}
			}
			list_for_each_entry(temp, &assigned_writes.node, node){
				if(range_overlap(pos, temp)){
					can_alloc_write = 0;
					break;
				}
			}
			if(can_alloc_write){
				list_del(&pos->node);
				list_add_tail(&pos->node, &assigned_writes.node);
				pos->assigned = 1;
				if(pos->task != NULL)
					wake_up_process(pos->task);
				return 1;
			} else {
				return 0;
			}
		}
	}

	/* If there is no waiting write lock,
	 * allocate all waiting read locks in correct range
	 * that do not overlap with assigned write locks */
	struct range_desc *temp = NULL;
	struct range_desc *safe_tmp;
	list_for_each_entry_safe(pos, safe_tmp, &waiting_reads.node, node){
		if(range_in_rotation(pos)){
			int is_overlap_w_write = 0;
			list_for_each_entry(temp, &assigned_writes.node, node){
				if(range_overlap(pos, temp)){
					is_overlap_w_write = 1;
					break;
				}
			}
			if(!is_overlap_w_write){
				list_del(&pos->node);
				list_add_tail(&pos->node, &assigned_reads.node);
				pos->assigned = 1;
				if(pos->task != NULL)
					wake_up_process(pos->task);
				result++;
			}
		}
	}
	return result;
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

