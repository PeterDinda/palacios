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
#include <palacios/vmm_io.h>
#include <palacios/vm_guest.h>

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

static int handle_info_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct debug_state * state = (struct debug_state *)priv_data;

    state->info_buf[state->info_offset++] = *(char*)src;

    if ((*(char*)src == 0xa) ||  (state->info_offset == (BUF_SIZE - 1))) {
	PrintDebug(core->vm_info, core, "BOCHSINFO>%s", state->info_buf);
	memset(state->info_buf, 0, BUF_SIZE);
	state->info_offset = 0;
    }

    return length;
}


static int handle_debug_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct debug_state * state = (struct debug_state *)priv_data;

    state->debug_buf[state->debug_offset++] = *(char*)src;

    if ((*(char*)src == 0xa) ||  (state->debug_offset == (BUF_SIZE - 1))) {
	PrintDebug(core->vm_info, core, "BOCHSDEBUG>%s", state->debug_buf);
	memset(state->debug_buf, 0, BUF_SIZE);
	state->debug_offset = 0;
    }

    return length;
}


static int handle_console_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct debug_state * state = (struct debug_state *)priv_data;

    state->cons_buf[state->cons_offset++] = *(char *)src;

    if ((*(char *)src == 0xa) ||  (state->cons_offset == (BUF_SIZE - 1))) {
	V3_Print(core->vm_info, core, "BOCHSCONSOLE>%s", state->cons_buf);
	memset(state->cons_buf, 0, BUF_SIZE);
	state->cons_offset = 0;
    }

    return length;
}


static int handle_gen_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data)  {
    
    switch (length) {
	case 1:
	    PrintDebug(core->vm_info, core, ">0x%.2x\n", *(uchar_t*)src);
	    break;
	case 2:
	    PrintDebug(core->vm_info, core, ">0x%.4x\n", *(ushort_t*)src);
	    break;
	case 4:
	    PrintDebug(core->vm_info, core, ">0x%.8x\n", *(uint_t*)src);
	    break;
	default:
	    PrintError(core->vm_info, core, "Invalid length in handle_gen_write\n");
	    return -1;
	    break;
    }

    return length;
}




static int debug_free(struct debug_state * state) {

    V3_Free(state);
    return 0;
};




static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))debug_free,

};




static int debug_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct debug_state * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    state = (struct debug_state *)V3_Malloc(sizeof(struct debug_state));

    if (state == NULL) {
	PrintError(vm, VCORE_NONE, "Could not allocate bochs debug state\n");
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "Creating Bochs Debug Device\n");
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    state->debug_offset = 0;
    state->info_offset = 0;
    state->cons_offset = 0;
    memset(state->debug_buf, 0, BUF_SIZE);
    memset(state->info_buf, 0, BUF_SIZE);
    memset(state->cons_buf, 0, BUF_SIZE);


    ret |= v3_dev_hook_io(dev, BOCHS_PORT1,  NULL, &handle_gen_write);
    ret |= v3_dev_hook_io(dev, BOCHS_PORT2, NULL, &handle_gen_write);
    ret |= v3_dev_hook_io(dev, BOCHS_INFO_PORT, NULL, &handle_info_write);
    ret |= v3_dev_hook_io(dev, BOCHS_DEBUG_PORT, NULL, &handle_debug_write);
    ret |= v3_dev_hook_io(dev, BOCHS_CONSOLE_PORT, NULL, &handle_console_write);
    
    if (ret != 0) {
	PrintError(vm, VCORE_NONE, "Could not hook Bochs Debug IO Ports\n");
	v3_remove_device(dev);
	return -1;
    }
  
    return 0;
}


device_register("BOCHS_DEBUG", debug_init);
