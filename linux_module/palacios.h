#ifndef _PALACIOS_H
#define _PALACIOS_H

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>


/* Global Control IOCTLs */
#define V3_CREATE_GUEST 12
#define V3_FREE_GUEST 13

#define V3_ADD_MEMORY    50
#define V3_RESET_MEMORY  51
#define V3_REMOVE_MEMORY 52

#define V3_ADD_PCI_HW_DEV 55
#define V3_ADD_PCI_USER_DEV 56

#define V3_DVFS_CTRL  60


/* VM Specific IOCTLs */
#define V3_VM_CONSOLE_CONNECT 20
#define V3_VM_STREAM_CONNECT 21

#define V3_VM_PAUSE 23
#define V3_VM_CONTINUE 24

#define V3_VM_LAUNCH 25
#define V3_VM_STOP 26
#define V3_VM_LOAD 27
#define V3_VM_SAVE 28
#define V3_VM_SIMULATE 29

#define V3_VM_INSPECT 30
#define V3_VM_DEBUG 31

#define V3_VM_MOVE_CORE 33

#define V3_VM_SEND    34
#define V3_VM_RECEIVE 35

#define V3_VM_MOVE_MEM 36

#define V3_VM_RESET    40

#define V3_VM_FB_INPUT 257
#define V3_VM_FB_QUERY 258

#define V3_VM_MEM_TRACK_SIZE 300
#define V3_VM_MEM_TRACK_CMD  301
#define V3_VM_MEM_TRACK_SNAP 302

#define V3_VM_HOST_DEV_CONNECT 10245

#define V3_VM_KSTREAM_USER_CONNECT 11245


struct v3_guest_img {
    unsigned long long size;
    void * guest_data;
    char name[128];
} __attribute__((packed));

typedef enum { PREALLOCATED=0,         // user space-allocated (e.g. hot remove)
	      REQUESTED,               // kernel will attempt allocation (anywhere)
	      REQUESTED32,             // kernel will attempt allocation (<4GB)
} v3_mem_region_type_t;

struct v3_mem_region {
    v3_mem_region_type_t type;         // 
    int                  node;         // numa node for REQUESTED (-1 = any)
    unsigned long long   base_addr;    // region start (hpa) for PREALLOCATED
    unsigned long long   num_pages;    // size for PREALLOCATED or request size for REQUESTED
                                       // should be power of 2 and > V3_CONFIG_MEM_BLOCK
} __attribute__((packed));

struct v3_debug_cmd {
    unsigned int core; 
    unsigned int cmd;
} __attribute__((packed));

struct v3_core_move_cmd {
    unsigned short vcore_id;
    unsigned short pcore_id;
} __attribute__((packed));

struct v3_mem_move_cmd{
    unsigned long long gpa;
    unsigned short     pcore_id;
} __attribute__((packed));

struct v3_reset_cmd {
#define V3_RESET_VM_ALL    0
#define V3_RESET_VM_HRT    1
#define V3_RESET_VM_ROS    2
#define V3_RESET_VM_CORE_RANGE  3
    unsigned int type;
    unsigned int first_core;  // for CORE_RANGE
    unsigned int num_cores;   // for CORE_RANGE
} __attribute__((packed));
    
struct v3_chkpt_info {
    char store[128];
    char url[256]; /* This might need to be bigger... */
    unsigned long long opts;
#define V3_CHKPT_OPT_NONE         0
#define V3_CHKPT_OPT_SKIP_MEM     1  // don't write memory to store
#define V3_CHKPT_OPT_SKIP_DEVS    2  // don't write devices to store
#define V3_CHKPT_OPT_SKIP_CORES   4  // don't write core arch ind data to store
#define V3_CHKPT_OPT_SKIP_ARCHDEP 8  // don't write core arch dep data to store
} __attribute__((packed));


struct v3_hw_pci_dev {
    char name[128];
    unsigned int bus;
    unsigned int dev;
    unsigned int func;
} __attribute__((packed));

struct v3_user_pci_dev {
    char name[128];
    unsigned short vendor_id;
    unsigned short dev_id;
} __attribute__((packed));



void * trace_malloc(size_t size, gfp_t flags);
void trace_free(const void * objp);


struct v3_guest {
    void * v3_ctx;

    void * img; 
    u32 img_size;

    char name[128];


    struct rb_root vm_ctrls;
    struct list_head exts;

    dev_t vm_dev; 
    struct cdev cdev;
};

// For now MAX_VMS must be a multiple of 8
// This is due to the minor number bitmap
#define MAX_VMS 32



int palacios_vmm_init( char *options );
int palacios_vmm_exit( void );


// This is how a component finds the proc dir we are using for global state
struct proc_dir_entry *palacios_get_procdir(void);

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
void *palacios_create_and_start_kernel_thread(int (*fn)(void * arg), void *arg, char *thread_name);
void *palacios_create_thread_on_cpu(int cpu_id, int (*fn)(void * arg), void *arg, char *thread_name);
void  palacios_start_thread(void *thread_ptr);
void *palacios_creeate_and_start_thread_on_cpu(int cpu_id, int (*fn)(void * arg), void *arg, char *thread_name);
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
#define ERROR(fmt, args...) printk((KERN_ERR "palacios (pcore %u) %s(%d): " fmt), palacios_get_cpu(), __FILE__, __LINE__, ##args)
#define WARNING(fmt, args...) printk((KERN_WARNING "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)
#define NOTICE(fmt, args...) printk((KERN_NOTICE "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)
#define INFO(fmt, args...) printk((KERN_INFO "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)
#define DEBUG(fmt, args...) printk((KERN_DEBUG "palacios (pcore %u): " fmt), palacios_get_cpu(), ##args)


#endif
