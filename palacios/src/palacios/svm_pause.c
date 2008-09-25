/* (c) 2008, Peter Dinda <pdinda@northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#include <palacios/svm_pause.h>
#include <palacios/vmm_intr.h>


int handle_svm_pause(struct guest_info * info)
{
  // handled as a nop

  info->rip+=2;

  return 0;

}
