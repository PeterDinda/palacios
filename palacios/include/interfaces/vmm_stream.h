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



struct v3_stream {
    void * host_stream_data;
    void * guest_stream_data;
    uint64_t (*input)(struct v3_stream * stream, uint8_t * buf, uint64_t len);
};


#ifdef __V3VEE__
#include <palacios/vmm.h>



/* VM Can be NULL */
struct v3_stream * v3_stream_open(struct v3_vm_info * vm, const char * name,
				  uint64_t (*input)(struct v3_stream * stream, uint8_t * buf, uint64_t len),
				  void * guest_stream_data);

uint64_t  v3_stream_output(struct v3_stream * stream, uint8_t * buf, uint32_t len);

void v3_stream_close(struct v3_stream * stream);

#endif


struct v3_stream_hooks {
    void *(*open)(struct v3_stream * stream, const char * name, void * host_vm_data);
    uint64_t (*output)(struct v3_stream * stream, char * buf, int len);
    void (*close)(struct v3_stream * stream);
};


void V3_Init_Stream(struct v3_stream_hooks * hooks);

#endif
