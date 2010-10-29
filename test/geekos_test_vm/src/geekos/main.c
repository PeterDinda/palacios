/*
 * GeekOS C code entry point
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2004, Iulian Neamtiu <neamtiu@cs.umd.edu>
 * $Revision: 1.2 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/debug.h>
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
#include <geekos/vm_cons.h>
#include <geekos/pci.h>
#include <geekos/gdt.h>



#define TEST_PAGING 0
#define TEST_PCI 1

/*
  static inline unsigned int cpuid_ecx(unsigned int op)
  {
  unsigned int eax, ecx;
  
  __asm__("cpuid"
  : "=a" (eax), "=c" (ecx)
  : "0" (op)
  : "bx", "dx" );
  
  return ecx;
  }
*/



extern void Get_MSR(ulong_t msr, unsigned int *val1, unsigned int *val2);
extern void Set_MSR(ulong_t msr, ulong_t val1, ulong_t val2);
extern uint_t Get_EIP();
extern uint_t Get_ESP();
extern uint_t Get_EBP();



extern void Invalidate_PG(void * addr);


int foo=42;

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


extern void MyBuzzVM();

#define MYBUZZVM_START MyBuzzVM
#define MYBUZZVM_LEN   0x3d

void BuzzVM()
{
  int x;
  int j;
  unsigned char init;
  
  
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



extern void RunVM();

int vmRunning = 0;

void RunVM() {
  vmRunning = 1;

  while(1);
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



void Hello(ulong_t arg)
{
  char *b="hello ";
  char byte;
  short port=0xe9;
  int i;
  while(1){
    for (i=0;i<6;i++) { 
      byte=b[i];
      __asm__ __volatile__ ("outb %b0, %w1" : : "a"(byte), "Nd"(port) );
    }
  }
}

void Keyboard_Listener(ulong_t arg) {
  ulong_t * doIBuzz = (ulong_t*)arg;
  Keycode key_press;

  Print("Press F4 to turn on/off the speaker\n");

  while ((key_press = Wait_For_Key())) {    
    if (key_press == KEY_F4) {
      PrintBoth("\nToggling Speaker Port\n");
      *doIBuzz = (*doIBuzz + 1) % 2;
    } else if (key_press == KEY_F5) {
      PrintBoth("\nMachine Restart\n");
      machine_real_restart();
    }
  }
  return;
}



extern char BSS_START, BSS_END;

extern char end;




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
  Init_VMCons();
  Init_Screen();
  InitSerial();
  Print("Initializing Memory\n");
  Init_Mem(bootInfo);
  Print("Memory Done\n");
  Init_CRC32();
  Init_TSS();
  Init_Interrupts();
  Init_Scheduler();
  Init_Traps();
  //  Init_Timer();
  Init_Keyboard();
  Init_VM(bootInfo);
  Init_Paging();

  //  Init_IDE();




  PrintBoth("\n\nHello, Welcome to this horrid output-only serial interface\n");
  PrintBoth("Eventually, this will let us control the VMM\n\n");
 
  PrintBoth("\n\n===>");
  


  

  
  PrintBoth("Launching Noisemaker and keyboard listener threads\n");
  
  key_thread = Start_Kernel_Thread(Keyboard_Listener, (ulong_t)&doIBuzz, PRIORITY_NORMAL, false);
  spkr_thread = Start_Kernel_Thread(Buzzer, (ulong_t)&doIBuzz, PRIORITY_NORMAL, false);





  PrintBoth("Next: setup GDT\n");
  {
    uint_t addr = 0xe0000;
    uint_t hi = 0;


   
    //    wrmsr(SYMBIOTIC_MSR, hi, addr);
    {
	uint_t msr_num = 0x0000001B;
	__asm__ __volatile__ ("rdmsr" : : "c"(msr_num) : "%eax","%edx","memory");
    }
    {
	uint_t msr_num = 0x0000001c;
	__asm__ __volatile__ ("rdmsr" : : "c"(msr_num) : "%eax","%edx","memory");
    }


    {
	uint_t msr_num = 0x0000001B;
	__asm__ __volatile__ ("wrmsr" : : "c"(msr_num), "a"(hi), "d"(addr) : "memory");
    }



    {
	uint_t msr_num = 0x535;
	__asm__ __volatile__ ("wrmsr" : : "c"(msr_num), "a"(hi), "d"(addr) : "memory");
    }


    while (1) {}

  }
  if (TEST_PAGING) {
      int i = 0;
      for (i = 0; i < 1024; i++) {
	  uint_t * addr = (uint_t *)0xa00000;
	  uint_t foo = *addr;
	  
	  PrintBoth("Read From 0x%x=%d\n", (uint_t)addr, foo);
      }

      
      //  Invalidate_PG((void *)0x2000);
       
      //  VM_Test(bootInfo, 32);  
      //VM_Test(bootInfo, 1536);
  }


  if (TEST_PCI) {
      Init_PCI();


  }

  while(1);
  
  /* Now this thread is done. */
  Exit(0);
}
