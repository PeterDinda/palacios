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

#include <palacios/vmm_types.h>


#ifdef __V3_32BIT__

void __inline__ v3_cpuid(uint_t target, addr_t * eax, addr_t * ebx, addr_t * ecx, addr_t * edx) {
  __asm__ __volatile__ (
			"pushl %%ebx\n\t"
			"cpuid\n\t"
			"movl %%ebx, %%esi\n\t"
			"popl %%ebx\n\t"
			: "=a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
			: "a" (target)
			);
  return;
}

#elif __V3_64BIT__

void __inline__ v3_cpuid(uint_t target, addr_t * eax, addr_t * ebx, addr_t * ecx, addr_t * edx) {
  __asm__ __volatile__ (
			"pushq %%rbx\n\t"
			"cpuid\n\t"
			"movq %%rbx, %%rsi\n\t"
			"popq %%rbx\n\t"
			: "=a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
			: "a" (target)
			);
  return;
}

#endif


void __inline__ v3_set_msr(uint_t msr, uint_t high_byte, uint_t low_byte) {
  __asm__ __volatile__ (
			"wrmsr"
			: 
			: "c" (msr), "d" (high_byte), "a" (low_byte)
			);


}



void __inline__ v3_get_msr(uint_t msr, uint_t * high_byte, uint_t * low_byte) {
  __asm__ __volatile__ (
			"rdmsr"
			: "=d" (*high_byte), "=a" (*low_byte) 
			: "c" (msr)
			);
}



void __inline__ v3_enable_ints() {
  __asm__ __volatile__ ("sti");
}

void __inline__ v3_disable_ints() {
  __asm__ __volatile__ ("cli");
}

