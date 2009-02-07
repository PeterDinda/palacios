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
#define ISR_OFFSET                        0x100   // 0x100 - 0x170
#define TMR_OFFSET                        0x180   // 0x180 - 0x1f0
#define IRR_OFFSET                        0x200   // 0x200 - 0x270
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
#define IER_OFFSET                        0x480   // 0x480 - 0x4f0
#define EXT_INT_LOC_VEC_TBL_OFFSET        0x500   // 0x500 - 0x530


struct apic_base_addr_msr {
  union {
    uint64_t val;
    struct {
      uchar_t rsvd;
      uint_t cpu_core      : 1;
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
  struct div_cfg_reg div_cfg;
  struct lint_vec_tbl_reg lint_vec_tbl;
  struct perf_ctr_loc_vec_tbl_reg perf_ctr_loc_vec_tbl;
  struct therm_loc_vec_tbl_reg therm_loc_vec_tbl;
  struct err_vec_tbl_reg err_vec_tbl;
  struct err_status_reg err_status;
  struct spurious_int_reg spurious_int;
  struct int_cmd_reg int_cmd;
  struct loc_dst_reg loc_dst;
  struct dst_fmt_reg dst_fmt;
  struct arb_prio_reg arb_prio;
  struct task_prio_reg task_prio;
  struct proc_prio_reg proc_prio;
  struct ext_apic_feature_reg ext_apic_feature;
  struct spec_eoi_reg spec_eoi;
  

  uint32_t tmr_cur_cnt_reg;
  uint32_t tmr_init_cnt_reg;



  uint32_t rem_rd_data;


  uchar_t int_req_reg[32];
  uchar_t int_svc_reg[32];
  uchar_t int_en_reg[32];
  uchar_t trig_mode_reg[32];
  
  uint32_t eoi;


};


static int read_apic_msr(uint_t msr, v3_msr_t * dst, void * priv_data) {
  struct vm_device * dev = (struct vm_device *)priv_data;
  struct apic_state * apic = (struct apic_state *)dev->private_data;
  PrintDebug("READING APIC BASE ADDR: HI=%x LO=%x\n", apic->base_addr_msr.hi, apic->base_addr_msr.lo);

  return -1;
}


static int write_apic_msr(uint_t msr, v3_msr_t src, void * priv_data) {
  //  struct vm_device * dev = (struct vm_device *)priv_data;
  //  struct apic_state * apic = (struct apic_state *)dev->private_data;

  PrintDebug("WRITING APIC BASE ADDR: HI=%x LO=%x\n", src.hi, src.lo);

  return -1;
}


static int apic_read(addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
  struct vm_device * dev = (struct vm_device *)priv_data;
  struct apic_state * apic = (struct apic_state *)dev->private_data;
  addr_t reg_addr  = guest_addr - apic->base_addr;

  if (length != 4) {
    PrintError("Invalid apic readlength\n");
    return -1;
  }

  switch (reg_addr) {
  case APIC_ID_OFFSET:
  case APIC_VERSION_OFFSET:
  case TPR_OFFSET:
  case APR_OFFSET:
  case PPR_OFFSET:
  case EOI_OFFSET:
  case REMOTE_READ_OFFSET:
  case LDR_OFFSET:
  case DFR_OFFSET:
  case SPURIOUS_INT_VEC_OFFSET:
  case ISR_OFFSET:
  case TMR_OFFSET:
  case IRR_OFFSET:
  case ESR_OFFSET:
  case INT_CMD_LO_OFFSET:
  case INT_CMD_HI_OFFSET:
  case TMR_LOC_VEC_TBL_OFFSET:
  case THERM_LOC_VEC_TBL_OFFSET:
  case PERF_CTR_LOC_VEC_TBL_OFFSET:
  case LINT0_VEC_TBL_OFFSET:
  case LINT1_VEC_TBL_OFFSET:
  case ERR_VEC_TBL_OFFSET:
  case TMR_INIT_CNT_OFFSET:
  case TMR_CUR_CNT_OFFSET:
  case TMR_DIV_CFG_OFFSET:
  case EXT_APIC_FEATURE_OFFSET:
  case EXT_APIC_CMD_OFFSET:
  case SEOI_OFFSET:
  case IER_OFFSET:
  case EXT_INT_LOC_VEC_TBL_OFFSET:
  default:
    PrintError("Read from Unhandled APIC Register: %x\n", (uint32_t)reg_addr);
    return -1;
  }
  return length;
}


static int apic_write(addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
  PrintDebug("Write to apic address space\n");
  struct vm_device * dev = (struct vm_device *)priv_data;
  struct apic_state * apic = (struct apic_state *)dev->private_data;
  addr_t reg_addr  = guest_addr - apic->base_addr;

  if (length != 4) {
    PrintError("Invalid apic write length\n");
    return -1;
  }

  switch (reg_addr) {
  case APIC_ID_OFFSET:
  case APIC_VERSION_OFFSET:
  case TPR_OFFSET:
  case APR_OFFSET:
  case PPR_OFFSET:
  case EOI_OFFSET:
  case REMOTE_READ_OFFSET:
  case LDR_OFFSET:
  case DFR_OFFSET:
  case SPURIOUS_INT_VEC_OFFSET:
  case ISR_OFFSET:
  case TMR_OFFSET:
  case IRR_OFFSET:
  case ESR_OFFSET:
  case INT_CMD_LO_OFFSET:
  case INT_CMD_HI_OFFSET:
  case TMR_LOC_VEC_TBL_OFFSET:
  case THERM_LOC_VEC_TBL_OFFSET:
  case PERF_CTR_LOC_VEC_TBL_OFFSET:
  case LINT0_VEC_TBL_OFFSET:
  case LINT1_VEC_TBL_OFFSET:
  case ERR_VEC_TBL_OFFSET:
  case TMR_INIT_CNT_OFFSET:
  case TMR_CUR_CNT_OFFSET:
  case TMR_DIV_CFG_OFFSET:
  case EXT_APIC_FEATURE_OFFSET:
  case EXT_APIC_CMD_OFFSET:
  case SEOI_OFFSET:
  case IER_OFFSET:
  case EXT_INT_LOC_VEC_TBL_OFFSET:
  default:
    PrintError("Write to Unhandled APIC Register: %x\n", (uint32_t)reg_addr);
    return -1;
  }

  return length;
}


static int apic_deinit(struct vm_device * dev) {
  struct guest_info * info = dev->vm;

  v3_unhook_msr(info, BASE_ADDR_MSR);

  return 0;
}


static int apic_init(struct vm_device * dev) {
  struct guest_info * info = dev->vm;
  struct apic_state * apic = (struct apic_state *)(dev->private_data);

  apic->base_addr = DEFAULT_BASE_ADDR;

  v3_hook_msr(info, BASE_ADDR_MSR, read_apic_msr, write_apic_msr, dev);

  v3_hook_full_mem(info, DEFAULT_BASE_ADDR, DEFAULT_BASE_ADDR + PAGE_SIZE_4KB, apic_read, apic_write, dev);

  return 0;
}



static struct vm_device_ops dev_ops = {
  .init = apic_init,
  .deinit = apic_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * v3_create_apic() {
  PrintDebug("Creating APIC\n");

  struct apic_state * apic = (struct apic_state *)V3_Malloc(sizeof(struct apic_state));

  struct vm_device * device = v3_create_device("APIC", &dev_ops, apic);
  
  return device;
}
