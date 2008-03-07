#ifndef __VMM_MEM_H
#define __VMM_MEM_H


#include <geekos/ktypes.h>


/*
 * The mem list is TEMPORARY, simply to lock down which pages are assigned to the VM
 * We will remove it and use the host page allocation mechanism in the future
 */


typedef unsigned long addr_t;


typedef struct mem_region {
  addr_t addr;
  uint_t num_pages;

  struct mem_region * next;
  struct mem_region * prev;
} mem_region_t;


typedef struct vmm_mem_list {
  uint_t num_pages;
  bool long_mode;

  uint_t num_regions;
  mem_region_t * head;
  //  mem_region_t * tail;
} vmm_mem_list_t;



/** Memory layout **/
/* Describes the layout of memory for the guest */
/* We use this to build the guest page tables */

typedef enum region_type {GUEST, UNMAPPED, SHARED} region_type_t;


typedef struct layout_region {
  addr_t start;
  addr_t end;

  region_type_t type;

  addr_t host_addr;

  struct layout_region * next;
  struct layout_region * prev;
} layout_region_t;


typedef struct vmm_mem_layout {
  uint_t num_pages;
  uint_t num_regions;

  layout_region_t * head;
} vmm_mem_layout_t;


/*** FOR THE LOVE OF GOD WRITE SOME UNIT TESTS FOR THIS THING ***/

void init_mem_list(vmm_mem_list_t * list);
void free_mem_list(vmm_mem_list_t * list);

int add_mem_list_pages(vmm_mem_list_t * list, addr_t addr, uint_t num_pages);
int remove_mem_list_pages(vmm_mem_list_t * list, addr_t addr, uint_t num_pages);

mem_region_t * get_mem_list_cursor(vmm_mem_list_t * list, addr_t addr);

addr_t get_mem_list_addr(vmm_mem_list_t * list, uint_t index);

void print_mem_list(vmm_mem_list_t * list);


void init_mem_layout(vmm_mem_layout_t * layout);
void free_mem_layout(vmm_mem_layout_t * layout);


int add_mem_range(vmm_mem_layout_t * layout, layout_region_t * region);
int add_shared_mem_range(vmm_mem_layout_t * layout, addr_t start, addr_t end, addr_t host_addr);
int add_unmapped_mem_range(vmm_mem_layout_t * layout, addr_t start, addr_t end);
int add_guest_mem_range(vmm_mem_layout_t * layout, addr_t start, addr_t end);


addr_t get_mem_layout_addr(vmm_mem_layout_t * layout, uint_t index);
layout_region_t * get_mem_layout_region(vmm_mem_layout_t * layout, addr_t addr);

void print_mem_layout(vmm_mem_layout_t * layout);





#endif
