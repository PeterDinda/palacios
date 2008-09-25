/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#ifndef __VMM_QUEUE_H__
#define __VMM_QUEUE_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>
#include <palacios/vmm_list.h>



/* IMPORTANT:
 * This implementation currently does no locking, and as such is not 
 * SMP/thread/interrupt safe
 */


struct queue_entry {
  addr_t entry;
  struct list_head entry_list;
};


struct gen_queue {
  uint_t num_entries;
  struct list_head entries;

  // We really need to implement this....
  // void * lock;
};


struct gen_queue * v3_create_queue();
void v3_init_queue(struct gen_queue * queue);

void v3_enqueue(struct gen_queue * queue, addr_t entry);
addr_t v3_dequeue(struct gen_queue * queue);



#endif // ! __V3VEE__

#endif
