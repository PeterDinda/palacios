/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda <pdinda@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_string.h>
#include <palacios/vm_guest_mem.h>

/* 
  The guest bios is compiled with blank space for am MP table
  at a default address.  A cookie value is temporarily placed 
  there so we can verify it exists.  If it does, we overwrite
  the MP table based on the configuration we are given in the 
  guest info.  

  Currently, we set up n identical processors (based on
  number of cores in guest info), with apics 0..n-1, and
  ioapic as n.

  The expectation is that the target will have 
  8 bytes (for ___HVMMP signature) followed by 896 bytes of space
  for a total of 904 bytes of space.
  We write the floating pointer at target (16 bytes), 
  immediately followed  by the mp config header, followed by
  the entries. 

*/

#define BIOS_MP_TABLE_DEFAULT_LOCATION 0xfcc00   // guest physical (linear)
#define BIOS_MP_TABLE_COOKIE         "___HVMMP"
#define BIOS_MP_TABLE_COOKIE_LEN     8

#define POINTER_SIGNATURE "_MP_"
#define HEADER_SIGNATURE "PCMP"

#define SPEC_REV ((uint8_t)0x4)
#define OEM_ID   "V3VEE   "
#define PROD_ID  "PALACIOS 1.3 "

#define LAPIC_ADDR 0xfee00000
#define LAPIC_VERSION  0x11

#define ENTRY_PROC 0
#define ENTRY_BUS 1
#define ENTRY_IOAPIC 2
#define ENTRY_IOINT 3
#define ENTRY_LOINT 4

#define IOAPIC_ADDR 0xfec00000
#define IOAPIC_VERSION 0x11

// These are bochs defaults - should really come from cpuid of machne
#define PROC_FAMILY 0x6
#define PROC_STEPPING 0x0
#define PROC_MODEL 0x0
#define PROC_FEATURE_FLAGS 0x00000201 


#define BUS_ISA "ISA   "

#define INT_TYPE_INT 0
#define INT_TYPE_NMI 1
#define INT_TYPE_SMI 2
#define INT_TYPE_EXT 3

#define INT_POLARITY_DEFAULT     0
#define INT_POLARITY_ACTIVE_HIGH 1
#define INT_POLARITY_RESERVED    2
#define INT_POLARITY_ACTIVE_LOW  3

#define INT_TRIGGER_DEFAULT      0
#define INT_TRIGGER_EDGE         1
#define INT_TRIGGER_RESERVED     2
#define INT_TRIGGER_LEVEL        3




// This points to the mp table header
struct mp_floating_pointer {
    uint32_t signature;          /* "_MP_" */
    uint32_t pointer;            /* gpa of MP table (0xfcc00) */
    uint8_t  length;             /* length in 16 byte chunks (paragraphs) */
    uint8_t  spec_rev;           /* 0x4 */
    uint8_t  checksum;
    uint8_t  mp_featurebyte[5];  /* zero out to indicate mp config table
				    first byte nonzero => default configurations (see spec)
				    second byte, bit 7 (top bit) = IMCR if set, virtual wire if zero */
} __attribute__((packed));


struct mp_table_header {
    uint32_t signature;                 /* "PCMP"                                             */
    uint16_t base_table_length;         /* bytes, starting from header                        */
    uint8_t  spec_rev;                  /* specification rvision (0x4 is the current rev)     */
    uint8_t  checksum;                  /* sum of all bytes, including checksum, must be zero */
    uint8_t  oem_id[8];                 /* OEM ID "V3VEE   "                                  */
    uint8_t  prod_id[12];               /* Product ID "PALACIOS 1.3"                          */
    uint32_t oem_table_ptr;             /* oem table, if used (zeroed)                        */
    uint16_t oem_table_size;            /* oem table length, if used                          */
    uint16_t entry_count;               /* numnber of entries in this table                   */
    uint32_t lapic_addr;                /* apic address on all processors                     */
    uint16_t extended_table_length;     /* zero by default                                    */
    uint8_t  extended_table_checksum;   /* zero by default                                    */
    uint8_t  reserved;                  /* zero by default                                    */
    /* this is followed by entries of the various types indicated below */
} __attribute__((packed));

struct mp_table_processor {
    uint8_t entry_type;          // type 0
    uint8_t lapic_id;            // 0..
    uint8_t lapic_version;       // 

    union {
	uint8_t data;       
	struct {
	    uint8_t en          : 1;        /* 1 = processor enabled */
	    uint8_t bp          : 1;        /* 1 = bootstrap processor */
	    uint8_t reserved    : 6;
	} __attribute__((packed));
    } __attribute__((packed)) cpu_flags;

    union {
	uint32_t data;
	struct {
	    uint8_t stepping    : 4;
	    uint8_t model       : 4;
	    uint8_t family      : 4; 
	    uint32_t rest       : 20;
	} __attribute__((packed));
    } __attribute__((packed)) cpu_signature;

    uint32_t cpu_feature_flags;      /* result of CPUID */
    uint32_t reserved[2];
} __attribute__((packed));

struct mp_table_bus {
    uint8_t entry_type;          /* type 1              */
    uint8_t bus_id;              /* 0..                 */
    uint8_t bus_type[6];         /* "PCI" "INTERN", etc */
} __attribute__((packed));


struct mp_table_ioapic {
    uint8_t entry_type;          /* type 2                            */
    uint8_t ioapic_id;           /* 0..                               */
    uint8_t ioapic_version;      /* bits 0..7 of the version register */

    union {
	uint8_t data;       
	struct {
	    uint8_t en         : 1;        /* 1=ioapic enabled */
	    uint8_t reserved   : 7;
	} __attribute__((packed));
    } __attribute__((packed)) ioapic_flags;

    uint32_t ioapic_address;     /* physical address (same for all procs) */
} __attribute__((packed));


struct mp_table_io_interrupt_assignment {
    uint8_t entry_type;          /* type 3 */
    uint8_t interrupt_type;      /* 0=int, 1=nmi, 2=smi, 3=ExtInt(8259) */
 
   union {
	uint16_t value;
	struct {
	    uint8_t po           : 2;        /* polarity (00 = default for bus, 01 = active high, 10 = reserved, 11 = active low */
	    uint8_t el           : 2;        /* trigger mode (00 = default for bus, 01 = edge, 10 = reserved, 11 = level) */
	    uint16_t reserved    : 12;
	} __attribute__((packed));
   } __attribute__((packed)) flags;

    uint8_t source_bus_id;
    uint8_t source_bus_irq;
    uint8_t dest_ioapic_id;
    uint8_t dest_ioapic_intn;
} __attribute__((packed));


struct mp_table_local_interrupt_assignment {
    uint8_t entry_type;          /* type 4 */
    uint8_t interrupt_type;      /* 0 = int, 1 = nmi, 2 = smi, 3 = ExtInt(8259) */

    union {
	uint16_t value;
	struct {
	    uint8_t po           : 2;        /* polarity (00 = default for bus, 01 = active high, 10 = reserved, 11 = active low */
	    uint8_t el           : 2;        /* trigger mode (00 = default for bus, 01 = edge, 10 = reserved, 11 = level) */
	    uint16_t reserved    : 12;
	} __attribute__((packed));
    } __attribute__((packed)) flags;

    uint8_t source_bus_id;
    uint8_t source_bus_irq;
    uint8_t dest_ioapic_id;
    uint8_t dest_ioapic_intn;
} __attribute__((packed));





static inline int check_for_cookie(void * target) {
    return (memcmp(target, BIOS_MP_TABLE_COOKIE, BIOS_MP_TABLE_COOKIE_LEN) == 0);
}

static inline int check_table(void * target) {
    uint32_t i;
    uint8_t sum;
    struct mp_table_header * header;

    header = (struct mp_table_header *)target;
    sum = 0;

    for (i = 0; i < header->base_table_length; i++) {
	sum += ((uint8_t *)target)[i];
    }

    if (sum == 0) { 
	return 1;
    } else {
	// failed checksum
	return 0;
    }
}


static inline int check_pointer(void * target) {
    uint32_t i;
    uint8_t sum;
    struct mp_floating_pointer * p;

    p = (struct mp_floating_pointer *)target;
    sum = 0;

    for (i = 0; i < p->length * 16; i++) {
	sum += ((uint8_t *)target)[i];
    }

    if (sum == 0) { 
	// passed
	return 1;
    } else {
	// failed
	return 0;
    }
}
    

static int write_pointer(void * target, uint32_t mptable_gpa) {
    uint32_t i;
    uint8_t sum;
    struct mp_floating_pointer * p = (struct mp_floating_pointer *)target;

    memset((void *)p, 0, sizeof(struct mp_floating_pointer));
    
    memcpy((void *)&(p->signature), POINTER_SIGNATURE, 4);
    
    p->pointer = mptable_gpa;
    p->length = 1;             // length in 16 byte chunks
    p->spec_rev = SPEC_REV;
    
    // checksum calculation
    p->checksum = 0;
    sum = 0;

    for (i = 0; i < 16; i++) {
	sum += ((uint8_t *)target)[i];
    }

    p->checksum = (255 - sum) + 1;

    return 0;
}
    

    

static int write_mptable(void * target, uint32_t numcores) {
    uint32_t i = 0;
    uint8_t sum = 0;
    uint8_t core = 0;
    uint8_t irq = 0;    
    struct mp_table_header * header = NULL;
    struct mp_table_processor * proc = NULL;
    struct mp_table_bus * bus = NULL;
    struct mp_table_ioapic * ioapic = NULL;
    struct mp_table_io_interrupt_assignment * interrupt = NULL;
    uint8_t * cur = target;

    header = (struct mp_table_header *)cur;
    cur = cur + sizeof(struct mp_table_header);
    
    memset((void *)header, 0, sizeof(struct mp_table_header));
    
    
    memcpy(&(header->signature), HEADER_SIGNATURE, 4);
    header->spec_rev = SPEC_REV;
    memcpy(header->oem_id, OEM_ID, 8);
    memcpy(header->prod_id, PROD_ID, 12);

    // n processors, 1 ioapic, 1 isa bus, 16 IRQs = 18+n
    header->entry_count = numcores + 18;
    header->lapic_addr = LAPIC_ADDR;
    
    // now we arrange the processors;
    
    for (core = 0; core < numcores; core++) { 
	proc = (struct mp_table_processor *)cur;
	memset((void *)proc, 0, sizeof(struct mp_table_processor));
	proc->entry_type = ENTRY_PROC;
	proc->lapic_id = core;
	proc->lapic_version = LAPIC_VERSION;
	proc->cpu_flags.en = 1;

	if (core == 0) {
	    proc->cpu_flags.bp = 1;
	} else {
	    proc->cpu_flags.bp = 0;
	}

	proc->cpu_signature.family = PROC_FAMILY;
	proc->cpu_signature.model = PROC_MODEL;
	proc->cpu_signature.stepping = PROC_STEPPING;
	proc->cpu_feature_flags = PROC_FEATURE_FLAGS;

	cur += sizeof(struct mp_table_processor);
    }

    // next comes the ISA bas
    bus = (struct mp_table_bus *)cur;
    cur += sizeof(struct mp_table_bus);

    memset((void *)bus, 0, sizeof(struct mp_table_bus));
    bus->entry_type = ENTRY_BUS;
    bus->bus_id = 0;
    memcpy(bus->bus_type, BUS_ISA, 6);

    // next comes the IOAPIC
    ioapic = (struct mp_table_ioapic *)cur;
    cur += sizeof(struct mp_table_ioapic);
    
    memset((void *)ioapic, 0, sizeof(struct mp_table_ioapic));
    ioapic->entry_type = ENTRY_IOAPIC;
    ioapic->ioapic_id = numcores;
    ioapic->ioapic_version = IOAPIC_VERSION;
    ioapic->ioapic_flags.en = 1;
    ioapic->ioapic_address = IOAPIC_ADDR;


    // The MPTABLE IRQ mappings are kind of odd. 
    // We don't include a bus IRQ 2, and instead remap Bus IRQ 0 to dest irq 2


    for (irq = 0; irq < 16; irq++) { 
	uint8_t dst_irq = irq;

	if (irq == 0) {
	    dst_irq = 2;
	} else if (irq == 2) {
	    continue;
	}

	interrupt = (struct mp_table_io_interrupt_assignment *)cur;
	memset((void *)interrupt, 0, sizeof(struct mp_table_io_interrupt_assignment));

	interrupt->entry_type = ENTRY_IOINT;
	interrupt->interrupt_type = INT_TYPE_INT;
	interrupt->flags.po = INT_POLARITY_DEFAULT;
	interrupt->flags.el = INT_TRIGGER_DEFAULT;
	interrupt->source_bus_id = 0;
	interrupt->source_bus_irq = irq;
	interrupt->dest_ioapic_id = numcores;
	interrupt->dest_ioapic_intn = dst_irq;

	cur += sizeof(struct mp_table_io_interrupt_assignment);
    }

    // now we can set the length;

    header->base_table_length = (cur - (uint8_t *)header);

    // checksum calculation
    header->checksum = 0;
    sum = 0;
    for (i = 0; i < header->base_table_length; i++) {
	sum += ((uint8_t *)target)[i];
    }
    header->checksum = (255 - sum) + 1;


	
    return 0;
}

static int mptable_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    void * target = NULL;

    if (v3_gpa_to_hva(&(vm->cores[0]), BIOS_MP_TABLE_DEFAULT_LOCATION, (addr_t *)&target) == -1) { 
	PrintError("Cannot inject mptable due to unmapped bios!\n");
	return -1;
    }
    
    if (!check_for_cookie(target)) { 
	PrintError("Cookie mismatch in writing mptable, aborting (probably wrong guest BIOS).\n");
	return -1;
    }

    if (vm->num_cores > 32) { 
	PrintError("No support for >32 cores in writing MP table, aborting.\n");
	return -1;
    }

    V3_Print("Constructing mptable for %u cores at %p\n", vm->num_cores, target);

    if (write_pointer(target, BIOS_MP_TABLE_DEFAULT_LOCATION + sizeof(struct mp_floating_pointer)) == -1) { 
	PrintError("Unable to write mptable floating pointer, aborting.\n");
	return -1;
    }

    if (!check_pointer(target)) { 
	PrintError("Failed to inject mptable floating pointer correctly --- checksum fails\n");
	return -1;
    }

    if (write_mptable(target + sizeof(struct mp_floating_pointer), vm->num_cores) == -1) {
	PrintError("Cannot inject mptable configuration header and entries\n");
	return -1;
    }

    if (!check_table(target + sizeof(struct mp_floating_pointer))) { 
	PrintError("Failed to inject mptable configuration header and entries correctly --- checksum fails\n");
	return -1;
    }


    return 0;
}



device_register("MPTABLE", mptable_init)
