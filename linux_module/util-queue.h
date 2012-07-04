/* 
 * Queue implementation
 * Jack Lange 2011
 */

#ifndef __PALACIOS_QUEUE_H__
#define __PALACIOS_QUEUE_H__


#include "palacios.h"

#include <linux/list.h>
#include <linux/spinlock.h>




struct queue_entry {
    void * entry;
    struct list_head node;
};


struct gen_queue {
    unsigned int num_entries;
    unsigned int max_entries;
    struct list_head entries;
    spinlock_t lock;
};


struct gen_queue * create_queue(unsigned int max_entries);
void init_queue(struct gen_queue * queue, unsigned int max_entries);
void deinit_queue(struct gen_queue * queue);


int enqueue(struct gen_queue * queue, void * entry);
void * dequeue(struct gen_queue * queue);





#endif
