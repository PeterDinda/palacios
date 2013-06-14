#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>

#include "palacios.h"

#include "memcheck.h"

// Whether not all allocations and frees will be printed
#define SHOW_ALL_ALLOCS    0

// Whether or now allocations outside of a size range will be printed
#define SHOW_THRESHOLD_WARNINGS 1
#define SMALL_KMALLOC_THRESHOLD 9
#define BIG_KMALLOC_THRESHOLD   ((1024*16)-1)
#define SMALL_VMALLOC_THRESHOLD 9
#define BIG_VMALLOC_THRESHOLD   ((1024*1024)-1)
#define SMALL_PAGE_ALLOC_THRESHOLD (4096-1)
#define BIG_PAGE_ALLOC_THRESHOLD   (4096*8-1)

// How far up the stack to track the caller
// 0 => palacios_...
// 1 => v3_alloc...
// 2 => caller of v3_alloc..
// ... 
#define STEP_BACK_DEPTH_FIRST 1
#define STEP_BACK_DEPTH_LAST  4
#define STEP_BACK_DEPTH       (STEP_BACK_DEPTH_LAST-STEP_BACK_DEPTH_FIRST+1)


typedef struct {
  int  inuse;         // 1=inuse
  palacios_memcheck_memtype_t type; 
                      // PALACIOS_KMALLOC,VMALLOC,PAGE_ALLOC
  void *addr;         // the allocated block's address
  unsigned long size; // the allocated block's size
  void *allocator[STEP_BACK_DEPTH];
                      // who allocated this
} memcheck_state_t;


// This lock is currently used only to control
// allocation of entries in the global state
static spinlock_t lock; 
static memcheck_state_t state[NUM_ALLOCS];

static void printmem(char *prefix, memcheck_state_t *m);


static memcheck_state_t *get_mem_entry(void)
{
  int i;
  unsigned long f;
  memcheck_state_t *m;

  palacios_spinlock_lock_irqsave(&lock,f);

  for (i=0;i<NUM_ALLOCS;i++) { 
    m=&(state[i]);
    if (!(m->inuse)) { 
      m->inuse=1;
      break;
    }
  }

  palacios_spinlock_unlock_irqrestore(&lock,f);
  
  if (i<NUM_ALLOCS) { 
    return m;
  } else {
    return 0;
  }
}


static memcheck_state_t *find_mem_entry(void *addr, unsigned long size, palacios_memcheck_memtype_t type)
{
  int i;
  memcheck_state_t *m;

  for (i=0;i<NUM_ALLOCS;i++) { 
    m=&(state[i]);
    if (m->inuse && m->addr == addr && m->type==type) { 
      if (size) {
	if (m->size==size) { 
	  return m;
	} else {
	  return 0;
	}
      } else {
	return m;
      }
    }
  }
  return 0;
}


static void free_mem_entry(memcheck_state_t *m)
{
  m->inuse=0;
}


void palacios_memcheck_init()
{
  memset(state,0,sizeof(memcheck_state_t)*NUM_ALLOCS);
  palacios_spinlock_init(&lock);
  DEBUG("MEMCHECK: MEMORY CHECKING INITED\n");
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


#if 0
static void clear_trace(void **trace)
{
  int i;

  for (i=0;i<STEP_BACK_DEPTH;i++) { 
    trace[i]=0;
  }
}
#endif

#define TYPE_TO_STR(type) ((type)==PALACIOS_KMALLOC ? "kmalloc" : \
                           (type)==PALACIOS_VMALLOC ? "vmalloc" : \
			   (type)==PALACIOS_PAGE_ALLOC ? "pagealloc" : "UNKNOWN")


static void printmem(char *prefix, memcheck_state_t *m)
{
  if (!m || !(m->addr) ) { 
    DEBUG("MEMCHECK: %s: memory state 0x%p BOGUS\n",prefix,m);
    return;
  }
  if (m->addr) { 
    DEBUG("MEMCHECK: %s: %s memory at 0x%p for %lu bytes allocator=" 
	  backtrace_format
	  "\n",
	  prefix,
	  TYPE_TO_STR(m->type),
	  m->addr,
	  m->size,
	  backtrace_expand(m->allocator));
  }
}


void palacios_memcheck_deinit()
{
  int i;
  memcheck_state_t *m;
  
  for (i=0;i<NUM_ALLOCS;i++) { 
    m=&(state[i]);
    if (m->inuse) { 
      printmem("ALLOCATED MEMORY AT DEINIT",m);
    } 
  }

  palacios_spinlock_deinit(&lock);

  INFO("MEMCHECK: DEINITED\n");

  // Note that this function could garbage collect at this 
  // point if we desired
}


void threshold(memcheck_state_t *m)
{
#if SHOW_THRESHOLD_WARNINGS
  switch (m->type) {
  case PALACIOS_KMALLOC:
    if (m->size < SMALL_KMALLOC_THRESHOLD ||
	m->size > BIG_KMALLOC_THRESHOLD) { 
      DEBUG("MEMCHECK: ALLOCATION EXCEEDS THRESHOLDS of %u and %u\n",
	    SMALL_KMALLOC_THRESHOLD, BIG_KMALLOC_THRESHOLD);
      printmem("ALLOCATION EXCEEDS",m);
    }
    break;
  case PALACIOS_VMALLOC:
    if (m->size < SMALL_VMALLOC_THRESHOLD ||
	m->size > BIG_VMALLOC_THRESHOLD) { 
      DEBUG("MEMCHECK: ALLOCATION EXCEEDS THRESHOLDS of %u and %u\n",
	    SMALL_VMALLOC_THRESHOLD, BIG_VMALLOC_THRESHOLD);
      printmem("ALLOCATION EXCEEDS",m);
    }
    break;
  case PALACIOS_PAGE_ALLOC:
    if (m->size < SMALL_PAGE_ALLOC_THRESHOLD ||
	m->size > BIG_PAGE_ALLOC_THRESHOLD) { 
      DEBUG("MEMCHECK: ALLOCATION EXCEEDS THRESHOLDS of %u and %u\n",
	    SMALL_PAGE_ALLOC_THRESHOLD, BIG_PAGE_ALLOC_THRESHOLD);
      printmem("ALLOCATION EXCEEDS",m);
    }
    break;
  default: 
    break;
  }
#endif
}

void find_overlaps(memcheck_state_t *m)
{
  int i;
  for (i=0;i<NUM_ALLOCS;i++) {
    memcheck_state_t *s = &(state[i]);
    if (s->inuse && s!=m && s->type==m->type) {
      if (((m->addr >= s->addr) && (m->addr < (s->addr+s->size))) ||
          (((m->addr+m->size-1) >= s->addr) && ((m->addr+m->size-1) < (s->addr+s->size))) ||
	  ((m->addr < s->addr) && (m->addr+m->size-1)>=(s->addr+s->size))) { 
	DEBUG("MEMCHECK: OVERLAP DETECTED\n");
	printmem("OVERLAP (0)",s);
	printmem("OVERLAP (1)",s);
      }
    }
  }
}

void palacios_memcheck_alloc(void *addr, unsigned long size, palacios_memcheck_memtype_t type)
{
  memcheck_state_t *m=get_mem_entry();

  if (!m) { 
    DEBUG("MEMCHECK: UNABLE TO ALLOCATE TRACKING DATA FOR %s ALLOC AT 0x%p FOR %lu BYTES\n",
	  TYPE_TO_STR(type),addr,size);
    return;
  }
  m->type=type;
  m->addr=addr;
  m->size=size;
  backtrace(m->allocator);

#if SHOW_ALL_ALLOCS
  printmem("ALLOCATE", m);
#endif

  threshold(m);
  find_overlaps(m);
}
  
void palacios_memcheck_free(void *addr,unsigned long size, palacios_memcheck_memtype_t type)
{
  memcheck_state_t *m=find_mem_entry(addr,0,type); // don't care about the size now
  
  if (!m){
    DEBUG("MEMCHECK: FREEING UNTRACKED %s MEMORY AT 0x%p FOR %lu BYTES - stack trace follows\n",TYPE_TO_STR(type),addr,size);
    dump_stack();
    return;
  }

  if (m->type==PALACIOS_PAGE_ALLOC) { 
    // need to verify sizes are identical
    if (size!=m->size) {
      DEBUG("MEMCHECK: FREEING %s MEMORY AT 0x%p FOR %lu bytes, BUT MATCHING ENTRY HAS %lu BYTES\n",TYPE_TO_STR(type),addr,size,m->size);
      printmem("MATCHING ENTRY",m);
    }
  }

#if SHOW_ALL_ALLOCS
  printmem("FREE",m);
#endif

  free_mem_entry(m);
}

