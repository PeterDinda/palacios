 /*
 * Physical memory allocation
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * $Revision: 1.13 $
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
#include <geekos/debug.h>


/* ----------------------------------------------------------------------
 * Global data
 * ---------------------------------------------------------------------- */

/*
 * List of Page structures representing each page of physical memory.
 */
struct Page* g_pageList;

void * g_ramdiskImage;
ulong_t s_ramdiskSize;


/*
 * Number of pages currently available on the freelist.
 */
uint_t g_freePageCount = 0;



/* 
 *  the disgusting way to get at the memory assigned to a VM
 */
extern ulong_t vm_range_start;
extern ulong_t vm_range_end;
extern ulong_t guest_kernel_start;
extern ulong_t guest_kernel_end;



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

    PrintBoth("Start: %u (0x%x), End: %u(0x%x)  (Type=0x%.4x)\n", (unsigned int)start, start, (unsigned int)end, end, flags);

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
    ulong_t heapAddr;
    ulong_t heapEnd;
    ulong_t vmmMemEnd;


    g_ramdiskImage = bootInfo->ramdisk_image;
    s_ramdiskSize = bootInfo->ramdisk_size;
    ulong_t initrdAddr = 0;
    ulong_t initrdEnd = 0;

    

    KASSERT(bootInfo->memSizeKB > 0);


    /*
     * Before we do anything, switch from setup.asm's temporary GDT
     * to the kernel's permanent GDT.
     */
    Init_GDT();


    PrintBoth("Total Memory Size: %u MBytes\n", bootInfo->memSizeKB/1024);
    PrintBoth("Page List (at 0x%x) Size: %u bytes\n", &s_freeList, numPageListBytes);

  
    /* Memory Layout:
     * bios area (1 page reserved) 
     * kernel_thread_obj (1 page)
     * kernel_stack (1 page)
     * available space
     * available space
     * ISA_HOLE_START - ISA_HOLE_END: hardware
     * EXTENDED_MEMORY:
     *        start - end:       kernel
     *        VM Guest (variable pages)
     *        Heap (512 Pages)
     *        Page List (variable pages)
     *        Available Memory for VMM (4096 pages)
     *        Ramdisk //Zheng 08/03/2008
     *        VM Memory (everything else)
     */

    //kernEnd = Round_Up_To_Page((ulong_t)&end);
    kernEnd = (ulong_t)&end;

    PrintBoth("Kernel End=%lx\n", kernEnd);


    /* ************************************************************************************** */
    /* If we have dynamic loading of the guest kernel, we should put the relocation code here */
    /* ************************************************************************************** */

    kernEnd = Round_Up_To_Page(kernEnd);
    heapAddr = kernEnd;
    heapEnd = Round_Up_To_Page(heapAddr + KERNEL_HEAP_SIZE);
    pageListAddr = heapEnd;
    pageListEnd = Round_Up_To_Page(pageListAddr + numPageListBytes);
    /* Global variables */
    // These must be set before we can call Add_Page_Range..
    g_pageList = (struct Page*) pageListAddr;
    s_numPages = numPages;
    /* ** */
    vmmMemEnd = Round_Up_To_Page(pageListEnd + VMM_AVAIL_MEM_SIZE);

    /*
     * Zheng 08/03/2008
     * copy the ramdisk to this area 
     */
    if (s_ramdiskSize > 0) {
      initrdAddr = vmmMemEnd;
      initrdEnd = Round_Up_To_Page(initrdAddr + s_ramdiskSize);
      PrintBoth("mem.c(%d) Move ramdisk(%dB) from %x to %x", __LINE__, s_ramdiskSize, g_ramdiskImage, initrdAddr);
      memcpy((ulong_t *)initrdAddr, (ulong_t *)g_ramdiskImage, s_ramdiskSize);
      PrintBoth(" done\n");
      PrintBoth("mem.c(%d) Set 0 to unused bytes in the last ramdisk page from %x to %x", __LINE__, initrdAddr+s_ramdiskSize, initrdEnd);
      memset((ulong_t *)initrdAddr + s_ramdiskSize, 0, initrdEnd - (initrdAddr + s_ramdiskSize));
      PrintBoth(" done\n");
    }

    
    /* 
     *  the disgusting way to get at the memory assigned to a VM
     */
    
    //vm_range_start = vmmMemEnd;
    //vm_range_end = endOfMem;
    /*
     * Zheng 08/03/2008
     */
    if (s_ramdiskSize > 0) {
      vm_range_start = initrdEnd;
      vm_range_end = endOfMem;    
    }

    Add_Page_Range(0, PAGE_SIZE, PAGE_UNUSED);                        // BIOS area
    Add_Page_Range(PAGE_SIZE, PAGE_SIZE * 3, PAGE_ALLOCATED);         // Intial kernel thread obj + stack
    Add_Page_Range(PAGE_SIZE * 3, ISA_HOLE_START, PAGE_AVAIL);     // Available space
    Add_Page_Range(ISA_HOLE_START, ISA_HOLE_END, PAGE_HW);            // Hardware ROMs
    Add_Page_Range(KERNEL_START_ADDR, kernEnd, PAGE_KERN);            // VMM Kernel
    //    Add_Page_Range(guest_kernel_start, guestEnd, PAGE_VM);                  // Guest kernel location
    Add_Page_Range(heapAddr, heapEnd, PAGE_HEAP);                     // Heap
    Add_Page_Range(pageListAddr, pageListEnd, PAGE_KERN);             // Page List 
    Add_Page_Range(pageListEnd, vmmMemEnd, PAGE_AVAIL);               // Available VMM memory


    if (s_ramdiskSize > 0) {
      /*
       * Zheng 08/03/2008
       */
      Add_Page_Range(vmmMemEnd, initrdEnd, PAGE_ALLOCATED);              //Ramdisk memory area      
      //    Add_Page_Range(vmmMemEnd, endOfMem, PAGE_VM);                // Memory allocated to the VM
      // Until we get a more intelligent memory allocator
      Add_Page_Range(initrdEnd, endOfMem, PAGE_AVAIL);                   // Memory allocated to the VM
    } else {
      Add_Page_Range(vmmMemEnd, endOfMem, PAGE_AVAIL);                   // Memory allocated to the VM
    }

    /* Initialize the kernel heap */
    Init_Heap(heapAddr, KERNEL_HEAP_SIZE);

    PrintBoth("%uKB memory detected, %u pages in freelist, %d bytes in kernel heap\n",
	bootInfo->memSizeKB, g_freePageCount, KERNEL_HEAP_SIZE);

    PrintBoth("Memory Layout:\n");
    PrintBoth("%x to %x - BIOS AREA\n", 0, PAGE_SIZE - 1);
    PrintBoth("%x to %x - KERNEL_THREAD_OBJ\n", PAGE_SIZE, PAGE_SIZE * 2 - 1);
    PrintBoth("%x to %x - KERNEL_STACK\n", PAGE_SIZE * 2, PAGE_SIZE * 3 - 1);
    PrintBoth("%lx to %x - FREE\n", PAGE_SIZE * 3, ISA_HOLE_START - 1);
    PrintBoth("%x to %x - ISA_HOLE\n", ISA_HOLE_START, ISA_HOLE_END - 1);
    PrintBoth("%x to %x - KERNEL CODE + VM_KERNEL\n", KERNEL_START_ADDR, kernEnd - 1);
    //    PrintBoth("%x to %x - VM_KERNEL\n", kernEnd, guestEnd - 1);
    PrintBoth("%x to %x - KERNEL HEAP\n", heapAddr, heapEnd - 1);
    PrintBoth("%lx to %lx - PAGE LIST\n", pageListAddr, pageListEnd - 1);
    PrintBoth("%lx to %x - FREE\n", pageListEnd, vmmMemEnd - 1);


    if (s_ramdiskSize > 0) {
      /*
       * Zheng 08/03/2008
       */
      PrintBoth("%lx to %x - RAMDISK\n", vmmMemEnd, initrdEnd - 1);
      
      PrintBoth("%lx to %x - GUEST_MEMORY (also free)\n", initrdEnd, endOfMem - 1);
    } else {
      PrintBoth("%lx to %x - GUEST_MEMORY (also free)\n", vmmMemEnd, endOfMem - 1);
    }
    
}

/*
 * Initialize the .bss section of the kernel executable image.
 */
void Init_BSS(void)
{
    extern char BSS_START, BSS_END;

    /* Fill .bss with zeroes */
    memset(&BSS_START, '\0', &BSS_END - &BSS_START);
    // screen is not inited yet - PAD
    // PrintBoth("BSS Inited, BSS_START=%x, BSS_END=%x\n",BSS_START,BSS_END);
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
