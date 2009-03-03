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

#include <palacios/vmm_debug.h>
#include <palacios/vmm.h>


void PrintDebugHex(uchar_t x) {
    unsigned char z;
  
    z = (x >> 4) & 0xf ;
    PrintDebug("%x", z);
    z = x & 0xf;
    PrintDebug("%x", z);
}

void PrintDebugMemDump(uchar_t *start, int n) {
    int i, j;

    for (i = 0; i < n; i += 16) {
	PrintDebug("%p", (void *)(start + i));

	for (j = i; (j < (i + 16)) && (j < n); j += 2) {
	    PrintDebug(" ");
	    PrintDebugHex(*((uchar_t *)(start + j)));

	    if ((j + 1) < n) { 
		PrintDebugHex(*((uchar_t *)(start + j + 1)));
	    }

	}

	PrintDebug(" ");

	for (j = i; (j < (i + 16)) && (j < n); j++) {
	    PrintDebug("%c", ((start[j] >= 32) && (start[j] <= 126)) ? start[j] : '.');
	}

	PrintDebug("\n");
    }
}
