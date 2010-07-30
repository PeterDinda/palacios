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
#include <palacios/vmm_mptable.h>
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

#define SPEC_REV ((uchar_t)0x4)
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
    uint32_t signature;          // "_MP_"
    uint32_t pointer;            // gpa of MP table (0xfcc00)
    uint8_t  length;             // length in 16 byte chunks (paragraphs)
    uint8_t  spec_rev;           // 0x4
    uint8_t  checksum;          
    uint8_t  mp_featurebyte[5];  // zero out to indicate mp config table
    // first byte nonzero => default configurations (see spec)
    // second byte, bit 7 (top bit) = IMCR if set, virtual wire if zero
} __attribute__((packed));


struct mp_table_header {
    uint32_t signature;          // "PCMP"
    uint16_t base_table_length;  // bytes, starting from header
    uint8_t  spec_rev;           // specification rvision (0x4 is the current rev)
    uint8_t  checksum;           // sum of all bytes, including checksum, must be zero
    uint8_t  oem_id[8];          // OEM ID "V3VEE   "
    uint8_t  prod_id[12];        // Product ID "PALACIOS 1.3"
    uint32_t oem_table_ptr;      // oem table, if used (zeroed)
    uint16_t oem_table_size;     // oem table length, if used
    uint16_t entry_count;        // numnber of entries in this table
    uint32_t lapic_addr;         // apic address on all processors
    uint16_t extended_table_length; // zero by default
    uint8_t  extended_table_checksum; // zero by default
    uint8_t  reserved;           // zero by default
    // this is followed by entries of the various types indicated below
} __attribute__((packed));

struct mp_table_processor {
    uint8_t entry_type;          // type 0
    uint8_t lapic_id;            // 0..
    uint8_t lapic_version;       // 
    union {
	uint8_t data;       
	struct {
	    uint8_t en:1;        // 1=processor enabled
	    uint8_t bp:1;        // 1=bootstrap processor
	    uint8_t reserved:6;
	} fields;
    } cpu_flags;
    union {
	uint32_t data;
	struct {
	    uint8_t stepping:4;
	    uint8_t model:4;
	    uint8_t family:4; 
	    uint32_t rest:20;
	} fields;
    } cpu_signature;
    uint32_t cpu_feature_flags;      // result of CPUID
    uint32_t reserved[2];
} __attribute__((packed));

struct mp_table_bus {
    uint8_t entry_type;          // type 1
    uint8_t bus_id;              // 0..
    uint8_t bus_type[6];         // "PCI" "INTERN", etc
} __attribute__((packed));


struct mp_table_ioapic {
    uint8_t entry_type;          // type 2
    uint8_t ioapic_id;           // 0..
    uint8_t ioapic_version;      // bits 0..7 of the version register
    union {
	uint8_t data;       
	struct {
	    uint8_t en:1;        // 1=ioapic enabled
	    uint8_t reserved:7;
	} fields;
    } ioapic_flags;
    uint32_t ioapic_address;     // physical address (same for all procs)
} __attribute__((packed));


struct mp_table_io_interrupt_assignment {
    uint8_t entry_type;          // type 3
    uint8_t interrupt_type;      // 0=int, 1=nmi, 2=smi, 3=ExtInt(8259)
    union {
	uint16_t data;
	struct {
	    uint8_t po:2;        // polarity (00=default for bus, 01=active high, 10=reserved, 11=active low
	    uint8_t el:2;        // trigger mode (00=default for bus, 01=edge, 10=reserved, 11=level)
	    uint16_t reserved:12;
	} fields;
    } io_interrupt_flags;
    uint8_t source_bus_id;
    uint8_t source_bus_irq;
    uint8_t dest_ioapic_id;
    uint8_t dest_ioapic_intn;
} __attribute__((packed));


struct mp_table_local_interrupt_assignment {
    uint8_t entry_type;          // type 4
    uint8_t interrupt_type;      // 0=int, 1=nmi, 2=smi, 3=ExtInt(8259)
    union {
	uint16_t data;
	struct {
	    uint8_t po:2;        // polarity (00=default for bus, 01=active high, 10=reserved, 11=active low
	    uint8_t el:2;        // trigger mode (00=default for bus, 01=edge, 10=reserved, 11=level)
	    uint16_t reserved:12;
	} fields;
    } io_interrupt_flags;
    uint8_t source_bus_id;
    uint8_t source_bus_irq;
    uint8_t dest_ioapic_id;
    uint8_t dest_ioapic_intn;
} __attribute__((packed));





static int check_for_cookie(void *target)
{
    return 0==memcmp(target,BIOS_MP_TABLE_COOKIE,BIOS_MP_TABLE_COOKIE_LEN);
}

static int check_table(void *target)
{
    uint32_t i;
    uint8_t sum;
    struct mp_table_header *header;

    V3_Print("Checksuming mptable header and entries at %p\n",target);

    header=(struct mp_table_header *)target;
    sum=0;
    for (i=0;i<header->base_table_length;i++) {
	sum+=((uint8_t *)target)[i];
    }
    if (sum==0) { 
	V3_Print("Checksum passed\n");
	return 1;
    } else {
	V3_Print("Checksum FAILED\n");
	return 0;
    }
}


static int check_pointer(void *target)
{
    uint32_t i;
    uint8_t sum;
    struct mp_floating_pointer *p;

    V3_Print("Checksuming mptable floating pointer at %p\n",target);

    p=(struct mp_floating_pointer *)target;
    sum=0;
    for (i=0;i<p->length*16;i++) {
	sum+=((uint8_t *)target)[i];
    }
    if (sum==0) { 
	V3_Print("Checksum passed\n");
	return 1;
    } else {
	V3_Print("Checksum FAILED\n");
	return 0;
    }
}
    

static int write_pointer(void *target, uint32_t mptable_gpa)
{
    uint32_t i;
    uint8_t sum;
    struct mp_floating_pointer *p=(struct mp_floating_pointer*)target;

    memset((void*)p,0,sizeof(*p));
    
    memcpy((void*)&(p->signature),POINTER_SIGNATURE,4);
    
    p->pointer=mptable_gpa;
    p->length=1;             // length in 16 byte chunks
    p->spec_rev=SPEC_REV;
    
    // checksum calculation
    p->checksum=0;
    sum=0;
    for (i=0;i<16;i++) {
	sum+=((uint8_t *)target)[i];
    }
    p->checksum=(255-sum)+1;

    V3_Print("MP Floating Pointer written to %p\n",target);

    return 0;
}
    

    

static int write_mptable(void *target, uint32_t numcores)
{
    uint32_t i;
    uint8_t sum;
    uint8_t core;
    uint8_t irq;    
    uint8_t *cur;
    struct mp_table_header *header;
    struct mp_table_processor *proc;
    struct mp_table_bus *bus;
    struct mp_table_ioapic *ioapic;
    struct mp_table_io_interrupt_assignment *interrupt;


    cur=(uint8_t *)target;
    header=(struct mp_table_header *)cur;
    cur=cur+sizeof(*header);
    
    memset((void*)header,0,sizeof(*header));
    
    
    memcpy(&(header->signature),HEADER_SIGNATURE,4);
    header->spec_rev=SPEC_REV;
    memcpy(header->oem_id,OEM_ID,8);
    memcpy(header->prod_id,PROD_ID,12);

    // n processors, 1 ioapic, 1 isa bus, 16 IRQs = 18+n
    header->entry_count=numcores+18;
    header->lapic_addr=LAPIC_ADDR;
    
    // now we arrange the processors;
    
    for (core=0;core<numcores;core++, cur+=sizeof(*proc)) { 
	proc=(struct mp_table_processor *)cur;
	memset((void*)proc,0,sizeof(*proc));
	proc->entry_type=ENTRY_PROC;
	proc->lapic_id=core;
	proc->lapic_version=LAPIC_VERSION;
	proc->cpu_flags.fields.en=1;
	proc->cpu_flags.fields.bp = (core==0);
	proc->cpu_signature.fields.family=PROC_FAMILY;
	proc->cpu_signature.fields.model=PROC_MODEL;
	proc->cpu_signature.fields.stepping=PROC_STEPPING;
	proc->cpu_feature_flags=PROC_FEATURE_FLAGS;
    }

    // next comes the ISA bas
    bus=(struct mp_table_bus *)cur;
    cur+=sizeof(*bus);

    memset((void*)bus,0,sizeof(*bus));
    bus->entry_type=ENTRY_BUS;
    bus->bus_id=0;
    memcpy(bus->bus_type,BUS_ISA,6);

    // next comes the IOAPIC
    ioapic=(struct mp_table_ioapic *)cur;
    cur+=sizeof(*ioapic);
    
    memset((void*)ioapic,0,sizeof(*ioapic));
    ioapic->entry_type=ENTRY_IOAPIC;
    ioapic->ioapic_id=numcores;
    ioapic->ioapic_version=IOAPIC_VERSION;
    ioapic->ioapic_flags.fields.en=1;
    ioapic->ioapic_address=IOAPIC_ADDR;

    for (irq=0;irq<16;irq++, cur+=sizeof(*interrupt)) { 
	interrupt=(struct mp_table_io_interrupt_assignment *)cur;
	memset((void*)interrupt,0,sizeof(*interrupt));
	interrupt->entry_type=ENTRY_IOINT;
	interrupt->interrupt_type=INT_TYPE_INT;
	interrupt->io_interrupt_flags.fields.po=INT_POLARITY_DEFAULT;
	interrupt->io_interrupt_flags.fields.el=INT_TRIGGER_DEFAULT;
	interrupt->source_bus_id=0;
	interrupt->source_bus_irq=irq;
	interrupt->dest_ioapic_id=numcores;
	interrupt->dest_ioapic_intn=irq;
    }

    // now we can set the length;

    header->base_table_length = (cur-(uint8_t*)header);

    // checksum calculation
    header->checksum=0;
    sum=0;
    for (i=0;i<header->base_table_length;i++) {
	sum+=((uint8_t *)target)[i];
    }
    header->checksum=(255-sum)+1;


	
    return 0;
}


int v3_inject_mptable(struct v3_vm_info *vm)
{
    void *target;

    if (v3_gpa_to_hva(&(vm->cores[0]),BIOS_MP_TABLE_DEFAULT_LOCATION,(addr_t*)&target)==-1) { 
	PrintError("Cannot inject mptable due to unmapped bios!\n");
	return -1;
    }
    
    if (!check_for_cookie(target)) { 
	PrintError("Cookie mismatch in writing mptable, aborting (probably wrong guest BIOS).\n");
	return -1;
    }

    if (vm->num_cores>32) { 
	PrintError("No support for >32 cores in writing MP table, aborting.\n");
	return -1;
    }

    V3_Print("Starting mptable pointer, header, and entry construction for %u cores at %p\n",vm->num_cores,target);

    if (-1==write_pointer(target,BIOS_MP_TABLE_DEFAULT_LOCATION+sizeof(struct mp_floating_pointer))) { 
	PrintError("Unable to write mptable floating pointer, aborting.\n");
	return -1;
    }

    if (!check_pointer(target)) { 
	PrintError("Failed to inject mptable floating pointer correctly --- checksum fails\n");
	return -1;
    }

    if (-1==write_mptable(target+sizeof(struct mp_floating_pointer),vm->num_cores)) {
	PrintError("Cannot inject mptable configuration header and entries\n");
	return -1;
    }

    if (!check_table(target+sizeof(struct mp_floating_pointer))) { 
	PrintError("Failed to inject mptable configuration header and entries correctly --- checksum fails\n");
	return -1;
    }

    V3_Print("Done with mptable pointer, header, and entry construction\n");

    return 0;
    
}
