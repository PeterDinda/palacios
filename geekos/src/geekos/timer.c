/*
 * GeekOS timer interrupt support
 * Copyright (c) 2001,2003 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * $Revision: 1.11 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <limits.h>
#include <geekos/io.h>
#include <geekos/int.h>
#include <geekos/irq.h>
#include <geekos/kthread.h>
#include <geekos/timer.h>

#include <geekos/serial.h>
#include <geekos/debug.h>

#include <geekos/io_defs.h>

#include <geekos/vmm_stubs.h>

/* PAD this currently is in nvram.c */
/* JRL: This is completely broken
extern void deliver_timer_interrupt_to_vmm(uint_t period_us);
*/

/* JRL Add a cpu frequency measurement */
uint_t cpu_khz_freq;










#if defined(__i386__)

#define rdtscll(val)                            \
     __asm__ __volatile__("rdtsc" : "=A" (val))

#elif defined(__x86_64__)

#define rdtscll(val) do {                                   \
    unsigned int a,d;                                       \
    asm volatile("rdtsc" : "=a" (a), "=d" (d));             \
    (val) = ((unsigned long)a) | (((unsigned long)d)<<32);  \
  } while(0)

#endif

#define do_div(n,base) ({				     \
      unsigned long __upper, __low, __high, __mod, __base;   \
      __base = (base);					     \
      asm("":"=a" (__low), "=d" (__high):"A" (n));	     \
      __upper = __high;					     \
      if (__high) {					     \
	__upper = __high % (__base);			     \
	__high = __high / (__base);			     \
      }									\
      asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
      asm("":"=A" (n):"a" (__low),"d" (__high));			\
      __mod;								\
    })


/**
 * This uses the Programmable Interval Timer that is standard on all
 * PC-compatible systems to determine the time stamp counter frequency.
 *
 * This uses the speaker output (channel 2) of the PIT. This is better than
 * using the timer interrupt output because we can read the value of the
 * speaker with just one inb(), where we need three i/o operations for the
 * interrupt channel. We count how many ticks the TSC does in 50 ms.
 *
 * Returns the detected time stamp counter frequency in KHz.
 */


static unsigned int 
pit_calibrate_tsc(void)
{
  uint_t khz = 0;
  ullong_t start, end;
  //        unsigned long flags;
  unsigned long pit_tick_rate = 1193182UL;  /* 1.193182 MHz */
  
  //        spin_lock_irqsave(&pit_lock, flags);
  
  outb((inb(0x61) & ~0x02) | 0x01, 0x61);
  
  outb(0xb0, 0x43);
  outb((pit_tick_rate / (1000 / 50)) & 0xff, 0x42);
  outb((pit_tick_rate / (1000 / 50)) >> 8, 0x42);
  //        start = get_cycles_sync();
  rdtscll(start);
  while ((inb(0x61) & 0x20) == 0);
  rdtscll(end);
  //   end = get_cycles_sync();
  
  //        spin_unlock_irqrestore(&pit_lock, flags);
  
  
  //  return (end - start) / 50;
  khz = end - start;
;

  return khz / 50;
}



/* END JRL */



#define MAX_TIMER_EVENTS	100

static int timerDebug = 0;
static int timeEventCount;
static int nextEventID;
timerEvent pendingTimerEvents[MAX_TIMER_EVENTS];



/*
 * Global tick counter
 */
volatile ulong_t g_numTicks;

/*
 * Number of times the spin loop can execute during one timer tick
 */
static int s_spinCountPerTick;

/*
 * Number of ticks to wait before calibrating the delay loop.
 */
#define CALIBRATE_NUM_TICKS	3

/*
 * The default quantum; maximum number of ticks a thread can use before
 * we suspend it and choose another.
 */
#define DEFAULT_MAX_TICKS 4

/*
 * Settable quantum.
 */
int g_Quantum = DEFAULT_MAX_TICKS;

/*
 * Ticks per second.
 * FIXME: should set this to something more reasonable, like 100.
 */
#define HZ 100
//#define TICKS_PER_SEC 18

/*#define DEBUG_TIMER */
#ifdef DEBUG_TIMER
#  define Debug(args...) Print(args)
#else
#  define Debug(args...)
#endif

ulong_t clock_time(void){//in millisec
      return g_numTicks * (1000/HZ);
}

/* ----------------------------------------------------------------------
 * Private functions
 * ---------------------------------------------------------------------- */

static void Timer_Interrupt_Handler(struct Interrupt_State* state)
{
  int i;
  struct Kernel_Thread* current = g_currentThread;

  Begin_IRQ(state);

  /* Update global and per-thread number of ticks */
  ++g_numTicks;
  ++current->numTicks;
  

  /* update timer events */
  for (i=0; i < timeEventCount; i++) {
    if (pendingTimerEvents[i].ticks == 0) {
      if (timerDebug) Print("timer: event %d expired (%d ticks)\n", 
			    pendingTimerEvents[i].id, pendingTimerEvents[i].origTicks);
      (pendingTimerEvents[i].callBack)(pendingTimerEvents[i].id, pendingTimerEvents[i].cb_arg);
      pendingTimerEvents[i].ticks = pendingTimerEvents[i].origTicks;
    } else {
      pendingTimerEvents[i].ticks--;
    }
  }

  /*
   * If thread has been running for an entire quantum,
   * inform the interrupt return code that we want
   * to choose a new thread.
   */
  if (current->numTicks >= g_Quantum) {
    g_needReschedule = true;
    /*
     * The current process is moved to a lower priority queue,
     * since it consumed a full quantum.
     */
    //if (current->currentReadyQueue < (MAX_QUEUE_LEVEL - 1)) {
      /*Print("process %d moved to ready queue %d\n", current->pid, current->currentReadyQueue); */
      //current->currentReadyQueue++;
    //}
    
  }
  

  send_tick_to_vmm(1000000/HZ);

  End_IRQ(state);
}

/*
 * Temporary timer interrupt handler used to calibrate
 * the delay loop.
 */
static void Timer_Calibrate(struct Interrupt_State* state)
{
    Begin_IRQ(state);
    if (g_numTicks < CALIBRATE_NUM_TICKS)
	++g_numTicks;
    else {
	/*
	 * Now we can look at EAX, which reflects how many times
	 * the loop has executed
	 */
	/*Print("Timer_Calibrate: eax==%d\n", state->eax);*/
	s_spinCountPerTick = INT_MAX  - state->eax;
	state->eax = 0;  /* make the loop terminate */
    }
    End_IRQ(state);
}

/*
 * Delay loop; spins for given number of iterations.
 */
static void Spin(int count)
{
    /*
     * The assembly code is the logical equivalent of
     *      while (count-- > 0) { // waste some time }
     * We rely on EAX being used as the counter
     * variable.
     */

    int result;
    __asm__ __volatile__ (
	"1: decl %%eax\n\t"
	"cmpl $0, %%eax\n\t"
	"nop; nop; nop; nop; nop; nop\n\t"
	"nop; nop; nop; nop; nop; nop\n\t"
	"jg 1b"
	: "=a" (result)
	: "a" (count)
    );
}

/*
 * Calibrate the delay loop.
 * This will initialize s_spinCountPerTick, which indicates
 * how many iterations of the loop are executed per timer tick.
 */
static void Calibrate_Delay(void)
{
    Disable_Interrupts();

    /* Install temporarily interrupt handler */
    Install_IRQ(TIMER_IRQ, &Timer_Calibrate);
    Enable_IRQ(TIMER_IRQ);

    Enable_Interrupts();

    /* Wait a few ticks */
    while (g_numTicks < CALIBRATE_NUM_TICKS)
	;

    /*
     * Execute the spin loop.xs
     * The temporary interrupt handler will overwrite the
     * loop counter when the next tick occurs.
     */


    Spin(INT_MAX);



    Disable_Interrupts();

    /*
     * Mask out the timer IRQ again,
     * since we will be installing a real timer interrupt handler.
     */
    Disable_IRQ(TIMER_IRQ);
    Enable_Interrupts();
}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

void Init_Timer(void)
{
  ushort_t foo = 1193182L / HZ;
  
  cpu_khz_freq = pit_calibrate_tsc();
  PrintBoth("CPU KHZ=%lu\n", (ulong_t)cpu_khz_freq);

  PrintBoth("Initializing timer and setting to %d Hz...\n",HZ);
  
  /* Calibrate for delay loop */
  Calibrate_Delay();
  PrintBoth("Delay loop: %d iterations per tick\n", s_spinCountPerTick);
  
  // Set Timer to HZ

  Out_Byte(0x43,0x36);          // channel 0, LSB/MSB, mode 3, binary
  Out_Byte(0x40, foo & 0xff);   // LSB
  Out_Byte(0x40, foo >>8);      // MSB
    
  /* Install an interrupt handler for the timer IRQ */

  Install_IRQ(TIMER_IRQ, &Timer_Interrupt_Handler);
  Enable_IRQ(TIMER_IRQ);
}


int Start_Timer_Secs(int seconds, timerCallback cb, void * arg) {
  return Start_Timer(seconds * HZ, cb, arg);
}


int Start_Timer_MSecs(int msecs, timerCallback cb, void * arg) {
  msecs += 10 - (msecs % 10);

  return Start_Timer(msecs * (HZ / 1000), cb, arg);
}



int Start_Timer(int ticks, timerCallback cb, void * arg)
{
    int ret;

    KASSERT(!Interrupts_Enabled());

    PrintBoth ("there\n");

    if (timeEventCount == MAX_TIMER_EVENTS) {
	return -1;
    } else {
	ret = nextEventID++;
	pendingTimerEvents[timeEventCount].id = ret;
	pendingTimerEvents[timeEventCount].callBack = cb;
	pendingTimerEvents[timeEventCount].cb_arg = arg;
	pendingTimerEvents[timeEventCount].ticks = ticks;
	pendingTimerEvents[timeEventCount].origTicks = ticks;
	timeEventCount++;

	return ret;
    }
}


int Get_Remaining_Timer_Ticks(int id)
{
    int i;

    KASSERT(!Interrupts_Enabled());
    for (i=0; i < timeEventCount; i++) {
	if (pendingTimerEvents[i].id == id) {
	    return pendingTimerEvents[i].ticks;
	}
    }

    return -1;
}



double Get_Remaining_Timer_Secs(int id) {
  return (Get_Remaining_Timer_Ticks(id) / HZ);
}


int Get_Remaining_Timer_MSecs(int id) {
  return ((Get_Remaining_Timer_Ticks(id) * 1000) / HZ);
}



int Cancel_Timer(int id)
{
    int i;
    KASSERT(!Interrupts_Enabled());
    for (i=0; i < timeEventCount; i++) {
	if (pendingTimerEvents[i].id == id) {
	    pendingTimerEvents[i] = pendingTimerEvents[timeEventCount-1];
	    timeEventCount--;
	    return 0;
	}
    }

    Print("timer: unable to find timer id %d to cancel it\n", id);
    return -1;
}


#define US_PER_TICK (HZ * 1000000)

/*
 * Spin for at least given number of microseconds.
 * FIXME: I'm sure this implementation leaves a lot to
 * be desired.
 */
void Micro_Delay(int us)
{
    int num = us * s_spinCountPerTick;
    int denom = US_PER_TICK;

    int numSpins = num / denom;
    int rem = num % denom;

    if (rem > 0)
	++numSpins;

    Debug("Micro_Delay(): num=%d, denom=%d, spin count = %d\n", num, denom, numSpins);

    Spin(numSpins);
}



