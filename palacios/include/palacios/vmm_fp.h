/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_FP_H
#define __VMM_FP_H

#include <palacios/vmm_types.h>
#include <palacios/vmm.h>
#ifdef V3_CONFIG_LAZY_FPU_SWITCH
#include <interfaces/vmm_lazy_fpu.h>
#endif

// the FPRs are arranged into the 
// precise layout of the FXSAVE/FXRESTORE instructions 
// bytes 32+, which is common for all three variants
// 8*6 reserved + 8*10 (fpu/mmx) + 16*16 (xmm) 
// + 3*16 (res) + 3*16 (ava) = 480 bytes
// another 32 bytes are used for the store header
// which varies depending on machine mode
struct v3_fp_regs {
  v3_fp_mmx_reg_t   stmm0;  // stmm0..7 are the x87 stack or mmx regs
  uint8_t           res0[6]; 
  v3_fp_mmx_reg_t   stmm1;  
  uint8_t           res1[6]; 
  v3_fp_mmx_reg_t   stmm2;  
  uint8_t           res2[6]; 
  v3_fp_mmx_reg_t   stmm3;  
  uint8_t           res3[6]; 
  v3_fp_mmx_reg_t   stmm4;  
  uint8_t           res4[6]; 
  v3_fp_mmx_reg_t   stmm5;
  uint8_t           res5[6]; 
  v3_fp_mmx_reg_t   stmm6;  
  uint8_t           res6[6]; 
  v3_fp_mmx_reg_t   stmm7;  
  uint8_t           res7[6]; 
  v3_xmm_reg_t      xmm0;   // xmm0..7 are the "classic" SSE regs
  v3_xmm_reg_t      xmm1;
  v3_xmm_reg_t      xmm2;
  v3_xmm_reg_t      xmm3;
  v3_xmm_reg_t      xmm4;
  v3_xmm_reg_t      xmm5;
  v3_xmm_reg_t      xmm6;
  v3_xmm_reg_t      xmm7;
  v3_xmm_reg_t      xmm8;    //xmm8..15 are the "new" SSE reg
  v3_xmm_reg_t      xmm9;
  v3_xmm_reg_t      xmm10;
  v3_xmm_reg_t      xmm11;
  v3_xmm_reg_t      xmm12;
  v3_xmm_reg_t      xmm13;
  v3_xmm_reg_t      xmm14;
  v3_xmm_reg_t      xmm15;
  v3_xmm_reg_t      res16;  // reserved
  v3_xmm_reg_t      res17;
  v3_xmm_reg_t      res18;
  v3_xmm_reg_t      ava19;
  v3_xmm_reg_t      ava20;
  v3_xmm_reg_t      ava21;
} __attribute__((packed)) __attribute__((aligned(16)));

// FXSAVE, 32 bit mode header (32 bytes)
// V3_FP_MODE_32
struct v3_fp_32_state {
  uint16_t          fcw;
  uint16_t          fsw;
  uint8_t           ftw;
  uint8_t           res0;
  uint16_t          fop;
  uint32_t          fip; //fpu instruction pointer
  uint16_t          fcs; //fpu code segment selector
  uint16_t          res1;
  uint32_t          fdp; //fpu data pointer
  uint16_t          fds; //fpu data segment selector
  uint16_t          res2;
  uint32_t          mxcsr;
  uint32_t          mxcsr_mask;
} __attribute__((packed)) __attribute__((aligned(16)));

// FXSAVE, 64 bit mode header, REX.W=1 (32 bytes)
// V3_FP_MODE_64
struct v3_fp_64_state {
  uint16_t          fcw;
  uint16_t          fsw;
  uint8_t           ftw;
  uint8_t           res0;
  uint16_t          fop;
  uint64_t          fip; //fpu instruction pointer
  uint64_t          fdp; //fpu data pointer
  uint32_t          mxcsr;
  uint32_t          mxcsr_mask;
} __attribute__((packed)) __attribute__((aligned(16)));


// FXSAVE, 64 bit mode header, REX.W=0 (32 bytes)
// V3_FP_MODE_64_COMPAT
struct v3_fp_64compat_state {
  uint16_t          fcw;
  uint16_t          fsw;
  uint8_t           ftw;
  uint8_t           res0;
  uint16_t          fop;
  uint32_t          fip; //fpu instruction pointer
  uint16_t          fcs; //fpu code segment selector
  uint16_t          res1;
  uint32_t          fdp; //fpu data pointer
  uint16_t          fds; //fpu data segment selector
  uint16_t          res2;
  uint32_t          mxcsr;
  uint32_t          mxcsr_mask;
} __attribute__((packed)) __attribute__((aligned(16)));


//
// This is an FXSAVE block
//    
struct v3_fp_state_core {
  union {
    struct v3_fp_32_state fp32;
    struct v3_fp_64_state fp64;
    struct v3_fp_64compat_state fp64compat;
  } header;
  struct v3_fp_regs fprs;
} __attribute__((packed)) __attribute__((aligned(16)));
  
struct v3_fp_state {
  // Do we need to restore on next entry?
  int need_restore;
  // The meaning 
  enum {V3_FP_MODE_32=0, V3_FP_MODE_64, V3_FP_MODE_64_COMPAT} state_type;
  struct v3_fp_state_core  state __attribute__((aligned(16)));
} ;


struct guest_info;

// Can we save FP state on this core?
int v3_can_handle_fp_state(); 

// Save state from this core to the structure
int v3_get_fp_state(struct guest_info *core);

// Restore FP state from this structure to this core
int v3_put_fp_state(struct guest_info *core);

int v3_init_fp(void);
int v3_deinit_fp(void);

#ifndef V3_CONFIG_FP_SWITCH

/* Ideally these would use the TS trick to do lazy calls to used_fpu() */
#define V3_FP_EXIT_SAVE(core)

#define V3_FP_ENTRY_RESTORE(core)	 				    \
  do {							                    \
    if ((core)->fp_state.need_restore) {				    \
      v3_put_fp_state(core);                                \
      (core)->fp_state.need_restore=0;		   			    \
    }                                                       \
  } while (0)

#else

#ifdef V3_CONFIG_LAZY_FPU_SWITCH


/* Ideally these would use the TS trick to do lazy calls to used_fpu() */
#define V3_FP_EXIT_SAVE(core)                                               \
  do {							                    \
    extern struct v3_lazy_fpu_hooks * lazy_fpu_hooks;			    \
    if ((lazy_fpu_hooks) && (lazy_fpu_hooks)->used_fpu)                   { \
      (lazy_fpu_hooks)->used_fpu();                                         \
    } else {                                                                \
      v3_get_fp_state(core);                                                \
    }                                                                       \
  } while (0)

#define V3_FP_ENTRY_RESTORE(core)	 				    \
  do {							                    \
    extern struct v3_lazy_fpu_hooks * lazy_fpu_hooks;		            \
    if ((core)->fp_state.need_restore) {				    \
      v3_put_fp_state(core);                                                \
      (core)->fp_state.need_restore=0;		   			    \
    } else {                                                                \
	if ((lazy_fpu_hooks) && (lazy_fpu_hooks)->will_use_fpu)           { \
	(lazy_fpu_hooks)->need_fpu();                   		    \
       } else {                                                             \
         v3_put_fp_state(core);                                             \
       }                                                                    \
    }						                            \
  } while (0)

#else

// conservative FPU switching

#define V3_FP_EXIT_SAVE(core) v3_get_fp_state(core)
#define V3_FP_ENTRY_RESTORE(core) v3_put_fp_state(core)

#endif

#endif

#ifdef V3_CONFIG_CHECKPOINT

struct v3_chkpt_ctx;

// save state from structure to checkpoint/migration context
int v3_save_fp_state(struct v3_chkpt_ctx *ctx, struct guest_info *core);

// load state from checkpoint/migration context to structure
int v3_load_fp_state(struct v3_chkpt_ctx *ctx, struct guest_info *core);


#endif

#endif
