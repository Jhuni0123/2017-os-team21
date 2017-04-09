#include <linux/rotation.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/syscalls.h>

#define INITIAL_ROT 0

int device_rot = INITIAL_ROT;
int range_desc_init = 0;

struct range_desc wait_read;
struct range_desc wait_write;
struct range_desc locked_read;
struct range_desc locked_write;

void init_range_desc(struct range_desc *head){
	head->degree = -1;
	head->range = -1;
	head->tid = -1;
	INIT_LIST_HEAD(&head->node);
}

void init_range_lists(){
	init_range_desc(&wait_read);
 	init_range_desc(&wait_write);
	init_range_desc(&locked_read);
	init_range_desc(&locked_write);
}

int is_locked_write = 0;
int is_waiting_write = 0;
struct range_desc *w_write = NULL;
int result = 0;

int do_rotlock_read(int degree, int range)
{
	struct range_desc* newitem =
		(struct range_desc*) kmalloc(sizeof(struct range_desc), GFP_KERNEL);
	if(newitem == NULL)
		return -ENOMEM;

	newitem->degree = degree;
	newitem->range = range;
	newitem->tid = task_pid_vnr(current);
	INIT_LIST_HEAD(&newitem->node);

	/* If device lock is in newitem's range
	   and there is no locked write nor waiting write,
	   give lock */
	if(range_in_rotation(newitem)&&(!is_locked_write)&&(!is_waiting_write)){
      	list_add_tail(&newitem->node, &locked_read.node);
      	return 0;
	}

	/* If it cannot be assigned, add it to wait and  make thread sleep */
	list_add_tail(&newitem->node, &wait_read.node);
	// todo: make it sleep


	return 0;
	
}

int do_rotlock_wait(int degree, int range)
{
}

int do_set_rotation(int degree)
{
	/* update device rotation info */
	device_rot = degree;
	/* initialize variables */
	is_locked_write = 0;
	is_waiting_write = 0;
	w_write = NULL;
  	/* check if there is a locked write lock
	 * if there is already a writer lock holding the rotation
	 * no more lock can be allocated: return 0 */
	struct range_desc *pos = NULL;
	list_for_each_entry(pos, &locked_write.node, node){
		if(range_in_rotation(pos)){
    		is_locked_write = 1;
    		return 0;
    	}
  	}
	/* check if there is a write lock waiting to be allocated */
	list_for_each_entry(pos, &wait_write.node, node){
    	if(range_in_rotation(pos)){
			is_waiting_write = 1;
	    	w_write = pos;
     		break;
    	}
  	}
	/* If there is a waiting write lock, check if there is
	 * an allocated read lock.
     * If there is not, allocate waiting read locks */
	int can_alloc_write = 1;
	if(is_waiting_write){
    	list_for_each_entry(pos, &locked_read.node, node){
      		if(range_overlap(w_write, pos)){
        		can_alloc_write = 0;
        		break;
      		}
    	}
    	if(can_alloc_write){
      		list_del(&w_write->node);
      		list_add_tail(&w_write->node, &locked_write.node);
      		is_locked_write = 1;
      		is_waiting_write = 0;
      		w_write = NULL;
      		// to do: unblock process block
      		return 1;
    	} else {
      		return 0;
    	}
	/* If there is no waiting write lock,
     * it's safe to allocate waiting read locks.
     * Allocate all. */
  	} else {
    	list_for_each_entry(pos, &wait_read.node, node){
      		if(range_in_rotation(pos)){
        		list_del(&pos->node);
        		list_add_tail(&pos->node, &locked_read.node);
        		result++;
      		}
    	}
  	}
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
