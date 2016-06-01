#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/printk.h>
#include <nautilus/cpu.h>
#include <nautilus/mm.h>
#include <nautilus/vc.h>
#include <dev/timer.h>
#include <dev/apic.h>

#include <palacios/vmm.h>

#include "palacios.h"
#include "console.h"


/*
  This is a simple proof-of-concept implementation of the Palacios
  host interface on top of Nautilus.   It is sufficient to allow
  us to boot a guest OS running Linux.   A few things to note:

  - Nautilus currently has a grand-unified allocator designed to help
    support parallel run-time integration.   All of alloc/valloc/page 
    allocation are built on top of that. 
  - For page allocation, constraints, NUMA, and filter expressions are
    ignored.
  - thread migration is not supported currently
  - hooking of host interrupts is not supported currently.
  - Palacios can sleep, yield, wakeup, etc, but be aware
    that Nautilus threads operate differently than those of
    a traditional kernel.

  Usage:
  - Do Nautilus regular startup to bring all cores to idle
  - From a kernel thread, ideally the init thread on core 0, 
    do palacios_vmm_init(memory_size_bytes,options)
  - You can now use the Palacios v3_* functions, which are
    not wrapped here.
  - You need to keep the Nautilus VM state in sync with
    the Palacios VM state.   The protocol for this is:
        1. before doing a VM creation, call 
              palacios_inform_new_vm_pre(name)
           this will also select the new vm for
           the creation and going forward
           then, once v3_create is done, call
              palacios_inform_new_vm_post(name, vm)
        2. during execution, whenever you want to
           manage a different VM, call 
              palacios_inform_select_vm(vm) 
           or 
              palacios_inform_select_vm_by_name(name)
           It is OK to to select repeatedly, etc.
        3. after doing a VM free, call
              palacios_inform_free_vm(name)
           or
              palacios_inform_free_selected_vm()
  - After you are done, do a palacios_vmm_deinit();

*/

// The following can be used to track memory bugs
// zero memory after allocation (now applies to valloc and page alloc as well)
#define ALLOC_ZERO_MEM 1
// pad allocations by this many bytes on both ends of block (heap only)
#define ALLOC_PAD       0
#define MAX_THREAD_NAME 32

int run_nk_thread = 0;

static struct nk_vm_state vms[NR_VMS];

static struct nk_vm_state *selected_vm;

static struct v3_vm_info * irq_to_guest_map[256];

static unsigned int cpu_khz=-1;

static char *print_buffer[NR_CPUS];

static void deinit_print_buffers(void)
{
    int i;

    for (i=0;i<NR_CPUS;i++) {
	if (print_buffer[i]) { 
	    palacios_free(print_buffer[i]);
	    print_buffer[i]=0;
	}
    }
}

static int init_print_buffers(void)
{
    int i;
    
    memset(print_buffer,0,sizeof(char*)*NR_CPUS);

    for (i=0;i<NR_CPUS;i++) { 
	print_buffer[i] = palacios_alloc(V3_PRINTK_BUF_SIZE);
	if (!print_buffer[i]) { 
	    ERROR("Cannot allocate print buffer for cpu %d\n",i);
	    deinit_print_buffers();
	    return -1;
	}
	memset(print_buffer[i],0,V3_PRINTK_BUF_SIZE);
    }

    
    return 0;

}


 
/**
 * Prints a message to the console.
 */
void palacios_print_scoped(void * vm, int vcore, const char *fmt, ...) 
{

  va_list ap;
  unsigned int cpu = palacios_get_cpu();
  char *buf = cpu < NR_CPUS ? print_buffer[cpu] : 0;

  if (!buf) { 
      INFO_PRINT("palacios (pcore %u): output skipped - no allocated buffer\n",cpu);
      return;
  } 


  va_start(ap, fmt);
  vsnprintf(buf,V3_PRINTK_BUF_SIZE, fmt, ap);
  va_end(ap);

  if (vm) {
    if (vcore>=0) { 
      INFO_PRINT("palacios (pcore %u vm %s vcore %u): %s",
		 cpu,
		 "some_guest",
		 vcore,
		 buf);
    } else {
      INFO_PRINT(KERN_INFO "palacios (pcore %u vm %s): %s",
		 cpu,
		 "some_guest",
		 buf);
    }
  } else {
    INFO_PRINT(KERN_INFO "palacios (pcore %u): %s",
	       cpu,
	       buf);
  }
    
  return;
}



/*
 * Allocates a contiguous region of pages of the requested size.
 * Returns the physical address of the first page in the region.
 */
void *palacios_allocate_pages(int num_pages, unsigned int alignment, int node_id, int (*filter_func)(void *paddr, void *filter_state), void *filter_state) 
{
    void * pg_addr = NULL;

    if (num_pages<=0) { 
	ERROR("ALERT ALERT Attempt to allocate zero or fewer pages (%d pages, alignment %d, node %d, filter_func %p, filter_state %p)\n",num_pages, alignment, node_id, filter_func, filter_state);
	return NULL;
    }

    // malloc currently guarantees alignment to the size of 
    // the allocation
    pg_addr = (void *)malloc(num_pages*4096);

    if (!pg_addr) { 
	ERROR("ALERT ALERT  Page allocation has FAILED Warning (%d pages, alignment %d, node %d, filter_func %p, filter_state %p)\n",num_pages, alignment, node_id, filter_func, filter_state);
	return NULL;
    }
    
    if ((uint64_t)pg_addr & 0xfff) { 
      ERROR("ALERT ALERT Page allocation has surprise offset\n");
      return NULL;
    }

#if ALLOC_ZERO_MEM
    memset(pg_addr,0,num_pages*4096);
#endif

    //INFO("allocpages: %p (%llu pages) alignment=%u\n", pg_addr, num_pages, alignment);

    return pg_addr;
}


/**
 * Frees a page previously allocated via palacios_allocate_page().
 * Note that palacios_allocate_page() can allocate multiple pages with
 * a single call while palacios_free_page() only frees a single page.
 */

void palacios_free_pages(void * page_paddr, int num_pages) {
    if (!page_paddr) { 
	ERROR("Ignoring free pages: 0x%p (0x%lx)for %d pages\n", page_paddr, (uintptr_t)page_paddr, num_pages);
	return;
    }
    free(page_paddr);

    INFO("freepages: %p (%llu pages) alignment=%u\n", page_paddr, num_pages);
}


void *
palacios_alloc_extended(unsigned int size, unsigned int flags, int node) {
    void * addr = NULL;

    if (size==0) { 
      ERROR("ALERT ALERT attempt to kmalloc zero bytes rejected\n");
      return NULL;
    }

    if (node==-1) { 
	addr = malloc(size+2*ALLOC_PAD);
    } else {
	// currently no numa-zone specific kmalloc
	addr = malloc(size+2*ALLOC_PAD);
    }

    if (!addr) { 
       ERROR("ALERT ALERT  kmalloc has FAILED FAILED FAILED\n");
       return NULL;
    }	

#if ALLOC_ZERO_MEM
    memset(addr,0,size+2*ALLOC_PAD);
#endif

    //INFO("malloc: 0x%p (%llu bytes)\n",addr+ALLOC_PAD,size);

    return addr+ALLOC_PAD;
}

void *
palacios_valloc(unsigned int size)
{
    void * addr = NULL;

    if (size==0) { 
      ERROR("ALERT ALERT attempt to vmalloc zero bytes rejected\n");
      return NULL;
    }

    // currently no vmalloc
    addr = malloc(size);

    if (!addr) {
       ERROR("ALERT ALERT  vmalloc has FAILED FAILED FAILED\n");
       return NULL;
    }	

#if ALLOC_ZERO_MEM
    memset(addr,0,size);
#endif

    //INFO("valloc: 0x%p (%llu bytes)\n",addr,size);

    return addr;
}

void palacios_vfree(void *p)
{
  if (!p) { 
      ERROR("Ignoring vfree: 0x%p\n",p);
      return;
  }
  // no vfree currently
  free(p);

  //INFO("vfree: 0x%p\n",p);
}

/**
 * Allocates 'size' bytes of kernel memory.
 * Returns the kernel virtual address of the memory allocated.
 */
void *
palacios_alloc(unsigned int size) 
{
    return palacios_alloc_extended(size,0,-1);
}

/**
 * Frees memory that was previously allocated by palacios_alloc().
 */
void
palacios_free(void *addr)
{
    return;
    if (!addr) {
	ERROR("Ignoring free : 0x%p\n", addr);
	return;
    }
    // no kfree
    free(addr-ALLOC_PAD);
    //INFO("free: %p\n",addr-ALLOC_PAD);
}

/**
 * Converts a kernel virtual address to the corresponding physical address.
 */
void *
palacios_vaddr_to_paddr(
	void *			vaddr
)
{
    return vaddr; // our memory mapping is identity

}

/**
 * Converts a physical address to the corresponding kernel virtual address.
 */
void *
palacios_paddr_to_vaddr(
	void *			paddr
)
{
    return paddr; // our memory mapping is identity
}

/**
 * Runs a function on the specified CPU.
 */
void 
palacios_xcall(
	int			cpu_id, 
	void			(*fn)(void *arg),
	void *			arg
)
{

    smp_xcall(cpu_id,fn,arg,1);

    return;
}



struct nautilus_thread_arg {
    int (*fn)(void * arg);
    void *arg; 
    char name[MAX_THREAD_NAME];
};

static void nautilus_thread_target(void * in, void ** out) 
{
    struct nautilus_thread_arg * thread_info = (struct nautilus_thread_arg *)in;
    int ret;

    ret = thread_info->fn(thread_info->arg);

    INFO("Palacios Thread (%s) EXITING with return code %d\n", thread_info->name, ret);

    palacios_free(thread_info); 
}

/**
 * Creates a kernel thread.
 */
void *
palacios_create_and_start_kernel_thread(
	int (*fn)		(void * arg),
	void *			arg,
	char *			thread_name,
	v3_resource_control_t   *rctl) 
{
    
    struct nautilus_thread_arg * thread_info = palacios_alloc(sizeof(struct nautilus_thread_arg));
    nk_thread_id_t tid = 0;
    
    if (!thread_info) { 
	ERROR("ALERT ALERT Unable to allocate thread\n");
	return NULL;
    }
    
    thread_info->fn = fn;
    thread_info->arg = arg;
    strncpy(thread_info->name,thread_name,MAX_THREAD_NAME);
    thread_info->name[MAX_THREAD_NAME-1] =0;
    
    nk_thread_start(nautilus_thread_target, thread_info, 0, 0, 0, &tid, CPU_ANY);

    return tid;
}


/**
 * Starts a kernel thread on the specified CPU.
 */
void * 
palacios_create_thread_on_cpu(int cpu_id,
			      int (*fn)(void * arg), 
			      void * arg, 
			      char * thread_name,
			      v3_resource_control_t *rctl) 
{
    nk_thread_id_t newtid;
    nk_thread_t * newthread = NULL;
    struct nautilus_thread_arg * thread_info = palacios_alloc(sizeof(struct nautilus_thread_arg));
    
    thread_info->fn = fn;
    thread_info->arg = arg;
    strncpy(thread_info->name, thread_name, MAX_THREAD_NAME);
    thread_info->name[MAX_THREAD_NAME-1] = 0;
    
    //INFO("CREATING A THREAD ON CPU ID: %d\n", cpu_id);
    
    if (nk_thread_create(nautilus_thread_target, thread_info, 0, 0, 0, &newtid, cpu_id) < 0) {
	ERROR("COULD NOT CREATE THREAD\n");
	return NULL;
    }
    //INFO("newtid: %lu\n", newtid);
    
    return newtid;
}

void
palacios_start_thread(void * th)
{
    nk_thread_run(th);
}

/*
  Convenience wrapper
*/
void * 
palacios_create_and_start_thread_on_cpu(int cpu_id,
					int (*fn)(void * arg), 
					void * arg, 
					char * thread_name,
                                        v3_resource_control_t *rctl ) 
{

    nk_thread_id_t tid;

    struct nautilus_thread_arg * thread_info = palacios_alloc(sizeof(struct nautilus_thread_arg));

    if (!thread_info) { 
	ERROR("ALERT ALERT Unable to allocate thread to start on cpu\n");
	return NULL;
    }

    thread_info->fn = fn;
    thread_info->arg = arg;
    strncpy(thread_info->name,thread_name,MAX_THREAD_NAME);
    thread_info->name[MAX_THREAD_NAME-1] =0;

    nk_thread_start(nautilus_thread_target, thread_info, 0, 0, 0,&tid,cpu_id); //

    return tid;
}



/**
 * Rebind a kernel thread to the specified CPU
 * The thread will be running on target CPU on return
 * non-zero return means failure
 */
int
palacios_move_thread_to_cpu(int new_cpu_id, 
			    void * thread_ptr) 
{

    INFO("Moving thread (%p) to cpu %d\n", thread_ptr, new_cpu_id);
    ERROR("NOT CURRENTLY SUPPORTED\n");
    return -1;
}


/**
 * Returns the CPU ID that the caller is running on.
 */
unsigned int 
palacios_get_cpu(void) 
{
    return  my_cpu_id(); 
}

static void
palacios_interrupt_cpu(	struct v3_vm_info *	vm, 
			int			cpu_id, 
			int                     vector)
{
  apic_ipi(per_cpu_get(apic),cpu_id,vector); // find out apic_dev * and cpu to apic id mapping 
}

struct pt_regs;


/**
 * Dispatches an interrupt to Palacios for handling.
 */
static void
palacios_dispatch_interrupt( int vector, void * dev, struct pt_regs * regs ) {
    struct v3_interrupt intr = {
	.irq		= vector,
	.error		= 0, //regs->orig_ax, /* TODO fix this */
	.should_ack	= 1,
    };
    
    if (irq_to_guest_map[vector]) {
	v3_deliver_irq(irq_to_guest_map[vector], &intr);
    }
    
}

/**
 * Instructs the kernel to forward the specified IRQ to Palacios.
 */
static int
palacios_hook_interrupt(struct v3_vm_info *	vm,
			unsigned int		vector ) 
{
    ERROR("UNSUPPORTED: PALACIOS_HOOK_INTERRUPT\n");
    return -1;
}


/**
 * Acknowledges an interrupt.
 */
static int
palacios_ack_interrupt(
	int			vector
) 
{
    ERROR("UNSUPPORTED: PALACIOS_ACK_INTERRUPT\n");
    return -1;
}
  
/**
 * Returns the CPU frequency in kilohertz.
 */
unsigned int
palacios_get_cpu_khz(void) 
{
    if (cpu_khz==-1) { 
	uint32_t cpu = (uint32_t)my_cpu_id();
	
	cpu_khz = nk_detect_cpu_freq(cpu);
	if (cpu_khz==-1) {
	    INFO("CANNOT GET THE CPU FREQUENCY. FAKING TO 1000000\n");
	    cpu_khz=1000000;
	}
    }
    INFO("Nautilus frequency at %u KHz\n",cpu_khz);
    return cpu_khz;
}

/**
 * Yield the CPU so other host OS tasks can run.
 * This will return immediately if there is no other thread that is runnable
 * And there is no real bound on how long it will yield
 */
void
palacios_yield_cpu(void)
{
    nk_yield();
    return;
}

/**
 * Yield the CPU so other host OS tasks can run.
 * Given now immediately if there is no other thread that is runnable
 * And there is no real bound on how long it will yield
 */
void palacios_sleep_cpu(unsigned int us)
{
    // sleep not supported on Nautilus
    // just yield
    nk_yield();
    udelay(us);
}

void palacios_wakeup_cpu(void *thread)
{
    // threads never go to sleep, so shouldn't happen
    ERROR("ERROR ERROR: WAKEUP_CPU CALLED. THREADS ARE NEVER ASLEEP");
    return;
}

/**
 * Allocates a mutex.
 * Returns NULL on failure.
 */
void *
palacios_mutex_alloc(void)
{
    spinlock_t *lock = palacios_alloc(sizeof(spinlock_t));
    
    if (lock) {
        spinlock_init(lock);
    } else {
	ERROR("ALERT ALERT Unable to allocate lock\n");
	return NULL;
    }
    
    return lock;
}

void palacios_mutex_init(void *mutex)
{
    spinlock_t *lock = (spinlock_t*)mutex;
    
    if (lock) {
	spinlock_init(lock);
	LOCKCHECK_ALLOC(lock);
    }
    
}

void palacios_mutex_deinit(void *mutex)
{
    spinlock_t *lock = (spinlock_t*)mutex;
  
    if (lock) {
	spinlock_deinit(lock);
	LOCKCHECK_FREE(lock);
    }
}


/**
 * Frees a mutex.
 */
void
palacios_mutex_free(void * mutex) {
    palacios_free(mutex);
    LOCKCHECK_FREE(mutex);
}

/**
 * Locks a mutex.
 */
void 
palacios_mutex_lock(void * mutex, int must_spin) {
    LOCKCHECK_LOCK_PRE(mutex);
    spin_lock((spinlock_t *)mutex);
    LOCKCHECK_LOCK_POST(mutex);
}


/**
 * Locks a mutex, disabling interrupts on this core
 */
void *
palacios_mutex_lock_irqsave(void * mutex, int must_spin) {
    
    unsigned long flags; 
    
    LOCKCHECK_LOCK_IRQSAVE_PRE(mutex,flags);
    flags = spin_lock_irq_save((spinlock_t *)mutex);
    LOCKCHECK_LOCK_IRQSAVE_POST(mutex,flags);

    //INFO("lock irqsave flags=%lu\n",flags);
    return (void *)flags;
}


/**
 * Unlocks a mutex.
 */
void 
palacios_mutex_unlock(
	void *			mutex
) 
{
    LOCKCHECK_UNLOCK_PRE(mutex);
    spin_unlock((spinlock_t *)mutex);
    LOCKCHECK_UNLOCK_POST(mutex);
}


/**
 * Unlocks a mutex and restores previous interrupt state on this core
 */
void 
palacios_mutex_unlock_irqrestore(void *mutex, void *flags)
{
    //INFO("unlock irqrestore flags=%lu\n",(unsigned long)flags);
    LOCKCHECK_UNLOCK_IRQRESTORE_PRE(mutex,(unsigned long)flags);
    // This is correct, flags is opaque
    spin_unlock_irq_restore((spinlock_t *)mutex,(uint8_t) (unsigned long)flags);
    LOCKCHECK_UNLOCK_IRQRESTORE_POST(mutex,(unsigned long)flags);
}


/**
 * Structure used by the Palacios hypervisor to interface with the host kernel.
 */
static struct v3_os_hooks palacios_os_hooks = {
	.print			    = palacios_print_scoped, 
	.allocate_pages		    = palacios_allocate_pages,  
	.free_pages		    = palacios_free_pages, 
	.vmalloc		    = palacios_valloc, 
	.vfree			    = palacios_vfree, 
	.malloc			    = palacios_alloc, 
	.free			    = palacios_free, 
	.vaddr_to_paddr		    = palacios_vaddr_to_paddr,  
	.paddr_to_vaddr		    = palacios_paddr_to_vaddr,  
	.hook_interrupt		    = palacios_hook_interrupt,  
	.ack_irq		    = palacios_ack_interrupt,  
	.get_cpu_khz		    = palacios_get_cpu_khz, 
	.start_kernel_thread        = palacios_create_and_start_kernel_thread, 
	.yield_cpu		    = palacios_yield_cpu, 
	.sleep_cpu		    = palacios_sleep_cpu, 
	.wakeup_cpu		    = palacios_wakeup_cpu, 
	.mutex_alloc		    = palacios_mutex_alloc, 
	.mutex_free		    = palacios_mutex_free, 
	.mutex_lock		    = palacios_mutex_lock, 
	.mutex_unlock		    = palacios_mutex_unlock, 
	.mutex_lock_irqsave         = palacios_mutex_lock_irqsave,  
	.mutex_unlock_irqrestore    = palacios_mutex_unlock_irqrestore, 
	.get_cpu		    = palacios_get_cpu, 
	.interrupt_cpu		    = palacios_interrupt_cpu, 
	.call_on_cpu		    = palacios_xcall, 
	.create_thread_on_cpu	    = palacios_create_thread_on_cpu, 
	.start_thread		    = palacios_start_thread, 
	.move_thread_to_cpu         = palacios_move_thread_to_cpu, // unsupported
};


int palacios_vmm_init(char * options)
{
    int num_cpus = nautilus_info.sys.num_cpus;
    char * cpu_mask = NULL;

    if (num_cpus > 0) {
	int major = 0;
	int minor = 0;
	int i = 0;

        cpu_mask = palacios_alloc((num_cpus / 8) + 1);
	
	if (!cpu_mask) { 
	    ERROR("Cannot allocate cpu mask\n");
	    return -1;
	}

	memset(cpu_mask, 0, (num_cpus / 8) + 1);
        
        for (i = 0; i < num_cpus; i++) {

            major = i / 8;
            minor = i % 8;
    
            *(cpu_mask + major) |= (0x1 << minor);
        }
    }


    memset(irq_to_guest_map, 0, sizeof(struct v3_vm_info *) * 256);

    memset(vms,0,sizeof(vms));

    if (init_print_buffers()) {
	INFO("Cannot initialize print buffers\n");
	palacios_free(cpu_mask);
	return -1;
    }
    
    INFO("printbuffer init done\n");

    INFO("NR_CPU: %d\n", NR_CPUS);

    INFO("palacios_init starting - calling init_v3\n");

    INFO("calling init_v3 = %p\n", Init_V3);

    INFO("num_cpus: %d\ncpu_mask: %x\noptions: %s\n", num_cpus, *cpu_mask, options);

    Init_V3(&palacios_os_hooks, cpu_mask, num_cpus, options);

    INFO("init_v3 done\n");

#ifdef V3_CONFIG_CONSOLE
    INFO("Initializing console\n");
    nautilus_console_init();
#endif


    return 0;

}


int palacios_vmm_exit( void ) 
{

#ifdef V3_CONFIG_CONSOLE
    nautilus_console_deinit();
#endif

    Shutdown_V3();

    INFO("palacios shutdown complete\n");

    deinit_print_buffers();

    return 0;
}


void palacios_inform_new_vm_pre(char *name)
{
  int i;
  for (i=0;i<NR_VMS;i++) { 
    if (!vms[i].name[0]) {
      strncpy(vms[i].name,name,MAX_VM_NAME);
      selected_vm = &vms[i];
      return;
    }
  }
}

void palacios_inform_new_vm_post(char *name, struct v3_vm_info *vm)
{
  struct nk_vm_state *n = palacios_find_vm_by_name(name);

  if (n) { 
    n->vm = vm;
    INFO("Registered VM %p with name %s, node=%p, selected VM=%p\n",
	 vm, n->name, n, selected_vm);
  } else {
    ERROR("Cannot find VM with name \"%s\"\n",name);
  }
}

void palacios_inform_free_vm(char *name) 
{
  struct nk_vm_state *n = palacios_find_vm_by_name(name);

  if (n==selected_vm) { 
    selected_vm = 0;
  }

  if (n) { 
    n->vm = 0;
    n->vc = 0;
    n->name[0] = 0;
  }
  
}

void palacios_inform_free_selected_vm()
{
  struct nk_vm_state *n = selected_vm;

  selected_vm = 0;

  if (n) { 
    n->vm = 0;
    n->vc = 0;
    n->name[0] = 0;
  }
}


struct nk_vm_state *palacios_find_vm_by_name(char *name)
{
  int i;
  for (i=0;i<NR_VMS;i++) { 
    if (!strncmp(vms[i].name,name,MAX_VM_NAME)) {
      return &vms[i];
    }
  }
  return 0;
}

struct nk_vm_state *palacios_find_vm(struct v3_vm_info *vm)
{
  int i;
  for (i=0;i<NR_VMS;i++) { 
    if (vms[i].vm == vm) { 
      return &vms[i];
    }
  }
  return 0;
}

void palacios_select_vm(struct v3_vm_info *vm)
{
  struct nk_vm_state *n = palacios_find_vm(vm);
  if (n) {
    selected_vm = n;
  }
}

void palacios_select_vm_by_name(char *name)
{
  struct nk_vm_state *n = palacios_find_vm_by_name(name);
  if (n) {
    selected_vm = n;
  }
}

struct nk_vm_state *palacios_get_selected_vm()
{
  return selected_vm;
}

