/* Northwestern University */
/* (c) 2008, Peter Dinda <pdinda@northwestern.edu> */

#include <palacios/svm_wbinvd.h>
#include <palacios/vmm_intr.h>


// Writeback and invalidate caches
// should raise #GP if CPL is not zero
// Otherwise execute

int handle_svm_wbinvd(struct guest_info * info)
{
  if (info->cpl!=0) { 
    PrintDebug("WBINVD: cpl!=0, injecting GPF\n");
    v3_raise_exception(info,GPF_EXCEPTION);
  } else {
    info->rip+=2;
    asm("wbinvd");
  }
  return 0;

}
