#ifndef __VMM_UTIL_H
#define __VMM_UTIL_H

#include <palacios/vmm_types.h>


#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif


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
#define GET_HIGH_32(x) (*((uint_t*)(((char*)(&(x)))+4)))


void PrintTraceHex(unsigned char x);

void PrintTraceMemDump(unsigned char * start, int n);




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
#define do_div(n,base) ({ \
	unsigned long __upper, __low, __high, __mod, __base; \
	__base = (base); \
	asm("":"=a" (__low), "=d" (__high):"A" (n)); \
	__upper = __high; \
	if (__high) { \
		__upper = __high % (__base); \
		__high = __high / (__base); \
	} \
	asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
	asm("":"=A" (n):"a" (__low),"d" (__high)); \
	__mod; \
})

#endif





#endif
