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

#ifndef __VMM_STUBS_H
#define __VMM_STUBS_H


#include <geekos/mem.h>
#include <geekos/malloc.h>


struct guest_info;




void * Allocate_VMM_Pages(int num_pages);
void Free_VMM_Page(void * page);

void * VMM_Malloc(unsigned int size);
void VMM_Free(void * addr);

void * Identity(void *addr);




int hook_irq_stub(struct guest_info * info, int irq);
int ack_irq(int irq);



int geekos_hook_interrupt(struct guest_info * info, uint_t irq);



unsigned int get_cpu_khz();

void Init_Stubs(struct guest_info * info);




/**** 
 * 
 * stubs called by geekos....
 * 
 ***/
void send_key_to_vmm(unsigned char status, unsigned char scancode);
void send_mouse_to_vmm(unsigned char packet[3]);
void send_tick_to_vmm(unsigned int period_us);


#if 0

# define do_div(n,base) ({					\
	uint32_t __base = (base);				\
	uint32_t __rem;						\
	__rem = ((uint64_t)(n)) % __base;			\
	(n) = ((uint64_t)(n)) / __base;				\
	__rem;							\
 })

#else

/*
 * do_div() is NOT a C function. It wants to return
 * two values (the quotient and the remainder), but
 * since that doesn't work very well in C, what it
 * does is:
 *
 * - modifies the 64-bit dividend _in_place_
 * - returns the 32-bit remainder
 *
 * This ends up being the most efficient "calling
 * convention" on x86.
 */
#define do_div(n,base) ({				     \
      unsigned long __upper, __low, __high, __mod, __base;   \
      __base = (base);					     \
      asm("":"=a" (__low), "=d" (__high):"A" (n));	     \
      __upper = __high;					     \
      if (__high) {					     \
	__upper = __high % (__base);			     \
	__high = __high / (__base);			     \
      }									\
      asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
      asm("":"=A" (n):"a" (__low),"d" (__high));			\
      __mod;								\
    })

#endif






#endif
