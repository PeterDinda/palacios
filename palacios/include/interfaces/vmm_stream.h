/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda (pdinda@northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_STREAM_H__
#define __VMM_STREAM_H__



#ifdef __V3VEE__
#include <palacios/vmm.h>

typedef void * v3_stream_t;

/* VM Can be NULL */
v3_stream_t v3_stream_open(struct v3_vm_info * vm, const char * name);
int v3_stream_write(v3_stream_t stream, uint8_t * buf, uint32_t len);

void v3_stream_close(v3_stream_t stream);

#endif


struct v3_stream_hooks {
    void *(*open)(const char * name, void * private_data);
    int (*write)(void * stream, char * buf, int len);
    void (*close)(void * stream);
};


void V3_Init_Stream(struct v3_stream_hooks * hooks);

#endif
