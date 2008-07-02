#include <palacios/svm_halt.h>

// From GeekOS
void Yield(void);


int handle_svm_halt(struct guest_info * info)
{
  // What we should do is starting waiting on an OS event that will
  // result in an injection of an interrupt.

  // What we will hackishly do instead is resume on any event
  // Plus is this totally GeekOS specific

  PrintDebug("GeekOS Yield\n");

  Yield();

  PrintDebug("GeekOS Yield Done\n");

  info->rip+=1;

  return 0;

}
