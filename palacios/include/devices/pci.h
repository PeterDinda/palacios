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

#include <palacios/vm_dev.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_rbtree.h>

#include <devices/pci_types.h>


typedef enum {PCI_BAR_IO, PCI_BAR_MEM32, PCI_BAR_MEM64_LOW, PCI_BAR_MEM64_HIGH, PCI_BAR_NONE} pci_bar_type_t;

struct bar_reg {
    int updated;
    pci_bar_type_t type;
    int num_resources;
    int (*bar_update)(struct pci_device * pci_dev, uint_t bar);
};

struct pci_device {
    union {
	uint8_t config_space[256];

	struct {
	    struct pci_config_header config_header;
	    uint8_t config_data[192];
	} __attribute__((packed));
    } __attribute__((packed));



    struct bar_reg bar[6];

    uint_t bus_num;
    struct rb_node dev_tree_node;

    int dev_num;
    char name[64];

    struct vm_device * vm_dev;  //the corresponding virtual device

    int (*config_update)(struct pci_device * pci_dev, uint_t reg_num, int length);


    void * priv_data;
};



struct vm_device * v3_create_pci();

struct pci_device * 
v3_pci_register_device(struct vm_device * pci,
		       uint_t bus_num,
		       const char * name,
		       int dev_num,
		       int (*config_update)(struct pci_device * pci_dev, uint_t reg_num, int length),
		       int (*bar_update)(struct pci_device * pci_dev, uint_t bar),
		       void * private_data);


#endif

#endif

