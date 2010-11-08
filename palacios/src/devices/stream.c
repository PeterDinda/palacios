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
#include <devices/serial.h>

struct stream_state {
    void *stream_in;
    void *stream_out;
    struct vm_device *frontend_dev;
    struct v3_stream_ops stream_ops;
};

static int stream_read(char *buf, uint_t length, void *private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct stream_state *state = (struct stream_state *) dev->private_data;
    
    return V3_StreamRead(state->stream_out,buf,length);
}

static int stream_write(char *buf, uint_t length, void *private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct stream_state *state = (struct stream_state *) dev->private_data;
    
    return V3_StreamWrite(state->stream_out,buf,length);
}


static void notify(void * data){
    struct stream_state *state = (struct stream_state *)data;
    char temp[1024];
    int len;	
	
    len = V3_StreamRead(state->stream_in, temp, 1024);	
    state->stream_ops.input(temp, len, state->stream_ops.front_data);
}

static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int stream_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    const char * frontend_tag = v3_cfg_val(frontend_cfg, "tag");
    struct vm_device * frontend = v3_find_dev(vm, frontend_tag);
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * stream_in = v3_cfg_val(cfg, "stream_in");
    char * stream_out = v3_cfg_val(cfg, "stream_out");
    struct stream_state *state;


    V3_ASSERT(frontend_cfg);
    V3_ASSERT(frontend_tag);
    V3_ASSERT(frontend);
    V3_ASSERT(stream_in);
    V3_ASSERT(stream_out);

    state = (struct stream_state *)V3_Malloc(sizeof(struct stream_state));
    V3_ASSERT(state);
    state->frontend_dev = frontend;

    state->stream_out = V3_StreamOpen(stream_out, NULL, NULL, STREAM_OPEN_MODE_READ | STREAM_OPEN_MODE_WRITE);
    state->stream_in = V3_StreamOpen(stream_in, notify, state, STREAM_OPEN_MODE_READ | STREAM_OPEN_MODE_WRITE);
    if (!state->stream_out || !state->stream_in) {
	PrintError("Could not open stream %s %s\n", stream_in, stream_out);
	V3_Free(state);
	return -1;
    }
	
    struct vm_device *dev = v3_allocate_device(dev_id, &dev_ops, state);
    V3_ASSERT(dev);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    state->stream_ops.read = stream_read;
    state->stream_ops.write = stream_write;

    v3_stream_register_serial(frontend, &(state->stream_ops), dev);

    return 0;
}

device_register("STREAM", stream_init)

