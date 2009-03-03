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

#ifndef _VPCI_H__
#define _VPCI_H__

#include <palacios/vm_dev.h>
#include <palacios/vmm_types.h>

#define PROG_INTERFACE(x) ((x)[0])
#define SUBCLASS(x) ((x)[1])
#define CLASSCODE(x) ((x)[2])
  
#define HEADER_TYPE(x) ((x)&0x7f)
  
#define PCI_DEVICE 0x0

#define IS_DEVICE(x) (HEADER_TYPE(x)==0x0)
      
#define IS_IO_ADDR(x)   ((x)&0x1)
#define IS_MEM_ADDR(x)  (!((x)&0x1))
#define GET_IO_ADDR(x)  (((uint_t)(x))&0xfffffffc) 
#define GET_MEM_ADDR(x) (((uint_t)(x))&0xfffffff0)
#define GET_MEM_TYPE(x) (((x)&0x6)>>2)

#define PCI_CONFIG_ADDRESS 0xcf8  // 32 bit, little endian
#define PCI_CONFIG_DATA    0xcfc  // 32 bit, little endian

#define PCI_IO_REGIONS 6

struct pci_device_config {
  uint16_t   vendor_id;
  uint16_t   device_id;
  uint16_t   command;
  uint16_t   status;
  uchar_t    revision;
  uchar_t    class_code[3];  // in order: programming interface, subclass, class code
  uchar_t    cache_line_size;
  uchar_t    latency_time;
  uchar_t    header_type; // bits 6-0: 00: other, 01: pci-pci bridge, 02: pci-cardbus; bit 7: 1=multifunction
  uchar_t    BIST;  
  uint32_t   BAR[6];
  uint32_t   cardbus_cis_pointer;
  uint16_t   subsystem_vendor_id;
  uint16_t   subsystem_id;
  uint32_t   expansion_rom_address;
  uchar_t    cap_ptr;  // capabilities list offset in config space
  uchar_t    reserved[7];
  uchar_t    intr_line; // 00=none, 01=IRQ1, etc.
  uchar_t    intr_pin;  // 00=none, otherwise INTA# to INTD#
  uchar_t    min_grant; // min busmaster time - units of 250ns
  uchar_t    max_latency; // units of 250ns - busmasters
  uint32_t   device_data[48]; 
};

struct pci_device;

 typedef void pci_mapioregion_fn(struct pci_device *pci_dev, int region_num,
                                uint32_t addr, uint32_t size, int type); 

typedef int port_read_fn(ushort_t port, void * dst, uint_t length, struct vm_device *vmdev); 
typedef int port_write_fn(ushort_t port, void * src, uint_t length, struct vm_device *vmdev);

#define PCI_ADDRESS_SPACE_MEM		0x00
#define PCI_ADDRESS_SPACE_IO		0x01
#define PCI_ADDRESS_SPACE_MEM_PREFETCH	0x08

struct pci_ioregion {
    uint32_t addr; //current PCI mapping address. -1 means not mapped 
    uint32_t size;  //actual ports/memories needed by device
    uint32_t mapped_size;  //mapped size, usually bigger than needed size, -1 not mapped
    uint8_t type;
    uchar_t reg_num;  //correponding to which BAR register it is
    pci_mapioregion_fn *map_func;

    port_read_fn **port_reads;   //array of read functions, hooked for each port in order, if NULL, do not hook that port
    port_write_fn **port_writes; 
};


struct pci_device {
    struct pci_device_config config; 
    struct pci_bus *bus;
    struct pci_device *next;

    int dev_num;
    char name[64];
    int irqline;

    struct pci_ops {
	 void (*raise_irq)(struct pci_device *dev, void *data);
	 void (*config_write)(struct pci_device *pci_dev, uchar_t addr, uint32_t val, int len);
    	 uint32_t (*config_read)(struct pci_device *pci_dev, uchar_t addr, int len);
    }ops;    

    struct pci_ioregion *ioregion[PCI_IO_REGIONS];
};


/*
struct pci_class_desc {
    uint16_t class;
    const char *desc;
};

static struct pci_class_desc pci_class_descriptions[] =
{
    { 0x0100, "SCSI controller"},
    { 0x0101, "IDE controller"},
    { 0x0102, "Floppy controller"},
    { 0x0103, "IPI controller"},
    { 0x0104, "RAID controller"},
    { 0x0106, "SATA controller"},
    { 0x0107, "SAS controller"},
    { 0x0180, "Storage controller"},
    { 0x0200, "Ethernet controller"},
    { 0x0201, "Token Ring controller"},
    { 0x0202, "FDDI controller"},
    { 0x0203, "ATM controller"},
    { 0x0280, "Network controller"},
    { 0x0300, "VGA controller"},
    { 0x0301, "XGA controller"},
    { 0x0302, "3D controller"},
    { 0x0380, "Display controller"},
    { 0x0400, "Video controller"},
    { 0x0401, "Audio controller"},
    { 0x0402, "Phone"},
    { 0x0480, "Multimedia controller"},
    { 0x0500, "RAM controller"},
    { 0x0501, "Flash controller"},
    { 0x0580, "Memory controller"},
    { 0x0600, "Host bridge"},
    { 0x0601, "ISA bridge"},
    { 0x0602, "EISA bridge"},
    { 0x0603, "MC bridge"},
    { 0x0604, "PCI bridge"},
    { 0x0605, "PCMCIA bridge"},
    { 0x0606, "NUBUS bridge"},
    { 0x0607, "CARDBUS bridge"},
    { 0x0608, "RACEWAY bridge"},
    { 0x0680, "Bridge"},
    { 0x0c03, "USB controller"},
    { 0, NULL}
};

*/
struct vm_device *v3_create_vpci();

#endif

