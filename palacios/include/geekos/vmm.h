#ifndef __VMM_H
#define __VMM_H


#include <geekos/ktypes.h>
#include <geekos/string.h>

#include <geekos/vmm_mem.h>


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




// We need to check the hook structure at runtime to ensure its SAFE
#define VMMMalloc(size)                                 \
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->malloc) {		\
      (os_hooks)->malloc(size);				\
    }							\
  } while (0)						\


/* ** */


#define VMM_INVALID_CPU 0
#define VMM_VMX_CPU 1
#define VMM_SVM_CPU 2



typedef struct guest_info {
  ullong_t rip;
  ullong_t rsp;

  vmm_mem_map_t mem_map;
  // preallocation map
  // device_map

  void * vmm_data;
} guest_info_t;



/* We need a memory map and an IO device map */

/* This will contain function pointers that provide OS services */
struct vmm_os_hooks {
  void (*print_info)(const char * format, ...);
  void (*print_debug)(const char * format, ...);
  void (*print_trace)(const char * format, ...);
  
  void *(*Allocate_Pages)(int numPages);
  void (*Free_Page)(void * page);

  void *(*malloc)(uint_t size);
  void (*free)(void * addr);


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
