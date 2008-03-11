#ifndef __VMM_PAGING_H
#define __VMM_PAGING_H

#include <geekos/ktypes.h>



#include <geekos/vmm_mem.h>
#include <geekos/vmm_util.h>

#define MAX_PAGE_TABLE_ENTRIES      1024
#define MAX_PAGE_DIR_ENTRIES        1024

#define MAX_PAGE_TABLE_ENTRIES_64      512
#define MAX_PAGE_DIR_ENTRIES_64        512
#define MAX_PAGE_DIR_PTR_ENTRIES_64    512
#define MAX_PAGE_MAP_ENTRIES_64        512

#define PAGE_DIRECTORY_INDEX(x)  ((((uint_t)x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x)      ((((uint_t)x) >> 12) & 0x3ff)
#define PAGE_OFFSET(x)           ((((uint_t)x) & 0xfff))

#define PAGE_ALLIGNED_ADDR(x)   (((uint_t) (x)) >> 12)
#define PAGE_ADDR(x)   (PAGE_ALLIGNED_ADDR(x) << 12)

#define PAGE_POWER 12

#define VM_WRITE     1
#define VM_USER      2
#define VM_NOCACHE   8
#define VM_READ      0
#define VM_EXEC      0


#define GUEST_PAGE   0x0
#define SHARED_PAGE  0x1

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


vmm_pde_t * generate_guest_page_tables(vmm_mem_layout_t * layout, vmm_mem_list_t * list);
pml4e64_t * generate_guest_page_tables_64(vmm_mem_layout_t * layout, vmm_mem_list_t * list);

void free_guest_page_tables(vmm_pde_t * pde);

void PrintDebugPageTables(vmm_pde_t * pde);




#endif
