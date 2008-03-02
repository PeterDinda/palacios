#include <geekos/vmm_mem.h>
#include <geekos/vmm.h>


extern struct vmm_os_hooks * os_hooks;


void init_mem_map(vmm_mem_map_t * map) {
  map->num_pages = 0;
  map->long_mode = false;
  
  map->num_regions = 0;
  map->head = NULL;
  map->tail = NULL;
}


void add_mem_map_pages(vmm_mem_map_t * map, ullong_t addr, uint_t numPages) {
  


}
