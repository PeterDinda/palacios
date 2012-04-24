/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2009, Chang Seok Bae <jhuell@gmail.com>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Jack Lange <jarusl@cs.northwestern.edu>
 *          Lei Xia <lxia@northwestern.edu>
 *          Chang Seok Bae <jhuell@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */ 
 
 

#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_rbtree.h>
#include <palacios/vmm_dev_mgr.h>

#include <devices/pci.h>
#include <devices/pci_types.h>

#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>


#include <devices/apic.h>


#ifndef V3_CONFIG_DEBUG_PCI
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define CONFIG_ADDR_PORT    0x0cf8
#define CONFIG_DATA_PORT    0x0cfc

#define PCI_DEV_IO_PORT_BASE 0xc000

#define PCI_BUS_COUNT 1

// This must always be a multiple of 8
#define MAX_BUS_DEVICES 32

#define PCI_CAP_ID_MSI 0x05
#define PCI_CAP_ID_MSIX 0x11


struct pci_addr_reg {
    union {
	uint32_t val;
	struct {
	    uint_t rsvd       : 2;
	    uint_t reg_num    : 6;
	    uint_t fn_num     : 3;
	    uint_t dev_num    : 5;
	    uint_t bus_num    : 8;
	    uint_t hi_reg_num : 4;
	    uint_t rsvd2      : 3;
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


    int (*raise_pci_irq)(struct pci_device * pci_dev, void * dev_data, struct v3_irq * vec);
    int (*lower_pci_irq)(struct pci_device * pci_dev, void * dev_data, struct v3_irq * vec);
    void * irq_dev_data;
};



struct pci_internal {
    // Configuration address register
    struct pci_addr_reg addr_reg;

    // Base IO Port which PCI devices will register with...
    uint16_t dev_io_base; 

    // Attached Busses
    struct pci_bus bus_list[PCI_BUS_COUNT];
};



struct cfg_range_hook {
    uint32_t start;
    uint32_t length;

    int (*write)(struct pci_device * pci_dev, uint32_t offset, 
		 void * src, uint_t length, void * private_data);

    int (*read)(struct pci_device * pci_dev, uint32_t offset, 
		void * dst, uint_t length, void * private_data);

    void * private_data;

    struct list_head list_node;
};



struct pci_cap {
    uint8_t id;
    uint32_t offset;
    uint8_t enabled;

    struct list_head cap_node;
};


// These mark read only fields in the pci config header. 
// If a bit is 1, then the field is writable in the header
/* Notes: 
 * BIST is disabled by default (All writes to it will be dropped 
 * Cardbus CIS is disabled (All writes are dropped)
 * Writes to capability pointer are disabled
 */
static uint8_t pci_hdr_write_mask_00[64] = { 0x00, 0x00, 0x00, 0x00, /* Device ID, Vendor ID */
					     0xbf, 0xff, 0x00, 0xf9, /* Command, status */
					     0x00, 0x00, 0x00, 0x00, /* Revision ID, Class code */
					     0x00, 0xff, 0x00, 0x00, /* CacheLine Size, Latency Timer, Header Type, BIST */
					     0xff, 0xff, 0xff, 0xff, /* BAR 0 */
					     0xff, 0xff, 0xff, 0xff, /* BAR 1 */
					     0xff, 0xff, 0xff, 0xff, /* BAR 2 */
					     0xff, 0xff, 0xff, 0xff, /* BAR 3 */
					     0xff, 0xff, 0xff, 0xff, /* BAR 4 */
					     0xff, 0xff, 0xff, 0xff, /* BAR 5 */
					     0x00, 0x00, 0x00, 0x00, /* CardBus CIS Ptr */
					     0xff, 0xff, 0xff, 0xff, /* SubSystem Vendor ID, SubSystem ID */
					     0xff, 0xff, 0xff, 0xff, /* ExpRom BAR */
					     0x00, 0x00, 0x00, 0x00, /* CAP ptr (0xfc to enable), RSVD */
					     0x00, 0x00, 0x00, 0x00, /* Reserved */
					     0xff, 0x00, 0x00, 0x00 /* INTR Line, INTR Pin, MIN_GNT, MAX_LAT */
}; 




#ifdef V3_CONFIG_DEBUG_PCI

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




// There won't be many hooks at all, so unordered lists are acceptible for now 
static struct cfg_range_hook * find_cfg_range_hook(struct pci_device * pci, uint32_t start, uint32_t length) {
    uint32_t end = start + length - 1; // end is inclusive
    struct cfg_range_hook * hook = NULL;

    list_for_each_entry(hook, &(pci->cfg_hooks), list_node) {
	uint32_t hook_end = hook->start + hook->length - 1;
	if (!((hook->start > end) || (hook_end < start))) {
	    return hook;
	}
    }
    
    return NULL;
}


int v3_pci_hook_config_range(struct pci_device * pci, 
			     uint32_t start, uint32_t length, 
			     int (*write)(struct pci_device * pci_dev, uint32_t offset, 
						 void * src, uint_t length, void * private_data), 
			     int (*read)(struct pci_device * pci_dev, uint32_t offset, 
						void * dst, uint_t length, void * private_data), 
			     void * private_data) {
    struct cfg_range_hook * hook = NULL;    
    

    if (find_cfg_range_hook(pci, start, length)) {
	PrintError("Tried to hook an already hooked config region\n");
	return -1;
    }
    
    hook = V3_Malloc(sizeof(struct cfg_range_hook));

    if (!hook) {
	PrintError("Could not allocate range hook\n");
	return -1;
    }

    memset(hook, 0, sizeof(struct cfg_range_hook));

    hook->start = start;
    hook->length = length;
    hook->private_data = private_data;
    hook->write = write;
    hook->read = read;

    list_add(&(hook->list_node), &(pci->cfg_hooks));

    return 0;

}




// Note byte ordering: LSB -> MSB
static uint8_t msi_32_rw_bitmask[10] = { 0x00, 0x00,                     /* ID, next ptr */
					 0x71, 0x00,                     /* MSG CTRL */
					 0xfc, 0xff, 0xff, 0xff,         /* MSG ADDR */
					 0xff, 0xff};                    /* MSG DATA */

static uint8_t msi_64_rw_bitmask[14] = { 0x00, 0x00,                     /* ID, next ptr */
					 0x71, 0x00,                     /* MSG CTRL */
					 0xfc, 0xff, 0xff, 0xff,         /* MSG LO ADDR */
					 0xff, 0xff, 0xff, 0xff,         /* MSG HI ADDR */
					 0xff, 0xff};                    /* MSG DATA */

static uint8_t msi_64pervect_rw_bitmask[24] = { 0x00, 0x00,              /* ID, next ptr */
						0x71, 0x00,              /* MSG CTRL */
						0xfc, 0xff, 0xff, 0xff,  /* MSG LO CTRL */
						0xff, 0xff, 0xff, 0xff,  /* MSG HI ADDR */
						0xff, 0xff,              /* MSG DATA */
						0x00, 0x00,              /* RSVD */
						0xff, 0xff, 0xff, 0xff,  
						0x00, 0x00, 0x00, 0x00}; 

static uint8_t msix_rw_bitmask[12] = { 0x00, 0x00,                       /* ID, next ptr */
				       0x00, 0x80, 
				       0xff, 0xff, 0xff, 0xff,
				       0x08, 0xff, 0xff, 0xff};


/* I am completely guessing what the format is here. 
   I only have version 1 of the PCIe spec and cannot download version 2 or 3 
   without paying the PCI-SIG $3000 a year for membership. 
   So this is just cobbled together from the version 1 spec and KVM. 
*/ 


static uint8_t pciev1_rw_bitmask[20] = { 0x00, 0x00, /* ID, next ptr */
					 0x00, 0x00, /* PCIE CAP register */
					 0x00, 0x00, 0x00, 0x00, /* DEV CAP */
					 0xff, 0xff, /* DEV CTRL */
					 0x0f, 0x00, /* DEV STATUS */
					 0x00, 0x00, 0x00, 0x00, /* LINK CAP */
					 0xfb, 0x01, /* LINK CTRL */
					 0x00, 0x00  /* LINK STATUS */ 
};


static uint8_t pciev2_rw_bitmask[60] = { 0x00, 0x00, /* ID, next ptr */
					 0x00, 0x00, /* PCIE CAP register */
					 0x00, 0x00, 0x00, 0x00, /* DEV CAP */
					 0xff, 0xff, /* DEV CTRL */
					 0x0f, 0x00, /* DEV STATUS */
					 0x00, 0x00, 0x00, 0x00, /* LINK CAP */
					 0xfb, 0x01, /* LINK CTRL */
					 0x00, 0x00, /* LINK STATUS */ 
					 0x00, 0x00, 0x00, 0x00, /* SLOT CAP ?? */
					 0x00, 0x00, /* SLOT CTRL ?? */
					 0x00, 0x00, /* SLOT STATUS */
					 0x00, 0x00, /* ROOT CTRL */
					 0x00, 0x00, /* ROOT CAP */
					 0x00, 0x00, 0x00, 0x00, /* ROOT STATUS */
					 0x00, 0x00, 0x00, 0x00, /* WHO THE FUCK KNOWS */
					 0x00, 0x00, 0x00, 0x00, 
					 0x00, 0x00, 0x00, 0x00, 
					 0x00, 0x00, 0x00, 0x00, 
					 0x00, 0x00, 0x00, 0x00 
};

static uint8_t pm_rw_bitmask[] = { 0x00, 0x00, /* ID, next ptr */
				   0x00, 0x00, /* PWR MGMT CAPS */
				   0x03, 0x9f, /* PWR MGMT CTRL */
				   0x00, 0x00  /* PMCSR_BSE, Data */
};



int cap_write(struct pci_device * pci, uint32_t offset, void * src, uint_t length, void * private_data) {
    struct pci_cap * cap = private_data;
    uint32_t cap_offset = cap->offset;
    pci_cap_type_t cap_type = cap->id;

    uint32_t write_offset = offset - cap_offset;
    void * cap_ptr = &(pci->config_space[cap_offset + 2]);    
    int i = 0;

    int msi_was_enabled = 0;
    int msix_was_enabled = 0;


    V3_Print("CAP write trapped (val=%x, cfg_offset=%d, write_offset=%d)\n", *(uint32_t *)src, offset, write_offset);

    if (cap_type == PCI_CAP_MSI) {
	struct msi_msg_ctrl * msg_ctrl = cap_ptr;

	if (msg_ctrl->msi_enable == 1) {
	    msi_was_enabled = 1;
	}
    } else if (cap_type == PCI_CAP_MSIX) {
	struct msix_cap * msix_cap = cap_ptr;

	if (msix_cap->msg_ctrl.msix_enable == 1) {
	    msix_was_enabled = 1;
	}
    }

    for (i = 0; i < length; i++) {
	uint8_t mask = 0;

	if (cap_type == PCI_CAP_MSI) {
	    struct msi_msg_ctrl * msg_ctrl = cap_ptr;

	    V3_Print("MSI Cap Ctrl=%x\n", *(uint16_t *)pci->msi_cap);
	    V3_Print("MSI ADDR=%x\n", *(uint32_t *)(cap_ptr + 2));
	    V3_Print("MSI HI ADDR=%x\n", *(uint32_t *)(cap_ptr + 6));
	    V3_Print("MSI Data=%x\n", *(uint16_t *)(cap_ptr + 10));

	    if (msg_ctrl->cap_64bit) {
		if (msg_ctrl->per_vect_mask) {
		    mask = msi_64pervect_rw_bitmask[write_offset];
		} else {
		    mask = msi_64_rw_bitmask[write_offset];
		}
	    } else {
		mask = msi_32_rw_bitmask[write_offset];
	    }
	} else if (cap_type == PCI_CAP_MSIX) {
	    mask = msix_rw_bitmask[write_offset];
	} else if (cap_type == PCI_CAP_PCIE) {
	    struct pcie_cap_reg * pcie_cap = cap_ptr;

	    if (pcie_cap->version == 1) {
		mask = pciev1_rw_bitmask[write_offset];
	    } else if (pcie_cap->version == 2) {
		mask = pciev2_rw_bitmask[write_offset];
	    } else {
		return 0;
	    }
	} else if (cap_type == PCI_CAP_PM) {
	    mask = pm_rw_bitmask[write_offset];
	}

	pci->config_space[offset + i] &= ~mask;
	pci->config_space[offset + i] |= ((*(uint8_t *)(src + i)) & mask);

	write_offset++;
    }


    if (pci->cmd_update) {

	/* Detect changes to interrupt types for cmd updates */
	if (cap_type == PCI_CAP_MSI) {
	    struct msi_msg_ctrl * msg_ctrl = cap_ptr;
	    
	    V3_Print("msi_was_enabled=%d, msi_is_enabled=%d\n", msi_was_enabled,  msg_ctrl->msi_enable);

	    if ((msg_ctrl->msi_enable == 1) && (msi_was_enabled == 0)) {
		pci->irq_type = IRQ_MSI;
		pci->cmd_update(pci, PCI_CMD_MSI_ENABLE, msg_ctrl->mult_msg_enable, pci->priv_data);
	    } else if ((msg_ctrl->msi_enable == 0) && (msi_was_enabled == 1)) {
		pci->irq_type = IRQ_NONE;
		pci->cmd_update(pci, PCI_CMD_MSI_DISABLE, 0, pci->priv_data);
	    }
	} else if (cap_type == PCI_CAP_MSIX) {
	    struct msix_cap * msix_cap = cap_ptr;

	    if ((msix_cap->msg_ctrl.msix_enable == 1) && (msix_was_enabled == 0)) {
		pci->irq_type = IRQ_MSIX;
		pci->cmd_update(pci, PCI_CMD_MSIX_ENABLE, msix_cap->msg_ctrl.table_size, pci->priv_data);
	    } else if ((msix_cap->msg_ctrl.msix_enable == 0) && (msix_was_enabled == 1)) {
		pci->irq_type = IRQ_NONE;
		pci->cmd_update(pci, PCI_CMD_MSIX_DISABLE, msix_cap->msg_ctrl.table_size, pci->priv_data);
	    }
	}
    }

    return 0;
}


static int init_pci_cap(struct pci_device * pci, pci_cap_type_t cap_type, uint_t cap_offset) {
    void * cap_ptr = &(pci->config_space[cap_offset + 2]);

    if (cap_type == PCI_CAP_MSI) {
	struct msi32_msg_addr * msi = cap_ptr;

	// We only expose a basic 32 bit MSI interface
	msi->msg_ctrl.msi_enable = 0;
	msi->msg_ctrl.mult_msg_enable = 0;
	msi->msg_ctrl.cap_64bit = 0;
	msi->msg_ctrl.per_vect_mask = 0;
	
	msi->addr.val = 0; 
	msi->data.val = 0;

    } else if (cap_type == PCI_CAP_MSIX) {
	
	

    } else if (cap_type == PCI_CAP_PCIE) {
	struct pcie_cap_v2 * pcie = cap_ptr;
	
	// The v1 and v2 formats are identical for the first X bytes
	// So we use the v2 struct, and only modify extended fields if v2 is detected
	
	pcie->dev_cap.fn_level_reset = 0;
	
	pcie->dev_ctrl.val &= 0x70e0; // only preserve max_payload_size and max_read_req_size untouched
	pcie->dev_ctrl.relaxed_order_enable = 1;
	pcie->dev_ctrl.no_snoop_enable = 1;

	pcie->dev_status.val = 0;

	pcie->link_cap.val &= 0x0003ffff;

	pcie->link_status.val &= 0x03ff;
	
	if (pcie->pcie_cap.version >= 2) {
	    pcie->slot_cap = 0;
	    pcie->slot_ctrl = 0;
	    pcie->slot_status = 0;

	    pcie->root_ctrl = 0;
	    pcie->root_cap = 0;
	    pcie->root_status = 0;
	}
    } else if (cap_type == PCI_CAP_PM) {

    }


    return 0;
}


// enumerate all capabilities and disable them.
static int scan_pci_caps(struct pci_device * pci) {
    uint_t cap_offset = pci->config_header.cap_ptr;
        
    V3_Print("Scanning for Capabilities (cap_offset=%d)\n", cap_offset);

    while (cap_offset != 0) {
	uint8_t id = pci->config_space[cap_offset];
	uint8_t next = pci->config_space[cap_offset + 1];

	V3_Print("Found Capability 0x%x at offset %d (0x%x)\n", 
		 id, cap_offset, cap_offset);

	struct pci_cap * cap = V3_Malloc(sizeof(struct pci_cap));

	if (!cap) {
	    PrintError("Error allocating PCI CAP info\n");
	    return -1;
	}
	memset(cap, 0, sizeof(struct pci_cap));
	
	cap->id = id;
	cap->offset = cap_offset;

	list_add(&(cap->cap_node), &(pci->capabilities));

	// set correct init values 
	init_pci_cap(pci, id, cap_offset);


	// set to the next pointer
	cap_offset = next;
    }

    // Disable Capabilities
    pci->config_header.cap_ptr = 0;

    // Hook Cap pointer to return cached config space value
    if (v3_pci_hook_config_range(pci, 0x34, 1, 
				 NULL, NULL, NULL) == -1) {
	PrintError("Could not hook cap pointer\n");
	return -1;
    }
    


/*
    // Disable all PCIE extended capabilities for now
    pci->config_space[0x100] = 0;
    pci->config_space[0x101] = 0;
    pci->config_space[0x102] = 0;
    pci->config_space[0x103] = 0;
*/  


    return 0;

}

int v3_pci_enable_capability(struct pci_device * pci, pci_cap_type_t cap_type) {
    uint32_t size = 0;
    struct pci_cap * tmp_cap = NULL;
    struct pci_cap * cap = NULL;
    void * cap_ptr = NULL;


    list_for_each_entry(tmp_cap, &(pci->capabilities), cap_node) {
	if (tmp_cap->id == cap_type) {
	    cap = tmp_cap;
	    break;
	}
    }

    if ((cap == NULL) || (cap->enabled)) {
	return -1;
    }


    V3_Print("Found Capability %x at %x (%d)\n", cap_type, cap->offset, cap->offset);

    // found the capability

    // mark it as enabled
    cap->enabled = 1;

    cap_ptr = &(pci->config_space[cap->offset + 2]);

    if (cap_type == PCI_CAP_MSI) {
	pci->msi_cap = cap_ptr;
	
	if (pci->msi_cap->cap_64bit) {
	    if (pci->msi_cap->per_vect_mask) {
		// 64 bit MSI w/ per vector masking
		size = 22;
	    } else {
		// 64 bit MSI
		size = 12;
	    }
	} else {
	    // 32 bit MSI
	    size = 8;
	}
    } else if (cap_type == PCI_CAP_MSIX) {
	pci->msix_cap = cap_ptr;
	
	// disable passthrough for MSIX BAR

	pci->bar[pci->msix_cap->bir].type = PCI_BAR_MEM32;

	size = 10;
    } else if (cap_type == PCI_CAP_PCIE) {
	struct pcie_cap_reg * pcie_cap = (struct pcie_cap_reg *)&(pci->config_space[cap->offset + 2]);

	if (pcie_cap->version == 1) {
	    size = 20;
	} else if (pcie_cap->version == 2) {
	    size = 60;
	} else {
	    return -1;
	}
    } else if (cap_type == PCI_CAP_PM) {
	size = 8;
    }


    V3_Print("Hooking capability range (offset=%d, size=%d)\n", cap->offset, size);

    if (v3_pci_hook_config_range(pci, cap->offset, size + 2, 
				 cap_write, NULL, cap) == -1) {
	PrintError("Could not hook config range (start=%d, size=%d)\n", 
		   cap->offset + 2, size);
	return -1;
    }



    // link it to the active capabilities list
    pci->config_space[cap->offset + 1] = pci->config_header.cap_ptr;
    pci->config_header.cap_ptr = cap->offset; // add to the head of the list

    return 0;
}




static int addr_port_read(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pci_internal * pci_state = priv_data;
    int reg_offset = port & 0x3;
    uint8_t * reg_addr = ((uint8_t *)&(pci_state->addr_reg.val)) + reg_offset;

    PrintDebug("Reading PCI Address Port (%x): %x len=%d\n", port, pci_state->addr_reg.val, length);

    if (reg_offset + length > 4) {
	PrintError("Invalid Address port write\n");
	return -1;
    }

    memcpy(dst, reg_addr, length);    

    return length;
}


static int addr_port_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pci_internal * pci_state = priv_data;
    int reg_offset = port & 0x3; 
    uint8_t * reg_addr = ((uint8_t *)&(pci_state->addr_reg.val)) + reg_offset;

    if (reg_offset + length > 4) {
	PrintError("Invalid Address port write\n");
	return -1;
    }

    // Set address register
    memcpy(reg_addr, src, length);

    PrintDebug("Writing PCI Address Port(%x): AddrReg=%x (op_val = %x, len=%d) \n", port, pci_state->addr_reg.val, *(uint32_t *)src, length);

    return length;
}


static int data_port_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data) {
    struct pci_internal * pci_state =  priv_data;
    struct pci_device * pci_dev = NULL;
    uint_t reg_num =  (pci_state->addr_reg.hi_reg_num << 16) +(pci_state->addr_reg.reg_num << 2) + (port & 0x3);
    int i = 0;
    int bytes_left = length;

    if (pci_state->addr_reg.bus_num != 0) {
	memset(dst, 0xff, length);
	return length;
    }


    pci_dev = get_device(&(pci_state->bus_list[0]), pci_state->addr_reg.dev_num, pci_state->addr_reg.fn_num);
    
    if (pci_dev == NULL) {
	memset(dst, 0xff, length);
	return length;
    } 

    PrintDebug("Reading PCI Data register. bus = %d, dev = %d, fn = %d, reg = %d (%x), cfg_reg = %x\n", 
	       pci_state->addr_reg.bus_num, 
	       pci_state->addr_reg.dev_num, 
	       pci_state->addr_reg.fn_num,
	       reg_num, reg_num, 
	       pci_state->addr_reg.val);


    while (bytes_left > 0) {
	struct cfg_range_hook * cfg_hook  = find_cfg_range_hook(pci_dev, reg_num + i, 1);
	void * cfg_dst =  &(pci_dev->config_space[reg_num + i]);

	if (cfg_hook) {
	    uint_t range_len = cfg_hook->length - ((reg_num + i) - cfg_hook->start);
	    range_len = (range_len > bytes_left) ? bytes_left : range_len;

	    if (cfg_hook->read) {
		cfg_hook->read(pci_dev, reg_num + i, cfg_dst, range_len, cfg_hook->private_data);
	    }
	    
	    bytes_left -= range_len;
	    i += range_len;
	} else {
	    if (pci_dev->config_read) {
		if (pci_dev->config_read(pci_dev, reg_num + i, cfg_dst, 1, pci_dev->priv_data) != 0) {
		    PrintError("Error in config_read from PCI device (%s)\n", pci_dev->name);
		}
	    }

	    bytes_left--;
	    i++;
	} 
    }	    

    memcpy(dst, &(pci_dev->config_space[reg_num]), length);
	    
    PrintDebug("\tVal=%x, len=%d\n", *(uint32_t *)dst, length);

    return length;
}



static int bar_update(struct pci_device * pci_dev, uint32_t offset, 
		      void * src, uint_t length, void * private_data) {
    struct v3_pci_bar * bar = (struct v3_pci_bar *)private_data;
    int bar_offset = offset & ~0x03;
    int bar_num = (bar_offset - 0x10) / 4;
    uint32_t new_val = *(uint32_t *)src;
    
    PrintDebug("Updating BAR Register  (Dev=%s) (bar=%d) (old_val=0x%x) (new_val=0x%x)\n", 
	       pci_dev->name, bar_num, bar->val, new_val);

    // Cache the changes locally
    memcpy(&(pci_dev->config_space[offset]), src, length);

    if (bar->type == PCI_BAR_PASSTHROUGH) {
        if (bar->bar_write(bar_num, (void *)(pci_dev->config_space + bar_offset), bar->private_data) == -1) {
	    PrintError("Error in Passthrough bar write operation\n");
	    return -1;
	}
	
	return 0;
    }
   
    // Else we are a virtualized BAR

    *(uint32_t *)(pci_dev->config_space + offset) &= bar->mask;

    switch (bar->type) {
	case PCI_BAR_IO: {
	    int i = 0;

	    PrintDebug("\tRehooking %d IO ports from base 0x%x to 0x%x for %d ports\n",
		       bar->num_ports, PCI_IO_BASE(bar->val), PCI_IO_BASE(new_val),
		       bar->num_ports);
		
	    // only do this if pci device is enabled....
	    if (!(pci_dev->config_header.status & 0x1)) {
		PrintError("PCI Device IO space not enabled\n");
	    }

	    for (i = 0; i < bar->num_ports; i++) {

		PrintDebug("Rehooking PCI IO port (old port=%u) (new port=%u)\n",  
			   PCI_IO_BASE(bar->val) + i, PCI_IO_BASE(new_val) + i);

		v3_unhook_io_port(pci_dev->vm, PCI_IO_BASE(bar->val) + i);

		if (v3_hook_io_port(pci_dev->vm, PCI_IO_BASE(new_val) + i, 
				    bar->io_read, bar->io_write, 
				    bar->private_data) == -1) {

		    PrintError("Could not hook PCI IO port (old port=%u) (new port=%u)\n",  
			       PCI_IO_BASE(bar->val) + i, PCI_IO_BASE(new_val) + i);
		    v3_print_io_map(pci_dev->vm);
		    return -1;
		}
	    }

	    bar->val = new_val;

	    break;
	}
	case PCI_BAR_MEM32: {
	    v3_unhook_mem(pci_dev->vm, V3_MEM_CORE_ANY, (addr_t)(bar->val));
	    
	    if (bar->mem_read) {
		v3_hook_full_mem(pci_dev->vm, V3_MEM_CORE_ANY, PCI_MEM32_BASE(new_val), 
				 PCI_MEM32_BASE(new_val) + (bar->num_pages * PAGE_SIZE_4KB),
				 bar->mem_read, bar->mem_write, pci_dev->priv_data);
	    } else {
		PrintError("Write hooks not supported for PCI\n");
		return -1;
	    }

	    bar->val = new_val;

	    break;
	}
	case PCI_BAR_NONE: {
	    PrintDebug("Reprogramming an unsupported BAR register (Dev=%s) (bar=%d) (val=%x)\n", 
		       pci_dev->name, bar_num, new_val);
	    break;
	}
	default:
	    PrintError("Invalid Bar Reg updated (bar=%d)\n", bar_num);
	    return -1;
    }

    return 0;
}


static int data_port_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct pci_internal * pci_state = priv_data;
    struct pci_device * pci_dev = NULL;
    uint_t reg_num = (pci_state->addr_reg.hi_reg_num << 16) +(pci_state->addr_reg.reg_num << 2) + (port & 0x3);
    int i = 0;
    int ret = length;

    if (pci_state->addr_reg.bus_num != 0) {
	return length;
    }

    PrintDebug("Writing PCI Data register. bus = %d, dev = %d, fn = %d, reg = %d (0x%x) addr_reg = 0x%x (val=0x%x, len=%d)\n", 
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

    /* update the config space
       If a hook has been registered for a given region, call the hook with the max write length
    */ 
    while (length > 0) {
	struct cfg_range_hook * cfg_hook  = find_cfg_range_hook(pci_dev, reg_num + i, 1);

	if (cfg_hook) {
	    uint_t range_len = cfg_hook->length - ((reg_num + i) - cfg_hook->start);
	    range_len = (range_len > length) ? length : range_len;
	    
	    if (cfg_hook->write) {
		cfg_hook->write(pci_dev, reg_num + i, (void *)(src + i), range_len, cfg_hook->private_data);
	    }

	    length -= range_len;
	    i += range_len;
	} else {
	    // send the writes to the cached config space, and to the generic callback if present
	    uint8_t mask = 0xff;

	    if (reg_num < 64) {
		mask = pci_hdr_write_mask_00[reg_num + i];
	    }
	    
	    if (mask != 0) {
		uint8_t new_val = *(uint8_t *)(src + i);
		uint8_t old_val = pci_dev->config_space[reg_num + i];

		pci_dev->config_space[reg_num + i] = ((new_val & mask) | (old_val & ~mask));
		
		if (pci_dev->config_write) {
		    pci_dev->config_write(pci_dev, reg_num + i, &(pci_dev->config_space[reg_num + i]), 1, pci_dev->priv_data);
		}
	    }	    

	    length--;
	    i++;
	}
    }

    return ret;
}



static int exp_rom_write(struct pci_device * pci_dev, uint32_t offset, 
			 void * src, uint_t length, void * private_data) {
    int bar_offset = offset & ~0x03;

    if (pci_dev->exp_rom_update) {
	pci_dev->exp_rom_update(pci_dev, (void *)(pci_dev->config_space + bar_offset), pci_dev->priv_data);
	
	return 0;
    }

    PrintError("Expansion ROM update not handled. Will appear to not Exist\n");

    return 0;
}


static int cmd_write(struct pci_device * pci_dev, uint32_t offset, 
		     void * src, uint_t length, void * private_data) {

    int i = 0;

    struct pci_cmd_reg old_cmd;
    struct pci_cmd_reg new_cmd;
    old_cmd.val = pci_dev->config_header.command;

    for (i = 0; i < length; i++) {
        uint8_t mask = pci_hdr_write_mask_00[offset + i];
	uint8_t new_val = *(uint8_t *)(src + i);
	uint8_t old_val = pci_dev->config_space[offset + i];

	pci_dev->config_space[offset + i] = ((new_val & mask) | (old_val & ~mask));
    }

    new_cmd.val = pci_dev->config_header.command;


    if (pci_dev->cmd_update) {
	if ((new_cmd.intx_disable == 1) && (old_cmd.intx_disable == 0)) {
	    pci_dev->irq_type = IRQ_NONE;
	    pci_dev->cmd_update(pci_dev, PCI_CMD_INTX_DISABLE, 0, pci_dev->priv_data);
	} else if ((new_cmd.intx_disable == 0) && (old_cmd.intx_disable == 1)) {
	    pci_dev->irq_type = IRQ_INTX;
	    pci_dev->cmd_update(pci_dev, PCI_CMD_INTX_ENABLE, 0, pci_dev->priv_data);
	}


	if ((new_cmd.dma_enable == 1) && (old_cmd.dma_enable == 0)) {
	    pci_dev->cmd_update(pci_dev, PCI_CMD_DMA_ENABLE, 0, pci_dev->priv_data);
	} else if ((new_cmd.dma_enable == 0) && (old_cmd.dma_enable == 1)) {
	    pci_dev->cmd_update(pci_dev, PCI_CMD_DMA_DISABLE, 0, pci_dev->priv_data);
	}
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


static int pci_free(struct pci_internal * pci_state) {
    int i;


    // cleanup devices
    for (i = 0; i < PCI_BUS_COUNT; i++) {
	struct pci_bus * bus = &(pci_state->bus_list[i]);
	struct rb_node * node = v3_rb_first(&(bus->devices));
	struct pci_device * dev = NULL;

	while (node) {
	    dev = rb_entry(node, struct pci_device, dev_tree_node);
	    node = v3_rb_next(node);
	    
	    v3_rb_erase(&(dev->dev_tree_node), &(bus->devices));
	    
	    // Free config range hooks
	    { 
		struct cfg_range_hook * hook = NULL;
		struct cfg_range_hook * tmp = NULL;
		list_for_each_entry_safe(hook, tmp, &(dev->cfg_hooks), list_node) {
		    list_del(&(hook->list_node));
		    V3_Free(hook);
		}
	    }

	    // Free caps
	    {
		struct pci_cap * cap = NULL;
		struct pci_cap * tmp = NULL;
		list_for_each_entry_safe(cap, tmp, &(dev->cfg_hooks), cap_node) {
		    list_del(&(cap->cap_node));
		    V3_Free(cap);
		}
	    }

	    V3_Free(dev);
	}

    }
    
    V3_Free(pci_state);
    return 0;
}

#ifdef V3_CONFIG_CHECKPOINT

#include <palacios/vmm_sprintf.h>

static int pci_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct pci_internal * pci = (struct pci_internal *)private_data;
    char buf[128];
    int i = 0;    
    
    v3_chkpt_save_32(ctx, "ADDR_REG", &(pci->addr_reg.val));
    v3_chkpt_save_16(ctx, "IO_BASE", &(pci->dev_io_base));

    for (i = 0; i < PCI_BUS_COUNT; i++) {
	struct pci_bus * bus = &(pci->bus_list[i]);
	struct rb_node * node = v3_rb_first(&(bus->devices));
	struct pci_device * dev = NULL;
	struct v3_chkpt_ctx * bus_ctx = NULL;

	snprintf(buf, 128, "pci-%d\n", i);
	
	bus_ctx = v3_chkpt_open_ctx(ctx->chkpt, ctx, buf);

	while (node) {
	    struct v3_chkpt_ctx * dev_ctx = NULL;
	    int bar_idx = 0;
	    dev = rb_entry(node, struct pci_device, dev_tree_node);

	    snprintf(buf, 128, "pci-%d.%d-%d", i, dev->dev_num, dev->fn_num);
	    dev_ctx = v3_chkpt_open_ctx(bus_ctx->chkpt, bus_ctx, buf);
	    
	    v3_chkpt_save(dev_ctx, "CONFIG_SPACE", 256, dev->config_space);

	    for (bar_idx = 0; bar_idx < 6; bar_idx++) {
		snprintf(buf, 128, "BAR-%d", bar_idx);
		v3_chkpt_save_32(dev_ctx, buf, &(dev->bar[bar_idx].val));
	    }

	    node = v3_rb_next(node);
	}
    }


    return 0;
}


static int pci_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct pci_internal * pci = (struct pci_internal *)private_data;
    char buf[128];
    int i = 0;    
    
    v3_chkpt_load_32(ctx, "ADDR_REG", &(pci->addr_reg.val));
    v3_chkpt_load_16(ctx, "IO_BASE", &(pci->dev_io_base));

    for (i = 0; i < PCI_BUS_COUNT; i++) {
	struct pci_bus * bus = &(pci->bus_list[i]);
	struct rb_node * node = v3_rb_first(&(bus->devices));
	struct pci_device * dev = NULL;
	struct v3_chkpt_ctx * bus_ctx = NULL;

	snprintf(buf, 128, "pci-%d\n", i);
	
	bus_ctx = v3_chkpt_open_ctx(ctx->chkpt, ctx, buf);

	while (node) {
	    struct v3_chkpt_ctx * dev_ctx = NULL;
	    int bar_idx = 0;
	    dev = rb_entry(node, struct pci_device, dev_tree_node);

	    snprintf(buf, 128, "pci-%d.%d-%d", i, dev->dev_num, dev->fn_num);
	    dev_ctx = v3_chkpt_open_ctx(bus_ctx->chkpt, bus_ctx, buf);
	    
	    v3_chkpt_load(dev_ctx, "CONFIG_SPACE", 256, dev->config_space);

	    for (bar_idx = 0; bar_idx < 6; bar_idx++) {
		snprintf(buf, 128, "BAR-%d", bar_idx);
		v3_chkpt_load_32(dev_ctx, buf, &(dev->bar[bar_idx].val));
	    }

	    node = v3_rb_next(node);
	}
    }


    return 0;
}


#endif




static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))pci_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = pci_save,
    .load = pci_load
#endif
};




static int pci_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct pci_internal * pci_state = V3_Malloc(sizeof(struct pci_internal));
    int i = 0;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;
    
    PrintDebug("PCI internal at %p\n",(void *)pci_state);
    
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, pci_state);
    
    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(pci_state);
	return -1;
    }

    
    pci_state->addr_reg.val = 0; 
    pci_state->dev_io_base = PCI_DEV_IO_PORT_BASE;

    init_pci_busses(pci_state);
    
    PrintDebug("Sizeof config header=%d\n", (int)sizeof(struct pci_config_header));
    
    for (i = 0; i < 4; i++) {
	ret |= v3_dev_hook_io(dev, CONFIG_ADDR_PORT + i, &addr_port_read, &addr_port_write);
	ret |= v3_dev_hook_io(dev, CONFIG_DATA_PORT + i, &data_port_read, &data_port_write);
    }
    
    if (ret != 0) {
	PrintError("Error hooking PCI IO ports\n");
	v3_remove_device(dev);
	return -1;
    }

    return 0;
}


device_register("PCI", pci_init)


static inline int init_bars(struct v3_vm_info * vm, struct pci_device * pci_dev) {
    int i = 0;

    for (i = 0; i < 6; i++) {
	int bar_offset = 0x10 + (4 * i);
	struct v3_pci_bar * bar = &(pci_dev->bar[i]);

	if (bar->type == PCI_BAR_IO) {
	    int j = 0;
	    bar->mask = (~((bar->num_ports) - 1)) | 0x01;

	    if (bar->default_base_port != 0xffff) {
		bar->val = bar->default_base_port & bar->mask;
	    } else {
		bar->val = 0;
	    }

	    bar->val |= 0x00000001;

	    for (j = 0; j < bar->num_ports; j++) {
		// hook IO
		if (bar->default_base_port != 0xffff) {
		    if (v3_hook_io_port(vm, bar->default_base_port + j,
					bar->io_read, bar->io_write, 
					bar->private_data) == -1) {
			PrintError("Could not hook default io port %x\n", bar->default_base_port + j);
			return -1;
		    }
		}
	    }

	    *(uint32_t *)(pci_dev->config_space + bar_offset) = bar->val;

	} else if (bar->type == PCI_BAR_MEM32) {
	    bar->mask = ~((bar->num_pages << 12) - 1);
	    bar->mask |= 0xf; // preserve the configuration flags

	    if (bar->default_base_addr != 0xffffffff) {
		bar->val = bar->default_base_addr & bar->mask;
	    } else {
		bar->val = 0;
	    }

	    // hook memory
	    if (bar->mem_read) {
		// full hook
		v3_hook_full_mem(vm, V3_MEM_CORE_ANY, bar->default_base_addr,
				 bar->default_base_addr + (bar->num_pages * PAGE_SIZE_4KB),
				 bar->mem_read, bar->mem_write, pci_dev->priv_data);
	    } else if (bar->mem_write) {
		// write hook
		PrintError("Write hooks not supported for PCI devices\n");
		return -1;
		/*
		  v3_hook_write_mem(pci_dev->vm_dev->vm, bar->default_base_addr, 
		  bar->default_base_addr + (bar->num_pages * PAGE_SIZE_4KB),
		  bar->mem_write, pci_dev->vm_dev);
		*/
 	    } else {
		// set the prefetchable flag...
		bar->val |= 0x00000008;
	    }


	    *(uint32_t *)(pci_dev->config_space + bar_offset) = bar->val;

	} else if (bar->type == PCI_BAR_MEM24) {
	    PrintError("16 Bit memory ranges not supported (reg: %d)\n", i);
	    return -1;
	} else if (bar->type == PCI_BAR_NONE) {
	    bar->val = 0x00000000;
	    bar->mask = 0x00000000; // This ensures that all updates will be dropped
	    *(uint32_t *)(pci_dev->config_space + bar_offset) = bar->val;
	} else if (bar->type == PCI_BAR_PASSTHROUGH) {

	    // Call the bar init function to get the local cached value
	    bar->bar_init(i, &(bar->val), bar->private_data);

	} else {
	    PrintError("Invalid BAR type for bar #%d\n", i);
	    return -1;
	}

	v3_pci_hook_config_range(pci_dev, bar_offset, 4, bar_update, NULL, bar);
    }

    return 0;
}


int v3_pci_set_irq_bridge(struct  vm_device * pci_bus, int bus_num, 
			  int (*raise_pci_irq)(struct pci_device * pci_dev, void * dev_data, struct v3_irq * vec),
			  int (*lower_pci_irq)(struct pci_device * pci_dev, void * dev_data, struct v3_irq * vec),
			  void * priv_data) {
    struct pci_internal * pci_state = (struct pci_internal *)pci_bus->private_data;


    pci_state->bus_list[bus_num].raise_pci_irq = raise_pci_irq;
    pci_state->bus_list[bus_num].lower_pci_irq = lower_pci_irq;
    pci_state->bus_list[bus_num].irq_dev_data = priv_data;

    return 0;
}

int v3_pci_raise_irq(struct vm_device * pci_bus, struct pci_device * dev, uint32_t vec_index) {
   struct v3_irq vec;

   vec.ack = NULL;
   vec.private_data = NULL;
   vec.irq = vec_index;

   return v3_pci_raise_acked_irq(pci_bus, dev, vec);
}

int v3_pci_lower_irq(struct vm_device * pci_bus, struct pci_device * dev, uint32_t vec_index) {
    struct v3_irq vec;

    vec.irq = vec_index;
    vec.ack = NULL;
    vec.private_data = NULL;
    
    return v3_pci_lower_acked_irq(pci_bus, dev, vec);
}

int v3_pci_raise_acked_irq(struct vm_device * pci_bus, struct pci_device * dev, struct v3_irq vec) {
   struct pci_internal * pci_state = (struct pci_internal *)pci_bus->private_data;
   struct pci_bus * bus = &(pci_state->bus_list[dev->bus_num]);


   if (dev->irq_type == IRQ_INTX) {
       return bus->raise_pci_irq(dev, bus->irq_dev_data, &vec);
   } else if (dev->irq_type == IRQ_MSI) {
       struct v3_gen_ipi ipi;
       struct msi_addr * addr = NULL;
       struct msi_data * data = NULL;       
       
       if (dev->msi_cap->cap_64bit) {
	   if (dev->msi_cap->per_vect_mask) {
	       struct msi64_pervec_msg_addr * msi = (void *)dev->msi_cap;
	       addr = &(msi->addr);
	       data = &(msi->data);
	   } else {
	       struct msi64_msg_addr * msi = (void *)dev->msi_cap;
	       addr = &(msi->addr);
	       data = &(msi->data);
	   }
       } else {
	   struct msi32_msg_addr * msi = (void *)dev->msi_cap;
	   addr = &(msi->addr);
	   data = &(msi->data);
       }

       memset(&ipi, 0, sizeof(struct v3_gen_ipi));

       ipi.vector = data->vector + vec.irq;
       ipi.mode = data->del_mode;
       ipi.logical = addr->dst_mode;
       ipi.trigger_mode = data->trig_mode;
       ipi.dst_shorthand = 0;
       ipi.dst = addr->dst_id;
       
       // decode MSI fields into IPI

       V3_Print("Decode MSI\n");

       v3_apic_send_ipi(dev->vm, &ipi, dev->apic_dev);

       return 0;       
   } else if (dev->irq_type == IRQ_MSIX) {
       addr_t msix_table_gpa = 0;
       struct msix_table * msix_table = NULL;
       uint_t bar_idx = dev->msix_cap->bir;
       struct v3_gen_ipi ipi;
       struct msi_addr * addr = NULL;
       struct msi_data * data = NULL;   
       
       if (dev->bar[bar_idx].type != PCI_BAR_MEM32) {
	   PrintError("Non 32bit MSIX BAR registers are not supported\n");
	   return -1;
       }

       msix_table_gpa = dev->bar[bar_idx].val;
       msix_table_gpa += dev->msix_cap->table_offset;

       if (v3_gpa_to_hva(&(dev->vm->cores[0]), msix_table_gpa, (void *)&(msix_table)) != 0) {
	   PrintError("Could not translate MSIX Table GPA (%p)\n", (void *)msix_table_gpa);
	   return -1;
       }
       
       memset(&ipi, 0, sizeof(struct v3_gen_ipi));

       data = &(msix_table->entries[vec.irq].data);
       addr = &(msix_table->entries[vec.irq].addr);;
       
       ipi.vector = data->vector + vec.irq;
       ipi.mode = data->del_mode;
       ipi.logical = addr->dst_mode;
       ipi.trigger_mode = data->trig_mode;
       ipi.dst_shorthand = 0;
       ipi.dst = addr->dst_id;
       
       // decode MSIX fields into IPI

       V3_Print("Decode MSIX\n");

       v3_apic_send_ipi(dev->vm, &ipi, dev->apic_dev);

       return 0;
   } 
   
   // Should never get here
   return -1;

}

int v3_pci_lower_acked_irq(struct vm_device * pci_bus, struct pci_device * dev, struct v3_irq vec) {
    if (dev->irq_type == IRQ_INTX) {
	struct pci_internal * pci_state = (struct pci_internal *)pci_bus->private_data;
	struct pci_bus * bus = &(pci_state->bus_list[dev->bus_num]);
	
	return bus->lower_pci_irq(dev, bus->irq_dev_data, &vec);
    } else {
	return -1;
    }
}


// if dev_num == -1, auto assign 
struct pci_device * v3_pci_register_device(struct vm_device * pci,
					   pci_device_type_t dev_type, 
					   int bus_num,
					   int dev_num,
					   int fn_num,
					   const char * name,
					   struct v3_pci_bar * bars,
					   int (*config_write)(struct pci_device * pci_dev, uint32_t reg_num, void * src, 
							       uint_t length, void * priv_data),
					   int (*config_read)(struct pci_device * pci_dev, uint32_t reg_num, void * dst, 
							      uint_t length, void * priv_data),
					   int (*cmd_update)(struct pci_device * pci_dev, pci_cmd_t cmd, uint64_t arg, void * priv_data),
					   int (*exp_rom_update)(struct pci_device * pci_dev, uint32_t * src, void * priv_data),
					   void * priv_data) {

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


    pci_dev->type = dev_type;
    
    switch (pci_dev->type) {
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
    pci_dev->vm = pci->vm;
    pci_dev->priv_data = priv_data;

    INIT_LIST_HEAD(&(pci_dev->cfg_hooks));
    INIT_LIST_HEAD(&(pci_dev->capabilities));

    
    {
	// locate APIC for MSI/MSI-X
	pci_dev->apic_dev = v3_find_dev(pci->vm, "apic");
    }

    // register update callbacks
    pci_dev->config_write = config_write;
    pci_dev->config_read = config_read;
    pci_dev->cmd_update = cmd_update;
    pci_dev->exp_rom_update = exp_rom_update;



    if (config_read) {
	int i = 0;

	// Only 256 bytes for now, should expand it in the future
	for (i = 0; i < 256; i++) {
	    config_read(pci_dev, i, &(pci_dev->config_space[i]), 1, pci_dev->priv_data);
	}
    }

    V3_Print("Scanning for Capabilities\n");

    // scan for caps
    scan_pci_caps(pci_dev);

    pci_dev->irq_type = IRQ_INTX;

    V3_Print("Caps scanned\n");

    // hook important regions
    v3_pci_hook_config_range(pci_dev, 0x30, 4, exp_rom_write, NULL, NULL);  // ExpRom
    v3_pci_hook_config_range(pci_dev, 0x04, 2, cmd_write, NULL, NULL);      // CMD Reg
    // * Status resets
    // * Drop BIST
    // 

    

    //copy bars
    for (i = 0; i < 6; i ++) {
	pci_dev->bar[i].type = bars[i].type;
	pci_dev->bar[i].private_data = bars[i].private_data;

	if (pci_dev->bar[i].type == PCI_BAR_IO) {
	    pci_dev->bar[i].num_ports = bars[i].num_ports;

	    // This is a horrible HACK becaues the BIOS is supposed to set the PCI base ports 
	    // And if the BIOS doesn't, Linux just happily overlaps device port assignments
	    if (bars[i].default_base_port != (uint16_t)-1) {
		pci_dev->bar[i].default_base_port = bars[i].default_base_port;
	    } else {
		pci_dev->bar[i].default_base_port = pci_state->dev_io_base;
		pci_state->dev_io_base += ( 0x100 * ((bars[i].num_ports / 0x100) + 1) );
	    }

	    pci_dev->bar[i].io_read = bars[i].io_read;
	    pci_dev->bar[i].io_write = bars[i].io_write;
	} else if (pci_dev->bar[i].type == PCI_BAR_MEM32) {
	    pci_dev->bar[i].num_pages = bars[i].num_pages;
	    pci_dev->bar[i].default_base_addr = bars[i].default_base_addr;
	    pci_dev->bar[i].mem_read = bars[i].mem_read;
	    pci_dev->bar[i].mem_write = bars[i].mem_write;
	} else if (pci_dev->bar[i].type == PCI_BAR_PASSTHROUGH) {
	    pci_dev->bar[i].bar_init = bars[i].bar_init;
	    pci_dev->bar[i].bar_write = bars[i].bar_write;
	} else {
	    pci_dev->bar[i].num_pages = 0;
	    pci_dev->bar[i].default_base_addr = 0;
	    pci_dev->bar[i].mem_read = NULL;
	    pci_dev->bar[i].mem_write = NULL;
	}
    }

    if (init_bars(pci->vm, pci_dev) == -1) {
	PrintError("could not initialize bar registers\n");
	return NULL;
    }

    // add the device
    add_device_to_bus(bus, pci_dev);

#ifdef V3_CONFIG_DEBUG_PCI
    pci_dump_state(pci_state);
#endif

    return pci_dev;
}

