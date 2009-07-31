/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2009, Chang Seok Bae <jhuell@gmail.com>
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Lei Xia <lxia@northwestern.edu>
 *          Chang Seok Bae <jhuell@gmail.com>
 *          Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */ 
 
 

#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_rbtree.h>

#include <devices/pci.h>
#include <devices/pci_types.h>

#ifndef DEBUG_PCI
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define CONFIG_ADDR_PORT    0x0cf8
#define CONFIG_DATA_PORT    0x0cfc


#define PCI_BUS_COUNT 1

// This must always be a multiple of 8
#define MAX_BUS_DEVICES 32

struct pci_addr_reg {
    union {
	uint32_t val;
	struct {
	    uint_t rsvd       : 2;
	    uint_t reg_num    : 6;
	    uint_t fn_num     : 3;
	    uint_t dev_num    : 5;
	    uint_t bus_num    : 8;
	    uint_t rsvd2      : 7;
	    uint_t enable     : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));





struct pci_bus {
    int bus_num;

    // Red Black tree containing all attached devices
    struct rb_root devices;

    // Bitmap of the allocated device numbers
    uint8_t dev_map[MAX_BUS_DEVICES / 8];

    int (*raise_pci_irq)(struct vm_device * dev, uint_t intr_line);
    struct vm_device * irq_bridge_dev;
};



struct pci_internal {
    // Configuration address register
    struct pci_addr_reg addr_reg;

    // Attached Busses
    struct pci_bus bus_list[PCI_BUS_COUNT];
};





#ifdef DEBUG_PCI

static void pci_dump_state(struct pci_internal * pci_state) {
    struct rb_node * node = v3_rb_first(&(pci_state->bus_list[0].devices));
    struct pci_device * tmp_dev = NULL;
    
    PrintDebug("===PCI: Dumping state Begin ==========\n");
    
    do {
	tmp_dev = rb_entry(node, struct pci_device, dev_tree_node);

  	PrintDebug("PCI Device Number: %d (%s):\n", tmp_dev->dev_num,  tmp_dev->name);
	PrintDebug("irq = %d\n", tmp_dev->config_header.intr_line);
	PrintDebug("Vend ID: 0x%x\n", tmp_dev->config_header.vendor_id);
	PrintDebug("Device ID: 0x%x\n", tmp_dev->config_header.device_id);

    } while ((node = v3_rb_next(node)));
    
    PrintDebug("====PCI: Dumping state End==========\n");
}

#endif




// Scan the dev_map bitmap for the first '0' bit
static int get_free_dev_num(struct pci_bus * bus) {
    int i, j;

    for (i = 0; i < sizeof(bus->dev_map); i++) {
	PrintDebug("i=%d\n", i);
	if (bus->dev_map[i] != 0xff) {
	    // availability
	    for (j = 0; j < 8; j++) {
		PrintDebug("\tj=%d\n", j);
		if (!(bus->dev_map[i] & (0x1 << j))) {
		    return ((i * 8) + j);
		}
	    }
	}
    }

    return -1;
}

static void allocate_dev_num(struct pci_bus * bus, int dev_num) {
    int major = (dev_num / 8);
    int minor = dev_num % 8;

    bus->dev_map[major] |= (0x1 << minor);
}



static inline 
struct pci_device * __add_device_to_bus(struct pci_bus * bus, struct pci_device * dev) {

  struct rb_node ** p = &(bus->devices.rb_node);
  struct rb_node * parent = NULL;
  struct pci_device * tmp_dev = NULL;

  while (*p) {
    parent = *p;
    tmp_dev = rb_entry(parent, struct pci_device, dev_tree_node);

    if (dev->devfn < tmp_dev->devfn) {
      p = &(*p)->rb_left;
    } else if (dev->devfn > tmp_dev->devfn) {
      p = &(*p)->rb_right;
    } else {
      return tmp_dev;
    }
  }

  rb_link_node(&(dev->dev_tree_node), parent, p);

  return NULL;
}


static inline 
struct pci_device * add_device_to_bus(struct pci_bus * bus, struct pci_device * dev) {

  struct pci_device * ret = NULL;

  if ((ret = __add_device_to_bus(bus, dev))) {
    return ret;
  }

  v3_rb_insert_color(&(dev->dev_tree_node), &(bus->devices));

  allocate_dev_num(bus, dev->dev_num);

  return NULL;
}


static struct pci_device * get_device(struct pci_bus * bus, uint8_t dev_num, uint8_t fn_num) {
    struct rb_node * n = bus->devices.rb_node;
    struct pci_device * dev = NULL;
    uint8_t devfn = ((dev_num & 0x1f) << 3) | (fn_num & 0x7);

    while (n) {
	dev = rb_entry(n, struct pci_device, dev_tree_node);
	
	if (devfn < dev->devfn) {
	    n = n->rb_left;
	} else if (devfn > dev->devfn) {
	    n = n->rb_right;
	} else {
	    return dev;
	}
    }
    
    return NULL;
}







static int addr_port_read(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct pci_internal * pci_state = (struct pci_internal *)dev->private_data;
    int reg_offset = port & 0x3;
    uint8_t * reg_addr = ((uint8_t *)&(pci_state->addr_reg.val)) + reg_offset;

    PrintDebug("Reading PCI Address Port (%x): %x len=%d\n", port, pci_state->addr_reg.val, length);

    if (length == 4) {
	if (reg_offset != 0) {
	    PrintError("Invalid Address Port Read\n");
	    return -1;
	}
	*(uint32_t *)dst = *(uint32_t *)reg_addr;
    } else if (length == 2) {
	if (reg_offset > 2) {
	    PrintError("Invalid Address Port Read\n");
	    return -1;
	}
	*(uint16_t *)dst = *(uint16_t *)reg_addr;
    } else if (length == 1) {
	*(uint8_t *)dst = *(uint8_t *)reg_addr;
    } else {
	PrintError("Invalid read length (%d) for PCI address register\n", length);
	return -1;
    }
    

    return length;
}


static int addr_port_write(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct pci_internal * pci_state = (struct pci_internal *)dev->private_data;
    int reg_offset = port & 0x3; 
    uint8_t * reg_addr = ((uint8_t *)&(pci_state->addr_reg.val)) + reg_offset;


    if (length == 4) {
	if (reg_offset != 0) {
	    PrintError("Invalid Address Port Write\n");
	    return -1;
	}

	PrintDebug("Writing PCI 4 bytes Val=%x\n",  *(uint32_t *)src);

	*(uint32_t *)reg_addr = *(uint32_t *)src;
    } else if (length == 2) {
	if (reg_offset > 2) {
	    PrintError("Invalid Address Port Write\n");
	    return -1;
	}

	PrintDebug("Writing PCI 2 byte Val=%x\n",  *(uint16_t *)src);

	*(uint16_t *)reg_addr = *(uint16_t *)src;
    } else if (length == 1) {
	PrintDebug("Writing PCI 1 byte Val=%x\n",  *(uint8_t *)src);
	*(uint8_t *)reg_addr = *(uint8_t *)src;
    } else {
	PrintError("Invalid write length (%d) for PCI address register\n", length);
	return -1;
    }

    PrintDebug("Writing PCI Address Port(%x): %x\n", port, pci_state->addr_reg.val);

    return length;
}


static int data_port_read(ushort_t port, void * dst, uint_t length, struct vm_device * vmdev) {
    struct pci_internal * pci_state =  (struct pci_internal *)(vmdev->private_data);
    struct pci_device * pci_dev = NULL;
    uint_t reg_num = (pci_state->addr_reg.reg_num << 2) + (port & 0x3);
    int i;

    if (pci_state->addr_reg.bus_num != 0) {
	int i = 0;
	for (i = 0; i < length; i++) {
	    *((uint8_t *)dst + i) = 0xff;
	}

	return length;
    }

    PrintDebug("Reading PCI Data register. bus = %d, dev = %d, reg = %d (%x), cfg_reg = %x\n", 
	       pci_state->addr_reg.bus_num, 
	       pci_state->addr_reg.dev_num, 
	       reg_num, reg_num, 
	       pci_state->addr_reg.val);

    pci_dev = get_device(&(pci_state->bus_list[0]), pci_state->addr_reg.dev_num, pci_state->addr_reg.fn_num);
    
    if (pci_dev == NULL) {
	for (i = 0; i < length; i++) {
	    *(uint8_t *)((uint8_t *)dst + i) = 0xff;
	}

	return length;
    }

    for (i = 0; i < length; i++) {
	*(uint8_t *)((uint8_t *)dst + i) = pci_dev->config_space[reg_num + i];
    }

    PrintDebug("\tVal=%x, len=%d\n", *(uint32_t *)dst, length);

    return length;
}


static inline int is_cfg_reg_writable(uchar_t header_type, int reg_num) {
    if (header_type == 0x00) {
	switch (reg_num) {
	    case 0x00:
	    case 0x01:
	    case 0x02:
	    case 0x03:
	    case 0x08:
	    case 0x09:
	    case 0x0a:
	    case 0x0b:
	    case 0x0e:
	    case 0x3d:
		return 0;
                           
           default:
               return 1;
 
	}
    } else if (header_type == 0x80) {
	switch (reg_num) {
	    case 0x00:
	    case 0x01:
	    case 0x02:
	    case 0x03:
	    case 0x08:
	    case 0x09:
	    case 0x0a:
	    case 0x0b:
	    case 0x0e:
	    case 0x3d:
		return 0;
                           
           default:
               return 1;
 
	}
    } else {
	// PCI to PCI Bridge = 0x01
	// CardBus Bridge = 0x02

	// huh?
	PrintError("Invalid PCI Header type (0x%.2x)\n", header_type);

	return -1;
    }
}


static int bar_update(struct pci_device * pci, int bar_num, uint32_t new_val) {
    struct v3_pci_bar * bar = &(pci->bar[bar_num]);

    PrintDebug("Updating BAR Register  (Dev=%s) (bar=%d) (old_val=%x) (new_val=%x)\n", 
	       pci->name, bar_num, bar->val, new_val);

    switch (bar->type) {
	case PCI_BAR_IO: {
	    int i = 0;

		PrintDebug("\tRehooking %d IO ports from base %x to %x\n",
			   bar->num_ports, PCI_IO_BASE(bar->val), PCI_IO_BASE(new_val));
		
	    // only do this if pci device is enabled....
	    for (i = 0; i < bar->num_ports; i++) {

		v3_dev_unhook_io(pci->vm_dev, PCI_IO_BASE(bar->val) + i);

		v3_dev_hook_io(pci->vm_dev, PCI_IO_BASE(new_val) + i, 
			       bar->io_read, bar->io_write);
	    }

	    bar->val = new_val;

	    break;
	}
	case PCI_BAR_MEM32: {
	    v3_unhook_mem(pci->vm_dev->vm, (addr_t)(bar->val));
	    
	    if (bar->mem_read) {
		v3_hook_full_mem(pci->vm_dev->vm, PCI_MEM32_BASE(new_val), 
				 PCI_MEM32_BASE(new_val) + (bar->num_pages * PAGE_SIZE_4KB),
				 bar->mem_read, bar->mem_write, pci->vm_dev);
	    } else {
		PrintError("Write hooks not supported for PCI\n");
		return -1;
	    }

	    bar->val = new_val;

	    break;
	}
	case PCI_BAR_NONE: {
	    PrintDebug("Reprogramming an unsupported BAR register (Dev=%s) (bar=%d) (val=%x)\n", 
		       pci->name, bar_num, new_val);
	    break;
	}
	default:
	    PrintError("Invalid Bar Reg updated (bar=%d)\n", bar_num);
	    return -1;
    }

    return 0;
}


static int data_port_write(ushort_t port, void * src, uint_t length, struct vm_device * vmdev) {
    struct pci_internal * pci_state = (struct pci_internal *)vmdev->private_data;
    struct pci_device * pci_dev = NULL;
    uint_t reg_num = (pci_state->addr_reg.reg_num << 2) + (port & 0x3);
    int i;


    if (pci_state->addr_reg.bus_num != 0) {
	return length;
    }

    PrintDebug("Writing PCI Data register. bus = %d, dev = %d, fn = %d, reg = %d (%x) addr_reg = %x (val=%x, len=%d)\n", 
	       pci_state->addr_reg.bus_num, 
	       pci_state->addr_reg.dev_num, 
	       pci_state->addr_reg.fn_num,
	       reg_num, reg_num, 
	       pci_state->addr_reg.val,
	       *(uint32_t *)src, length);


    pci_dev = get_device(&(pci_state->bus_list[0]), pci_state->addr_reg.dev_num, pci_state->addr_reg.fn_num);
    
    if (pci_dev == NULL) {
	PrintError("Writing configuration space for non-present device (dev_num=%d)\n", 
		   pci_state->addr_reg.dev_num); 
	return -1;
    }
    

    for (i = 0; i < length; i++) {
	uint_t cur_reg = reg_num + i;
	int writable = is_cfg_reg_writable(pci_dev->config_header.header_type, cur_reg);
	
	if (writable == -1) {
	    PrintError("Invalid PCI configuration space\n");
	    return -1;
	}

	if (writable) {
	    pci_dev->config_space[cur_reg] = *(uint8_t *)((uint8_t *)src + i);

	    if ((cur_reg >= 0x10) && (cur_reg < 0x28)) {
		// BAR Register Update
		int bar_reg = ((cur_reg & ~0x3) - 0x10) / 4;
		
		pci_dev->bar_update_flag = 1;
		pci_dev->bar[bar_reg].updated = 1;
		
		// PrintDebug("Updating BAR register %d\n", bar_reg);

	    } else if ((cur_reg >= 0x30) && (cur_reg < 0x34)) {
		// Extension ROM update

		pci_dev->ext_rom_update_flag = 1;
	    } else if (cur_reg == 0x04) {
		// COMMAND update	     
		uint8_t command = *((uint8_t *)src + i);
		
		PrintError("command update for %s old=%x new=%x\n",
			   pci_dev->name, 
			   pci_dev->config_space[cur_reg],command);

		pci_dev->config_space[cur_reg] = command;	      

		if (pci_dev->cmd_update) {
		    pci_dev->cmd_update(pci_dev, (command & 0x01), (command & 0x02));
		}
		
	    } else if (cur_reg == 0x0f) {
		// BIST update
		pci_dev->config_header.BIST = 0x00;
	    }
	} else {
	    PrintError("PCI Write to read only register %d\n", cur_reg);
	}
    }

    if (pci_dev->config_update) {
	pci_dev->config_update(pci_dev, reg_num, length);
    }

    // Scan for BAR updated
    if (pci_dev->bar_update_flag) {
	for (i = 0; i < 6; i++) {
	    if (pci_dev->bar[i].updated) {
		int bar_offset = 0x10 + 4 * i;

		*(uint32_t *)(pci_dev->config_space + bar_offset) &= pci_dev->bar[i].mask;
		// check special flags....

		// bar_update
		if (bar_update(pci_dev, i, *(uint32_t *)(pci_dev->config_space + bar_offset)) == -1) {
		    PrintError("PCI Device %s: Bar update Error Bar=%d\n", pci_dev->name, i);
		    return -1;
		}

		pci_dev->bar[i].updated = 0;
	    }
	}
	pci_dev->bar_update_flag = 0;
    }

    if ((pci_dev->ext_rom_update_flag) && (pci_dev->ext_rom_update)) {
	pci_dev->ext_rom_update(pci_dev);
	pci_dev->ext_rom_update_flag = 0;
    }


    return length;
}



static int pci_reset_device(struct vm_device * dev) {
    PrintDebug("pci: reset device\n");    
    return 0;
}


static int pci_start_device(struct vm_device * dev) {
    PrintDebug("pci: start device\n");
    return 0;
}


static int pci_stop_device(struct vm_device * dev) {
    PrintDebug("pci: stop device\n");  
    return 0;
}



static int pci_free(struct vm_device * dev) {
    int i = 0;
    
    for (i = 0; i < 4; i++){
	v3_dev_unhook_io(dev, CONFIG_ADDR_PORT + i);
	v3_dev_unhook_io(dev, CONFIG_DATA_PORT + i);
    }
    
    return 0;
}



static void init_pci_busses(struct pci_internal * pci_state) {
    int i;

    for (i = 0; i < PCI_BUS_COUNT; i++) {
	pci_state->bus_list[i].bus_num = i;
	pci_state->bus_list[i].devices.rb_node = NULL;
	memset(pci_state->bus_list[i].dev_map, 0, sizeof(pci_state->bus_list[i].dev_map));
    }
}




static struct v3_device_ops dev_ops = {
    .free = pci_free,
    .reset = pci_reset_device,
    .start = pci_start_device,
    .stop = pci_stop_device,
};




static int pci_init(struct guest_info * vm, void * cfg_data) {
    struct pci_internal * pci_state = V3_Malloc(sizeof(struct pci_internal));
    int i = 0;
    
    PrintDebug("PCI internal at %p\n",(void *)pci_state);
    
    struct vm_device * dev = v3_allocate_device("PCI", &dev_ops, pci_state);
    
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "PCI");
	return -1;
    }

    
    pci_state->addr_reg.val = 0; 

    init_pci_busses(pci_state);
    
    PrintDebug("Sizeof config header=%d\n", (int)sizeof(struct pci_config_header));
    
    for (i = 0; i < 4; i++) {
	v3_dev_hook_io(dev, CONFIG_ADDR_PORT + i, &addr_port_read, &addr_port_write);
	v3_dev_hook_io(dev, CONFIG_DATA_PORT + i, &data_port_read, &data_port_write);
    }

    return 0;
}


device_register("PCI", pci_init)


static inline int init_bars(struct pci_device * pci_dev) {
    int i = 0;

    for (i = 0; i < 6; i++) {
	int bar_offset = 0x10 + (4 * i);

	if (pci_dev->bar[i].type == PCI_BAR_IO) {
	    int j = 0;
	    pci_dev->bar[i].mask = (~((pci_dev->bar[i].num_ports) - 1)) | 0x01;

	    pci_dev->bar[i].val = pci_dev->bar[i].default_base_port & pci_dev->bar[i].mask;
	    pci_dev->bar[i].val |= 0x00000001;

	    for (j = 0; j < pci_dev->bar[i].num_ports; j++) {
		// hook IO
		if (pci_dev->bar[i].default_base_port != 0xffff) {
		    if (v3_dev_hook_io(pci_dev->vm_dev, pci_dev->bar[i].default_base_port + j,
				       pci_dev->bar[i].io_read, pci_dev->bar[i].io_write) == -1) {
			PrintError("Could not hook default io port %x\n", pci_dev->bar[i].default_base_port + j);
			return -1;
		    }
		}
	    }

	    *(uint32_t *)(pci_dev->config_space + bar_offset) = pci_dev->bar[i].val;

	} else if (pci_dev->bar[i].type == PCI_BAR_MEM32) {
	    pci_dev->bar[i].mask = ~((pci_dev->bar[i].num_pages << 12) - 1);
	    pci_dev->bar[i].mask |= 0xf; // preserve the configuration flags

	    pci_dev->bar[i].val = pci_dev->bar[i].default_base_addr & pci_dev->bar[i].mask;

	    // hook memory
	    if (pci_dev->bar[i].mem_read) {
		// full hook
		v3_hook_full_mem(pci_dev->vm_dev->vm, pci_dev->bar[i].default_base_addr,
				 pci_dev->bar[i].default_base_addr + (pci_dev->bar[i].num_pages * PAGE_SIZE_4KB),
				 pci_dev->bar[i].mem_read, pci_dev->bar[i].mem_write, pci_dev->vm_dev);
	    } else if (pci_dev->bar[i].mem_write) {
		// write hook
		PrintError("Write hooks not supported for PCI devices\n");
		return -1;
		/*
		  v3_hook_write_mem(pci_dev->vm_dev->vm, pci_dev->bar[i].default_base_addr, 
		  pci_dev->bar[i].default_base_addr + (pci_dev->bar[i].num_pages * PAGE_SIZE_4KB),
		  pci_dev->bar[i].mem_write, pci_dev->vm_dev);
		*/
 	    } else {
		// set the prefetchable flag...
		pci_dev->bar[i].val |= 0x00000008;
	    }


	    *(uint32_t *)(pci_dev->config_space + bar_offset) = pci_dev->bar[i].val;

	} else if (pci_dev->bar[i].type == PCI_BAR_MEM16) {
	    PrintError("16 Bit memory ranges not supported (reg: %d)\n", i);
	    return -1;
	} else if (pci_dev->bar[i].type == PCI_BAR_NONE) {
	    pci_dev->bar[i].val = 0x00000000;
	    pci_dev->bar[i].mask = 0x00000000; // This ensures that all updates will be dropped
	    *(uint32_t *)(pci_dev->config_space + bar_offset) = pci_dev->bar[i].val;
	} else {
	    PrintError("Invalid BAR type for bar #%d\n", i);
	    return -1;
	}
    }

    return 0;
}


int v3_pci_set_irq_bridge(struct  vm_device * pci_bus, int bus_num, 
			  int (*raise_pci_irq)(struct vm_device * dev, uint_t intr_line), 
			  struct vm_device * bridge_dev) {
    struct pci_internal * pci_state = (struct pci_internal *)pci_bus->private_data;


    pci_state->bus_list[bus_num].raise_pci_irq = raise_pci_irq;
    pci_state->bus_list[bus_num].irq_bridge_dev = bridge_dev;

    return 0;
}

int v3_pci_raise_irq(struct vm_device * pci_bus, int bus_num, struct pci_device * dev) {
   struct pci_internal * pci_state = (struct pci_internal *)pci_bus->private_data;
   struct pci_bus * bus = &(pci_state->bus_list[bus_num]);

   return bus->raise_pci_irq(bus->irq_bridge_dev, dev->config_header.intr_pin);
}

// if dev_num == -1, auto assign 
struct pci_device * v3_pci_register_device(struct vm_device * pci,
					   pci_device_type_t dev_type, 
					   int bus_num,
					   int dev_num,
					   int fn_num,
					   const char * name,
					   struct v3_pci_bar * bars,
					   int (*config_update)(struct pci_device * pci_dev, uint_t reg_num, int length),
					   int (*cmd_update)(struct pci_device *pci_dev, uchar_t io_enabled, uchar_t mem_enabled),
					   int (*ext_rom_update)(struct pci_device * pci_dev),
					   struct vm_device * dev) {

    struct pci_internal * pci_state = (struct pci_internal *)pci->private_data;
    struct pci_bus * bus = &(pci_state->bus_list[bus_num]);
    struct pci_device * pci_dev = NULL;
    int i;

    if (dev_num > MAX_BUS_DEVICES) {
	PrintError("Requested Invalid device number (%d)\n", dev_num);
	return NULL;
    }

    if (dev_num == PCI_AUTO_DEV_NUM) {
	PrintDebug("Searching for free device number\n");
	if ((dev_num = get_free_dev_num(bus)) == -1) {
	    PrintError("No more available PCI slots on bus %d\n", bus->bus_num);
	    return NULL;
	}
    }
    
    PrintDebug("Checking for PCI Device\n");

    if (get_device(bus, dev_num, fn_num) != NULL) {
	PrintError("PCI Device already registered at slot %d on bus %d\n", 
		   dev_num, bus->bus_num);
	return NULL;
    }

    
    pci_dev = (struct pci_device *)V3_Malloc(sizeof(struct pci_device));

    if (pci_dev == NULL) {
	PrintError("Could not allocate pci device\n");
	return NULL;
    }

    memset(pci_dev, 0, sizeof(struct pci_device));

    
    switch (dev_type) {
	case PCI_STD_DEVICE:
	    pci_dev->config_header.header_type = 0x00;
	    break;
	case PCI_MULTIFUNCTION:
	    pci_dev->config_header.header_type = 0x80;
	    break;
	default:
	    PrintError("Unhandled PCI Device Type: %d\n", dev_type);
	    return NULL;
    }

    pci_dev->bus_num = bus_num;
    pci_dev->dev_num = dev_num;
    pci_dev->fn_num = fn_num;

    strncpy(pci_dev->name, name, sizeof(pci_dev->name));
    pci_dev->vm_dev = dev;

    // register update callbacks
    pci_dev->config_update = config_update;
    pci_dev->cmd_update = cmd_update;
    pci_dev->ext_rom_update = ext_rom_update;


    //copy bars
    for (i = 0; i < 6; i ++) {
	pci_dev->bar[i].type = bars[i].type;

	if (pci_dev->bar[i].type == PCI_BAR_IO) {
	    pci_dev->bar[i].num_ports = bars[i].num_ports;
	    pci_dev->bar[i].default_base_port = bars[i].default_base_port;
	    pci_dev->bar[i].io_read = bars[i].io_read;
	    pci_dev->bar[i].io_write = bars[i].io_write;
	} else if (pci_dev->bar[i].type == PCI_BAR_MEM32) {
	    pci_dev->bar[i].num_pages = bars[i].num_pages;
	    pci_dev->bar[i].default_base_addr = bars[i].default_base_addr;
	    pci_dev->bar[i].mem_read = bars[i].mem_read;
	    pci_dev->bar[i].mem_write = bars[i].mem_write;
	} else {
	    pci_dev->bar[i].num_pages = 0;
	    pci_dev->bar[i].default_base_addr = 0;
	    pci_dev->bar[i].mem_read = NULL;
	    pci_dev->bar[i].mem_write = NULL;
	}
    }

    if (init_bars(pci_dev) == -1) {
	PrintError("could not initialize bar registers\n");
	return NULL;
    }

    // add the device
    add_device_to_bus(bus, pci_dev);

#ifdef DEBUG_PCI
    pci_dump_state(pci_state);
#endif

    return pci_dev;
}



