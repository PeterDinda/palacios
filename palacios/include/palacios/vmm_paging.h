#ifndef __VMM_PAGING_H
#define __VMM_PAGING_H


#include <palacios/vmm_types.h>



#include <palacios/vmm_mem.h>
#include <palacios/vmm_util.h>

/*

In the following, when we say "page table", we mean the whole 2 or 4 layer
page table (PDEs, PTEs), etc.


guest-visible paging state
 This is the state that the guest thinks the machine is using
 It consists of
   - guest physical memory
       The physical memory addresses the guest is allowed to use
       (see shadow page maps, below)
   - guest page tables 
       (we care about when the current one changes)
   - guest paging registers (these are never written to hardware)
        CR0
        CR3


shadow paging state
 This the state that the machine will actually use when the guest
 is running.  It consists of:
   - current shadow page table
        This is the page table actually useed when the guest is running.
        It is changed/regenerated when the guest page table changes
        It mostly reflects the guest page table, except that it restricts 
        physical addresses to those the VMM allocates to the guest.
   - shadow page maps
        This is a mapping from guest physical memory addresses to
        the current location of the guest physical memory content.   
        It maps from regions of physical memory addresses to regions 
        located in physical memory or elsewhere.  
        (8192,16384) -> MEM(8912,...)
        (0,8191) -> DISK(65536,..) 
   - guest paging registers (these are written to guest state)
        CR0
        CR3

host paging state
  This is the state we expect to be operative when the VMM is running.
  Typically, this is set up by the host os into which we have embedded
  the VMM, but we include the description here for clarity.
    - current page table
        This is the page table we use when we are executing in 
        the VMM (or the host os)
    - paging regisers
        CR0
        CR3


The reason why the shadow paging state and the host paging state are
distinct is to permit the guest to use any virtual address it wants,
irrespective of the addresses the VMM or the host os use.  These guest
virtual addresses are reflected in the shadow paging state.  When we
exit from the guest, we switch to the host paging state so that any
virtual addresses that overlap between the guest and VMM/host now map
to the physical addresses epxected by the VMM/host.  On AMD SVM, this
switch is done by the hardware.  On Intel VT, the switch is done
by the hardware as well, but we are responsible for manually updating
the host state in the vmcs before entering the guest.


*/




#define MAX_PTE32_ENTRIES          1024
#define MAX_PDE32_ENTRIES          1024

#define MAX_PTE64_ENTRIES          512
#define MAX_PDE64_ENTRIES          512
#define MAX_PDPE64_ENTRIES         512
#define MAX_PML4E64_ENTRIES        512

#define PDE32_INDEX(x)  ((((uint_t)x) >> 22) & 0x3ff)
#define PTE32_INDEX(x)  ((((uint_t)x) >> 12) & 0x3ff)


#define PAGE_ALIGNED_ADDR(x)   (((uint_t) (x)) >> 12)

#ifndef PAGE_ADDR
#define PAGE_ADDR(x)   (PAGE_ALIGNED_ADDR(x) << 12)
#endif
#define PAGE_OFFSET(x)  ((((uint_t)x) & 0xfff))


#define PAGE_POWER 12

#define CR3_TO_PDE32(cr3) (((ulong_t)cr3) & 0xfffff000)
#define CR3_TO_PDPTRE(cr3) (((ulong_t)cr3) & 0xffffffe0)
#define CR3_TO_PML4E64(cr3)  (((ullong_t)cr3) & 0x000ffffffffff000)

#define VM_WRITE     1
#define VM_USER      2
#define VM_NOCACHE   8
#define VM_READ      0
#define VM_EXEC      0


/* PDE 32 bit PAGE STRUCTURES */
typedef enum {NOT_PRESENT, PTE32, LARGE_PAGE} pde32_entry_type_t;

typedef struct pde32 {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t reserved        : 1;
  uint_t large_pages     : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t pt_base_addr    : 20;
} pde32_t;

typedef struct pte32 {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t dirty           : 1;
  uint_t pte_attr        : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t page_base_addr  : 20;
} pte32_t;
/* ***** */

/* 32 bit PAE PAGE STRUCTURES */

//
// Fill in
//

/* ********** */


/* LONG MODE 64 bit PAGE STRUCTURES */
typedef struct pml4e64 {
  uint_t present        : 1;
  uint_t writable       : 1;
  uint_t user           : 1;
  uint_t pwt            : 1;
  uint_t pcd            : 1;
  uint_t accessed       : 1;
  uint_t reserved       : 1;
  uint_t zero           : 2;
  uint_t vmm_info       : 3;
  uint_t pdp_base_addr_lo : 20;
  uint_t pdp_base_addr_hi : 20;
  uint_t available      : 11;
  uint_t no_execute     : 1;
} pml4e64_t;


typedef struct pdpe64 {
  uint_t present        : 1;
  uint_t writable       : 1;
  uint_t user           : 1;
  uint_t pwt            : 1;
  uint_t pcd            : 1;
  uint_t accessed       : 1;
  uint_t reserved       : 1;
  uint_t large_pages    : 1;
  uint_t zero           : 1;
  uint_t vmm_info       : 3;
  uint_t pd_base_addr_lo : 20;
  uint_t pd_base_addr_hi : 20;
  uint_t available      : 11;
  uint_t no_execute     : 1;
} pdpe64_t;




typedef struct pde64 {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t reserved        : 1;
  uint_t large_pages     : 1;
  uint_t reserved2       : 1;
  uint_t vmm_info        : 3;
  uint_t pt_base_addr_lo    : 20;
  uint_t pt_base_addr_hi : 20;
  uint_t available       : 11;
  uint_t no_execute      : 1;
} pde64_t;

typedef struct pte64 {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t dirty           : 1;
  uint_t pte_attr        : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t page_base_addr_lo  : 20;
  uint_t page_base_addr_hi : 20;
  uint_t available       : 11;
  uint_t no_execute      : 1;
} pte64_t;

/* *************** */


typedef enum { PDE32 } paging_mode_t;




void delete_page_tables_pde32(pde32_t * pde);


pde32_entry_type_t pde32_lookup(pde32_t * pde, addr_t addr, addr_t * entry);
int pte32_lookup(pte32_t * pte, addr_t addr, addr_t * entry);



struct guest_info;

pde32_t * create_passthrough_pde32_pts(struct guest_info * guest_info);






void PrintDebugPageTables(pde32_t * pde);




#endif
