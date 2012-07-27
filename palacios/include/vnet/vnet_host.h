/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VNET_HOST_H__
#define __VNET_HOST_H__

#include <vnet/vnet_base.h>
#include <vnet/vnet_vmm.h>

struct vnet_thread {
    void * host_thread;
};

struct vnet_timer {
    void * host_timer;
};

typedef unsigned long vnet_lock_t;
typedef void *vnet_intr_flags_t;


struct vnet_host_hooks {
    void *(*thread_start)(int (*fn)(void * arg), 
			  void * arg, 
			  char * thread_name);

    void (*thread_sleep)(long timeout);
    void (*thread_wakeup)(void * thread);
    void (*thread_stop)(void * thread);
    int (*thread_should_stop)(void);

    void *(*timer_create)(unsigned long interval, 
			  void (* timer_fun)(void * priv_data), 
			  void * data);

    void (*timer_del)(void * timer);
    void (*timer_start)(void * timer);
    void (*timer_stop)(void * timer);
    void (*timer_reset)(void * timer, unsigned long interval);

    void (*udelay)(unsigned long usecs);

    /* duplicate part from os_hooks */
    void (*yield_cpu)(void); 
    void (*print)(const char * format, ...)
  	__attribute__ ((format (printf, 1, 2)));
  
    void *(*allocate_pages)(int num_pages, unsigned int alignment);
    void (*free_pages)(void * page, int num_pages);

    void *(*malloc)(unsigned int size);
    void (*free)(void * addr);

    void *(*paddr_to_vaddr)(void * addr);
    void *(*vaddr_to_paddr)(void * addr);

    void *(*mutex_alloc)(void);
    void (*mutex_free)(void * mutex);
    void (*mutex_lock)(void * mutex, int must_spin);
    void (*mutex_unlock)(void * mutex);
    vnet_intr_flags_t (*mutex_lock_irqsave)(void * mutex, int must_spin);
    void (*mutex_unlock_irqrestore)(void * mutex, vnet_intr_flags_t flags);
};



#ifdef __V3VEE__

extern struct vnet_host_hooks * host_hooks;


/* MEMORY ALLOCATE/DEALLOCATE */

#define PAGE_SIZE_4KB 4096
		
/* 4KB-aligned */
static inline void * Vnet_AllocPages(int num_pages){
    if ((host_hooks) && host_hooks->allocate_pages) {
	return host_hooks->allocate_pages(num_pages, PAGE_SIZE_4KB);
    }

    return NULL;
}

static inline void Vnet_FreePages(void * page, int num_pages){
    if ((host_hooks) && host_hooks->free_pages) {	
	host_hooks->free_pages(page, num_pages);
    }
} 

static inline void * Vnet_VAddr(void * addr) {
    if ((host_hooks) && host_hooks->paddr_to_vaddr){
	return host_hooks->paddr_to_vaddr(addr);
    }

    return NULL;
}

static inline void * Vnet_PAddr(void * addr) {
    if ((host_hooks) && host_hooks->vaddr_to_paddr) {
	return host_hooks->vaddr_to_paddr(addr);
    }

    return NULL;
}

static inline void * Vnet_Malloc(uint32_t size){
    if ((host_hooks) && host_hooks->malloc) {
	return host_hooks->malloc(size);
    }

    return NULL;
}

static inline void Vnet_Free(void * addr){  
    if ((host_hooks) && host_hooks->free) {
	host_hooks->free(addr);
    }
}


static inline void Vnet_Yield(void){
    if ((host_hooks) && (host_hooks)->yield_cpu) {
	host_hooks->yield_cpu();
    }
}

/* THREAD FUNCTIONS */
struct vnet_thread * vnet_start_thread(int (*func)(void *), void *arg, char * name);

static inline void vnet_thread_sleep(long timeout){
    if((host_hooks) && host_hooks->thread_sleep){
	host_hooks->thread_sleep(timeout);
    }
}

static inline void vnet_thread_wakeup(struct vnet_thread * thread){
    if((host_hooks) && host_hooks->thread_wakeup){
	host_hooks->thread_wakeup(thread->host_thread);
    }
}


static inline void vnet_thread_stop(struct vnet_thread * thread){
    if((host_hooks) && host_hooks->thread_stop){
	host_hooks->thread_stop(thread->host_thread);
    }
}

static inline int vnet_thread_should_stop(void){
    if((host_hooks) && host_hooks->thread_should_stop){
	return host_hooks->thread_should_stop();
    }

    return 0;
}

static inline void  vnet_udelay(unsigned long usecs){
    if((host_hooks) && host_hooks->udelay){
	host_hooks->udelay(usecs);
    }
}

/* TIMER FUNCTIONS */
/* interval, in jittes */
struct vnet_timer * vnet_create_timer(unsigned long interval, 
				      void (* timer_fun)(void * priv_data), 
				      void * pri_data);

static inline void vnet_del_timer(struct vnet_timer * timer){
    if((host_hooks) && host_hooks->timer_del){
	host_hooks->timer_del(timer->host_timer);
	Vnet_Free(timer);
    }
}
	
static inline void vnet_start_timer(struct vnet_timer * timer){
    if((host_hooks) && host_hooks->timer_start){
	host_hooks->timer_start(timer->host_timer);
    }
}

static inline void vnet_stop_timer(struct vnet_timer * timer){
    if((host_hooks) && host_hooks->timer_stop){
	host_hooks->timer_stop(timer->host_timer);
    }
}

static inline void vnet_reset_timer(struct vnet_timer * timer, 
				    unsigned long new_interval){
    if((host_hooks) && host_hooks->timer_reset){
	host_hooks->timer_reset(timer->host_timer, new_interval);
    }
}



#define Vnet_Print(level, fmt, args...)					\
    do {								\
	extern int net_debug;						\
	if(level <= net_debug) {					\
	    extern struct vnet_host_hooks * host_hooks;			\
	    if ((host_hooks) && (host_hooks)->print) {			\
	    	(host_hooks)->print((fmt), ##args);			\
	    }								\
	}								\
    } while (0)	


#define Vnet_Debug(fmt, args...)					\
    do {								\
	    extern struct vnet_host_hooks * host_hooks;			\
	    if ((host_hooks) && (host_hooks)->print) {			\
	    	(host_hooks)->print((fmt), ##args);			\
	    }			       					\
    } while (0)	




/* Lock Utilities */
static inline int vnet_lock_init(vnet_lock_t * lock) {
    if((host_hooks) && host_hooks->mutex_alloc){
	*lock = (addr_t)(host_hooks->mutex_alloc());
    	if (*lock) {
	    return 0;
    	}
    }
    return -1;
}

static inline void vnet_lock_deinit(vnet_lock_t * lock) {
    if (host_hooks && (host_hooks->mutex_free)) { 
	host_hooks->mutex_free((void *)*lock);
	*lock = 0;
    }
}

static inline void vnet_lock(vnet_lock_t lock) {
    if (host_hooks && (host_hooks->mutex_lock)) { 
	host_hooks->mutex_lock((void *)lock,0);    
    }
}

static inline void vnet_unlock(vnet_lock_t lock) {
    if (host_hooks && (host_hooks->mutex_lock)) { 
	host_hooks->mutex_unlock((void *)lock);
    }
}

static inline vnet_intr_flags_t vnet_lock_irqsave(vnet_lock_t lock) 
{
    if (host_hooks && host_hooks->mutex_lock_irqsave) { 
	return (host_hooks->mutex_lock_irqsave((void *)lock, 1));
    } else {
	return NULL;
    }
}


static inline void vnet_unlock_irqrestore(vnet_lock_t lock, vnet_intr_flags_t irq_state) 
{
    if (host_hooks && (host_hooks->mutex_unlock_irqrestore)) {
	host_hooks->mutex_unlock_irqrestore((void *)lock,irq_state);
    }
}
#endif


void init_vnet(struct vnet_host_hooks * hooks);
void deinit_vnet(void);


#endif

 
