#ifndef __VMM_PAGING_H
#define __VMM_PAGING_H


#include <geekos/ktypes.h>



#include <geekos/vmm_mem.h>
#include <geekos/vmm_util.h>

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




#define MAX_PAGE_TABLE_ENTRIES      1024
#define MAX_PAGE_DIR_ENTRIES        1024

#define MAX_PAGE_TABLE_ENTRIES_64      512
#define MAX_PAGE_DIR_ENTRIES_64        512
#define MAX_PAGE_DIR_PTR_ENTRIES_64    512
#define MAX_PAGE_MAP_ENTRIES_64        512

#define PAGE_DIRECTORY_INDEX(x)  ((((uint_t)x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x)      ((((uint_t)x) >> 12) & 0x3ff)
#define PAGE_OFFSET(x)           ((((uint_t)x) & 0xfff))

#define PAGE_ALIGNED_ADDR(x)   (((uint_t) (x)) >> 12)

#ifndef PAGE_ADDR
#define PAGE_ADDR(x)   (PAGE_ALIGNED_ADDR(x) << 12)
#endif

#define PAGE_POWER 12

#define CR3_TO_PDE(cr3) (((ulong_t)cr3) & 0xfffff000)
#define CR3_TO_PDPTRE(cr3) (((ulong_t)cr3) & 0xffffffe0)
#define CR3_TO_PML4E(cr3)  (((ullong_t)cr3) & 0x000ffffffffff000)

#define VM_WRITE     1
#define VM_USER      2
#define VM_NOCACHE   8
#define VM_READ      0
#define VM_EXEC      0


typedef struct pde {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t reserved        : 1;
  uint_t large_pages     : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t pt_base_addr    : 20;
} vmm_pde_t;

typedef struct pte {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t dirty           : 1;
  uint_t pte_attr        : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t page_base_addr  : 20;
} vmm_pte_t;



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


typedef struct pml4e {
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



typedef enum { PDE32 } paging_mode_t;


typedef struct shadow_page_state {

  // these two reflect the top-level page directory
  // of the guest page table
  paging_mode_t           guest_mode;
  reg_ex_t                guest_cr3;         // points to guest's current page table

  // Should thi sbe here
  reg_ex_t                guest_cr0;

  // these two reflect the top-level page directory 
  // the shadow page table
  paging_mode_t           shadow_mode;
  reg_ex_t                shadow_cr3;


} shadow_page_state_t;



int init_shadow_page_state(shadow_page_state_t * state);

// This function will cause the shadow page table to be deleted
// and rewritten to reflect the guest page table and the shadow map
int wholesale_update_shadow_page_state(shadow_page_state_t * state, shadow_map_t * mem_map);

vmm_pde_t * create_passthrough_pde32_pts(shadow_map_t * map);

//void free_guest_page_tables(vmm_pde_t * pde);

void PrintDebugPageTables(vmm_pde_t * pde);


#endif
