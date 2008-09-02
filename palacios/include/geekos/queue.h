#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <geekos/list2.h>
#include <geekos/ktypes.h>
#include <geekos/malloc.h>


/* IMPORTANT:
 * This implementation currently does no locking, and as such is not 
 * SMP/thread/interrupt safe
 */


struct queue_entry {
  void * entry;
  struct list_head entry_list;
};


struct gen_queue {
  uint_t num_entries;
  struct list_head entries;

  // We really need to implement this....
  // void * lock;
};


struct gen_queue * create_queue();
void init_queue(struct gen_queue * queue);

void enqueue(struct gen_queue * queue, void * entry);
void * dequeue(struct gen_queue * queue);




#endif
