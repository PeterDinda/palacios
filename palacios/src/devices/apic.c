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
 * Authors: Jack Lange <jarusl@cs.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (SMP)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <devices/apic.h>
#include <devices/apic_regs.h>
#include <palacios/vmm.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_types.h>


#include <palacios/vmm_queue.h>
#include <palacios/vmm_lock.h>

/* The locking in this file is nasty.
 * There are 3 different locking approaches that are taken, depending on the APIC operation
 * 1. Queue locks. Actual irq insertions are done via queueing irq ops at the dest apic. 
 *    The destination apic's core is responsible for draining the queue, and actually 
 *    setting the vector table. 
 * 2. State lock. This is a standard lock taken when internal apic state is read/written. 
 *    When an irq's destination is determined this lock is taken to examine the apic's 
 *    addressability. 
 * 3. VM barrier lock. This is taken when actual VM core state is changed (via SIPI). 
 */



#ifndef V3_CONFIG_DEBUG_APIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#else

static char * shorthand_str[] = { 
    "(no shorthand)",
    "(self)",
    "(all)",
    "(all-but-me)",
};

static char * deliverymode_str[] = { 
    "(fixed)",
    "(lowest priority)",
    "(SMI)",
    "(reserved)",
    "(NMI)",
    "(INIT)",
    "(Start Up)",
    "(ExtInt)",
};

#endif

typedef enum { APIC_TMR_INT, APIC_THERM_INT, APIC_PERF_INT, 
	       APIC_LINT0_INT, APIC_LINT1_INT, APIC_ERR_INT } apic_irq_type_t;


#define APIC_SHORTHAND_NONE        0x0
#define APIC_SHORTHAND_SELF        0x1
#define APIC_SHORTHAND_ALL         0x2
#define APIC_SHORTHAND_ALL_BUT_ME  0x3

#define APIC_DEST_PHYSICAL    0x0
#define APIC_DEST_LOGICAL     0x1


#define BASE_ADDR_MSR     0x0000001B
#define DEFAULT_BASE_ADDR 0xfee00000

#define APIC_ID_OFFSET                    0x020
#define APIC_VERSION_OFFSET               0x030
#define TPR_OFFSET                        0x080
#define APR_OFFSET                        0x090
#define PPR_OFFSET                        0x0a0
#define EOI_OFFSET                        0x0b0
#define REMOTE_READ_OFFSET                0x0c0
#define LDR_OFFSET                        0x0d0
#define DFR_OFFSET                        0x0e0
#define SPURIOUS_INT_VEC_OFFSET           0x0f0

#define ISR_OFFSET0                       0x100   // 0x100 - 0x170
#define ISR_OFFSET1                       0x110   // 0x100 - 0x170
#define ISR_OFFSET2                       0x120   // 0x100 - 0x170
#define ISR_OFFSET3                       0x130   // 0x100 - 0x170
#define ISR_OFFSET4                       0x140   // 0x100 - 0x170
#define ISR_OFFSET5                       0x150   // 0x100 - 0x170
#define ISR_OFFSET6                       0x160   // 0x100 - 0x170
#define ISR_OFFSET7                       0x170   // 0x100 - 0x170

#define TRIG_OFFSET0                      0x180   // 0x180 - 0x1f0
#define TRIG_OFFSET1                      0x190   // 0x180 - 0x1f0
#define TRIG_OFFSET2                      0x1a0   // 0x180 - 0x1f0
#define TRIG_OFFSET3                      0x1b0   // 0x180 - 0x1f0
#define TRIG_OFFSET4                      0x1c0   // 0x180 - 0x1f0
#define TRIG_OFFSET5                      0x1d0   // 0x180 - 0x1f0
#define TRIG_OFFSET6                      0x1e0   // 0x180 - 0x1f0
#define TRIG_OFFSET7                      0x1f0   // 0x180 - 0x1f0


#define IRR_OFFSET0                       0x200   // 0x200 - 0x270
#define IRR_OFFSET1                       0x210   // 0x200 - 0x270
#define IRR_OFFSET2                       0x220   // 0x200 - 0x270
#define IRR_OFFSET3                       0x230   // 0x200 - 0x270
#define IRR_OFFSET4                       0x240   // 0x200 - 0x270
#define IRR_OFFSET5                       0x250   // 0x200 - 0x270
#define IRR_OFFSET6                       0x260   // 0x200 - 0x270
#define IRR_OFFSET7                       0x270   // 0x200 - 0x270


#define ESR_OFFSET                        0x280
#define INT_CMD_LO_OFFSET                 0x300
#define INT_CMD_HI_OFFSET                 0x310
#define TMR_LOC_VEC_TBL_OFFSET            0x320
#define THERM_LOC_VEC_TBL_OFFSET          0x330
#define PERF_CTR_LOC_VEC_TBL_OFFSET       0x340
#define LINT0_VEC_TBL_OFFSET              0x350
#define LINT1_VEC_TBL_OFFSET              0x360
#define ERR_VEC_TBL_OFFSET                0x370
#define TMR_INIT_CNT_OFFSET               0x380
#define TMR_CUR_CNT_OFFSET                0x390
#define TMR_DIV_CFG_OFFSET                0x3e0
#define EXT_APIC_FEATURE_OFFSET           0x400
#define EXT_APIC_CMD_OFFSET               0x410
#define SEOI_OFFSET                       0x420

#define IER_OFFSET0                       0x480   // 0x480 - 0x4f0
#define IER_OFFSET1                       0x490   // 0x480 - 0x4f0
#define IER_OFFSET2                       0x4a0   // 0x480 - 0x4f0
#define IER_OFFSET3                       0x4b0   // 0x480 - 0x4f0
#define IER_OFFSET4                       0x4c0   // 0x480 - 0x4f0
#define IER_OFFSET5                       0x4d0   // 0x480 - 0x4f0
#define IER_OFFSET6                       0x4e0   // 0x480 - 0x4f0
#define IER_OFFSET7                       0x4f0   // 0x480 - 0x4f0

#define EXT_INT_LOC_VEC_TBL_OFFSET0       0x500   // 0x500 - 0x530
#define EXT_INT_LOC_VEC_TBL_OFFSET1       0x510   // 0x500 - 0x530
#define EXT_INT_LOC_VEC_TBL_OFFSET2       0x520   // 0x500 - 0x530
#define EXT_INT_LOC_VEC_TBL_OFFSET3       0x530   // 0x500 - 0x530

struct apic_msr {
    union {
	uint64_t value;
	struct {
	    uint8_t rsvd;
	    uint8_t bootstrap_cpu : 1;
	    uint8_t rsvd2         : 2;
	    uint8_t apic_enable   : 1;
	    uint64_t base_addr    : 40;
	    uint32_t rsvd3        : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));





struct irq_queue_entry {
    uint32_t vector;
    int (*ack)(struct guest_info * core, uint32_t irq, void * private_data);
    void * private_data;

    struct list_head list_node;
};




typedef enum {INIT_ST, 
	      SIPI, 
	      STARTED} ipi_state_t; 

struct apic_dev_state;

struct apic_state {
    addr_t base_addr;

    /* MSRs */
    struct apic_msr base_addr_msr;


    /* memory map registers */

    struct lapic_id_reg lapic_id;
    struct apic_ver_reg apic_ver;
    struct ext_apic_ctrl_reg ext_apic_ctrl;
    struct local_vec_tbl_reg local_vec_tbl;
    struct tmr_vec_tbl_reg tmr_vec_tbl;
    struct tmr_div_cfg_reg tmr_div_cfg;
    struct lint_vec_tbl_reg lint0_vec_tbl;
    struct lint_vec_tbl_reg lint1_vec_tbl;
    struct perf_ctr_loc_vec_tbl_reg perf_ctr_loc_vec_tbl;
    struct therm_loc_vec_tbl_reg therm_loc_vec_tbl;
    struct err_vec_tbl_reg err_vec_tbl;
    struct err_status_reg err_status;
    struct spurious_int_reg spurious_int;
    struct int_cmd_reg int_cmd;
    struct log_dst_reg log_dst;
    struct dst_fmt_reg dst_fmt;
    //struct arb_prio_reg arb_prio;     // computed on the fly
    //struct task_prio_reg task_prio;   // stored in core.ctrl_regs.apic_tpr
    //struct proc_prio_reg proc_prio;   // computed on the fly
    struct ext_apic_feature_reg ext_apic_feature;
    struct spec_eoi_reg spec_eoi;
  

    uint32_t tmr_cur_cnt;
    uint32_t tmr_init_cnt;
    uint32_t missed_ints;

    struct local_vec_tbl_reg ext_intr_vec_tbl[4];

    uint32_t rem_rd_data;


    ipi_state_t ipi_state;

    uint8_t int_req_reg[32];
    uint8_t int_svc_reg[32];
    uint8_t int_en_reg[32];
    uint8_t trig_mode_reg[32];

    struct {
	int (*ack)(struct guest_info * core, uint32_t irq, void * private_data);
	void * private_data;
    } irq_ack_cbs[256];

    struct guest_info * core;

    void * controller_handle;

    struct v3_timer * timer;


    struct {
	v3_lock_t lock;
	
	uint64_t num_entries;
	struct list_head entries;
    } irq_queue ;

    uint32_t eoi;


};




struct apic_dev_state {
    int num_apics;
  
    v3_lock_t state_lock;

    struct apic_state apics[0];
} __attribute__((packed));





static int apic_read(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data);
static int apic_write(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data);

static void set_apic_tpr(struct apic_state *apic, uint32_t val);

static int is_apic_bsp(struct apic_state * apic) {
    return ((apic->base_addr_msr.value & 0x0000000000000100LL) != 0);
}




// No locking done
static void init_apic_state(struct apic_state * apic, uint32_t id) {
    apic->base_addr = DEFAULT_BASE_ADDR;

    if (id == 0) { 
	// boot processor, enabled
	apic->base_addr_msr.value = 0x0000000000000900LL;
    } else {
	// ap processor, enabled
	apic->base_addr_msr.value = 0x0000000000000800LL;
    }

    // same base address regardless of ap or main
    apic->base_addr_msr.value |= ((uint64_t)DEFAULT_BASE_ADDR); 

    PrintDebug(VM_NONE, VCORE_NONE, "apic %u: (init_apic_state): msr=0x%llx\n",id, apic->base_addr_msr.value);

    PrintDebug(VM_NONE, VCORE_NONE, "apic %u: (init_apic_state): Sizeof Interrupt Request Register %d, should be 32\n",
	       id, (uint_t)sizeof(apic->int_req_reg));

    memset(apic->int_req_reg, 0, sizeof(apic->int_req_reg));
    memset(apic->int_svc_reg, 0, sizeof(apic->int_svc_reg));
    memset(apic->int_en_reg, 0xff, sizeof(apic->int_en_reg));
    memset(apic->trig_mode_reg, 0, sizeof(apic->trig_mode_reg));

    apic->eoi = 0x00000000;
    apic->rem_rd_data = 0x00000000;
    apic->tmr_init_cnt = 0x00000000;
    apic->tmr_cur_cnt = 0x00000000;
    apic->missed_ints = 0;

    // note that it's the *lower* 24 bits that are
    // reserved, not the upper 24.  
    apic->lapic_id.val = 0;
    apic->lapic_id.apic_id = id;
    
    apic->ipi_state = INIT_ST;

    // The P6 has 6 LVT entries, so we set the value to (6-1)...
    apic->apic_ver.val = 0x80050010;

    set_apic_tpr(apic,0x00000000);
    // note that arbitration priority and processor priority are derived values
    // and are computed on the fly

    apic->log_dst.val = 0x00000000;
    apic->dst_fmt.val = 0xffffffff;
    apic->spurious_int.val = 0x000000ff;
    apic->err_status.val = 0x00000000;
    apic->int_cmd.val = 0x0000000000000000LL;
    apic->tmr_vec_tbl.val = 0x00010000;
    apic->therm_loc_vec_tbl.val = 0x00010000;
    apic->perf_ctr_loc_vec_tbl.val = 0x00010000;
    apic->lint0_vec_tbl.val = 0x00010000;
    apic->lint1_vec_tbl.val = 0x00010000;
    apic->err_vec_tbl.val = 0x00010000;
    apic->tmr_div_cfg.val = 0x00000000;
    //apic->ext_apic_feature.val = 0x00000007;
    apic->ext_apic_feature.val = 0x00040007;
    apic->ext_apic_ctrl.val = 0x00000000;
    apic->spec_eoi.val = 0x00000000;


    INIT_LIST_HEAD(&(apic->irq_queue.entries));
    v3_lock_init(&(apic->irq_queue.lock));
    apic->irq_queue.num_entries = 0;

}





static int read_apic_msr(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)priv_data;
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]);

    PrintDebug(core->vm_info, core, "apic %u: core %u: MSR read getting %llx\n", apic->lapic_id.val, core->vcpu_id, apic->base_addr_msr.value);
 
    dst->value = apic->base_addr_msr.value;
 
    return 0;
}


static int write_apic_msr(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)priv_data;
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]);
    struct v3_mem_region * old_reg = v3_get_mem_region(core->vm_info, core->vcpu_id, apic->base_addr);


    PrintDebug(core->vm_info, core, "apic %u: core %u: MSR write of %llx\n", apic->lapic_id.val, core->vcpu_id, src.value);

    if (old_reg == NULL) {
	// uh oh...
	PrintError(core->vm_info, core, "apic %u: core %u: APIC Base address region does not exit...\n",
		   apic->lapic_id.val, core->vcpu_id);
	return -1;
    }
    


    v3_delete_mem_region(core->vm_info, old_reg);

    apic->base_addr_msr.value = src.value;

    apic->base_addr = src.value & ~0xfffULL;

    if (v3_hook_full_mem(core->vm_info, core->vcpu_id, apic->base_addr, 
			 apic->base_addr + PAGE_SIZE_4KB, 
			 apic_read, apic_write, apic_dev) == -1) {
	PrintError(core->vm_info, core, "apic %u: core %u: Could not hook new APIC Base address\n",
		   apic->lapic_id.val, core->vcpu_id);

	return -1;
    }


    return 0;
}





// irq_num is the bit offset into a 256 bit buffer...
static int activate_apic_irq(struct apic_state * apic, uint32_t irq_num, 
			     int (*ack)(struct guest_info * core, uint32_t irq, void * private_data), 
			     void * private_data) {
    int major_offset = (irq_num & ~0x00000007) >> 3;
    int minor_offset = irq_num & 0x00000007;
    uint8_t * req_location = apic->int_req_reg + major_offset;
    uint8_t * en_location = apic->int_en_reg + major_offset;
    uint8_t flag = 0x1 << minor_offset;


    PrintDebug(VM_NONE, VCORE_NONE, "apic %u: core %d: Raising APIC IRQ %d\n", apic->lapic_id.val, apic->core->vcpu_id, irq_num);

    if (*req_location & flag) {
	PrintDebug(VM_NONE, VCORE_NONE, "Interrupt %d  coallescing\n", irq_num);
	return 0;
    }

    if (*en_location & flag) {
	*req_location |= flag;
	apic->irq_ack_cbs[irq_num].ack = ack;
	apic->irq_ack_cbs[irq_num].private_data = private_data;

	return 1;
    } else {
	PrintDebug(VM_NONE, VCORE_NONE, "apic %u: core %d: Interrupt  not enabled... %.2x\n", 
		   apic->lapic_id.val, apic->core->vcpu_id, *en_location);
    }

    return 0;
}



static int add_apic_irq_entry(struct apic_state * apic, uint32_t irq_num, 
			      int (*ack)(struct guest_info * core, uint32_t irq, void * private_data),
			      void * private_data) {
    unsigned int flags = 0;
    struct irq_queue_entry * entry = NULL;

    if (irq_num <= 15) {
	PrintError(VM_NONE, VCORE_NONE, "core %d: Attempting to raise an invalid interrupt: %d\n", 
		    apic->core->vcpu_id, irq_num);
	return -1;
    }

    entry = V3_Malloc(sizeof(struct irq_queue_entry));

    if (entry == NULL) {
	PrintError(VM_NONE, VCORE_NONE, "Could not allocate irq queue entry\n");
	return -1;
    }

    entry->vector = irq_num;
    entry->ack = ack;
    entry->private_data = private_data;

    flags = v3_lock_irqsave(apic->irq_queue.lock);
    
    list_add_tail(&(entry->list_node), &(apic->irq_queue.entries));
    apic->irq_queue.num_entries++;

    v3_unlock_irqrestore(apic->irq_queue.lock, flags);
  

    return 0;
}

static void drain_irq_entries(struct apic_state * apic) {

    while (1) {
	unsigned int flags = 0;
	struct irq_queue_entry * entry = NULL;
    
	flags = v3_lock_irqsave(apic->irq_queue.lock);
	
	if (!list_empty(&(apic->irq_queue.entries))) {
	    struct list_head * q_entry = apic->irq_queue.entries.next;
	    entry = list_entry(q_entry, struct irq_queue_entry, list_node);

	    apic->irq_queue.num_entries--;
	    list_del(q_entry);
	}
	
	v3_unlock_irqrestore(apic->irq_queue.lock, flags);

	if (entry == NULL) {
	    break;
	}

	activate_apic_irq(apic, entry->vector, entry->ack, entry->private_data);

	V3_Free(entry);
    }

}



static int get_highest_isr(struct apic_state * apic) {
    int i = 0, j = 0;

    // We iterate backwards to find the highest priority in-service request
    for (i = 31; i >= 0; i--) {
	uint8_t  * svc_major = apic->int_svc_reg + i;
    
	if ((*svc_major) & 0xff) {
	    for (j = 7; j >= 0; j--) {
		uint8_t flag = 0x1 << j;
		if ((*svc_major) & flag) {
		    return ((i * 8) + j);
		}
	    }
	}
    }

    return -1;
}
 


static int get_highest_irr(struct apic_state * apic) {
    int i = 0, j = 0;

    // We iterate backwards to find the highest priority enabled requested interrupt
    for (i = 31; i >= 0; i--) {
	uint8_t  * req_major = apic->int_req_reg + i;
	uint8_t  * en_major = apic->int_en_reg + i;
	
	if ((*req_major) & 0xff) {
	    for (j = 7; j >= 0; j--) {
		uint8_t flag = 0x1 << j;
		if ((*req_major & *en_major) & flag) {
		    return ((i * 8) + j);
		}
	    }
	}
    }

    return -1;
}
 

static uint32_t get_isrv(struct apic_state *apic)
{
    int isr = get_highest_isr(apic);
    
    if (isr>=0) { 
	return (uint32_t) isr;
    } else {
	return 0;
    }
}

static uint32_t get_irrv(struct apic_state *apic)
{
    int irr = get_highest_irr(apic);
    
    if (irr>=0) { 
	return (uint32_t) irr;
    } else {
	return 0;
    }
}


static uint32_t get_apic_tpr(struct apic_state *apic)
{
    return (uint32_t) (apic->core->ctrl_regs.apic_tpr); // see comment in vmm_ctrl_regs.c for how this works

}

static void set_apic_tpr(struct apic_state *apic, uint32_t val)
{
    PrintDebug(VM_NONE, VCORE_NONE, "Set apic_tpr to 0x%x from apic reg path\n",val);
    apic->core->ctrl_regs.apic_tpr = (uint64_t) val; // see comment in vmm_ctrl_regs.c for how this works
}

static uint32_t get_apic_ppr(struct apic_state *apic)
{
    uint32_t tpr = get_apic_tpr(apic);
    uint32_t isrv = get_isrv(apic);
    uint32_t tprlevel, isrlevel;
    uint32_t ppr;

    tprlevel = (tpr >> 4) & 0xf;
    isrlevel = (isrv >> 4) & 0xf;

    if (tprlevel>=isrlevel) { 
	ppr = tpr;  // get class and subclass
    } else { 
	ppr = (isrlevel << 4); // get class only
    }
    
    return ppr;
}



static uint32_t get_apic_apr(struct apic_state *apic)
{
    uint32_t tpr = get_apic_tpr(apic);
    uint32_t isrv = get_isrv(apic);
    uint32_t irrv = get_irrv(apic);
    uint32_t tprlevel, isrlevel, irrlevel;

    tprlevel = (tpr >> 4) & 0xf;
    isrlevel = (isrv >> 4) & 0xf;
    irrlevel = (irrv >> 4) & 0xf;

    if (tprlevel >= isrlevel) { 
	if (tprlevel >= irrlevel) { 
	    return tpr;  // get both class and subclass
	} else {
	    return irrlevel << 4; // get class only
	}
    } else {
	if (isrlevel >= irrlevel) {
	    return isrlevel << 4; // get class only
	} else {
	    return irrlevel << 4; // get class only
	}
    }
    
}


static int apic_do_eoi(struct guest_info * core, struct apic_state * apic) {
    int isr_irq = get_highest_isr(apic);

    if (isr_irq != -1) {
	int major_offset = (isr_irq & ~0x00000007) >> 3;
	int minor_offset = isr_irq & 0x00000007;
	uint8_t flag = 0x1 << minor_offset;
	uint8_t * svc_location = apic->int_svc_reg + major_offset;
	
	PrintDebug(core->vm_info, core, "apic %u: core ?: Received APIC EOI for IRQ %d\n", apic->lapic_id.val,isr_irq);
	
	*svc_location &= ~flag;

	if (apic->irq_ack_cbs[isr_irq].ack) {
	    apic->irq_ack_cbs[isr_irq].ack(core, isr_irq, apic->irq_ack_cbs[isr_irq].private_data);
	}

#ifdef V3_CONFIG_CRAY_XT
	
	if ((isr_irq == 238) || 
	    (isr_irq == 239)) {
	    PrintDebug(core->vm_info, core, "apic %u: core ?: Acking IRQ %d\n", apic->lapic_id.val,isr_irq);
	}
	
	if (isr_irq == 238) {
	    V3_ACK_IRQ(238);
	}
#endif
    } else {
	//PrintError(core->vm_info, core, "apic %u: core ?: Spurious EOI...\n",apic->lapic_id.val);
    }
	
    return 0;
}
 

static int activate_internal_irq(struct apic_state * apic, apic_irq_type_t int_type) {
    uint32_t vec_num = 0;
    uint32_t del_mode = 0;
    int masked = 0;


    switch (int_type) {
	case APIC_TMR_INT:
	    vec_num = apic->tmr_vec_tbl.vec;
	    del_mode = IPI_FIXED;
	    masked = apic->tmr_vec_tbl.mask;
	    break;
	case APIC_THERM_INT:
	    vec_num = apic->therm_loc_vec_tbl.vec;
	    del_mode = apic->therm_loc_vec_tbl.msg_type;
	    masked = apic->therm_loc_vec_tbl.mask;
	    break;
	case APIC_PERF_INT:
	    vec_num = apic->perf_ctr_loc_vec_tbl.vec;
	    del_mode = apic->perf_ctr_loc_vec_tbl.msg_type;
	    masked = apic->perf_ctr_loc_vec_tbl.mask;
	    break;
	case APIC_LINT0_INT:
	    vec_num = apic->lint0_vec_tbl.vec;
	    del_mode = apic->lint0_vec_tbl.msg_type;
	    masked = apic->lint0_vec_tbl.mask;
	    break;
	case APIC_LINT1_INT:
	    vec_num = apic->lint1_vec_tbl.vec;
	    del_mode = apic->lint1_vec_tbl.msg_type;
	    masked = apic->lint1_vec_tbl.mask;
	    break;
	case APIC_ERR_INT:
	    vec_num = apic->err_vec_tbl.vec;
	    del_mode = IPI_FIXED;
	    masked = apic->err_vec_tbl.mask;
	    break;
	default:
	    PrintError(VM_NONE, VCORE_NONE, "apic %u: core ?: Invalid APIC interrupt type\n", apic->lapic_id.val);
	    return -1;
    }

    // interrupt is masked, don't send
    if (masked == 1) {
	PrintDebug(VM_NONE, VCORE_NONE, "apic %u: core ?: Inerrupt is masked\n", apic->lapic_id.val);
	return 0;
    }

    if (del_mode == IPI_FIXED) {
	//PrintDebug(VM_NONE, VCORE_NONE, "Activating internal APIC IRQ %d\n", vec_num);
	return add_apic_irq_entry(apic, vec_num, NULL, NULL);
    } else {
	PrintError(VM_NONE, VCORE_NONE, "apic %u: core ?: Unhandled Delivery Mode\n", apic->lapic_id.val);
	return -1;
    }
}



static inline int should_deliver_cluster_ipi(struct apic_dev_state * apic_dev,
					     struct guest_info * dst_core, 
					     struct apic_state * dst_apic, uint8_t mda) {

    int ret = 0;


    if 	( ((mda & 0xf0) == (dst_apic->log_dst.dst_log_id & 0xf0)) &&  /* (I am in the cluster and */
	  ((mda & 0x0f) & (dst_apic->log_dst.dst_log_id & 0x0f)) ) {  /*  I am in the set)        */
	ret = 1;
    } else {
	ret = 0;
    }


    if (ret == 1) {
	PrintDebug(VM_NONE, VCORE_NONE, "apic %u core %u: accepting clustered IRQ (mda 0x%x == log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->vcpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
    } else {
	PrintDebug(VM_NONE, VCORE_NONE, "apic %u core %u: rejecting clustered IRQ (mda 0x%x != log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->vcpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
    }

    return ret;

}

static inline int should_deliver_flat_ipi(struct apic_dev_state * apic_dev,
					  struct guest_info * dst_core,
					  struct apic_state * dst_apic, uint8_t mda) {

    int ret = 0;


    if ((dst_apic->log_dst.dst_log_id & mda) != 0) {  // I am in the set 
	ret = 1;
    } else {
	ret = 0;
    }


    if (ret == 1) {
	PrintDebug(VM_NONE, VCORE_NONE, "apic %u core %u: accepting flat IRQ (mda 0x%x == log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->vcpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
    } else {
	PrintDebug(VM_NONE, VCORE_NONE, "apic %u core %u: rejecting flat IRQ (mda 0x%x != log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->vcpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
    }


    return ret;
}



static int should_deliver_ipi(struct apic_dev_state * apic_dev, 
			      struct guest_info * dst_core, 
			      struct apic_state * dst_apic, uint8_t mda) {
    addr_t flags = 0;
    int ret = 0;

    flags = v3_lock_irqsave(apic_dev->state_lock);

    if (dst_apic->dst_fmt.model == 0xf) {

	if (mda == 0xff) {
	    /* always deliver broadcast */
	    ret = 1;
	} else {
	    ret = should_deliver_flat_ipi(apic_dev, dst_core, dst_apic, mda);
	}
    } else if (dst_apic->dst_fmt.model == 0x0) {

	if (mda == 0xff) {
	    /*  always deliver broadcast */
	    ret = 1;
	} else {
	    ret = should_deliver_cluster_ipi(apic_dev, dst_core, dst_apic, mda);
	}

    } else {
	ret = -1;
    }
    
    v3_unlock_irqrestore(apic_dev->state_lock, flags);


    if (ret == -1) {
	PrintError(VM_NONE, VCORE_NONE, "apic %u core %u: invalid destination format register value 0x%x for logical mode delivery.\n", 
		   dst_apic->lapic_id.val, dst_core->vcpu_id, dst_apic->dst_fmt.model);
    }

    return ret;
}




// Only the src_apic pointer is used
static int deliver_ipi(struct apic_state * src_apic, 
		       struct apic_state * dst_apic, 
		       struct v3_gen_ipi * ipi) {


    struct guest_info * dst_core = dst_apic->core;


    switch (ipi->mode) {

	case IPI_FIXED:  
	case IPI_LOWEST_PRIO: {
	    // lowest priority - 
	    // caller needs to have decided which apic to deliver to!

	    PrintDebug(VM_NONE, VCORE_NONE, "delivering IRQ %d to core %u\n", ipi->vector, dst_core->vcpu_id); 

	    add_apic_irq_entry(dst_apic, ipi->vector, ipi->ack, ipi->private_data);
	    
	    if (dst_apic != src_apic) { 
		PrintDebug(VM_NONE, VCORE_NONE, " non-local core with new interrupt, forcing it to exit now\n"); 
		v3_interrupt_cpu(dst_core->vm_info, dst_core->pcpu_id, 0);
	    }

	    break;
	}
	case IPI_INIT: { 

	    PrintDebug(VM_NONE, VCORE_NONE, " INIT delivery to core %u\n", dst_core->vcpu_id);

	    if (is_apic_bsp(dst_apic)) {
		PrintError(VM_NONE, VCORE_NONE, "Attempted to INIT BSP CPU. Ignoring since I have no idea what the hell to do...\n");
		break;
	    }


	    if (dst_apic->ipi_state != INIT_ST) { 
		v3_raise_barrier(dst_core->vm_info, src_apic->core);
		dst_core->core_run_state = CORE_STOPPED;
		dst_apic->ipi_state = INIT_ST;
		v3_lower_barrier(dst_core->vm_info);

	    }

	    // We transition the target core to SIPI state
	    dst_apic->ipi_state = SIPI;  // note: locking should not be needed here

	    // That should be it since the target core should be
	    // waiting in host on this transition
	    // either it's on another core or on a different preemptive thread
	    // in both cases, it will quickly notice this transition 
	    // in particular, we should not need to force an exit here

	    PrintDebug(VM_NONE, VCORE_NONE, " INIT delivery done\n");

	    break;							
	}
	case IPI_SIPI: { 

	    // Sanity check
	    if (dst_apic->ipi_state != SIPI) { 
		PrintError(VM_NONE, VCORE_NONE, " core %u is not in SIPI state (mode = %d), ignored!\n",
			   dst_core->vcpu_id, dst_apic->ipi_state);
		break;
	    }

	    v3_reset_vm_core(dst_core, ipi->vector);

	    PrintDebug(VM_NONE, VCORE_NONE, " SIPI delivery (0x%x -> 0x%x:0x0) to core %u\n",
		       ipi->vector, dst_core->segments.cs.selector, dst_core->vcpu_id);
	    // Maybe need to adjust the APIC?
	    
	    // We transition the target core to SIPI state
	    dst_core->core_run_state = CORE_RUNNING;  // note: locking should not be needed here
	    dst_apic->ipi_state = STARTED;
	    
	    // As with INIT, we should not need to do anything else
	    
	    PrintDebug(VM_NONE, VCORE_NONE, " SIPI delivery done\n");
	    
	    break;							
	}

	case IPI_EXTINT: // EXTINT
	    /* Two possible things to do here: 
	     * 1. Ignore the IPI and assume the 8259a (PIC) will handle it
	     * 2. Add 32 to the vector and inject it...
	     * We probably just want to do 1 here, and assume the raise_irq() will hit the 8259a.
	     */
	    return 0;

	case IPI_SMI: 
	case IPI_RES1: // reserved						
	case IPI_NMI:
	default:
	    PrintError(VM_NONE, VCORE_NONE, "IPI %d delivery is unsupported\n", ipi->mode); 
	    return -1;
    }
    
    return 0;
    
}

static struct apic_state * find_physical_apic(struct apic_dev_state * apic_dev, uint32_t dst_idx) {
    struct apic_state * dst_apic = NULL;
    addr_t flags;
    int i;

    flags = v3_lock_irqsave(apic_dev->state_lock);

    if ( (dst_idx > 0) && (dst_idx < apic_dev->num_apics) ) { 
	// see if it simply is the core id
	if (apic_dev->apics[dst_idx].lapic_id.apic_id == dst_idx) { 
	     dst_apic = &(apic_dev->apics[dst_idx]);
	}
    }

    for (i = 0; i < apic_dev->num_apics; i++) { 
	if (apic_dev->apics[i].lapic_id.apic_id == dst_idx) { 
	    dst_apic =  &(apic_dev->apics[i]);
	}
    }

    v3_unlock_irqrestore(apic_dev->state_lock, flags);

    return dst_apic;

}


static int route_ipi(struct apic_dev_state * apic_dev,
		     struct apic_state * src_apic, 
		     struct v3_gen_ipi * ipi) {
    struct apic_state * dest_apic = NULL;


    PrintDebug(VM_NONE, VCORE_NONE, "apic: IPI %s %u from apic %p to %s %s %u\n",
	       deliverymode_str[ipi->mode], 
	       ipi->vector, 
	       src_apic, 	       
	       (ipi->logical == 0) ? "(physical)" : "(logical)", 
	       shorthand_str[ipi->dst_shorthand], 
	       ipi->dst);


    switch (ipi->dst_shorthand) {

	case APIC_SHORTHAND_NONE:  // no shorthand
	    if (ipi->logical == APIC_DEST_PHYSICAL) { 

		dest_apic = find_physical_apic(apic_dev, ipi->dst);
		
		if (dest_apic == NULL) { 
		    PrintError(VM_NONE, VCORE_NONE, "apic: Attempted send to unregistered apic id=%u\n", ipi->dst);
		    return -1;
		}

		if (deliver_ipi(src_apic, dest_apic, ipi) == -1) {
		    PrintError(VM_NONE, VCORE_NONE, "apic: Could not deliver IPI\n");
		    return -1;
		}


		PrintDebug(VM_NONE, VCORE_NONE, "apic: done\n");

	    } else if (ipi->logical == APIC_DEST_LOGICAL) {
		
		if (ipi->mode != IPI_LOWEST_PRIO) { 
		    int i;
		    uint8_t mda = ipi->dst;

		    // logical, but not lowest priority
		    // we immediately trigger
		    // fixed, smi, reserved, nmi, init, sipi, etc

		    
		    for (i = 0; i < apic_dev->num_apics; i++) { 
			int del_flag = 0;
			
			dest_apic = &(apic_dev->apics[i]);
			
			del_flag = should_deliver_ipi(apic_dev, dest_apic->core, dest_apic, mda);
			
			if (del_flag == -1) {

			    PrintError(VM_NONE, VCORE_NONE, "apic: Error checking delivery mode\n");
			    return -1;
			} else if (del_flag == 1) {

			    if (deliver_ipi(src_apic, dest_apic, ipi) == -1) {
				PrintError(VM_NONE, VCORE_NONE, "apic: Error: Could not deliver IPI\n");
				return -1;
			    }
			}
		    }
		} else {  // APIC_LOWEST_DELIVERY
		    struct apic_state * cur_best_apic = NULL;
		    uint32_t cur_best_apr;
		    uint8_t mda = ipi->dst;
		    int i;

		    // logical, lowest priority

		    for (i = 0; i < apic_dev->num_apics; i++) { 
			int del_flag = 0;

			dest_apic = &(apic_dev->apics[i]);
			
			del_flag = should_deliver_ipi(apic_dev, dest_apic->core, dest_apic, mda);
			
			if (del_flag == -1) {
			    PrintError(VM_NONE, VCORE_NONE, "apic: Error checking delivery mode\n");

			    return -1;
			} else if (del_flag == 1) {
			    // update priority for lowest priority scan
			    addr_t flags = 0;

			    flags = v3_lock_irqsave(apic_dev->state_lock);

			    if (cur_best_apic == 0) {
				cur_best_apic = dest_apic;  
				cur_best_apr = get_apic_apr(dest_apic) & 0xf0;
			    } else {
				uint32_t dest_apr = get_apic_apr(dest_apic) & 0xf0;
				if (dest_apr < cur_best_apr) {
				    cur_best_apic = dest_apic;
				    cur_best_apr = dest_apr;
				}
			    } 

			    v3_unlock_irqrestore(apic_dev->state_lock, flags);

			}
		    }

		    // now we will deliver to the best one if it exists
		    if (!cur_best_apic) { 
			PrintDebug(VM_NONE, VCORE_NONE, "apic: lowest priority deliver, but no destinations!\n");
		    } else {
			if (deliver_ipi(src_apic, cur_best_apic, ipi) == -1) {
			    PrintError(VM_NONE, VCORE_NONE, "apic: Error: Could not deliver IPI\n");
			    return -1;
			}
			//V3_Print(VM_NONE, VCORE_NONE, "apic: logical, lowest priority delivery to apic %u\n",cur_best_apic->lapic_id.val);
		    }
		}
	    }

	    break;
	    
	case APIC_SHORTHAND_SELF:  // self

	    if (src_apic == NULL) {    /* this is not an apic, but it's trying to send to itself??? */
		PrintError(VM_NONE, VCORE_NONE, "apic: Sending IPI to self from generic IPI sender\n");
		break;
	    }



	    if (ipi->logical == APIC_DEST_PHYSICAL)  {  /* physical delivery */
		if (deliver_ipi(src_apic, src_apic, ipi) == -1) {
		    PrintError(VM_NONE, VCORE_NONE, "apic: Could not deliver IPI to self (physical)\n");
		    return -1;
		}
	    } else if (ipi->logical == APIC_DEST_LOGICAL) {  /* logical delivery */
		PrintError(VM_NONE, VCORE_NONE, "apic: use of logical delivery in self (untested)\n");

		if (deliver_ipi(src_apic, src_apic, ipi) == -1) {
		    PrintError(VM_NONE, VCORE_NONE, "apic: Could not deliver IPI to self (logical)\n");
		    return -1;
		}
	    }

	    break;
	    
	case APIC_SHORTHAND_ALL: 
	case APIC_SHORTHAND_ALL_BUT_ME: { /* all and all-but-me */
	    /* assuming that logical verus physical doesn't matter
	       although it is odd that both are used */
	    int i;

	    for (i = 0; i < apic_dev->num_apics; i++) { 
		dest_apic = &(apic_dev->apics[i]);
		
		if ((dest_apic != src_apic) || (ipi->dst_shorthand == APIC_SHORTHAND_ALL)) { 
		    if (deliver_ipi(src_apic, dest_apic, ipi) == -1) {
			PrintError(VM_NONE, VCORE_NONE, "apic: Error: Could not deliver IPI\n");
			return -1;
		    }
		}
	    }

	    break;
	}
	default:
	    PrintError(VM_NONE, VCORE_NONE, "apic: Error routing IPI, invalid Mode (%d)\n", ipi->dst_shorthand);
	    return -1;
    }
 
    return 0;
}


// External function, expected to acquire lock on apic
static int apic_read(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]);
    addr_t reg_addr  = guest_addr - apic->base_addr;
    struct apic_msr * msr = (struct apic_msr *)&(apic->base_addr_msr.value);
    uint32_t val = 0;


    PrintDebug(core->vm_info, core, "apic %u: core %u: at %p: Read apic address space (%p)\n",
	       apic->lapic_id.val, core->vcpu_id, apic, (void *)guest_addr);

    if (msr->apic_enable == 0) {
	PrintError(core->vm_info, core, "apic %u: core %u: Read from APIC address space with disabled APIC, apic msr=0x%llx\n",
		   apic->lapic_id.val, core->vcpu_id, apic->base_addr_msr.value);
	return -1;
    }


    /* Because "May not be supported" doesn't matter to Linux developers... */
    /*   if (length != 4) { */
    /*     PrintError(core->vm_info, core, "Invalid apic read length (%d)\n", length); */
    /*     return -1; */
    /*   } */

    switch (reg_addr & ~0x3) {
	case EOI_OFFSET:
	    // Well, only an idiot would read from a architectural write only register
	    // Oh, Hello Linux.
	    //    PrintError(core->vm_info, core, "Attempting to read from write only register\n");
	    //    return -1;
	    break;

	    // data registers
	case APIC_ID_OFFSET:
	    val = apic->lapic_id.val;
	    break;
	case APIC_VERSION_OFFSET:
	    val = apic->apic_ver.val;
	    break;
	case TPR_OFFSET:
	    val = get_apic_tpr(apic);
	    break;
	case APR_OFFSET:
	    val = get_apic_apr(apic);
	    break;
	case PPR_OFFSET:
	    val = get_apic_ppr(apic);
	    break;
	case REMOTE_READ_OFFSET:
	    val = apic->rem_rd_data;
	    break;
	case LDR_OFFSET:
	    val = apic->log_dst.val;
	    break;
	case DFR_OFFSET:
	    val = apic->dst_fmt.val;
	    break;
	case SPURIOUS_INT_VEC_OFFSET:
	    val = apic->spurious_int.val;
	    break;
	case ESR_OFFSET:
	    val = apic->err_status.val;
	    break;
	case TMR_LOC_VEC_TBL_OFFSET:
	    val = apic->tmr_vec_tbl.val;
	    break;
	case LINT0_VEC_TBL_OFFSET:
	    val = apic->lint0_vec_tbl.val;
	    break;
	case LINT1_VEC_TBL_OFFSET:
	    val = apic->lint1_vec_tbl.val;
	    break;
	case ERR_VEC_TBL_OFFSET:
	    val = apic->err_vec_tbl.val;
	    break;
	case TMR_INIT_CNT_OFFSET:
	    val = apic->tmr_init_cnt;
	    break;
	case TMR_DIV_CFG_OFFSET:
	    val = apic->tmr_div_cfg.val;
	    break;

	case IER_OFFSET0:
	    val = *(uint32_t *)(apic->int_en_reg);
	    break;
	case IER_OFFSET1:
	    val = *(uint32_t *)(apic->int_en_reg + 4);
	    break;
	case IER_OFFSET2:
	    val = *(uint32_t *)(apic->int_en_reg + 8);
	    break;
	case IER_OFFSET3:
	    val = *(uint32_t *)(apic->int_en_reg + 12);
	    break;
	case IER_OFFSET4:
	    val = *(uint32_t *)(apic->int_en_reg + 16);
	    break;
	case IER_OFFSET5:
	    val = *(uint32_t *)(apic->int_en_reg + 20);
	    break;
	case IER_OFFSET6:
	    val = *(uint32_t *)(apic->int_en_reg + 24);
	    break;
	case IER_OFFSET7:
	    val = *(uint32_t *)(apic->int_en_reg + 28);
	    break;

	case ISR_OFFSET0:
	    val = *(uint32_t *)(apic->int_svc_reg);
	    break;
	case ISR_OFFSET1:
	    val = *(uint32_t *)(apic->int_svc_reg + 4);
	    break;
	case ISR_OFFSET2:
	    val = *(uint32_t *)(apic->int_svc_reg + 8);
	    break;
	case ISR_OFFSET3:
	    val = *(uint32_t *)(apic->int_svc_reg + 12);
	    break;
	case ISR_OFFSET4:
	    val = *(uint32_t *)(apic->int_svc_reg + 16);
	    break;
	case ISR_OFFSET5:
	    val = *(uint32_t *)(apic->int_svc_reg + 20);
	    break;
	case ISR_OFFSET6:
	    val = *(uint32_t *)(apic->int_svc_reg + 24);
	    break;
	case ISR_OFFSET7:
	    val = *(uint32_t *)(apic->int_svc_reg + 28);
	    break;
   
	case TRIG_OFFSET0:
	    val = *(uint32_t *)(apic->trig_mode_reg);
	    break;
	case TRIG_OFFSET1:
	    val = *(uint32_t *)(apic->trig_mode_reg + 4);
	    break;
	case TRIG_OFFSET2:
	    val = *(uint32_t *)(apic->trig_mode_reg + 8);
	    break;
	case TRIG_OFFSET3:
	    val = *(uint32_t *)(apic->trig_mode_reg + 12);
	    break;
	case TRIG_OFFSET4:
	    val = *(uint32_t *)(apic->trig_mode_reg + 16);
	    break;
	case TRIG_OFFSET5:
	    val = *(uint32_t *)(apic->trig_mode_reg + 20);
	    break;
	case TRIG_OFFSET6:
	    val = *(uint32_t *)(apic->trig_mode_reg + 24);
	    break;
	case TRIG_OFFSET7:
	    val = *(uint32_t *)(apic->trig_mode_reg + 28);
	    break;

	case IRR_OFFSET0:
	    val = *(uint32_t *)(apic->int_req_reg);
	    break;
	case IRR_OFFSET1:
	    val = *(uint32_t *)(apic->int_req_reg + 4);
	    break;
	case IRR_OFFSET2:
	    val = *(uint32_t *)(apic->int_req_reg + 8);
	    break;
	case IRR_OFFSET3:
	    val = *(uint32_t *)(apic->int_req_reg + 12);
	    break;
	case IRR_OFFSET4:
	    val = *(uint32_t *)(apic->int_req_reg + 16);
	    break;
	case IRR_OFFSET5:
	    val = *(uint32_t *)(apic->int_req_reg + 20);
	    break;
	case IRR_OFFSET6:
	    val = *(uint32_t *)(apic->int_req_reg + 24);
	    break;
	case IRR_OFFSET7:
	    val = *(uint32_t *)(apic->int_req_reg + 28);
	    break;
	case TMR_CUR_CNT_OFFSET:
	    val = apic->tmr_cur_cnt;
	    break;

	    // We are not going to implement these....
	case THERM_LOC_VEC_TBL_OFFSET:
	    val = apic->therm_loc_vec_tbl.val;
	    break;
	case PERF_CTR_LOC_VEC_TBL_OFFSET:
	    val = apic->perf_ctr_loc_vec_tbl.val;
	    break;

 

	    // handled registers
	case INT_CMD_LO_OFFSET:    
	    val = apic->int_cmd.lo;
	    break;
	case INT_CMD_HI_OFFSET:
	    val = apic->int_cmd.hi;
	    break;

	    // handle current timer count

	    // Unhandled Registers
	case EXT_INT_LOC_VEC_TBL_OFFSET0:
	    val = apic->ext_intr_vec_tbl[0].val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET1:
	    val = apic->ext_intr_vec_tbl[1].val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET2:
	    val = apic->ext_intr_vec_tbl[2].val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET3:
	    val = apic->ext_intr_vec_tbl[3].val;
	    break;
    

	case EXT_APIC_FEATURE_OFFSET:
	case EXT_APIC_CMD_OFFSET:
	case SEOI_OFFSET:

	default:
	    PrintError(core->vm_info, core, "apic %u: core %u: Read from Unhandled APIC Register: %x (getting zero)\n", 
		       apic->lapic_id.val, core->vcpu_id, (uint32_t)reg_addr);
	    return -1;
    }


    if (length == 1) {
	uint_t byte_addr = reg_addr & 0x3;
	uint8_t * val_ptr = (uint8_t *)dst;
    
	*val_ptr = *(((uint8_t *)&val) + byte_addr);

    } else if ((length == 2) && 
	       ((reg_addr & 0x3) != 0x3)) {
	uint_t byte_addr = reg_addr & 0x3;
	uint16_t * val_ptr = (uint16_t *)dst;
	*val_ptr = *(((uint16_t *)&val) + byte_addr);

    } else if (length == 4) {
	uint32_t * val_ptr = (uint32_t *)dst;
	*val_ptr = val;

    } else {
	PrintError(core->vm_info, core, "apic %u: core %u: Invalid apic read length (%d)\n", 
		   apic->lapic_id.val, core->vcpu_id, length);
	return -1;
    }

    PrintDebug(core->vm_info, core, "apic %u: core %u: Read finished (val=%x)\n", 
	       apic->lapic_id.val, core->vcpu_id, *(uint32_t *)dst);

    return length;
}


/**
 *
 */
static int apic_write(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]); 
    addr_t reg_addr  = guest_addr - apic->base_addr;
    struct apic_msr * msr = (struct apic_msr *)&(apic->base_addr_msr.value);
    uint32_t op_val = *(uint32_t *)src;
    addr_t flags = 0;

    PrintDebug(core->vm_info, core, "apic %u: core %u: at %p and priv_data is at %p\n",
	       apic->lapic_id.val, core->vcpu_id, apic, priv_data);

    PrintDebug(core->vm_info, core, "apic %u: core %u: write to address space (%p) (val=%x)\n", 
	       apic->lapic_id.val, core->vcpu_id, (void *)guest_addr, *(uint32_t *)src);

    if (msr->apic_enable == 0) {
	PrintError(core->vm_info, core, "apic %u: core %u: Write to APIC address space with disabled APIC, apic msr=0x%llx\n",
		   apic->lapic_id.val, core->vcpu_id, apic->base_addr_msr.value);
	return -1;
    }


    if (length != 4) {
	PrintError(core->vm_info, core, "apic %u: core %u: Invalid apic write length (%d)\n", 
		   apic->lapic_id.val, length, core->vcpu_id);
	return -1;
    }

    switch (reg_addr) {
	case REMOTE_READ_OFFSET:
	case APIC_VERSION_OFFSET:
	case APR_OFFSET:
	case IRR_OFFSET0:
	case IRR_OFFSET1:
	case IRR_OFFSET2:
	case IRR_OFFSET3:
	case IRR_OFFSET4:
	case IRR_OFFSET5:
	case IRR_OFFSET6:
	case IRR_OFFSET7:
	case ISR_OFFSET0:
	case ISR_OFFSET1:
	case ISR_OFFSET2:
	case ISR_OFFSET3:
	case ISR_OFFSET4:
	case ISR_OFFSET5:
	case ISR_OFFSET6:
	case ISR_OFFSET7:
	case TRIG_OFFSET0:
	case TRIG_OFFSET1:
	case TRIG_OFFSET2:
	case TRIG_OFFSET3:
	case TRIG_OFFSET4:
	case TRIG_OFFSET5:
	case TRIG_OFFSET6:
	case TRIG_OFFSET7:
	case PPR_OFFSET:
	case EXT_APIC_FEATURE_OFFSET:

	    PrintError(core->vm_info, core, "apic %u: core %u: Attempting to write to read only register %p (error)\n", 
		       apic->lapic_id.val, core->vcpu_id, (void *)reg_addr);

	    break;

	    // Data registers
	case APIC_ID_OFFSET:
	    //V3_Print(core->vm_info, core, "apic %u: core %u: my id is being changed to %u\n", 
	    //       apic->lapic_id.val, core->vcpu_id, op_val);

	    apic->lapic_id.val = op_val;
	    break;
	case TPR_OFFSET:
	    set_apic_tpr(apic,op_val);
	    break;
	case LDR_OFFSET:
	    PrintDebug(core->vm_info, core, "apic %u: core %u: setting log_dst.val to 0x%x\n",
		       apic->lapic_id.val, core->vcpu_id, op_val);
	    flags = v3_lock_irqsave(apic_dev->state_lock);
	    apic->log_dst.val = op_val;
	    v3_unlock_irqrestore(apic_dev->state_lock, flags);
	    break;
	case DFR_OFFSET:
	    flags = v3_lock_irqsave(apic_dev->state_lock);
	    apic->dst_fmt.val = op_val;
	    v3_unlock_irqrestore(apic_dev->state_lock, flags);
	    break;
	case SPURIOUS_INT_VEC_OFFSET:
	    apic->spurious_int.val = op_val;
	    break;
	case ESR_OFFSET:
	    apic->err_status.val = op_val;
	    break;
	case TMR_LOC_VEC_TBL_OFFSET:
	    apic->tmr_vec_tbl.val = op_val;
	    break;
	case THERM_LOC_VEC_TBL_OFFSET:
	    apic->therm_loc_vec_tbl.val = op_val;
	    break;
	case PERF_CTR_LOC_VEC_TBL_OFFSET:
	    apic->perf_ctr_loc_vec_tbl.val = op_val;
	    break;
	case LINT0_VEC_TBL_OFFSET:
	    apic->lint0_vec_tbl.val = op_val;
	    break;
	case LINT1_VEC_TBL_OFFSET:
	    apic->lint1_vec_tbl.val = op_val;
	    break;
	case ERR_VEC_TBL_OFFSET:
	    apic->err_vec_tbl.val = op_val;
	    break;
	case TMR_INIT_CNT_OFFSET:
	    apic->tmr_init_cnt = op_val;
	    apic->tmr_cur_cnt = op_val;
	    break;
	case TMR_CUR_CNT_OFFSET:
	    apic->tmr_cur_cnt = op_val;
	    break;
	case TMR_DIV_CFG_OFFSET:
	    PrintDebug(core->vm_info, core, "apic %u: core %u: setting tmr_div_cfg to 0x%x\n",
		       apic->lapic_id.val, core->vcpu_id, op_val);
	    apic->tmr_div_cfg.val = op_val;
	    break;


	    // Enable mask (256 bits)
	case IER_OFFSET0:
	    *(uint32_t *)(apic->int_en_reg) = op_val;
	    break;
	case IER_OFFSET1:
	    *(uint32_t *)(apic->int_en_reg + 4) = op_val;
	    break;
	case IER_OFFSET2:
	    *(uint32_t *)(apic->int_en_reg + 8) = op_val;
	    break;
	case IER_OFFSET3:
	    *(uint32_t *)(apic->int_en_reg + 12) = op_val;
	    break;
	case IER_OFFSET4:
	    *(uint32_t *)(apic->int_en_reg + 16) = op_val;
	    break;
	case IER_OFFSET5:
	    *(uint32_t *)(apic->int_en_reg + 20) = op_val;
	    break;
	case IER_OFFSET6:
	    *(uint32_t *)(apic->int_en_reg + 24) = op_val;
	    break;
	case IER_OFFSET7:
	    *(uint32_t *)(apic->int_en_reg + 28) = op_val;
	    break;

	case EXT_INT_LOC_VEC_TBL_OFFSET0:
	    apic->ext_intr_vec_tbl[0].val = op_val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET1:
	    apic->ext_intr_vec_tbl[1].val = op_val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET2:
	    apic->ext_intr_vec_tbl[2].val = op_val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET3:
	    apic->ext_intr_vec_tbl[3].val = op_val;
	    break;


	    // Action Registers
	case EOI_OFFSET:
	    // do eoi 
	    apic_do_eoi(core, apic);
	    break;

	case INT_CMD_LO_OFFSET: {
	    // execute command 

	    struct v3_gen_ipi tmp_ipi;

	    apic->int_cmd.lo = op_val;

	    tmp_ipi.vector = apic->int_cmd.vec;
	    tmp_ipi.mode = apic->int_cmd.del_mode;
	    tmp_ipi.logical = apic->int_cmd.dst_mode;
	    tmp_ipi.trigger_mode = apic->int_cmd.trig_mode;
	    tmp_ipi.dst_shorthand = apic->int_cmd.dst_shorthand;
	    tmp_ipi.dst = apic->int_cmd.dst;
		
	    tmp_ipi.ack = NULL;
	    tmp_ipi.private_data = NULL;
	    

	    //	    V3_Print(core->vm_info, core, "apic %u: core %u: sending cmd 0x%llx to apic %u\n", 
	    //       apic->lapic_id.val, core->vcpu_id,
	    //       apic->int_cmd.val, apic->int_cmd.dst);

	    if (route_ipi(apic_dev, apic, &tmp_ipi) == -1) { 
		PrintError(core->vm_info, core, "IPI Routing failure\n");
		return -1;
	    }

	    break;
	}
	case INT_CMD_HI_OFFSET: {
	    apic->int_cmd.hi = op_val;
	    //V3_Print(core->vm_info, core, "apic %u: core %u: writing command high=0x%x\n", apic->lapic_id.val, core->vcpu_id,apic->int_cmd.hi);
	    break;
	}
	// Unhandled Registers
	case EXT_APIC_CMD_OFFSET:
	case SEOI_OFFSET:
	default:
	    PrintError(core->vm_info, core, "apic %u: core %u: Write to Unhandled APIC Register: %x (ignored)\n", 
		       apic->lapic_id.val, core->vcpu_id, (uint32_t)reg_addr);

	    return -1;
    }

    PrintDebug(core->vm_info, core, "apic %u: core %u: Write finished\n", apic->lapic_id.val, core->vcpu_id);

    return length;

}



/* Interrupt Controller Functions */


static int apic_intr_pending(struct guest_info * core, void * private_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]); 
    int req_irq = 0;    
    int svc_irq = 0;

    // Activate all queued IRQ entries
    drain_irq_entries(apic);

    // Check for newly activated entries
    req_irq = get_highest_irr(apic);
    svc_irq = get_highest_isr(apic);

    //    PrintDebug(core->vm_info, core, "apic %u: core %u: req_irq=%d, svc_irq=%d\n",apic->lapic_id.val,info->vcpu_id,req_irq,svc_irq);


    if ((req_irq >= 0) && 
	(req_irq > svc_irq)) {

	// We have a new requested vector that is higher priority than
	// the vector that is in-service

	uint32_t ppr = get_apic_ppr(apic);

	if ((req_irq & 0xf0) > (ppr & 0xf0)) {
	    // it's also higher priority than the current
	    // processor priority.  Therefore this
	    // interrupt can go in now.
	    return 1;
	} else {
	    // processor priority is currently too high
	    // for this interrupt to go in now.  
	    // note that if tpr=0xf?, then ppr=0xf?
	    // and thus all vectors will be masked
	    // as required (tpr=0xf? => all masked)
	    return 0;
	}
    } else {
	// the vector that is in service is higher
	// priority than any new requested vector
	return 0;
    }
}



static int apic_get_intr_number(struct guest_info * core, void * private_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]); 
    int req_irq = get_highest_irr(apic);
    int svc_irq = get_highest_isr(apic);


    // for the logic here, see the comments for apic_intr_pending
    if ((req_irq >=0) &&
	(req_irq > svc_irq)) { 
	
	uint32_t ppr = get_apic_ppr(apic);
	
	if ((req_irq & 0xf0) > (ppr & 0xf0)) {
	    return req_irq;
	} else {
	    // hmm, this should not have happened, but, anyway,
	    // no interrupt is currently ready to go in
	    return -1;
	}
    } else {
	return -1;
    }
}



int v3_apic_send_ipi(struct v3_vm_info * vm, struct v3_gen_ipi * ipi, void * dev_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)
	(((struct vm_device *)dev_data)->private_data);

    return route_ipi(apic_dev, NULL, ipi);
}



static int apic_begin_irq(struct guest_info * core, void * private_data, int irq) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]); 
    int major_offset = (irq & ~0x00000007) >> 3;
    int minor_offset = irq & 0x00000007;
    uint8_t *req_location = apic->int_req_reg + major_offset;
    uint8_t *svc_location = apic->int_svc_reg + major_offset;
    uint8_t flag = 0x01 << minor_offset;

    if (*req_location & flag) {
	// we will only pay attention to a begin irq if we
	// know that we initiated it!
	*svc_location |= flag;
	*req_location &= ~flag;
    } else {
	// do nothing... 
	//PrintDebug(core->vm_info, core, "apic %u: core %u: begin irq for %d ignored since I don't own it\n",
	//	   apic->lapic_id.val, core->vcpu_id, irq);
    }

    return 0;
}


/* Timer Functions */

static void apic_inject_timer_intr(struct guest_info *core,
			           void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]); 
    // raise irq
    PrintDebug(core->vm_info, core, "apic %u: core %u: Raising APIC Timer interrupt (periodic=%d) (icnt=%d)\n",
	       apic->lapic_id.val, core->vcpu_id,
	       apic->tmr_vec_tbl.tmr_mode, apic->tmr_init_cnt);

    if (apic_intr_pending(core, priv_data)) {
        PrintDebug(core->vm_info, core, "apic %u: core %u: Overriding pending IRQ %d\n", 
	           apic->lapic_id.val, core->vcpu_id, 
	           apic_get_intr_number(core, priv_data));
    }

    if (activate_internal_irq(apic, APIC_TMR_INT) == -1) {
	PrintError(core->vm_info, core, "apic %u: core %u: Could not raise Timer interrupt\n",
		   apic->lapic_id.val, core->vcpu_id);
    }

    return;
}
	



static void apic_update_time(struct guest_info * core, 
			     uint64_t cpu_cycles, uint64_t cpu_freq, 
			     void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->vcpu_id]); 

    // The 32 bit GCC runtime is a pile of shit
#ifdef __V3_64BIT__
    uint64_t tmr_ticks = 0;
#else 
    uint32_t tmr_ticks = 0;
#endif

    uint8_t tmr_div = *(uint8_t *)&(apic->tmr_div_cfg.val);
    uint_t shift_num = 0;


    // Check whether this is true:
    //   -> If the Init count is zero then the timer is disabled
    //      and doesn't just blitz interrupts to the CPU
    if ((apic->tmr_init_cnt == 0) || 
	( (apic->tmr_vec_tbl.tmr_mode == APIC_TMR_ONESHOT) &&
	  (apic->tmr_cur_cnt == 0))) {
	//PrintDebug(core->vm_info, core, "apic %u: core %u: APIC timer not yet initialized\n",apic->lapic_id.val,info->vcpu_id);
	return;
    }


    switch (tmr_div) {
	case APIC_TMR_DIV1:
	    shift_num = 0;
	    break;
	case APIC_TMR_DIV2:
	    shift_num = 1;
	    break;
	case APIC_TMR_DIV4:
	    shift_num = 2;
	    break;
	case APIC_TMR_DIV8:
	    shift_num = 3;
	    break;
	case APIC_TMR_DIV16:
	    shift_num = 4;
	    break;
	case APIC_TMR_DIV32:
	    shift_num = 5;
	    break;
	case APIC_TMR_DIV64:
	    shift_num = 6;
	    break;
	case APIC_TMR_DIV128:
	    shift_num = 7;
	    break;
	default:
	    PrintError(core->vm_info, core, "apic %u: core %u: Invalid Timer Divider configuration\n",
		       apic->lapic_id.val, core->vcpu_id);
	    return;
    }

    tmr_ticks = cpu_cycles >> shift_num;
    //    PrintDebug(core->vm_info, core, "Timer Ticks: %p\n", (void *)tmr_ticks);

    if (tmr_ticks < apic->tmr_cur_cnt) {
	apic->tmr_cur_cnt -= tmr_ticks;
#ifdef V3_CONFIG_APIC_ENQUEUE_MISSED_TMR_IRQS
	if (apic->missed_ints && !apic_intr_pending(core, priv_data)) {
	    PrintDebug(core->vm_info, core, "apic %u: core %u: Injecting queued APIC timer interrupt.\n",
		       apic->lapic_id.val, core->vcpu_id);
	    apic_inject_timer_intr(core, priv_data);
	    apic->missed_ints--;
	}
#endif /* CONFIG_APIC_ENQUEUE_MISSED_TMR_IRQS */ 
    } else {
	tmr_ticks -= apic->tmr_cur_cnt;
	apic->tmr_cur_cnt = 0;

	apic_inject_timer_intr(core, priv_data);

	if (apic->tmr_vec_tbl.tmr_mode == APIC_TMR_PERIODIC) {
	    int queued_ints = tmr_ticks / apic->tmr_init_cnt;
	    tmr_ticks = tmr_ticks % apic->tmr_init_cnt;
	    apic->tmr_cur_cnt = apic->tmr_init_cnt - tmr_ticks;
	    apic->missed_ints += queued_ints;
	}
    }

    return;
}

static struct intr_ctrl_ops intr_ops = {
    .intr_pending = apic_intr_pending,
    .get_intr_number = apic_get_intr_number,
    .begin_irq = apic_begin_irq,
};


static struct v3_timer_ops timer_ops = {
    .update_timer = apic_update_time,
};




static int apic_free(struct apic_dev_state * apic_dev) {
    int i = 0;
    struct v3_vm_info * vm = NULL;

    for (i = 0; i < apic_dev->num_apics; i++) {
	struct apic_state * apic = &(apic_dev->apics[i]);
	struct guest_info * core = apic->core;
	
	vm = core->vm_info;

	v3_remove_intr_controller(core, apic->controller_handle);

	if (apic->timer) {
	    v3_remove_timer(core, apic->timer);
	}

	v3_lock_deinit(&(apic->irq_queue.lock));

	// unhook memory

    }

    v3_unhook_msr(vm, BASE_ADDR_MSR);

    v3_lock_deinit(&(apic_dev->state_lock));

    V3_Free(apic_dev);
    return 0;
}

#ifdef V3_CONFIG_CHECKPOINT

#define KEY_MAX 128
#define MAKE_KEY(x) snprintf(key,KEY_MAX,"%s%d",x,i);

static int apic_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct apic_dev_state * apic_state = (struct apic_dev_state *)private_data;
    int i = 0;
    uint32_t temp;
    char key[KEY_MAX];

    V3_CHKPT_SAVE(ctx, "NUM_APICS", apic_state->num_apics,savefailout);

    for (i = 0; i < apic_state->num_apics; i++) {
      drain_irq_entries(&(apic_state->apics[i]));

      MAKE_KEY("BASE_ADDR"); 
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].base_addr,savefailout);
      MAKE_KEY("BASE_ADDR_MSR"); 
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].base_addr_msr,savefailout);
      MAKE_KEY("LAPIC_ID");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].lapic_id,savefailout);
      MAKE_KEY("APIC_VER");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].apic_ver,savefailout);
      MAKE_KEY("EXT_APIC_CTRL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].ext_apic_ctrl,savefailout);
      MAKE_KEY("LOCAL_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].local_vec_tbl,savefailout);
      MAKE_KEY("TMR_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].tmr_vec_tbl,savefailout);
      MAKE_KEY("TMR_DIV_CFG");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].tmr_div_cfg,savefailout);
      MAKE_KEY("LINT0_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].lint0_vec_tbl,savefailout);
      MAKE_KEY("LINT1_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].lint1_vec_tbl,savefailout);
      MAKE_KEY("PERF_CTR_LOC_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].perf_ctr_loc_vec_tbl,savefailout);
      MAKE_KEY("THERM_LOC_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].therm_loc_vec_tbl,savefailout);
      MAKE_KEY("ERR_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].err_vec_tbl,savefailout);
      MAKE_KEY("ERR_STATUS");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].err_status,savefailout);
      MAKE_KEY("SPURIOUS_INT");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].spurious_int,savefailout);
      MAKE_KEY("INT_CMD");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].int_cmd,savefailout);
      MAKE_KEY("LOG_DST");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].log_dst,savefailout);
      MAKE_KEY("DST_FMT");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].dst_fmt,savefailout);

      // APR and PPR are stored only for compatability
      // TPR is in APIC_TPR, APR and PPR are derived
	
      temp = get_apic_apr(&(apic_state->apics[i]));
      MAKE_KEY("ARB_PRIO");
      V3_CHKPT_SAVE(ctx, key, temp,savefailout);
      temp = get_apic_tpr(&(apic_state->apics[i]));
      MAKE_KEY("TASK_PRIO");
      V3_CHKPT_SAVE(ctx,key,temp,savefailout);
      temp = get_apic_ppr(&(apic_state->apics[i]));
      MAKE_KEY("PROC_PRIO");
      V3_CHKPT_SAVE(ctx, key,temp,savefailout);

      MAKE_KEY("EXT_APIC_FEATURE");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].ext_apic_feature,savefailout);
      MAKE_KEY("SPEC_EOI");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].spec_eoi,savefailout);
      MAKE_KEY("TMR_CUR_CNT");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].tmr_cur_cnt,savefailout);
      
      MAKE_KEY("TMR_INIT_CNT");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].tmr_init_cnt,savefailout);
      MAKE_KEY("EXT_INTR_VEC_TBL");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].ext_intr_vec_tbl,savefailout);

      MAKE_KEY("REM_RD_DATA");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].rem_rd_data,savefailout);
      MAKE_KEY("IPI_STATE");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].ipi_state,savefailout);
      MAKE_KEY("INT_REQ_REG");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].int_req_reg,savefailout);
      MAKE_KEY("INT_SVC_REG");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].int_svc_reg,savefailout);
      MAKE_KEY("INT_EN_REG");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].int_en_reg,savefailout);
      MAKE_KEY("TRIG_MODE_REG");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].trig_mode_reg,savefailout);
      MAKE_KEY("EOI");
      V3_CHKPT_SAVE(ctx, key, apic_state->apics[i].eoi,savefailout);

    }

    return 0;

 savefailout:
    PrintError(VM_NONE, VCORE_NONE, "Failed to save apic\n");
    return -1;
}

static int apic_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct apic_dev_state *apic_state = (struct apic_dev_state *)private_data;
    int i = 0;
    uint32_t temp;
    char key[KEY_MAX];

    V3_CHKPT_LOAD(ctx,"NUM_APICS", apic_state->num_apics, loadfailout);

    for (i = 0; i < apic_state->num_apics; i++) {
      drain_irq_entries(&(apic_state->apics[i]));

      MAKE_KEY("BASE_ADDR"); 
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].base_addr,loadfailout);
      MAKE_KEY("BASE_ADDR_MSR"); 
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].base_addr_msr,loadfailout);
      MAKE_KEY("LAPIC_ID");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].lapic_id,loadfailout);
      MAKE_KEY("APIC_VER");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].apic_ver,loadfailout);
      MAKE_KEY("EXT_APIC_CTRL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].ext_apic_ctrl,loadfailout);
      MAKE_KEY("LOCAL_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].local_vec_tbl,loadfailout);
      MAKE_KEY("TMR_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].tmr_vec_tbl,loadfailout);
      MAKE_KEY("TMR_DIV_CFG");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].tmr_div_cfg,loadfailout);
      MAKE_KEY("LINT0_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].lint0_vec_tbl,loadfailout);
      MAKE_KEY("LINT1_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].lint1_vec_tbl,loadfailout);
      MAKE_KEY("PERF_CTR_LOC_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].perf_ctr_loc_vec_tbl,loadfailout);
      MAKE_KEY("THERM_LOC_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].therm_loc_vec_tbl,loadfailout);
      MAKE_KEY("ERR_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].err_vec_tbl,loadfailout);
      MAKE_KEY("ERR_STATUS");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].err_status,loadfailout);
      MAKE_KEY("SPURIOUS_INT");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].spurious_int,loadfailout);
      MAKE_KEY("INT_CMD");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].int_cmd,loadfailout);
      MAKE_KEY("LOG_DST");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].log_dst,loadfailout);
      MAKE_KEY("DST_FMT");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].dst_fmt,loadfailout);

      // APR and PPR are stored only for compatability
      // TPR is in APIC_TPR, APR and PPR are derived

      MAKE_KEY("ARB_PRIO");
      V3_CHKPT_LOAD(ctx, key, temp,loadfailout);
      // discarded

      MAKE_KEY("TASK_PRIO");
      V3_CHKPT_LOAD(ctx,key,temp,loadfailout);
      set_apic_tpr(&(apic_state->apics[i]),temp);
      
      MAKE_KEY("PROC_PRIO");
      V3_CHKPT_LOAD(ctx, key,temp,loadfailout);
      // discarded


      MAKE_KEY("EXT_APIC_FEATURE");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].ext_apic_feature,loadfailout);
      MAKE_KEY("SPEC_EOI");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].spec_eoi,loadfailout);
      MAKE_KEY("TMR_CUR_CNT");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].tmr_cur_cnt,loadfailout);
      
      MAKE_KEY("TMR_INIT_CNT");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].tmr_init_cnt,loadfailout);
      MAKE_KEY("EXT_INTR_VEC_TBL");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].ext_intr_vec_tbl,loadfailout);

      MAKE_KEY("REM_RD_DATA");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].rem_rd_data,loadfailout);
      MAKE_KEY("IPI_STATE");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].ipi_state,loadfailout);
      MAKE_KEY("INT_REQ_REG");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].int_req_reg,loadfailout);
      MAKE_KEY("INT_SVC_REG");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].int_svc_reg,loadfailout);
      MAKE_KEY("INT_EN_REG");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].int_en_reg,loadfailout);
      MAKE_KEY("TRIG_MODE_REG");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].trig_mode_reg,loadfailout);
      MAKE_KEY("EOI");
      V3_CHKPT_LOAD(ctx, key, apic_state->apics[i].eoi,loadfailout);
    }
    
    
    return 0;
    
 loadfailout:
    PrintError(VM_NONE,VCORE_NONE, "Failed to load apic\n");
    return -1;
    
}

#endif

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))apic_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = apic_save,
    .load = apic_load
#endif
};



static int apic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    char * dev_id = v3_cfg_val(cfg, "ID");
    struct apic_dev_state * apic_dev = NULL;
    int i = 0;

    PrintDebug(vm, VCORE_NONE, "apic: creating an APIC for each core\n");

    apic_dev = (struct apic_dev_state *)V3_Malloc(sizeof(struct apic_dev_state) + 
						  sizeof(struct apic_state) * vm->num_cores);


    if (!apic_dev) {
	PrintError(vm, VCORE_NONE, "Failed to allocate space for APIC\n");
	return -1;
    }

    memset(apic_dev,0,
	   sizeof(struct apic_dev_state) + 
	   sizeof(struct apic_state) * vm->num_cores);

    apic_dev->num_apics = vm->num_cores;
    v3_lock_init(&(apic_dev->state_lock));

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, apic_dev);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "apic: Could not attach device %s\n", dev_id);
	V3_Free(apic_dev);
	return -1;
    }

    
    for (i = 0; i < vm->num_cores; i++) {
	struct apic_state * apic = &(apic_dev->apics[i]);
	struct guest_info * core = &(vm->cores[i]);

	apic->core = core;

	init_apic_state(apic, i);

    	apic->controller_handle = v3_register_intr_controller(core, &intr_ops, apic_dev);

    	apic->timer = v3_add_timer(core, &timer_ops, apic_dev);

	if (apic->timer == NULL) {
	    PrintError(vm, VCORE_NONE,"APIC: Failed to attach timer to core %d\n", i);
	    v3_remove_device(dev);
	    return -1;
	}

	v3_hook_full_mem(vm, core->vcpu_id, apic->base_addr, apic->base_addr + PAGE_SIZE_4KB, apic_read, apic_write, apic_dev);

	PrintDebug(vm, VCORE_NONE, "apic %u: (setup device): done, my id is %u\n", i, apic->lapic_id.val);
    }

#ifdef V3_CONFIG_DEBUG_APIC
    for (i = 0; i < vm->num_cores; i++) {
	struct apic_state * apic = &(apic_dev->apics[i]);
	PrintDebug(vm, VCORE_NONE, "apic: sanity check: apic %u (at %p) has id %u and msr value %llx and core at %p\n",
		   i, apic, apic->lapic_id.val, apic->base_addr_msr.value,apic->core);
    }
#endif


    PrintDebug(vm, VCORE_NONE, "apic: priv_data is at %p\n", apic_dev);

    v3_hook_msr(vm, BASE_ADDR_MSR, read_apic_msr, write_apic_msr, apic_dev);

    return 0;
}



device_register("LAPIC", apic_init)
