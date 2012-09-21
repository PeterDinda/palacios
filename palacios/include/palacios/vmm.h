/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_H__
#define __VMM_H__


/*#include <palacios/vm_guest.h>*/

struct guest_info;


#ifdef __V3VEE__
#include <palacios/vmm_mem.h>
#include <palacios/vmm_types.h>

//#include <palacios/vmm_types.h>
#include <palacios/vmm_string.h>




/* utility definitions */


#define V3_Print(fmt, args...)					\
    do {							\
	extern struct v3_os_hooks * os_hooks;			\
	if ((os_hooks) && (os_hooks)->print) {			\
	    (os_hooks)->print((fmt), ##args);			\
	}							\
    } while (0)	


#define PrintDebug(fmt, args...)			\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->print) {	\
	    (os_hooks)->print((fmt), ##args);	\
	}						\
    } while (0)						


#define PrintError(fmt, args...)					\
    do {								\
	extern struct v3_os_hooks * os_hooks;				\
	if ((os_hooks) && (os_hooks)->print) {			\
	    (os_hooks)->print("%s(%d): " fmt, __FILE__, __LINE__, ##args); \
	}								\
    } while (0)						



/* 4KB-aligned */
#define V3_AllocPages(num_pages)			        	\
    ({							        	\
	extern struct v3_os_hooks * os_hooks;		        	\
	void * ptr = 0;					        	\
	if ((os_hooks) && (os_hooks)->allocate_pages) {	        	\
	    ptr = (os_hooks)->allocate_pages(num_pages,PAGE_SIZE_4KB);	\
	}						        	\
	ptr;						        	\
    })


#define V3_AllocAlignedPages(num_pages, align)		        	\
    ({							        	\
	extern struct v3_os_hooks * os_hooks;		        	\
	void * ptr = 0;					        	\
	if ((os_hooks) && (os_hooks)->allocate_pages) {	        	\
	    ptr = (os_hooks)->allocate_pages(num_pages,align);  	\
	}						        	\
	ptr;						        	\
    })


#define V3_FreePages(page, num_pages)			\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->free_pages) {	\
	    (os_hooks)->free_pages(page, num_pages);	\
	}						\
    } while(0)


#define V3_VAddr(addr) ({					\
	    extern struct v3_os_hooks * os_hooks;		\
	    void * var = 0;					\
	    if ((os_hooks) && (os_hooks)->paddr_to_vaddr) {	\
		var = (os_hooks)->paddr_to_vaddr(addr);		\
	    }							\
	    var;						\
	})


#define V3_PAddr(addr) ({					\
	    extern struct v3_os_hooks * os_hooks;		\
	    void * var = 0;					\
	    if ((os_hooks) && (os_hooks)->vaddr_to_paddr) {	\
		var = (os_hooks)->vaddr_to_paddr(addr);		\
	    }							\
	    var;						\
	})



#define V3_Malloc(size) ({				\
	    extern struct v3_os_hooks * os_hooks;	\
	    void * var = 0;				\
	    if ((os_hooks) && (os_hooks)->malloc) {	\
		var = (os_hooks)->malloc(size);		\
	    }						\
	    if (!var) PrintError("MALLOC FAILURE. Memory LEAK!!\n");	\
	    var;					\
	})

// We need to check the hook structure at runtime to ensure its SAFE
#define V3_Free(addr)				\
    do {					\
	extern struct v3_os_hooks * os_hooks;	\
	if ((os_hooks) && (os_hooks)->free) {	\
	    (os_hooks)->free(addr);		\
	}					\
    } while (0)

// uint_t V3_CPU_KHZ();
#define V3_CPU_KHZ() ({							\
	    unsigned int khz = 0;					\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->get_cpu_khz) {		\
		khz = (os_hooks)->get_cpu_khz();			\
	    }								\
	    khz;							\
	})								\
	



#define V3_Hook_Interrupt(vm, irq) ({					\
	    int ret = 0;						\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->hook_interrupt) {		\
		ret = (os_hooks)->hook_interrupt(vm, irq);		\
	    }								\
	    ret;							\
	})								\
	

#define V3_ACK_IRQ(irq)						\
    do {							\
	extern struct v3_os_hooks * os_hooks;			\
	if ((os_hooks) && (os_hooks)->ack_irq) {		\
	    (os_hooks)->ack_irq(irq);				\
	}							\
    } while (0)



#define V3_Get_CPU() ({  				                \
            int ret = 0;                                                \
            extern struct v3_os_hooks * os_hooks;                       \
            if ((os_hooks) && (os_hooks)->get_cpu) {                    \
                ret = (os_hooks)->get_cpu();                            \
            }                                                           \
            ret;                                                        \
        })




#define V3_CREATE_THREAD(fn, arg, name)	({				\
	    void * thread = NULL;					\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->start_kernel_thread) {	\
		thread = (os_hooks)->start_kernel_thread(fn, arg, name); \
	    }								\
	    thread;							\
	})




#define V3_Call_On_CPU(cpu, fn, arg)    		\
    do {						\
        extern struct v3_os_hooks * os_hooks;           \
        if ((os_hooks) && (os_hooks)->call_on_cpu) {    \
            (os_hooks)->call_on_cpu(cpu, fn, arg);      \
        }                                               \
    } while (0)



#define V3_CREATE_THREAD_ON_CPU(cpu, fn, arg, name) ({			\
	    void * thread = NULL;					\
	    extern struct v3_os_hooks * os_hooks;			\
	    if ((os_hooks) && (os_hooks)->start_thread_on_cpu) {	\
		thread = (os_hooks)->start_thread_on_cpu(cpu, fn, arg, name); \
	    }								\
	    thread;							\
	})

#define V3_MOVE_THREAD_TO_CPU(pcpu, thread) ({				\
	int ret = -1;							\
	extern struct v3_os_hooks * os_hooks;				\
	if((os_hooks) && (os_hooks)->move_thread_to_cpu) {		\
	    ret = (os_hooks)->move_thread_to_cpu(pcpu, thread);		\
	}								\
	ret;								\
    })
    

/* ** */


#define V3_ASSERT(x)							\
    do {								\
	extern struct v3_os_hooks * os_hooks; 				\
	if (!(x)) {							\
	    PrintDebug("Failed assertion in %s: %s at %s, line %d, RA=%lx\n", \
		       __func__, #x, __FILE__, __LINE__,		\
		       (ulong_t) __builtin_return_address(0));		\
	    while(1){							\
		if ((os_hooks) && (os_hooks)->yield_cpu) {		\
	    		(os_hooks)->yield_cpu();			\
		}							\
	    }								\
	}								\
    } while(0)								\
	


#define V3_Yield()					\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->yield_cpu) {	\
	    (os_hooks)->yield_cpu();			\
	}						\
    } while (0)						\


#define V3_Sleep(usec)				\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->sleep_cpu) {\
	    (os_hooks)->sleep_cpu(usec);		\
	} else {					\
	    V3_Yield();                                 \
        }                                               \
    }  while (0)                                        \

#define V3_Wakeup(cpu)					\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->wakeup_cpu) {	\
	    (os_hooks)->wakeup_cpu(cpu);			\
	}						\
    } while (0)						\


typedef enum v3_vm_class {V3_INVALID_VM, V3_PC_VM, V3_CRAY_VM} v3_vm_class_t;


// Maybe make this a define....
typedef enum v3_cpu_arch {V3_INVALID_CPU, V3_SVM_CPU, V3_SVM_REV3_CPU, V3_VMX_CPU, V3_VMX_EPT_CPU, V3_VMX_EPT_UG_CPU} v3_cpu_arch_t;


v3_cpu_mode_t v3_get_host_cpu_mode();

void v3_yield(struct guest_info * info, int usec);
void v3_yield_cond(struct guest_info * info, int usec);
void v3_print_cond(const char * fmt, ...);

void v3_interrupt_cpu(struct v3_vm_info * vm, int logical_cpu, int vector);



v3_cpu_arch_t v3_get_cpu_type(int cpu_id);


int v3_vm_enter(struct guest_info * info);
int v3_reset_vm_core(struct guest_info * core, addr_t rip);


#endif /*!__V3VEE__ */



struct v3_vm_info;

/* This will contain function pointers that provide OS services */
struct v3_os_hooks {
    void (*print)(const char * format, ...)
  	__attribute__ ((format (printf, 1, 2)));
  
    void *(*allocate_pages)(int num_pages, unsigned int alignment);
    void (*free_pages)(void * page, int num_pages);

    void *(*malloc)(unsigned int size);
    void (*free)(void * addr);

    void *(*paddr_to_vaddr)(void * addr);
    void *(*vaddr_to_paddr)(void * addr);

    int (*hook_interrupt)(struct v3_vm_info * vm, unsigned int irq);
    int (*ack_irq)(int irq);

    unsigned int (*get_cpu_khz)(void);

    void (*yield_cpu)(void); 
    void (*sleep_cpu)(unsigned int usec);
    void (*wakeup_cpu)(void *cpu);

    void *(*mutex_alloc)(void);
    void (*mutex_free)(void * mutex);
    void (*mutex_lock)(void * mutex, int must_spin);
    void (*mutex_unlock)(void * mutex);
    void *(*mutex_lock_irqsave)(void * mutex, int must_spin);
    void (*mutex_unlock_irqrestore)(void * mutex, void *flags);

    unsigned int (*get_cpu)(void);



    void * (*start_kernel_thread)(int (*fn)(void * arg), void * arg, char * thread_name); 
    void (*interrupt_cpu)(struct v3_vm_info * vm, int logical_cpu, int vector);
    void (*call_on_cpu)(int logical_cpu, void (*fn)(void * arg), void * arg);
    void * (*start_thread_on_cpu)(int cpu_id, int (*fn)(void * arg), void * arg, char * thread_name);
    int (*move_thread_to_cpu)(int cpu_id,  void * thread);
};


/*
 *
 * This is the interrupt state that the VMM's interrupt handlers need to see
 */
struct v3_interrupt {
    unsigned int irq;
    unsigned int error;

    unsigned int should_ack;  /* Should the vmm ack this interrupt, or will
    			       * the host OS do it? */
};




void Init_V3(struct v3_os_hooks * hooks, char * cpus, int num_cpus);
void Shutdown_V3( void );


struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data, char * name);
int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask);
int v3_stop_vm(struct v3_vm_info * vm);
int v3_pause_vm(struct v3_vm_info * vm);
int v3_continue_vm(struct v3_vm_info * vm);
int v3_simulate_vm(struct v3_vm_info * vm, unsigned int msecs);


int v3_save_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_load_vm(struct v3_vm_info * vm, char * store, char * url);

int v3_send_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_receive_vm(struct v3_vm_info * vm, char * store, char * url);

int v3_move_vm_core(struct v3_vm_info * vm, int vcore_id, int target_cpu);


int v3_free_vm(struct v3_vm_info * vm);

int v3_deliver_irq(struct v3_vm_info * vm, struct v3_interrupt * intr);


#endif
