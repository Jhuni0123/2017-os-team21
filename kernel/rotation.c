#include <linux/rotation.h>
#include <linux/list.h>

#define INITIAL_ROT = 0;

int device_rot = INITIAL_ROT;
int range_desc_init = 0;

struct range_desc wait_read;
struct range_desc wait_write;
struct range_desc locked_read;
struct range_desc locked_write;

void init_range_desc(range_desc *head){
	head->degree = -1;
	head->range = -1;
	head->tid = -1;
	head->node = LIST_HEAD_INIT(head->node);
}

void init_range_lists(){
	init_range_desc(&wait_read);
 	init_range_desc(&wait_write);
	init_range_desc(&locekd_read);
	init_range_desc(&locked_write);
}

int is_locked_write = 0;
int is_waiting_write = 0;
struct range_desc *w_write = NULL;
int result = 0;

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
	struct range_desc *pos = (struct range_desc *) kmalloc(sizeof(struct range_desc), GFP_ATOMIC);
 	if(pos == NULL){
		printk("DEBUG: kamlloc failure for pos");
		return -ENOMEM;
	}
	list_for_each_entry(pos, &locked_write.node, head){
		if(range_in_rotation(pos)){
    		is_locked_write = 1;
    		return 0;
    	}
  	}
	/* check if there is a write lock waiting to be allocated */
	list_for_each_entry(pos, &waiting_write.node, head){
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
    	list_for_each_entry(pos, &locked_read.node, head){
      		if(range_overlap(w_write, pos)){
        		cannot_alloc_write = 0;
        		break;
      		}
    	}
    	if(can_alloc_write){
      		list_del(w_write);
      		list_add(w_write, &locked_write.node);
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
    	list_for_each_entry(pos, &waiting_read.node, head){
      		if(range_in_rotation(pos)){
        		list_del(pos);
        		list_add(pos, &locked_read.node, head);
        		result++;
      		}
    	}
  	}
  	return result;
}

