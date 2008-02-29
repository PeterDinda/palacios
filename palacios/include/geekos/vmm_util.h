#ifndef __VMM_UTIL_H
#define __VMM_UTIL_H

#include <geekos/vmm.h>



//#define PAGE_SIZE 4096

typedef union reg_ex {
  ullong_t r_reg;
  struct {
    uint_t low;
    uint_t high;
  } e_reg;

} reg_ex_t;



void PrintTraceHex(unsigned char x);

void PrintTraceMemDump(unsigned char * start, int n);



#endif
