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
    void *stream;
    struct vm_device *frontend_dev;
};


static int stream_read(char *buf, uint_t length, void *private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct stream_state *state = (struct stream_state *) dev->private_data;
    
    return V3_StreamRead(state->stream,buf,length);
}

static int stream_write(char *buf, uint_t length, void *private_data) 
{
    struct vm_device *dev = (struct vm_device *) private_data;
    struct stream_state *state = (struct stream_state *) dev->private_data;
    
    return V3_StreamWrite(state->stream,buf,length);
}



static struct v3_stream_ops stream_ops = {
    .write = stream_write,
    .read = stream_read
};

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
    char * path = v3_cfg_val(cfg, "localname");

    /* read configuration */
    V3_ASSERT(frontend_cfg);
    V3_ASSERT(frontend_tag);
    V3_ASSERT(frontend);

    
    /* allocate state */
    state = (struct cons_state *)V3_Malloc(sizeof(struct stream_state));
    V3_ASSERT(state);
    state->frontend_dev = frontend;
    V3_ASSERT(path);
	
    /* The system is responsible for interpreting the localname of the stream */
    state->stream = V3_StreamOpen(ttypath, STREAM_OPEN_MODE_READ | STREAM_OPEN_MODE_WRITE);
    if (!state->stream) {
	PrintError("Could not open localname %s\n", path);
	V3_Free(state);
	return -1;
    }

    /* allocate device */
    struct vm_device *dev = v3_allocate_device(dev_id, &dev_ops, state);
    V3_ASSERT(dev);

    /* attach device to virtual machine */
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    /* attach to front-end display adapter */
    v3_console_register_cga(frontend, &cons_ops, dev);

	return 0;
}

device_register("CURSES_CONSOLE", cons_init)

