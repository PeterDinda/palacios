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

#include <palacios/vm_dev.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_rbtree.h>

#include <devices/pci_types.h>


#define V3_PCI_BAR_MEM		0x00
#define V3_PCI_BAR_IO		0x01
#define V3_PCI_BAR_MEM_PREFETCH	0x08


struct pci_device {
    union {
	struct pci_config_header header;
	uint8_t header_space[64];
    } __attribute__((packed));

    uint8_t config_space[192];

    uint_t bus_num;
    struct rb_node dev_tree_node;

    int dev_num;
    char name[64];

    struct vm_device * vm_dev;  //the corresponding virtual device

    int (*config_read)(struct pci_device * pci_dev, uint_t reg_num, void * dst, int len);
    int (*config_write)(struct pci_device * pci_dev, uint_t reg_num, void * src, int len);
    int (*bar_update)(struct pci_device * pci_dev, uint_t bar_reg, uint32_t val);

    void * priv_data;
};



struct vm_device * v3_create_pci();

struct pci_bus * v3_get_pcibus(struct guest_info *vm, int bus_no);

struct pci_device * 
v3_pci_register_device(struct vm_device * dev,
		       uint_t bus_num,
		       const char * name,
		       int dev_num,
		       int (*config_read)(struct pci_device * pci_dev, uint_t reg_num, void * dst, int len),
		       int (*config_write)(struct pci_device * pci_dev, uint_t reg_num, void * src, int len),
		       int (*bar_update)(struct pci_device * pci_dev, uint_t bar_reg, uint32_t val),
		       void * private_data);


#endif

