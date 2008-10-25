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


#include <palacios/vmm_decoder.h>


int v3_opcode_cmp(const uchar_t * op1, const uchar_t * op2) {
  if (op1[0] != op2[0]) {
    return op1[0] - op2[0];;
  } else {
    return memcmp(op1 + 1, op2 + 1, op1[0]);
  }
}


void v3_strip_rep_prefix(uchar_t * instr, int length) {
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
