#ifndef __SVM_WBINVD_H
#define __SVM_WBINVD_H

#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmcb.h>
#include <palacios/vmm.h>


int handle_svm_wbinvd(struct guest_info * info);


#endif // ! __V3VEE__

#endif
