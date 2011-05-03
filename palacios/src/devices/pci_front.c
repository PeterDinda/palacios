/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Authors: 
 *    Peter Dinda <pdinda@northwestern.edu>    (PCI front device forwarding to host dev interface)
 *    Jack Lange <jarusl@cs.northwestern.edu>  (original PCI passthrough to physical hardware)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


/* 
  This is front-end PCI device intended to be used together with the
  host device interface and a *virtual* PCI device implementation in
  the host OS.  It makes it possible to project such a virtual device
  into the guest as a PCI device.  It's based on the PCI passthrough
  device, which projects *physical* PCI devices into the guest.

  If you need to project a non-PCI host-based virtual or physical
  device into the guest, you should use the generic device.

*/

/* 
 * The basic idea is that we do not change the hardware PCI configuration
 * Instead we modify the guest environment to map onto the physical configuration
 * 
 * The pci subsystem handles most of the configuration space, except for the bar registers.
 * We handle them here, by either letting them go directly to hardware or remapping through virtual hooks
 * 
 * Memory Bars are always remapped via the shadow map, 
 * IO Bars are selectively remapped through hooks if the guest changes them 
 */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vm_guest.h> 
#include <palacios/vmm_symspy.h>

#include <devices/pci.h>
#include <devices/pci_types.h>

#include <interfaces/vmm_host_dev.h>


#ifndef V3_CONFIG_DEBUG_PCI_FRONT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


// Our own address in PCI-land
union pci_addr_reg {
    uint32_t value;
    struct {
	uint_t rsvd1   : 2;
	uint_t reg     : 6;
	uint_t func    : 3;
	uint_t dev     : 5;
	uint_t bus     : 8;
	uint_t rsvd2   : 7;
	uint_t enable  : 1;
    } __attribute__((packed));
} __attribute__((packed));


// identical to PCI passthrough device
typedef enum { PT_BAR_NONE,
	       PT_BAR_IO, 
	       PT_BAR_MEM32, 
	       PT_BAR_MEM24, 
	       PT_BAR_MEM64_LO, 
	       PT_BAR_MEM64_HI,
	       PT_EXP_ROM } pt_bar_type_t;

// identical to PCI passthrough device
struct pt_bar {
    uint32_t size;
    pt_bar_type_t type;

    /*  We store 64 bit memory bar addresses in the high BAR
     *  because they are the last to be updated
     *  This means that the addr field must be 64 bits
     */
    uint64_t addr; 

    uint32_t val;
};




struct pci_front_internal {
    // this is our local cache of what the host device has
    union {
	uint8_t config_space[256];
	struct pci_config_header real_hdr;
    } __attribute__((packed));
    
    // We do need a representation of the bars
    // since we need to be made aware when they are written
    // so that we can change the hooks.
    //
    // We assume here that the PCI subsystem, on a bar write
    // will first send us a config_update, which we forward to
    // the host dev.   Then it will send us a bar update
    // which we will use to rehook the device
    //
    struct pt_bar bars[6];      // our bars (for update purposes)
    //
    // Currently unsupported
    //
    //struct pt_bar exp_rom;      // and exp ram areas of the config space, above
     
    struct vm_device  *pci_bus;  // what bus we are attached to
    struct pci_device *pci_dev;  // our representation as a registered PCI device

    union pci_addr_reg pci_addr; // our pci address

    char name[32];

    v3_host_dev_t     host_dev;  // the actual implementation
};



/*
static int push_config(struct pci_front_internal *state, uint8_t *config)
{
    if (v3_host_dev_config_write(state->host_dev, 0, config, 256) != 256) { 
	return -1;
    } else {
	return 0;
    }
}
*/

static int pull_config(struct pci_front_internal *state, uint8_t *config)
{
    if (v3_host_dev_read_config(state->host_dev, 0, config, 256) != 256) { 
	return -1;
    } else {
	return 0;
    }
}


static int pci_front_read_mem(struct guest_info * core, 
			      addr_t              gpa,
			      void              * dst,
			      uint_t              len,
			      void              * priv)
{
    int i;
    int rc;
    struct vm_device *dev = (struct vm_device *) priv;
    struct pci_front_internal *state = (struct pci_front_internal *) dev->private_data;

    PrintDebug("pci_front (%s): reading 0x%x bytes from gpa 0x%p from host dev 0x%p ...",
	       state->name, len, (void*)gpa, state->host_dev);

    rc = v3_host_dev_read_mem(state->host_dev, gpa, dst, len);

    PrintDebug(" done ... read %d bytes: 0x", rc);

    for (i = 0; i < rc; i++) { 
	PrintDebug("%x", ((uint8_t *)dst)[i]);
    }

    PrintDebug("\n");

    return rc;
}

static int pci_front_write_mem(struct guest_info * core, 
			       addr_t              gpa,
			       void              * src,
			       uint_t              len,
			       void              * priv)
{
    int i;
    int rc;
    struct vm_device *dev = (struct vm_device *) priv;
    struct pci_front_internal *state = (struct pci_front_internal *) dev->private_data;

    PrintDebug("pci_front (%s): writing 0x%x bytes to gpa 0x%p to host dev 0x%p bytes=0x",
	       state->name, len, (void*)gpa, state->host_dev);

    for (i = 0; i < len; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }

    rc = v3_host_dev_write_mem(state->host_dev, gpa, src, len);

    PrintDebug(" %d bytes written\n",rc);
    
    return rc;
}


static int pci_front_read_port(struct guest_info * core, 
			       uint16_t            port, 
			       void              * dst, 
			       uint_t              len, 
			       void              * priv_data) 
{
    int i;
    struct pci_front_internal *state = (struct pci_front_internal *) priv_data;
    
    PrintDebug("pci_front (%s): reading 0x%x bytes from port 0x%x from host dev 0x%p ...",
	       state->name, len, port, state->host_dev);

    int rc = v3_host_dev_read_io(state->host_dev, port, dst, len);
    
    PrintDebug(" done ... read %d bytes: 0x", rc);

    for (i = 0; i < rc; i++) { 
	PrintDebug("%x", ((uint8_t *)dst)[i]);
    }

    PrintDebug("\n");

    return rc;
    
}

static int pci_front_write_port(struct guest_info * core, 
				uint16_t            port, 
				void              * src, 
				uint_t              len, 
				void              * priv_data) 
{
    int i;
    struct pci_front_internal *state = (struct pci_front_internal *) priv_data;
    
    PrintDebug("pci_front (%s): writing 0x%x bytes to port 0x%x to host dev 0x%p bytes=0x",
	       state->name, len, port, state->host_dev);

    for (i = 0; i < len; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }

    int rc = v3_host_dev_write_io(state->host_dev, port, src, len);

    PrintDebug(" %d bytes written\n",rc);
    
    return rc;
}



//
// This is called at registration time for the device
// 
// We assume that someone has called pull_config to get a local
// copy of the config data from the host device by this point
//
static int pci_bar_init(int bar_num, uint32_t * dst, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pci_front_internal * state = (struct pci_front_internal *)(dev->private_data);


    const uint32_t bar_base_reg = 4;   // offset in 32bit words to skip to the first bar

    union pci_addr_reg pci_addr = {state->pci_addr.value};  // my address

    uint32_t bar_val = 0;
    uint32_t max_val = 0;

    struct pt_bar * pbar = &(state->bars[bar_num]);

    pci_addr.reg = bar_base_reg + bar_num;

    PrintDebug("pci_front (%s): pci_bar_init: PCI Address = 0x%x\n", state->name, pci_addr.value);

    // This assumees that pull_config() has been previously called and 
    // we have a local copy of the host device's configuration space
    bar_val = *((uint32_t*)(&(state->config_space[(bar_base_reg+bar_num)*4])));

    // Now let's set our copy of the relevant bar accordingly
    pbar->val = bar_val; 
    
    // Now we will configure the hooks relevant to this bar

    // We preset this type when we encounter a MEM64 Low BAR
    // This is a 64 bit memory region that we turn into a memory hook
    if (pbar->type == PT_BAR_MEM64_HI) {
	struct pt_bar * lo_pbar = &(state->bars[bar_num - 1]);

	max_val = PCI_MEM64_MASK_HI;

	pbar->size += lo_pbar->size;

	PrintDebug("pci_front (%s): pci_bar_init: Adding 64 bit PCI mem region: start=0x%p, end=0x%p as a full hook\n",
		   state->name, 
		   (void *)(addr_t)pbar->addr, 
		   (void *)(addr_t)(pbar->addr + pbar->size));

	if (v3_hook_full_mem(dev->vm,
			     V3_MEM_CORE_ANY,
			     pbar->addr,
			     pbar->addr+pbar->size-1,
			     pci_front_read_mem,
			     pci_front_write_mem,
			     dev)<0) { 
	    
	    PrintError("pci_front (%s): pci_bar_init: failed to hook 64 bit region (0x%p, 0x%p)\n",
		       state->name, 
		       (void *)(addr_t)pbar->addr,
		       (void *)(addr_t)(pbar->addr + pbar->size - 1));
	    return -1;
	}

    } else if ((bar_val & 0x3) == 0x1) {
	// This an I/O port region which we will turn into a range of hooks

	int i = 0;

	pbar->type = PT_BAR_IO;
	pbar->addr = PCI_IO_BASE(bar_val);

	max_val = bar_val | PCI_IO_MASK;

	pbar->size = (uint16_t)~PCI_IO_BASE(max_val) + 1;

	
	PrintDebug("pci_front (%s): pci_bar_init: hooking ports 0x%x through 0x%x\n",
		   state->name, (uint32_t)pbar->addr, (uint32_t)pbar->addr + pbar->size - 1);

	for (i = 0; i < pbar->size; i++) {
	    if (v3_dev_hook_io(dev,
			       pbar->addr + i, 
			       pci_front_read_port,
			       pci_front_write_port)<0) {
		PrintError("pci_front (%s): pci_bar_init: unabled to hook I/O port 0x%x\n",state->name, (unsigned)(pbar->addr+i));
		return -1;
	    }
	}

    } else {

	// might be a 32 bit memory region or an empty bar

	max_val = bar_val | PCI_MEM_MASK;

	if (max_val == 0) {
	    // nothing, so just ignore it
	    pbar->type = PT_BAR_NONE;
	} else {

	    // memory region - hook it

	    if ((bar_val & 0x6) == 0x0) {
		// 32 bit memory region

		pbar->type = PT_BAR_MEM32;
		pbar->addr = PCI_MEM32_BASE(bar_val);
		pbar->size = ~PCI_MEM32_BASE(max_val) + 1;

		PrintDebug("pci_front (%s): pci_init_bar: adding 32 bit PCI mem region: start=0x%p, end=0x%p\n",
			   state->name, 
			   (void *)(addr_t)pbar->addr, 
			   (void *)(addr_t)(pbar->addr + pbar->size));

		if (v3_hook_full_mem(dev->vm, 
				     V3_MEM_CORE_ANY,
				     pbar->addr,
				     pbar->addr+pbar->size-1,
				     pci_front_read_mem,
				     pci_front_write_mem,
				     dev) < 0 ) { 
		    PrintError("pci_front (%s): pci_init_bar: unable to hook 32 bit memory region 0x%p to 0x%p\n",
			       state->name, (void*)(pbar->addr), (void*)(pbar->addr+pbar->size-1));
		    return -1;
		}

	    } else if ((bar_val & 0x6) == 0x2) {

		// 24 bit memory region

		pbar->type = PT_BAR_MEM24;
		pbar->addr = PCI_MEM24_BASE(bar_val);
		pbar->size = ~PCI_MEM24_BASE(max_val) + 1;


		if (v3_hook_full_mem(dev->vm, 
				     V3_MEM_CORE_ANY,
				     pbar->addr,
				     pbar->addr+pbar->size-1,
				     pci_front_read_mem,
				     pci_front_write_mem,
				     dev) < 0 ) { 
		    PrintError("pci_front (%s): pci_init_bar: unable to hook 24 bit memory region 0x%p to 0x%p\n",
			       state->name, (void*)(pbar->addr), (void*)(pbar->addr+pbar->size-1));
		    return -1;
		}

	    } else if ((bar_val & 0x6) == 0x4) {
		
		// partial update of a 64 bit region, no hook done yet

		struct pt_bar * hi_pbar = &(state->bars[bar_num + 1]);

		pbar->type = PT_BAR_MEM64_LO;
		hi_pbar->type = PT_BAR_MEM64_HI;

		// Set the low bits, only for temporary storage until we calculate the high BAR
		pbar->addr = PCI_MEM64_BASE_LO(bar_val);
		pbar->size = ~PCI_MEM64_BASE_LO(max_val) + 1;

		PrintDebug("pci_front (%s): pci_bar_init: partial 64 bit update\n",state->name);

	    } else {
		PrintError("pci_front (%s): pci_bar_init: invalid memory bar type\n",state->name);
		return -1;
	    }

	}
    }



    // Update the pci subsystem versions
    *dst = bar_val;

    return 0;
}


//
// If the guest modifies a BAR, we expect that pci.c will do the following,
// in this order
//
//    1. notify us via the config_update callback, which we will feed back
//       to the host device
//    2. notify us of the bar change via the following callback 
//
// This callback will unhook as needed for the old bar value and rehook
// as needed for the new bar value
//
static int pci_bar_write(int bar_num, uint32_t * src, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pci_front_internal * state = (struct pci_front_internal *)dev->private_data;
    
    struct pt_bar * pbar = &(state->bars[bar_num]);

    PrintDebug("pci_front (%s): bar update: bar_num=%d, src=0x%x\n", state->name, bar_num, *src);
    PrintDebug("pci_front (%s): the current bar has size=%u, type=%d, addr=%p, val=0x%x\n",
	       state->name, pbar->size, pbar->type, (void *)(addr_t)pbar->addr, pbar->val);



    if (pbar->type == PT_BAR_NONE) {
	PrintDebug("pci_front (%s): bar update is to empty bar - ignored\n",state->name);
	return 0;
    } else if (pbar->type == PT_BAR_IO) {
	int i = 0;

	// unhook old ports
	PrintDebug("pci_front (%s): unhooking I/O ports 0x%x through 0x%x\n", 
		   state->name, 
		   (unsigned)(pbar->addr), (unsigned)(pbar->addr+pbar->size-1));
	for (i = 0; i < pbar->size; i++) {
	    if (v3_dev_unhook_io(dev, pbar->addr + i) == -1) {
		PrintError("pci_front (%s): could not unhook previously hooked port.... 0x%x\n", 
			   state->name, 
			   (uint32_t)pbar->addr + i);
		return -1;
	    }
	}

	PrintDebug("pci_front (%s): setting I/O Port range size=%d\n", state->name, pbar->size);

	// 
	// Not clear if this cooking is needed... why not trust
	// the write?  Who cares if it wants to suddenly hook more ports?
	// 

	// clear the low bits to match the size
	*src &= ~(pbar->size - 1);

	// Set reserved bits
	*src |= (pbar->val & ~PCI_IO_MASK);

	pbar->addr = PCI_IO_BASE(*src);	

	PrintDebug("pci_front (%s): cooked src=0x%x\n", state->name, *src);

	PrintDebug("pci_front (%s): rehooking I/O ports 0x%x through 0x%x\n",
		   state->name, (unsigned)(pbar->addr), (unsigned)(pbar->addr+pbar->size-1));

	for (i = 0; i < pbar->size; i++) {
	    if (v3_dev_hook_io(dev,
			       pbar->addr + i, 
			       pci_front_read_port, 
			       pci_front_write_port)<0) { 
		PrintError("pci_front (%s): unable to rehook port 0x%x\n",state->name, (unsigned)(pbar->addr+i));
		return -1;
	    }
	}

    } else if (pbar->type == PT_BAR_MEM32) {

	if (v3_unhook_mem(dev->vm,V3_MEM_CORE_ANY,pbar->addr)<0) { 
	    PrintError("pci_front (%s): unable to unhook 32 bit memory region starting at 0x%p\n", 
		       state->name, (void*)(pbar->addr));
	    return -1;
	}

	// Again, not sure I need to do this cooking...

	// clear the low bits to match the size
	*src &= ~(pbar->size - 1);

	// Set reserved bits
	*src |= (pbar->val & ~PCI_MEM_MASK);

	PrintDebug("pci_front (%s): cooked src=0x%x\n", state->name, *src);

	pbar->addr = PCI_MEM32_BASE(*src);

	PrintDebug("pci_front (%s): rehooking 32 bit memory region 0x%p through 0x%p\n",
		   state->name, (void*)(pbar->addr), (void*)(pbar->addr + pbar->size - 1));
		   
	if (v3_hook_full_mem(dev->vm,
			     V3_MEM_CORE_ANY,
			     pbar->addr,
			     pbar->addr+pbar->size-1,
			     pci_front_read_mem,
			     pci_front_write_mem,
			     dev)<0) { 
	    PrintError("pci_front (%s): unable to rehook 32 bit memory region 0x%p through 0x%p\n",
		       state->name, (void*)(pbar->addr), (void*)(pbar->addr + pbar->size - 1));
	    return -1;
	}

    } else if (pbar->type == PT_BAR_MEM64_LO) {
	// We only store the written values here, the actual reconfig comes when the high BAR is updated

	// clear the low bits to match the size
	*src &= ~(pbar->size - 1);

	// Set reserved bits
	*src |= (pbar->val & ~PCI_MEM_MASK);

	// Temp storage, used when hi bar is written
	pbar->addr = PCI_MEM64_BASE_LO(*src);

	PrintDebug("pci_front (%s): handled partial update for 64 bit memory region\n",state->name);

    } else if (pbar->type == PT_BAR_MEM64_HI) {
	struct pt_bar * lo_vbar = &(state->bars[bar_num - 1]);

	if (v3_unhook_mem(dev->vm,V3_MEM_CORE_ANY,pbar->addr)<0) { 
	    PrintError("pci_front (%s): unable to unhook 64 bit memory region starting at 0x%p\n", 
		       state->name, (void*)(pbar->addr));
	    return -1;
	}

	
	// We don't set size, because we assume region is less than 4GB

	// Set reserved bits
	*src |= (pbar->val & ~PCI_MEM64_MASK_HI);

	pbar->addr = PCI_MEM64_BASE_HI(*src);
	pbar->addr <<= 32;
	pbar->addr += lo_vbar->addr;

	PrintDebug("pci_front (%s): rehooking 64 bit memory region 0x%p through 0x%p\n",
		   state->name, (void*)(pbar->addr), (void*)(pbar->addr + pbar->size - 1));
		   
	if (v3_hook_full_mem(dev->vm,
			     V3_MEM_CORE_ANY,
			     pbar->addr,
			     pbar->addr+pbar->size-1,
			     pci_front_read_mem,
			     pci_front_write_mem,
			     dev)<0) { 
	    PrintError("pci_front (%s): unable to rehook 64 bit memory region 0x%p through 0x%p\n",
		       state->name, (void*)(pbar->addr), (void*)(pbar->addr + pbar->size - 1));
	    return -1;
	}
	
    } else {
	PrintError("pci_front (%s): unhandled PCI bar type %d\n", state->name, pbar->type);
	return -1;
    }

    pbar->val = *src;
    
    return 0;
}


static int pci_front_config_update(uint_t reg_num, void * src, uint_t length, void * private_data) 
{
    int i;
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pci_front_internal * state = (struct pci_front_internal *)dev->private_data;
    union pci_addr_reg pci_addr = {state->pci_addr.value};
    
    pci_addr.reg = reg_num >> 2;

    PrintDebug("pci_front (%s): configuration update: writing 0x%x bytes at offset 0x%x to host device 0x%p, bytes=0x",
	       state->name, length, pci_addr.value, state->host_dev);
    
    for (i = 0; i < length; i++) { 
	PrintDebug("%x", ((uint8_t *)src)[i]);
    }

    PrintDebug("\n");

    if (v3_host_dev_write_config(state->host_dev,
				 pci_addr.value,
				 src,
				 length) != length) { 
	PrintError("pci_front (%s): configuration update: unable to write all bytes\n",state->name);
	return -1;
    }


    return 0;
}


static int unhook_all_mem(struct pci_front_internal *state)
{
    int bar_num;
    struct vm_device *bus = state->pci_bus;


    for (bar_num=0;bar_num<6;bar_num++) { 
	struct pt_bar * pbar = &(state->bars[bar_num]);

	PrintDebug("pci_front (%s): unhooking for bar %d\n", state->name, bar_num);

	if (pbar->type == PT_BAR_MEM32) {
	    if (v3_unhook_mem(bus->vm,V3_MEM_CORE_ANY,pbar->addr)<0) { 
		PrintError("pci_front (%s): unable to unhook 32 bit memory region starting at 0x%p\n", 
			   state->name, (void*)(pbar->addr));
		return -1;
	    }
	} else  if (pbar->type == PT_BAR_MEM64_HI) {

	    if (v3_unhook_mem(bus->vm,V3_MEM_CORE_ANY,pbar->addr)<0) { 
		PrintError("pci_front (%s): unable to unhook 64 bit memory region starting at 0x%p\n", 
			   state->name, (void*)(pbar->addr));
		return -1;
	    }
	}
    }
    
    return 0;
}



static int setup_virt_pci_dev(struct v3_vm_info * vm_info, struct vm_device * dev) 
{
    struct pci_front_internal * state = (struct pci_front_internal *)dev->private_data;
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int bus_num = 0;
    int i;

    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_PASSTHROUGH;
	bars[i].private_data = dev;
	bars[i].bar_init = pci_bar_init;
	bars[i].bar_write = pci_bar_write;
    }

    pci_dev = v3_pci_register_device(state->pci_bus,
				     PCI_STD_DEVICE,
				     bus_num, -1, 0, 
				     state->name, bars,
				     pci_front_config_update,
				     NULL,      // no support for command updates
				     NULL,      // no support for expansion roms              
				     dev);


    state->pci_dev = pci_dev;


    // EXPANSION ROMS CURRENTLY UNSUPPORTED

    // COMMANDS CURRENTLY UNSUPPORTED

    return 0;
}



//
// Note: potential bug:  not clear what pointer I get here
//
static int pci_front_free(struct pci_front_internal *state)
{

    if (unhook_all_mem(state)<0) { 
	return -1;
    }

    // the device manager will unhook the i/o ports for us

    if (state->host_dev) { 
	v3_host_dev_close(state->host_dev);
	state->host_dev=0;
    }


    V3_Free(state);

    PrintDebug("pci_front (%s): freed\n",state->name);

    return 0;
}


static struct v3_device_ops dev_ops = {
//
// Note: potential bug:  not clear what pointer I get here
//
    .free = (int (*)(void*))pci_front_free,
};







static int pci_front_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
    struct vm_device * dev;
    struct vm_device * bus;
    struct pci_front_internal *state;
    char *dev_id;
    char *bus_id;
    char *url;

    
    if (!(dev_id = v3_cfg_val(cfg, "ID"))) { 
	PrintError("pci_front: no id  given!\n");
	return -1;
    }
    
    if (!(bus_id = v3_cfg_val(cfg, "bus"))) { 
	PrintError("pci_front (%s): no bus given!\n",dev_id);
	return -1;
    }
    
    if (!(url = v3_cfg_val(cfg, "hostdev"))) { 
	PrintError("pci_front (%s): no host device url given!\n",dev_id);
	return -1;
    }
    
    if (!(bus = v3_find_dev(vm,bus_id))) { 
	PrintError("pci_front (%s): cannot attach to bus %s\n",dev_id,bus_id);
	return -1;
    }
    
    if (!(state = V3_Malloc(sizeof(struct pci_front_internal)))) { 
	PrintError("pci_front (%s): cannot allocate state for device\n",dev_id);
	return -1;
    }
    
    memset(state, 0, sizeof(struct pci_front_internal));
    
    state->pci_bus = bus;
    strncpy(state->name, dev_id, 32);
    
    if (!(dev = v3_add_device(vm, dev_id, &dev_ops, state))) { 
	PrintError("pci_front (%s): unable to add device\n",state->name);
	return -1;
    }
    
    if (!(state->host_dev=v3_host_dev_open(url,V3_BUS_CLASS_PCI,dev,vm))) { 
	PrintError("pci_front (%s): unable to attach to host device %s\n",state->name, url);
	v3_remove_device(dev);
	return -1;
    }
    
    // fetch config space from the host
    if (pull_config(state,state->config_space)) { 
	PrintError("pci_front (%s): cannot initially configure device\n",state->name);
	v3_remove_device(dev);
	return -1;
    }

    // setup virtual device for now
    if (setup_virt_pci_dev(vm,dev)<0) { 
	PrintError("pci_front (%s): cannot set up virtual pci device\n", state->name);
	v3_remove_device(dev);
	return -1;
    }

    // We do not need to hook anything here since pci will call
    // us back via the bar_init functions

    PrintDebug("pci_front (%s): inited and ready to be Potemkinized\n",state->name);

    return 0;

}


device_register("PCI_FRONT", pci_front_init)
