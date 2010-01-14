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
#include <palacios/vmm_sym_iface.h>

#include <devices/pci.h>
#include <devices/pci_types.h>


// Hardcoded... Are these standard??
#define PCI_CFG_ADDR    0xcf8
#define PCI_CFG_DATA    0xcfc

#define PCI_BUS_MAX  7
#define PCI_DEV_MAX 32
#define PCI_FN_MAX   7

#define PCI_DEVICE 0x0
#define PCI_PCI_BRIDGE 0x1
#define PCI_CARDBUS_BRIDGE 0x2

#define PCI_HDR_SIZE 256


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


typedef enum { PT_BAR_NONE,
	       PT_BAR_IO, 
	       PT_BAR_MEM32, 
	       PT_BAR_MEM24, 
	       PT_BAR_MEM64_LOW, 
	       PT_BAR_MEM64_HIGH } pt_bar_type_t;

struct pt_bar {
    uint32_t size;
    pt_bar_type_t type;
    uint32_t addr;

    uint32_t val;
};

struct pt_dev_state {
    union {
	uint8_t config_space[256];
	struct pci_config_header real_hdr;
    } __attribute__((packed));

    struct pt_bar phys_bars[6];
    struct pt_bar virt_bars[6];

    
    struct vm_device * pci_bus;
    struct pci_device * pci_dev;

    union pci_addr_reg phys_pci_addr;

    char name[32];
};


static inline uint32_t pci_cfg_read32(uint32_t addr) {
    v3_outdw(PCI_CFG_ADDR, addr);
    return v3_indw(PCI_CFG_DATA);
}



static inline void pci_cfg_write32(uint32_t addr, uint32_t val) {
    v3_outdw(PCI_CFG_ADDR, addr);
    v3_outdw(PCI_CFG_DATA, val);
}



static inline uint16_t pci_cfg_read16(uint32_t addr) {
    v3_outw(PCI_CFG_ADDR, addr);
    return v3_inw(PCI_CFG_DATA);
}



static inline void pci_cfg_write16(uint32_t addr, uint16_t val) {
    v3_outw(PCI_CFG_ADDR, addr);
    v3_outw(PCI_CFG_DATA, val);
}



static inline uint8_t pci_cfg_read8(uint32_t addr) {
    v3_outb(PCI_CFG_ADDR, addr);
    return v3_inb(PCI_CFG_DATA);
}



static inline void pci_cfg_write8(uint32_t addr, uint8_t val) {
    v3_outb(PCI_CFG_ADDR, addr);
    v3_outb(PCI_CFG_DATA, val);
}


// We initialize this 
static int pci_bar_init(int bar_num, uint32_t * dst,void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pt_dev_state * state = (struct pt_dev_state *)dev->private_data;
    const uint32_t bar_base_reg = 4;
    union pci_addr_reg pci_addr = {state->phys_pci_addr.value};
    uint32_t bar_val = 0;
    uint32_t max_val = 0;
    //addr_t irq_state = 0;
    struct pt_bar * pbar = &(state->phys_bars[bar_num]);


    // should read from cached header
    pci_addr.reg = bar_base_reg + bar_num;

    PrintDebug("PCI Address = 0x%x\n", pci_addr.value);

    bar_val = pci_cfg_read32(pci_addr.value);
    pbar->val = bar_val;

    if ((bar_val & 0x3) == 0x1) {
	int i = 0;

	// IO bar
	pbar->type = PT_BAR_IO;
	pbar->addr = PCI_IO_BASE(bar_val);

	max_val = bar_val | PCI_IO_MASK;

	// Cycle the physical bar, to determine the actual size
	// Disable irqs, to try to prevent accesses to the space via a interrupt handler
	// This is not SMP safe!!
	// What we probably want to do is write a 0 to the command register
	//irq_state = v3_irq_save();
	
	pci_cfg_write32(pci_addr.value, max_val);
	max_val = pci_cfg_read32(pci_addr.value);
	pci_cfg_write32(pci_addr.value, bar_val);

	//v3_irq_restore(irq_state);

	V3_Print("max_val = %x\n", max_val);

	pbar->size = (uint16_t)~PCI_IO_BASE(max_val) + 1;

	
	V3_Print("IO Bar with %d (%x) ports %x->%x\n", pbar->size, pbar->size, pbar->addr, pbar->addr + pbar->size);
	// setup a set of null io hooks
	// This allows the guest to do passthrough IO to these ports
	// While still reserving them in the IO map
	for (i = 0; i < pbar->size; i++) {
	    v3_hook_io_port(dev->vm, pbar->addr + i, NULL, NULL, NULL); 
	}

    } else {

	// might be memory, might be nothing	

	max_val = bar_val | PCI_MEM_MASK;

	// Cycle the physical bar, to determine the actual size
	// Disable irqs, to try to prevent accesses to the space via a interrupt handler
	// This is not SMP safe!!
	// What we probably want to do is write a 0 to the command register
	//irq_state = v3_irq_save();
	
	pci_cfg_write32(pci_addr.value, max_val);
	max_val = pci_cfg_read32(pci_addr.value);
	pci_cfg_write32(pci_addr.value, bar_val);

	//v3_irq_restore(irq_state);

	
	if (max_val == 0) {
	    pbar->type = PT_BAR_NONE;
	} else {

	    // if its a memory region, setup passthrough mem mapping

	    if ((bar_val & 0x6) == 0x0) {
		// MEM 32
		pbar->type = PT_BAR_MEM32;
		pbar->addr = PCI_MEM32_BASE(bar_val);
		pbar->size = ~PCI_MEM32_BASE(max_val) + 1;

		PrintDebug("Adding 32 bit PCI mem region: start=0x%x, end=0x%x\n",
			   pbar->addr, pbar->addr + pbar->size);

		v3_add_shadow_mem(dev->vm, 
				  pbar->addr, 
				  pbar->addr + pbar->size - 1,
				  pbar->addr);

	    } else if ((bar_val & 0x6) == 0x2) {
		// Mem 24
		pbar->type = PT_BAR_MEM24;
		pbar->addr = PCI_MEM24_BASE(bar_val);
		pbar->size = ~PCI_MEM24_BASE(max_val) + 1;

		v3_add_shadow_mem(dev->vm, 
				  pbar->addr, 
				  pbar->addr + pbar->size - 1,
				  pbar->addr);

	    } else if ((bar_val & 0x6) == 0x4) {
		// Mem 64
		PrintError("64 Bit PCI bars not supported\n");
		return -1;
	    } else {
		PrintError("Invalid Memory bar type\n");
		return -1;
	    }

	}
    }


    // Initially the virtual bars match the physical ones
    memcpy(&(state->virt_bars[bar_num]), &(state->phys_bars[bar_num]), sizeof(struct pt_bar));

    PrintDebug("bar_num=%d, bar_val=0x%x\n", bar_num, bar_val);

    PrintDebug("phys bar  type=%d, addr=0x%x, size=%d\n", 
	       pbar->type, pbar->addr, 
	       pbar->size);

    PrintDebug("virt bar  type=%d, addr=0x%x, size=%d\n",
	       state->virt_bars[bar_num].type, state->virt_bars[bar_num].addr, 
	       state->virt_bars[bar_num].size);

    // Update the pci subsystem versions
    *dst = bar_val;

    return 0;
}

static int pt_io_read(uint16_t port, void * dst, uint_t length, void * priv_data) {
    struct pt_bar * pbar = (struct pt_bar *)priv_data;
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


static int pt_io_write(uint16_t port, void * src, uint_t length, void * priv_data) {
    struct pt_bar * pbar = (struct pt_bar *)priv_data;
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





static int pci_bar_write(int bar_num, uint32_t * src, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pt_dev_state * state = (struct pt_dev_state *)dev->private_data;
    
    struct pt_bar * pbar = &(state->phys_bars[bar_num]);
    struct pt_bar * vbar = &(state->virt_bars[bar_num]);

    PrintDebug("Bar update: bar_num=%d, src=0x%x\n", bar_num,*src);
    PrintDebug("vbar is size=%u, type=%d, addr=0x%x, val=0x%x\n",vbar->size, vbar->type, vbar->addr, vbar->val);
    PrintDebug("pbar is size=%u, type=%d, addr=0x%x, val=0x%x\n",pbar->size, pbar->type, pbar->addr, pbar->val);



    if (vbar->type == PT_BAR_NONE) {
	return 0;
    } else if (vbar->type == PT_BAR_IO) {
	int i = 0;

	// unhook old ports
	for (i = 0; i < vbar->size; i++) {
	    if (v3_unhook_io_port(dev->vm, vbar->addr + i) == -1) {
		PrintError("Could not unhook previously hooked port.... %d (0x%x)\n", 
			   vbar->addr + i, vbar->addr + i);
		return -1;
	    }
	}

	PrintDebug("Setting IO Port range size=%d\n", pbar->size);

	// clear the low bits to match the size
	*src &= ~(pbar->size - 1);

	// Set reserved bits
	*src |= (pbar->val & ~PCI_IO_MASK);

	vbar->addr = PCI_IO_BASE(*src);	

	PrintDebug("Cooked src=0x%x\n", *src);

	PrintDebug("Rehooking passthrough IO ports starting at %d (0x%x)\n", 
		   vbar->addr, vbar->addr);

	if (vbar->addr == pbar->addr) {
	    // Map the io ports as passthrough
	    for (i = 0; i < pbar->size; i++) {
		v3_hook_io_port(dev->vm, pbar->addr + i, NULL, NULL, NULL); 
	    }
	} else {
	    // We have to manually handle the io redirection
	    for (i = 0; i < vbar->size; i++) {
		v3_hook_io_port(dev->vm, vbar->addr + i, pt_io_read, pt_io_write, pbar); 
	    }
	}
    } else if (vbar->type == PT_BAR_MEM32) {
	// remove old mapping
	struct v3_shadow_region * old_reg = v3_get_shadow_region(dev->vm, vbar->addr);

	if (old_reg == NULL) {
	    // uh oh...
	    PrintError("Could not find PCI Passthrough memory redirection region (addr=0x%x)\n", vbar->addr);
	    return -1;
	}

	v3_delete_shadow_region(dev->vm, old_reg);

	// clear the low bits to match the size
	*src &= ~(pbar->size - 1);

	// Set reserved bits
	*src |= (pbar->val & ~PCI_MEM_MASK);

	PrintDebug("Cooked src=0x%x\n", *src);

	vbar->addr = PCI_MEM32_BASE(*src);

	PrintDebug("Adding pci Passthrough remapping: start=0x%x, size=%d, end=0x%x\n", 
		   vbar->addr, vbar->size, vbar->addr + vbar->size);

	v3_add_shadow_mem(dev->vm, 
			  vbar->addr, 
			  vbar->addr + vbar->size - 1,
			  pbar->addr);
	
    } else {
	PrintError("Unhandled Pasthrough PCI Bar type %d\n", vbar->type);
	return -1;
    }


    vbar->val = *src;
    

    return 0;
}


static int pt_config_update(uint_t reg_num, void * src, uint_t length, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pt_dev_state * state = (struct pt_dev_state *)dev->private_data;
    union pci_addr_reg pci_addr = {state->phys_pci_addr.value};

    pci_addr.reg = reg_num;

    if (length == 1) {
	pci_cfg_write8(pci_addr.value, *(uint8_t *)src);
    } else if (length == 2) {
	pci_cfg_write16(pci_addr.value, *(uint16_t *)src);
    } else if (length == 4) {
	pci_cfg_write32(pci_addr.value, *(uint32_t *)src);	
    }

    
    return 0;
}



static int find_real_pci_dev(uint16_t vendor_id, uint16_t device_id, struct pt_dev_state * state) {
    union pci_addr_reg pci_addr = {0x80000000};
    uint_t i, j, k, m;    

    union {
	uint32_t value;
	struct {
	    uint16_t vendor;
	    uint16_t device;
	} __attribute__((packed));
    } __attribute__((packed)) pci_hdr = {0};


    //PrintDebug("Scanning PCI busses for vendor=%x, device=%x\n", vendor_id, device_id);
    for (i = 0, pci_addr.bus = 0; i < PCI_BUS_MAX; i++, pci_addr.bus++) {
	for (j = 0, pci_addr.dev = 0; j < PCI_DEV_MAX; j++, pci_addr.dev++) {
	    for (k = 0, pci_addr.func = 0; k < PCI_FN_MAX; k++, pci_addr.func++) {

		v3_outdw(PCI_CFG_ADDR, pci_addr.value);
		pci_hdr.value = v3_indw(PCI_CFG_DATA);

		//PrintDebug("\bus=%d, tvendor=%x, device=%x\n", pci_addr.bus, pci_hdr.vendor, pci_hdr.device);

		if ((pci_hdr.vendor == vendor_id) && (pci_hdr.device == device_id)) {
		    uint32_t * cfg_space = (uint32_t *)&state->real_hdr;
    
		    state->phys_pci_addr = pci_addr;

		    // Copy the configuration space to the local cached version
		    for (m = 0, pci_addr.reg = 0; m < PCI_HDR_SIZE; m += 4, pci_addr.reg++) {
			cfg_space[pci_addr.reg] = pci_cfg_read32(pci_addr.value);
		    }


		    PrintDebug("Found device %x:%x (bus=%d, dev=%d, func=%d)\n", 
			       vendor_id, device_id, 
			       pci_addr.bus, pci_addr.dev, pci_addr.func);

		    return 0;
		}
	    }
	}
    }

    return -1;
}



static int setup_virt_pci_dev(struct guest_info * info, struct vm_device * dev) {
    struct pt_dev_state * state = (struct pt_dev_state *)dev->private_data;
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
				     pt_config_update, NULL, NULL, 
				     dev);
    
    // This will overwrite the bar registers.. but that should be ok.
    memcpy(pci_dev->config_space, (uint8_t *)&(state->real_hdr), sizeof(struct pci_config_header));

    state->pci_dev = pci_dev;

    v3_sym_map_pci_passthrough(info, pci_dev->bus_num, pci_dev->dev_num, pci_dev->fn_num);


    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};



static int irq_handler(struct guest_info * info, struct v3_interrupt * intr, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct pt_dev_state * state = (struct pt_dev_state *)dev->private_data;

//    PrintDebug("Handling E1000 IRQ %d\n", intr->irq);

    v3_pci_raise_irq(state->pci_bus, 0, state->pci_dev);

    V3_ACK_IRQ(intr->irq);

    return 0;
}




static int passthrough_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct pt_dev_state * state = V3_Malloc(sizeof(struct pt_dev_state));
    struct vm_device * dev = NULL;
    struct vm_device * pci = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    char * name = v3_cfg_val(cfg, "name");    

    memset(state, 0, sizeof(struct pt_dev_state));

    if (!pci) {
	PrintError("Could not find PCI device\n");
	return -1;
    }
    
    state->pci_bus = pci;
    strncpy(state->name, name, 32);


    dev = v3_allocate_device(name, &dev_ops, state);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }


    if (find_real_pci_dev(atox(v3_cfg_val(cfg, "vendor_id")), 
			  atox(v3_cfg_val(cfg, "device_id")), 
			  state) == -1) {
	PrintError("Could not find PCI Device %s:%s\n", 
		   v3_cfg_val(cfg, "vendor_id"), 
		   v3_cfg_val(cfg, "device_id"));
	return 0;
    }

    setup_virt_pci_dev(vm, dev);

    v3_hook_irq(vm, atoi(v3_cfg_val(cfg, "irq")), irq_handler, dev);
    //    v3_hook_irq(info, 64, irq_handler, dev);

    return 0;
}




device_register("PCI_PASSTHROUGH", passthrough_init)
