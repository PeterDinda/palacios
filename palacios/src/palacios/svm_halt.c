/* (c) 2008, Peter Dinda <pdinda@northwestern.edu> */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */


#include <palacios/svm_halt.h>
#include <palacios/vmm_intr.h>
// From GeekOS
void Yield(void);


//
// This should trigger a #GP if cpl!=0, otherwise, yield to host
//

int handle_svm_halt(struct guest_info * info)
{
  if (info->cpl!=0) { 
    v3_raise_exception(info, GPF_EXCEPTION);
  } else {
    
    // What we should do is starting waiting on an OS event that will
    // result in an injection of an interrupt.
    
    // What we will hackishly do instead is resume on any event
    // Plus is this totally GeekOS specific
    
    ullong_t yield_start = 0;
    ullong_t yield_stop = 0;
    uint32_t gap = 0;
    
    PrintDebug("GeekOS Yield\n");
    
    rdtscll(yield_start);
    Yield();
    rdtscll(yield_stop);
    
    
    //v3_update_time(info, yield_stop - yield_start);
    gap = yield_stop - yield_start;
    v3_raise_irq(info, 0);
    
    PrintDebug("GeekOS Yield Done (%d cycles)\n", gap);
    
    info->rip+=1;
  }
    
  return 0;

}
