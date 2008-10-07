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

#ifndef __VMM_UTIL_H
#define __VMM_UTIL_H

#ifdef __V3VEE__

#include <palacios/vmm_types.h>


typedef union reg_ex {
  ullong_t r_reg;
  struct {
    uint_t low;
    uint_t high;
  } e_reg;

} reg_ex_t;



// These are the GPRs layed out according to 'pusha'
struct VMM_GPRs {
  uint_t edi;
  uint_t esi;
  uint_t ebp;
  uint_t esp;
  uint_t ebx;
  uint_t edx;
  uint_t ecx;
  uint_t eax;
};


#define GET_LOW_32(x) (*((uint_t*)(&(x))))
#define GET_HIGH_32(x) (*((uint_t*)(((uchar_t*)(&(x)))+4)))


void PrintTraceHex(uchar_t x);
void PrintTraceLL(ullong_t num);
void PrintTraceMemDump(uchar_t * start, int n);




#define rdtsc(low,high)						\
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscl(low)						\
     __asm__ __volatile__("rdtsc" : "=a" (low) : : "edx")

#if defined(__i386__)

#define rdtscll(val)				\
     __asm__ __volatile__("rdtsc" : "=A" (val))

#elif defined(__x86_64__)

#define rdtscll(val) do {				    \
    unsigned int a,d;					    \
    asm volatile("rdtsc" : "=a" (a), "=d" (d));		    \
    (val) = ((unsigned long)a) | (((unsigned long)d)<<32);  \
  } while(0)

#endif









#ifdef __V3_64BIT__
#define do_divll do_div


#define do_div(n,base) ({					\
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



/* This divides two 64bit unsigned ints
 * The previous version only allows 32 bit bases(?)...
 * 
 * NOTE: This absolutely sucks... there has to be a better way....
 */
#define do_divll(n, base) ({				\
      ullong_t __rem = 0;				\
      ullong_t __num = 0;				\
      while (n > base) {				\
	__num++;					\
	n -= base;					\
      }							\
      __rem = n;					\
      n = __num;					\
      __rem;						\
    })						

#endif





#endif // ! __V3VEE__

#endif
