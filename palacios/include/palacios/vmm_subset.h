/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_SUBSET_H__
#define __VMM_SUBSET_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>

/* Subset Barriers */

typedef struct v3_counting_barrier {
    // number of threads that must arrive at the barrier
    // note that this can only be set when there is no barreir in progress
    uint64_t      size;
    uint64_t      count[2];
    uint64_t      cur;
} v3_counting_barrier_t;


static inline void v3_init_counting_barrier(v3_counting_barrier_t *b, uint64_t size)
{
    b->size=size; b->count[0]=b->count[1]=0; b->cur=0;
}

static inline void v3_counting_barrier(volatile v3_counting_barrier_t *b)
{
    uint64_t old;
    volatile uint64_t *curp = &(b->cur);
    long mycur = *curp;
    volatile uint64_t *countp = &(b->count[mycur]);
 
    old = __sync_fetch_and_add(countp,1);

    if (old==(b->size-1)) { 
	// I'm the last to the party
	*curp ^= 0x1;
	*countp = 0;
    } else {
	// k1om compiler does not know what "volatile" means
	// hence this hand-coding.
	do { 
	    __asm__ __volatile__( "movq %1, %0" : "=r"(old) : "m"(*countp) : );
	} while (old);
    }
}


#endif

#endif
