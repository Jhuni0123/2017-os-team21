#include <linux/rotation.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/syscalls.h>

#define INITIAL_ROT 0

int device_rot = INITIAL_ROT;
int range_desc_init = 0;

struct range_desc waiting_reads;
struct range_desc waiting_writes;
struct range_desc assigned_reads;
struct range_desc assigned_writes;

void init_range_desc(struct range_desc *head){
	head->degree = -1;
	head->range = -1;
	head->tid = -1;
	head->task = NULL;
	head->type = -1;
	head->assigned = -1;
	INIT_LIST_HEAD(&head->node);
}

void init_range_lists(){
	init_range_desc(&waiting_reads);
	init_range_desc(&waiting_writes);
 	init_range_desc(&assigned_reads);
	init_range_desc(&assigned_writes);
}

/* common functions used by syscalls */
static inline int check_param(int degree, int range);

int A() /* always use with lock x */
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
	list_for_each_entry(pos, &waiting_reads.node, node){
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

int do_rotlock(int degree, int range, char lock_flag, struct range_desc *head)
{
	if(!check_param(degree, range))
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
	newitem->type = lock_flag;
	newitem->assigned = 0;
	INIT_LIST_HEAD(&newitem->node);

	// TODO: grab lock x 
	list_add_tail(&newitem->node, &head->node);	
	if(range_in_rotation(newitem))
		A();

	/* this sleep implementation prevents concurrency issues
	 * see: http://www.linuxjournal.com/article/8144 */
	// TODO: release lock x
	set_current_state(TASK_INTERRUPTIBLE);
	while(!newitem->assigned) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);

	return 0;
	
}

int do_rotunlock(int degree, int range, char lock_flag, struct range_desc* head)
{
	if(!check_param(degree, range))
		return -EINVAL;
	
	/* find that exact lock */
	pid_t tid = task_pid_vnr(current);
	struct range_desc *curr;
	list_for_each_entry(curr, &head->node, node) {
		if((curr->degree == degree)&&(curr->range == range)&&(curr->tid == tid))
			break;
	}
	if(curr == head) {	/* this is true only if iteration is finished */
		printk("DEBUG: error: no match for rotunlock_read\n");
		return -EINVAL;
	}
	// todo: error check
	
	/* delete it from list.
	 * If device lock is in curr's range, call A */
	// todo: set lock x 
	list_del(&curr->node);
	kfree(curr);
	if(range_in_rotation(curr))
		A();	
	// todo: unset lock x

	return 0;

}

int do_set_rotation(int degree)
{	
	if(degree < 0 || degree >= 360)
		return -EINVAL;
	/* update device rotation info */
	device_rot = degree;
	/* call A */
	// todo: set lock x
	int result = A();
	// todo: unset lock x
	return result;
}


SYSCALL_DEFINE1(set_rotation, int, degree)
{
	return do_set_rotation(degree);
}

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
	return do_rotlock(degree, range, READ_LOCK_FLAG, &waiting_reads);
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
	return do_rotlock(degree, range, WRITE_LOCK_FLAG, &waiting_writes);
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
	return do_rotunlock(degree, range, READ_LOCK_FLAG, &assigned_reads);
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
	return do_rotunlock(degree, range, WRITE_LOCK_FLAG, &assigned_writes);
}

/* implementation of common functions */
static inline int check_param(int degree, int range)
{
	/* check if degree and range is correct */
	if(degree < 0 || degree >= 360){
		printk("DEBUG: error: degree out of range\n");
		return 0;
	}
	if(range <= 0 || range >= 180){
		printk("DEBUG: error: range out of range\n");
		return 0;
	}
	return 1;
}

