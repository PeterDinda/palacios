#include <palacios/svm_halt.h>
#include <palacios/vmm_intr.h>
// From GeekOS
void Yield(void);


int handle_svm_halt(struct guest_info * info)
{
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
  raise_irq(info, 0);

  PrintDebug("GeekOS Yield Done (%d cycles)\n", gap);

  info->rip+=1;

  return 0;

}
