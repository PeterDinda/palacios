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
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Lei Xia <lxia@northwestern.edu>
 *             Chang Seok Bae <jhuell@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __DEVICES_PCI_H__
#define __DEVICES_PCI_H__

#ifdef __V3VEE__


#include <palacios/vmm_types.h>
#include <palacios/vmm_rbtree.h>
#include <palacios/vmm_intr.h>

#include <devices/pci_types.h>

struct vm_device;


typedef enum { PCI_CMD_DMA_DISABLE  = 1,
	       PCI_CMD_DMA_ENABLE   = 2,
	       PCI_CMD_INTX_DISABLE = 3, 
	       PCI_CMD_INTX_ENABLE  = 4,
	       PCI_CMD_MSI_DISABLE  = 5,
	       PCI_CMD_MSI_ENABLE   = 6,
	       PCI_CMD_MSIX_DISABLE = 7,
	       PCI_CMD_MSIX_ENABLE  = 8 } pci_cmd_t;

typedef enum { PCI_BAR_IO, 
	       PCI_BAR_MEM24, 
	       PCI_BAR_MEM32, 
	       PCI_BAR_MEM64_LO, 
	       PCI_BAR_MEM64_HI, 
	       PCI_BAR_PASSTHROUGH,
	       PCI_BAR_NONE } pci_bar_type_t;

typedef enum {PCI_STD_DEVICE, PCI_TO_PCI_BRIDGE, PCI_CARDBUS, PCI_MULTIFUNCTION, PCI_PASSTHROUGH} pci_device_type_t;



// For the rest of the subclass codes see:
// http://www.acm.uiuc.edu/sigops/roll_your_own/7.c.1.html

#define PCI_AUTO_DEV_NUM (-1)

struct guest_info;

struct pci_device;

struct v3_pci_bar {
    pci_bar_type_t type;
    
    union {
	struct {
	    int num_pages;
	    addr_t default_base_addr;
	    int (*mem_read)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * private_data);
	    int (*mem_write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * private_data);
	};

	struct {
	    int num_ports;
	    uint16_t default_base_port;
	    int (*io_read)(struct guest_info * core, uint16_t port, void * src, uint_t length, void * private_data);
	    int (*io_write)(struct guest_info * core, uint16_t port, void * src, uint_t length, void * private_data);
	};
	
	struct {
	    int (*bar_init)(int bar_num, uint32_t * dst, void * private_data);
	    int (*bar_write)(int bar_num, uint32_t * src, void * private_data);
	};
    };
    
    void * private_data;

    // Internal PCI data
    uint32_t val;
    uint8_t updated;
    uint32_t mask;
};


#define PCI_IO_MASK 0xfffffffc
#define PCI_MEM24_MASK 0x000ffff0
#define PCI_MEM_MASK 0xfffffff0
#define PCI_MEM64_MASK_HI 0xffffffff
#define PCI_MEM64_MASK_LO 0xfffffff0
#define PCI_EXP_ROM_MASK 0xfffff800



#define PCI_IO_BASE(bar_val) (bar_val & PCI_IO_MASK)
#define PCI_MEM24_BASE(bar_val) (bar_val & PCI_MEM24_MASK)
#define PCI_MEM32_BASE(bar_val) (bar_val & PCI_MEM_MASK)
#define PCI_MEM64_BASE_HI(bar_val) (bar_val & PCI_MEM64_MASK_HI)
#define PCI_MEM64_BASE_LO(bar_val) (bar_val & PCI_MEM64_MASK_LO)
#define PCI_EXP_ROM_BASE(rom_val) (rom_val & PCI_EXP_ROM_MASK)

#define PCI_IO_BAR_VAL(addr) ((addr & PCI_IO_MASK) | 0x1)
#define PCI_MEM24_BAR_VAL(addr, prefetch) (((addr & PCI_MEM24_MASK) | 0x2) | ((prefetch) != 0) << 3)
#define PCI_MEM32_BAR_VAL(addr, prefetch) (((addr & PCI_MEM_MASK) | ((prefetch) != 0) << 3))
#define PCI_MEM64_HI_BAR_VAL(addr, prefetch) (addr & PCI_MEM64_MASK_HI)
#define PCI_MEM64_LO_BAR_VAL(addr, prefetch) ((((addr) & PCI_MEM64_MASK_LO) | 0x4) | ((prefetch) != 0) << 3)
#define PCI_EXP_ROM_VAL(addr, enable) (((addr) & PCI_EXP_ROM_MASK) | ((enable) != 0))


struct pci_device {

    pci_device_type_t type;

    union {
	uint8_t config_space[256];

	struct {
	    struct pci_config_header config_header;
	    uint8_t config_data[192];
	} __attribute__((packed));
    } __attribute__((packed));

    struct v3_pci_bar bar[6];

    struct rb_node dev_tree_node;

    uint_t bus_num;

    union {
	uint8_t devfn;
	struct {
	    uint8_t fn_num       : 3;
	    uint8_t dev_num      : 5;
	} __attribute__((packed));
    } __attribute__((packed));

    char name[64];

    int (*config_write)(struct pci_device * pci_dev, uint32_t reg_num, void * src, 
			uint_t length, void * priv_data);
    int (*config_read)(struct pci_device * pci_dev, uint32_t reg_num, void * dst, 
		       uint_t length, void * priv_data);
    int (*cmd_update)(struct pci_device * pci_dev, pci_cmd_t cmd, uint64_t arg, void * priv_data);
    int (*exp_rom_update)(struct pci_device * pci_dev, uint32_t * src, void * private_data);

    struct v3_vm_info * vm;

    struct list_head cfg_hooks;
    struct list_head capabilities; 

    struct msi_msg_ctrl * msi_cap;
    struct msix_cap * msix_cap;
    struct vm_device * apic_dev;

    enum {IRQ_NONE, IRQ_INTX, IRQ_MSI, IRQ_MSIX} irq_type;

    void * priv_data;
};


int v3_pci_set_irq_bridge(struct vm_device * pci_bus, int bus_num,
			  int (*raise_pci_irq)(struct pci_device * pci_dev, void * dev_data, struct v3_irq * vec), 
			  int (*lower_pci_irq)(struct pci_device * pci_dev, void * dev_data, struct v3_irq * vec), 
			  void * dev_data);


/* Raising a PCI IRQ requires the specification of a vector index. 
 *  If you are not sure, set vec_index to 0. 
 * For IntX IRQs, the index is the interrupt line the device is using (INTA=0, INTB=1, ...) - only used in multi-function devices
 * For MSI and MSIX, the index is the vector index if multi-vectors are enabled  
 */

int v3_pci_raise_irq(struct vm_device * pci_bus, struct pci_device * dev, uint32_t vec_index);
int v3_pci_lower_irq(struct vm_device * pci_bus, struct pci_device * dev, uint32_t vec_index);

int v3_pci_raise_acked_irq(struct vm_device * pci_bus, struct pci_device * dev, struct v3_irq vec);
int v3_pci_lower_acked_irq(struct vm_device * pci_bus, struct pci_device * dev, struct v3_irq vec);

struct pci_device * 
v3_pci_register_device(struct vm_device * pci,
		       pci_device_type_t dev_type, 
		       int bus_num,
		       int dev_num,
		       int fn_num,
		       const char * name,
		       struct v3_pci_bar * bars,
		       int (*config_write)(struct pci_device * pci_dev, uint32_t reg_num, void * src, 
					   uint_t length, void * private_data),
		       int (*config_read)(struct pci_device * pci_dev, uint32_t reg_num, void * dst, 
					  uint_t length, void * private_data),
		       int (*cmd_update)(struct pci_device *pci_dev, pci_cmd_t cmd, uint64_t arg, void * priv_data),
		       int (*exp_rom_update)(struct pci_device * pci_dev, uint32_t * src, void * private_data),
		       void * priv_data);



int v3_pci_hook_config_range(struct pci_device * pci, 
			     uint32_t start, uint32_t length, 
			     int (*write)(struct pci_device * pci_dev, uint32_t offset, 
						 void * src, uint_t length, void * private_data), 
			     int (*read)(struct pci_device * pci_dev, uint32_t offset, 
						 void * src, uint_t length, void * private_data), 
			     void * private_data);




typedef enum { PCI_CAP_INVALID = 0, 
	       PCI_CAP_PM = 0x1,
 	       PCI_CAP_MSI = 0x5,
	       PCI_CAP_MSIX = 0x11,
               PCI_CAP_PCIE = 0x10 } pci_cap_type_t;

int v3_pci_enable_capability(struct pci_device * pci, pci_cap_type_t cap_type);


#endif

#endif

