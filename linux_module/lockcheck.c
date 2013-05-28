#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>

#include "palacios.h"

#include "lockcheck.h"

#define PRINT_LOCK_ALLOC  0
#define PRINT_LOCK_FREE   0
#define PRINT_LOCK_LOCK   0
#define PRINT_LOCK_UNLOCK 0

// How far up the stack to track the caller
// 0 => palacios_...
// 1 => v3_lock...
// 2 => caller of v3_lock..
// ... 
#define STEP_BACK_DEPTH_FIRST 1
#define STEP_BACK_DEPTH_LAST  4
#define STEP_BACK_DEPTH       (STEP_BACK_DEPTH_LAST-STEP_BACK_DEPTH_FIRST+1)

// show when multiple locks are held simultaneously
// This is the minimum number
#define WARN_MULTIPLE_THRESHOLD 3

typedef struct {
  int  inuse;         // nonzero if this is in use
  void *lock;         // the lock
  void *allocator[STEP_BACK_DEPTH];
                      // who allocated this
  int   lockcount;    // how many times it's been locked/unlocked (lock=+1, unlock=-1)
  int   irqcount;     // how many times interrupts have been turned off (+1/-1)
  void *lastlocker[STEP_BACK_DEPTH];
                      // who last locked
  void *lastunlocker[STEP_BACK_DEPTH]; 
                      // who last unlocked
  void *lastirqlocker[STEP_BACK_DEPTH];
                      // who last locked
  unsigned long lastlockflags; // their flags
  void *lastirqunlocker[STEP_BACK_DEPTH]
                    ; // who last unlocked
  unsigned long lastunlockflags; // their flags
} lockcheck_state_t;


// This lock is currently used only to control
// allocation of entries in the global state
static spinlock_t lock; 
static lockcheck_state_t state[NUM_LOCKS];

static void printlock(char *prefix, lockcheck_state_t *l);


typedef struct {
  u32 top;               // next open slot 0..
  void *lock[LOCK_STACK_DEPTH]; // the stack
  char irq[LOCK_STACK_DEPTH];   // locked with irqsave?
} lock_stack_t;

static DEFINE_PER_CPU(lock_stack_t, lock_stack);

static lockcheck_state_t *get_lock_entry(void)
{
  int i;
  unsigned long f;
  lockcheck_state_t *l;

  spin_lock_irqsave(&lock,f);

  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (!(l->inuse)) { 
      l->inuse=1;
      break;
    }
  }

  spin_unlock_irqrestore(&lock,f);
  
  if (i<NUM_LOCKS) { 
    return l;
  } else {
    return 0;
  }
}


static lockcheck_state_t *find_lock_entry(void *lock)
{
  int i;
  lockcheck_state_t *l;

  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (l->inuse && l->lock == lock) { 
      return l;
    }
  }
  return 0;
}


static void free_lock_entry(lockcheck_state_t *l)
{
  l->inuse=0;
}


static void lock_stack_print(void)
{
  u32 i;
  char buf[64];
  lock_stack_t *mystack = &(get_cpu_var(lock_stack));
  u32 cpu = get_cpu();  put_cpu();
  
  if ((mystack->top)>0) { 
    for (i=mystack->top; i>0;i--) {
      snprintf(buf,64,"LOCK STACK (cpu=%u, index=%u, irq=%d)",cpu, i-1, (int)(mystack->irq[i-1]));
      printlock(buf,find_lock_entry(mystack->lock[i-1]));
    }
  }
  put_cpu_var(lock_stack);
}


static void lock_stack_lock(void *lock, char irq)
{
  lock_stack_t *mystack = &(get_cpu_var(lock_stack));
  u32 cpu = get_cpu();  put_cpu();

  if (mystack->top>=(LOCK_STACK_DEPTH-1)) {
    put_cpu_var(lock_stack);
    DEBUG("LOCKCHECK: Locking lock 0x%p on cpu %u exceeds stack limit of %d\n",lock,cpu,LOCK_STACK_DEPTH);
    lock_stack_print();
  } else {
    mystack->lock[mystack->top] = lock;
    mystack->irq[mystack->top] = irq;
    mystack->top++;
    put_cpu_var(lock_stack);
  }
}

static void lock_stack_unlock(void *lock, char irq)
{
  lock_stack_t *mystack = &(get_cpu_var(lock_stack));
  u32 cpu = get_cpu(); put_cpu();

  if (mystack->top==0) {
    put_cpu_var(lock_stack);
    DEBUG("LOCKCHECK: Unlocking lock 0x%p on cpu %u when lock stack is empty\n",lock,cpu);
  } else {
    if (mystack->lock[mystack->top-1] != lock) { 
      void *otherlock=mystack->lock[mystack->top-1];
      put_cpu_var(lock_stack);
      DEBUG("LOCKCHECK: Unlocking lock 0x%p on cpu %u when top of stack is lock 0x%p\n",lock,cpu, otherlock);
      lock_stack_print();
    } else {
      if (irq!=mystack->irq[mystack->top-1]) {
	char otherirq = mystack->irq[mystack->top-1];
	put_cpu_var(lock_stack);
	DEBUG("LOCKCHECK: Unlocking lock 0x%p on cpu %u with irq=%d, but was locked with irq=%d\n",lock,cpu,irq,otherirq);
	lock_stack_print();
      } else {
	mystack->top--;
	put_cpu_var(lock_stack);
      }
    }
  }

}

void palacios_lockcheck_init()
{
  memset(state,0,sizeof(lockcheck_state_t)*NUM_LOCKS);
  spin_lock_init(&lock);
  DEBUG("LOCKCHECK: LOCK CHECKING INITED\n");
}

//
// This needs to be defined explictly since the intrinsic does not take a var
//
#define backtrace(t) \
  t[0]=__builtin_return_address(STEP_BACK_DEPTH_FIRST); \
  t[1]=__builtin_return_address(STEP_BACK_DEPTH_FIRST+1); \
  t[2]=__builtin_return_address(STEP_BACK_DEPTH_FIRST+2); \
  t[3]=__builtin_return_address(STEP_BACK_DEPTH_FIRST+3); 

//
// For printing a backtrace
//
//
#define backtrace_format "%pS << %pS << %pS << %pS"
#define backtrace_expand(t) ((t)[0]),((t)[1]),((t)[2]),((t)[3])


static void clear_trace(void **trace)
{
  int i;

  for (i=0;i<STEP_BACK_DEPTH;i++) { 
    trace[i]=0;
  }
}


static void printlock(char *prefix, lockcheck_state_t *l)
{
  if (!l || !(l->lock) ) { 
    DEBUG("LOCKCHECK: %s: lock 0x%p BOGUS\n",prefix,l);
    return;
  }
  if (l->lock) { 
    DEBUG("LOCKCHECK: %s: lock 0x%p, allocator=" 
	  backtrace_format
          ", lockcount=%d, lastlocker="
	  backtrace_format
	  ", lastunlocker="
	  backtrace_format
	  ", irqcount=%d, lastirqlocker="
	  backtrace_format
	  ", lastlockflags=%lu, lastirqunlocker="
	  backtrace_format
	  ", lastunlockflags=%lu\n",
	  prefix,l->lock,
	  backtrace_expand(l->allocator),
	  l->lockcount,
	  backtrace_expand(l->lastlocker),
	  backtrace_expand(l->lastunlocker),
	  l->irqcount,
	  backtrace_expand(l->lastirqlocker),
	  l->lastlockflags,
	  backtrace_expand(l->lastirqunlocker),
	  l->lastunlockflags);
  }
}



static void find_multiple_locks_held(void)
{
  int i;
  int have=0;
  lockcheck_state_t *l;
  char buf[64];

  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (l->inuse && l->lockcount>0) { 
      have++;
      if (have>=WARN_MULTIPLE_THRESHOLD) { 
	break;
      }
    }
  }
  
  if (have>=WARN_MULTIPLE_THRESHOLD) { 
    have=0;
    for (i=0;i<NUM_LOCKS;i++) { 
      l=&(state[i]);
      if (l->inuse && l->lockcount>0) {
	snprintf(buf,64,"MULTIPLE LOCKS HELD (%d)",have);
	printlock(buf,l);
	have++;
      }
    }
  }
    
}

static void find_multiple_irqs_held(void)
{
  int i;
  int have=0;
  lockcheck_state_t *l;
  char buf[64];

  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (l->inuse && l->irqcount>0) {
      have++;
      if (have>=WARN_MULTIPLE_THRESHOLD) { 
	break;
      }
    }
  }
  
  if (have>=WARN_MULTIPLE_THRESHOLD) { 
    have=0;
    for (i=0;i<NUM_LOCKS;i++) { 
      l=&(state[i]);
      if (l->inuse && l->irqcount>0) { 
	snprintf(buf,64,"MULTIPLE IRQS HELD (%d)",have);
	printlock(buf,l);
	have++;
      }
    }
  }

}


void palacios_lockcheck_deinit()
{
  int i;
  lockcheck_state_t *l;
  
  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (l->lock) { 
      printlock("ALLOCATED LOCK AT DEINIT",l);
      if ((l->lockcount)) { 
	printlock("BAD LOCK COUNT AT DEINIT",l);
      }
      if ((l->irqcount)) { 
	printlock("BAD IRQ COUNT AT DEINIT",l);
      }
    }
  } 
  INFO("LOCKCHECK: DEINITED\n");
}


void palacios_lockcheck_alloc(void *lock)
{
  lockcheck_state_t *l=get_lock_entry();

  if (!l) { 
    DEBUG("LOCKCHECK: UNABLE TO ALLOCATE TRACKING DATA FOR LOCK 0x%p\n",lock);
  }
  l->lock=lock;
  backtrace(l->allocator);
  l->lockcount=l->irqcount=0;
  clear_trace(l->lastlocker);
  clear_trace(l->lastunlocker);
  clear_trace(l->lastirqlocker);
  clear_trace(l->lastirqunlocker);
  //INFO("LOCKCHECK: LOCK ALLOCATE 0x%p\n",lock);
#if PRINT_LOCK_ALLOC
  printlock("NEW LOCK", l);
#endif
}
  
void palacios_lockcheck_free(void *lock)
{
  lockcheck_state_t *l=find_lock_entry(lock);
  
  if (!l){
    DEBUG("LOCKCHECK: FREEING UNTRACKED LOCK 0x%p\n",lock);
    return;
  }

  if ((l->lockcount)) { 
    printlock("BAD LOCK COUNT AT FREE",l);
  }

  if ((l->irqcount)) { 
    printlock("BAD IRQ COUNT AT FREE",l);
  }

#if PRINT_LOCK_FREE
  printlock("FREE LOCK",l);
#endif

  free_lock_entry(l);
}

void palacios_lockcheck_lock(void *lock)
{
  lockcheck_state_t *l=find_lock_entry(lock);
  
  if (!l) { 
    DEBUG("LOCKCHECK: LOCKING UNTRACKED LOCK 0x%p\n",lock);
    return;
  }
  
  if (l->lockcount!=0) { 
    printlock("BAD LOCKCOUNT AT LOCK",l);
  }
  if (l->irqcount!=0) { 
    printlock("BAD IRQCOUNT AT LOCK",l);
  }
  
  l->lockcount++;
  backtrace(l->lastlocker);

  find_multiple_locks_held();

  lock_stack_lock(lock,0);

#if PRINT_LOCK_LOCK
  printlock("LOCK",l);
#endif

}
void palacios_lockcheck_unlock(void *lock)
{
  lockcheck_state_t *l=find_lock_entry(lock);
  
  if (!l) { 
    DEBUG("LOCKCHECK: UNLOCKING UNTRACKED LOCK 0x%p\n",lock);
    return;
  }
  
  if (l->lockcount!=1) { 
    printlock("LOCKCHECK: BAD LOCKCOUNT AT UNLOCK",l);
  }
  if (l->irqcount!=0) { 
    printlock("LOCKCHECK: BAD IRQCOUNT AT UNLOCK",l);
  }

  lock_stack_unlock(lock,0);
  
  l->lockcount--;
  backtrace(l->lastunlocker);

#if PRINT_LOCK_UNLOCK
  printlock("UNLOCK",l);
#endif


}

void palacios_lockcheck_lock_irqsave(void *lock,unsigned long flags)
{
  lockcheck_state_t *l=find_lock_entry(lock);
  
  if (!l) { 
    DEBUG("LOCKCHECK: IRQ LOCKING UNTRACKED LOCK 0x%p\n",lock);
    return;
  }
  
  if (l->lockcount!=0) { 
    printlock("BAD LOCKCOUNT AT IRQ LOCK",l);
  }
  if (l->irqcount!=0) { 
    printlock("BAD IRQCOUNT AT IRQ LOCK",l);
  }
  
  l->irqcount++;
  l->lastlockflags=flags;
  backtrace(l->lastirqlocker);


  find_multiple_irqs_held();

  lock_stack_lock(lock,1);

#if PRINT_LOCK_LOCK
  printlock("LOCK_IRQSAVE",l);
#endif



}

void palacios_lockcheck_unlock_irqrestore(void *lock,unsigned long flags)
{
  lockcheck_state_t *l=find_lock_entry(lock);
  
  if (!l) { 
    DEBUG("LOCKCHECK: IRQ UNLOCKING UNTRACKED LOCK 0x%p\n",lock);
    return;
  }
  
  if (l->lockcount!=0) { 
    printlock("LOCKCHECK: BAD LOCKCOUNT AT IRQ UNLOCK",l);
  }
  if (l->irqcount!=1) { 
    printlock("LOCKCHECK: BAD IRQCOUNT AT IRQ UNLOCK",l);
  }
  
  l->irqcount--;
  l->lastunlockflags = flags;

  lock_stack_unlock(lock,1);

  backtrace(l->lastirqunlocker);

#if PRINT_LOCK_UNLOCK
  printlock("UNLOCK_IRQRESTORE",l);
#endif
  
}
