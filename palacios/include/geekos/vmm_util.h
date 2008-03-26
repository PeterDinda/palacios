#ifndef __VMM_UTIL_H
#define __VMM_UTIL_H

#include <geekos/ktypes.h>


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


#endif
