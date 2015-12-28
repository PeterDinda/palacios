#ifndef _PALACIOS_H
#define _PALACIOS_H

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned int *uintptr_t;
typedef int *intptr_t;

#define LOCKCHECK_ALLOC(x)
#define LOCKCHECK_LOCK_PRE(x)
#define LOCKCHECK_LOCK_POST(x)
#define LOCKCHECK_LOCK_IRQSAVE_PRE(x,y)
#define LOCKCHECK_LOCK_IRQSAVE_POST(x,y)
#define LOCKCHECK_UNLOCK_PRE(x)
#define LOCKCHECK_UNLOCK_POST(x)
#define LOCKCHECK_UNLOCK_IRQRESTORE_PRE(x,y)
#define LOCKCHECK_UNLOCK_IRQRESTORE_POST(x,y)
#define LOCKCHECK_FREE(x)


#define NR_CPUS 64




int palacios_vmm_init( uint64_t memsize, char * options );
int palacios_vmm_exit( void );


struct v3_resource_control;

// Selected exported stubs, for use in other palacios components, like vnet
// The idea is that everything uses the same stubs
void  palacios_print_scoped(void *vm, int vcore, const char *fmt, ...);
#define palacios_print(...) palacios_print_scoped(0,-1, __VA_ARGS__)
// node_id=-1 => no node constraint
void *palacios_allocate_pages(int num_pages, unsigned int alignment, int node_id, int (*filter_func)(void *paddr, void *filter_state), void *filter_state);
void  palacios_free_pages(void *page_addr, int num_pages);
void *palacios_alloc(unsigned int size);
// node_id=-1 => no node constraint
void *palacios_alloc_extended(unsigned int size, unsigned int flags, int node_id);
void  palacios_free(void *);
void *palacios_valloc(unsigned int size); // use instead of vmalloc
void  palacios_vfree(void *);             // use instead of vfree
void *palacios_vaddr_to_paddr(void *vaddr);
void *palacios_paddr_to_vaddr(void *paddr);
void  palacios_xcall(int cpu_id, void (*fn)(void *arg), void *arg);
void *palacios_create_and_start_kernel_thread(int (*fn)(void * arg), void *arg, char *thread_name, struct v3_resource_control  *rctl);
void *palacios_create_thread_on_cpu(int cpu_id, int (*fn)(void * arg), void *arg, char *thread_name, struct v3_resource_control  *rctl);
void  palacios_start_thread(void *thread_ptr);
void *palacios_creeate_and_start_thread_on_cpu(int cpu_id, int (*fn)(void * arg), void *arg, char *thread_name, struct v3_resource_control  *rctl);
int   palacios_move_thread_to_cpu(int new_cpu_id, void *thread_ptr);
void  palacios_yield_cpu(void);
void  palacios_sleep_cpu(unsigned int us);
unsigned int palacios_get_cpu(void);
unsigned int palacios_get_cpu_khz(void);
void  palacios_used_fpu(void);
void  palacios_need_fpu(void);
void *palacios_mutex_alloc(void);         // allocates and inits a lock
void  palacios_mutex_init(void *mutex);   // only inits a lock
void  palacios_mutex_deinit(void *mutex); // only deinits a lock
void  palacios_mutex_free(void *mutex);   // deinits and frees a lock
void  palacios_mutex_lock(void *mutex, int must_spin);
void  palacios_mutex_unlock(void *mutex);
void *palacios_mutex_lock_irqsave(void *mutex, int must_spin);
void  palacios_mutex_unlock_irqrestore(void *mutex, void *flags);
// Macros for spin-locks in the module code
// By using these macros, the lock checker will be able
// to see the module code as well as the core VMM
#define palacios_spinlock_init(l) palacios_mutex_init(l)
#define palacios_spinlock_deinit(l) palacios_mutex_deinit(l)
#define palacios_spinlock_lock(l) palacios_mutex_lock(l,0)
#define palacios_spinlock_unlock(l) palacios_mutex_unlock(l)
#define palacios_spinlock_lock_irqsave(l,f) do { f=(unsigned long)palacios_mutex_lock_irqsave(l,0); } while (0)
#define palacios_spinlock_unlock_irqrestore(l,f) palacios_mutex_unlock_irqrestore(l,(void*)f)


// Palacios Printing Support

// These macros affect how palacios_print will generate output
// Turn this on for unprefaced output from palacios_print
#define V3_PRINTK_OLD_STYLE_OUTPUT 0
// Maximum length output from palacios_print
#define V3_PRINTK_BUF_SIZE 1024
// Turn this on to check if new-style output for palacios_print  contains only 7-bit chars
#define V3_PRINTK_CHECK_7BIT 1

//
// The following macros are for printing in the linux module itself, even before
// Palacios is initialized and after it it deinitialized
// All printk's in linux_module use these macros, for easier control
#define KERN_ERR ""
#define KERN_WARNING ""
#define KERN_NOTICE ""
#define KERN_INFO ""
#define KERN_DEBUG ""
#define ERROR(fmt, args...) printk((KERN_ERR "palacios (pcore %u) %s(%d): " fmt), palacios_get_cpu(), __FILE__, __LINE__, ##args)
#define WARNING(fmt, args...) printk((KERN_WARNING "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)
#define NOTICE(fmt, args...) printk((KERN_NOTICE "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)
#define INFO(fmt, args...) printk((KERN_INFO "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)
#define DEBUG(fmt, args...) printk((KERN_DEBUG "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)


#endif
