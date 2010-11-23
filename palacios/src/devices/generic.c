/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 * Contributor: 2008, Jack Lange <jarusl@cs.northwestern.edu>
 *        
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_dev_mgr.h>

#ifndef CONFIG_DEBUG_GENERIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


typedef enum {GENERIC_IGNORE, 
	      GENERIC_PASSTHROUGH, 
	      GENERIC_PRINT_AND_PASSTHROUGH, 
	      GENERIC_PRINT_AND_IGNORE} generic_mode_t;

struct generic_internal {
};


struct port_range {
    uint_t start;
    uint_t end;
    generic_mode_t mode;
    struct list_head range_link;
};





static int generic_write_port_passthrough(struct guest_info * core, uint16_t port, void * src, 
					  uint_t length, void * priv_data) {
    uint_t i;

    PrintDebug("generic: writing 0x");

    for (i = 0; i < length; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }
  
    PrintDebug(" to port 0x%x ... ", port);

    switch (length) {
	case 1:
	    v3_outb(port, ((uint8_t *)src)[0]);
	    break;
	case 2:
	    v3_outw(port, ((uint16_t *)src)[0]);
	    break;
	case 4:
	    v3_outdw(port, ((uint32_t *)src)[0]);
	    break;
	default:
	    for (i = 0; i < length; i++) { 
		v3_outb(port, ((uint8_t *)src)[i]);
	    }
    }

    PrintDebug(" done\n");
  
    return length;
}

static int generic_read_port_passthrough(struct guest_info * core, uint16_t port, void * src, 
					 uint_t length, void * priv_data) {
    uint_t i;

    PrintDebug("generic: reading 0x%x bytes from port 0x%x ...", length, port);


    switch (length) {
	case 1:
	    ((uint8_t *)src)[0] = v3_inb(port);
	    break;
	case 2:
	    ((uint16_t *)src)[0] = v3_inw(port);
	    break;
	case 4:
	    ((uint32_t *)src)[0] = v3_indw(port);
	    break;
	default:
	    for (i = 0; i < length; i++) { 
		((uint8_t *)src)[i] = v3_inb(port);
	    }
    }

    PrintDebug(" done ... read 0x");

    for (i = 0; i < length; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }

    PrintDebug("\n");

    return length;
}

static int generic_write_port_ignore(struct guest_info * core, uint16_t port, void * src, 
				     uint_t length, void * priv_data) {
    int i;

    PrintDebug("generic: writing 0x");

    for (i = 0; i < length; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }
  
    PrintDebug(" to port 0x%x ... ignored\n", port);
 
    return length;
}

static int generic_read_port_ignore(struct guest_info * core, uint16_t port, void * src, 
				    uint_t length, void * priv_data) {

    PrintDebug("generic: reading 0x%x bytes from port 0x%x ...", length, port);

    memset((uint8_t *)src, 0, length);
    PrintDebug(" ignored (return zeroed buffer)\n");

    return length;
}





static int generic_free(struct generic_internal * state) {
    PrintDebug("generic: deinit_device\n");

    V3_Free(state);
    return 0;
}





static struct v3_device_ops dev_ops = { 
    .free = (int (*)(void *))generic_free, 
};




static int add_port_range(struct vm_device * dev, uint_t start, uint_t end, generic_mode_t mode) {
    uint_t i = 0;

    PrintDebug("generic: Adding Port Range: 0x%x to 0x%x as %s\n", 
	       start, end, 
	       (mode == GENERIC_PRINT_AND_PASSTHROUGH) ? "print-and-passthrough" : "print-and-ignore");
    
    for (i = start; i <= end; i++) { 
	if (mode == GENERIC_PRINT_AND_PASSTHROUGH) { 
	    if (v3_dev_hook_io(dev, i, 
				&generic_read_port_passthrough, 
				&generic_write_port_passthrough) == -1) { 
		PrintError("generic: can't hook port 0x%x (already hooked?)\n", i);
		return -1;
	    }
	} else if (mode == GENERIC_PRINT_AND_IGNORE) { 
	    if (v3_dev_hook_io(dev, i, 
				&generic_read_port_ignore, 
				&generic_write_port_ignore) == -1) { 
		PrintError("generic: can't hook port 0x%x (already hooked?)\n", i);
		return -1;
	    }
	} 
    }
    
    return 0;
}





static int generic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct generic_internal * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    v3_cfg_tree_t * port_cfg = v3_cfg_subtree(cfg, "ports");


    state = (struct generic_internal *)V3_Malloc(sizeof(struct generic_internal));

    if (state == NULL) {
	PrintError("Could not allocate generic state\n");
	return -1;
    }
    
    memset(state, 0, sizeof(struct generic_internal));
    
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    PrintDebug("generic: init_device\n");

    // scan port list....
    while (port_cfg) {
	uint16_t start = atox(v3_cfg_val(port_cfg, "start"));
	uint16_t end = atox(v3_cfg_val(port_cfg, "end"));
	char * mode_str = v3_cfg_val(port_cfg, "mode");
	generic_mode_t mode = GENERIC_IGNORE;

	if (strcasecmp(mode_str, "print_and_ignore") == 0) {
	    mode = GENERIC_PRINT_AND_IGNORE;
	} else if (strcasecmp(mode_str, "print_and_passthrough") == 0) {
	    mode = GENERIC_PRINT_AND_PASSTHROUGH;
	} else {
	    PrintError("Invalid Mode %s\n", mode_str);
	    v3_remove_device(dev);
	    return -1;
	}
	
	if (add_port_range(dev, start, end, mode) == -1) {
	    PrintError("Could not add port range %d-%d\n", start, end);
	    v3_remove_device(dev);
	    return -1;
	}

	port_cfg = v3_cfg_next_branch(port_cfg);
    }


    return 0;
}

device_register("GENERIC", generic_init)
