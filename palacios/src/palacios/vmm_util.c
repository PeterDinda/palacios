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


#include <palacios/vmm_util.h>

#include <palacios/vmm.h>

extern struct v3_os_hooks * os_hooks;


void PrintTraceHex(unsigned char x) {
    unsigned char z;
  
    z = (x >> 4) & 0xf;
    PrintTrace("%x", z);
    z = x & 0xf;
    PrintTrace("%x", z);
}

void PrintTraceLL(ullong_t num) {
    unsigned char * z = (unsigned char *)&num;
    int i;
  
    for (i = 7; i >= 0; i--) {
	PrintTraceHex(*(z + i));
    }
}


void PrintTraceMemDump(uchar_t * start, int n) {
    int i, j;

    for (i = 0; i < n; i += 16) {
	PrintTrace("%p", (void *)(start + i));
	for (j = i; (j < (i + 16)) && (j < n); j += 2) {
	    PrintTrace(" ");
	    PrintTraceHex(*(uchar_t *)(start + j));
	    if ((j + 1) < n) { 
		PrintTraceHex(*((uchar_t *)(start + j + 1)));
	    }
	}
	PrintTrace(" ");
	for (j = i; (j < (i + 16)) && (j < n); j++) {
	    PrintTrace("%c", ((start[j] >= 32) && (start[j] <= 126)) ? start[j] : '.');
	}
	PrintTrace("\n");
    }
}



