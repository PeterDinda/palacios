#ifndef _V3_GUEST_MEM_
#define _V3_GUEST_MEM_

#include <stdint.h>
#include "v3_ctrl.h"

struct v3_guest_mem_block {
  void     *gpa;      // guest physical address is
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


#endif

