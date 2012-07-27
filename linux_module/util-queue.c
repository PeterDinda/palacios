/* 
 * Queue implementation
 * Jack Lange 2011
 */

#include <linux/slab.h>

#include "palacios.h"
#include "util-queue.h"

void init_queue(struct gen_queue * queue, unsigned int max_entries) {
    queue->num_entries = 0;
    queue->max_entries = max_entries;

    INIT_LIST_HEAD(&(queue->entries));
    spin_lock_init(&(queue->lock));
}

void deinit_queue(struct gen_queue * queue) {
    while (dequeue(queue)) {
	ERROR("Freeing non-empty queue. PROBABLE MEMORY LEAK DETECTED\n");
    }
}

struct gen_queue * create_queue(unsigned int max_entries) {
    struct gen_queue * tmp_queue = palacios_alloc(sizeof(struct gen_queue));
    if (!tmp_queue) { 
	ERROR("Unable to allocate a queue\n");
	return NULL;
    }
    init_queue(tmp_queue, max_entries);
    return tmp_queue;
}

int enqueue(struct gen_queue * queue, void * entry) {
    struct queue_entry * q_entry = NULL;
    unsigned long flags;

    if (queue->num_entries >= queue->max_entries) {
	return -1;
    }

    q_entry = palacios_alloc(sizeof(struct queue_entry));
    
    if (!q_entry) { 
	ERROR("Unable to allocate a queue entry on enqueue\n");
	return -1;
    }

    spin_lock_irqsave(&(queue->lock), flags);

    q_entry->entry = entry;
    list_add_tail(&(q_entry->node), &(queue->entries));
    queue->num_entries++;

    spin_unlock_irqrestore(&(queue->lock), flags);

    return 0;
}


void * dequeue(struct gen_queue * queue) {
    void * entry_val = 0;
    unsigned long flags;

    spin_lock_irqsave(&(queue->lock), flags);

    if (!list_empty(&(queue->entries))) {
	struct list_head * q_entry = queue->entries.next;
	struct queue_entry * tmp_entry = list_entry(q_entry, struct queue_entry, node);
	
	entry_val = tmp_entry->entry;
	list_del(q_entry);
	palacios_free(tmp_entry);

	queue->num_entries--;

    }

    spin_unlock_irqrestore(&(queue->lock), flags);

    return entry_val;
}
