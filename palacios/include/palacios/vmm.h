#ifndef __VMM_H
#define __VMM_H


#include <palacios/vm_guest.h>
#include <palacios/vmm_mem.h>

#ifdef __V3VEE__

//#include <palacios/vmm_types.h>
#include <palacios/vmm_string.h>


//#include <palacios/vmm_paging.h>

/* utility definitions */

#if VMM_DEBUG
#define PrintDebug(fmt, args...)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_debug) {	\
      (os_hooks)->print_debug((fmt), ##args);		\
    }							\
  } while (0)						
#else
#define PrintDebug(fmt,args ...)
#endif


#if VMM_INFO
#define PrintInfo(fmt, args...) 		        \
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_info) {		\
      (os_hooks)->print_info((fmt), ##args);		\
    }							\
  } while (0)						
#else
#define PrintInfo(fmt, args...)
#endif


#if VMM_TRACE
#define PrintTrace(fmt, args...)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->print_trace) {	\
      (os_hooks)->print_trace((fmt), ##args);		\
    }							\
  } while (0)						
#else
#define PrintTrace(fmt, args...)
#endif


#define V3_AllocPages(ptr, num_pages)		        \
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    ptr = 0;						\
    if ((os_hooks) && (os_hooks)->allocate_pages) {	\
      ptr = (os_hooks)->allocate_pages(num_pages);	\
    }							\
  } while (0)						\


/*
#define V3_Malloc(type, var, size)			\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    var = 0;						\
    if ((os_hooks) && (os_hooks)->malloc) {		\
      var = (type)(os_hooks)->malloc(size);		\
    }							\
  } while (0)						\
*/

#define V3_Malloc(size) ({			\
      extern struct vmm_os_hooks * os_hooks;	\
      void * var = 0;				\
      if ((os_hooks) && (os_hooks)->malloc) {	\
	var = (os_hooks)->malloc(size);		\
      }						\
      var;					\
    })

// We need to check the hook structure at runtime to ensure its SAFE
#define V3_Free(addr)					\
  do {							\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->free) {		\
      (os_hooks)->free(addr);				\
    }							\
  } while (0)						\

#define V3_CPU_KHZ()					\
  ({ 							\
    unsigned int khz = 0;				\
    extern struct vmm_os_hooks * os_hooks;		\
    if ((os_hooks) && (os_hooks)->get_cpu_khz) {	\
      khz = (os_hooks)->get_cpu_khz();			\
    }							\
    khz;						\
  })							\
    

/* ** */

#define V3_ASSERT(x)							\
  do {									\
    if (!(x)) {							 	\
      PrintDebug("Failed assertion in %s: %s at %s, line %d, RA=%lx\n",	\
		 __func__, #x, __FILE__, __LINE__,			\
		 (ulong_t) __builtin_return_address(0));                \
      while(1);								\
    }									\
  } while(0)								\
    

#define VMM_INVALID_CPU 0
#define VMM_VMX_CPU 1
#define VMM_SVM_CPU 2

#endif //!__V3VEE__


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

  int (*hook_interrupt)(struct guest_info * info, int irq);
  int (*ack_irq)(int irq);


  unsigned int (*get_cpu_khz)();

  // Do we need this here?
  //  void (*snprintf)(char * dst, char * format, int len, ...);



  void (*start_kernel_thread)(); // include pointer to function
};



/* This will contain Function pointers that control the VMs */
struct vmm_ctrl_ops {
  int (*init_guest)(struct guest_info* info);
  int (*start_guest)(struct guest_info * info);
  //  int (*stop_vm)(uint_t vm_id);

  int (*has_nested_paging)();
};




void Init_VMM(struct vmm_os_hooks * hooks, struct vmm_ctrl_ops * vmm_ops);





#endif
