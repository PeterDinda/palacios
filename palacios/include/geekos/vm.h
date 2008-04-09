#ifndef __VM_H
#define __VM_H

#define MAGIC_CODE 0xf1e2d3c4

struct layout_region {
  ulong_t length;
  ulong_t final_addr;
};

struct guest_mem_layout {
  ulong_t magic;
  ulong_t num_regions;
  struct layout_region regions[0];
};


int RunVMM(struct Boot_Info * bootInfo);


#endif

