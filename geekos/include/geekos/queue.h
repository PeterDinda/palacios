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
