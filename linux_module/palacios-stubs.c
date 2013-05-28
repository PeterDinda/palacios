#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/irq_vectors.h>
#include <asm/io.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>
#include "palacios.h"

#include "mm.h"

#include "memcheck.h"
#include "lockcheck.h"

// The following can be used to track heap bugs
// zero memory after allocation
#define ALLOC_ZERO_MEM 0
// pad allocations by this many bytes on both ends of block
#define ALLOC_PAD      0


u32 pg_allocs = 0;
u32 pg_frees = 0;
u32 mallocs = 0;
u32 frees = 0;
u32 vmallocs = 0;
u32 vfrees = 0;

static struct v3_vm_info * irq_to_guest_map[256];


extern unsigned int cpu_khz;

extern int cpu_list[NR_CPUS];
extern int cpu_list_len;


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

#if !V3_PRINTK_OLD_STYLE_OUTPUT

    for (i=0;i<NR_CPUS;i++) { 
	print_buffer[i] = palacios_alloc(V3_PRINTK_BUF_SIZE);
	if (!print_buffer[i]) { 
	    ERROR("Cannot allocate print buffer for cpu %d\n",i);
	    deinit_print_buffers();
	    return -1;
	}
	memset(print_buffer[i],0,V3_PRINTK_BUF_SIZE);
    }

#endif
    
    return 0;

}
 
/**
 * Prints a message to the console.
 */
void palacios_print_scoped(void * vm, int vcore, const char *fmt, ...) {

#if V3_PRINTK_OLD_STYLE_OUTPUT

  va_list ap;

  va_start(ap, fmt);
  vprintk(fmt, ap);
  va_end(ap);

  return

#else 

  va_list ap;
  char *buf;
  unsigned int cpu = palacios_get_cpu();
  struct v3_guest *guest = (struct v3_guest *)vm;

  buf = print_buffer[cpu];

  if (!buf) { 
      printk(KERN_INFO "palacios (pcore %u): output skipped - no allocated buffer\n",cpu);
      return;
  } 

  va_start(ap, fmt);
  vsnprintf(buf,V3_PRINTK_BUF_SIZE, fmt, ap);
  va_end(ap);

#if V3_PRINTK_CHECK_7BIT
  {
      char c=0;
      int i;
      for (i=0;i<strlen(buf);i++) { 
	  if (buf[i] < 0) {
	      c=buf[i];
	      break;
	  }
      }
      if (c!=0) { 
	  printk(KERN_INFO "palacios (pcore %u): ALERT ALERT 8 BIT CHAR (c=%d) DETECTED\n", cpu,c);
      }
  }
#endif

  if (guest) {
    if (vcore>=0) { 
      printk(KERN_INFO "palacios (pcore %u vm %s vcore %u): %s",
	     cpu,
	     guest->name,
	     vcore,
	     buf);
    } else {
       printk(KERN_INFO "palacios (pcore %u vm %s): %s",
	     cpu,
	     guest->name,
	     buf);
    }
  } else {
    printk(KERN_INFO "palacios (pcore %u): %s",
	   cpu,
	   buf);
  }
    
  return;

#endif

}


/*
 * Allocates a contiguous region of pages of the requested size.
 * Returns the physical address of the first page in the region.
 */
void *palacios_allocate_pages(int num_pages, unsigned int alignment) {
    void * pg_addr = NULL;

    if (num_pages<=0) { 
      ERROR("ALERT ALERT Attempt to allocate zero or fewer pages\n");
      return NULL;
    }

    pg_addr = (void *)alloc_palacios_pgs(num_pages, alignment);

    if (!pg_addr) { 
	ERROR("ALERT ALERT  Page allocation has FAILED Warning\n");
	return NULL;
    }

    pg_allocs += num_pages;

    MEMCHECK_ALLOC_PAGES(pg_addr,num_pages*4096);

    return pg_addr;
}


/**
 * Frees a page previously allocated via palacios_allocate_page().
 * Note that palacios_allocate_page() can allocate multiple pages with
 * a single call while palacios_free_page() only frees a single page.
 */

void palacios_free_pages(void * page_paddr, int num_pages) {
    pg_frees += num_pages;
    free_palacios_pgs((uintptr_t)page_paddr, num_pages);
    MEMCHECK_FREE_PAGES(page_paddr,num_pages*4096);

}


void *
palacios_alloc_extended(unsigned int size, unsigned int flags) {
    void * addr = NULL;

    if (size==0) { 
      // note that modern kernels will respond to a zero byte
      // kmalloc and return the address 0x10...  In Palacios, 
      // we will simply not allow 0 byte allocs at all, of any kind
      ERROR("ALERT ALERT attempt to kmalloc zero bytes rejected\n");
      return NULL;
    }

    addr = kmalloc(size+2*ALLOC_PAD, flags);

    if (!addr) { 
       ERROR("ALERT ALERT  kmalloc has FAILED FAILED FAILED\n");
       return NULL;
    }	

    mallocs++;

#if ALLOC_ZERO_MEM
    memset(addr,0,size+2*ALLOC_PAD);
#endif

    MEMCHECK_KMALLOC(addr,size+2*ALLOC_PAD);

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

    addr = vmalloc(size);

    if (!addr) { 
       ERROR("ALERT ALERT  vmalloc has FAILED FAILED FAILED\n");
       return NULL;
    }	

    vmallocs++;

    MEMCHECK_VMALLOC(addr,size);

    return addr;
}

void palacios_vfree(void *p)
{
  vfree(p);
  vfrees++;
  MEMCHECK_VFREE(p);
}

/**
 * Allocates 'size' bytes of kernel memory.
 * Returns the kernel virtual address of the memory allocated.
 */
void *
palacios_alloc(unsigned int size) {

    // It is very important that this test remains since 
    // this function is used extensively throughout palacios and the linux
    // module, both in places where interrupts are off and where they are on
    // a GFP_KERNEL call, when done with interrupts off can lead to DEADLOCK
    if (irqs_disabled()) {
	return palacios_alloc_extended(size,GFP_ATOMIC);
    } else {
	return palacios_alloc_extended(size,GFP_KERNEL);
    }

}

/**
 * Frees memory that was previously allocated by palacios_alloc().
 */
void
palacios_free(
	void *			addr
)
{
    frees++;
    kfree(addr-ALLOC_PAD);
    MEMCHECK_KFREE(addr-ALLOC_PAD);
}

/**
 * Converts a kernel virtual address to the corresponding physical address.
 */
void *
palacios_vaddr_to_paddr(
	void *			vaddr
)
{
    return (void*) __pa(vaddr);

}

/**
 * Converts a physical address to the corresponding kernel virtual address.
 */
void *
palacios_paddr_to_vaddr(
	void *			paddr
)
{
  return __va(paddr);
}

/**
 * Runs a function on the specified CPU.
 */
static void 
palacios_xcall(
	int			cpu_id, 
	void			(*fn)(void *arg),
	void *			arg
)
{


    // We set wait to 1, but I'm not sure this is necessary
    smp_call_function_single(cpu_id, fn, arg, 1);
    
    return;
}


#define MAX_THREAD_NAME 32

struct lnx_thread_arg {
    int (*fn)(void * arg);
    void * arg;
    char name[MAX_THREAD_NAME];
};

static int lnx_thread_target(void * arg) {
    struct lnx_thread_arg * thread_info = (struct lnx_thread_arg *)arg;
    int ret = 0;
    /*
      INFO("Daemonizing new Palacios thread (name=%s)\n", thread_info->name);

      daemonize(thread_info->name);
      allow_signal(SIGKILL);
    */


    ret = thread_info->fn(thread_info->arg);


    INFO("Palacios Thread (%s) EXITING\n", thread_info->name);

    palacios_free(thread_info);
    // handle cleanup 

    do_exit(ret);
    
    return 0; // should not get here.
}

/**
 * Creates a kernel thread.
 */
void *
palacios_start_kernel_thread(
	int (*fn)		(void * arg),
	void *			arg,
	char *			thread_name) {

    struct lnx_thread_arg * thread_info = palacios_alloc(sizeof(struct lnx_thread_arg));

    if (!thread_info) { 
	ERROR("ALERT ALERT Unable to allocate thread\n");
	return NULL;
    }

    thread_info->fn = fn;
    thread_info->arg = arg;
    strncpy(thread_info->name,thread_name,MAX_THREAD_NAME);
    thread_info->name[MAX_THREAD_NAME-1] =0;

    return kthread_run( lnx_thread_target, thread_info, thread_info->name );
}


/**
 * Starts a kernel thread on the specified CPU.
 */
void * 
palacios_start_thread_on_cpu(int cpu_id, 
			     int (*fn)(void * arg), 
			     void * arg, 
			     char * thread_name ) {
    struct task_struct * thread = NULL;
    struct lnx_thread_arg * thread_info = palacios_alloc(sizeof(struct lnx_thread_arg));

    if (!thread_info) { 
	ERROR("ALERT ALERT Unable to allocate thread to start on cpu\n");
	return NULL;
    }

    thread_info->fn = fn;
    thread_info->arg = arg;
    strncpy(thread_info->name,thread_name,MAX_THREAD_NAME);
    thread_info->name[MAX_THREAD_NAME-1] =0;

    thread = kthread_create( lnx_thread_target, thread_info, thread_info->name );

    if (IS_ERR(thread)) {
	WARNING("Palacios error creating thread: %s\n", thread_info->name);
	palacios_free(thread_info);
	return NULL;
    }

    if (set_cpus_allowed_ptr(thread, cpumask_of(cpu_id)) != 0) {
	WARNING("Attempt to start thread on disallowed CPU\n");
	kthread_stop(thread);
	palacios_free(thread_info);
	return NULL;
    }

    wake_up_process(thread);

    return thread;
}


/**
 * Rebind a kernel thread to the specified CPU
 * The thread will be running on target CPU on return
 * non-zero return means failure
 */
int
palacios_move_thread_to_cpu(int new_cpu_id, 
			    void * thread_ptr) {
    struct task_struct * thread = (struct task_struct *)thread_ptr;

    INFO("Moving thread (%p) to cpu %d\n", thread, new_cpu_id);

    if (thread == NULL) {
	thread = current;
    }

    /*
     * Bind to the specified CPU.  When this call returns,
     * the thread should be running on the target CPU.
     */
    return set_cpus_allowed_ptr(thread, cpumask_of(new_cpu_id));
}


/**
 * Returns the CPU ID that the caller is running on.
 */
unsigned int 
palacios_get_cpu(void) 
{

    /* We want to call smp_processor_id()
     * But this is not safe if kernel preemption is possible 
     * We need to ensure that the palacios threads are bound to a give cpu
     */

    unsigned int cpu_id = get_cpu(); 
    put_cpu();
    return cpu_id;
}

/**
 * Interrupts the physical CPU corresponding to the specified logical guest cpu.
 *
 * NOTE: 
 * This is dependent on the implementation of xcall_reschedule().  Currently
 * xcall_reschedule does not explicitly call schedule() on the destination CPU,
 * but instead relies on the return to user space to handle it. Because
 * palacios is a kernel thread schedule will not be called, which is correct.
 * If it ever changes to induce side effects, we'll need to figure something
 * else out...
 */

#include <asm/apic.h>

static void
palacios_interrupt_cpu(
	struct v3_vm_info *	vm, 
	int			cpu_id, 
	int                     vector
)
{
    if (vector == 0) {
	smp_send_reschedule(cpu_id);
    } else {
	apic->send_IPI_mask(cpumask_of(cpu_id), vector);
    }
    return;
}

/**
 * Dispatches an interrupt to Palacios for handling.
 */
static void
palacios_dispatch_interrupt( int vector, void * dev, struct pt_regs * regs ) {
    struct v3_interrupt intr = {
	.irq		= vector,
	.error		= regs->orig_ax,
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
			unsigned int		vector ) {
    INFO("hooking vector %d\n", vector);  	

    if (irq_to_guest_map[vector]) {
	WARNING(
	       "%s: Interrupt vector %u is already hooked.\n",
	       __func__, vector);
	return -1;
    }

    DEBUG(
	   "%s: Hooking interrupt vector %u to vm %p.\n",
	   __func__, vector, vm);

    irq_to_guest_map[vector] = vm;

    /*
     * NOTE: Normally PCI devices are supposed to be level sensitive,
     *       but we need them to be edge sensitive so that they are
     *       properly latched by Palacios.  Leaving them as level
     *       sensitive would lead to an interrupt storm.
     */
    //ioapic_set_trigger_for_vector(vector, ioapic_edge_sensitive);
    
    //set_idtvec_handler(vector, palacios_dispatch_interrupt);
    if (vector < 32) {
	ERROR("unexpected vector for hooking\n");
	return -1;
    } else {
	int device_id = 0;		
	
	int flag = 0;
	int error;
		
	DEBUG("hooking vector: %d\n", vector);		

	if (vector == 32) {
	    flag = IRQF_TIMER;
	} else {
	    flag = IRQF_SHARED;
	}

	error = request_irq((vector - 32),
			    (void *)palacios_dispatch_interrupt,
			    flag,
			    "interrupt_for_palacios",
			    &device_id);
	
	if (error) {
	    ERROR("error code for request_irq is %d\n", error);
	    ERROR("request vector %d failed", vector);
	    return -1;
	}
    }
	
    return 0;
}



/**
 * Acknowledges an interrupt.
 */
static int
palacios_ack_interrupt(
	int			vector
) 
{
  ack_APIC_irq(); 
  DEBUG("Pretending to ack interrupt, vector=%d\n", vector);
  return 0;
}
  
/**
 * Returns the CPU frequency in kilohertz.
 */
unsigned int
palacios_get_cpu_khz(void) 
{
    INFO("cpu_khz is %u\n", cpu_khz);

    if (cpu_khz == 0) { 
	INFO("faking cpu_khz to 1000000\n");
	return 1000000;
    } else {
	return cpu_khz;
    }
  //return 1000000;
}

/**
 * Yield the CPU so other host OS tasks can run.
 * This will return immediately if there is no other thread that is runnable
 * And there is no real bound on how long it will yield
 */
void
palacios_yield_cpu(void)
{
    schedule();
    return;
}

/**
 * Yield the CPU so other host OS tasks can run.
 * Given now immediately if there is no other thread that is runnable
 * And there is no real bound on how long it will yield
 */
void palacios_sleep_cpu(unsigned int us)
{

    set_current_state(TASK_INTERRUPTIBLE);
    if (us) {
        unsigned int uspj = 1000000U/HZ;
        unsigned int jiffies = us/uspj + ((us%uspj) !=0);  // ceiling 
        schedule_timeout(jiffies);
    } else {
        schedule();
    }
    return;
}

void palacios_wakeup_cpu(void *thread)
{
    wake_up_process(thread);
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
	spin_lock_init(lock);
	LOCKCHECK_ALLOC(lock);
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
    spin_lock_init(lock);
    LOCKCHECK_ALLOC(lock);
  }
}

void palacios_mutex_deinit(void *mutex)
{
  spinlock_t *lock = (spinlock_t*)mutex;
  
  if (lock) {
    // no actual spin_lock_deinit on linux
    // our purpose here is to drive the lock checker
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
    spin_lock((spinlock_t *)mutex);
    LOCKCHECK_LOCK(mutex);
}


/**
 * Locks a mutex, disabling interrupts on this core
 */
void *
palacios_mutex_lock_irqsave(void * mutex, int must_spin) {
    
    unsigned long flags; 
    
    spin_lock_irqsave((spinlock_t *)mutex,flags);
    LOCKCHECK_LOCK_IRQSAVE(mutex,flags);

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
    spin_unlock((spinlock_t *)mutex);
    LOCKCHECK_UNLOCK(mutex);
}


/**
 * Unlocks a mutex and restores previous interrupt state on this core
 */
void 
palacios_mutex_unlock_irqrestore(void *mutex, void *flags)
{
    // This is correct, flags is opaque
    spin_unlock_irqrestore((spinlock_t *)mutex,(unsigned long)flags);
    LOCKCHECK_UNLOCK_IRQRESTORE(mutex,(unsigned long)flags);
}

/**
 * Structure used by the Palacios hypervisor to interface with the host kernel.
 */
static struct v3_os_hooks palacios_os_hooks = {
	.print			= palacios_print_scoped,
	.allocate_pages		= palacios_allocate_pages,
	.free_pages		= palacios_free_pages,
	.malloc			= palacios_alloc,
	.free			= palacios_free,
	.vaddr_to_paddr		= palacios_vaddr_to_paddr,
	.paddr_to_vaddr		= palacios_paddr_to_vaddr,
	.hook_interrupt		= palacios_hook_interrupt,
	.ack_irq		= palacios_ack_interrupt,
	.get_cpu_khz		= palacios_get_cpu_khz,
	.start_kernel_thread    = palacios_start_kernel_thread,
	.yield_cpu		= palacios_yield_cpu,
	.sleep_cpu		= palacios_sleep_cpu,
	.wakeup_cpu		= palacios_wakeup_cpu,
	.mutex_alloc		= palacios_mutex_alloc,
	.mutex_free		= palacios_mutex_free,
	.mutex_lock		= palacios_mutex_lock, 
	.mutex_unlock		= palacios_mutex_unlock,
	.mutex_lock_irqsave     = palacios_mutex_lock_irqsave, 
	.mutex_unlock_irqrestore= palacios_mutex_unlock_irqrestore,
	.get_cpu		= palacios_get_cpu,
	.interrupt_cpu		= palacios_interrupt_cpu,
	.call_on_cpu		= palacios_xcall,
	.start_thread_on_cpu	= palacios_start_thread_on_cpu,
	.move_thread_to_cpu     = palacios_move_thread_to_cpu,
};




int palacios_vmm_init( char *options )
{
    int num_cpus = num_online_cpus();
    char * cpu_mask = NULL;

    if (cpu_list_len > 0) {
	int major = 0;
	int minor = 0;
	int i = 0;

        cpu_mask = palacios_alloc((num_cpus / 8) + 1);

	if (!cpu_mask) { 
	    ERROR("Cannot allocate cpu mask\n");
	    return -1;
	}

	memset(cpu_mask, 0, (num_cpus / 8) + 1);
        
        for (i = 0; i < cpu_list_len; i++) {
	    if (cpu_list[i] >= num_cpus) {
		WARNING("CPU (%d) exceeds number of available CPUs. Ignoring...\n", cpu_list[i]);
		continue;
	    }

            major = cpu_list[i] / 8;
            minor = cpu_list[i] % 8;
    
            *(cpu_mask + major) |= (0x1 << minor);
        }
    }

    memset(irq_to_guest_map, 0, sizeof(struct v3_vm_info *) * 256);

    if (init_print_buffers()) {
	ERROR("Cannot initialize print buffers\n");
	palacios_free(cpu_mask);
	return -1;
    }

    INFO("palacios_init starting - calling init_v3\n");

    Init_V3(&palacios_os_hooks, cpu_mask, num_cpus, options);

    return 0;

}


int palacios_vmm_exit( void ) {

    Shutdown_V3();

    INFO("palacios shutdown complete\n");

    deinit_print_buffers();

    return 0;
}
