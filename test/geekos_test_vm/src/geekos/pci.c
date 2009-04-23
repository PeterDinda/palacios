/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <geekos/malloc.h>
#include <geekos/pci.h>
#include <geekos/io.h>
#include <geekos/debug.h>

#define PCI_CONFIG_ADDRESS 0xcf8  // 32 bit, little endian
#define PCI_CONFIG_DATA    0xcfc  // 32 bit, little endian

#define PCI_MAX_NUM_BUSES  4


struct pci_device_config {
    ushort_t   vendor_id;
    ushort_t   device_id;
    ushort_t   command;
    ushort_t   status;
    uchar_t    revision;
    uchar_t    class_code[3];  // in order: programming interface, subclass, class code

#define PROG_INTERFACE(x) ((x)[0])
#define SUBCLASS(x) ((x)[1])
#define CLASSCODE(x) ((x)[2])
  
  
    uchar_t    cache_line_size;
    uchar_t    latency_time;
    uchar_t    header_type; // bits 6-0: 00: other, 01: pci-pci bridge, 02: pci-cardbus; bit 7: 1=multifunction
    
#define HEADER_TYPE(x) ((x) & 0x7f)
    
#define PCI_DEVICE 0x0
#define PCI_PCI_BRIDGE 0x1
#define PCI_CARDBUS_BRIDGE 0x2

#define MULTIFUNCTION(x) ((x) & 0x80)

#define IS_DEVICE(x) (HEADER_TYPE(x) == 0x0)
#define IS_PCI_PCI_BRIDGE(x) (HEADER_TYPE(x) == 0x1)
#define IS_PCI_CARDBUS_BRIDGE(x) (HEADER_TYPE(x) == 0x2)

    uchar_t    BIST;
  
    union {  
    
	// header = 00 (Device)
	struct {
	    uint_t     BAR[6];
	    
#define IS_IO_ADDR(x)   ((x) & 0x1)
#define IS_MEM_ADDR(x)  (!((x) & 0x1))
#define GET_IO_ADDR(x)  (((uint_t)(x)) & 0xfffffffc) 
#define GET_MEM_ADDR(x) (((uint_t)(x)) & 0xfffffff0)
#define GET_MEM_TYPE(x) (((x) & 0x6) >> 2)
#define GET_MEM_PREFETCHABLE(x) ((x) & 0x8)

	    uint_t     cardbus_cis_pointer;
	    ushort_t   subsystem_vendor_id;
	    ushort_t   subsystem_id;
	    uint_t     expansion_rom_address;
	    uchar_t    cap_ptr;  // capabilities list offset in config space
	    uchar_t    reserved[7];
	    uchar_t    intr_line; // 00=none, 01=IRQ1, etc.
	    uchar_t    intr_pin;  // 00=none, otherwise INTA# to INTD#
	    uchar_t    min_grant; // min busmaster time - units of 250ns
	    uchar_t    max_latency; // units of 250ns - busmasters
	    uint_t     device_data[48];
	}  __attribute__((__packed__))  pci_device_config;
	
	// header = 01 (pci-pci bridge)
	struct {
	    uint_t     BAR[2];
	    uchar_t    primary_bus;  // the one closer to the processor
	    uchar_t    secondary_bus; // the one further away
	    uchar_t    subordinate_bus;
	    uchar_t    secondary_lat_timer;
	    uchar_t    io_base;
	    uchar_t    io_limit;
	    ushort_t   secondary_status;
	    ushort_t   memory_base;
	    ushort_t   memory_limit;
	    ushort_t   prefetchable_memory_base;
	    ushort_t   prefetchable_memory_limit;
	    uint_t     prefetchable_memory_base_upper;  // for 64 bit?
	    uint_t     prefetchable_memory_limit_upper;
	    ushort_t   io_base_upper;  // upper 16
	    ushort_t   io_limit_upper; 
	    uint_t     reserved;
	    uint_t     expansion_rom_address;
	    uchar_t    intr_line;
	    uchar_t    intr_pin;
	    ushort_t   bridge_ctl;
	    uint_t     device_data[48];
	}  __attribute__((__packed__))  pci_pci_bridge_config;
	

	// header = 02 (pci-cardbus bridge)
	struct {
	    uint_t     cardbus_base_addr;
	    uchar_t    cap_ptr;
	    uchar_t    reserved;
	    ushort_t   secondary_status;
	    uchar_t    pci_bus;
	    uchar_t    cardbus_bus;
	    uchar_t    subordinate_bus;
	    uchar_t    cardbus_lat_timer;
	    uint_t     memory_base0;
	    uint_t     memory_limit0;
	    uint_t     memory_base1;
	    uint_t     memory_limit1;
	    ushort_t   io_base0;
	    ushort_t   io_base0_upper;
	    ushort_t   io_limit0;
	    ushort_t   io_limit0_upper;
	    ushort_t   io_base1;
	    ushort_t   io_base1_upper;
	    ushort_t   io_limit1;
	    ushort_t   io_limit1_upper;
	    uchar_t    intr_line;
	    uchar_t    intr_pin;
	    ushort_t   bridge_ctl;
	    ushort_t   subsystem_vendor_id;
	    ushort_t   subsystem_device_id;
	    uint_t     legacy_16bit_base_addr;
	    uint_t     reserved2[14];
	    uint_t     device_data[32];
	}  __attribute__((__packed__)) pci_cardbus_bridge_config;
	
    }  __attribute__((__packed__)) u;
    
};

struct pci_bus {
    uint_t number;
    struct pci_bus * next;
    
    struct pci_device * device_list;
};

struct pci_device {
    uint_t number;
    uint_t function;
    struct pci_bus    * bus;
    struct pci_device * next;
  
    struct pci_device_config config;
};

struct pci {
    uint_t          num_buses;
    struct pci_bus * bus_list;
};
    

static uint_t ReadPCIDWord(uint_t bus, uint_t dev, uint_t func, uint_t offset) {
    uint_t address;
    uint_t data;
 
    address = ((bus << 16) | (dev << 11) |
	       (func << 8) | (offset & 0xfc) | ((uint_t)0x80000000));
 
    Out_DWord(PCI_CONFIG_ADDRESS, address);
    data = In_DWord(PCI_CONFIG_DATA);

    return data;
}


static ushort_t ReadPCIWord(uint_t bus, uint_t dev, uint_t func, uint_t offset) {
    return (ushort_t) (ReadPCIDWord(bus, dev, func, offset) >> ((offset & 0x2) * 8));
}


static struct pci * NewPCI() {
    struct pci * p = (struct pci *)Malloc(sizeof(struct pci));
    p->bus_list = NULL;
    p->num_buses = 0;
    return p;
}

static void AddPCIBus(struct pci * p, struct pci_bus * bus) {
    bus->next = p->bus_list;
    p->bus_list = bus;
}
    

static struct pci_bus * NewPCIBus(struct pci * p) {
    struct pci_bus * pb = (struct pci_bus *)Malloc(sizeof(struct pci_bus));
    pb->device_list = NULL;
    pb->number = (p->num_buses);
    p->num_buses++;
    return pb;
}

static void AddPCIDevice(struct pci_bus * b, struct pci_device * d) {
    d->bus = b;
    d->next = b->device_list;
    b->device_list = d;
}
 
static struct pci_device * NewPCIDevice(struct pci_bus * pb) {
    struct pci_device *pd = (struct pci_device *)Malloc(sizeof(struct pci_device));
    pd->number = 0;
    pd->bus = NULL;
    pd->next = NULL;
    return pd;
}

static void GetPCIDeviceConfig(uint_t bus, uint_t dev, uint_t fn, struct pci_device * d) {
    uint_t numdwords = (sizeof(struct pci_device_config) / 4);
    uint_t i;
    uint_t * p = (uint_t *)&(d->config);

    for (i = 0; i < numdwords; i++) {
	p[i] = ReadPCIDWord(bus, dev, fn, i * 4);
	PrintBoth("Reading Config Word %d (val=%x)\n", i * 4, p[i]);
    }
}



static struct pci * ScanPCI() {
    uint_t bus, dev, fn;
    ushort_t vendor;
    struct pci * thepci = NewPCI();
    struct pci_bus * thebus;
 
    for (bus = 0; bus < PCI_MAX_NUM_BUSES; bus++) {

	// Are there any devices on the bus?
	for (dev = 0; dev < 32; dev++) { 
	    vendor = ReadPCIWord(bus, dev, 0, 0);

	    if (vendor != 0xffff) { 
		break;
	    }
	}

	if (dev == 32) { 
	    continue;
	}

	// There are devices.  Create a bus.
	thebus = NewPCIBus(thepci);
	thebus->number = bus;


	// Add the devices to the bus
	for (dev = 0; dev < 32; dev++) { 
	    for (fn = 0; fn < 7; fn++) {
		
		vendor = ReadPCIWord(bus, dev, fn, 0);

		if (vendor != 0xffff) { 
		    struct pci_device * thedev = NewPCIDevice(thebus);
		    
		    thedev->number = dev;
		    thedev->function = fn;

		    GetPCIDeviceConfig(bus, dev, fn, thedev);
		    
		    AddPCIDevice(thebus, thedev);
		}
	    }
	}

	AddPCIBus(thepci,thebus);
    }


    return thepci;
}

static void PrintPCIShared(struct pci_device * thedev) {
    PrintBoth("    Slot: %u.%u\n", thedev->number, thedev->function);
    PrintBoth("      vendor_id:        0x%x\n", (uint_t) thedev->config.vendor_id);
    PrintBoth("      device_id:        0x%x\n", (uint_t) thedev->config.device_id);
    PrintBoth("      command:          0x%x\n", (uint_t) thedev->config.command);
    PrintBoth("      status:           0x%x\n", (uint_t) thedev->config.status);
    PrintBoth("      revision:         0x%x\n", (uint_t) thedev->config.revision);
    PrintBoth("      class_code:       0x%x%x%x (prog_interface 0x%x, subclass 0x%x, classcode 0x%x)\n", 
	      (uint_t) thedev->config.class_code[0],
	      (uint_t) thedev->config.class_code[1],
	      (uint_t) thedev->config.class_code[2],
	      (uint_t) PROG_INTERFACE(thedev->config.class_code),
	      (uint_t) SUBCLASS(thedev->config.class_code),
	      (uint_t) CLASSCODE(thedev->config.class_code) );
    PrintBoth("      cache_line_size:  0x%x\n", (uint_t) thedev->config.cache_line_size);
    PrintBoth("      latency_time:     0x%x\n", (uint_t) thedev->config.latency_time);
    PrintBoth("      header_type:      0x%x (%s%s)\n", (uint_t) thedev->config.header_type,
	      HEADER_TYPE(thedev->config.header_type)==PCI_DEVICE ? "PCI Device" :
	      HEADER_TYPE(thedev->config.header_type)==PCI_PCI_BRIDGE ? "PCI-PCI Bridge" :
	      HEADER_TYPE(thedev->config.header_type)==PCI_CARDBUS_BRIDGE ? "PCI-Cardbus Bridge" : "UNKNOWN",
	      MULTIFUNCTION(thedev->config.header_type) ? " Multifunction" : ""
	      );
    PrintBoth("      BIST:             0x%x\n", (uint_t) thedev->config.BIST);
}

static void PrintPCIDevice(struct pci_device * thedev) {
    int i;
  
    PrintBoth("     PCI Device:\n");

    PrintPCIShared(thedev);

    for (i = 0; i < 6; i++) { 
	PrintBoth("      BAR[%d]:           0x%x (", i, (uint_t) thedev->config.u.pci_device_config.BAR[i]);

	if (IS_IO_ADDR(thedev->config.u.pci_device_config.BAR[i])) { 

	    PrintBoth("IO Address 0x%x)\n", GET_IO_ADDR(thedev->config.u.pci_device_config.BAR[i]));

	} else if (IS_MEM_ADDR(thedev->config.u.pci_device_config.BAR[i])) { 

	    PrintBoth("Memory Address 0x%x type=0x%x%s\n", 
		      GET_MEM_ADDR(thedev->config.u.pci_device_config.BAR[i]),
		      GET_MEM_TYPE(thedev->config.u.pci_device_config.BAR[i]),
		      GET_MEM_PREFETCHABLE(thedev->config.u.pci_device_config.BAR[i]) ? " prefetchable)" : ")");

	} else {
	    PrintBoth("UNKNOWN)\n");
	}
    }

    PrintBoth("      cardbus_cis_ptr:  0x%x\n", (uint_t) thedev->config.u.pci_device_config.cardbus_cis_pointer);
    PrintBoth("      subsystem_vendor: 0x%x\n", (uint_t) thedev->config.u.pci_device_config.subsystem_vendor_id);
    PrintBoth("      subsystem_id:     0x%x\n", (uint_t) thedev->config.u.pci_device_config.subsystem_id);
    PrintBoth("      exp_rom_address:  0x%x\n", (uint_t) thedev->config.u.pci_device_config.expansion_rom_address);
    PrintBoth("      cap ptr           0x%x\n", (uint_t) thedev->config.u.pci_device_config.cap_ptr);

    for (i = 0; i < 7; i++) { 
	PrintBoth("      reserved[%d]:      0x%x\n", i, (uint_t) thedev->config.u.pci_device_config.reserved[i]);
    }

    PrintBoth("      intr_line:        0x%x\n", (uint_t) thedev->config.u.pci_device_config.intr_line);
    PrintBoth("      intr_pin:         0x%x\n", (uint_t) thedev->config.u.pci_device_config.intr_pin);
    PrintBoth("      min_grant:        0x%x\n", (uint_t) thedev->config.u.pci_device_config.min_grant);
    PrintBoth("      max_latency:      0x%x\n", (uint_t) thedev->config.u.pci_device_config.max_latency);

    for (i = 0; i < 48; i++) { 
	PrintBoth("      device_data[%d]:   0x%x\n", i, (uint_t) thedev->config.u.pci_device_config.device_data[i]);
    }
}

static void PrintPCIPCIBridge(struct pci_device * thedev) {
    int i;
  
    PrintBoth("     PCI-PCI Bridge:\n");

    PrintPCIShared(thedev);

    for (i = 0; i < 2; i++) { 

	PrintBoth("      BAR[%d]:           0x%x (", i, (uint_t) thedev->config.u.pci_pci_bridge_config.BAR[i]);

	if (IS_IO_ADDR(thedev->config.u.pci_pci_bridge_config.BAR[i])) { 

	    PrintBoth("IO Address 0x%x)\n", GET_IO_ADDR(thedev->config.u.pci_pci_bridge_config.BAR[i]));

	} else if (IS_MEM_ADDR(thedev->config.u.pci_pci_bridge_config.BAR[i])) { 

	    PrintBoth("Memory Address 0x%x type=0x%x%s\n",
		      GET_MEM_ADDR(thedev->config.u.pci_pci_bridge_config.BAR[i]),
		      GET_MEM_TYPE(thedev->config.u.pci_pci_bridge_config.BAR[i]),
		      GET_MEM_PREFETCHABLE(thedev->config.u.pci_pci_bridge_config.BAR[i]) ? " prefetchable)" : ")");

	} else {
	    PrintBoth("UNKNOWN)\n");
	}
    }

    PrintBoth("      primary_bus:      0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.primary_bus);
    PrintBoth("      secondary_bus:    0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.secondary_bus);
    PrintBoth("      subordinate_bus:  0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.subordinate_bus);
    PrintBoth("      second_lat_timer: 0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.secondary_lat_timer);
    PrintBoth("      io_base:          0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.io_base);
    PrintBoth("      io_limit:         0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.io_limit);
    PrintBoth("      secondary_status: 0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.secondary_status);
    PrintBoth("      memory_base:      0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.memory_base);
    PrintBoth("      memory_limit:     0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.memory_limit);
    PrintBoth("      prefetch_base:    0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.prefetchable_memory_base);
    PrintBoth("      prefetch_limit:   0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.prefetchable_memory_limit);
    PrintBoth("      prefetch_base_up: 0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.prefetchable_memory_base_upper);
    PrintBoth("      prefetch_limit_u: 0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.prefetchable_memory_limit_upper);
    PrintBoth("      memory_limit:     0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.memory_limit);
    PrintBoth("      memory_limit:     0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.memory_limit);
    PrintBoth("      io_base_up:       0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.io_base_upper);
    PrintBoth("      io_limit_up:      0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.io_limit_upper);
    PrintBoth("      reserved:         0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.reserved);
    PrintBoth("      exp_rom_address:  0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.expansion_rom_address);
    PrintBoth("      intr_line:        0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.intr_line);
    PrintBoth("      intr_pin:         0x%x\n", (uint_t) thedev->config.u.pci_pci_bridge_config.intr_pin);

    for (i = 0; i < 48; i++) { 
	PrintBoth("      device_data[%d]:   0x%x\n", i, (uint_t) thedev->config.u.pci_pci_bridge_config.device_data[i]);
    }
}

static void PrintPCICardbusBridge(struct pci_device * thedev) {
    int i;
  
    PrintBoth("     PCI-Cardbus Bridge:\n");

    PrintPCIShared(thedev);

    PrintBoth("      cardbus_base_add: 0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.cardbus_base_addr);
    PrintBoth("      cap_ptr:          0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.cap_ptr);
    PrintBoth("      reserved:         0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.reserved);
    PrintBoth("      secondary_status  0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.secondary_status);
    PrintBoth("      pci_bus:          0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.pci_bus);
    PrintBoth("      cardbus_bus:      0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.cardbus_bus);
    PrintBoth("      subordinate_bus:  0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.subordinate_bus);
    PrintBoth("      cardbus_lat_time: 0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.cardbus_lat_timer);
    PrintBoth("      memory_base0      0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.memory_base0);
    PrintBoth("      memory_limit0     0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.memory_limit0);
    PrintBoth("      memory_base1      0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.memory_base1);
    PrintBoth("      memory_limit1     0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.memory_limit1);
    PrintBoth("      io_base0          0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_base0);
    PrintBoth("      io_base0_up       0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_base0_upper);
    PrintBoth("      io_limit0         0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_limit0);
    PrintBoth("      io_limit0_up      0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_limit0_upper);
    PrintBoth("      io_base1          0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_base1);
    PrintBoth("      io_base1_up       0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_base1_upper);
    PrintBoth("      io_limit1         0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_limit1);
    PrintBoth("      io_limit1_up      0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.io_limit1_upper);
    PrintBoth("      intr_line:        0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.intr_line);
    PrintBoth("      intr_pin:         0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.intr_pin);
    PrintBoth("      bridge_ctl:       0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.bridge_ctl);
    PrintBoth("      subsys_vend_id:   0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.subsystem_vendor_id);
    PrintBoth("      subsys_dev_id:    0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.subsystem_device_id);
    PrintBoth("      legacy16_base:    0x%x\n", (uint_t) thedev->config.u.pci_cardbus_bridge_config.legacy_16bit_base_addr);
    
    for (i = 0; i < 14; i++) {
	PrintBoth("      reserved2[%d]:    0x%x\n", 
		  (uint_t)thedev->config.u.pci_cardbus_bridge_config.reserved2[i]);
    }

    for (i = 0; i < 48; i++) { 
	PrintBoth("      device_data[%d]:   0x%x\n", i, 
		  (uint_t)thedev->config.u.pci_cardbus_bridge_config.device_data[i]);
    }
}

static void PrintPCIUnknown(struct pci_device * thedev) {
    PrintBoth("    PCI Unknown Element\n");
    PrintPCIShared(thedev);
}

static void PrintPCIElement(struct pci_device * thedev)  { 
    switch (HEADER_TYPE(thedev->config.header_type)) { 

	case PCI_DEVICE:
	    PrintPCIDevice(thedev);
	    break;

	case PCI_PCI_BRIDGE:
	    PrintPCIPCIBridge(thedev);
	    break;

	case PCI_CARDBUS_BRIDGE:
	    PrintPCICardbusBridge(thedev);
	    break;

	default:
	    PrintPCIUnknown(thedev);
	    break;
    }
}
    

static void PrintPCIBus(struct pci_bus * thebus) {
    struct pci_device * thedev;

    PrintBoth("  PCI Bus:\n");
    PrintBoth("   Number: %u\n",thebus->number);

    thedev = thebus->device_list;

    while (thedev) { 
	PrintPCIElement(thedev);
	thedev = thedev->next;
    }
}

static void PrintPCI(struct pci * thepci) {
    struct pci_bus * thebus;

    PrintBoth("PCI Configuration:\n");
    PrintBoth(" Number of Buses: %u\n", thepci->num_buses);

    thebus = thepci->bus_list;

    while (thebus) { 
	PrintPCIBus(thebus);
	thebus = thebus->next;
    }

}

int Init_PCI() {
    PrintPCI(ScanPCI());

    return 0;

}
