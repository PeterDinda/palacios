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
#include <asm/delay.h>
#include <linux/timer.h>

#include <vnet/vnet.h>
#include "palacios.h"
#include "mm.h"
#include "palacios-vnet.h"
#include "linux-exts.h"




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

    while (kthread_stop(kthread)==-EINTR)
	;
}

static int host_kthread_should_stop(void){
    return kthread_should_stop();
}


static void host_udelay(unsigned long usecs){
    udelay(usecs);
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
    struct host_timer * timer = (struct host_timer *)palacios_alloc(sizeof(struct host_timer));

    if (!timer) { 
	ERROR("Unable to allocate timer in VNET\n");
	return NULL;
    }

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

    palacios_free(timer);
}


static void *
host_allocate_pages(int num_pages, unsigned int alignment)
{
    // allocates pages preferentially on the caller's node
    return palacios_allocate_pages(num_pages, alignment, -1);
}



static struct vnet_host_hooks vnet_host_hooks = {
    .timer_create	        = host_create_timer,
    .timer_del		        = host_del_timer,
    .timer_start		= host_start_timer,
    .timer_stop		        = host_stop_timer,
    .timer_reset	        = host_reset_timer,

    .thread_start 	        = palacios_start_kernel_thread,
    .thread_sleep  	        = host_kthread_sleep,
    .thread_wakeup	        = host_kthread_wakeup,
    .thread_stop	        = host_kthread_stop,
    .thread_should_stop	        = host_kthread_should_stop,
    .udelay	                = host_udelay,

    .yield_cpu		        = palacios_yield_cpu,
    .mutex_alloc	        = palacios_mutex_alloc,
    .mutex_free	                = palacios_mutex_free,
    .mutex_lock	                = palacios_mutex_lock, 
    .mutex_unlock	        = palacios_mutex_unlock,
    .mutex_lock_irqsave         = palacios_mutex_lock_irqsave, 
    .mutex_unlock_irqrestore    = palacios_mutex_unlock_irqrestore,

    .print			= palacios_print_scoped,
    .allocate_pages	        = host_allocate_pages,
    .free_pages	                = palacios_free_pages,
    .malloc		        = palacios_alloc,
    .free			= palacios_free,
    .vaddr_to_paddr		= palacios_vaddr_to_paddr,
    .paddr_to_vaddr		= palacios_paddr_to_vaddr,
};



static int vnet_init( void ) {
    init_vnet(&vnet_host_hooks);
	
    vnet_bridge_init();
    vnet_ctrl_init();

    INFO("V3 VNET Inited\n");
        
    return 0;
}


static int vnet_deinit( void ) {

    INFO("V3 Control Deinit Start\n");

    vnet_ctrl_deinit();

    INFO("V3 Bridge Deinit Start\n");

    vnet_bridge_deinit();

    INFO("V3 VNET Deinit Start\n");

    deinit_vnet();

    INFO("V3 VNET Deinited\n");

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
