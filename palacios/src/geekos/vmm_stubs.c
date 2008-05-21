#include <geekos/vmm_stubs.h>
#include <geekos/serial.h>
#include <palacios/vm_guest.h>
#include <geekos/debug.h>



static inline void VM_Out_Byte(ushort_t port, uchar_t value)
{
    __asm__ __volatile__ (
	"outb %b0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a byte from an I/O port.
 */
static inline uchar_t VM_In_Byte(ushort_t port)
{
    uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}




void * Identity(void *addr) { return addr; };

void * Allocate_VMM_Pages(int num_pages) {
  void * start_page = Alloc_Page();
  //SerialPrint("Starting by Allocating Page: %x (%d of %d)\n",start_page, 1, num_pages); 
  int i = 1;

  while (i < num_pages) {
    void * tmp_page = Alloc_Page();
    //SerialPrint("Allocating Page: %x (%d of %d)\n",tmp_page, i+1, num_pages); 
    
    if (tmp_page != start_page + (PAGE_SIZE * i)) {
      //we have to start over...;
      while (i >= 0) {
	Free_Page(start_page + (PAGE_SIZE * i));
	i--;
      }
      start_page = Alloc_Page();
      //SerialPrint("Starting over by Allocating Page: %x (%d of %d)\n",start_page, 1, num_pages);
      i = 1;
      continue;
    }
    i++;
  }

  return start_page;
}

void Free_VMM_Page(void * page) {
  Free_Page(page);
}


void * VMM_Malloc(unsigned int size) {
  return Malloc((unsigned long) size);
}


void VMM_Free(void * addr) {
  Free(addr);
}



struct guest_info * irq_map[256];

static void pic_intr_handler(struct Interrupt_State * state) {
  Begin_IRQ(state);
  struct guest_info * info =   irq_map[state->intNum - 32];
  SerialPrint("Interrupt %d (IRQ=%d)\n", state->intNum, state->intNum - 32);

  if (info) {
    info->vm_ops.raise_irq(info, state->intNum - 32);
  } else {
    SerialPrint("Interrupt handler error: NULL pointer found, no action taken\n");
    End_IRQ(state);
    return;
  }

  // End_IRQ(state);
}


int hook_irq_stub(struct guest_info * info, int irq) {
  if (irq_map[irq]) {
    return -1;
  }

  SerialPrint("Hooking IRQ: %d (vm=0x%x)\n", irq, info);
  irq_map[irq] = info;
  volatile void *foo = pic_intr_handler;
  foo=0;
  Disable_IRQ(irq);
  Install_IRQ(irq, pic_intr_handler);
  Enable_IRQ(irq);
  return 0;
}


int ack_irq(int irq) {
  End_IRQ_num(irq);
  return 0;
}

  
void Init_Stubs() {
  memset(irq_map, 0, sizeof(struct guest_info *) * 256);
}


unsigned int get_cpu_khz() {
  extern uint_t cpu_khz_freq;

  unsigned long  print_khz = (unsigned long)(cpu_khz_freq & 0xffffffff);
  
  PrintBoth("Detected %lu.%lu MHz CPU\n", print_khz / 1000, print_khz % 1000);

  return cpu_khz_freq;
}




#if 0


/* ------ Calibrate the TSC ------- 
 * Return processor ticks per second / CALIBRATE_FRAC.
 *
 * Ported From Xen
 */
#define PIT_MODE 0x43
#define PIT_CH2 0x42
#define CLOCK_TICK_RATE 1193180 /* system crystal frequency (Hz) */
#define CALIBRATE_FRAC  20      /* calibrate over 50ms */
#define CALIBRATE_LATCH ((CLOCK_TICK_RATE+(CALIBRATE_FRAC/2))/CALIBRATE_FRAC)


unsigned long long get_cpu_khz() {
    ullong_t start, end;
    unsigned long count;
    unsigned long long tmp;
    unsigned long print_tmp; 

    /* Set the Gate high, disable speaker */
    VM_Out_Byte((VM_In_Byte(0x61) & ~0x02) | 0x01, 0x61);

    /*
     * Now let's take care of CTC channel 2
     *
     * Set the Gate high, program CTC channel 2 for mode 0, (interrupt on
     * terminal count mode), binary count, load 5 * LATCH count, (LSB and MSB)
     * to begin countdown.
     */
    VM_Out_Byte(0xb0, PIT_MODE);           /* binary, mode 0, LSB/MSB, Ch 2 */
    VM_Out_Byte(CALIBRATE_LATCH & 0xff, PIT_CH2); /* LSB of count */
    VM_Out_Byte(CALIBRATE_LATCH >> 8, PIT_CH2);   /* MSB of count */

    rdtscll(start);
    for ( count = 0; (VM_In_Byte(0x61) & 0x20) == 0; count++ )
        continue;
    rdtscll(end);

    /* Error if the CTC doesn't behave itself. */
    if ( count == 0 ) {
     PrintBoth("CPU Frequency Calibration Error\n");
      return 0;
    }

    tmp = ((end - start) * (ullong_t)CALIBRATE_FRAC);

    do_div(tmp, 1000);

    tmp &= 0xffffffff;
    print_tmp = (unsigned long)tmp;

   PrintBoth("Detected %lu.%lu MHz CPU\n", print_tmp / 1000, print_tmp % 1000);
    return tmp;
}

#undef PIT_CH2
#undef PIT_MODE
#undef CLOCK_TICK_RATE
#undef CALIBRATE_FRAC  
#undef CALIBRATE_LATCH 

#endif
