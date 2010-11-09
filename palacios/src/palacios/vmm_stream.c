/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>

#include <palacios/vmm_stream.h>
#include <palacios/vm_guest.h>

static struct v3_stream_hooks * stream_hooks = NULL;

// VM can be NULL
v3_stream_t v3_stream_open(struct v3_vm_info * vm, const char * name) {
    V3_ASSERT(stream_hooks != NULL);
    V3_ASSERT(stream_hooks->open != NULL);

    return stream_hooks->open(name, vm->host_priv_data);
}

int v3_stream_write(v3_stream_t stream, uint8_t * buf, uint32_t len) {
    V3_ASSERT(stream_hooks != NULL);
    V3_ASSERT(stream_hooks->write != NULL);

    return stream_hooks->write(stream, buf, len);
}

void v3_stream_close(v3_stream_t stream) {
    V3_ASSERT(stream_hooks != NULL);
    V3_ASSERT(stream_hooks->close != NULL);

    return stream_hooks->close(stream);
}





void V3_Init_Stream(struct v3_stream_hooks * hooks) {
    stream_hooks = hooks;
    PrintDebug("V3 stream inited\n");

    return;
}
