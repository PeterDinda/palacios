#ifndef __VMM_MEM_H
#define __VMM_MEM_H


#include <geekos/ktypes.h>


typedef struct mem_region {
  ullong_t addr;
  uint_t numPages;

  struct mem_region * next;
  struct mem_region * prev;
} mem_region_t;


typedef struct vmm_mem_map {
  uint_t num_pages;
  bool long_mode;

  uint_t num_regions;
  mem_region_t * head;
  mem_region_t * tail;
} vmm_mem_map_t;


void init_mem_map(vmm_mem_map_t * map);

void add_pages(vmm_mem_map_t * map, ullong_t addr, uint_t numPages);
int remove_pages(vmm_mem_map_t * map, ullong_t addr, uint_t numPages);



#endif
