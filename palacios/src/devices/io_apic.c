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
#include <devices/apic.h>
#include <palacios/vm_guest.h>

#ifndef V3_CONFIG_DEBUG_IO_APIC
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

    void * apic_dev_data;

    void * router_handle;

    struct v3_vm_info * vm;
  
};


static void init_ioapic_state(struct io_apic_state * ioapic, uint32_t id) {
    int i = 0;
    ioapic->base_addr = IO_APIC_BASE_ADDR;
    ioapic->index_reg = 0;

    ioapic->ioapic_id.id = id;
    ioapic->ioapic_ver.val = 0x00170011;
    ioapic->ioapic_arb_id.val = 0x00000000;

    for (i = 0; i < 24; i++) {
	ioapic->redir_tbl[i].val = 0x0001000000000000LL;
	// Mask all interrupts until they are enabled....
	ioapic->redir_tbl[i].mask = 1;
    }
    
    // special case redir_tbl[0] for pin 0 as ExtInt for Virtual Wire Mode
    // ioapic->redir_tbl[0].del_mode=EXTINT;
    // ioapic->redir_tbl[0].mask=0;
}


static int ioapic_read(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
    struct io_apic_state * ioapic = (struct io_apic_state *)(priv_data);
    uint32_t reg_tgt = guest_addr - ioapic->base_addr;
    uint32_t * op_val = (uint32_t *)dst;

    PrintDebug(core->vm_info, core, "ioapic %u: IOAPIC Read at %p\n", ioapic->ioapic_id.id, (void *)guest_addr);

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
	    default: {
		uint_t redir_index = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) >> 1;
		uint_t hi_val = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) & 1;
		
		if (redir_index > 23) {
		    PrintError(core->vm_info, core, "ioapic %u: Invalid redirection table entry %x\n", ioapic->ioapic_id.id, (uint32_t)redir_index);
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

    PrintDebug(core->vm_info, core, "ioapic %u: IOAPIC Read at %p gave value 0x%x\n", ioapic->ioapic_id.id, (void *)guest_addr, *op_val);

    return length;
}


static int ioapic_write(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data) {
    struct io_apic_state * ioapic = (struct io_apic_state *)(priv_data);
    uint32_t reg_tgt = guest_addr - ioapic->base_addr;
    uint32_t op_val = *(uint32_t *)src;

    PrintDebug(core->vm_info, core, "ioapic %u: IOAPIC Write at %p (val = %d)\n",  ioapic->ioapic_id.id, (void *)guest_addr, *(uint32_t *)src);

    if (reg_tgt == 0x00) {
	PrintDebug(core->vm_info, core, "ioapic %u: Setting ioapic index register to 0x%x.\n", ioapic->ioapic_id.id, op_val);
	ioapic->index_reg = op_val;
    } else if (reg_tgt == 0x10) {
	// IOWIN register
	switch (ioapic->index_reg) {
	    case IOAPIC_ID_REG:
		// What does this do to our relationship with the ICC bus?
		ioapic->ioapic_id.val = op_val;
		break;
	    case IOAPIC_VER_REG:
		// GPF/PageFault/Ignore?
		PrintError(core->vm_info, core, "ioapic %u: Writing to read only IOAPIC register\n", ioapic->ioapic_id.id);
		return -1;
	    case IOAPIC_ARB_REG:
		ioapic->ioapic_arb_id.val = op_val;
		break;
	    default:
		{
		    uint_t redir_index = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) >> 1;
		    uint_t hi_val = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) & 1;

		    PrintDebug(core->vm_info, core, "ioapic %u: Writing value 0x%x to redirection entry %u (%s)\n",
			       ioapic->ioapic_id.id, op_val, redir_index, hi_val ? "hi" : "low");

		    if (redir_index > 23) {
			PrintError(core->vm_info, core, "ioapic %u: Invalid redirection table entry %x\n", ioapic->ioapic_id.id, (uint32_t)redir_index);
			return -1;
		    }
		    if (hi_val) {
			PrintDebug(core->vm_info, core, "ioapic %u: Writing to hi of pin %d\n", ioapic->ioapic_id.id, redir_index);
			ioapic->redir_tbl[redir_index].hi = op_val;
		    } else {
			PrintDebug(core->vm_info, core, "ioapic %u: Writing to lo of pin %d\n", ioapic->ioapic_id.id, redir_index);
			op_val &= REDIR_LO_MASK;
			ioapic->redir_tbl[redir_index].lo &= ~REDIR_LO_MASK;
			ioapic->redir_tbl[redir_index].lo |= op_val;
		    }
		}
	}
    }

    return length;
}


static int ioapic_raise_irq(struct v3_vm_info * vm, void * private_data, struct v3_irq * irq) {
    struct io_apic_state * ioapic = (struct io_apic_state *)(private_data);  
    struct redir_tbl_entry * irq_entry = NULL;
    uint8_t irq_num = irq->irq;

    if (irq_num == 0) { 
      // IRQ 0 being raised, in the Palacios context, means the PIT
      // However, the convention is that it is the PIC that is connected
      // to PIN 0 of the IOAPIC and the PIT is connected to pin 2
      // Hence we convert this to the relvant pin.  In the future,
      // the PIC may signal to the IOAPIC in a different path.
      // Yes, this is kind of hideous, but it is needed to have the
      // PIT correctly show up via the IOAPIC
      irq_num = 2;
    }

    if (irq_num >= 24) {
	PrintDebug(vm, VCORE_NONE, "ioapic %u: IRQ out of range of IO APIC\n", ioapic->ioapic_id.id);
	return -1;
    }

    irq_entry = &(ioapic->redir_tbl[irq_num]);

    if (irq_entry->mask == 0) {
	struct v3_gen_ipi ipi;

	PrintDebug(vm, VCORE_NONE, "ioapic %u: IOAPIC Signaling APIC to raise INTR %d\n", 
		   ioapic->ioapic_id.id, irq_entry->vec);


	ipi.vector = irq_entry->vec;
	ipi.mode = irq_entry->del_mode;
	ipi.logical = irq_entry->dst_mode;
	ipi.trigger_mode = irq_entry->trig_mode;
	ipi.dst = irq_entry->dst_field;
	ipi.dst_shorthand = 0;

	ipi.ack = irq->ack;
	ipi.private_data = irq->private_data;

	PrintDebug(vm, VCORE_NONE, "ioapic %u: IPI: vector 0x%x, mode 0x%x, logical 0x%x, trigger 0x%x, dst 0x%x, shorthand 0x%x\n",
		   ioapic->ioapic_id.id, ipi.vector, ipi.mode, ipi.logical, ipi.trigger_mode, ipi.dst, ipi.dst_shorthand);
	// Need to add destination argument here...
	if (v3_apic_send_ipi(vm, &ipi, ioapic->apic_dev_data) == -1) {
	    PrintError(vm, VCORE_NONE, "Error sending IPI to apic %d\n", ipi.dst);
	    return -1;
	}
    }

    return 0;
}

/* I don't know if we can do anything here.... */
static int ioapic_lower_irq(struct v3_vm_info * vm, void * private_data, struct v3_irq * irq) {
    return 0;
}

static struct intr_router_ops router_ops = {
    .raise_intr = ioapic_raise_irq,
    .lower_intr = ioapic_lower_irq, 
};




static int io_apic_free(struct io_apic_state * ioapic) {

    v3_remove_intr_router(ioapic->vm, ioapic->router_handle);

    // unhook memory

    V3_Free(ioapic);

    return 0;
}

#ifdef V3_CONFIG_CHECKPOINT
static int io_apic_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct io_apic_state * io_apic = (struct io_apic_state *)private_data;


    V3_CHKPT_SAVE(ctx, "BASE_ADDR" ,io_apic->base_addr,savefailout);
    V3_CHKPT_SAVE(ctx, "INDEX_REG", io_apic->index_reg,savefailout);
    V3_CHKPT_SAVE(ctx, "IOAPIC_ID", io_apic->ioapic_id,savefailout);
    V3_CHKPT_SAVE(ctx, "IOAPIC_VER", io_apic->ioapic_ver,savefailout);
    V3_CHKPT_SAVE(ctx, "IOAPIC_ARB_ID", io_apic->ioapic_arb_id,savefailout);
    V3_CHKPT_SAVE(ctx, "REDIR_TABLE", io_apic->redir_tbl,savefailout);

    return 0;

 savefailout:
    PrintError(VM_NONE, VCORE_NONE, "ioapic save failed\n");
    return -1;
}

static int io_apic_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct io_apic_state * io_apic = (struct io_apic_state *)private_data;

    V3_CHKPT_LOAD(ctx, "BASE_ADDR", io_apic->base_addr,loadfailout);
    V3_CHKPT_LOAD(ctx, "INDEX_REG", io_apic->index_reg,loadfailout);
    V3_CHKPT_LOAD(ctx, "IOAPIC_ID", io_apic->ioapic_id,loadfailout);
    V3_CHKPT_LOAD(ctx, "IOAPIC_VER", io_apic->ioapic_ver,loadfailout);
    V3_CHKPT_LOAD(ctx, "IOAPIC_ARB_ID", io_apic->ioapic_arb_id,loadfailout);
    V3_CHKPT_LOAD(ctx, "REDIR_TABLE", io_apic->redir_tbl,loadfailout);

    return 0;

 loadfailout:
    PrintError(VM_NONE, VCORE_NONE, "ioapic load failed\n");
    return -1;
    
}
#endif



static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))io_apic_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = io_apic_save, 
    .load = io_apic_load
#endif
};



static int ioapic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * apic_dev = v3_find_dev(vm, v3_cfg_val(cfg, "apic"));
    char * dev_id = v3_cfg_val(cfg, "ID");


    PrintDebug(vm, VCORE_NONE, "ioapic: Creating IO APIC\n");

    struct io_apic_state * ioapic = (struct io_apic_state *)V3_Malloc(sizeof(struct io_apic_state));

    if (!ioapic) {
	PrintError(vm, VCORE_NONE, "Cannot allocate in init\n");
	return -1;
    }

    memset(ioapic,0,sizeof(struct io_apic_state));

    ioapic->apic_dev_data = apic_dev;

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, ioapic);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE,  "ioapic: Could not attach device %s\n", dev_id);
	V3_Free(ioapic);
	return -1;
    }

    ioapic->router_handle = v3_register_intr_router(vm, &router_ops, ioapic);
    ioapic->vm = vm;

    init_ioapic_state(ioapic, vm->num_cores);

    v3_hook_full_mem(vm, V3_MEM_CORE_ANY, ioapic->base_addr, ioapic->base_addr + PAGE_SIZE_4KB, 
		     ioapic_read, ioapic_write, ioapic);
  
    return 0;
}


device_register("IOAPIC", ioapic_init)
