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

#ifdef INSTRUMENT_VMM
 
#include <palacios/svm_handler.h>
#include <palacios/vmm_instrument.h>
#include <palacios/vmm_ringbuffer.h>
 
void __attribute__((__no_instrument_function__)) v3_init_cyg_profiler (){

// initialize

	v3_Inst_RingBuff_init (&ring_buff, 2048); //dequeue at every 4095
	
	instrument_start = 1;
	
}

static void inline __attribute__((__no_instrument_function__)) read_cyg_profiler (){

		while( v3_Inst_RingBuff_data_size(ring_buff) > 0) {

		v3_Inst_RingBuff_read (ring_buff, pack_data, 1); 
		PrintDebug("CYG_PROF: %d %8lu %08x %08x\n", pack_data->state, (unsigned long)pack_data->time, pack_data->cur_fn, pack_data->pre_fn);	
	
		}	
}

void __attribute__((__no_instrument_function__))
__cyg_profile_func_enter( void *this, void *callsite )
{
		
	if(instrument_start > 0) {

	rdtscll(now);
  	
	if((v3_Inst_RingBuff_data_size(ring_buff) % 500) == 0 || v3_Inst_RingBuff_data_size(ring_buff) == 4095) {
	read_cyg_profiler();
	}
  
  
  pack_data->time = now - last; //time spent previous
  pack_data->state = 0; //enter to be 0
  pack_data->cur_fn = (unsigned int)this; //this
  pack_data->pre_fn = (unsigned int)callsite; //callsite

	v3_Inst_RingBuff_write (ring_buff, pack_data, 1);  

  rdtscll(now);
  last = now;
	}
}

void __attribute__((__no_instrument_function__)) 
__cyg_profile_func_exit( void *this, void *callsite )
{
	if(instrument_start > 0 ) {
	rdtscll(now);

	if((v3_Inst_RingBuff_data_size(ring_buff) % 500) == 0 || v3_Inst_RingBuff_data_size(ring_buff) == 4095) {
	read_cyg_profiler();
	}
  
  pack_data->time = now - last; //time spent previous
  pack_data->state = 1; //exit to be 0
  pack_data->cur_fn = (unsigned int)this; //this
  pack_data->pre_fn = (unsigned int)callsite; //callsite
  
  v3_Inst_RingBuff_write (ring_buff, pack_data, 1);
  
  rdtscll(now);
  last = now;
	}
}

#endif
