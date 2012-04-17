/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jacklange@cs.pitt.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


/* This is the generic passthrough PCI virtual device */

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
#include <palacios/vm_guest.h> // must include this to avoid dependency issue
#include <palacios/vmm_symspy.h>

#include <devices/pci.h>
#include <devices/pci_types.h>
#include <interfaces/host_pci.h>

#define PCI_BUS_MAX  7
#define PCI_DEV_MAX 32
#define PCI_FN_MAX   7

#define PCI_DEVICE 0x0
#define PCI_PCI_BRIDGE 0x1
#define PCI_CARDBUS_BRIDGE 0x2

#define PCI_HDR_SIZE 256




struct host_pci_state {
    // This holds the description of the host PCI device configuration
    struct v3_host_pci_dev * host_dev;


    struct v3_host_pci_bar virt_bars[6];
    struct v3_host_pci_bar virt_exp_rom;
     
    struct vm_device * pci_bus;
    struct pci_device * pci_dev;

    char name[32];
};



/*
static int pci_exp_rom_init(struct vm_device * dev, struct host_pci_state * state) {
    struct pci_device * pci_dev = state->pci_dev;
    struct v3_host_pci_bar * hrom = &(state->host_dev->exp_rom);


    
    PrintDebug("Adding 32 bit PCI mem region: start=%p, end=%p\n",
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


	PrintDebug("phys exp_rom: addr=%p, size=%u\n", 
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
	PrintError("Invalid PCI passthrough IO Redirection size read\n");
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
	PrintError("Invalid PCI passthrough IO Redirection size write\n");
	return -1;
    }
    
    return length;

}



static int pci_bar_init(int bar_num, uint32_t * dst, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    struct v3_host_pci_bar * hbar = &(state->host_dev->bars[bar_num]);
    uint32_t bar_val = 0;

    if (hbar->type == PT_BAR_IO) {
	int i = 0;

	bar_val = PCI_IO_BAR_VAL(hbar->addr);

	for (i = 0; i < hbar->size; i++) {
	    v3_hook_io_port(dev->vm, hbar->addr + i, NULL, NULL, NULL);
	}
    } else if (hbar->type == PT_BAR_MEM32) {
	bar_val = PCI_MEM32_BAR_VAL(hbar->addr, hbar->prefetchable);

	v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, 
			  hbar->addr, hbar->addr + hbar->size - 1,
			  hbar->addr);
	
    } else if (hbar->type == PT_BAR_MEM24) {
	bar_val = PCI_MEM24_BAR_VAL(hbar->addr, hbar->prefetchable);

	v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, 
			  hbar->addr, hbar->addr + hbar->size - 1,
			  hbar->addr);
    } else if (hbar->type == PT_BAR_MEM64_LO) {
	PrintError("Don't currently handle 64 bit bars...\n");
    } else if (hbar->type == PT_BAR_MEM64_HI) {
	PrintError("Don't currently handle 64 bit bars...\n");
    }


    memcpy(&(state->virt_bars[bar_num]), hbar, sizeof(struct v3_host_pci_bar));

    *dst = bar_val;

    return 0;
}



static int pci_bar_write(int bar_num, uint32_t * src, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
    struct v3_host_pci_bar * hbar = &(state->host_dev->bars[bar_num]);
    struct v3_host_pci_bar * vbar = &(state->virt_bars[bar_num]);



    if (vbar->type == PT_BAR_NONE) {
	return 0;
    } else if (vbar->type == PT_BAR_IO) {
	int i = 0;

	// unhook old ports
	for (i = 0; i < vbar->size; i++) {
	    if (v3_unhook_io_port(dev->vm, vbar->addr + i) == -1) {
		PrintError("Could not unhook previously hooked port.... %d (0x%x)\n", 
			   (uint32_t)vbar->addr + i, (uint32_t)vbar->addr + i);
		return -1;
	    }
	}

	// clear the low bits to match the size
	vbar->addr = *src & ~(hbar->size - 1);

	// udpate source version
	*src = PCI_IO_BAR_VAL(vbar->addr);

	PrintDebug("Rehooking passthrough IO ports starting at %d (0x%x)\n", 
		   (uint32_t)vbar->addr, (uint32_t)vbar->addr);

	if (vbar->addr == hbar->addr) {
	    // Map the io ports as passthrough
	    for (i = 0; i < hbar->size; i++) {
		v3_hook_io_port(dev->vm, hbar->addr + i, NULL, NULL, NULL); 
	    }
	} else {
	    // We have to manually handle the io redirection
	    for (i = 0; i < vbar->size; i++) {
		v3_hook_io_port(dev->vm, vbar->addr + i, pt_io_read, pt_io_write, hbar); 
	    }
	}
    } else if (vbar->type == PT_BAR_MEM32) {
	// remove old mapping
	struct v3_mem_region * old_reg = v3_get_mem_region(dev->vm, V3_MEM_CORE_ANY, vbar->addr);

	if (old_reg == NULL) {
	    // uh oh...
	    PrintError("Could not find PCI Passthrough memory redirection region (addr=0x%x)\n", (uint32_t)vbar->addr);
	    return -1;
	}

	v3_delete_mem_region(dev->vm, old_reg);

	// clear the low bits to match the size
	vbar->addr = *src & ~(hbar->size - 1);

	// Set reserved bits
	*src = PCI_MEM32_BAR_VAL(vbar->addr, hbar->prefetchable);

	PrintDebug("Adding pci Passthrough remapping: start=0x%x, size=%d, end=0x%x (hpa=%p)\n", 
		   (uint32_t)vbar->addr, vbar->size, (uint32_t)vbar->addr + vbar->size, (void *)hbar->addr);

	v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, 
			  vbar->addr, 
			  vbar->addr + vbar->size - 1,
			  hbar->addr);

    } else if (vbar->type == PT_BAR_MEM64_LO) {
	// We only store the written values here, the actual reconfig comes when the high BAR is updated

	vbar->addr = *src & ~(hbar->size - 1);

	*src = PCI_MEM64_LO_BAR_VAL(vbar->addr, hbar->prefetchable);


    } else if (vbar->type == PT_BAR_MEM64_HI) {
	struct v3_host_pci_bar * lo_vbar = &(state->virt_bars[bar_num - 1]);
	struct v3_mem_region * old_reg =  v3_get_mem_region(dev->vm, V3_MEM_CORE_ANY, vbar->addr);

	if (old_reg == NULL) {
	    // uh oh...
	    PrintError("Could not find PCI Passthrough memory redirection region (addr=%p)\n", 
		       (void *)(addr_t)vbar->addr);
	    return -1;
	}

	// remove old mapping
	v3_delete_mem_region(dev->vm, old_reg);

	vbar->addr = (((uint64_t)*src) << 32) + lo_vbar->addr;

	// We don't set size, because we assume region is less than 4GB
	// src does not change, because there are no reserved bits
	

	PrintDebug("Adding pci Passthrough remapping: start=%p, size=%p, end=%p\n", 
		   (void *)(addr_t)vbar->addr, (void *)(addr_t)vbar->size, 
		   (void *)(addr_t)(vbar->addr + vbar->size));

	if (v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, vbar->addr, 
			      vbar->addr + vbar->size - 1, hbar->addr) == -1) {

	    PrintDebug("Fail to insert shadow region (%p, %p)  -> %p\n",
		       (void *)(addr_t)vbar->addr,
		       (void *)(addr_t)(vbar->addr + vbar->size - 1),
		       (void *)(addr_t)hbar->addr);
	    return -1;
	}
	
    } else {
	PrintError("Unhandled Pasthrough PCI Bar type %d\n", vbar->type);
	return -1;
    }


    return 0;
}


static int pt_config_write(struct pci_device * pci_dev, uint32_t reg_num, void * src, uint_t length, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
//    V3_Print("Writing host PCI config space update\n");

    // We will mask all operations to the config header itself, 
    // and only allow direct access to the device specific config space
    if (reg_num < 64) {
	return 0;
    }

    return v3_host_pci_config_write(state->host_dev, reg_num, src, length);
}



static int pt_config_read(struct pci_device * pci_dev, uint32_t reg_num, void * dst, uint_t length, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;
    
  //  V3_Print("Reading host PCI config space update\n");

    return v3_host_pci_config_read(state->host_dev, reg_num, dst, length);
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

    PrintDebug("exp_rom update: src=0x%x\n", *src);
    PrintDebug("vrom is size=%u, addr=0x%x\n", vrom->size, (uint32_t)vrom->addr);
    PrintDebug("hrom is size=%u, addr=0x%x\n", hrom->size, (uint32_t)hrom->addr);

    if (hrom->exp_rom_enabled) {
	// only remove old mapping if present, I.E. if the rom was enabled previously 
	if (vrom->exp_rom_enabled) {
	    struct v3_mem_region * old_reg = v3_get_mem_region(dev->vm, V3_MEM_CORE_ANY, vrom->addr);
	  
	    if (old_reg == NULL) {
		// uh oh...
		PrintError("Could not find PCI Passthrough exp_rom_base redirection region (addr=0x%x)\n", (uint32_t)vrom->addr);
		return -1;
	    }
	  
	    v3_delete_mem_region(dev->vm, old_reg);
	}
      
      
	vrom->addr = *src & ~(hrom->size - 1);
      
	// Set flags in actual register value
	*src = PCI_EXP_ROM_VAL(vrom->addr, (*src & 0x00000001));
      
	PrintDebug("Cooked src=0x%x\n", *src);
      
  
	PrintDebug("Adding pci Passthrough exp_rom_base remapping: start=0x%x, size=%u, end=0x%x\n", 
		   (uint32_t)vrom->addr, vrom->size, (uint32_t)vrom->addr + vrom->size);
      
	if (v3_add_shadow_mem(dev->vm, V3_MEM_CORE_ANY, vrom->addr, 
			      vrom->addr + vrom->size - 1, hrom->addr) == -1) {
	    PrintError("Failed to remap pci exp_rom: start=0x%x, size=%u, end=0x%x\n", 
		       (uint32_t)vrom->addr, vrom->size, (uint32_t)vrom->addr + vrom->size);
	    return -1;
	}
    }
    
    return 0;
}


static int pt_cmd_update(struct pci_device * pci, pci_cmd_t cmd, uint64_t arg, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)(priv_data);
    struct host_pci_state * state = (struct host_pci_state *)dev->private_data;

    V3_Print("Host PCI Device: CMD update (%d)(arg=%llu)\n", cmd, arg);
    
    v3_host_pci_cmd_update(state->host_dev, cmd, arg);

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
	PrintError("ERROR Symbiotic Passthrough is not enabled\n");
	return -1;
#endif
    }

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = NULL,
};


static int irq_ack(struct guest_info * core, uint32_t irq, void * private_data) {
    struct host_pci_state * state = (struct host_pci_state *)private_data;

    
    //    V3_Print("Acking IRQ %d\n", irq);
    v3_host_pci_ack_irq(state->host_dev, irq);

    return 0;
}


static int irq_handler(void * private_data, uint32_t vec_index) {
    struct host_pci_state * state = (struct host_pci_state *)private_data;
    struct v3_irq vec;

    vec.irq = vec_index;
    vec.ack = irq_ack;
    vec.private_data = state;


    //    V3_Print("Raising host PCI IRQ %d\n", vec_index);

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
	PrintError("PCI bus not specified in config file\n");
	return -1;
    }
    
    state->pci_bus = pci;
    strncpy(state->name, dev_id, 32);


    dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    state->host_dev = v3_host_pci_get_dev(vm, url, state);

    if (state->host_dev == NULL) {
	PrintError("Could not connect to host pci device (%s)\n", url);
	return -1;
    }


    state->host_dev->irq_handler = irq_handler;

    if (setup_virt_pci_dev(vm, dev) == -1) {
	PrintError("Could not setup virtual host PCI device\n");
	return -1;
    }

    return 0;
}




device_register("HOST_PCI", host_pci_init)
