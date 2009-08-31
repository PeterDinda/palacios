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


#include <devices/apic.h>
#include <devices/apic_regs.h>
#include <palacios/vmm.h>
#include <palacios/vmm_msr.h>


#ifndef CONFIG_DEBUG_APIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


typedef enum { APIC_TMR_INT, APIC_THERM_INT, APIC_PERF_INT, 
	       APIC_LINT0_INT, APIC_LINT1_INT, APIC_ERR_INT } apic_irq_type_t;

#define APIC_FIXED_DELIVERY  0x0
#define APIC_SMI_DELIVERY    0x2
#define APIC_NMI_DELIVERY    0x4
#define APIC_INIT_DELIVERY   0x5
#define APIC_EXTINT_DELIVERY 0x7


#define BASE_ADDR_MSR 0x0000001B
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
	uint64_t val;
	struct {
	    uchar_t rsvd;
	    uint_t bootstrap_cpu : 1;
	    uint_t rsvd2         : 2;
	    uint_t apic_enable   : 1;
	    ullong_t base_addr   : 40;
	    uint_t rsvd3         : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




struct apic_state {
    addr_t base_addr;

    /* MSRs */
    v3_msr_t base_addr_msr;


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
    struct arb_prio_reg arb_prio;
    struct task_prio_reg task_prio;
    struct proc_prio_reg proc_prio;
    struct ext_apic_feature_reg ext_apic_feature;
    struct spec_eoi_reg spec_eoi;
  

    uint32_t tmr_cur_cnt;
    uint32_t tmr_init_cnt;


    struct local_vec_tbl_reg ext_intr_vec_tbl[4];

    uint32_t rem_rd_data;


    uchar_t int_req_reg[32];
    uchar_t int_svc_reg[32];
    uchar_t int_en_reg[32];
    uchar_t trig_mode_reg[32];
  
    uint32_t eoi;


};


static void init_apic_state(struct apic_state * apic) {
    apic->base_addr = DEFAULT_BASE_ADDR;
    apic->base_addr_msr.value = 0x0000000000000900LL;
    apic->base_addr_msr.value |= ((uint64_t)DEFAULT_BASE_ADDR); 

    PrintDebug("Sizeof Interrupt Request Register %d, should be 32\n", 
	       (uint_t)sizeof(apic->int_req_reg));

    memset(apic->int_req_reg, 0, sizeof(apic->int_req_reg));
    memset(apic->int_svc_reg, 0, sizeof(apic->int_svc_reg));
    memset(apic->int_en_reg, 0xff, sizeof(apic->int_en_reg));
    memset(apic->trig_mode_reg, 0, sizeof(apic->trig_mode_reg));

    apic->eoi = 0x00000000;
    apic->rem_rd_data = 0x00000000;
    apic->tmr_init_cnt = 0x00000000;
    apic->tmr_cur_cnt = 0x00000000;

    // TODO:
    // We need to figure out what the APIC ID is....
    apic->lapic_id.val = 0x00000000;

    // The P6 has 6 LVT entries, so we set the value to (6-1)...
    apic->apic_ver.val = 0x80050010;

    apic->task_prio.val = 0x00000000;
    apic->arb_prio.val = 0x00000000;
    apic->proc_prio.val = 0x00000000;
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
}




static int read_apic_msr(uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    dst->value = apic->base_addr;
    return 0;
}


static int write_apic_msr(uint_t msr, v3_msr_t src, void * priv_data) {
    //  struct vm_device * dev = (struct vm_device *)priv_data;
    //  struct apic_state * apic = (struct apic_state *)dev->private_data;

    PrintError("WRITING APIC BASE ADDR: HI=%x LO=%x\n", src.hi, src.lo);

    return -1;
}


// irq_num is the bit offset into a 256 bit buffer...
static int activate_apic_irq(struct apic_state * apic, uint32_t irq_num) {
    int major_offset = (irq_num & ~0x00000007) >> 3;
    int minor_offset = irq_num & 0x00000007;
    uchar_t * req_location = apic->int_req_reg + major_offset;
    uchar_t * en_location = apic->int_en_reg + major_offset;
    uchar_t flag = 0x1 << minor_offset;

    if (irq_num <= 15) {
	PrintError("Attempting to raise an invalid interrupt: %d\n", irq_num);
	return -1;
    }

    PrintDebug("Raising APIC IRQ %d\n", irq_num);

    if (*en_location & flag) {
	*req_location |= flag;
    } else {
	PrintDebug("Interrupt  not enabled... %.2x\n", *en_location);
	return 0;
    }

    return 0;
}



static int get_highest_isr(struct apic_state * apic) {
    int i = 0, j = 0;

    // We iterate backwards to find the highest priority
    for (i = 31; i >= 0; i--) {
	uchar_t  * svc_major = apic->int_svc_reg + i;
    
	if ((*svc_major) & 0xff) {
	    for (j = 7; j >= 0; j--) {
		uchar_t flag = 0x1 << j;
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

    // We iterate backwards to find the highest priority
    for (i = 31; i >= 0; i--) {
	uchar_t  * req_major = apic->int_req_reg + i;
    
	if ((*req_major) & 0xff) {
	    for (j = 7; j >= 0; j--) {
		uchar_t flag = 0x1 << j;
		if ((*req_major) & flag) {
		    return ((i * 8) + j);
		}
	    }
	}
    }

    return -1;
}
 



static int apic_do_eoi(struct apic_state * apic) {
    int isr_irq = get_highest_isr(apic);

    if (isr_irq != -1) {
	int major_offset = (isr_irq & ~0x00000007) >> 3;
	int minor_offset = isr_irq & 0x00000007;
	uchar_t flag = 0x1 << minor_offset;
	uchar_t * svc_location = apic->int_svc_reg + major_offset;
	
	PrintDebug("Received APIC EOI for IRQ %d\n", isr_irq);
	
	*svc_location &= ~flag;

#ifdef CONFIG_CRAY_XT
	
	if ((isr_irq == 238) || 
	    (isr_irq == 239)) {
	    PrintError("Acking IRQ %d\n", isr_irq);
	}
	
	if (isr_irq == 238) {
	    V3_ACK_IRQ(238);
	}
#endif
    } else {
	PrintError("Spurious EOI...\n");
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
	    del_mode = APIC_FIXED_DELIVERY;
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
	    del_mode = APIC_FIXED_DELIVERY;
	    masked = apic->err_vec_tbl.mask;
	    break;
	default:
	    PrintError("Invalid APIC interrupt type\n");
	    return -1;
    }

    // interrupt is masked, don't send
    if (masked == 1) {
	PrintDebug("Inerrupt is masked\n");
	return 0;
    }

    if (del_mode == APIC_FIXED_DELIVERY) {
	//PrintDebug("Activating internal APIC IRQ %d\n", vec_num);
	return activate_apic_irq(apic, vec_num);
    } else {
	PrintError("Unhandled Delivery Mode\n");
	return -1;
    }
}


static int apic_read(addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    addr_t reg_addr  = guest_addr - apic->base_addr;
    struct apic_msr * msr = (struct apic_msr *)&(apic->base_addr_msr.value);
    uint32_t val = 0;


    PrintDebug("Read apic address space (%p)\n", 
	       (void *)guest_addr);

    if (msr->apic_enable == 0) {
	PrintError("Write to APIC address space with disabled APIC\n");
	return -1;
    }


    /* Because "May not be supported" doesn't matter to Linux developers... */
    /*   if (length != 4) { */
    /*     PrintError("Invalid apic read length (%d)\n", length); */
    /*     return -1; */
    /*   } */

    switch (reg_addr & ~0x3) {
	case EOI_OFFSET:
	    // Well, only an idiot would read from a architectural write only register
	    // Oh, Hello Linux.
	    //    PrintError("Attempting to read from write only register\n");
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
	    val = apic->task_prio.val;
	    break;
	case APR_OFFSET:
	    val = apic->arb_prio.val;
	    break;
	case PPR_OFFSET:
	    val = apic->proc_prio.val;
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
	    PrintError("Read from Unhandled APIC Register: %x\n", (uint32_t)reg_addr);
	    return -1;
    }


    if (length == 1) {
	uint_t byte_addr = reg_addr & 0x3;
	uint8_t * val_ptr = (uint8_t *)dst;
    
	*val_ptr = *(((uint8_t *)&val) + byte_addr);

    } else if ((length == 2) && 
	       ((reg_addr & 0x3) == 0x3)) {
	uint_t byte_addr = reg_addr & 0x3;
	uint16_t * val_ptr = (uint16_t *)dst;
	*val_ptr = *(((uint16_t *)&val) + byte_addr);

    } else if (length == 4) {
	uint32_t * val_ptr = (uint32_t *)dst;
	*val_ptr = val;

    } else {
	PrintError("Invalid apic read length (%d)\n", length);
	return -1;
    }

    PrintDebug("Read finished (val=%x)\n", *(uint32_t *)dst);

    return length;
}


static int apic_write(addr_t guest_addr, void * src, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    addr_t reg_addr  = guest_addr - apic->base_addr;
    struct apic_msr * msr = (struct apic_msr *)&(apic->base_addr_msr.value);
    uint32_t op_val = *(uint32_t *)src;

    PrintDebug("Write to apic address space (%p) (val=%x)\n", 
	       (void *)guest_addr, *(uint32_t *)src);

    if (msr->apic_enable == 0) {
	PrintError("Write to APIC address space with disabled APIC\n");
	return -1;
    }


    if (length != 4) {
	PrintError("Invalid apic write length (%d)\n", length);
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
#if 1
	    PrintError("Attempting to write to read only register %p (ignored)\n", (void *)reg_addr);
#else   
	    PrintError("Attempting to write to read only register %p (error)\n", (void *)reg_addr);
	    return -1;
#endif
	    break;

	    // Data registers
	case APIC_ID_OFFSET:
	    apic->lapic_id.val = op_val;
	    break;
	case TPR_OFFSET:
	    apic->task_prio.val = op_val;
	    break;
	case LDR_OFFSET:
	    apic->log_dst.val = op_val;
	    break;
	case DFR_OFFSET:
	    apic->dst_fmt.val = op_val;
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
	    apic_do_eoi(apic);
	    break;

	case INT_CMD_LO_OFFSET:
	case INT_CMD_HI_OFFSET:
	    // Unhandled Registers

	case EXT_APIC_CMD_OFFSET:
	case SEOI_OFFSET:
	default:
	    PrintError("Write to Unhandled APIC Register: %x\n", (uint32_t)reg_addr);
	    return -1;
    }

    PrintDebug("Write finished\n");

    return length;
}



/* Interrupt Controller Functions */

// returns 1 if an interrupt is pending, 0 otherwise
static int apic_intr_pending(void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    int req_irq = get_highest_irr(apic);
    int svc_irq = get_highest_isr(apic);

    if ((req_irq >= 0) && 
	(req_irq > svc_irq)) {
	return 1;
    }

    return 0;
}

static int apic_get_intr_number(void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    int req_irq = get_highest_irr(apic);
    int svc_irq = get_highest_isr(apic);

    if (svc_irq == -1) {
	return req_irq;
    } else if (svc_irq < req_irq) {
	return req_irq;
    }

    return -1;
}

static int apic_raise_intr(void * private_data, int irq) {
#ifdef CONFIG_CRAY_XT
    // The Seastar is connected directly to the LAPIC via LINT0 on the ICC bus

    if (irq == 238) {
	struct vm_device * dev = (struct vm_device *)private_data;
	struct apic_state * apic = (struct apic_state *)dev->private_data;

	return activate_apic_irq(apic, irq);
    }
#endif
    return 0;
}

static int apic_lower_intr(void * private_data, int irq) {
    return 0;
}

static int apic_begin_irq(void * private_data, int irq) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    int major_offset = (irq & ~0x00000007) >> 3;
    int minor_offset = irq & 0x00000007;
    uchar_t * req_location = apic->int_req_reg + major_offset;
    uchar_t * svc_location = apic->int_svc_reg + major_offset;
    uchar_t flag = 0x01 << minor_offset;

    *svc_location |= flag;
    *req_location &= ~flag;

#ifdef CONFIG_CRAY_XT
    if ((irq == 238) || (irq == 239)) {
	PrintError("APIC: Begin IRQ %d (ISR=%x), (IRR=%x)\n", irq, *svc_location, *req_location);
    }
#endif

    return 0;
}



int v3_apic_raise_intr(struct vm_device * apic_dev, int intr_num) {
    struct apic_state * apic = (struct apic_state *)apic_dev->private_data;
    return activate_apic_irq(apic, intr_num);
}



/* Timer Functions */
static void apic_update_time(ullong_t cpu_cycles, ullong_t cpu_freq, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct apic_state * apic = (struct apic_state *)dev->private_data;
    // The 32 bit GCC runtime is a pile of shit
#ifdef __V3_64BIT__
    uint64_t tmr_ticks = 0;
#else 
    uint32_t tmr_ticks = 0;
#endif

    uchar_t tmr_div = *(uchar_t *)&(apic->tmr_div_cfg.val);
    uint_t shift_num = 0;


    // Check whether this is true:
    //   -> If the Init count is zero then the timer is disabled
    //      and doesn't just blitz interrupts to the CPU
    if ((apic->tmr_init_cnt == 0) || 
	( (apic->tmr_vec_tbl.tmr_mode == APIC_TMR_ONESHOT) &&
	  (apic->tmr_cur_cnt == 0))) {
	//PrintDebug("APIC timer not yet initialized\n");
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
	    PrintError("Invalid Timer Divider configuration\n");
	    return;
    }

    tmr_ticks = cpu_cycles >> shift_num;
    //    PrintDebug("Timer Ticks: %p\n", (void *)tmr_ticks);

    if (tmr_ticks < apic->tmr_cur_cnt) {
	apic->tmr_cur_cnt -= tmr_ticks;
    } else {
	tmr_ticks -= apic->tmr_cur_cnt;
	apic->tmr_cur_cnt = 0;

	// raise irq
	PrintDebug("Raising APIC Timer interrupt (periodic=%d) (icnt=%d) (div=%d)\n", 
		   apic->tmr_vec_tbl.tmr_mode, apic->tmr_init_cnt, shift_num);

	if (apic_intr_pending(priv_data)) {
	    PrintDebug("Overriding pending IRQ %d\n", apic_get_intr_number(priv_data));
	}

	if (activate_internal_irq(apic, APIC_TMR_INT) == -1) {
	    PrintError("Could not raise Timer interrupt\n");
	}
    
	if (apic->tmr_vec_tbl.tmr_mode == APIC_TMR_PERIODIC) {
	    tmr_ticks = tmr_ticks % apic->tmr_init_cnt;
	    apic->tmr_cur_cnt = apic->tmr_init_cnt - tmr_ticks;
	}
    }


}



static struct intr_ctrl_ops intr_ops = {
    .intr_pending = apic_intr_pending,
    .get_intr_number = apic_get_intr_number,
    .raise_intr = apic_raise_intr,
    .begin_irq = apic_begin_irq,
    .lower_intr = apic_lower_intr, 
};


static struct vm_timer_ops timer_ops = {
    .update_time = apic_update_time,
};




static int apic_free(struct vm_device * dev) {
    struct guest_info * info = dev->vm;

    v3_unhook_msr(info, BASE_ADDR_MSR);

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = apic_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};



static int apic_init(struct guest_info * vm, void * cfg_data) {
    PrintDebug("Creating APIC\n");

    struct apic_state * apic = (struct apic_state *)V3_Malloc(sizeof(struct apic_state));

    struct vm_device * dev = v3_allocate_device("LAPIC", &dev_ops, apic);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "LAPIC");
	return -1;
    }

    v3_register_intr_controller(vm, &intr_ops, dev);
    v3_add_timer(vm, &timer_ops, dev);

    init_apic_state(apic);

    v3_hook_msr(vm, BASE_ADDR_MSR, read_apic_msr, write_apic_msr, dev);

    v3_hook_full_mem(vm, apic->base_addr, apic->base_addr + PAGE_SIZE_4KB, apic_read, apic_write, dev);

    return 0;
}



device_register("LAPIC", apic_init)
