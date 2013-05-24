#ifndef _lockcheck
#define _lockcheck

#define CHECK_LOCKS  0
#define NUM_LOCKS    1024

#if CHECK_LOCKS
#define LOCKCHECK_INIT() palacios_lockcheck_init()
#define LOCKCHECK_ALLOC(lock) palacios_lockcheck_alloc(lock)
#define LOCKCHECK_FREE(lock)  palacios_lockcheck_free(lock)
#define LOCKCHECK_LOCK(lock)  palacios_lockcheck_lock(lock)
#define LOCKCHECK_UNLOCK(lock) palacios_lockcheck_unlock(lock)
#define LOCKCHECK_LOCK_IRQSAVE(lock, flags)  palacios_lockcheck_lock_irqsave(lock,flags)
#define LOCKCHECK_UNLOCK_IRQRESTORE(lock, flags) palacios_lockcheck_unlock_irqrestore(lock,flags)
#define LOCKCHECK_DEINIT() palacios_lockcheck_deinit()
#else
#define LOCKCHECK_INIT()
#define LOCKCHECK_ALLOC(lock) 
#define LOCKCHECK_FREE(lock)  
#define LOCKCHECK_LOCK(lock)  
#define LOCKCHECK_UNLOCK(lock) 
#define LOCKCHECK_LOCK_IRQSAVE(lock, flags)  
#define LOCKCHECK_UNLOCK_IRQRESTORE(lock, flags) 
#define LOCKCHECK_DEINIT()
#endif

void palacios_lockcheck_init(void);
void palacios_lockcheck_alloc(void *lock);
void palacios_lockcheck_free(void *lock);
void palacios_lockcheck_lock(void *lock);
void palacios_lockcheck_unlock(void *lock);
void palacios_lockcheck_lock_irqsave(void *lock,unsigned long flags);
void palacios_lockcheck_unlock_irqrestore(void *lock,unsigned long flags);
void palacios_lockcheck_deinit(void);

#endif
