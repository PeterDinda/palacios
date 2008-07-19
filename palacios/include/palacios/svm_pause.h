#ifndef __SVM_PAUSE_H
#define __SVM_PAUSE_H
#include <palacios/vm_guest.h>
#include <palacios/vmcb.h>
#include <palacios/vmm.h>


int handle_svm_pause(struct guest_info * info);



#endif
