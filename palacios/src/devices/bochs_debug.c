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



#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>

#define BUF_SIZE 1024

#define BOCHS_PORT1 0x400
#define BOCHS_PORT2 0x401
#define BOCHS_INFO_PORT 0x402
#define BOCHS_DEBUG_PORT 0x403

#define BOCHS_CONSOLE_PORT 0xe9


struct debug_state {
    char debug_buf[BUF_SIZE];
    uint_t debug_offset;

    char info_buf[BUF_SIZE];
    uint_t info_offset;

    char cons_buf[BUF_SIZE];
    uint_t cons_offset;
};

static int handle_info_write(struct guest_info * core, ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct debug_state * state = (struct debug_state *)dev->private_data;

    state->info_buf[state->info_offset++] = *(char*)src;

    if ((*(char*)src == 0xa) ||  (state->info_offset == (BUF_SIZE - 1))) {
	PrintDebug("BOCHSINFO>%s", state->info_buf);
	memset(state->info_buf, 0, BUF_SIZE);
	state->info_offset = 0;
    }

    return length;
}


static int handle_debug_write(struct guest_info * core, ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct debug_state * state = (struct debug_state *)dev->private_data;

    state->debug_buf[state->debug_offset++] = *(char*)src;

    if ((*(char*)src == 0xa) ||  (state->debug_offset == (BUF_SIZE - 1))) {
	PrintDebug("BOCHSDEBUG>%s", state->debug_buf);
	memset(state->debug_buf, 0, BUF_SIZE);
	state->debug_offset = 0;
    }

    return length;
}


static int handle_console_write(struct guest_info * core, ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct debug_state * state = (struct debug_state *)dev->private_data;

    state->cons_buf[state->cons_offset++] = *(char*)src;

    if ((*(char*)src == 0xa) ||  (state->cons_offset == (BUF_SIZE - 1))) {
	PrintDebug("BOCHSCONSOLE>%s", state->cons_buf);
	memset(state->cons_buf, 0, BUF_SIZE);
	state->cons_offset = 0;
    }

    return length;
}


static int handle_gen_write(struct guest_info * core, ushort_t port, void * src, uint_t length, struct vm_device * dev) {

    switch (length) {
	case 1:
	    PrintDebug(">0x%.2x\n", *(uchar_t*)src);
	    break;
	case 2:
	    PrintDebug(">0x%.4x\n", *(ushort_t*)src);
	    break;
	case 4:
	    PrintDebug(">0x%.8x\n", *(uint_t*)src);
	    break;
	default:
	    PrintError("Invalid length in handle_gen_write\n");
	    return -1;
	    break;
    }

    return length;
}




static int debug_free(struct vm_device * dev) {
    v3_dev_unhook_io(dev, BOCHS_PORT1);
    v3_dev_unhook_io(dev, BOCHS_PORT2);
    v3_dev_unhook_io(dev, BOCHS_INFO_PORT);
    v3_dev_unhook_io(dev, BOCHS_DEBUG_PORT);

    return 0;
};




static struct v3_device_ops dev_ops = {
    .free = debug_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};




static int debug_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct debug_state * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");

    state = (struct debug_state *)V3_Malloc(sizeof(struct debug_state));

    V3_ASSERT(state != NULL);

    PrintDebug("Creating Bochs Debug Device\n");
    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, state);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	return -1;
    }

    state->debug_offset = 0;
    state->info_offset = 0;
    memset(state->debug_buf, 0, BUF_SIZE);
    memset(state->info_buf, 0, BUF_SIZE);


    v3_dev_hook_io(dev, BOCHS_PORT1,  NULL, &handle_gen_write);
    v3_dev_hook_io(dev, BOCHS_PORT2, NULL, &handle_gen_write);
    v3_dev_hook_io(dev, BOCHS_INFO_PORT, NULL, &handle_info_write);
    v3_dev_hook_io(dev, BOCHS_DEBUG_PORT, NULL, &handle_debug_write);
    v3_dev_hook_io(dev, BOCHS_CONSOLE_PORT, NULL, &handle_console_write);
    
  
    return 0;
}


device_register("BOCHS_DEBUG", debug_init);
