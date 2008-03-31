/*
 * GeekOS C code entry point
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2004, Iulian Neamtiu <neamtiu@cs.umd.edu>
 * $Revision: 1.24 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/bootinfo.h>
#include <geekos/string.h>
#include <geekos/screen.h>
#include <geekos/mem.h>
#include <geekos/crc32.h>
#include <geekos/tss.h>
#include <geekos/int.h>
#include <geekos/kthread.h>
#include <geekos/trap.h>
#include <geekos/timer.h>
#include <geekos/keyboard.h>
#include <geekos/io.h>
#include <geekos/serial.h>
#include <geekos/reboot.h>
#include <geekos/mem.h>
#include <geekos/paging.h>
#include <geekos/ide.h>
#include <geekos/malloc.h>

#include <geekos/debug.h>
#include <geekos/vmm.h>


#include <geekos/gdt.h>


#include <geekos/vmm_stubs.h>


#define SPEAKER_PORT 0x61


void Buzz(unsigned delay, unsigned num)
{
  volatile int x;
  int i,j;
  unsigned char init;
  
  init=In_Byte(SPEAKER_PORT);

  for (i=0;i<num;i++) { 
    Out_Byte(SPEAKER_PORT, init|0x2);
    for (j=0;j<delay;j++) { 
      x+=j;
    }
    Out_Byte(SPEAKER_PORT, init);
    for (j=0;j<delay;j++) { 
      x+=j;
    }
  }
}

inline void MyOut_Byte(ushort_t port, uchar_t value)
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
inline uchar_t MyIn_Byte(ushort_t port)
{
    uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}



int IO_Read(ushort_t port, void * dst, uint_t length) {
  uchar_t * iter = dst;
  uint_t i;

  for (i = 0; i < length; i++) {
    *iter = MyIn_Byte(port);    
    iter++;
  }
  
  return 0;
}



int IO_Write(ushort_t port, void * src, uint_t length) {
  uchar_t * iter = src;
  uint_t i;


  for (i = 0; i < length; i++) {
    MyOut_Byte(port, *iter);    
    iter++;
  }

  return 0;
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

  init=MyIn_Byte(SPEAKER_PORT);

  while (1) {
    MyOut_Byte(SPEAKER_PORT, init|0x2);
    for (j=0;j<1000000;j++) { 
      x+=j;
    }
    MyOut_Byte(SPEAKER_PORT, init);
    for (j=0;j<1000000;j++) { 
      x+=j;
    }
  }
}








void Buzzer(ulong_t arg) {
  ulong_t *doIBuzz = (ulong_t*)arg;
  while (1) {
    // Quick and dirty hack to save my hearing...
    // I'm not too worried about timing, so I'll deal with concurrency later...
    if (*doIBuzz == 1) {
      Buzz(1000000, 10);
    }
  }

}





void Keyboard_Listener(ulong_t arg) {
  ulong_t * doIBuzz = (ulong_t*)arg;
  Keycode key_press;

  Print("Press F4 to turn on/off the speaker\n");

  while ((key_press = Wait_For_Key())) {    
    if (key_press == KEY_F4) {
      Print("\nToggling Speaker Port\n");
      SerialPrintLevel(100,"\nToggling Speaker Port\n");
      *doIBuzz = (*doIBuzz + 1) % 2;
    } else if (key_press == KEY_F5) {
      Print("\nMachine Restart\n");
      SerialPrintLevel(100,"\nMachine Restart\n");
      machine_real_restart();
    }
  }
  return;
}



extern char BSS_START, BSS_END;

extern char end;


/* This is an ugly hack to get at the VM  memory */
ulong_t vm_range_start;
ulong_t vm_range_end;
ulong_t guest_kernel_start;
ulong_t guest_kernel_end;
/* ** */


int AllocateAndMapPagesForRange(uint_t start, uint_t length, pte_t template_pte)
{
  uint_t address;

  for (address=start;address<start+length;address+=PAGE_SIZE) { 
    void *page;
    pte_t pte = template_pte;
    
    page=Alloc_Page();
    KASSERT(page);
    
    pte.pageBaseAddr=PAGE_ALLIGNED_ADDR(page);

    KASSERT(MapPage((void*)address,&pte,1));
  }
  
  return 0;
}
    


/*
 * Kernel C code entry point.
 * Initializes kernel subsystems, mounts filesystems,
 * and spawns init process.
 */
void Main(struct Boot_Info* bootInfo)
{
  struct Kernel_Thread * key_thread;
  struct Kernel_Thread * spkr_thread;

  ulong_t doIBuzz = 0;

  Init_BSS();
  Init_Screen();


  Init_Serial();
  Init_Mem(bootInfo);
  Init_CRC32();
  Init_TSS();
  Init_Interrupts();
  Init_Scheduler();
  Init_Traps();
  Init_Timer();
  Init_Keyboard();
  Init_VM(bootInfo);
  Init_Paging();

  //  Init_IDE();

  // Print("Done; stalling\n");


  
#if 0
  SerialPrint("Dumping VM kernel Code (first 128 bytes @ 0x%x)\n", 0x100000);
  SerialMemDump((unsigned char *)0xfe000, 4096);
  /*
    SerialPrint("Dumping kernel Code (first 512 bytes @ 0x%x)\n",KERNEL_START);
    SerialMemDump((unsigned char *)VM_KERNEL_START, 512);
  */
#endif

#if 0
  SerialPrint("Dumping BIOS code f0000-fffff\n\n");
  SerialMemDump((unsigned char *)0xf0000, 65536);
  /*
    SerialPrint("Dumping kernel Code (first 512 bytes @ 0x%x)\n",KERNEL_START);
    SerialMemDump((unsigned char *)VM_KERNEL_START, 512);
  */
#endif

#if 1
  SerialPrintLevel(1000,"Launching Noisemaker and keyboard listener threads\n");
  key_thread = Start_Kernel_Thread(Keyboard_Listener, (ulong_t)&doIBuzz, PRIORITY_NORMAL, false);
  spkr_thread = Start_Kernel_Thread(Buzzer, (ulong_t)&doIBuzz, PRIORITY_NORMAL, false);

#endif

  {
    struct vmm_os_hooks os_hooks;
    struct vmm_ctrl_ops vmm_ops;
    guest_info_t vm_info;
    addr_t rsp;
    addr_t rip;

    memset(&os_hooks, 0, sizeof(struct vmm_os_hooks));
    memset(&vmm_ops, 0, sizeof(struct vmm_ctrl_ops));
    memset(&vm_info, 0, sizeof(guest_info_t));

    os_hooks.print_debug = &PrintBoth;
    os_hooks.print_info = &Print;
    os_hooks.print_trace = &SerialPrint;
    os_hooks.allocate_pages = &Allocate_VMM_Pages;
    os_hooks.free_page = &Free_VMM_Page;
    os_hooks.malloc = &VMM_Malloc;
    os_hooks.free = &VMM_Free;
    os_hooks.vaddr_to_paddr = &Identity;
    os_hooks.paddr_to_vaddr = &Identity;


    //   DumpGDT();
    Init_VMM(&os_hooks, &vmm_ops);
  
    init_shadow_map(&(vm_info.mem_map));
    init_shadow_page_state(&(vm_info.shadow_page_state));
    vm_info.page_mode = SHADOW_PAGING;

    vm_info.cpu_mode = REAL;

    init_vmm_io_map(&(vm_info.io_map));

    
    if (0) {
      
      //    add_shared_mem_range(&(vm_info.mem_layout), 0, 0x800000, 0x10000);    
      //    add_shared_mem_range(&(vm_info.mem_layout), 0, 0x1000000, 0);
      
      rip = (ulong_t)(void*)&BuzzVM;
      //  rip -= 0x10000;
      //    rip = (addr_t)(void*)&exit_test;
      //  rip -= 0x2000;
      vm_info.rip = rip;
      rsp = (addr_t)Alloc_Page();
      
      vm_info.rsp = (rsp +4092 );// - 0x2000;
      
            
    } else {
      //add_shared_mem_range(&(vm_info.mem_layout), 0x0, 0x1000, 0x100000);
      //      add_shared_mem_range(&(vm_info.mem_layout), 0x0, 0x100000, 0x0);
      
      shadow_region_t *ent = Malloc(sizeof(shadow_region_t));;
      init_shadow_region_physical(ent,0,0x100000,GUEST_REGION_PHYSICAL_MEMORY,
				  0x100000, HOST_REGION_PHYSICAL_MEMORY);
      add_shadow_region(&(vm_info.mem_map),ent);

      hook_io_port(&(vm_info.io_map), 0x61, &IO_Read, &IO_Write);
      /*
      vm_info.cr0 = 0;
      vm_info.cs.base=0xf000;
      vm_info.cs.limit=0xffff;
      */
      vm_info.rip = 0xfff0;
      vm_info.rsp = 0x0;
    }

    PrintBoth("Initializing Guest (eip=0x%.8x) (esp=0x%.8x)\n", (uint_t)vm_info.rip,(uint_t)vm_info.rsp);
    (vmm_ops).init_guest(&vm_info);
    PrintBoth("Starting Guest\n");
    (vmm_ops).start_guest(&vm_info);
  }


  


  TODO("Write a Virtual Machine Monitor");
  

  Exit(0);
}
