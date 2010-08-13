/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/icc_bus.h>
#include <devices/apic_regs.h>
#include <palacios/vm_guest.h>

#ifndef CONFIG_DEBUG_IO_APIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



#define IO_APIC_BASE_ADDR 0xfec00000


#define IOAPIC_ID_REG 0x00
#define IOAPIC_VER_REG 0x01
#define IOAPIC_ARB_REG 0x02

#define IOAPIC_REDIR_BASE_REG 0x10

#define REDIR_LO_MASK  ~0x00005000

struct ioapic_reg_sel {
    union {
	uint32_t val;
	struct {
	    uint_t reg_addr     : 8;
	    uint_t rsvd         : 24;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct ioapic_id_reg {
    union {
	uint32_t val;
	struct {
	    uint_t rsvd1      : 24;
	    uint_t id         : 4;
	    uint_t rsvd2      : 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct ioapic_ver_reg {
    union {
	uint32_t val;
	struct {
	    uint_t version    : 8;
	    uint_t rsvd1      : 8;
	    uint_t max_redir  : 8;
	    uint_t rsvd2      : 8;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ioapic_arb_reg {
    union {
	uint32_t val;
	struct {
	    uint_t rsvd1      : 24;
	    uint_t max_redir  : 4;
	    uint_t rsvd2      : 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct redir_tbl_entry {
    union {
	uint64_t val;
	struct {
	    uint32_t lo;
	    uint32_t hi;
	} __attribute__((packed));
	struct {
	    uint_t vec        : 8;

#define FIXED        0x0
#define LOWEST_PRIOR 0x1
#define SMI          0x2
#define NMI          0x4
#define INIT         0x5
#define EXTINT       0x7
	    uint_t del_mode   : 3;

#define PHSYICAL_DST_MODE 0
#define LOGICAL_DST_MODE 1
	    uint_t dst_mode   : 1;
	    uint_t del_status : 1;

#define HIGH_ACTIVE 0
#define LOW_ACTIVE 1
	    uint_t intr_pol   : 1;
	    uint_t rem_irr    : 1;
	    uint_t trig_mode  : 1;
	    uint_t mask       : 1;
	    uint64_t rsvd     : 39;
	    uint_t dst_field  : 8;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



struct io_apic_state {
    addr_t base_addr;

    uint32_t index_reg;

    struct ioapic_id_reg ioapic_id;
    struct ioapic_ver_reg ioapic_ver;
    struct ioapic_arb_reg ioapic_arb_id;
  
    struct redir_tbl_entry redir_tbl[24];

    struct vm_device * icc_bus;
  
};


static void init_ioapic_state(struct io_apic_state * ioapic, uint32_t id) {
    int i = 0;
    ioapic->base_addr = IO_APIC_BASE_ADDR;
    ioapic->index_reg = 0;

    ioapic->ioapic_id.val = id;
    ioapic->ioapic_ver.val = 0x00170011;
    ioapic->ioapic_arb_id.val = 0x00000000;

    for (i = 0; i < 24; i++) {
	ioapic->redir_tbl[i].val = 0x0001000000000000LL;
	// Mask all interrupts until they are enabled....
	ioapic->redir_tbl[i].mask = 1;
    }
    
    // special case redir_tbl[0] for pin 0 as ExtInt for Virtual Wire Mode
    ioapic->redir_tbl[0].del_mode=EXTINT;
    ioapic->redir_tbl[0].mask=0;
}


static int ioapic_read(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);
    uint32_t reg_tgt = guest_addr - ioapic->base_addr;
    uint32_t * op_val = (uint32_t *)dst;

    PrintDebug("ioapic %u: IOAPIC Read at %p\n", ioapic->ioapic_id.val, (void *)guest_addr);

    if (reg_tgt == 0x00) {
	*op_val = ioapic->index_reg;
    } else if (reg_tgt == 0x10) {
	// IOWIN register
	switch (ioapic->index_reg) {
	    case IOAPIC_ID_REG:
		*op_val = ioapic->ioapic_id.val;
		break;
	    case IOAPIC_VER_REG:
		*op_val = ioapic->ioapic_ver.val;
		break;
	    case IOAPIC_ARB_REG:
		*op_val = ioapic->ioapic_arb_id.val;
		break;
	    default:
		{
		    uint_t redir_index = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) >> 1;
		    uint_t hi_val = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) % 1;

		    if (redir_index > 0x3f) {
			PrintError("ioapic %u: Invalid redirection table entry %x\n", ioapic->ioapic_id.val, (uint32_t)redir_index);
			return -1;
		    }
		    if (hi_val) {
			*op_val = ioapic->redir_tbl[redir_index].hi;
		    } else {
			*op_val = ioapic->redir_tbl[redir_index].lo;
		    }
		}
	}
    }

    PrintDebug("ioapic %u: IOAPIC Read at %p gave value 0x%x\n", ioapic->ioapic_id.val, (void *)guest_addr, *op_val);

    return length;
}


static int ioapic_write(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);
    uint32_t reg_tgt = guest_addr - ioapic->base_addr;
    uint32_t op_val = *(uint32_t *)src;

    PrintDebug("ioapic %u: IOAPIC Write at %p (val = %d)\n",  ioapic->ioapic_id.val, (void *)guest_addr, *(uint32_t *)src);

    if (reg_tgt == 0x00) {
	ioapic->index_reg = op_val;
    } else if (reg_tgt == 0x10) {
	// IOWIN register
	switch (ioapic->index_reg) {
	    case IOAPIC_ID_REG:
		ioapic->ioapic_id.val = op_val;
		break;
	    case IOAPIC_VER_REG:
		// GPF/PageFault/Ignore?
		PrintError("ioapic %u: Writing to read only IOAPIC register\n", ioapic->ioapic_id.val);
		return -1;
	    case IOAPIC_ARB_REG:
		ioapic->ioapic_arb_id.val = op_val;
		break;
	    default:
		{
		    uint_t redir_index = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) >> 1;
		    uint_t hi_val = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) % 1;




		    if (redir_index > 0x3f) {
			PrintError("ioapic %u: Invalid redirection table entry %x\n", ioapic->ioapic_id.val, (uint32_t)redir_index);
			return -1;
		    }
		    if (hi_val) {
			PrintDebug("ioapic %u: Writing to hi of pin %d\n", ioapic->ioapic_id.val, redir_index);
			ioapic->redir_tbl[redir_index].hi = op_val;
		    } else {
			PrintDebug("ioapic %u: Writing to lo of pin %d\n", ioapic->ioapic_id.val, redir_index);
			op_val &= REDIR_LO_MASK;
			ioapic->redir_tbl[redir_index].lo &= ~REDIR_LO_MASK;
			ioapic->redir_tbl[redir_index].lo |= op_val;
		    }
		}
	}
    }

    return length;
}


static int ioapic_raise_irq(struct v3_vm_info * vm, void * private_data, int irq) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);  
    struct redir_tbl_entry * irq_entry = NULL;

    if (irq > 24) {
	PrintDebug("ioapic %u: IRQ out of range of IO APIC\n", ioapic->ioapic_id.val);
	return -1;
    }

    irq_entry = &(ioapic->redir_tbl[irq]);

    if (irq_entry->mask == 0) {

	PrintDebug("ioapic %u: IOAPIC Signalling APIC to raise INTR %d\n", ioapic->ioapic_id.val, irq_entry->vec);


	// the format of the redirection table entry is just slightly 
	// different than that of the lapic's cmd register, which is the other
	// way an IPI is initiated.   So we will translate
	//
	struct int_cmd_reg icr;
	
	icr.val = irq_entry->val;
	icr.rsvd1=0;
	icr.lvl=1;
	icr.trig_mode=irq_entry->trig_mode;
	icr.rem_rd_status=0;
	icr.dst_shorthand=0; // no shorthand
	icr.rsvd2=0;

	v3_icc_send_ipi(ioapic->icc_bus, ioapic->ioapic_id.val,icr.val, irq);
    }

    return 0;
}

/* I don't know if we can do anything here.... */
static int ioapic_lower_irq(struct v3_vm_info * vm, void * private_data, int irq) {
    return 0;
}

static struct intr_router_ops router_ops = {
    .raise_intr = ioapic_raise_irq,
    .lower_intr = ioapic_lower_irq, 
};




static int io_apic_free(struct vm_device * dev) {
    //  struct guest_info * info = dev->vm;

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = io_apic_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};



static int ioapic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * icc_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    char * dev_id = v3_cfg_val(cfg, "ID");

    if (!icc_bus) {
	PrintError("ioapic: Could not locate ICC BUS device (%s)\n", v3_cfg_val(cfg, "bus"));
	return -1;
    }

    PrintDebug("ioapic: Creating IO APIC\n");

    struct io_apic_state * ioapic = (struct io_apic_state *)V3_Malloc(sizeof(struct io_apic_state));

    ioapic->icc_bus = icc_bus;

    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, ioapic);


    if (v3_attach_device(vm, dev) == -1) {
	PrintError("ioapic: Could not attach device %s\n", dev_id);
	return -1;
    }


    v3_register_intr_router(vm, &router_ops, dev);

    init_ioapic_state(ioapic,vm->num_cores);

    v3_icc_register_ioapic(vm,icc_bus,ioapic->ioapic_id.val);

    v3_hook_full_mem(vm, V3_MEM_CORE_ANY, ioapic->base_addr, ioapic->base_addr + PAGE_SIZE_4KB, 
		     ioapic_read, ioapic_write, dev);
  
    return 0;
}


device_register("IOAPIC", ioapic_init)
