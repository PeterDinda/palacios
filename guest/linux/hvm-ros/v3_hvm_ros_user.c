/*
  Copyright (c) 2015 Peter Dinda
*/

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>

#include "v3_hvm_ros_user.h"


#define HRT_FAIL_VEC 0x1d

static FILE *log = NULL;


#if DEBUG_ENABLE==1
#define DEBUG(fmt, args...) if (log) { fprintf(log, "DEBUG: " fmt, ##args); }
#else
#define DEBUG(...)
#endif

#if INFO_ENABLE==1
#define INFO(fmt, args...) if (log) {  fprintf(log, fmt, ##args); }
        fprintf(stdout, fmt, ##args); \
    }
#else
#define INFO(...)
#endif



typedef unsigned char uchar_t;

#define rdtscll(val)					\
    do {						\
	uint64_t tsc;					\
	uint32_t a, d;					\
	asm volatile("rdtsc" : "=a" (a), "=d" (d));	\
	*(uint32_t *)&(tsc) = a;			\
	*(uint32_t *)(((uchar_t *)&tsc) + 4) = d;	\
	val = tsc;					\
    } while (0)					


/*
  This convention match the definition in palacios/include/palacios/vmm_hvm.h

  Calling convention:

64 bit:
  rax = hcall number
  rbx = 0x6464646464646464...
  rcx = 1st arg
  rdx = 2nd arg
  rsi = 3rd arg
  rdi = 4th arg
  r8  = 5th arg
  r9  = 6th arg
  r10 = 7th arg
  r11 = 8th arg

32 bit:
  eax = hcall number
  ebx = 0x32323232
  arguments on stack in C order (first argument is TOS)
     arguments are also 32 bit
*/
#define HCALL64(rc,id,a,b,c,d,e,f,g,h)		      \
  asm volatile ("movq %1, %%rax; "		      \
		"pushq %%rbx; "			      \
		"movq $0x6464646464646464, %%rbx; "   \
		"movq %2, %%rcx; "		      \
		"movq %3, %%rdx; "		      \
		"movq %4, %%rsi; "		      \
		"movq %5, %%rdi; "		      \
		"movq %6, %%r8 ; "		      \
		"movq %7, %%r9 ; "		      \
		"movq %8, %%r10; "		      \
		"movq %9, %%r11; "		      \
		"vmmcall ;       "		      \
		"movq %%rax, %0; "		      \
		"popq %%rbx; "			      \
		: "=m"(rc)			      \
		: "m"(id),			      \
                  "m"(a), "m"(b), "m"(c), "m"(d),     \
		  "m"(e), "m"(f), "m"(g), "m"(h)      \
		: "%rax","%rcx","%rdx","%rsi","%rdi", \
		  "%r8","%r9","%r10","%r11"	      \
		)

#define HCALL32(rc,id,a,b,c,d,e,f,g,h)		      \
  asm volatile ("movl %1, %%eax; "		      \
		"pushl %%ebx; "			      \
		"movl $0x32323232, %%ebx; "	      \
		"pushl %9;"			      \
		"pushl %8;"			      \
		"pushl %7;"			      \
		"pushl %6;"			      \
		"pushl %5;"			      \
		"pushl %4;"			      \
		"pushl %3;"			      \
		"pushl %2;"			      \
		"vmmcall ;       "		      \
		"movl %%eax, %0; "		      \
		"addl $32, %%esp; "		      \
		"popl %%ebx; "			      \
		: "=r"(rc)			      \
		: "m"(id),			      \
		  "m"(a), "m"(b), "m"(c), "m"(d),     \
		"m"(e), "m"(f), "m"(g), "m"(h)	      \
		: "%eax"			      \
		)

#ifdef __x86_64__
#define HCALL(rc,id,a,b,c,d,e,f,g,h)  HCALL64(rc,id,a,b,c,d,e,f,g,h)
#else
#define HCALL(rc,id,a,b,c,d,e,f,g,h)  HCALL32(rc,id,a,b,c,d,e,f,g,h)   
#endif

static inline uint64_t 
Syscall (uint64_t num, 
		 uint64_t a1,
		 uint64_t a2,
		 uint64_t a3,
		 uint64_t a4,
		 uint64_t a5,
		 uint64_t a6)
{
    uint64_t rc;

    __asm__ __volatile__ ("movq %1, %%rax; "
			  "movq %2, %%rdi; "
			  "movq %3, %%rsi; "
			  "movq %4, %%rdx; "
			  "movq %5, %%r10; "
			  "movq %6, %%r8; "
			  "movq %7, %%r9; "
			  "syscall; "
			  "movq %%rax, %0; "
			  : "=m"(rc)
			  : "m"(num), "m"(a1), "m"(a2), "m"(a3), "m"(a4), "m"(a5), "m"(a6)
			  : "%rax", "%rdi", "%rcx", "%rsi", "%rdx", "%r10", "%r8", "%r9", "%r11", "memory");

    return rc;
}


/* This must match the definition in palacios/include/palacios/vmm_hvm.h" */
struct v3_ros_event {
    enum { ROS_NONE=0, ROS_PAGE_FAULT=1, ROS_SYSCALL=2, HRT_EXCEPTION=3, HRT_THREAD_EXIT=4, ROS_DONE=5} event_type;
    uint64_t       last_ros_event_result; // valid when ROS_NONE
    union {
	struct {   // valid when ROS_PAGE_FAULT
	    uint64_t rip;
	    uint64_t cr2;
	    enum {ROS_READ, ROS_WRITE} action;
	} page_fault;
	struct { // valid when ROS_SYSCALL
	    uint64_t args[8];
	} syscall;
    struct { // valid when HRT_EXCEPTION
        uint64_t rip;
        uint64_t vector;
    } excp;
    struct { // valid when HRT_THREAD_EXIT
        uint64_t nktid;
    } thread_exit;
    };
};


int v3_hvm_ros_user_init()
{
    log = stderr;
    return 0;
}


int v3_hvm_ros_user_set_log(FILE *l)
{
    log = l;
    return 0;
}

int v3_hvm_ros_user_deinit()
{
    log = NULL;
    return 0;
}


int v3_hvm_handle_ros_event_nohcall(struct v3_ros_event *event)
{
    uint64_t rc;
    char t;
    
    switch (event->event_type) { 
	case ROS_PAGE_FAULT: 
	    // force the ros kernel to the PTE
	    if (event->page_fault.action==ROS_READ) { 
		DEBUG("EVENT [PF READ] : %p\n", (volatile char*)(event->page_fault.cr2));
		t=*(volatile char*)(event->page_fault.cr2);
		t=t; // avoid wanting for this throwaway
	    } else if (event->page_fault.action==ROS_WRITE) { 
		DEBUG("EVENT [PF WRITE] : %p\n", (volatile char*)(event->page_fault.cr2));
		
		// stash the old contents
		char tmp = *((volatile char *)event->page_fault.cr2);
		
		// force a write
		*((volatile char*)event->page_fault.cr2) = 0xa;
		
		// put the old value back
		*((volatile char*)event->page_fault.cr2) = tmp;
		
	    } else {
		INFO("Huh?\n");
	    }
	    
	    DEBUG("Done.");
	    event->last_ros_event_result = 0;
	    //event->event_type            = ROS_DONE;
	    event->event_type            = ROS_NONE;
	    // completed
	    DEBUG("[OK]\n");
	    
	    break;
	    
	case ROS_SYSCALL:
	    DEBUG("EVENT [SYSCALL] : syscall(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
		  event->syscall.args[0],
		  event->syscall.args[1],
		  event->syscall.args[2],
		  event->syscall.args[3],
		  event->syscall.args[4],
		  event->syscall.args[5],
		  event->syscall.args[6],
		  event->syscall.args[7]);
	    
	    rc = Syscall(event->syscall.args[0],
			 event->syscall.args[1],
			 event->syscall.args[2],
			 event->syscall.args[3],
			 event->syscall.args[4],
			 event->syscall.args[5],
			 event->syscall.args[6]);
	    
	    if ((long int)rc<0) {
		DEBUG("Syscall (num=0x%llx) failed (rc=0x%llx)\n", (unsigned long long)event->syscall.args[0], (unsigned long long)rc);
	    }
	    
	    DEBUG("Return = 0x%lx...", rc);
	    event->last_ros_event_result = rc;
	    //event->event_type            = ROS_DONE;
	    event->event_type            = ROS_NONE;
	    DEBUG("[OK]\n");
	    
	    break;
	    
	case HRT_EXCEPTION:
	    DEBUG("EVENT [EXCEPTION] : (tid=%lx) at %p vector=0x%lx...", syscall(SYS_gettid), (void*)event->excp.rip, event->excp.vector);
	    if (event->excp.vector == HRT_FAIL_VEC) {
		INFO("HRT execution failed due to exception in HRT\n");
	    } else {
		INFO("HRT execution failed due to exception in ROS (vec=0x%lx)\n", event->excp.vector);
	    }
	    
	    event->last_ros_event_result = 0;
	    //event->event_type            = ROS_DONE;
	    event->event_type            = ROS_NONE;
	    DEBUG("[OK]\n");
	    exit(EXIT_FAILURE);
	    break;

	default:
	    DEBUG( "Unknown ROS event 0x%x\n", event->event_type);
	    break;
    }
    
    return 0;
}


int v3_hvm_handle_ros_event(struct v3_ros_event *event)
{
    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    char t;

    switch (event->event_type) { 
	case ROS_PAGE_FAULT: 
	    // force the ros kernel to the PTE
	    if (event->page_fault.action==ROS_READ) { 
		DEBUG("Handling page fault read for %p\n", (volatile char*)(event->page_fault.cr2));
		t=*(volatile char*)(event->page_fault.cr2);
		t=t; // avoid wanting for this throwaway
	    } else if (event->page_fault.action==ROS_WRITE) { 
		DEBUG("Handling page fault write for %p\n", (volatile char*)(event->page_fault.cr2));
		
		// stash the old contents
		char tmp = *((volatile char *)event->page_fault.cr2);
		
		// force a write
		*((volatile char*)event->page_fault.cr2) = 0xa;
		
		// put the old value back
		*((volatile char*)event->page_fault.cr2) = tmp;
		
	    } else {
		INFO("Huh?\n");
	    }
	    
	    DEBUG("Done - doing hypercall...");
	    num = 0xf00d;
	    a1 = 0x1f;
	    a2 = 0; // success
	    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
	    // completed
	    DEBUG("[OK]\n");
	    
	    break;
	    
	case ROS_SYSCALL:
	    DEBUG("Doing system call: syscall(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
		  event->syscall.args[0],
		  event->syscall.args[1],
		  event->syscall.args[2],
		  event->syscall.args[3],
		  event->syscall.args[4],
		  event->syscall.args[5],
		  event->syscall.args[6],
		  event->syscall.args[7]);
	    
	    rc = Syscall(event->syscall.args[0],
			 event->syscall.args[1],
			 event->syscall.args[2],
			 event->syscall.args[3],
			 event->syscall.args[4],
			 event->syscall.args[5],
			 event->syscall.args[6]);
	    
	    if ((long int)rc<0) {
		DEBUG("Syscall (num=0x%llx) failed (rc=0x%llx)\n", (unsigned long long)event->syscall.args[0], (unsigned long long)rc);
	    }
	    
	    DEBUG("Return = 0x%llx, doing hypercall...", rc);
	    num = 0xf00d;
	    a1 = 0x1f;
	    a2 = rc;
	    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
	    DEBUG("[OK]\n");
	    
	    break;

	case HRT_EXCEPTION:
	    DEBUG("Handling exception (tid=%lx) at %p vector=0x%lx...", syscall(SYS_gettid), (void*)event->excp.rip, event->excp.vector);
	    if (event->excp.vector == HRT_FAIL_VEC) {
		INFO("HRT execution failed due to exception in HRT\n");
	    } else {
		INFO("HRT execution failed due to exception in ROS (vec=0x%lx)\n", event->excp.vector);
	    }
	    
	    num = 0xf00d;
	    a1 = 0x1f;
	    a2 = rc;
	    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
	    DEBUG("[OK]\n");
	    exit(EXIT_FAILURE);
	    break;
	    
	default:
	    DEBUG( "Unknown ROS event 0x%x\n", event->event_type);
	    break;
    }
    
    return 0;
}

static void wait_for_merge_completion()
{
    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    struct v3_ros_event event;
    
    memset(&event, 1, sizeof(event));
    
    rc = 1;
    
    while (rc) { 
	num = 0xf00d;
	a1 = 0xf;
	a2 = (unsigned long long) &event;
	HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    }
    
}


static void wait_for_completion()
{
  unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
  struct v3_ros_event event;

  memset(&event, 1, sizeof(event));

  rc = 1;

  while (rc) { 
    num = 0xf00d;
    a1 = 0xf;
    a2 = (unsigned long long) &event;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) { 
	//      usleep(100);
	if (event.event_type != ROS_NONE) { 
	    v3_hvm_handle_ros_event(&event);
	}
    }
  }
}


int v3_hvm_ros_install_hrt_image(void *image, uint64_t size)
{
    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long i;
    volatile long long sum=0;

    num = 0xf00d;
    a1 = 0x8; // install image
    a2 = (unsigned long long) image;
    a3 = size;

    // touch the whoel image to make it has ptes
    for (i=0;i<size;i++) { 
	sum+=((char*)image)[i];
    }

    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);

    if (rc) { 
	return -1;
    } else {
	return 0;
    }
}

int v3_hvm_ros_reset(reset_type what)
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;
    
    num=0xf00d;
    switch (what) { 
	case RESET_ROS:
	    a1 = 0x1;
	    break;
	case RESET_HRT:
	    a1 = 0x2;
	    break;
	case RESET_BOTH:
	    a1 = 0x3;
	    break;
    }
    
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    
    if (rc) {
	INFO("Error in request to reset rc=0x%llx\n",rc);
	return -1;
    } else {
	// no waiting for completion here
	return 0;
    }
}


int v3_hvm_ros_merge_address_spaces()
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    a1 = 0x30; // merge address spaces
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) {
      INFO("Error in request to merge address spaces rc=0x%llx\n",rc);
      return -1;
    } else {
      wait_for_merge_completion();
      return 0;
    }
}

int v3_hvm_ros_unmerge_address_spaces()
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    a1 = 0x31; // merge address spaces
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) {
      INFO("Error in request to unmerge address spaces rc=0x%llx\n",rc);
      return -1;
    } else {
      wait_for_completion();
      return 0;
    }
}


int v3_hvm_ros_mirror_gdt(uint64_t fsbase, uint64_t cpu)
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    a1 = 0x51; // mirror the ROS FS and GS on the HRT side
    a2 = fsbase;
    a3 = cpu;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) {
      INFO("Error in request to mirror GDT (rc=0x%llx)\n", rc);
      return -1;
    } else {
      wait_for_completion();
      return 0;
    }

}

int v3_hvm_ros_unmirror_gdt()
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    a1 = 0x53; // unmirror GDT and the rest
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) {
      INFO("Error in request to unmirror GDT (rc=0x%llx)\n", rc);
      return -1;
    } else {
      wait_for_completion();
      return 0;
    }
}


int v3_hvm_ros_invoke_hrt_async(void *buf, int par)
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    if (par) { 
	a1 = 0x21; // issue "function" in parallel
    } else {
	a1 = 0x20; // issue "function" sequentially
    }
    a2 = (unsigned long long) buf;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) { 
	INFO("Error in request to launch %s function rc=0x%llx\n", par ? "parallel" : "", rc);
	return -1;
    } else {
	wait_for_completion();
	return 0;
    }
}

int v3_hvm_ros_invoke_hrt_async_nowait(void *buf, int par)
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    if (par) { 
	a1 = 0x21; // issue "function" in parallel
    } else {
	a1 = 0x20; // issue "function" sequentially
    }
    a2 = (unsigned long long) buf;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    return 0;
}





/*
  Synchronous operation model 

  On ROS:

  [0] => issue count
  [1] => completion count
  [2] => function call ptr

  1. merge
  2. indicate this is the address for sync
  3. ++[0]
  4. wait for [1] to match
  5. goto 3

  On HRT:

  1. merge
  2. cnt=1;
  3. wait for [0] to get to cnt
  4. exec
  5. ++[1]   ++cnt
  6. goto 3
*/

static volatile unsigned long long sync_proto[3]={0,0,0};


static void wait_for_sync()
{
  unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
  struct v3_ros_event event;

  memset(&event, 1, sizeof(event));
  
  rc = 1;

  while (rc!=4) { 
    num = 0xf00d;
    a1 = 0xf;
    a2 = (unsigned long long) &event;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc!=4) { 
	//      usleep(100);
	if (event.event_type != ROS_NONE) { 
	    v3_hvm_handle_ros_event(&event);
	}
    }
  }
}


int v3_hvm_ros_synchronize()
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    // make sure this address is touched, then zero
    sync_proto[0]=sync_proto[1]=sync_proto[2]=1;
    sync_proto[0]=sync_proto[1]=sync_proto[2]=0;

    num=0xf00d;
    a1 = 0x28; // issue sync request setup
    a2 = (unsigned long long) sync_proto;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    
    if (rc) { 
	INFO("Synchronize call failed with rc=0x%llx\n",rc);
	return -1;
    } else {
	wait_for_sync();
	return 0;
    }
}


int v3_hvm_ros_desynchronize()
{
    unsigned long long num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
    unsigned long long rc;

    num=0xf00d;
    a1 = 0x29; // issue sync request teardown
    a2 = (unsigned long long) sync_proto;
    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);

    if (rc) { 
	INFO("Desynchronize call failed with rc=0x%llx\n",rc);
	return -1;
    } else {
	wait_for_completion();
	return 0;
    }
}

#define HOW_OFTEN 1000000

int v3_hvm_ros_invoke_hrt_sync(void *buf, int ros)
{
    int i;
    sync_proto[2]=(unsigned long long)buf;
    sync_proto[0]++;

    i=0;
    while (sync_proto[1] != sync_proto[0]) {
	i++;
	if (ros && (!i%HOW_OFTEN)) { 
	    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;
	    struct v3_ros_event event;

	    memset(&event, 1, sizeof(event));

	    num = 0xf00d;
	    a1 = 0xf;
	    a2 = (unsigned long long) &event;
	    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
	    if (event.event_type != ROS_NONE) { 
		v3_hvm_handle_ros_event(&event);
	    }
	}
    }   
    return 0;
}

extern void *__v3_hvm_ros_signal_handler_stub;

void (*__v3_hvm_ros_signal_handler)(uint64_t) = 0;
static void *__v3_hvm_ros_signal_handler_stack = 0;
static uint64_t __v3_hvm_ros_signal_handler_stack_size=0;

int v3_hvm_ros_register_signal(void (*h)(uint64_t), void *stack, uint64_t stack_size)
{
    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;

    if (mlock(stack,stack_size)) { 
	INFO("Can't pin stack - proceeding\n");
    }

    // clear it and touch it
    memset(stack,0,stack_size);

    __v3_hvm_ros_signal_handler_stack = stack;
    __v3_hvm_ros_signal_handler_stack_size = stack_size;
    __v3_hvm_ros_signal_handler = h;

    // Attempt to install
    
    num = 0xf00d;
    a1 = 0x40;
    a2 = (unsigned long long) &__v3_hvm_ros_signal_handler_stub;
    a3 = (unsigned long long) stack + stack_size - 16; // put us at the top of the stack, and 16 byte aligned

    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    if (rc) {
	INFO("Failed to install HVM signal handler\n");
	return -1;
    } 

    return 0;
}

int v3_hvm_ros_unregister_signal()
{
    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;

    // an unregister boils down to setting handler to null
    num = 0xf00d;
    a1 = 0x40;
    a2 = (unsigned long long) 0;
    a3 = (unsigned long long) 0; 

    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);

    if (rc) {
	INFO("Failed to uninstall HVM signal handler\n");
    } 

    // and now do user space cleanup

    __v3_hvm_ros_signal_handler = 0;

    if (__v3_hvm_ros_signal_handler_stack) { 
	munlock(__v3_hvm_ros_signal_handler_stack,__v3_hvm_ros_signal_handler_stack_size);
	__v3_hvm_ros_signal_handler_stack = 0;
	__v3_hvm_ros_signal_handler_stack_size = 0;
    }
    
    if (rc) { 
	return -1;
    } else {
	return 0;
    }
}


int  v3_hvm_hrt_signal_ros(uint64_t code)
{
    unsigned long long rc, num, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0, a8=0;

    num = 0xf00d;
    a1 = 0x41;
    a2 = (unsigned long long) code;

    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);

    return rc ? -1 : rc;

}

    
