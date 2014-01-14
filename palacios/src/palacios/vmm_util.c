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
#include <palacios/vmm_sprintf.h>

void v3_dump_mem(uint8_t * start, int n) {
    int i, j;
    char buf[128];

    if (!start) {
	return;
    }      
    
    for (i = 0; i < n; i += 16) {
      snprintf(buf, 128, "%p ", (void *)(start + i));
      for (j = i; (j < (i + 16)) && (j < n); j++) {
	snprintf(buf+strlen(buf),128-strlen(buf),"%02x ", *(uint8_t *)(start + j));
      }
      for (j = i; (j < (i + 16)) && (j < n); j++) {
	snprintf(buf+strlen(buf),128-strlen(buf),"%c", ((start[j] >= 32) && (start[j] <= 126)) ? start[j] : '.');
      }
      snprintf(buf+strlen(buf),128-strlen(buf), "\n");
      buf[strlen(buf)]=0;
      V3_Print(VM_NONE, VCORE_NONE, "%s",buf);
    }
    
}



