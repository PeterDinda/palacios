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


#ifndef __PALACIOS_QUEUE_H__
#define __PALACIOS_QUEUE_H__



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

int enqueue(struct gen_queue * queue, void * entry);
void * dequeue(struct gen_queue * queue);





#endif
