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
 * Author: Chang Bae <c.s.bae@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */



#include <palacios/svm_handler.h>
#include <palacios/vmm_instrument.h>

#define NO_INSTRUMENTATION
#include <palacios/vmm_ringbuffer.h>
#undef NO_INSTRUMENTATION

#define RING_SIZE 2000

static ullong_t last = 0;
static struct v3_ringbuf * func_ring = NULL;

struct instrumented_func {
    ullong_t time; 
    uint_t exiting;
    void * called_fn;
    void * calling_fn;
} __attribute__((packed));



static void print_instrumentation()  __attribute__((__no_instrument_function__));

void __cyg_profile_func_enter(void * this, void * callsite)   __attribute__((__no_instrument_function__));
void __cyg_profile_func_exit(void * this, void * callsite)   __attribute__((__no_instrument_function__));

void v3_init_instrumentation() {
    PrintDebug("Creating Ring Buffer (unit size = %d)\n", (uint_t)sizeof(struct instrumented_func));
    // initialize
    func_ring = v3_create_ringbuf(sizeof(struct instrumented_func) * RING_SIZE); //dequeue at every 4095  
}



__attribute__((__no_instrument_function__))
    void __cyg_profile_func_enter(void * this, void * callsite) {

    if (func_ring != NULL) {

	struct instrumented_func tmp_fn;
	ullong_t now = 0; 

	rdtscll(now);

	//PrintDebug("Entering Function\n");

	if (v3_ringbuf_avail_space(func_ring) < sizeof(struct instrumented_func)) {
	    print_instrumentation();
	}
    
	tmp_fn.time = now - last; // current tsc
	tmp_fn.exiting = 0; //enter to be 0
	tmp_fn.called_fn = this; //this
	tmp_fn.calling_fn = callsite; //callsite
    
	//    PrintDebug("Writing Function: fn_data=%p, size=%d\n", 
	//       (void *)&tmp_fn, (uint_t)sizeof(struct instrumented_func));
	v3_ringbuf_write(func_ring, (uchar_t *)&tmp_fn, sizeof(struct instrumented_func));  

	rdtscll(last);
    }
}


__attribute__((__no_instrument_function__))
    void __cyg_profile_func_exit(void * this, void * callsite){

    if (func_ring != NULL) {

	struct instrumented_func tmp_fn;
	ullong_t now = 0;

	rdtscll(now);
    
	//    PrintDebug("Exiting Function\n");

	if (v3_ringbuf_avail_space(func_ring) < sizeof(struct instrumented_func)) {
	    print_instrumentation();
	}
    
	tmp_fn.time = now - last; // current tsc
	tmp_fn.exiting = 1; //exit to be 0
	tmp_fn.called_fn = this; //this
	tmp_fn.calling_fn = callsite; //callsite

	//    PrintDebug("Writing Function: fn_data=%p, size=%d\n", 
	//       (void *)&tmp_fn, (uint_t)sizeof(struct instrumented_func));    
	v3_ringbuf_write(func_ring, (uchar_t *)&tmp_fn, sizeof(struct instrumented_func));
    
	rdtscll(last);
    }
}



static void print_instrumentation() {

    struct instrumented_func tmp_fn;

    //  PrintDebug("Printing Instrumentation\n");
    while (v3_ringbuf_data_len(func_ring) >= sizeof(struct instrumented_func)) {
    
	v3_ringbuf_read(func_ring, (uchar_t *)&tmp_fn, sizeof(struct instrumented_func)); 
    
	PrintDebug("CYG_PROF: %d %p %p %p\n", 
		   tmp_fn.exiting, 
		   (void *)(addr_t)(tmp_fn.time), 
		   tmp_fn.called_fn, 
		   tmp_fn.calling_fn);
    }
}



