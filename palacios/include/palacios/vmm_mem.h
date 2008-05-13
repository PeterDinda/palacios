#ifndef __VMM_MEM_H
#define __VMM_MEM_H



#include <palacios/vmm_types.h>


typedef ulong_t addr_t;

struct guest_info;


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
  GUEST_REGION_NOTHING, 
  GUEST_REGION_PHYSICAL_MEMORY, 
  GUEST_REGION_MEMORY_MAPPED_DEVICE} guest_region_type_t;

// These are the types of physical memory address regions
// from the perspective of the HOST
typedef enum host_region_type { 
  HOST_REGION_INVALID,                    // This region is INVALID (this is a return type, to denote errors)
  HOST_REGION_NOTHING,                    // This region is mapped as not present (always generate page faults)
  HOST_REGION_PHYSICAL_MEMORY,            // Region is a section of host memory
  HOST_REGION_MEMORY_MAPPED_DEVICE,       // Region is allocated for DMA
  HOST_REGION_UNALLOCATED,                // Region is mapped on demand
  HOST_REGION_REMOTE,                     // Region is located on a remote machine
  HOST_REGION_SWAPPED,                    // Region is swapped
} host_region_type_t;



#define shadow_mem_type_t host_region_type_t

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



struct shadow_map {
  uint_t num_regions;

  shadow_region_t * head;
};


void init_shadow_region(shadow_region_t * entry,
			   addr_t               guest_addr_start,
			   addr_t               guest_addr_end,
			   guest_region_type_t  guest_region_type,
			   host_region_type_t   host_region_type);

/*
void init_shadow_region_physical(shadow_region_t * entry,
				    addr_t               guest_addr_start,
				    addr_t               guest_addr_end,
				    guest_region_type_t  guest_region_type,
				    addr_t               host_addr_start,
				    host_region_type_t   host_region_type);
*/

int add_shadow_region_passthrough(struct guest_info * guest_info, 
				  addr_t guest_addr_start,
				  addr_t guest_addr_end,
				  addr_t host_addr_start);

void init_shadow_map(struct shadow_map * map);
void free_shadow_map(struct shadow_map * map);

shadow_region_t * get_shadow_region_by_addr(struct shadow_map * map, addr_t guest_addr);

shadow_region_t * get_shadow_region_by_index(struct shadow_map * map, uint_t index);

host_region_type_t lookup_shadow_map_addr(struct shadow_map * map, addr_t guest_addr, addr_t * host_addr);

host_region_type_t get_shadow_addr_type(struct guest_info * info, addr_t guest_addr);
addr_t get_shadow_addr(struct guest_info * info, addr_t guest_addr);

// Semantics:
// Adding a region that overlaps with an existing region results is undefined
// and will probably fail
int add_shadow_region(struct shadow_map * map, shadow_region_t * entry);

// Semantics:
// Deletions result in splitting
int delete_shadow_region(struct shadow_map * map,
			     addr_t guest_start, 
			     addr_t guest_end);


void print_shadow_map(struct shadow_map * map);




#endif
