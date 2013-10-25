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

#include <palacios/vmm.h>
#include <palacios/vmm_fp.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_lowlevel.h>

#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>
#endif


static int can_do_fp=-1;

// assumes identical on all cores...
int v3_can_handle_fp_state()
{
  if (can_do_fp!=-1) { 
    return can_do_fp;
  } else {
    uint32_t eax, ebx, ecx, edx;

    v3_cpuid(CPUID_FEATURE_IDS,&eax,&ebx,&ecx,&edx);
    
    can_do_fp= !!(edx & (1<<25)); // do we have SSE?
    
    return can_do_fp;
  }
}

int v3_init_fp()
{
  if (v3_can_handle_fp_state()) { 
    V3_Print(VM_NONE,VCORE_NONE,"Floating point save/restore init:  available on this hardware\n");
  } else {
    V3_Print(VM_NONE,VCORE_NONE,"Floating point save/restore init:  UNAVAILABLE ON THIS HARDWARE\n");
  }
  return 0;
}

int v3_deinit_fp()
{
  V3_Print(VM_NONE,VCORE_NONE,"Floating point save/restore deinited\n");
  return 0;
}

#define EFER_MSR 0xc0000080


int v3_get_fp_state(struct guest_info *core)
{ 
  if (v3_can_handle_fp_state()) { 
    /*
      If the fast-FXSAVE/FXRSTOR (FFXSR) feature is enabled in EFER, FXSAVE and FXRSTOR do not save or restore the XMM0–15 registers when executed in 64-bit mode at CPL 0. The x87 environment and MXCSR are saved whether fast-FXSAVE/FXRSTOR is enabled or not. Software can use the CPUID instruction to determine whether the fast-FXSAVE/FXRSTOR feature is available
      (CPUID Fn8000_0001h_EDX[FFXSR]). The fast-FXSAVE/FXRSTOR feature has no effect on FXSAVE/FXRSTOR in non 64-bit mode or when CPL > 0.
      
    */

    // We need to assure that the fast-FXSAVE/FXRSTOR are not on
    // otherwise we will NOT have the XMM regs since we running at CPL 0
    //

    int restore=0;
    uint32_t high,low;
    
    v3_get_msr(EFER_MSR,&high,&low);
    
    if (low & (0x1<<14)) { 
      // fast save is in effect
      low &= ~(0x1<<14);
      restore=1;
      v3_set_msr(EFER_MSR, high, low);
    }
    
    __asm__ __volatile__(" rex64/fxsave %0 ; "
			 : "=m"(core->fp_state.state)); /* no input, no clobber */
    if (restore) { 
      low |= 0x1<<14;
      v3_set_msr(EFER_MSR, high, low);
    }

    // this is a giant guess
    // we really need to capture the state type as seen in the guest, not here...
    core->fp_state.state_type=V3_FP_MODE_64;
    
    return 0;

  } else {
    return -1;
  }
}


// Restore FP state from this structure to this core
int v3_put_fp_state(struct guest_info *core)
{
  if (v3_can_handle_fp_state()) {
    // We need to assure that the fast-FXSAVE/FXRSTOR are not on
    // otherwise we will NOT have the XMM regs since we running at CPL 0
    //

    int restore=0;
    uint32_t high,low;
    
    v3_get_msr(EFER_MSR,&high,&low);
    
    if (low & (0x1<<14)) { 
      // fast restore is in effect
      low &= ~(0x1<<14);
      restore=1;
      v3_set_msr(EFER_MSR, high, low);
    }

    __asm__ __volatile__(" rex64/fxrstor %0; "
			 : /* no output */
			 : "m"((core->fp_state.state)) ); /* no clobber*/

    
    if (restore) { 
      low |= 0x1<<14;
      v3_set_msr(EFER_MSR, high, low);
    }

    return 0;
  } else {
    return -1;
  }
}

#ifdef V3_CONFIG_CHECKPOINT


int v3_save_fp_state(struct v3_chkpt_ctx *ctx, struct guest_info *core)
{
  V3_CHKPT_SAVE(ctx, "FP_STATE_TYPE", core->fp_state.state_type, savefailout);
  if (v3_chkpt_save(ctx,"FP_STATE_BLOB",sizeof(core->fp_state.state),&(core->fp_state.state))) { 
    goto savefailout;
  }
  
  return 0;

 savefailout:
  PrintError(core->vm_info,core,"Unable to save floating point state\n");
  return -1;
}


int v3_load_fp_state(struct v3_chkpt_ctx *ctx, struct guest_info *core)
{
  V3_CHKPT_LOAD(ctx, "FP_STATE_TYPE", core->fp_state.state_type, loadfailout);
  if (v3_chkpt_load(ctx,"FP_STATE_BLOB",sizeof(core->fp_state.state),&(core->fp_state.state))) { 
    goto loadfailout;
  }
  
  return 0;

 loadfailout:
  PrintError(core->vm_info,core,"Unable to load floating point state\n");
  return -1;
}

#endif
