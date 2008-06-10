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

  /* This is disabled for the time being */
  foo = 0;


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

