/* (c) 2008, Peter Dinda <pdinda@northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#ifndef __SVM_PAUSE_H
#define __SVM_PAUSE_H

#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmcb.h>
#include <palacios/vmm.h>


int handle_svm_pause(struct guest_info * info);


#endif // ! __V3VEE__

#endif
