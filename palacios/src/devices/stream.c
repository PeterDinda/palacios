/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_stream.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_string.h>


struct stream_state {
    v3_stream_t stream;

    struct v3_dev_char_ops * char_ops;
};



static int stream_write(uint8_t * buf, uint64_t length, void * private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct stream_state *state = (struct stream_state *) dev->private_data;
    
    return v3_stream_write(state->stream, buf, length);
}




static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int stream_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
    //   v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * stream_name = v3_cfg_val(cfg, "stream");
    struct stream_state * state = NULL;


    state = (struct stream_state *)V3_Malloc(sizeof(struct stream_state));

    V3_ASSERT(state);


    state->stream = v3_stream_open(vm, stream_name);

    if (state->stream == NULL) {
	PrintError("Could not open stream %s\n", stream_name);
	V3_Free(state);
	return -1;
    }

    state->char_ops->write = stream_write;
	
    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not allocate device %s\n", dev_id);
	return -1;
    }

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }



    return 0;
}

device_register("STREAM", stream_init)

