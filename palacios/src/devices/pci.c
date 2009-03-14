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
};



struct pci_internal {
    // Configuration address register
    struct pci_addr_reg addr_reg;

    // Attached Busses
    struct pci_bus bus_list[PCI_BUS_COUNT];
};



#ifdef PCI_DEBUG
static void pci_dump_state(struct pci_internal * pci_state);
#endif

// Scan the dev_map bitmap for the first '0' bit
static int get_free_dev_num(struct pci_bus * bus) {
    int i, j;

    for (i = 0; i < sizeof(bus->dev_map); i++) {
	if (bus->dev_map[i] != 0xff) {
	    // availability
	    for (j = 0; j < 8; j++) {
		if (!(bus->dev_map[i] & (0x1 << j))) {
		    return i * 8 + j;
		}
	    }
	}
    }

    return -1;
}

static void allocate_dev_num(struct pci_bus * bus, int dev_num) {
    int major = dev_num / 8;
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

    if (dev->dev_num < tmp_dev->dev_num) {
      p = &(*p)->rb_left;
    } else if (dev->dev_num > tmp_dev->dev_num) {
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


static struct pci_device * get_device(struct pci_bus * bus, int dev_num) {
    struct rb_node * n = bus->devices.rb_node;
    struct pci_device * dev = NULL;

    while (n) {
	dev = rb_entry(n, struct pci_device, dev_tree_node);
	
	if (dev_num < dev->dev_num) {
	    n = n->rb_left;
	} else if (dev_num > dev->dev_num) {
	    n = n->rb_right;
	} else {
	    return dev;
	}
    }
    
    return NULL;
}



static int read_pci_header(struct pci_device * pci_dev, int reg_num, void * dst, int length) {

    if (length == 4) {
	*(uint32_t *)dst = *(uint32_t *)(pci_dev->header_space + reg_num);
    } else if (length == 2) {
	*(uint16_t *)dst = *(uint16_t *)(pci_dev->header_space + reg_num);
    } else if (length == 1) {
	*(uint8_t *)dst = pci_dev->header_space[reg_num];
    } else {
	PrintError("Invalid Read length (%d) for PCI configration header\n", length);
	return -1;
    }

    return length;
}


static int write_pci_header(struct pci_device * pci_dev, int reg_num, void * src, int length) {

    if (length == 4) {
	*(uint32_t *)(pci_dev->header_space + reg_num) = *(uint32_t *)src;
    } else if (length == 2) {
	*(uint16_t *)(pci_dev->header_space + reg_num) = *(uint16_t *)src;
    } else if (length == 1) {
	pci_dev->header_space[reg_num] = *(uint8_t *)src;
    } else {
	PrintError("Invalid Read length (%d) for PCI configration header\n", length);
	return -1;
    }

    // This is kind of ugly...
    if ((reg_num >= 0x10) && (reg_num < 0x27)) {
	int bar_num = (reg_num & ~0x3) - 0x10;
	uint32_t val = *(uint32_t *)(pci_dev->header_space + (reg_num & ~0x3));

	pci_dev->bar_update(pci_dev, bar_num, val);
    }

    return length;
}


static int addr_port_read(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct pci_internal * pci_state = (struct pci_internal *)dev->private_data;
    
    if (length != 4) {
	PrintError("Invalid read length (%d) for PCI address register\n", length);
	return -1;
    }
    
    PrintDebug("Reading PCI Address Port: %x\n", pci_state->addr_reg.val);
    *(uint32_t *)dst = pci_state->addr_reg.val;

    return length;
}


static int addr_port_write(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct pci_internal * pci_state = (struct pci_internal *)dev->private_data;

    if (length != 4) {
	PrintError("Invalid write length (%d) for PCI address register\n", length);
	return -1;
    }

    pci_state->addr_reg.val = *(uint32_t *)src;
    PrintDebug("Writing PCI Address Port: %x\n", pci_state->addr_reg.val);    

    return length;
}


static int data_port_read(ushort_t port, void * dst, uint_t length, struct vm_device * vmdev) {
    struct pci_internal * pci_state =  (struct pci_internal *)vmdev->private_data;;
    struct pci_device * pci_dev = NULL;
    uint_t reg_num = pci_state->addr_reg.reg_num;

        
    PrintDebug("Reading PCI Data register. bus = %d, dev = %d, reg = %d (%x)\n", 
	       pci_state->addr_reg.bus_num, 
	       pci_state->addr_reg.dev_num, 
	       reg_num);

    
    pci_dev = get_device(&(pci_state->bus_list[0]), pci_state->addr_reg.dev_num);
    
    if (pci_dev == NULL) {
	//*(uint32_t *)dst = 0xffffffff;

	PrintError("Reading configuration space for non-present device (dev_num=%d)\n", 
		   pci_state->addr_reg.dev_num); 

	return -1;
    }

    // Header register
    if (reg_num < 0x40) {
	return read_pci_header(pci_dev, reg_num, dst, length);
    }

    if (pci_dev->config_read) {
	return pci_dev->config_read(pci_dev, reg_num, dst, length);
    }


    if (length == 4) {
	*(uint32_t *)dst = *(uint32_t *)(pci_dev->config_space + reg_num - 0x40);
    } else if (length == 2) {
	*(uint16_t *)dst = *(uint16_t *)(pci_dev->config_space + reg_num - 0x40);
    } else if (length == 1) {
	*(uint8_t *)dst = pci_dev->config_space[reg_num - 0x40];
    } else {
	PrintError("Invalid Read length (%d) for PCI data register", length);
	return -1;
    }
	
    return length;
}


static int data_port_write(ushort_t port, void * src, uint_t length, struct vm_device * vmdev) {
    struct pci_internal * pci_state = (struct pci_internal *)vmdev->private_data;;
    struct pci_device * pci_dev = NULL;
    uint_t reg_num = pci_state->addr_reg.reg_num;
    
    
    PrintDebug("Writing PCI Data register. bus = %d, dev = %d, reg = %d (%x)\n", 
	       pci_state->addr_reg.bus_num, 
	       pci_state->addr_reg.dev_num, 
	       reg_num);

    pci_dev = get_device(&(pci_state->bus_list[0]), pci_state->addr_reg.dev_num);
    
    if (pci_dev == NULL) {
	PrintError("Writing configuration space for non-present device (dev_num=%d)\n", 
		   pci_state->addr_reg.dev_num); 
	return -1;
    }
    
    // Header register
    if (reg_num < 0x40) {
	return write_pci_header(pci_dev, reg_num, src, length);
    }
    

    if (pci_dev->config_write) {
	return pci_dev->config_write(pci_dev, reg_num, src, length);
    }


    if (length == 4) {
	*(uint32_t *)(pci_dev->config_space + reg_num - 0x40) = *(uint32_t *)src;
    } else if (length == 2) {
	*(uint16_t *)(pci_dev->config_space + reg_num - 0x40) =	*(uint16_t *)src;
    } else if (length == 1) {
	pci_dev->config_space[reg_num - 0x40] = *(uint8_t *)src;
    } else {
	PrintError("Invalid Write length (%d) for PCI data register", length);
	return -1;
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



static int pci_deinit_device(struct vm_device * dev) {
    int i = 0;
    
    for (i = 0; i < 4; i++){
	v3_dev_unhook_io(dev, CONFIG_ADDR_PORT + i);
	v3_dev_unhook_io(dev, CONFIG_DATA_PORT + i);
    }
    
    return 0;
}




static int init_i440fx(struct pci_internal * pci_state) {

    struct pci_device * dev = v3_pci_register_device(NULL, 0, "i440FX", 0, 
						     NULL, NULL, NULL, NULL);
    
    if (!dev) {
	return -1;
    }
    
    dev->header.vendor_id = 0x8086;
    dev->header.device_id = 0x1237;
    dev->header.revision = 0x0002;
    dev->header.subclass = 0x00; //  SubClass: host2pci
    dev->header.class = 0x06;    // Class: PCI bridge
    dev->header.header_type = 0x00;

    dev->bus_num = 0;
    
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



static int pci_init_device(struct vm_device * dev) {
    struct pci_internal * pci_state = (struct pci_internal *)dev->private_data;;
    int i = 0;
    
    PrintDebug("pci: init_device\n");

    // JRL: Fix this....
    //    dev->vm->pci = dev;   //should be in vmm_config.c
    
    pci_state->addr_reg.val = 0; 

    init_pci_busses(pci_state);

    if (init_i440fx(pci_state) == -1) {
	PrintError("Could not intialize i440fx\n");
	return -1;
    }
    
    for (i = 0; i < 4; i++) {
	v3_dev_hook_io(dev, CONFIG_ADDR_PORT + i, &addr_port_read, &addr_port_write);
	v3_dev_hook_io(dev, CONFIG_DATA_PORT + i, &data_port_read, &data_port_write);
    }

    return 0;
}


static struct vm_device_ops dev_ops = {
    .init = pci_init_device, 
    .deinit = pci_deinit_device,
    .reset = pci_reset_device,
    .start = pci_start_device,
    .stop = pci_stop_device,
};


struct vm_device * v3_create_pci() {
    struct pci_internal * pci_state = V3_Malloc(sizeof(struct pci_internal));
    
    PrintDebug("PCI internal at %p\n",(void *)pci_state);
    
    struct vm_device * device = v3_create_device("PCI", &dev_ops, pci_state);
    
    return device;
}





/* JRL: TODO This needs to be completely rethought... */
struct pci_bus * v3_get_pcibus(struct guest_info * vm, int bus_no) {
    //    struct pci_internal * pci_state = NULL;

    /*
      if (vm->pci == NULL) {
      PrintError("There is no PCI bus in guest %p\n", vm);
      return NULL;
      }
      
      pci_state = (struct pci_internal *)vm->pci->private_data;
      
      if ((bus_no >= 0) && (bus_no < PCI_BUS_COUNT)) {
      return &(pci_state->bus_list[bus_no]);
      }
    */
    return NULL;
}




// if dev_num == -1, auto assign 
struct pci_device * v3_pci_register_device(struct vm_device * dev,
					   uint_t bus_num,
					   const char * name,
					   int dev_num,
					   int (*config_read)(struct pci_device * pci_dev, uint_t reg_num, void * dst, int len),
					   int (*config_write)(struct pci_device * pci_dev, uint_t reg_num, void * src, int len),
					   int (*bar_update)(struct pci_device * pci_dev, uint_t bar_reg, uint32_t val),
					   void * private_data) {

    struct pci_internal * pci_state = (struct pci_internal *)dev->private_data;
    struct pci_bus * bus = &(pci_state->bus_list[bus_num]);
    struct pci_device * pci_dev = NULL;

    if (dev_num > MAX_BUS_DEVICES) {
	PrintError("Requested Invalid device number (%d)\n", dev_num);
	return NULL;
    }

    if (dev_num == -1) {
	if ((dev_num = get_free_dev_num(bus)) == -1) {
	    PrintError("No more available PCI slots on bus %d\n", bus->bus_num);
	    return NULL;
	}
    }
    
    if (get_device(bus, dev_num) != NULL) {
	PrintError("PCI Device already registered at slot %d on bus %d\n", 
		   dev_num, bus->bus_num);
	return NULL;
    }

    
    pci_dev = (struct pci_device *)V3_Malloc(sizeof(struct pci_device));

    if (pci_dev == NULL) {
	return NULL;
    }

    memset(pci_dev, 0, sizeof(struct pci_device));
	
    
    pci_dev->bus_num = bus_num;
    pci_dev->dev_num = dev_num;

    strncpy(pci_dev->name, name, sizeof(pci_dev->name));
    pci_dev->vm_dev = dev;

    pci_dev->config_read = config_read;
    pci_dev->config_write = config_write;
    pci_dev->bar_update = bar_update;

    pci_dev->priv_data = private_data;

    // add the device
    add_device_to_bus(bus, pci_dev);
    
#ifdef DEBUG_PCI
    pci_dump_state(pci_state);
#endif

    return pci_dev;
}



#ifdef DEBUG_PCI

static void pci_dump_state(struct pci_internal * pci_state) {
    struct rb_node * node = v3_rb_first(&(pci_state->bus_list[0].devices));
    struct pci_device * tmp_dev = NULL;
    
    PrintDebug("===PCI: Dumping state Begin ==========\n");
    
    do {
	tmp_dev = rb_entry(node, struct pci_device, dev_tree_node);

  	PrintDebug("PCI Device Number: %d (%s):\n", tmp_dev->dev_num,  tmp_dev->name);
	PrintDebug("irq = %d\n", tmp_dev->header.irq_line);
	PrintDebug("Vend ID: 0x%x\n", tmp_dev->header.vendor_id);
	PrintDebug("Device ID: 0x%x\n", tnp_dev->header.device_id);

    } while ((node = v3_rb_next(node)));
    
    PrintDebug("====PCI: Dumping state End==========\n");
}

#endif
