#ifndef __VMM_H
#define __VMM_H


#include <geekos/ktypes.h>
#include <geekos/string.h>



/* utility definitions */
#define PrintDebug(fmt, args...)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_debug) {	\
      (os_hooks)->print_debug((fmt), ##args);		\
    }							\
  } while (0)						\
    


#define PrintInfo(fmt, args...) 		\
  do {						\
    extern struct vmm_os_hooks * os_hooks;	\
    if ((os_hooks) && (os_hooks)->print_info) {	\
      (os_hooks)->print_info((fmt), ##args);	\
    }						\
  } while (0)					\
  

#define PrintTrace(fmt, args...)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_trace) {	\
      (os_hooks)->print_trace((fmt), ##args);		\
    }							\
  } while (0)						\
  

/* ** */




#define VMM_INVALID_CPU 0
#define VMM_VMX_CPU 1
#define VMM_SVM_CPU 2


/* This will contain function pointers that provide OS services */
struct vmm_os_hooks {
  void (*print_info)(const char * format, ...);
  void (*print_debug)(const char * format, ...);
  void (*print_trace)(const char * format, ...);
};




/* This will contain Function pointers that control the VMs */
struct vmm_ctrl_ops {
  

};



void Init_VMM(struct vmm_os_hooks * hooks);




#endif
