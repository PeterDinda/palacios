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
#include <interfaces/vmm_stream.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_string.h>


struct stream_state {
    v3_stream_t stream;

    struct v3_dev_char_ops char_ops;

    void * push_fn_arg;
};


static int serial_event_handler(struct v3_vm_info * vm, 
				struct v3_serial_event * evt, 
				void * private_data) {
    struct stream_state * state = (struct stream_state *)private_data;

    if (state->char_ops.push != NULL){
    	state->char_ops.push(vm, evt->data, evt->len, state->push_fn_arg);
    }

    return 0;
}

static int stream_write(uint8_t * buf, uint64_t length, void * private_data) {
    struct stream_state * state = (struct stream_state *)private_data;
    
    return v3_stream_write(state->stream, buf, length);
}

static int stream_free(struct stream_state * state) {
    v3_stream_close(state->stream);

    // detach host event

    V3_Free(state);

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))stream_free,
};

static int stream_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * stream_name = v3_cfg_val(cfg, "name");
    struct stream_state * state = NULL;

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");

    state = (struct stream_state *)V3_Malloc(sizeof(struct stream_state));

    if (state == NULL) {
	PrintError("Could not allocate stream backend device\n");
	return -1;
    }

    memset(state, 0, sizeof(struct stream_state));

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not allocate device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }



    state->stream = v3_stream_open(vm, stream_name);

    if (state->stream == NULL) {
	PrintError("Could not open stream %s\n", stream_name);
	v3_remove_device(dev);
	return -1;
    }

    state->char_ops.write = stream_write;

    if (v3_dev_connect_char(vm, v3_cfg_val(frontend_cfg, "tag"), 
			    &(state->char_ops), frontend_cfg, 
			    state, &(state->push_fn_arg)) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	v3_remove_device(dev);
	return -1;
    }

    v3_hook_host_event(vm, HOST_SERIAL_EVT, V3_HOST_EVENT_HANDLER(serial_event_handler), state);

    return 0;
}

device_register("CHAR_STREAM", stream_init);
