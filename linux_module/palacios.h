#ifndef _PALACIOS_H
#define _PALACIOS_H

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>


/* Global Control IOCTLs */
#define V3_CREATE_GUEST 12
#define V3_FREE_GUEST 13

#define V3_ADD_MEMORY 50
#define V3_ADD_PCI_HW_DEV 55
#define V3_ADD_PCI_USER_DEV 56

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

#define V3_VM_FB_INPUT 257
#define V3_VM_FB_QUERY 258

#define V3_VM_HOST_DEV_CONNECT 10245

#define V3_VM_KSTREAM_USER_CONNECT 11245


struct v3_guest_img {
    unsigned long long size;
    void * guest_data;
    char name[128];
} __attribute__((packed));

struct v3_mem_region {
    unsigned long long base_addr;
    unsigned long long num_pages;
} __attribute__((packed));

struct v3_debug_cmd {
    unsigned int core; 
    unsigned int cmd;
} __attribute__((packed));

struct v3_core_move_cmd {
    unsigned short vcore_id;
    unsigned short pcore_id;
} __attribute__((packed));

struct v3_chkpt_info {
    char store[128];
    char url[256]; /* This might need to be bigger... */
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



int palacios_vmm_init( void );
int palacios_vmm_exit( void );


// This is how a component finds the proc dir we are using for global state
struct proc_dir_entry *palacios_get_procdir(void);

// Selected exported stubs, for use in other palacios components, like vnet
// The idea is that everything uses the same stubs
void  palacios_print(const char *fmt, ...);
void *palacios_allocate_pages(int num_pages, unsigned int alignment);
void  palacios_free_pages(void *page_addr, int num_pages);
void *palacios_alloc(unsigned int size);
void *palacios_alloc_extended(unsigned int size, unsigned int flags);
void  palacios_free(void *);
void *palacios_vaddr_to_paddr(void *vaddr);
void *palacios_paddr_to_vaddr(void *paddr);
void *palacios_start_kernel_thread(int (*fn)(void * arg), void *arg, char *thread_name);
void *palacios_start_thread_on_cpu(int cpu_id, int (*fn)(void * arg), void *arg, char *thread_name);
int   palacios_move_thread_to_cpu(int new_cpu_id, void *thread_ptr);
void  palacios_yield_cpu(void);
void  palacios_yield_cpu_timed(unsigned int us);
unsigned int palacios_get_cpu(void);
unsigned int palacios_get_cpu_khz(void);
void *palacios_mutex_alloc(void);
void  palacios_mutex_free(void *mutex);
void  palacios_mutex_lock(void *mutex, int must_spin);
void  palacios_mutex_unlock(void *mutex);
void *palacios_mutex_lock_irqsave(void *mutex, int must_spin);
void  palacios_mutex_unlock_irqrestore(void *mutex, void *flags);



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
