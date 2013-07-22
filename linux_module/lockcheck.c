#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/sort.h>
#include <linux/timer.h>
#include <linux/time.h>

#include "palacios.h"

#include "lockcheck.h"


// Number of outputs possible before output stops
// set to zero to remove the limit
#define OUTPUT_LIMIT      0
//
// Number of outputs to skip before
// printing and counting
#define OUTPUT_SKIP       0

//
// Whether or not to print these events
//
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

// Detect when last lock a processor holds is released
// but interrupts remain off, similarly for first acquire
#define CHECK_IRQ_LAST_RELEASE   0
#define CHECK_IRQ_FIRST_ACQUIRE  1

// Show hottest locks every this many locks or lock_irqsaves
// 0 indicates this should not be shown
#define HOT_LOCK_INTERVAL        100000

// How often the long lock timer checker should run
// And how long a lock needs to be held before it complains
// set to zero to deactivate
#define LONG_LOCK_INTERVAL_MS    10
#define LONG_LOCK_HELD_MS        20

//
// Whether lockcheck should lock its own data structures during an
// event (alloc, dealloc, lock, unlock, etc) and the subsequent
// checking.  If this is off, it will only lock on a lock allocation,
// in order to assure each lock has a distinct slot.  If this is off,
// lockcheck may miss problems (or see additional problems) due to
// internal race conditions.  However, if it is on, lockcheck is
// essentially adding a global lock acuire to each lock operation in
// palacios, which will perturb palacios.
//
#define LOCK_SELF 1


typedef struct {
  int  inuse;         // nonzero if this is in use
  void *lock;         // the lock
  void *holder;       // current thread holding it
  u32   holdingcpu;   // on which cpu it acquired it
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
  unsigned long locktime; 
                      // when it was locked
  void *lastirqunlocker[STEP_BACK_DEPTH]
                    ; // who last unlocked
  unsigned long lastunlockflags; // their flags
  int   hotlockcount; // how many times it's been locked
} lockcheck_state_t;


// This lock is currently used only to control
// allocation of entries in the global state
static spinlock_t mylock; 
static lockcheck_state_t state[NUM_LOCKS];
static lockcheck_state_t *sorted_state[NUM_LOCKS];

static int numout=0;

static int globallockcount=0;

static struct timer_list  long_lock_timer;


#define DEBUG_OUTPUT(fmt, args...)					\
do {									\
  numout++;   								\
  if (numout>=OUTPUT_SKIP) {                                            \
    if (OUTPUT_LIMIT==0 ||(numout-OUTPUT_SKIP)<OUTPUT_LIMIT) {	        \
      DEBUG(fmt, ##args);						\
    } else {								\
      if ((numout-OUTPUT_SKIP)==OUTPUT_LIMIT) {			        \
        DEBUG("LOCKCHECK: Output limit hit - no further printouts\n");	\
      }									\
    }									\
  }                                                                     \
} while (0)

#define DEBUG_DUMPSTACK()			                        \
do {						                        \
  if (numout>=OUTPUT_SKIP) {                                            \
    if (OUTPUT_LIMIT==0 || (numout-OUTPUT_SKIP)<OUTPUT_LIMIT) {	        \
      dump_stack();				                        \
    }                                                                   \
  }						                        \
} while (0)

#if LOCK_SELF
#define LOCK_DECL unsigned long f
#define LOCK() spin_lock_irqsave(&mylock,f)
#define UNLOCK() spin_unlock_irqrestore(&mylock,f)
#define LOCK_ALLOC_DECL 
#define LOCK_ALLOC()  
#define UNLOCK_ALLOC() 
#else
#define LOCK_DECL 
#define LOCK()  
#define UNLOCK() 
#define LOCK_ALLOC_DECL unsigned long f
#define LOCK_ALLOC() spin_lock_irqsave(&mylock,f)
#define UNLOCK_ALLOC() spin_unlock_irqrestore(&mylock,f)
#endif

static void printlock(char *prefix, lockcheck_state_t *l);

typedef struct {
  u32 top;               // next open slot 0..
  void *lock[LOCK_STACK_DEPTH]; // the stack
  char irq[LOCK_STACK_DEPTH];   // locked with irqsave?
  char irqoff[LOCK_STACK_DEPTH]; // if so, what was the flag?
} lock_stack_t;

static DEFINE_PER_CPU(lock_stack_t, lock_stack);

static lockcheck_state_t *get_lock_entry(void)
{
  int i;
  lockcheck_state_t *l;
  LOCK_ALLOC_DECL;

  LOCK_ALLOC();
  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (!(l->inuse)) { 
      l->inuse=1;
      break;
    }
  }
  UNLOCK_ALLOC();
  
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
      snprintf(buf,64,"LOCK STACK (cpu=%u, index=%u, irq=%d, irqoff=%d)",cpu, i-1, (int)(mystack->irq[i-1]), (int)(mystack->irqoff[i-1]));
      printlock(buf,find_lock_entry(mystack->lock[i-1]));
    }
  }
  put_cpu_var(lock_stack);
}


static void lock_stack_lock(void *lock, char irq, unsigned long flags)
{
  lock_stack_t *mystack = &(get_cpu_var(lock_stack));
  u32 cpu = get_cpu();  put_cpu();

  if (mystack->top>=(LOCK_STACK_DEPTH-1)) {
    put_cpu_var(lock_stack);
    DEBUG_OUTPUT("LOCKCHECK: Locking lock 0x%p on cpu %u exceeds stack limit of %d\n",lock,cpu,LOCK_STACK_DEPTH);
    lock_stack_print();
  } else {
    int oldtop = mystack->top;
    mystack->lock[mystack->top] = lock;
    mystack->irq[mystack->top] = irq;
    mystack->irqoff[mystack->top] = irqs_disabled_flags(flags);
    mystack->top++;
    put_cpu_var(lock_stack);
    if (CHECK_IRQ_FIRST_ACQUIRE && oldtop==0 && irqs_disabled_flags(flags) ) { 
       DEBUG_OUTPUT("LOCKCHECK: First lock on lock stack of processor %d but irqs were already disabled - stack trace follows\n", cpu);
       DEBUG_DUMPSTACK();
    }
       
  }
  
  
}

static void lock_stack_unlock(void *lock, char irq, unsigned long flags)
{
  lock_stack_t *mystack = &(get_cpu_var(lock_stack));
  u32 cpu = get_cpu(); put_cpu();

  if (mystack->top==0) {
    put_cpu_var(lock_stack);
    DEBUG_OUTPUT("LOCKCHECK: Unlocking lock 0x%p on cpu %u when lock stack is empty\n",lock,cpu);
  } else {
    if (mystack->lock[mystack->top-1] != lock) { 
      void *otherlock=mystack->lock[mystack->top-1];
      put_cpu_var(lock_stack);
      DEBUG_OUTPUT("LOCKCHECK: Unlocking lock 0x%p on cpu %u when top of stack is lock 0x%p\n",lock,cpu, otherlock);
      lock_stack_print();
    } else {
      if (irq!=mystack->irq[mystack->top-1]) {
	char otherirq = mystack->irq[mystack->top-1];
	put_cpu_var(lock_stack);
	DEBUG_OUTPUT("LOCKCHECK: Unlocking lock 0x%p on cpu %u with irq=%d, but was locked with irq=%d\n",lock,cpu,irq,otherirq);
	lock_stack_print();
        mystack = &(get_cpu_var(lock_stack));
        mystack->top--;
        put_cpu_var(lock_stack);
      } else {
	if (irq) { 
          if (irqs_disabled_flags(flags)!=mystack->irqoff[mystack->top-1]) {
             char otherirqoff = mystack->irqoff[mystack->top-1];
             put_cpu_var(lock_stack);
             DEBUG_OUTPUT("LOCKCHECK: Unlocking lock 0x%p on cpu %u sets irqoff=%d but the matching lock returned irqoff=%d\n", lock, cpu, irqs_disabled_flags(flags), otherirqoff);
             lock_stack_print();
             mystack = &(get_cpu_var(lock_stack));
             mystack->top--;
             put_cpu_var(lock_stack);
          } else {
            // irq, and irq states match - good
            mystack->top--;
            put_cpu_var(lock_stack);
          }
        } else {
          // !irq - we are good
 	  mystack->top--;
	  put_cpu_var(lock_stack);
        }
      }
    }
  }

  mystack = &(get_cpu_var(lock_stack));
  if (mystack->top == 0) {
    put_cpu_var(lock_stack);
    if (CHECK_IRQ_LAST_RELEASE && irqs_disabled()) { 
       DEBUG_OUTPUT("LOCKCHECK: Lock stack on cpu %u is now empty, but irqs are still disabled! Stack trace follows\n", cpu);
       DEBUG_DUMPSTACK();
    }
  } else {
    put_cpu_var(lock_stack);
  }

}


// pointers are to the pointers in the sorted_state array
int compare(const void *a, const void *b)
{
  lockcheck_state_t *l = *((lockcheck_state_t **)a);
  lockcheck_state_t *r = *((lockcheck_state_t **)b);

  return -(l->hotlockcount - r->hotlockcount);
}

static void hot_lock_show(void)
{
  int n, i;
  char buf[64];

  n=0;
  for (i=0;i<NUM_LOCKS;i++) { 
    if (state[i].inuse) { 
      sorted_state[n]=&(state[i]);
      n++;
    }
  }

  sort(sorted_state,n,sizeof(lockcheck_state_t *),compare,NULL);
  
  for (i=0;i<n;i++) {
    snprintf(buf,64,"HOT LOCK (%d of %d) %d acquires", i,n,sorted_state[i]->hotlockcount);
    printlock(buf,sorted_state[i]);
  }
}


static void hot_lock_lock(void *lock)
{ 
  lockcheck_state_t *l = find_lock_entry(lock);
   
  if (!l) { return; }

  l->hotlockcount++;
  globallockcount++;

  if (HOT_LOCK_INTERVAL && !(globallockcount % HOT_LOCK_INTERVAL )) {
    DEBUG_OUTPUT("LOCKCHECK: Hot locks after %d acquires Follow\n",globallockcount);
    hot_lock_show();
  }
}


#define hot_lock_unlock(X) // nothing for now


void long_lock_callback(unsigned long ignored)
{
  // See if any lock has been held for too long
  // then print all the locks if it has

  // Note that this function does not lock because the 
  // idea is to try to catch a deadlock, which might
  // imply that we are stalled waiting on our lock already
  
  int i;
  int have=0;
  lockcheck_state_t *l;
  char buf[64];
  unsigned long now;


  now = jiffies;

  for (i=0;i<NUM_LOCKS;i++) { 
    l=&(state[i]);
    if (l->inuse && (l->irqcount>0 || l->lockcount>0)) {
      if (jiffies_to_msecs(now-l->locktime)>LONG_LOCK_HELD_MS) { 
	have++;
      }
    }
  }
  
  if (have>0) { 
    DEBUG_OUTPUT("LOCKCHECK: LONG LOCK: %d locks have been held longer than %u ms - Dump of all held locks Follows\n",have,LONG_LOCK_HELD_MS);
    have=0;
    for (i=0;i<NUM_LOCKS;i++) { 
      l=&(state[i]);
      if (l->inuse && (l->irqcount>0 || l->lockcount>0)) { 
	snprintf(buf,64,"LONG LOCK (%d - %s)",
		 have,
		 jiffies_to_msecs(now-l->locktime)>LONG_LOCK_HELD_MS ? "**OLD**" : "ok");
	printlock(buf,l);
	have++;
      }
    }
  }
  
  // reset the timer to run again
  
  mod_timer(&long_lock_timer,jiffies+msecs_to_jiffies(LONG_LOCK_INTERVAL_MS));

}


void palacios_lockcheck_init()
{
  memset(state,0,sizeof(lockcheck_state_t)*NUM_LOCKS);
  spin_lock_init(&mylock);

#if LONG_LOCK_INTERVAL_MS>0
  setup_timer(&long_lock_timer,long_lock_callback,0);
  mod_timer(&long_lock_timer, jiffies + msecs_to_jiffies(LONG_LOCK_INTERVAL_MS));
#endif

  DEBUG_OUTPUT("LOCKCHECK: LOCK CHECKING INITED (internal locking: %d, output limit: %d, output skip: %d)\n",LOCK_SELF,OUTPUT_LIMIT,OUTPUT_SKIP);
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
    DEBUG_OUTPUT("LOCKCHECK: %s: lock 0x%p BOGUS\n",prefix,l);
    return;
  }
  if (l->lock) { 
    DEBUG_OUTPUT("LOCKCHECK: %s: lock 0x%p, holder %s on %u, allocator=" 
	  backtrace_format
          ", lockcount=%d, lastlocker="
	  backtrace_format
	  ", lastunlocker="
	  backtrace_format
	  ", irqcount=%d, lastirqlocker="
	  backtrace_format
	  ", lastlockflags=%lu, lastirqunlocker="
	  backtrace_format
	  ", lastunlockflags=%lu"
  	  ", lockage=%ums\n",
 	  prefix,l->lock,
	  l->holder ? ((struct task_struct *)(l->holder))->comm : "none",
	  l->holdingcpu,
	  backtrace_expand(l->allocator),
	  l->lockcount,
	  backtrace_expand(l->lastlocker),
	  backtrace_expand(l->lastunlocker),
	  l->irqcount,
	  backtrace_expand(l->lastirqlocker),
	  l->lastlockflags,
	  backtrace_expand(l->lastirqunlocker),
          l->lastunlockflags,
 	  (l->lockcount>0) || (l->irqcount>0) ? jiffies_to_msecs(jiffies-l->locktime) : 0);
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
  LOCK_DECL;
  
  LOCK();

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
#if LONG_LOCK_INTERVAL_MS>0
  del_timer(&long_lock_timer);
#endif
  UNLOCK();
  INFO("LOCKCHECK: DEINITED\n");
}


void palacios_lockcheck_alloc(void *lock)
{
  lockcheck_state_t *l;
  LOCK_DECL;

  LOCK();

  l=get_lock_entry();
  
  if (!l) { 
    DEBUG_OUTPUT("LOCKCHECK: UNABLE TO ALLOCATE TRACKING DATA FOR LOCK 0x%p\n",lock);
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
  
  UNLOCK();
}
  
void palacios_lockcheck_free(void *lock)
{
  lockcheck_state_t *l;
  LOCK_DECL;

  LOCK();
  l=find_lock_entry(lock);
  
  if (!l){
    UNLOCK();
    DEBUG_OUTPUT("LOCKCHECK: FREEING UNTRACKED LOCK 0x%p - stack trace follows\n",lock);
    DEBUG_DUMPSTACK();
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

  UNLOCK();

}

void palacios_lockcheck_lock(void *lock)
{
  LOCK_DECL;
  lockcheck_state_t *l;


  LOCK();

  l=find_lock_entry(lock); 
 
  if (!l) { 
    UNLOCK();
    DEBUG_OUTPUT("LOCKCHECK: LOCKING UNTRACKED LOCK 0x%p - stack follows\n",lock);
    DEBUG_DUMPSTACK();
    return;
  }
  
  if (l->lockcount!=0) { 
    printlock("BAD LOCKCOUNT AT LOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  if (l->irqcount!=0) { 
    printlock("BAD IRQCOUNT AT LOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  
  l->lockcount++;
  l->holder=current;
  l->holdingcpu=get_cpu(); put_cpu();
  l->locktime=jiffies;
  backtrace(l->lastlocker);

  find_multiple_locks_held();

  lock_stack_lock(lock,0,0);

  hot_lock_lock(lock);

#if PRINT_LOCK_LOCK
  printlock("LOCK",l);
#endif

  UNLOCK();

}
void palacios_lockcheck_unlock(void *lock)
{
  LOCK_DECL;
  lockcheck_state_t *l;

  LOCK();

  l=find_lock_entry(lock);
  
  if (!l) { 
    UNLOCK();
    DEBUG_OUTPUT("LOCKCHECK: UNLOCKING UNTRACKED LOCK 0x%p - stack follows\n",lock);
    DEBUG_DUMPSTACK();
    return;
  }
  
  if (l->lockcount!=1) { 
    printlock("LOCKCHECK: BAD LOCKCOUNT AT UNLOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  if (l->irqcount!=0) { 
    printlock("LOCKCHECK: BAD IRQCOUNT AT UNLOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }

  lock_stack_unlock(lock,0,0);

  hot_lock_unlock(lock);
  
  l->holder=0;
  l->holdingcpu=0;
  l->lockcount--;
  backtrace(l->lastunlocker);

#if PRINT_LOCK_UNLOCK
  printlock("UNLOCK",l);
#endif

  UNLOCK();

}

void palacios_lockcheck_lock_irqsave(void *lock,unsigned long flags)
{
  LOCK_DECL;
  lockcheck_state_t *l;
  
  LOCK();

  l=find_lock_entry(lock);

  if (!l) { 
    UNLOCK();
    DEBUG_OUTPUT("LOCKCHECK: IRQ LOCKING UNTRACKED LOCK 0x%p - stack follows\n",lock);
    DEBUG_DUMPSTACK();
    return;
  }
  
  if (l->lockcount!=0) { 
    printlock("BAD LOCKCOUNT AT IRQ LOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  if (l->irqcount!=0) { 
    printlock("BAD IRQCOUNT AT IRQ LOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  
  l->irqcount++;
  l->holder=current;
  l->holdingcpu=get_cpu(); put_cpu();
  l->lastlockflags=flags;
  l->locktime=jiffies;
  backtrace(l->lastirqlocker);

  find_multiple_irqs_held();

  lock_stack_lock(lock,1,flags);

  hot_lock_lock(lock);

#if PRINT_LOCK_LOCK
  printlock("LOCK_IRQSAVE",l);
#endif

  UNLOCK();


}


//
// This is separated into two components to avoid a race between
// the underlying spin_unlock_irqrestore and the next lockcheck_lock_irqsave
// If simply record the state after the unlock, we might see that the 
// irqcount has already increased.  Therefore, we will acquire the 
// lockchecker lock in _pre and release it in _post.  Note that when
// we release the lock in _post, we restore the flags provided by the 
// code under test - NOT our original flags
//
// unlock_pre() - stores flags, takes mylock discard flags
// At this point, a lockcheck_lock cannot enter, since it's stuck on mylock
// spinunlock - restores lock, restores original flags
// unlock_post() - restores mylock WITH orig flags
//
void palacios_lockcheck_unlock_irqrestore_pre(void *lock,unsigned long flags)
{
  LOCK_DECL;
  
  LOCK();  // flags are discarded
  // at this point, the actual spin unlock can run 
  // if another thread hits lockcheck_irqsave at this point, it
  // will block on mylock
}

void palacios_lockcheck_unlock_irqrestore_post(void *lock,unsigned long flags)
{
  LOCK_DECL;
  lockcheck_state_t *l;

#if LOCK_SELF
  // when we unlock, want to restore the flags *the user wants*
  f = flags;
#endif

  // Note that we DO NOT take mylock here, since we already took it in
  // _pre

  l=find_lock_entry(lock);

  if (!l) { 
    UNLOCK();  // release any waiting threads on lockcheck_lock_irqsave
    DEBUG_OUTPUT("LOCKCHECK: IRQ UNLOCKING UNTRACKED LOCK 0x%p - stack follows\n",lock);
    DEBUG_DUMPSTACK();
    return;
  }
  
  if (l->lockcount!=0) { 
    printlock("LOCKCHECK: BAD LOCKCOUNT AT IRQ UNLOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  if (l->irqcount!=1) { 
    printlock("LOCKCHECK: BAD IRQCOUNT AT IRQ UNLOCK - stack follows",l);
    DEBUG_DUMPSTACK();
  }
  
  l->holder=0;
  l->holdingcpu=0;
  l->irqcount--;
  l->lastunlockflags = flags;

  lock_stack_unlock(lock,1,flags);

  hot_lock_unlock(lock);

  backtrace(l->lastirqunlocker);

#if PRINT_LOCK_UNLOCK
  printlock("UNLOCK_IRQRESTORE",l);
#endif
  UNLOCK(); // release any waiting threads on lockcheck_lock_irqsave

}
