/*
 * Physical memory allocation
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * $Revision: 1.1 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/defs.h>
#include <geekos/ktypes.h>
#include <geekos/kassert.h>
#include <geekos/bootinfo.h>
#include <geekos/gdt.h>
#include <geekos/screen.h>
#include <geekos/int.h>
#include <geekos/malloc.h>
#include <geekos/string.h>
#include <geekos/mem.h>


#include <geekos/serial.h>

/* ----------------------------------------------------------------------
 * Global data
 * ---------------------------------------------------------------------- */

/*
 * List of Page structures representing each page of physical memory.
 */
struct Page* g_pageList;

/*
 * Number of pages currently available on the freelist.
 */
uint_t g_freePageCount = 0;

/* ----------------------------------------------------------------------
 * Private data and functions
 * ---------------------------------------------------------------------- */

/*
 * Defined in paging.c
 */
extern int debugFaults;
#define Debug(args...) if (debugFaults) Print(args)

/*
 * List of pages available for allocation.
 */
static struct Page_List s_freeList;

/*
 * Total number of physical pages.
 */
int unsigned s_numPages;

/*
 * Add a range of pages to the inventory of physical memory.
 */
static void Add_Page_Range(ulong_t start, ulong_t end, int flags)
{
    ulong_t addr;

    PrintBoth("Start: %u, End: %u\n", (unsigned int)start, (unsigned int)end);

    KASSERT(Is_Page_Multiple(start));
    KASSERT(Is_Page_Multiple(end));
    KASSERT(start < end);

    //Print("Adding %lu pages\n", (end - start) / PAGE_SIZE);

    for (addr = start; addr < end; addr += PAGE_SIZE) {
      //      Print("Adding Page at %u\n", (unsigned int)addr);
	struct Page *page = Get_Page(addr);

	page->flags = flags;

	if (flags == PAGE_AVAIL) {
	    /* Add the page to the freelist */
	    Add_To_Back_Of_Page_List(&s_freeList, page);

	    /* Update free page count */
	    ++g_freePageCount;
	} else {
	    Set_Next_In_Page_List(page, 0);
	    Set_Prev_In_Page_List(page, 0);
	}

    }
    //   Print("%d pages now in freelist\n", g_freePageCount);

}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

/*
 * The linker defines this symbol to indicate the end of
 * the executable image.
 */
extern char end;

/*
 * Initialize memory management data structures.
 * Enables the use of Alloc_Page() and Free_Page() functions.
 */
void Init_Mem(struct Boot_Info* bootInfo)
{
    ulong_t numPages = bootInfo->memSizeKB >> 2;
    ulong_t endOfMem = numPages * PAGE_SIZE;
    unsigned numPageListBytes = sizeof(struct Page) * numPages;
    ulong_t pageListAddr;
    ulong_t pageListEnd;
    ulong_t kernEnd;


    KASSERT(bootInfo->memSizeKB > 0);

    if (bootInfo->memSizeKB != TOP_OF_MEM/1024) { 
      PrintBoth("Kernel compiled for %d KB machine, but machine claims %d KB\n",TOP_OF_MEM/1024,bootInfo->memSizeKB);
      if (bootInfo->memSizeKB < TOP_OF_MEM/1024) { 
	PrintBoth("Kernel compiled for more memory than machine has.  Panicking\n");
	KASSERT(0);
      }
    }


    bootInfo->memSizeKB = TOP_OF_MEM / 1024;

    /*
     * Before we do anything, switch from setup.asm's temporary GDT
     * to the kernel's permanent GDT.
     */
    Init_GDT();

    /*
     * We'll put the list of Page objects right after the end
     * of the kernel, and mark it as "kernel".  This will bootstrap
     * us sufficiently that we can start allocating pages and
     * keeping track of them.
     */

    // JRL: This is stupid... 
    // with large mem sizes the page list overruns into the ISA 
    // hole. By blind luck this causes an unrelated assertion failure, otherwise
    // I might never have caught it...
    // We fix it by moving the page list after the kernel heap...
    // For now we'll make our own stupid assumption that the mem size
    // is large enough to accomodate the list in high mem.

    PrintBoth("Total Memory Size: %u MBytes\n", bootInfo->memSizeKB/1024);

    

    PrintBoth("Page struct size: %u bytes\n", sizeof(struct Page));
    PrintBoth("Page List Size: %u bytes\n", numPageListBytes);

  
    //pageListAddr = Round_Up_To_Page((ulong_t) &end);
    //pageListAddr = Round_Up_To_Page(HIGHMEM_START + KERNEL_HEAP_SIZE);

    // Note that this is now moved to be just above the kernel heap
    // see defs.h for layout
    pageListAddr=Round_Up_To_Page(KERNEL_PAGELIST);
      
    pageListEnd = Round_Up_To_Page(pageListAddr + numPageListBytes);

    g_pageList = (struct Page*) pageListAddr;
    //    kernEnd = Round_Up_To_Page(pageListAddr + numPageListBytes);
    //
    // PAD - Note: I am changing this so that everything through the end of 
    // the VM boot package (bioses/vmxassist) is off limits
    //kernEnd = Round_Up_To_Page((ulong_t) &end);
    kernEnd = Round_Up_To_Page(end);
    s_numPages = numPages;

    PrintBoth("Pagelist addr: %p\n", g_pageList);
    PrintBoth("index: %p\n", &g_pageList[3]);
    PrintBoth("direct offset: %p\n", g_pageList + (sizeof(struct Page) * 2));
    //  PrintBoth("Kernel Size=%lx\n", (kernEnd - KERNEL_START_ADDR));
    // PrintBoth("Kernel Start=%x\n", KERNEL_START_ADDR);
    PrintBoth("Kernel End=%lx\n", kernEnd);
    //PrintBoth("end=%x\n", end);


    /*
     * The initial kernel thread and its stack are placed
     * just beyond the ISA hole.
     */
    // This is no longer true
    // KASSERT(ISA_HOLE_END == KERN_THREAD_OBJ);
    // instead, 
    //KASSERT(KERN_THREAD_OBJ==(START_OF_VM+VM_SIZE));
    //KASSERT(KERN_STACK == KERN_THREAD_OBJ + PAGE_SIZE);

    /*
     * Memory looks like this:
     * 0 - start: available (might want to preserve BIOS data area)
     * start - end: kernel
     * end - ISA_HOLE_START: available
     * ISA_HOLE_START - ISA_HOLE_END: used by hardware (and ROM BIOS?)
     * ISA_HOLE_END - HIGHMEM_START: used by initial kernel thread
     * HIGHMEM_START - end of memory: available
     *    (the kernel heap is located at HIGHMEM_START; any unused memory
     *    beyond that is added to the freelist)
     */

  


    // The kernel is still in low memory at this point, in the VM region
    // Thus we will mark it as kernel use
    // Add_Page_Range(KERNEL_START_ADDR, kernEnd, PAGE_KERN);
    
    
    //Add_Page_Range(kernEnd, ISA_HOLE_START, PAGE_AVAIL);
    // ISA hole remains closed (no actual memory)
    // Add_Page_Range(ISA_HOLE_START, ISA_HOLE_END, PAGE_HW);
    
    //Add_Page_Range(ISA_HOLE_END, HIGHMEM_START, PAGE_ALLOCATED);
    // Add_Page_Range(HIGHMEM_START, HIGHMEM_START + KERNEL_HEAP_SIZE, PAGE_HEAP);
    //Add_Page_Range(HIGHMEM_START + KERNEL_HEAP_SIZE, endOfMem, PAGE_AVAIL);
    /* JRL: move page list after kernel heap */
    
    //Now, above the VM region...

    // Kernel thread object
    Add_Page_Range(KERNEL_THREAD_OBJECT,KERNEL_THREAD_OBJECT+KERNEL_THREAD_OBJECT_SIZE,PAGE_ALLOCATED);
    // Kernel stack
    Add_Page_Range(KERNEL_STACK,KERNEL_STACK+KERNEL_STACK_SIZE,PAGE_ALLOCATED);
    // Kernel heap
    Add_Page_Range(KERNEL_HEAP,KERNEL_HEAP+KERNEL_HEAP_SIZE,PAGE_HEAP);
    // Kernel page list
    Add_Page_Range(pageListAddr, pageListEnd, PAGE_KERN);
    // Free space
    Add_Page_Range(pageListEnd,Round_Down_To_Page(FINAL_KERNEL_START), PAGE_AVAIL);
    // The kernel 
    Add_Page_Range(Round_Down_To_Page(FINAL_KERNEL_START),Round_Up_To_Page(FINAL_VMBOOTEND+1),PAGE_KERN);
    // The vmbootpackage 
    // IDT (this should be one page)
    Add_Page_Range(IDT_LOCATION,TSS_LOCATION,PAGE_KERN);
    // TSS (this should be one page)
    Add_Page_Range(TSS_LOCATION,GDT_LOCATION, PAGE_KERN);
    // GDT (this should be one page)
    Add_Page_Range(GDT_LOCATION,TOP_OF_MEM, PAGE_KERN);

    /* Initialize the kernel heap */
    Init_Heap(KERNEL_HEAP, KERNEL_HEAP_SIZE);

    PrintBoth("%uKB memory detected, %u pages in freelist, %d bytes in kernel heap\n",
	bootInfo->memSizeKB, g_freePageCount, KERNEL_HEAP_SIZE);

    PrintBoth("Memory Layout:\n");

    PrintBoth("%x to %x - INITIAL THREAD\n",KERNEL_THREAD_OBJECT,KERNEL_THREAD_OBJECT+KERNEL_THREAD_OBJECT_SIZE-1);
    PrintBoth("%x to %x - KERNEL STACK\n",KERNEL_STACK,KERNEL_STACK+KERNEL_STACK_SIZE-1);
    PrintBoth("%x to %x - KERNEL HEAP\n",KERNEL_HEAP,KERNEL_HEAP+KERNEL_HEAP_SIZE-1);
    PrintBoth("%lx to %lx - PAGE LIST\n",pageListAddr,pageListEnd-1);
    PrintBoth("%lx to %x - FREE\n",pageListEnd,FINAL_KERNEL_START-1);
    PrintBoth("%x to %x - KERNEL CODE\n",FINAL_KERNEL_START,FINAL_KERNEL_END);
    PrintBoth("%x to %x - IDT\n",IDT_LOCATION,TSS_LOCATION-1);
    PrintBoth("%x to %x - TSS\n",TSS_LOCATION,GDT_LOCATION-1);
    PrintBoth("%x to %x - GDT\n",GDT_LOCATION,TOP_OF_MEM-1);


}

/*
 * Initialize the .bss section of the kernel executable image.
 */
void Init_BSS(void)
{
    extern char BSS_START, BSS_END;

    /* Fill .bss with zeroes */
    memset(&BSS_START, '\0', &BSS_END - &BSS_START);
    PrintBoth("BSS Inited, BSS_START=%x, BSS_END=%x\n",BSS_START,BSS_END);
}

/*
 * Allocate a page of physical memory.
 */
void* Alloc_Page(void)
{
    struct Page* page;
    void *result = 0;

    bool iflag = Begin_Int_Atomic();

    /* See if we have a free page */
    if (!Is_Page_List_Empty(&s_freeList)) {
	/* Remove the first page on the freelist. */
	page = Get_Front_Of_Page_List(&s_freeList);
	KASSERT((page->flags & PAGE_ALLOCATED) == 0);
	Remove_From_Front_Of_Page_List(&s_freeList);

	/* Mark page as having been allocated. */
	page->flags |= PAGE_ALLOCATED;
	g_freePageCount--;
	result = (void*) Get_Page_Address(page);
    }

    End_Int_Atomic(iflag);

    return result;
}

/*
 * Free a page of physical memory.
 */
void Free_Page(void* pageAddr)
{
    ulong_t addr = (ulong_t) pageAddr;
    struct Page* page;
    bool iflag;

    iflag = Begin_Int_Atomic();

    KASSERT(Is_Page_Multiple(addr));

    /* Get the Page object for this page */
    page = Get_Page(addr);
    KASSERT((page->flags & PAGE_ALLOCATED) != 0);

    /* Clear the allocation bit */
    page->flags &= ~(PAGE_ALLOCATED);

    /* Put the page back on the freelist */
    Add_To_Back_Of_Page_List(&s_freeList, page);
    g_freePageCount++;

    End_Int_Atomic(iflag);
}
