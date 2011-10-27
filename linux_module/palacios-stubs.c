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

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>
#include "palacios.h"




#include "mm.h"


u32 pg_allocs = 0;
u32 pg_frees = 0;
u32 mallocs = 0;
u32 frees = 0;


static struct v3_vm_info * irq_to_guest_map[256];


extern unsigned int cpu_khz;


/**
 * Prints a message to the console.
 */
static void palacios_print(const char *	fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintk(fmt, ap);
  va_end(ap);
  
  return;
}



/*
 * Allocates a contiguous region of pages of the requested size.
 * Returns the physical address of the first page in the region.
 */
static void * palacios_allocate_pages(int num_pages, unsigned int alignment) {
    void * pg_addr = NULL;

    pg_addr = (void *)alloc_palacios_pgs(num_pages, alignment);
    pg_allocs += num_pages;

    return pg_addr;
}


/**
 * Frees a page previously allocated via palacios_allocate_page().
 * Note that palacios_allocate_page() can allocate multiple pages with
 * a single call while palacios_free_page() only frees a single page.
 */

static void palacios_free_pages(void * page_paddr, int num_pages) {
    pg_frees += num_pages;
    free_palacios_pgs((uintptr_t)page_paddr, num_pages);
}


/**
 * Allocates 'size' bytes of kernel memory.
 * Returns the kernel virtual address of the memory allocated.
 */
static void *
palacios_alloc(unsigned int size) {
    void * addr = NULL;

    if (irqs_disabled()) {
    	addr = kmalloc(size, GFP_ATOMIC);
    } else {
    	addr = kmalloc(size, GFP_KERNEL);
    }
    mallocs++;

 
    return addr;
}

/**
 * Frees memory that was previously allocated by palacios_alloc().
 */
static void
palacios_free(
	void *			addr
)
{
    frees++;
    kfree(addr);
    return;
}

/**
 * Converts a kernel virtual address to the corresponding physical address.
 */
static void *
palacios_vaddr_to_paddr(
	void *			vaddr
)
{
    return (void*) __pa(vaddr);

}

/**
 * Converts a physical address to the corresponding kernel virtual address.
 */
static void *
palacios_paddr_to_vaddr(
	void *			paddr
)
{
  return __va(paddr);
}

/**
 * Runs a function on the specified CPU.
 */

// For now, do call only on local CPU 
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

struct lnx_thread_arg {
    int (*fn)(void * arg);
    void * arg;
};

static int lnx_thread_target(void * arg) {
    struct lnx_thread_arg * thread_info = (struct lnx_thread_arg *)arg;
    int ret = 0;
    /*
      printk("Daemonizing new Palacios thread (name=%s)\n", thread_info->name);

      daemonize(thread_info->name);
      allow_signal(SIGKILL);
    */


    ret = thread_info->fn(thread_info->arg);

    kfree(thread_info);
    // handle cleanup 

    do_exit(ret);
    
    return 0; // should not get here.
}

/**
 * Creates a kernel thread.
 */
static void *
palacios_start_kernel_thread(
	int (*fn)		(void * arg),
	void *			arg,
	char *			thread_name) {

    struct lnx_thread_arg * thread_info = kmalloc(sizeof(struct lnx_thread_arg), GFP_KERNEL);

    thread_info->fn = fn;
    thread_info->arg = arg;

    return kthread_run( lnx_thread_target, thread_info, thread_name );
}


/**
 * Starts a kernel thread on the specified CPU.
 */
static void * 
palacios_start_thread_on_cpu(int cpu_id, 
			     int (*fn)(void * arg), 
			     void * arg, 
			     char * thread_name ) {
    struct task_struct * thread = NULL;
    struct lnx_thread_arg * thread_info = kmalloc(sizeof(struct lnx_thread_arg), GFP_KERNEL);

    thread_info->fn = fn;
    thread_info->arg = arg;


    thread = kthread_create( lnx_thread_target, thread_info, thread_name );

    if (IS_ERR(thread)) {
	printk("Palacios error creating thread: %s\n", thread_name);
	return NULL;
    }

    if (set_cpus_allowed_ptr(thread, cpumask_of(cpu_id)) != 0) {
	kthread_stop(thread);
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
static int
palacios_move_thread_to_cpu(int new_cpu_id, 
			    void * thread_ptr) {
    struct task_struct * thread = (struct task_struct *)thread_ptr;

    printk("Moving thread (%p) to cpu %d\n", thread, new_cpu_id);

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
static unsigned int 
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
    printk("hooking vector %d\n", vector);  	

    if (irq_to_guest_map[vector]) {
	printk(KERN_WARNING
	       "%s: Interrupt vector %u is already hooked.\n",
	       __func__, vector);
	return -1;
    }

    printk(KERN_DEBUG
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
	panic("unexpected vector for hooking\n");
    } else {
	int device_id = 0;		
	
	int flag = 0;
	int error;
		
	printk("hooking vector: %d\n", vector);		

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
	    printk("error code for request_irq is %d\n", error);
	    panic("request vector %d failed",vector);
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
  printk("Pretending to ack interrupt, vector=%d\n",vector);
  return 0;
}
  
/**
 * Returns the CPU frequency in kilohertz.
 */
static unsigned int
palacios_get_cpu_khz(void) 
{
    printk("cpu_khz is %u\n",cpu_khz);

    if (cpu_khz == 0) { 
	printk("faking cpu_khz to 1000000\n");
	return 1000000;
    } else {
	return cpu_khz;
    }
  //return 1000000;
}

/**
 * Yield the CPU so other host OS tasks can run.
 */
static void
palacios_yield_cpu(void)
{
    schedule();
    return;
}



/**
 * Allocates a mutex.
 * Returns NULL on failure.
 */
static void *
palacios_mutex_alloc(void)
{
    spinlock_t *lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);

    if (lock) {
	spin_lock_init(lock);
    }
    
    return lock;
}

/**
 * Frees a mutex.
 */
static void
palacios_mutex_free(void * mutex) {
    kfree(mutex);
}

/**
 * Locks a mutex.
 */
static void 
palacios_mutex_lock(void * mutex, int must_spin) {
    spin_lock((spinlock_t *)mutex);
}

/**
 * Unlocks a mutex.
 */
static void 
palacios_mutex_unlock(
	void *			mutex
) 
{
    spin_unlock((spinlock_t *)mutex);
}

/**
 * Structure used by the Palacios hypervisor to interface with the host kernel.
 */
static struct v3_os_hooks palacios_os_hooks = {
	.print			= palacios_print,
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
	.mutex_alloc		= palacios_mutex_alloc,
	.mutex_free		= palacios_mutex_free,
	.mutex_lock		= palacios_mutex_lock, 
	.mutex_unlock		= palacios_mutex_unlock,
	.get_cpu		= palacios_get_cpu,
	.interrupt_cpu		= palacios_interrupt_cpu,
	.call_on_cpu		= palacios_xcall,
	.start_thread_on_cpu	= palacios_start_thread_on_cpu,
	.move_thread_to_cpu = palacios_move_thread_to_cpu,
};




int palacios_vmm_init( void )
{
    
    memset(irq_to_guest_map, 0, sizeof(struct v3_vm_info *) * 256);
    
    printk("palacios_init starting - calling init_v3\n");
    
    Init_V3(&palacios_os_hooks, num_online_cpus());

    return 0;

}


int palacios_vmm_exit( void ) {

    Shutdown_V3();

    return 0;
}
