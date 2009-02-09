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

#ifndef __VMM_INSTRUMENT_H__
#define __VMM_INSTRUMENT_H__

#ifdef __V3VEE__

#ifdef INSTRUMENT_VMM 

#include <palacios/vmm_types.h>
#include <palacios/vmm_ringbuffer.h>

ullong_t now, last;

int instrument_start;

struct Inst_RingBuff *ring_buff;

struct PackData *pack_data;

void __attribute__((__no_instrument_function__)) v3_init_cyg_profiler ();
 
void __attribute__((__no_instrument_function__))
__cyg_profile_func_enter( void *this, void *callsite );
 
void __attribute__((__no_instrument_function__)) 
__cyg_profile_func_exit( void *this, void *callsite );


#endif // INSTRUMENT_VMM

#endif // __V3VEE__

#endif // 


