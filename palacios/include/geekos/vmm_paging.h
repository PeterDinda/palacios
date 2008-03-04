#ifndef __VMM_PAGING_H
#define __VMM_PAGING_H

#include <geekos/ktypes.h>
#include <geekos/vmm.h>


#include <geekos/vmm_mem.h>
#include <geekos/vmm_util.h>

#define MAX_PAGE_TABLE_ENTRIES      1024
#define MAX_PAGE_DIR_ENTRIES        1024


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


typedef struct pde {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t reserved        : 1;
  uint_t large_pages     : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t pt_base_addr    : 20;
} pde_t;

typedef struct pte {
  uint_t present         : 1;
  uint_t flags           : 4;
  uint_t accessed        : 1;
  uint_t dirty           : 1;
  uint_t pte_attr        : 1;
  uint_t global_page     : 1;
  uint_t vmm_info        : 3;
  uint_t page_base_addr  : 20;
} pte_t;


pde_t * generate_guest_page_tables(vmm_mem_layout_t * layout, vmm_mem_list_t * list);


void PrintDebugPageTables(pde_t * pde);




#endif
