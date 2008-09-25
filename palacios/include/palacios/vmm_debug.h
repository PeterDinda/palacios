/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#ifndef __VMM_DEBUG_H
#define __VMM_DEBUG_H


#ifdef __V3VEE__
#include <palacios/vmm.h>

struct dbg_bp32 {
  uint_t breakpoint : 32;
};

struct dbg_bp64 {
  ullong_t breakpoint;
};

struct dr6_32 {
  uint_t bp0 : 1;
  uint_t bp1 : 1;
  uint_t bp2 : 1;
  uint_t bp3 : 1;
  uint_t rsvd1 : 8; // read as ones
  uint_t rsvd2 : 1; // read as zero
  uint_t bd  : 1; 
  uint_t bs  : 1;
  uint_t bt  : 1;
  uint_t rsvd3 : 16; // read as one
};


struct dr6_64 {
  uint_t bp0 : 1;
  uint_t bp1 : 1;
  uint_t bp2 : 1;
  uint_t bp3 : 1;
  uint_t rsvd1 : 8; // read as ones
  uint_t rsvd2 : 1; // read as zero
  uint_t bd  : 1; 
  uint_t bs  : 1;
  uint_t bt  : 1;
  uint_t rsvd3 : 16; // read as one
  uint_t rsvd4 : 32; // MBZ
};


struct dr7_32 {
  uint_t L0   : 1;
  uint_t G0   : 1;
  uint_t L1   : 1;
  uint_t G1   : 1;
  uint_t L2   : 1;
  uint_t G2   : 1;
  uint_t L3   : 1;
  uint_t G3   : 1;
  uint_t LE   : 1;
  uint_t GE   : 1;
  uint_t rsvd1 : 1; // Read as one
  uint_t rsvd2 : 2; // Read as zero
  uint_t GD   : 1;
  uint_t rsvd3 : 2; // Read as zero
  uint_t rw0  : 1;
  uint_t len0 : 1;
  uint_t rw1  : 1;
  uint_t len1 : 1;
  uint_t rw2  : 1;
  uint_t len2 : 1;
  uint_t rw3  : 1;
  uint_t len3 : 1;
};


struct dr7_64 {
  uint_t L0   : 1;
  uint_t G0   : 1;
  uint_t L1   : 1;
  uint_t G1   : 1;
  uint_t L2   : 1;
  uint_t G2   : 1;
  uint_t L3   : 1;
  uint_t G3   : 1;
  uint_t LE   : 1;
  uint_t GE   : 1;
  uint_t rsvd1 : 1; // Read as one
  uint_t rsvd2 : 2; // Read as zero
  uint_t GD   : 1;
  uint_t rsvd3 : 2; // Read as zero
  uint_t rw0  : 1;
  uint_t len0 : 1;
  uint_t rw1  : 1;
  uint_t len1 : 1;
  uint_t rw2  : 1;
  uint_t len2 : 1;
  uint_t rw3  : 1;
  uint_t len3 : 1;
  uint_t rsvd4 : 32; // MBZ
};






void PrintDebugHex(unsigned char x);
void PrintDebugMemDump(unsigned char *start, int n);



#endif // !__V3VEE__

#endif
