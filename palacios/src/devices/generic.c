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
#include <palacios/vm_guest_mem.h>

#ifdef CONFIG_HOST_DEVICE
#include <interfaces/vmm_host_dev.h>
#endif

#ifndef CONFIG_DEBUG_GENERIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define MAX_NAME      32
#define MAX_MEM_HOOKS 16

typedef enum {GENERIC_IGNORE, 
	      GENERIC_PASSTHROUGH, 
	      GENERIC_PRINT_AND_PASSTHROUGH, 
	      GENERIC_PRINT_AND_IGNORE} generic_mode_t;

struct generic_internal {
    enum {GENERIC_PHYSICAL, GENERIC_HOST} forward_type;
#ifdef CONFIG_HOST_DEVICE
    v3_host_dev_t                         host_dev;
#endif
    struct vm_device                      *dev; // me

    char                                  name[MAX_NAME];
    
    uint32_t                              num_mem_hooks;
    addr_t                                mem_hook[MAX_MEM_HOOKS];
};




static int generic_write_port_passthrough(struct guest_info * core, 
					  uint16_t port, 
					  void * src, 
					  uint_t length, 
					  void * priv_data) 
{
    struct generic_internal *state = (struct generic_internal *) priv_data;
    uint_t i;

    switch (state->forward_type) { 
	case GENERIC_PHYSICAL:
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
		    break;
	    }
	    return length;
	    break;
#ifdef CONFIG_HOST_DEVICE
	case GENERIC_HOST:
	    if (state->host_dev) { 
		return v3_host_dev_write_io(state->host_dev,port,src,length);
	    } else {
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("generic (%s): unknown forwarding type\n", state->name);
	    return -1;
	    break;
    }
}

static int generic_write_port_print_and_passthrough(struct guest_info * core, uint16_t port, void * src, 
						    uint_t length, void * priv_data) {
    uint_t i;
    int rc;

#ifdef CONFIG_DEBUG_GENERIC
    struct generic_internal *state = (struct generic_internal *) priv_data;
#endif

    PrintDebug("generic (%s): writing 0x%x bytes to port 0x%x using %s ...", state->name,
	       length, port,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" :
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");

    PrintDebug("generic (%s): writing 0x", state->name);

    for (i = 0; i < length; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }
  
    PrintDebug(" to port 0x%x ... ", port);

    rc=generic_write_port_passthrough(core,port,src,length,priv_data);

    PrintDebug(" done\n");
  
    return rc;
}

static int generic_read_port_passthrough(struct guest_info * core, 
					 uint16_t port, 
					 void * dst, 
					 uint_t length, 
					 void * priv_data) 
{
    struct generic_internal *state = (struct generic_internal *) priv_data;

    uint_t i;

    switch (state->forward_type) { 
	case GENERIC_PHYSICAL:
	    switch (length) {
		case 1:
		    ((uint8_t *)dst)[0] = v3_inb(port);
		    break;
		case 2:
		    ((uint16_t *)dst)[0] = v3_inw(port);
		    break;
		case 4:
		    ((uint32_t *)dst)[0] = v3_indw(port);
		    break;
		default:
		    for (i = 0; i < length; i++) { 
			((uint8_t *)dst)[i] = v3_inb(port);
		    }
	    }
	    return length;
	    break;
#ifdef CONFIG_HOST_DEVICE
	case GENERIC_HOST:
	    if (state->host_dev) { 
		return v3_host_dev_read_io(state->host_dev,port,dst,length);
	    }
	    break;
#endif
	default:
	    PrintError("generic (%s): unknown forwarding type\n", state->name);
	    return -1;
	    break;
    }

    return -1;
}

static int generic_read_port_print_and_passthrough(struct guest_info * core, uint16_t port, void * src, 
						   uint_t length, void * priv_data) {
    uint_t i;
    int rc;

#ifdef CONFIG_DEBUG_GENERIC
    struct generic_internal *state = (struct generic_internal *) priv_data;
#endif

    PrintDebug("generic (%s): reading 0x%x bytes from port 0x%x using %s ...", state->name, length, port,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" :
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");


    rc=generic_read_port_passthrough(core,port,src,length,priv_data);

    PrintDebug(" done ... read 0x");

    for (i = 0; i < rc; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }

    PrintDebug("\n");

    return rc;
}


static int generic_read_port_ignore(struct guest_info * core, uint16_t port, void * src, 
				    uint_t length, void * priv_data) {

    memset((uint8_t *)src, 0, length);

    return length;
}

static int generic_read_port_print_and_ignore(struct guest_info * core, uint16_t port, void * src, 
					      uint_t length, void * priv_data) {
   
#ifdef CONFIG_DEBUG_GENERIC
    struct generic_internal *state = (struct generic_internal *) priv_data;
#endif

    PrintDebug("generic (%s): reading 0x%x bytes from port 0x%x using %s ...", state->name, length, port,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" :
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");


    memset((uint8_t *)src, 0, length);
    PrintDebug(" ignored (return zeroed buffer)\n");

    return length;
}

static int generic_write_port_ignore(struct guest_info * core, uint16_t port, void * src, 
				     uint_t length, void * priv_data) {

    return length;
}

static int generic_write_port_print_and_ignore(struct guest_info * core, uint16_t port, void * src, 
					      uint_t length, void * priv_data) {
    int i;

#ifdef CONFIG_DEBUG_GENERIC
    struct generic_internal *state = (struct generic_internal *) priv_data;
#endif

    PrintDebug("generic (%s): writing 0x%x bytes to port 0x%x using %s ", state->name, length, port,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" :
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");
    
    memset((uint8_t *)src, 0, length);
    PrintDebug(" ignored - data was: 0x");

    for (i = 0; i < length; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }
    
    PrintDebug("\n");

    return length;
}



static int generic_write_mem_passthrough(struct guest_info * core, 
					 addr_t              gpa,
					 void              * src,
					 uint_t              len,
					 void              * priv)
{
    struct vm_device *dev = (struct vm_device *) priv;
    struct generic_internal *state = (struct generic_internal *) dev->private_data;
    
    switch (state->forward_type) { 
	case GENERIC_PHYSICAL:
	    memcpy(V3_VAddr((void*)gpa),src,len);
	    return len;
	    break;
#ifdef CONFIG_HOST_DEVICE
	case GENERIC_HOST:
	    if (state->host_dev) { 
		return v3_host_dev_write_mem(state->host_dev,gpa,src,len);
	    } else {
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("generic (%s): unknown forwarding type\n", state->name);
	    return -1;
	    break;
    }
}

static int generic_write_mem_print_and_passthrough(struct guest_info * core, 
						   addr_t              gpa,
						   void              * src,
						   uint_t              len,
						   void              * priv)
{
#ifdef CONFIG_DEBUG_GENERIC
    struct vm_device *dev = (struct vm_device *) priv;
    struct generic_internal *state = (struct generic_internal *) dev->private_data;
#endif

    PrintDebug("generic (%s): writing %u bytes to GPA 0x%p via %s ... ", state->name,
	       len,(void*)gpa,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" : 
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");
    
    int rc = generic_write_mem_passthrough(core,gpa,src,len,priv);

    PrintDebug("done\n");
    
    return rc;
}

static int generic_write_mem_ignore(struct guest_info * core, 
				    addr_t              gpa,
				    void              * src,
				    uint_t              len,
				    void              * priv)
{
    return len;
}

static int generic_write_mem_print_and_ignore(struct guest_info * core, 
					      addr_t              gpa,
					      void              * src,
					      uint_t              len,
					      void              * priv)
{
#ifdef CONFIG_DEBUG_GENERIC
    struct vm_device *dev = (struct vm_device *) priv;
    struct generic_internal *state = (struct generic_internal *) dev->private_data;
#endif

    PrintDebug("generic (%s): ignoring write of %u bytes to GPA 0x%p via %s", state->name,
	       len,(void*)gpa,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" : 
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");
    
    return len;
}

static int generic_read_mem_passthrough(struct guest_info * core, 
					addr_t              gpa,
					void              * dst,
					uint_t              len,
					void              * priv)
{
    struct vm_device *dev = (struct vm_device *) priv;
    struct generic_internal *state = (struct generic_internal *) dev->private_data;
    
    switch (state->forward_type) { 
	case GENERIC_PHYSICAL:
	    memcpy(dst,V3_VAddr((void*)gpa),len);
	    return len;
	    break;
#ifdef CONFIG_HOST_DEVICE
	case GENERIC_HOST:
	    if (state->host_dev) { 
		return v3_host_dev_read_mem(state->host_dev,gpa,dst,len);
	    } else {
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("generic (%s): unknown forwarding type\n", state->name);
	    break;
    }
    
    return -1;
}

static int generic_read_mem_print_and_passthrough(struct guest_info * core, 
						  addr_t              gpa,
						  void              * dst,
						  uint_t              len,
						  void              * priv)
{
#ifdef CONFIG_DEBUG_GENERIC
    struct vm_device *dev = (struct vm_device *) priv;
    struct generic_internal *state = (struct generic_internal *) dev->private_data;
#endif

    PrintDebug("generic (%s): attempting to read %u bytes from GPA 0x%p via %s ... ", state->name,
	       len,(void*)gpa,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" : 
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");
    
    int rc = generic_read_mem_passthrough(core,gpa,dst,len,priv);

    PrintDebug("done - read %d bytes\n", rc);
    
    return rc;
}

static int generic_read_mem_ignore(struct guest_info * core, 
				   addr_t              gpa,
				   void              * dst,
				   uint_t              len,
				   void              * priv)
{
#ifdef CONFIG_DEBUG_GENERIC
    struct vm_device *dev = (struct vm_device *) priv;
    struct generic_internal *state = (struct generic_internal *) dev->private_data;
#endif

    PrintDebug("generic (%s): ignoring attempt to read %u bytes from GPA 0x%p via %s ... ", state->name,
	       len,(void*)gpa,
	       state->forward_type == GENERIC_PHYSICAL ? "physical" : 
	       state->forward_type == GENERIC_HOST ? "host" : "UNKNOWN");

    memset((uint8_t *)dst, 0, len);

    PrintDebug("returning zeros\n");

    return len;
}


static int generic_read_mem_print_and_ignore(struct guest_info * core, 
					     addr_t              gpa,
					     void              * dst,
					     uint_t              len,
					     void              * priv)
{
    memset((uint8_t *)dst, 0, len);
    return len;
}


static int generic_free(struct generic_internal * state) {
    int i;
    
    PrintDebug("generic (%s): deinit_device\n", state->name);
    
#ifdef CONFIG_HOST_DEVICE
    if (state->host_dev) { 
	v3_host_dev_close(state->host_dev);
	state->host_dev=0;
    }
#endif
    
    // Note that the device manager handles unhooking the I/O ports
    // We need to handle unhooking memory regions    
    for (i=0;i<state->num_mem_hooks;i++) {
	if (v3_unhook_mem(state->dev->vm,V3_MEM_CORE_ANY,state->mem_hook[i])<0) { 
	    PrintError("generic (%s): unable to unhook memory starting at 0x%p\n", state->name,(void*)(state->mem_hook[i]));
	    return -1;
	}
    }
	     
    V3_Free(state);
    return 0;
}





static struct v3_device_ops dev_ops = { 
    .free = (int (*)(void *))generic_free, 
};




static int add_port_range(struct vm_device * dev, uint_t start, uint_t end, generic_mode_t mode) {
    uint_t i = 0;

    struct generic_internal *state = (struct generic_internal *) dev->private_data;

    PrintDebug("generic (%s): adding port range 0x%x to 0x%x as %s\n", state->name,
	       start, end, 
	       (mode == GENERIC_PRINT_AND_PASSTHROUGH) ? "print-and-passthrough" : 
	       (mode == GENERIC_PRINT_AND_IGNORE) ? "print-and-ignore" :
	       (mode == GENERIC_PASSTHROUGH) ? "passthrough" :
	       (mode == GENERIC_IGNORE) ? "ignore" : "UNKNOWN");
	
    for (i = start; i <= end; i++) { 
	switch (mode) { 
	    case GENERIC_PRINT_AND_PASSTHROUGH:
		if (v3_dev_hook_io(dev, i, 
				   &generic_read_port_print_and_passthrough, 
				   &generic_write_port_print_and_passthrough) == -1) { 
		    PrintError("generic (%s): can't hook port 0x%x (already hooked?)\n", state->name, i);
		    return -1;
		}
		break;
		
	    case GENERIC_PRINT_AND_IGNORE:
		if (v3_dev_hook_io(dev, i, 
				   &generic_read_port_print_and_ignore, 
				   &generic_write_port_print_and_ignore) == -1) { 
		    PrintError("generic (%s): can't hook port 0x%x (already hooked?)\n", state->name, i);
		    return -1;
		}
		break;
	    case GENERIC_PASSTHROUGH:
		if (v3_dev_hook_io(dev, i, 
				   &generic_read_port_passthrough, 
				   &generic_write_port_passthrough) == -1) { 
		    PrintError("generic (%s): can't hook port 0x%x (already hooked?)\n", state->name, i);
		    return -1;
		}
		break;
	    case  GENERIC_IGNORE:
		if (v3_dev_hook_io(dev, i, 
				   &generic_read_port_ignore, 
				   &generic_write_port_ignore) == -1) { 
		    PrintError("generic (%s): can't hook port 0x%x (already hooked?)\n", state->name, i);
		    return -1;
		}
		break;
	    default:
		PrintError("generic (%s): huh?\n", state->name);
		break;
	}
    }
    
    return 0;
}


static int add_mem_range(struct vm_device * dev, addr_t start, addr_t end, generic_mode_t mode) {

    struct generic_internal *state = (struct generic_internal *) dev->private_data;

    PrintDebug("generic (%s): adding memory range 0x%p to 0x%p as %s\n", state->name,
	       (void*)start, (void*)end, 
	       (mode == GENERIC_PRINT_AND_PASSTHROUGH) ? "print-and-passthrough" : 
	       (mode == GENERIC_PRINT_AND_IGNORE) ? "print-and-ignore" :
	       (mode == GENERIC_PASSTHROUGH) ? "passthrough" :
	       (mode == GENERIC_IGNORE) ? "ignore" : "UNKNOWN");
	
    switch (mode) { 
	case GENERIC_PRINT_AND_PASSTHROUGH:
	    if (v3_hook_full_mem(dev->vm, V3_MEM_CORE_ANY, start, end+1, 
				 &generic_read_mem_print_and_passthrough, 
				 &generic_write_mem_print_and_passthrough, dev) == -1) { 
		PrintError("generic (%s): can't hook memory region 0x%p to 0x%p\n", state->name,(void*)start,(void*)end);
		return -1;
	    }
	    break;
	    
	case GENERIC_PRINT_AND_IGNORE:
	    if (v3_hook_full_mem(dev->vm, V3_MEM_CORE_ANY, start, end+1, 
				 &generic_read_mem_print_and_ignore, 
				 &generic_write_mem_print_and_ignore, dev) == -1) { 
		PrintError("generic (%s): can't hook memory region 0x%p to 0x%p\n", state->name,(void*)start,(void*)end);
		return -1;
	    }
	    break;

	case GENERIC_PASSTHROUGH:
	    if (v3_hook_full_mem(dev->vm, V3_MEM_CORE_ANY, start, end+1, 
				 &generic_read_mem_passthrough, 
				 &generic_write_mem_passthrough, dev) == -1) { 
		PrintError("generic (%s): can't hook memory region 0x%p to 0x%p\n", state->name,(void*)start,(void*)end);
		return -1;
	    }
	    break;

	case  GENERIC_IGNORE:
	    if (v3_hook_full_mem(dev->vm, V3_MEM_CORE_ANY, start, end+1, 
				 &generic_read_mem_ignore, 
				 &generic_write_mem_ignore, dev) == -1) { 
		PrintError("generic (%s): can't hook memory region 0x%p to 0x%p\n", state->name,(void*)start,(void*)end);
		return -1;
	    }
	    break;
	default:
	    PrintError("generic (%s): huh?\n",state->name);
	    break;
    }

    return 0;
}



/*
   The device can be used to forward to the underlying physical device 
   or to a host device that has a given url.   Both memory and ports can be forwarded as

        GENERIC_PASSTHROUGH => send writes and reads to physical device or host
        GENERIC_PRINT_AND_PASSTHROUGH => also print what it's doing

        GENERIC_IGNORE => ignore writes and reads
        GENERIC_PRINT_AND_PASSTHROUGH => also print what it's doing


	The purpose of the "PRINT" variants is to make it easy to spy on
	device interactions (although you will not see DMA or interrupts)


   <device class="generic" id="my_id" 
         empty | forward="physical_device" or forward="host_device" host_device="url">

  (empty implies physical_dev)

     <ports>
         <start>portno1</start>
         <end>portno2</end>   => portno1 through portno2 (inclusive)
         <mode>PRINT_AND_PASSTHROUGH</mode>  (as above)
     </ports>

     <memory>
         <start>gpa1</start>
         <end>gpa2</end>     => memory addreses gpa1 through gpa2 (inclusive); page granularity
         <mode> ... as above </mode>
     </memory>

*/

static int generic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct generic_internal * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * forward = v3_cfg_val(cfg, "forward");
#ifdef CONFIG_HOST_DEVICE
    char * host_dev = v3_cfg_val(cfg, "hostdev");
#endif
    v3_cfg_tree_t * port_cfg = v3_cfg_subtree(cfg, "ports");
    v3_cfg_tree_t * mem_cfg = v3_cfg_subtree(cfg, "memory");


    state = (struct generic_internal *)V3_Malloc(sizeof(struct generic_internal));

    if (state == NULL) {
	PrintError("generic (%s): could not allocate generic state\n",dev_id);
	return -1;
    }
    
    memset(state, 0, sizeof(struct generic_internal));
    strncpy(state->name,dev_id,MAX_NAME);

    if (!forward) { 
	state->forward_type=GENERIC_PHYSICAL;
    } else {
	if (!strcasecmp(forward,"physical_device")) { 
	    state->forward_type=GENERIC_PHYSICAL;
	} else if (!strcasecmp(forward,"host_device")) { 
#ifdef CONFIG_HOST_DEVICE
	    state->forward_type=GENERIC_HOST;
#else
	    PrintError("generic (%s): cannot configure host device since host device support is not built in\n", state->name);
	    V3_Free(state);
	    return -1;
#endif
	} else {
	    PrintError("generic (%s): unknown forwarding type \"%s\"\n", state->name, forward);
	    V3_Free(state);
	    return -1;
	}
    }
    
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("generic: could not attach device %s\n", state->name);
	V3_Free(state);
	return -1;
    }

    state->dev=dev;


#ifdef CONFIG_HOST_DEVICE
    if (state->forward_type==GENERIC_HOST) { 
	if (!host_dev) { 
	    PrintError("generic (%s): host forwarding requested, but no host device given\n", state->name);
	    v3_remove_device(dev);
	    return -1;
	} else {
	    state->host_dev = v3_host_dev_open(host_dev,V3_BUS_CLASS_DIRECT,dev);
	    if (!(state->host_dev)) { 
		PrintError("generic (%s): unable to open host device \"%s\"\n", state->name,host_dev);
		v3_remove_device(dev);
		return -1;
	    } else {
		PrintDebug("generic (%s): successfully attached host device \"%s\"\n", state->name,host_dev);
	    }
	}
    }
#endif

    PrintDebug("generic (%s): init_device\n", state->name);

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
	} else if (strcasecmp(mode_str, "passthrough") == 0) {
	    mode = GENERIC_PASSTHROUGH;
	} else if (strcasecmp(mode_str, "ignore") == 0) {
	    mode = GENERIC_IGNORE;
	} else {
	    PrintError("generic (%s): invalid mode %s in adding ports\n", state->name, mode_str);
	    v3_remove_device(dev);
	    return -1;
	}
	
	
	if (add_port_range(dev, start, end, mode) == -1) {
	    PrintError("generic (%s): could not add port range 0x%x to 0x%x\n", state->name, start, end);
	    v3_remove_device(dev);
	    return -1;
	}

	port_cfg = v3_cfg_next_branch(port_cfg);
    }

    // scan memory list....
    while (mem_cfg) {
	addr_t  start = atox(v3_cfg_val(mem_cfg, "start"));
	addr_t end = atox(v3_cfg_val(mem_cfg, "end"));
	char * mode_str = v3_cfg_val(mem_cfg, "mode");
	generic_mode_t mode = GENERIC_IGNORE;

	if (strcasecmp(mode_str, "print_and_ignore") == 0) {
	    mode = GENERIC_PRINT_AND_IGNORE;
	} else if (strcasecmp(mode_str, "print_and_passthrough") == 0) {
	    mode = GENERIC_PRINT_AND_PASSTHROUGH;
	} else if (strcasecmp(mode_str, "passthrough") == 0) {
	    mode = GENERIC_PASSTHROUGH;
	} else if (strcasecmp(mode_str, "ignore") == 0) {
	    mode = GENERIC_IGNORE;
	} else {
	    PrintError("generic (%s): invalid mode %s for adding memory\n", state->name, mode_str);
	    v3_remove_device(dev);
	    return -1;
	}

	if (state->num_mem_hooks>=MAX_MEM_HOOKS) { 
	    PrintError("generic (%s): cannot add another memory hook (increase MAX_MEM_HOOKS)\n", state->name);
	    v3_remove_device(dev);
	    return -1;
	}
	
	if (add_mem_range(dev, start, end, mode) == -1) {
	    PrintError("generic (%s): could not add memory range 0x%p to 0x%p\n", state->name, (void*)start, (void*)end);
	    v3_remove_device(dev);
	    return -1;
	}
	
	state->mem_hook[state->num_mem_hooks] = start;
	state->num_mem_hooks++;

	mem_cfg = v3_cfg_next_branch(port_cfg);
    }
    
    PrintDebug("generic (%s): initialization complete\n", state->name);

    return 0;
}

device_register("GENERIC", generic_init)
