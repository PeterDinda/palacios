/* 
 * Palacios VNET Host Hooks Implementations 
 * Lei Xia 2010
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <asm/delay.h>
#include <linux/timer.h>

#include <vnet/vnet.h>
#include "mm.h"
#include "palacios-vnet.h"
#include "linux-exts.h"

static void host_print(const char *	fmt, ...) {

    va_list ap;
    va_start(ap, fmt);
    vprintk(fmt, ap);
    va_end(ap);

    return;
}


static void * host_allocate_pages(int num_pages, unsigned int alignment){
    uintptr_t addr = 0; 
    struct page * pgs = NULL;
    int order = get_order(num_pages * PAGE_SIZE);
	 
    pgs = alloc_pages(GFP_KERNEL, order);
    
    WARN(!pgs, "Could not allocate pages\n");
       
    addr = page_to_pfn(pgs) << PAGE_SHIFT; 
   
    return (void *)addr;
}


static void host_free_pages(void * page_paddr, int num_pages) {
    uintptr_t pg_addr = (uintptr_t)page_paddr;
	
    __free_pages(pfn_to_page(pg_addr >> PAGE_SHIFT), get_order(num_pages * PAGE_SIZE));
}


static void *
host_alloc(unsigned int size) {
    void * addr;
    addr =  kmalloc(size, GFP_KERNEL);

    return addr;
}

static void
host_free(
	void *			addr
)
{
    kfree(addr);
    return;
}

static void *
host_vaddr_to_paddr(void * vaddr)
{
    return (void*) __pa(vaddr);

}

static void *
host_paddr_to_vaddr(void * paddr)
{
    return __va(paddr);
}


static void *
host_start_kernel_thread(
	int (*fn)(void * arg),
	void * arg,
	char * thread_name) {
    struct task_struct * thread = NULL;

    thread = kthread_run(fn, arg, thread_name );

    return thread;
}

static void host_kthread_sleep(long timeout){
    set_current_state(TASK_INTERRUPTIBLE);

    if(timeout <= 0){
    	schedule();
    }else {
       schedule_timeout(timeout);
    }

    return;
}

static void host_kthread_wakeup(void * thread){
    struct task_struct * kthread = (struct task_struct *)thread;
	
    wake_up_process(kthread);
}

static void host_kthread_stop(void * thread){
    struct task_struct * kthread = (struct task_struct *)thread;

    kthread_stop(kthread);
}

static int host_kthread_should_stop(void){
    return kthread_should_stop();
}


static void host_udelay(unsigned long usecs){
    udelay(usecs);
}



static void
host_yield_cpu(void)
{
    schedule();
    return;
}

static void *
host_mutex_alloc(void)
{
    spinlock_t * lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);

    if (lock) {
	spin_lock_init(lock);
    }

    return lock;
}

static void
host_mutex_free(
	void * mutex
) 
{
    kfree(mutex);
}

static void 
host_mutex_lock(void * mutex, 
		int must_spin)
{
    spin_lock((spinlock_t *)mutex);
}

static void 
host_mutex_unlock(void * mutex) 
{
    spin_unlock((spinlock_t *)mutex);
}




struct host_timer {
    struct timer_list timer;
    unsigned long interval;

    int active;
    void (* timer_fun)(void * private_data);
    void * pri_data;
};


void timeout_fn(unsigned long arg){
    struct host_timer * timer = (struct host_timer *)arg;

    if(timer->active){
	timer->timer_fun(timer->pri_data);
	
    	mod_timer(&(timer->timer), timer->interval);
    }
}

static void *
host_create_timer(unsigned long interval, 
		  void (* timer_fun)(void * priv_data), 
		  void * data){
    struct host_timer * timer = (struct host_timer *)kmalloc(sizeof(struct host_timer), GFP_KERNEL);

    timer->interval = interval;
    timer->timer_fun = timer_fun;
    timer->pri_data = data;

    init_timer(&(timer->timer));

    timer->timer.data = (unsigned long)timer;
    timer->timer.function = timeout_fn;
    timer->timer.expires = interval;

    return timer;
}

static void
host_start_timer(void * vnet_timer){
    struct host_timer * timer = (struct host_timer *)vnet_timer;

    timer->active = 1;
    add_timer(&(timer->timer));
}

static void
host_reset_timer(void * vnet_timer, unsigned long interval){
    struct host_timer * timer = (struct host_timer *)timer;

    timer->interval = interval;
}

static void
host_stop_timer(void * vnet_timer){
    struct host_timer * timer = (struct host_timer *)vnet_timer;

    timer->active = 0;
    del_timer(&(timer->timer));
}

static void
host_del_timer(void * vnet_timer){
    struct host_timer * timer = (struct host_timer *)vnet_timer;

    del_timer(&(timer->timer));

    kfree(timer);
}





static struct vnet_host_hooks vnet_host_hooks = {
    .timer_create	= host_create_timer,
    .timer_del		= host_del_timer,
    .timer_start		= host_start_timer,
    .timer_stop		= host_stop_timer,
    .timer_reset	= host_reset_timer,

    .thread_start 	= host_start_kernel_thread,
    .thread_sleep  	= host_kthread_sleep,
    .thread_wakeup	= host_kthread_wakeup,
    .thread_stop	= host_kthread_stop,
    .thread_should_stop	= host_kthread_should_stop,
    .udelay	= host_udelay,

    .yield_cpu		= host_yield_cpu,
    .mutex_alloc	= host_mutex_alloc,
    .mutex_free	= host_mutex_free,
    .mutex_lock	= host_mutex_lock, 
    .mutex_unlock	= host_mutex_unlock,

    .print			= host_print,
    .allocate_pages	= host_allocate_pages,
    .free_pages	= host_free_pages,
    .malloc		= host_alloc,
    .free			= host_free,
    .vaddr_to_paddr		= host_vaddr_to_paddr,
    .paddr_to_vaddr		= host_paddr_to_vaddr,
};



static int vnet_init( void ) {
    init_vnet(&vnet_host_hooks);
	
    vnet_bridge_init();
    vnet_ctrl_init();

    printk("V3 VNET Inited\n");
        
    return 0;
}


static int vnet_deinit( void ) {
    deinit_vnet();

    vnet_bridge_deinit();
    vnet_ctrl_deinit();

    printk("V3 VNET Deinited\n");

    return 0;
}

static struct linux_ext vnet_ext = {
    .name = "VNET",
    .init = vnet_init,
    .deinit = vnet_deinit,
    .guest_init = NULL,
    .guest_deinit = NULL
};

register_extension(&vnet_ext);
