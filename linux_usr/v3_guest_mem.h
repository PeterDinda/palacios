#ifndef _V3_GUEST_MEM_
#define _V3_GUEST_MEM_

#include <stdint.h>
#include "v3_ctrl.h"

struct v3_guest_mem_block {
  void     *gpa;      // guest physical address this region starts at
  void     *cumgpa;   // cumulative GPA in the VM including this block
  void     *hpa;      // mapped to this host physical address
  void     *uva;      // which is mapped here in this process
  uint64_t numpages;  // this many 4K pages
};

// Whole memory map of guest's physical memory
// that is backed by host physical memory (ie, everything we 
// can read or write from the host user space)
struct v3_guest_mem_map {
  int      fd;              // used by mmap
  uint64_t numblocks;
  struct v3_guest_mem_block block[0];
};

// This function gets the basic memory map, but does not map it
struct v3_guest_mem_map * v3_guest_mem_get_map(char *vmdev);
// This function mmaps it into the guest address space
// and fills out the "myaddr" fields
int v3_map_guest_mem(struct v3_guest_mem_map *map);
// This function unmaps it - it assumes myaddrs are valid
int v3_unmap_guest_mem(struct v3_guest_mem_map *map);
// What is the first GPA of this guest that contains memory ?
void *v3_gpa_start(struct v3_guest_mem_map *map);
// What is the last GPA of this guest that contains memory ?
void *v3_gpa_end(struct v3_guest_mem_map *map);
// map from a gpa to a uva, optionally giving the number of 
// subsequent bytes for which the mapping is continguous
static inline void *v3_gpa_to_uva(struct v3_guest_mem_map *map, void *gpa, uint64_t *num_bytes)
{
    uint64_t left, right, middle;

    if (!map->fd) { 
	// not mapped
	if (num_bytes) { *num_bytes=0;}
	return 0;
    }
    
    left = 0; right=map->numblocks-1;
    
    while (right>=left) { 
	middle = (right+left)/2;
	if (gpa > map->block[middle].cumgpa) { 
	    left=middle+1;
	} else if (gpa < map->block[middle].gpa) { 
	    right=middle-1;
	} else {
	    break;
	}
    }

    if (right>=left) { 
	if (num_bytes) { 
	    *num_bytes = map->block[middle].cumgpa - gpa + 1;
	}
	return map->block[middle].uva + (gpa - map->block[middle].gpa);
    } else {
	if (num_bytes) { 
	    *num_bytes=0;
	} 
	return 0;
    }
}

// efficiently map function over the specified guest memory
int v3_guest_mem_apply(void (*func)(void *data, uint64_t num_bytes, void *priv),
		       struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, void *priv);

// read data out of guest
int v3_guest_mem_read(struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, char *data);

// write data into  guest
int v3_guest_mem_write(struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, char *data);

// hash the guest's data
int v3_guest_mem_hash(struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, uint64_t *hash);

#endif

