/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#ifndef __VMM_MEM_H
#define __VMM_MEM_H


#ifdef __V3VEE__ 


#include <palacios/vmm_types.h>

#include <palacios/vmm_paging.h>

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
  HOST_REGION_HOOK,                       // This region is mapped as not present (always generate page faults)
  HOST_REGION_PHYSICAL_MEMORY,            // Region is a section of host memory
  HOST_REGION_MEMORY_MAPPED_DEVICE,       // Region is allocated for DMA
  HOST_REGION_UNALLOCATED,                // Region is mapped on demand
  HOST_REGION_REMOTE,                     // Region is located on a remote machine
  HOST_REGION_SWAPPED,                    // Region is swapped
} host_region_type_t;



#define shadow_mem_type_t host_region_type_t

struct shadow_region {
  guest_region_type_t     guest_type;
  addr_t                  guest_start; 
  addr_t                  guest_end; 

  host_region_type_t      host_type;
  addr_t                  host_addr; // This either points to a host address mapping, 
                                     // or a structure holding the map info 

  struct shadow_region *next, *prev;
};



struct shadow_map {
  uint_t num_regions;

  struct shadow_region * head;
};


void init_shadow_region(struct shadow_region * entry,
			   addr_t               guest_addr_start,
			   addr_t               guest_addr_end,
			   guest_region_type_t  guest_region_type,
			   host_region_type_t   host_region_type);

/*
void init_shadow_region_physical(struct shadow_region * entry,
				    addr_t               guest_addr_start,
				    addr_t               guest_addr_end,
				    guest_region_type_t  guest_region_type,
				    addr_t               host_addr_start,
				    host_region_type_t   host_region_type);
*/

int add_shadow_region_passthrough(struct guest_info * guest_info, 
				  addr_t guest_addr_start,
				  addr_t guest_addr_end,
				  addr_t host_addr);

void init_shadow_map(struct guest_info * info);
void free_shadow_map(struct shadow_map * map);

struct shadow_region * get_shadow_region_by_addr(struct shadow_map * map, addr_t guest_addr);

struct shadow_region * get_shadow_region_by_index(struct shadow_map * map, uint_t index);

host_region_type_t lookup_shadow_map_addr(struct shadow_map * map, addr_t guest_addr, addr_t * host_addr);

host_region_type_t get_shadow_addr_type(struct guest_info * info, addr_t guest_addr);
addr_t get_shadow_addr(struct guest_info * info, addr_t guest_addr);

// Semantics:
// Adding a region that overlaps with an existing region results is undefined
// and will probably fail
int add_shadow_region(struct shadow_map * map, struct shadow_region * entry);

// Semantics:
// Deletions result in splitting
int delete_shadow_region(struct shadow_map * map,
			     addr_t guest_start, 
			     addr_t guest_end);


void print_shadow_map(struct shadow_map * map);



struct vmm_mem_hook {
  // Called when data is read from a memory page
  int (*read)(addr_t guest_addr, void * dst, uint_t length, void * priv_data);
  
  // Called when data is written to a memory page
  int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data);

  void * priv_data;
  struct shadow_region * region;
};



struct vmm_mem_hook * get_mem_hook(struct guest_info * info, addr_t guest_addr);

int hook_guest_mem(struct guest_info * info, addr_t guest_addr_start, addr_t guest_addr_end,
		   int (*read)(addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		   int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data),
		   void * priv_data);
int unhook_guest_mem(struct guest_info * info, addr_t guest_addr);




int handle_special_page_fault(struct guest_info * info, addr_t fault_addr, addr_t gp_addr, pf_error_t access_info);


#endif // ! __V3VEE__


#endif
