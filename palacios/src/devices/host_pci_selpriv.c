/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Peter Dinda <pdinda@northwestern.edu> (refactoring and selpriv feature)
 * Copyright (c) 2012, Jack Lange <jacklange@cs.pitt.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Authors: Jack Lange <jacklange@cs.pitt.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (selective privilege and refactor)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


/* 
   This is the generic passthrough PCI virtual device which allows
   access to the underlying device to be dynamically provided and
   revoked.  It is a variant of the host_pci.c device, which is
   "always on" (access permitted).
*/

/* 
 * The basic idea is that we do not change the hardware PCI
 * configuration Instead we modify the guest environment to map onto
 * the physical configuration
 * 
 * The pci subsystem handles most of the configuration space, except
 * for the bar registers.  We handle them here, by either letting them
 * go directly to hardware or remapping through virtual hooks
 * 
 * Memory Bars are remapped via the shadow map on privileged operation
 * IO Bars are selectively remapped through hooks if the guest changes
 * them
 *   
 * By default, this device is fully privileged at all times, meaning
 * that passthrough to the underlying host side implementation is
 * always active.  In this mode, it should behave the same has the
 * host_pci.c device.
 *
 * The device can also be instantiated with the options
 * selective_privilege=yes and privilege_extension=name In this
 * "selective privilege" mode of operation, the device starts without
 * privilege, and the extension can dynamically switch between
 * privileged and unprivileged operation.  In unprivileged operation,
 * the bars are remapped to stubs, and interrupt delivery is disabled.
 */


#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vm_guest.h> // must include this to avoid dependency issue
#include <palacios/vmm_symspy.h>

#include <devices/pci.h>
#include <devices/pci_types.h>
#include <interfaces/host_pci.h>

#include <palacios/vmm_types.h>

#include <gears/privilege.h>


#ifndef V3_CONFIG_DEBUG_HOST_PCI_SELPRIV
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


// set this to one if you want to test the device
// in "always on" mode for compatability with host_pci.c
#define TEST_NOSELPRIV 0


#define PCI_BUS_MAX  7
#define PCI_DEV_MAX 32
#define PCI_FN_MAX   7

#define PCI_DEVICE 0x0
#define PCI_PCI_BRIDGE 0x1
#define PCI_CARDBUS_BRIDGE 0x2

#define PCI_HDR_SIZE 256


typedef enum { UNPRIVILEGED=0, PRIVILEGED} priv_state_t;
typedef enum { UNMAPPED=0, STUB, PHYSICAL}  bar_state_t;

struct host_pci_state {
    // This holds the description of the host PCI device configuration
    struct v3_host_pci_dev * host_dev;


    struct v3_host_pci_bar virt_bars[6];
    struct v3_host_pci_bar virt_exp_rom;
     
    struct vm_device * pci_bus;
    struct pci_device * pci_dev;

    char name[32];

  // The current privilege level of the device
  priv_state_t             priv_state;
  // how each bar is currently mapped
  bar_state_t              bar_state[6];
};



/*
static int pci_exp_rom_init(struct vm_device * dev, struct host_pci_state * state) {
    struct pci_device * pci_dev = state->pci_dev;
    struct v3_host_pci_bar * hrom = &(state->host_dev->exp_rom);


    
    PrintDebug(info->vm_info, info, "Adding 32 bit PCI mem region: start=%p, end=%p\n",
	       (void *)(addr_t)hrom->addr, 
	       (void *)(addr_t)(hrom->addr + hrom->size));

    if (hrom->exp_rom_enabled) {
	// only map shadow memory if the ROM is enabled 

	v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, 
			  hrom->addr, 
			  hrom->addr + hrom->size - 1,
			  hrom->addr);

	// Initially the virtual location matches the physical ones
	memcpy(&(state->virt_exp_rom), hrom, sizeof(struct v3_host_pci_bar));


	PrintDebug(info->vm_info, info, "phys exp_rom: addr=%p, size=%u\n", 
		   (void *)(addr_t)hrom->addr, 
		   hrom->size);


	// Update the pci subsystem versions
	pci_dev->config_header.expansion_rom_address = PCI_EXP_ROM_VAL(hrom->addr, hrom->exp_rom_enabled);
    }



    return 0;
}
*/


static int pt_io_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data) {
    struct v3_host_pci_bar * pbar = (struct v3_host_pci_bar *)priv_data;
    int port_offset = port % pbar->size;

    if (length == 1) {
	*(uint8_t *)dst = v3_inb(pbar->addr + port_offset);
    } else if (length == 2) {
	*(uint16_t *)dst = v3_inw(pbar->addr + port_offset);
    } else if (length == 4) {
	*(uint32_t *)dst = v3_indw(pbar->addr + port_offset);
    } else {
	PrintError(core->vm_info, core, "Invalid PCI passthrough IO Redirection size read\n");
	return -1;
    }

    return length;
}


static int pt_io_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct v3_host_pci_bar * pbar = (struct v3_host_pci_bar *)priv_data;
    int port_offset = port % pbar->size;
    
    if (length == 1) {
	v3_outb(pbar->addr + port_offset, *(uint8_t *)src);
    } else if (length == 2) {
	v3_outw(pbar->addr + port_offset, *(uint16_t *)src);
    } else if (length == 4) {
	v3_outdw(pbar->addr + port_offset, *(uint32_t *)src);
    } else {
	PrintError(core->vm_info, core, "Invalid PCI passthrough IO Redirection size write\n");
	return -1;
    }
    
    return length;

}


static int pt_io_read_stub(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data) {
  PrintDebug(core->vm_info, core, "Ignoring read of %u bytes at port %d\n",length,port);
  memset(dst,0,length);
  return length;
}

static int pt_io_write_stub(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
  PrintDebug(core->vm_info, core, "Ignoring read of %u bytes at port %d\n",length,port);
  return length;
}


static int pt_mem_read_stub(struct guest_info * core, 
			    addr_t guest_addr, 
			    void * dst, 
			    uint_t length, 
			    void * priv_data)
{
  PrintDebug(core->vm_info, core, "Ignoring read of %u bytes at %p\n",length,(void*)guest_addr);

  memset(dst,0,length);
  return length;
}

static int pt_mem_write_stub(struct guest_info * core, 
			     addr_t guest_addr, 
			     void * src, 
			     uint_t length, 
			     void * priv_data)
{
  PrintDebug(core->vm_info, core, "Ignoring write of %u bytes at %p\n",length,(void*)guest_addr);
  return length;
}


static int pci_unmap_bar(struct vm_device        *dev,
			 struct host_pci_state   *state,
			 int                      bar_num)
{
  //  struct v3_host_pci_bar *hbar = &(state->host_dev->bars[bar_num]);
  struct v3_host_pci_bar *vbar = &(state->virt_bars[bar_num]);
  
  if (state->bar_state[bar_num] == UNMAPPED) {
    // this is idempotent, so not an error
    return 0;
  }
  
  if (state->bar_state[bar_num] != PHYSICAL &&
      state->bar_state[bar_num] != STUB) { 
    PrintError(dev->vm, VCORE_NONE, "Unknown bar state %d\n", state->bar_state[bar_num]);
    return -1;
  }

  switch (vbar->type) { 
    
  case PT_BAR_NONE:
    break;
    
  case PT_BAR_IO: {
    int i;
    // unhook old ports - same for both physical and stub
    for (i = 0; i < vbar->size; i++) {
      if (v3_unhook_io_port(dev->vm, vbar->addr + i) == -1) {
	PrintError(dev->vm, VCORE_NONE, "Could not unhook previously hooked port.... %d (0x%x)\n", 
		   (uint32_t)vbar->addr + i, (uint32_t)vbar->addr + i);
	return -1;
      }
    }
  }
    break;
    
  case PT_BAR_MEM32: {
    
    if (state->bar_state[bar_num] == PHYSICAL ) { 
      // remove old mapping
      struct v3_mem_region * old_reg = v3_get_mem_region(dev->vm, V3_MEM_CORE_ANY, vbar->addr);
      
      if (old_reg == NULL) {
	// uh oh...
	PrintError(dev->vm, VCORE_NONE, "Could not find PCI Passthrough memory redirection region (addr=0x%x)\n", (uint32_t)vbar->addr);
	return -1;
      }
      
      v3_delete_mem_region(dev->vm, old_reg); // void return?!
      
    } else {  // STUB
      if (v3_unhook_mem(dev->vm, V3_MEM_CORE_ANY, vbar->addr)==-1) { 
	PrintError(dev->vm, VCORE_NONE, "Failed to unhook memory\n");
	return -1;
      }
    }
  }

    break;

  case PT_BAR_MEM64_LO: 
    
    // the unmapping happens on the HI BAR

    break;

  case PT_BAR_MEM64_HI: {
    //struct v3_host_pci_bar * lo_vbar = &(state->virt_bars[bar_num - 1]);
    struct v3_mem_region * old_reg =  v3_get_mem_region(dev->vm, V3_MEM_CORE_ANY, vbar->addr);

    if (state->bar_state[bar_num] == PHYSICAL) { 
      if (old_reg == NULL) {
	// uh oh...
	PrintError(dev->vm, VCORE_NONE, "Could not find PCI Passthrough memory redirection region (addr=%p)\n", 
		   (void *)(addr_t)vbar->addr);
	return -1;
      }
      
      // remove old mapping
      v3_delete_mem_region(dev->vm, old_reg); // void return?!
	  
    } else { // STUB
      if (v3_unhook_mem(dev->vm, V3_MEM_CORE_ANY, vbar->addr)==-1) { 
	PrintError(dev->vm, VCORE_NONE, "Failed to unhook memory\n");
	return -1;
      }
    }
  }
    break;
    
  default:
      PrintError(dev->vm, VCORE_NONE, "Unhandled Pasthrough PCI Bar type %d\n", vbar->type);
      return -1;
      break;
  }
  
  state->bar_state[bar_num] = UNMAPPED;
  
  return 0;
  
}

  
  
static int pci_map_bar(struct vm_device        *dev,
		       struct host_pci_state   *state,
		       int                      bar_num,
		       bar_state_t              target)
{
  struct v3_host_pci_bar *hbar = &(state->host_dev->bars[bar_num]);
  struct v3_host_pci_bar *vbar = &(state->virt_bars[bar_num]);
  
  if (state->bar_state[bar_num] != UNMAPPED) { 
    PrintError(dev->vm, VCORE_NONE, "Attempt to map bar that is already mapped\n");
    return -1;
  }
  
  if (target != STUB && target != PHYSICAL) { 
    PrintError(dev->vm, VCORE_NONE, "Attempt to map to unknown target %d\n",target);
    return -1;
  }
  
  
  switch (vbar->type) { 
    
  case PT_BAR_NONE:
    break;
    
  case PT_BAR_IO: {
    int i;
    
    if (target == PHYSICAL) { 
      if (vbar->addr == hbar->addr) {
	// Map the io ports as passthrough
	PrintDebug(dev->vm,VCORE_NONE,"Adding io port direct mappings\n");
	for (i = 0; i < hbar->size; i++) {
	  if (v3_hook_io_port(dev->vm, hbar->addr + i, NULL, NULL, NULL) == -1) { 
	    PrintError(dev->vm, VCORE_NONE, "Failed to hook ioport %llu direct\n",hbar->addr+i);
	    return -1;
	  }
	}
      } else {
	// We have to manually handle the io redirection
	PrintDebug(dev->vm,VCORE_NONE,"Adding io port indirect mappings\n");
	for (i = 0; i < vbar->size; i++) {
	  if (v3_hook_io_port(dev->vm, vbar->addr + i, pt_io_read, pt_io_write, hbar) == -1) { 
	    PrintError(dev->vm,VCORE_NONE,"Failed to hook ioport %llu indirect\n", vbar->addr+i);
	    return -1;
	  }
	}
      }
    } else { // STUB
      // hook to stubs
      PrintDebug(dev->vm,VCORE_NONE,"Adding io port indirect mappings to stubs\n");
      for (i = 0; i < vbar->size; i++) {
	if (v3_hook_io_port(dev->vm, vbar->addr + i, pt_io_read_stub, pt_io_write_stub, dev)==-1) { 
	  PrintError(dev->vm, VCORE_NONE, "Failed to hook ioport %llu indirect to stub\n",vbar->addr+i);
	  return -1;
	}
      } 
    }
  }
    break;
    
  case PT_BAR_MEM32: {
    
    if (target == PHYSICAL) { 
      PrintDebug(dev->vm, VCORE_NONE, 
		 "Adding pci Passthrough mapping: start=0x%x, size=%d, end=0x%x (hpa=%p)\n", 
		 (uint32_t)vbar->addr, vbar->size, (uint32_t)vbar->addr + vbar->size, (void *)hbar->addr);
      
      if (v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, 
			    vbar->addr, 
			    vbar->addr + vbar->size - 1,
			    hbar->addr) == -1 ) {
	PrintError(dev->vm, VCORE_NONE, "Failed to add region\n");
	return -1;
      }
    } else { // STUB
      // we will hook the memory instead with the stubs
      PrintDebug(dev->vm, VCORE_NONE, 
		 "Adding pci Passthrough mapping to stubs: start=0x%x, size=%d, end=0x%x (hpa=%p)\n",
 		 (uint32_t)vbar->addr, vbar->size, (uint32_t)vbar->addr + vbar->size, (void *)hbar->addr);
      
      if (v3_hook_full_mem(dev->vm, V3_MEM_CORE_ANY,
			   vbar->addr, vbar->addr+vbar->size-1,
			   pt_mem_read_stub, pt_mem_write_stub, dev) == -1) { 
	PrintError(dev->vm, VCORE_NONE, "Cannot hook memory for unprivileged access\n");
	return -1;
      }
    }
  }
    break;
    
  case PT_BAR_MEM64_LO: 
    
    // the mapping happens on the HI BAR
    
    break;
    
  case PT_BAR_MEM64_HI: {
    
    if (target == PHYSICAL) { 
      PrintDebug(dev->vm, VCORE_NONE, 
		 "Adding pci Passthrough remapping: start=%p, size=%p, end=%p\n", 
		 (void *)(addr_t)vbar->addr, (void *)(addr_t)vbar->size, 
		 (void *)(addr_t)(vbar->addr + vbar->size));
      
      if (v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, vbar->addr, 
			    vbar->addr + vbar->size - 1, hbar->addr) == -1) {
	
	PrintDebug(dev->vm, VCORE_NONE, "Fail to insert shadow region (%p, %p)  -> %p\n",
		   (void *)(addr_t)vbar->addr,
		   (void *)(addr_t)(vbar->addr + vbar->size - 1),
		   (void *)(addr_t)hbar->addr);
	return -1;
      }
    } else { // STUB
      // we will hook the memory instead, using the stubs
      PrintDebug(dev->vm, VCORE_NONE, 
		 "Adding pci Passthrough mapping to stubs: start=0x%x, size=%d, end=0x%x (hpa=%p)\n",
 		 (uint32_t)vbar->addr, vbar->size, (uint32_t)vbar->addr + vbar->size, (void *)hbar->addr);
      
      if (v3_hook_full_mem(dev->vm, V3_MEM_CORE_ANY,
			   vbar->addr, vbar->addr+vbar->size-1,
			   pt_mem_read_stub, pt_mem_write_stub, dev) == -1) { 
	PrintError(dev->vm, VCORE_NONE, "Cannot hook memory for unprivileged access\n");
	return -1;
      }
    }
  }
    break;
    
  default:
    PrintError(dev->vm, VCORE_NONE, "Unhandled Pasthrough PCI Bar type %d\n", vbar->type);
    return -1;
    break;
  }
  
  state->bar_state[bar_num]=target;
  
  return 0;
  
 }
	

//
// This unmaps the bar no matter what state it's in
//
static int deactivate_bar(struct vm_device *dev,
			   int bar_num)
{
  struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
  
  if (pci_unmap_bar(dev, state, bar_num)) { 
    PrintError(dev->vm, VCORE_NONE, "Cannot unmap bar %d\n", bar_num);
    return -1;
  } 
  return 0;
}
  
//
// This does nothing if the bar is already physical
// if it's not, it unmaps it and then maps it physical
//
static int activate_bar_physical(struct vm_device *dev,
				 int bar_num)
{
  struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
  
  if (state->bar_state[bar_num] != PHYSICAL) { 
    if (pci_unmap_bar(dev, state, bar_num)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot unmap bar %d\n", bar_num);
      return -1;
    } 
    if (pci_map_bar(dev,state,bar_num,PHYSICAL)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot map bar %d\n", bar_num);
      return -1;
    }
  }
  return 0;
}

//
// This does nothing if the bar is already stub
// if it's not, it unmaps it and then maps it stub
//
static int activate_bar_stub(struct vm_device *dev,
			     int bar_num)
{
  struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
  
  if (state->bar_state[bar_num] != STUB) { 
    if (pci_unmap_bar(dev, state, bar_num)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot unmap bar %d\n", bar_num);
      return -1;
    } 
    if (pci_map_bar(dev,state,bar_num,STUB)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot map bar %d\n", bar_num);
      return -1;
    }
  }
  return 0;
}

static int activate_bar(struct vm_device *dev,
			int bar_num)
{
  struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
  
  if (state->priv_state == PRIVILEGED) { 
    return activate_bar_physical(dev,bar_num);
  } else {
    return activate_bar_stub(dev,bar_num);
  }
}



static int reactivate_device_privileged(struct vm_device *dev)
{
  int i;
  struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
  
  PrintDebug(dev->vm, VCORE_NONE, "Switch device to privileged operation\n");
  
  for (i=0;i<6;i++) { 
    if (activate_bar_physical(dev,i)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot activate bar %d as physical\n",i);
      return -1;
    }
  }
  state->priv_state=PRIVILEGED;
  return 0;
}

static int reactivate_device_unprivileged(struct vm_device *dev)
{
  int i;
  struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
  
  PrintDebug(dev->vm, VCORE_NONE, "Switch device to unprivileged operation\n");
  
  for (i=0;i<6;i++) { 
    if (activate_bar_stub(dev,i)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot activate bar %d as stub\n",i);
      return -1;
    }
  }
  state->priv_state=UNPRIVILEGED;
  return 0;
}


static int pci_bar_init(int bar_num, uint32_t * dst, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    struct v3_host_pci_bar * hbar = &(state->host_dev->bars[bar_num]);
    uint32_t bar_val = 0;

    if (hbar->type == PT_BAR_IO) {
	bar_val = PCI_IO_BAR_VAL(hbar->addr);
    } else if (hbar->type == PT_BAR_MEM32) {
	bar_val = PCI_MEM32_BAR_VAL(hbar->addr, hbar->prefetchable);
    } else if (hbar->type == PT_BAR_MEM24) {
	bar_val = PCI_MEM24_BAR_VAL(hbar->addr, hbar->prefetchable);
    } else if (hbar->type == PT_BAR_MEM64_LO) {
	struct v3_host_pci_bar * hi_hbar = &(state->host_dev->bars[bar_num + 1]);
	bar_val = PCI_MEM64_LO_BAR_VAL(hi_hbar->addr, hbar->prefetchable);
    } else if (hbar->type == PT_BAR_MEM64_HI) {
	bar_val = PCI_MEM64_HI_BAR_VAL(hbar->addr, hbar->prefetchable);
    } else {
        PrintDebug(dev->vm, VCORE_NONE, "Unknown bar type %d\n",hbar->type);
    }

    memcpy(&(state->virt_bars[bar_num]), hbar, sizeof(struct v3_host_pci_bar));

    *dst = bar_val;

    if (activate_bar(dev,bar_num)) { 
      PrintError(dev->vm, VCORE_NONE,"Cannot activate bar %d\n",bar_num);
      return -1;
    }


    return 0;
}



static int pci_bar_write(int bar_num, uint32_t * src, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
    struct v3_host_pci_bar * hbar = &(state->host_dev->bars[bar_num]);
    struct v3_host_pci_bar * vbar = &(state->virt_bars[bar_num]);


    if (deactivate_bar(dev,bar_num)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot deactivate bar %d\n", bar_num);
      return -1;
    }

    switch (vbar->type) { 
    case PT_BAR_NONE:
      // nothing to do
      break;

    case PT_BAR_IO: 

      // clear the low bits to match the size
      vbar->addr = *src & ~(hbar->size - 1);
      
      // udpate source version
      *src = PCI_IO_BAR_VAL(vbar->addr);

      break;

    case PT_BAR_MEM32:

      // clear the low bits to match the size
      vbar->addr = *src & ~(hbar->size - 1);
      
      // Set reserved bits
      *src = PCI_MEM32_BAR_VAL(vbar->addr, hbar->prefetchable);
      
      break;

    case  PT_BAR_MEM64_LO:

      // We only store the written values here, the actual reconfig comes when the high BAR is updated
      
      vbar->addr = *src & ~(hbar->size - 1);
      
      *src = PCI_MEM64_LO_BAR_VAL(vbar->addr, hbar->prefetchable);

      break;

    case PT_BAR_MEM64_HI: {

      struct v3_host_pci_bar * lo_vbar = &(state->virt_bars[bar_num - 1]);

      vbar->addr = (((uint64_t)*src) << 32) + lo_vbar->addr;
      
      // We don't set size, because we assume region is less than 4GB
      // src does not change, because there are no reserved bits
	
    }
      break;
      
    default:
      PrintError(dev->vm, VCORE_NONE, "Unhandled Pasthrough PCI Bar type %d\n", vbar->type);
      return -1;
      
      break;
    }

    if (activate_bar(dev,bar_num)) { 
      PrintError(dev->vm, VCORE_NONE, "Cannot activate bar %d\n", bar_num);
      return -1;
    }

    return 0;

}


static int pt_config_write(struct pci_device * pci_dev, uint32_t reg_num, void * src, uint_t length, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
//    V3_PrintStubStub(dev->vm, VCORE_NONE, "Writing host PCI config space update\n");

    // We will mask all operations to the config header itself, 
    // and only allow direct access to the device specific config space
    if (reg_num < 64) {
	return 0;
    }

    if (TEST_NOSELPRIV || state->priv_state==PRIVILEGED) { 
      return v3_host_pci_config_write(state->host_dev, reg_num, src, length);
    } else {
      PrintError(dev->vm, VCORE_NONE, "configuration write while unprivileged - CURRENTLY IGNORED\n");
      return length;
    }
}



static int pt_config_read(struct pci_device * pci_dev, uint32_t reg_num, void * dst, uint_t length, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
  //  V3_PrintStubStub(dev->vm, VCORE_NONE, "Reading host PCI config space update\n");

    if (TEST_NOSELPRIV || state->priv_state==PRIVILEGED) { 
      return v3_host_pci_config_read(state->host_dev, reg_num, dst, length);
    } else {
      PrintError(dev->vm, VCORE_NONE, "configuration read while unprivileged - CURRENTLY IGNORED\n");
      memset(dst,0,length);
      return length;
    }
}



/* This is really iffy....
 * It was totally broken before, but it's _not_ totally fixed now
 * The Expansion rom can be enabled/disabled via software using the low order bit
 * We should probably handle that somehow here... 
 */
static int pt_exp_rom_write(struct pci_device * pci_dev, uint32_t * src, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)(priv_data);
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
    struct v3_host_pci_bar * hrom = &(state->host_dev->exp_rom);
    struct v3_host_pci_bar * vrom = &(state->virt_exp_rom);

    PrintDebug(dev->vm, VCORE_NONE, "exp_rom update: src=0x%x\n", *src);
    PrintDebug(dev->vm, VCORE_NONE, "vrom is size=%u, addr=0x%x\n", vrom->size, (uint32_t)vrom->addr);
    PrintDebug(dev->vm, VCORE_NONE, "hrom is size=%u, addr=0x%x\n", hrom->size, (uint32_t)hrom->addr);


    if (!TEST_NOSELPRIV && state->priv_state != PRIVILEGED) { 
      PrintError(dev->vm, VCORE_NONE, "Attempt to do expansion rom write when not privileged is IGNORED\n");
      //PrintError(dev->vm, VCORE_NONE,"\tKCH: letting it through though\n");
      /* KCH: let it through for now, commenting below */
      return 0;
    }

    if (hrom->exp_rom_enabled) {
	// only remove old mapping if present, I.E. if the rom was enabled previously 
	if (vrom->exp_rom_enabled) {
	    struct v3_mem_region * old_reg = v3_get_mem_region(dev->vm, V3_MEM_CORE_ANY, vrom->addr);
	  
	    if (old_reg == NULL) {
		// uh oh...
		PrintError(dev->vm, VCORE_NONE, "Could not find PCI Passthrough exp_rom_base redirection region (addr=0x%x)\n", (uint32_t)vrom->addr);
		return -1;
	    }
	  
	    v3_delete_mem_region(dev->vm, old_reg);
	}
      
      
	vrom->addr = *src & ~(hrom->size - 1);
      
	// Set flags in actual register value
	*src = PCI_EXP_ROM_VAL(vrom->addr, (*src & 0x00000001));
      
	PrintDebug(dev->vm, VCORE_NONE, "Cooked src=0x%x\n", *src);
      
  
	PrintDebug(dev->vm, VCORE_NONE, "Adding pci Passthrough exp_rom_base remapping: start=0x%x, size=%u, end=0x%x\n", 
		   (uint32_t)vrom->addr, vrom->size, (uint32_t)vrom->addr + vrom->size);
      
	if (v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, vrom->addr, 
			      vrom->addr + vrom->size - 1, hrom->addr) == -1) {
	    PrintError(dev->vm, VCORE_NONE, "Failed to remap pci exp_rom: start=0x%x, size=%u, end=0x%x\n", 
		       (uint32_t)vrom->addr, vrom->size, (uint32_t)vrom->addr + vrom->size);
	    return -1;
	}
    }
    
    return 0;
}


static int pt_cmd_update(struct pci_device * pci, pci_cmd_t cmd, uint64_t arg, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)(priv_data);
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;

    V3_Print(dev->vm, VCORE_NONE, "Host PCI Device: CMD update (%d)(arg=%llu)\n", cmd, arg);
    
    
    if (TEST_NOSELPRIV || state->priv_state == PRIVILEGED) { 
      v3_host_pci_cmd_update(state->host_dev, cmd, arg);
    } else {
      PrintError(dev->vm, VCORE_NONE, "Attempt to do cmd update when not in privileged mode is IGNORED\n");
      return 0;
    }

    return 0;
}


static int setup_virt_pci_dev(struct v3_vm_info * vm_info, struct vm_device * dev) {
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
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
				     pt_config_write,
				     pt_config_read,
				     pt_cmd_update,
				     pt_exp_rom_write,               
				     dev);


    state->pci_dev = pci_dev;

    //    pci_exp_rom_init(dev, state);
    pci_dev->config_header.expansion_rom_address = 0;
    
    v3_pci_enable_capability(pci_dev, PCI_CAP_MSI);
//    v3_pci_enable_capability(pci_dev, PCI_CAP_MSIX);
    v3_pci_enable_capability(pci_dev, PCI_CAP_PCIE);
    v3_pci_enable_capability(pci_dev, PCI_CAP_PM);



    if (state->host_dev->iface == SYMBIOTIC) {
#ifdef V3_CONFIG_SYMBIOTIC
	v3_sym_map_pci_passthrough(vm_info, pci_dev->bus_num, pci_dev->dev_num, pci_dev->fn_num);
#else
	PrintError(vm_info, VCORE_NONE, "ERROR Symbiotic Passthrough is not enabled\n");
	return -1;
#endif
    }

    return 0;
}



static int init_priv(struct guest_info * core, void ** private_data) 
{
  PrintDebug(core->vm_info, core, "Initializing host_pci device selective privilege operation\n");
  return 0;
}

static int lower_priv(struct guest_info * core, void * private_data)
{
  struct vm_device * dev = (struct vm_device *)private_data;
  
  PrintDebug(core->vm_info, core, "Lowering privilege for device %s\n", dev->name);
  
  return reactivate_device_unprivileged(dev);
}

static int raise_priv(struct guest_info * core, void * private_data)
{
  struct vm_device * dev = (struct vm_device *)private_data;
  
  PrintDebug(core->vm_info, core, "Raising privilege for device %s\n", dev->name);
    
  return reactivate_device_privileged(dev);
}

static int deinit_priv(struct guest_info * core, void * private_data)
{
  PrintDebug(core->vm_info, core, "Deinitializing host_pci device selective privilege operation\n");
  return 0;
}


static struct v3_device_ops dev_ops = {
    .free = NULL,
};


static int irq_ack(struct guest_info * core, uint32_t irq, void * private_data) {
    struct host_pci_state * state = (struct host_pci_state *)private_data;

    
    PrintDebug(core->vm_info,core,"GM HPCI: IRQ ACK of irq %d\n", irq);
    //    V3_PrintStubStub(core->vm_info, core, "Acking IRQ %d\n", irq);
    v3_host_pci_ack_irq(state->host_dev, irq);

    return 0;
}


static int irq_handler(void * private_data, uint32_t vec_index) {
    struct host_pci_state * state = (struct host_pci_state *)private_data;
    struct v3_irq vec;

    vec.irq = vec_index;
    vec.ack = irq_ack;
    vec.private_data = state;

    // For selective privilege, the interrupt is always delivered
    // identically to the regular case, regardless of the current
    // privilege level.  The idea here is that delivery of the
    // interrupt will result in a dispatch to the privileged guest code
    // and the entry to that code will enable privilege before any
    // mapped bar is read or written.

    PrintDebug(0, 0,"Host PCI / Selective Privilege: Delivering vec irq %d, state %d\n", vec.irq, (int)(state->pci_dev->irq_type));
    if (state->pci_dev->irq_type == IRQ_NONE) {
	return 0;
    } else if (state->pci_dev->irq_type == IRQ_INTX) {
	v3_pci_raise_acked_irq(state->pci_bus, state->pci_dev, vec);
    } else {
	v3_pci_raise_irq(state->pci_bus, state->pci_dev, vec_index);
    }

    return 0;
}


static int host_pci_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct host_pci_state * state = V3_Malloc(sizeof(struct host_pci_state));
    struct vm_device * dev = NULL;
    struct vm_device * pci = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    char * dev_id = v3_cfg_val(cfg, "ID");    
    char * url = v3_cfg_val(cfg, "url");

    memset(state, 0, sizeof(struct host_pci_state));

    if (!pci) {
	PrintError(vm, VCORE_NONE, "PCI bus not specified in config file\n");
	return -1;
    }
    
    state->pci_bus = pci;
    strncpy(state->name, dev_id, 32);


    dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    state->host_dev = v3_host_pci_get_dev(vm, url, state);

    if (state->host_dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not connect to host pci device (%s)\n", url);
	return -1;
    }


    state->host_dev->irq_handler = irq_handler;

    char * sel_priv = v3_cfg_val(cfg, "selective_privilege");

    if (!sel_priv || !strncasecmp(sel_priv,"off",3) || !strncasecmp(sel_priv,"0",1)) { 
#if TEST_NOSELPRIV
      state->priv_state=PRIVILEGED;
#else
      state->priv_state=UNPRIVILEGED;
#endif
      PrintDebug(vm, VCORE_NONE, "Selective privilege functionality disabled\n");
    } else {

      char * ext_priv = v3_cfg_val(cfg, "privilege_extension");

      if (!ext_priv) { 
	PrintError(vm, VCORE_NONE, "Selective privilege functionality requested, but not privilege extension given\n");
	return -1;
      }
      if (v3_bind_privilege(&(vm->cores[0]), 
			    ext_priv, 
			    init_priv,
			    lower_priv,
			    raise_priv,
			    deinit_priv,
			    dev)) { 
	PrintError(vm, VCORE_NONE, "Cannot bind to privilege %s\n",ext_priv);
	return -1;
      }

      state->priv_state=UNPRIVILEGED;

      PrintDebug(vm, VCORE_NONE, "Selective privilege functionality enabled (initially unprivileged)\n");

    }

    if (setup_virt_pci_dev(vm, dev) == -1) {
	PrintError(vm, VCORE_NONE, "Could not setup virtual host PCI device\n");
	return -1;
    }

    return 0;
}




device_register("HOST_PCI_SELPRIV", host_pci_init)

