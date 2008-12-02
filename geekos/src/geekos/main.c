/*
 * GeekOS C code entry point
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2004, Iulian Neamtiu <neamtiu@cs.umd.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * $Revision: 1.47 $
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


#include <geekos/vm.h>
#include <geekos/gdt.h>

#include <geekos/vmm_stubs.h>

#include <geekos/pci.h>


#include <geekos/net.h>


#define SPEAKER_PORT 0x61




void Spin()
{
  // hack - competing thread
  while (1) {};

}


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


  //Out_Byte(0x1234,5);
  //Out_Byte(0x1234,5);

  Init_BSS();
  Init_Screen();

  Init_Serial();

  /*  {
    extern char BSS_START, BSS_END;

    SerialPrint("BSS 0x%x->0x%x\n", &BSS_START, &BSS_END);

    }*/


  // SerialPrint("Guest Mem Dump at 0x%x\n", 0x100000);
  //SerialMemDump((unsigned char *)(0x100000), 261 * 1024);

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
  
  //Init_PCI();



  //  Init_Network();


  //  Init_IDE();

  // Print("Done; stalling\n");


  
#if 0
  SerialPrint("Dumping VM kernel Code (first 128 bytes @ 0x%x)\n", 0x100000);
  SerialMemDump((unsigned char *)0x100000, 256);
  /*
    SerialPrint("Dumping kernel Code (first 512 bytes @ 0x%x)\n",KERNEL_START);
    SerialMemDump((unsigned char *)VM_KERNEL_START, 512);
  */
#endif


#if 1
  struct Kernel_Thread *spin_thread;

  spin_thread=Start_Kernel_Thread(Spin,0,PRIORITY_NORMAL,false);

#endif

#if 0
  {

  struct Kernel_Thread * key_thread;
  struct Kernel_Thread * spkr_thread;

  ulong_t doIBuzz = 0;

  SerialPrint("Dumping BIOS code ffff0-fffff\n\n");
  SerialMemDump((unsigned char *)0x10fff0, 16);
  /*
    SerialPrint("Dumping kernel Code (first 512 bytes @ 0x%x)\n",KERNEL_START);
    SerialMemDump((unsigned char *)VM_KERNEL_START, 512);
  */

  SerialPrint("Noisemaker and keyboard listener threads\n");
  key_thread = Start_Kernel_Thread(Keyboard_Listener, (ulong_t)&doIBuzz, PRIORITY_NORMAL, false);
  spkr_thread = Start_Kernel_Thread(Buzzer, (ulong_t)&doIBuzz, PRIORITY_NORMAL, false);
  }
#endif


  {
    RunVMM(bootInfo);
  }


  SerialPrint("RunVMM returned, spinning\n");
  while (1) {} 


  TODO("Write a Virtual Machine Monitor");
  

  Exit(0);
}
