#ifndef __VMM_MEM_H
#define __VMM_MEM_H


#include <geekos/ktypes.h>

typedef ulong_t addr_t;

/*

        Guest                  Shadow                 Host
  Virtual   Physical    Virtual     Physical   Virtual     Physical
               OK                      OK
               OK                      NOK
               NOK                     OK
               NOK                     NOK

*/

// These are the types of physical memory address regions
// from the perspective of the guest
typedef enum guest_region_type { 
  GUEST_REGION_PHYSICAL_MEMORY, 
  GUEST_REGION_NOTHING, 
  GUEST_REGION_MEMORY_MAPPED_DEVICE} guest_region_type_t;

// These are the types of physical memory address regions
// from the perspective of the HOST
typedef enum host_region_type { 
  HOST_REGION_PHYSICAL_MEMORY, 
  HOST_REGION_UNALLOCATED, 
  HOST_REGION_NOTHING, 
  HOST_REGION_MEMORY_MAPPED_DEVICE,
  HOST_REGION_REMOTE,
  HOST_REGION_SWAPPED,
} host_region_type_t;



typedef struct shadow_region {
  guest_region_type_t     guest_type;
  addr_t                  guest_start; 
  addr_t                  guest_end; 

  host_region_type_t      host_type;
  union host_addr_t {
    struct physical_addr { 
       addr_t                  host_start; 
    }                     phys_addr;
    // Other addresses, like on disk, etc, would go here
  }                       host_addr;
  struct shadow_region *next, *prev;
} shadow_region_t;



typedef struct shadow_map {
  uint_t num_regions;

  shadow_region_t * head;
} shadow_map_t;


void init_shadow_region(shadow_region_t * entry,
			   addr_t               guest_addr_start,
			   addr_t               guest_addr_end,
			   guest_region_type_t  guest_region_type,
			   host_region_type_t   host_region_type);

void init_shadow_region_physical(shadow_region_t * entry,
				    addr_t               guest_addr_start,
				    addr_t               guest_addr_end,
				    guest_region_type_t  guest_region_type,
				    addr_t               host_addr_start,
				    host_region_type_t   host_region_type);
  
void init_shadow_map(shadow_map_t * map);
void free_shadow_map(shadow_map_t * map);

shadow_region_t * get_shadow_region_by_addr(shadow_map_t * map, addr_t guest_addr);

shadow_region_t * get_shadow_region_by_index(shadow_map_t * map, uint_t index);

int guest_paddr_to_host_paddr(shadow_region_t * entry, 
			      addr_t guest_addr,
			      addr_t * host_addr);


// Semantics:
// Adding a region that overlaps with an existing region results is undefined
// and will probably fail
int add_shadow_region(shadow_map_t * map, shadow_region_t * entry);

// Semantics:
// Deletions result in splitting
int delete_shadow_region(shadow_map_t * map,
			     addr_t guest_start, 
			     addr_t guest_end);


void print_shadow_map(shadow_map_t * map);





#endif
