/*
 * Paging (virtual memory) support
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.2 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/string.h>
#include <geekos/int.h>
#include <geekos/idt.h>
#include <geekos/kthread.h>
#include <geekos/kassert.h>
#include <geekos/screen.h>
#include <geekos/mem.h>
#include <geekos/malloc.h>
#include <geekos/gdt.h>
#include <geekos/segment.h>
//#include <geekos/user.h>
//#include <geekos/vfs.h>
#include <geekos/crc32.h>
#include <geekos/paging.h>
#include <geekos/debug.h>


/* ----------------------------------------------------------------------
 * Public data
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * Private functions/data
 * ---------------------------------------------------------------------- */

#define SECTORS_PER_PAGE (PAGE_SIZE / SECTOR_SIZE)

/*
 * flag to indicate if debugging paging code
 */
int debugFaults = 0;
#define Debug(args...) if (debugFaults) PrintBoth(args)



void PrintPD(pde_t *pde)
{
  uint_t i;

  PrintBoth("Page Directory at %p:\n",pde);
  for (i = 0; i < NUM_PAGE_DIR_ENTRIES; i++) { 
    if (pde[i].present) {
      if ((i * PAGE_SIZE * 1024) > 0x40000000) {
	PrintPDE((void*)(PAGE_SIZE*NUM_PAGE_TABLE_ENTRIES*i),&(pde[i]));
      }
    }
  }
}

void PrintPT(void *starting_address, pte_t *pte) 
{
  int i;

  PrintBoth("Page Table at %p:\n",pte);
  for (i=0;i<NUM_PAGE_TABLE_ENTRIES;i++) { 
    if (pte[i].present) {
      PrintPTE(starting_address + PAGE_SIZE*i,&(pte[i]));
    }
  }
}


void PrintPDE(void *virtual_address, pde_t *pde)
{
  Print("PDE %p -> %p : present=%x, flags=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	      virtual_address,
	      (void*) (pde->pageTableBaseAddr << PAGE_POWER),
	      pde->present,
	      pde->flags,
	      pde->accessed,
	      pde->reserved,
	      pde->largePages,
	      pde->globalPage,
	      pde->kernelInfo);
}
  
void PrintPTE(void *virtual_address, pte_t *pte)
{
  PrintBoth("PTE %p -> %p : present=%x, flags=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, kernelInfo=%x\n",
	      virtual_address,
	      (void*)(pte->pageBaseAddr << PAGE_POWER),
	      pte->present,
	      pte->flags,
	      pte->accessed,
	      pte->dirty,
	      pte->pteAttribute,
	      pte->globalPage,
	      pte->kernelInfo);
}


void DumpPageTables(pde_t *pde)
{
  uint_t i;
  
  PrintBoth("Dumping the pages starting with the pde page at %p\n",pde);

  for (i = 0; i < NUM_PAGE_DIR_ENTRIES; i++) { 
    if (pde[i].present) {
      if ((i * PAGE_SIZE * 1024) >= 0x40000000) {
	PrintPDE((void *)(PAGE_SIZE * NUM_PAGE_TABLE_ENTRIES * i), &(pde[i]));
	PrintPT((void *)(PAGE_SIZE * NUM_PAGE_TABLE_ENTRIES * i), (void *)(pde[i].pageTableBaseAddr << PAGE_POWER));
      }
    }
  }
}
    
    
    


int checkPaging()
{
  unsigned long reg=0;
  __asm__ __volatile__( "movl %%cr0, %0" : "=a" (reg));
  Print("Paging on ? : %d\n", (reg & (1<<31)) != 0);
  return (reg & (1<<31)) != 0;
}


/*
 * Print diagnostic information for a page fault.
 */
static void Print_Fault_Info(uint_t address, faultcode_t faultCode)
{
    extern uint_t g_freePageCount;

    g_freePageCount+=0;

    PrintBoth("Pid %d, Page Fault received, at address %x (%d pages free)\n",
        g_currentThread->pid, address, g_freePageCount);
    if (faultCode.protectionViolation)
	PrintBoth("   Protection Violation, ");
    else
        PrintBoth("   Non-present page, ");
    if (faultCode.writeFault)
	PrintBoth("Write Fault, ");
    else
        PrintBoth("Read Fault, ");
    if (faultCode.userModeFault)
        PrintBoth("in User Mode\n");
    else
        PrintBoth("in Supervisor Mode\n");
}

/*
 * Handler for page faults.
 * You should call the Install_Interrupt_Handler() function to
 * register this function as the handler for interrupt 14.
 */
/*static*/ void Page_Fault_Handler(struct Interrupt_State* state)
{
    ulong_t address;
    faultcode_t faultCode;

    KASSERT(!Interrupts_Enabled());

    /* Get the address that caused the page fault */
    address = Get_Page_Fault_Address();
    PrintBoth("Page fault @%lx\n", address);

    /* Get the fault code */
    faultCode = *((faultcode_t *) &(state->errorCode));

    /* rest of your handling code here */
    PrintBoth("Unexpected Page Fault received\n");
    Print_Fault_Info(address, faultCode);
    Dump_Interrupt_State(state);
    /* user faults just kill the process */
    if (!faultCode.userModeFault) KASSERT(0);

    /* For now, just kill the thread/process. */
    Exit(-1);
}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */



/*
 * Initialize virtual memory by building page tables
 * for the kernel and physical memory.
 */
void Init_VM(struct Boot_Info *bootInfo)
{
  int numpages;
  int numpagetables;
  int i,j;

  pde_t *pd;
  pte_t *pt;

  PrintBoth("Intitialing Virtual Memory\n");

  if (checkPaging()) { 
      PrintBoth("Paging is currently ON\n");
    return ;
  }

  PrintBoth("Paging is currently OFF - initializing the pages for a 1-1 map\n");
  
  numpages=bootInfo->memSizeKB / (PAGE_SIZE/1024);
  numpagetables = numpages / NUM_PAGE_TABLE_ENTRIES + ((numpages % NUM_PAGE_TABLE_ENTRIES) != 0 );

  PrintBoth("We need %d pages, and thus %d page tables, and one page directory\n",numpages, numpagetables);
  
  pd = (pde_t*)Alloc_Page();
  
  if (!pd) { 
      PrintBoth("We are giving up since we can't allocate a page directory!\n");
    return;
  } else {
      PrintBoth("Our PDE is at physical address %p\n",pd);
  }
  
  for (i=0;i<NUM_PAGE_DIR_ENTRIES;i++) { 
    if (i>=numpagetables) { 
      pd[i].present=0;
      pd[i].flags=0;
      pd[i].accessed=0;
      pd[i].reserved=0;
      pd[i].largePages=0;
      pd[i].globalPage=0;
      pd[i].kernelInfo=0;
      pd[i].pageTableBaseAddr=0;
    } else {
      pt = (pte_t*)Alloc_Page();
      if (!pt) { 
	  PrintBoth("We are giving up since we can't allocate page table %d\n",i);
      } else {
	//PrintBoth("Page Table %d is at physical address %p\n",i,pt);
      }
      pd[i].present=1;
      pd[i].flags= VM_READ | VM_WRITE | VM_EXEC | VM_USER;
      pd[i].accessed=0;
      pd[i].reserved=0;
      pd[i].largePages=0;
      pd[i].globalPage=0;
      pd[i].kernelInfo=0;
      pd[i].pageTableBaseAddr = PAGE_ALLIGNED_ADDR(pt);
      
      for (j=0;j<NUM_PAGE_TABLE_ENTRIES;j++) { 
	if (i*NUM_PAGE_TABLE_ENTRIES + j >= numpages) {
	  pt[j].present=0;
	  pt[j].flags=0;
	  pt[j].accessed=0;
	  pt[j].dirty=0;
	  pt[j].pteAttribute=0;
	  pt[j].globalPage=0;
	  pt[j].kernelInfo=0;
	  pt[j].pageBaseAddr=0;
	} else {
	  pt[j].present=1;
	  pt[j].flags=VM_READ | VM_WRITE | VM_EXEC | VM_USER;
	  pt[j].accessed=0;
	  pt[j].dirty=0;
	  pt[j].pteAttribute=0;
	  pt[j].globalPage=0;
	  pt[j].kernelInfo=0;
	  pt[j].pageBaseAddr=(i*NUM_PAGE_TABLE_ENTRIES + j);
	}
      }
    }
  }


  PrintBoth("Done creating 1<->1 initial page tables\n");
  PrintBoth("Now installing page fault handler\n");
  //  SerialDumpPageTables(pd);
  Install_Interrupt_Handler(14,Page_Fault_Handler);
  PrintBoth("Now turning on the paging bit!\n");
  Enable_Paging(pd);
  PrintBoth("We are still alive after paging turned on!\n");
  PrintBoth("checkPaging returns %d\n",checkPaging());
}



/* How we test...
 * 1 <-> 1 mapping of physical memory, 
 * We assume that physical memory << 1G
 * we setup the test so that 1G+ is mapped in a testable order
 * then go through and ensure that all the paging operations work...
 * The static mapping sits at 1G
 * The tests are run at 2G

 */

void VM_Test(struct Boot_Info *bootInfo, uint_t num_test_pages) {
  int i,j;

  pde_t * pd;
  //pde_t *pde;
  //  pte_t *pte;

  PrintBoth("Running Paging Test\n");

  pd = Get_PDBR();


  void * one_gig = (void *)0x40000000;
  void * two_gig = (void *)0x80000000;

  /* Set up the 1 GIG static map */
  ulong_t pde_offset = (((ulong_t)one_gig / PAGE_SIZE) / 1024);    
  for (i = 0; i < num_test_pages; i += 1024) {
    pde_t * pde = &(pd[pde_offset + (i / 1024)]);
    pte_t * pte = (pte_t *)Alloc_Page();
    memset(pte, 0, PAGE_SIZE);

    pde->present = 1;
    pde->flags= VM_READ | VM_WRITE | VM_EXEC | VM_USER;
    pde->pageTableBaseAddr = PAGE_ALLIGNED_ADDR(pte);

    for (j = 0; (j + i) < num_test_pages ; j++) {
      pte[j].present = 1;
      pte[j].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
      pte[j].pageBaseAddr = PAGE_ALLIGNED_ADDR(Alloc_Page());
    }
  }

  PrintBoth("Setup VM Test static map\n");


  /* Setup the Two Gig test area */
  /* First is just a incrmental mirroring of the 1G area */

  pde_offset = (((ulong_t)two_gig / PAGE_SIZE) / 1024);  
  
  for (i = 0; i < num_test_pages; i += 1024) {
    pde_t * pde = &(pd[pde_offset + (i / 1024)]);
    pte_t * pte = (pte_t *)Alloc_Page();
    memset(pte, 0, PAGE_SIZE);

    pde->present = 1;
    pde->flags= VM_READ | VM_WRITE | VM_EXEC | VM_USER;
    pde->pageTableBaseAddr = PAGE_ALLIGNED_ADDR(pte);

    for (j = 0; (j + i) < num_test_pages; j++) {
      pte_t * static_pte = LookupPage(one_gig + (PAGE_SIZE * (j+i)));
      pte[j].present = 1;
      pte[j].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
      pte[j].pageBaseAddr = static_pte->pageBaseAddr;
    }
  }

  PrintBoth("Setup initial test area\n");

  PrintBoth("Loading CR3\n");
  Set_PDBR(pd);

  DumpPageTables(pd);

  PrintBoth("Writing to Test Area\n");

  /* Write data to each page in the 2G test area */
  uint_t * test_ptr = (uint_t *)two_gig;
  for (i = 0; i < num_test_pages; i++) {

    PrintBoth("Writing %d to %p\n", i, test_ptr);
    *test_ptr = (uint_t)i;
    test_ptr += PAGE_SIZE / 4;
  }


  PrintBoth("Reversing Page Mapping\n");
  
  /* Reverse 2G test area mapping */
  
  pde_offset = (((ulong_t)two_gig / PAGE_SIZE) / 1024);

  for (i = 0; i < num_test_pages; i += 1024) {
    pde_t * pde = &(pd[pde_offset + (i / 1024)]);
    pte_t * pte = (pte_t *)(pde->pageTableBaseAddr << 12);

    for (j = 0; (j + i) < num_test_pages ; j++) {
      pte_t * static_pte = LookupPage(one_gig + (PAGE_SIZE * (num_test_pages - (j+i) - 1)));
      pte[j].pageBaseAddr = static_pte->pageBaseAddr;
    } 
  }


  Set_PDBR(pd);
  
  PrintBoth("Page Mapping Reversed\n");
  DumpPageTables(pd);


  PrintBoth("Page Consistency Check\n");

  test_ptr = (uint_t *)two_gig;
  for (i = 0; i < num_test_pages; i++) {
    if ((*test_ptr) != num_test_pages - (i+1)) {
      PrintBoth("Consistency Error: (Test Value=%d; Actual Value=%d)\n", (num_test_pages - i), (*test_ptr));
      while(1);
    }
    test_ptr += PAGE_SIZE / 4;
  }

  PrintBoth("Test Sucessful\n");


  PrintBoth("Invalidation Test\n");


  Invalidate_PG(two_gig);

  uint_t foo = 0;

  foo = *(uint_t *)two_gig;
  *(uint_t *)((char *)two_gig + 4) = foo;


  PrintBoth("Invalidation Test Successful\n");
}


//
pte_t *LookupPage(void *vaddr)
{
  uint_t pde_offset = ((uint_t)vaddr) >> 22;
  uint_t pte_offset = (((uint_t)vaddr) >> 12) & 0x3ff;
  pte_t *pte; 
  pde_t *pde = Get_PDBR();

  KASSERT(pde);

  pde+=pde_offset;

  if (!(pde->present)) { 
    return 0;
  }

  pte = (pte_t *)((pde->pageTableBaseAddr)<<12);

  pte+=pte_offset;

  return pte;
}


pte_t *CreateAndAddPageTable(void *vaddr, uint_t flags)
{
  int i;

  KASSERT(!(PAGE_OFFSET(vaddr)));
  
  pte_t *pt = Alloc_Page();
  
  KASSERT(pt);
  
  for (i=0;i<NUM_PAGE_TABLE_ENTRIES;i++) { 
    pt[i].present=0;
  }

  pde_t *pde = Get_PDBR();
  
  pde=&(pde[PAGE_DIRECTORY_INDEX(vaddr)]);

  KASSERT(!(pde->present));
  
  pde->present=1;
  pde->flags=flags;
  pde->accessed=0;
  pde->reserved=0;
  pde->largePages=0;
  pde->globalPage=0;
  pde->kernelInfo=0;
  pde->pageTableBaseAddr = PAGE_ALLIGNED_ADDR(pt);

  return pt;
}

pte_t *MapPage(void *vaddr, pte_t *pte, int alloc_pde)
{
  pte_t *oldpte = LookupPage(vaddr);

  if (!pte) {
    if (alloc_pde) { 
      CreateAndAddPageTable(vaddr,pte->flags);
      oldpte = LookupPage(vaddr);
      KASSERT(pte);
    } else {
      return 0;
    }
  }

  *oldpte = *pte;
  
  return oldpte;
}

pte_t *UnMapPage(void *vaddr)
{
  pte_t *oldpte = LookupPage(vaddr);

  if (!oldpte) {
    return 0;
  }
  oldpte->present=0;
  
  return oldpte;
}

  

/**
 * Initialize paging file data structures.
 * All filesystems should be mounted before this function
 * is called, to ensure that the paging file is available.
 */
void Init_Paging(void)
{
  PrintBoth("Initializing Paging\n");
}

/**
 * Find a free bit of disk on the paging file for this page.
 * Interrupts must be disabled.
 * @return index of free page sized chunk of disk space in
 *   the paging file, or -1 if the paging file is full
 */
int Find_Space_On_Paging_File(void)
{
    KASSERT(!Interrupts_Enabled());
    TODO("Find free page in paging file");
}

/**
 * Free a page-sized chunk of disk space in the paging file.
 * Interrupts must be disabled.
 * @param pagefileIndex index of the chunk of disk space
 */
void Free_Space_On_Paging_File(int pagefileIndex)
{
    KASSERT(!Interrupts_Enabled());
    TODO("Free page in paging file");
}

/**
 * Write the contents of given page to the indicated block
 * of space in the paging file.
 * @param paddr a pointer to the physical memory of the page
 * @param vaddr virtual address where page is mapped in user memory
 * @param pagefileIndex the index of the page sized chunk of space
 *   in the paging file
 */
void Write_To_Paging_File(void *paddr, ulong_t vaddr, int pagefileIndex)
{
    struct Page *page = Get_Page((ulong_t) paddr);
    KASSERT(!(page->flags & PAGE_PAGEABLE)); /* Page must be locked! */
    TODO("Write page data to paging file");
}

/**
 * Read the contents of the indicated block
 * of space in the paging file into the given page.
 * @param paddr a pointer to the physical memory of the page
 * @param vaddr virtual address where page will be re-mapped in
 *   user memory
 * @param pagefileIndex the index of the page sized chunk of space
 *   in the paging file
 */
void Read_From_Paging_File(void *paddr, ulong_t vaddr, int pagefileIndex)
{
    struct Page *page = Get_Page((ulong_t) paddr);
    KASSERT(!(page->flags & PAGE_PAGEABLE)); /* Page must be locked! */
    TODO("Read page data from paging file");
}

