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


#include <palacios/vmm.h>
#include <palacios/vmm_types.h>


struct v3_vm_info;

typedef enum { PT_BAR_NONE,
	       PT_BAR_IO, 
	       PT_BAR_MEM32, 
	       PT_BAR_MEM24, 
	       PT_BAR_MEM64_LO, 
	       PT_BAR_MEM64_HI,
	       PT_EXP_ROM } pt_bar_type_t;


typedef enum { HOST_PCI_CMD_DMA_DISABLE = 1,
	       HOST_PCI_CMD_DMA_ENABLE = 2,
	       HOST_PCI_CMD_INTX_DISABLE = 3,
	       HOST_PCI_CMD_INTX_ENABLE = 4,
	       HOST_PCI_CMD_MSI_DISABLE = 5,
	       HOST_PCI_CMD_MSI_ENABLE = 6,
	       HOST_PCI_CMD_MSIX_DISABLE = 7,
	       HOST_PCI_CMD_MSIX_ENABLE = 8 } host_pci_cmd_t;

struct v3_host_pci_bar {
    uint32_t size;
    pt_bar_type_t type;

    /*  We store 64 bit memory bar addresses in the high BAR
     *  because they are the last to be updated
     *  This means that the addr field must be 64 bits
     */
    uint64_t addr; 

    union {
	uint32_t flags;
	struct {
	    uint32_t prefetchable    : 1;
	    uint32_t cacheable       : 1;
	    uint32_t exp_rom_enabled : 1;
	    uint32_t rsvd            : 29;
	} __attribute__((packed));
    } __attribute__((packed));


};



struct v3_host_pci_dev {
    struct v3_host_pci_bar bars[6];
    struct v3_host_pci_bar exp_rom;

    uint8_t cfg_space[256];

    enum {IOMMU, SYMBIOTIC, EMULATED} iface;

    int (*irq_handler)(void * guest_data, uint32_t vec_index);

    void * host_data;
    void * guest_data;
};

// For now we just support the single contiguous region
// This can be updated in the future to support non-contiguous guests
struct v3_guest_mem_region {
    uint64_t start;
    uint64_t end;
};


#ifdef __V3VEE__

#include <devices/pci.h>


struct v3_host_pci_dev * v3_host_pci_get_dev(struct v3_vm_info * vm, char * url, void * priv_data);


int v3_host_pci_config_write(struct v3_host_pci_dev * v3_dev, uint32_t reg_num, void * src, uint32_t length);
int v3_host_pci_config_read(struct v3_host_pci_dev * v3_dev, uint32_t reg_num, void * dst, uint32_t length);

int v3_host_pci_cmd_update(struct v3_host_pci_dev * v3_dev, pci_cmd_t cmd, uint64_t arg);

int v3_host_pci_ack_irq(struct v3_host_pci_dev * v3_dev, uint32_t vector);


#endif


struct v3_host_pci_hooks {
    struct v3_host_pci_dev * (*request_device)(char * url, void * v3_ctx);

    // emulated interface

    int (*config_write)(struct v3_host_pci_dev * v3_dev, uint32_t reg_num, void * src, uint32_t length);
    int (*config_read)(struct v3_host_pci_dev * v3_dev, uint32_t reg_num, void * dst, uint32_t length);

    int (*pci_cmd)(struct v3_host_pci_dev * v3_dev, host_pci_cmd_t cmd, uint64_t arg);
    
    int (*ack_irq)(struct v3_host_pci_dev * v3_dev, uint32_t vector);


};



void V3_Init_Host_PCI(struct v3_host_pci_hooks * hooks);

int V3_get_guest_mem_region(struct v3_vm_info * vm, struct v3_guest_mem_region * region, uint64_t gpa);
int V3_host_pci_raise_irq(struct v3_host_pci_dev * v3_dev, uint32_t vec_index);

