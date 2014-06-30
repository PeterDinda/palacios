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

#include <palacios/vmm.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_lowlevel.h>


extern struct v3_os_hooks * os_hooks;


int v3_lock_init(v3_lock_t * lock) {
    *lock = (addr_t)(os_hooks->mutex_alloc());

    if (!(*lock)) {
	return -1;
    }

    return 0;
}


void v3_lock_deinit(v3_lock_t * lock) {
    os_hooks->mutex_free((void *)*lock);
    *lock = 0;
}

void v3_lock(v3_lock_t lock) {
    os_hooks->mutex_lock((void *)lock, 0);    
}

void v3_unlock(v3_lock_t lock) {
    os_hooks->mutex_unlock((void *)lock);
}

addr_t v3_lock_irqsave(v3_lock_t lock) {
    return (addr_t) (os_hooks->mutex_lock_irqsave((void *)lock, 1));
}


void v3_unlock_irqrestore(v3_lock_t lock, addr_t irq_state) {
    os_hooks->mutex_unlock_irqrestore((void *)lock,(void*)irq_state);
}


int v3_rw_lock_init(v3_rw_lock_t *lock)
{
    lock->reader_count=0;
    return v3_lock_init(&(lock->lock));
}

void v3_rw_lock_deinit(v3_rw_lock_t *lock)
{
    v3_lock_deinit(&(lock->lock));
    lock->reader_count=0;
}

void v3_read_lock(v3_rw_lock_t *lock)
{
    addr_t flags;

    flags=v3_lock_irqsave(lock->lock);
    lock->reader_count++;
    v3_unlock_irqrestore(lock->lock,flags);
    // readers can come in after us, writers cannot
}
void v3_read_unlock(v3_rw_lock_t *lock)
{
    addr_t flags;

    flags=v3_lock_irqsave(lock->lock);
    lock->reader_count--;
    v3_unlock_irqrestore(lock->lock,flags);
    // readers can come in after us, and also writers if reader_count==0
}

void v3_write_lock(v3_rw_lock_t *lock)
{
    // a less hideous implementation is possible, of course...
    while (1) { 
	v3_lock(lock->lock);
	if (!(lock->reader_count)) { 
	    break;
	}
	v3_unlock(lock->lock);
	V3_Yield();
    }
    // holding lock now - reader or writer cannot come in after us
}

addr_t v3_write_lock_irqsave(v3_rw_lock_t *lock)
{
    addr_t flags;

    while (1) { 
	flags=v3_lock_irqsave(lock->lock);
	if (!(lock->reader_count)) { 
	    break;
	}
	v3_unlock_irqrestore(lock->lock,flags);
	V3_Yield();
    }
    // holding lock now with interrupts off - reader or writer canot come in after us
    return flags;
}

void v3_write_unlock(v3_rw_lock_t *lock) 
{
    // I am already holding this lock
    v3_unlock(lock->lock);
    // readers/writers can now come in
}

void v3_write_unlock_irqrestore(v3_rw_lock_t *lock, addr_t irq_state)
{
    // I am already holding this lock with interrupts off
    v3_unlock_irqrestore(lock->lock,irq_state);
    // readers/writers can now come in
}
