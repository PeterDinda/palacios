/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#include <palacios/vmm_decoder.h>


int opcode_cmp(const uchar_t * op1, const uchar_t * op2) {
  if (op1[0] != op2[0]) {
    return op1[0] - op2[0];;
  } else {
    return memcmp(op1 + 1, op2 + 1, op1[0]);
  }
}


void strip_rep_prefix(uchar_t * instr, int length) {
  int read_ctr = 0;
  int write_ctr = 0;
  int found = 0;

  while (read_ctr < length) {
    if ((!found) && 
	( (instr[read_ctr] == 0xF2) ||
	  (instr[read_ctr] == 0xF3))) {
      read_ctr++;
      found = 1;
    } else {
      instr[write_ctr] = instr[read_ctr];
      write_ctr++;
      read_ctr++;
    }
  }
}
