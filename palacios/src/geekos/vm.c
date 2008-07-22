#include <geekos/vmm_stubs.h>

#include <geekos/debug.h>
#include <geekos/serial.h>
#include <geekos/vm.h>
#include <geekos/screen.h>

#include <palacios/vmm.h>
#include <palacios/vmm_io.h>




//test decoder
//#include <palacios/vmm_decoder.h>

extern int parse();

#define SPEAKER_PORT 0x61

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




int IO_Read(ushort_t port, void * dst, uint_t length, void * priv_data) {

  if (length != 1) {
    return 0;
  }

  *(uchar_t*)dst = VM_In_Byte(port);    
  return 1;
}



int IO_Write(ushort_t port, void * src, uint_t length, void * priv_data) {

  if (length != 1) {
    return 0;
  }

  VM_Out_Byte(port, *(uchar_t *)src);    

  return 1;
}


int IO_Read_to_Serial(ushort_t port, void * dst, uint_t length, void * priv_data) {
  PrintBoth("Input from Guest on port %d (0x%x) Length=%d\n", port, port, length);
  
  return 0;
}


char * bochs_debug_buf = NULL;
int bochs_debug_offset = 0;

char * bochs_info_buf = NULL;
int bochs_info_offset = 0;


int IO_BOCHS_debug(ushort_t port, void * src, uint_t length, void * priv_data) {
  if (!bochs_debug_buf) {
    bochs_debug_buf = (char*)Malloc(1024);
  }

  bochs_debug_buf[bochs_debug_offset++] = *(char*)src;

  if ((*(char*)src == 0xa) ||  (bochs_debug_offset == 1023)) {
    SerialPrint("BOCHSDEBUG>%s", bochs_debug_buf);
    memset(bochs_debug_buf, 0, 1024);
    bochs_debug_offset = 0;
  }

  return length;
}

int IO_BOCHS_info(ushort_t port, void * src, uint_t length, void * priv_data) {
  if (!bochs_info_buf) {
    bochs_info_buf = (char*)Malloc(1024);
  }

  bochs_info_buf[bochs_info_offset++] = *(char*)src;

  if ((*(char*)src == 0xa) ||  (bochs_info_offset == 1023)) {
    SerialPrint("BOCHSINFO>%s", bochs_info_buf);
    memset(bochs_info_buf, 0, 1024);
    bochs_info_offset = 0;
  }

  return length;
}


int IO_Write_to_Serial(ushort_t port, void * src, uint_t length, void * priv_data) {
 SerialPrint("Output from Guest on port %d (0x%x) Length=%d\n", port, port, length);
  switch (length) {

  case 1:
    SerialPrint(">0x%.2x\n", *(char*)src);
    break;
  case 2:
    SerialPrint(">0x%.4x\n", *(ushort_t*)src);
    break;
  case 4:
    SerialPrint(">0x%.8x\n", *(uint_t*)src);
    break;
  default:
    break;
  }

  //  SerialMemDump(src, length);
  return length;
}



void BuzzVM()
{
  int x;
  int j;
  unsigned char init;

#if 0  
  __asm__ __volatile__ (
    "popf"
    );
    
#endif
    
  PrintBoth("Starting To Buzz\n");

  init=VM_In_Byte(SPEAKER_PORT);

  while (1) {
    VM_Out_Byte(SPEAKER_PORT, init|0x2);
    for (j=0;j<1000000;j++) { 
      x+=j;
    }
    VM_Out_Byte(SPEAKER_PORT, init);
    for (j=0;j<1000000;j++) { 
      x+=j;
    }
  }
}



int passthrough_mem_read(void * guest_addr, void * dst, uint_t length, void * priv_data) {
  memcpy(dst, (void*)guest_addr, length);
  return length;
}

int passthrough_mem_write(void * guest_addr, void * src, uint_t length, void * priv_data) {
  memcpy((void*)guest_addr, src, length);
  return length;
}



/* We need a configuration mechanism, so we can wrap this completely inside the VMM code, 
 * with no pollution into the HOST OS
 */

int RunVMM(struct Boot_Info * bootInfo) {
  void * config_data;

  struct vmm_os_hooks os_hooks;
  struct vmm_ctrl_ops vmm_ops;
  v3_guest_t* vm_info = 0;
  


  
  memset(&os_hooks, 0, sizeof(struct vmm_os_hooks));
  memset(&vmm_ops, 0, sizeof(struct vmm_ctrl_ops));

  
  os_hooks.print_debug = &SerialPrint;
  os_hooks.print_info = &Print;
  os_hooks.print_trace = &SerialPrint;
  os_hooks.allocate_pages = &Allocate_VMM_Pages;
  os_hooks.free_page = &Free_VMM_Page;
  os_hooks.malloc = &VMM_Malloc;
  os_hooks.free = &VMM_Free;
  os_hooks.vaddr_to_paddr = &Identity;
  os_hooks.paddr_to_vaddr = &Identity;
  os_hooks.hook_interrupt = &geekos_hook_interrupt_new;
  os_hooks.ack_irq = &ack_irq;
  os_hooks.get_cpu_khz = &get_cpu_khz;


  
  Init_V3(&os_hooks, &vmm_ops);
  
  //test decoder
  PrintBoth("testing decoder\n");
  parse();
  PrintBoth("testing decoder done\n");
  

  extern char _binary_vm_kernel_start;
  PrintBoth(" Guest Load Addr: 0x%x\n", &_binary_vm_kernel_start);
  
  config_data = &_binary_vm_kernel_start;

  vm_info = (vmm_ops).allocate_guest();

  PrintBoth("Allocated Guest\n");

  (vmm_ops).config_guest(vm_info, config_data);

  PrintBoth("Configured guest\n");
  
  v3_hook_io_port(vm_info, 0x61, &IO_Read, &IO_Write, NULL);
  //v3_hook_io_port(&vm_info, 0x05, &IO_Read, &IO_Write_to_Serial, NULL);
  
  
  v3_hook_io_port(vm_info, 0x400, &IO_Read, &IO_Write_to_Serial, NULL);
  v3_hook_io_port(vm_info, 0x401, &IO_Read, &IO_Write_to_Serial, NULL);
  v3_hook_io_port(vm_info, 0x402, &IO_Read, &IO_BOCHS_info, NULL);
  v3_hook_io_port(vm_info, 0x403, &IO_Read, &IO_BOCHS_debug, NULL);
  

  (vmm_ops).init_guest(vm_info);
  PrintBoth("Starting Guest\n");
  //Clear_Screen();
  (vmm_ops).start_guest(vm_info);
  
    return 0;
}
