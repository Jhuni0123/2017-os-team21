#include <linux/rotation.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/syscalls.h>

#define INITIAL_ROT 0

int device_rot = INITIAL_ROT;
int range_desc_init = 0;

struct range_desc waiting_locks;
struct range_desc assigned_reads;
struct range_desc assigned_writes;

void init_range_desc(struct range_desc *head){
	head->degree = -1;
	head->range = -1;
	head->tid = -1;
	INIT_LIST_HEAD(&head->node);
}

void init_range_lists(){
	init_range_desc(&waiting_locks);
 	init_range_desc(&assigned_reads);
	init_range_desc(&assigned_writes);
}

int is_locked_write = 0;
int is_waiting_write = 0;
struct range_desc *w_write = NULL;

int A()
{
	/* initialize variables */
	is_locked_write = 0;
	is_waiting_write = 0;
	w_write = NULL;
	/* declare variables */
	int result = 0;
  	/* check if there is a locked write lock
	 * if there is already a writer lock holding the rotation
	 * no more lock can be allocated: return 0 */
	struct range_desc *pos = NULL;
	list_for_each_entry(pos, &assigned_writes.node, node){
		if(range_in_rotation(pos)){
    		is_locked_write = 1;
    		return 0;
    	}
  	}
	/* See all the waiting locks in FIFO order
	 * until confronting a writing lock request
	 * whose range matches current device rotation.
	 * If it's a read lock in correct range, assign it.
	 * If it's a write lock in correct range, assign it 
	 * if and only if it's the first lock of the list 
	 * which is in correct, and there is no read lock assigned.
	 * The second condition is to be checked later*/
	int first_entry = 1;
	int is_write_to_be_assigned = 0;
	list_for_each_entry(pos, &waiting_locks.node, node){
      	if(range_in_rotation(pos)){
			if(pos->type== READ_LOCK_FLAG){
				list_del(&pos->node);
				list_add_tail(&pos->node, &assigned_reads.node);
				// todo: make the task wake up
				result++;
				first_entry = 0;
			} else {
				is_waiting_write = 1;
				w_write = pos;
				if(first_entry){
					is_write_to_be_assigned = 1;
					first_entry = 0;
				}
				break;
			}
		}
   	}
	/* Sanity check: result > 0 and is_write_to_be_assigned == 1
	 * cannot happen at the same time */
	if((result>0)&&is_write_to_be_assigned){
		printk("DEBUG: error: result>0 and is_write_to_be_assigned happen at the same time.\n");
		return -1;
	}
	/* If there is a write lock request that might be assigned,
	 * check if there is already read locks assigned */
	int is_locked_read = 0;
	if(is_write_to_be_assigned){
		list_for_each_entry(pos, &assigned_reads.node, node){
			if(range_in_rotation(pos)){
				is_locked_read = 1;
				break;
			}
		}
		if(!is_locked_read){
			list_del(&w_write->node);
			list_add_tail(&w_write->node, &assigned_writes.node);
			is_locked_write = 1;
			is_waiting_write = 0;
			w_write = NULL;
			// todo: unblock process block
			return 1;
		}
	}
	return result;
}

int do_rotlock_read(int degree, int range)
{
	// todo: error checking

	/* make a new range_desc to hold lock info */
	struct range_desc* newitem =
		(struct range_desc*) kmalloc(sizeof(struct range_desc), GFP_KERNEL);
	if(newitem == NULL)
		return -ENOMEM;

	newitem->type = READ_LOCK_FLAG;
	newitem->degree = degree;
	newitem->range = range;
	newitem->tid = task_pid_vnr(current);
	INIT_LIST_HEAD(&newitem->node);

	/* If device lock is in newitem's range
	   and there is no locked write nor waiting write,
	   give lock */
	// todo: set lock x (reason: access vars is_locked_write, is_waiting_write)
	if(range_in_rotation(newitem)&&(!is_locked_write)&&(!is_waiting_write)){
      	list_add_tail(&newitem->node, &assigned_reads.node);
      	return 0;
	}
	// todo: unset lock x

	/* If it cannot be assigned, add it to wait and  make thread sleep */
	list_add_tail(&newitem->node, &waiting_locks.node);
	// todo: make it sleep

	return 0;
	
}

int do_rotlock_write(int degree, int range)
{
}

int do_set_rotation(int degree)
{	
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
	return do_rotlock_read(degree, range);
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
	return 0;
	//return do_rotlock_write(degree, range);
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
	return 0;
	//return do_rotunlock_read(degree, range);
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
	return 0;
	//return do_rotunlock_write(degree, range);
}
