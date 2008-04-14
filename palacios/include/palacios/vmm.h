#ifndef __VMM_H
#define __VMM_H


//#include <palacios/vmm_types.h>
#include <palacios/vmm_string.h>

#include <palacios/vmm_mem.h>
//#include <palacios/vmm_paging.h>

#include <palacios/vm_guest.h>

/* utility definitions */
#define PrintDebug(fmt, args...)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_debug) {	\
      (os_hooks)->print_debug((fmt), ##args);		\
    }							\
  } while (0)						\



#define PrintInfo(fmt, args...) 		        \
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_info) {		\
      (os_hooks)->print_info((fmt), ##args);		\
    }							\
  } while (0)						\


#define PrintTrace(fmt, args...)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_trace) {	\
      (os_hooks)->print_trace((fmt), ##args);		\
    }							\
  } while (0)						\




/* This clearly won't work, we need some way to get a return value out of it */
#define VMMMalloc(type, var, size)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->malloc) {		\
      var = (type)(os_hooks)->malloc(size);		\
    }							\
  } while (0)						\


// We need to check the hook structure at runtime to ensure its SAFE
#define VMMFree(addr)					\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->free) {		\
      (os_hooks)->free(addr);				\
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
  
  void *(*allocate_pages)(int numPages);
  void (*free_page)(void * page);

  void *(*malloc)(unsigned int size);
  void (*free)(void * addr);

  void *(*paddr_to_vaddr)(void *addr);
  void *(*vaddr_to_paddr)(void *addr);

  // Do we need this here?
  void (*snprintf)(char * dst, char * format, int len, ...);

  void (*start_kernel_thread)(); // include pointer to function
};



/* This will contain Function pointers that control the VMs */
struct vmm_ctrl_ops {
  int (*init_guest)(struct guest_info* info);
  int (*start_guest)(struct guest_info * info);
  //  int (*stop_vm)(uint_t vm_id);
};





void Init_VMM(struct vmm_os_hooks * hooks, struct vmm_ctrl_ops * vmm_ops);





#endif
