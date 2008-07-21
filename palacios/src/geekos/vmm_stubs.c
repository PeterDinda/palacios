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

//
//
// This is the interrupt state that the VMM's interrupt handlers need to see
//
struct vmm_intr_state {
  uint_t irq;
  uint_t error;

  uint_t should_ack;  // Should the vmm ack this interrupt, or will
                      // the host OS do it?

  // This is the value given when the interrupt is hooked.
  // This will never be NULL
  void *opaque;
};

// This is the function the interface code should call to deliver
// the interrupt to the vmm for handling
extern void deliver_interrupt_to_vmm(struct vmm_intr_state *state);


struct guest_info * irq_map[256];

void *my_opaque[256];


static void translate_intr_handler(struct Interrupt_State *state)
{


  struct vmm_intr_state mystate;

  mystate.irq=state->intNum-32;
  mystate.error=state->errorCode;
  mystate.should_ack=0;
  mystate.opaque=my_opaque[mystate.irq];

  PrintBoth("translate_intr_handler: opaque=0x%x\n",mystate.opaque);

  deliver_interrupt_to_vmm(&mystate);

  End_IRQ(state);

}

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

//
//
// I really don't know what the heck this is doing... PAD
//
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


int geekos_hook_interrupt_new(uint_t irq, void * opaque)
{
  if (my_opaque[irq]) { 
    PrintBoth("Attempt to hook interrupt that is already hooked\n");
    return -1;
  } else {
    PrintBoth("Hooked interrupt 0x%x with opaque 0x%x\n",irq,opaque);
    my_opaque[irq]=opaque;
  }

  Disable_IRQ(irq);
  Install_IRQ(irq,translate_intr_handler);
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

