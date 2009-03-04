/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_queue.h>



void v3_init_queue(struct gen_queue * queue) {
    queue->num_entries = 0;
    INIT_LIST_HEAD(&(queue->entries));
}


struct gen_queue * v3_create_queue() {
    struct gen_queue * tmp_queue = V3_Malloc(sizeof(struct gen_queue));
    v3_init_queue(tmp_queue);
    return tmp_queue;
}

void v3_enqueue(struct gen_queue * queue, addr_t entry) {
    struct queue_entry * q_entry = V3_Malloc(sizeof(struct queue_entry));

    q_entry->entry = entry;
    list_add_tail(&(q_entry->entry_list), &(queue->entries));
    queue->num_entries++;
}


addr_t v3_dequeue(struct gen_queue * queue) {
    addr_t entry_val = 0;

    if (!list_empty(&(queue->entries))) {
	struct list_head * q_entry = queue->entries.next;
	struct queue_entry * tmp_entry = list_entry(q_entry, struct queue_entry, entry_list);

	entry_val = tmp_entry->entry;
	list_del(q_entry);
	V3_Free(tmp_entry);
    }

    return entry_val;
}
