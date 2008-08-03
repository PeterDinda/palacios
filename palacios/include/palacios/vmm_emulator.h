#ifndef __VMM_EMULATOR_H__
#define __VMM_EMULATOR_H__

#ifdef __V3VEE__

#include <palacios/vmm_list.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_paging.h>





struct emulated_page {
  addr_t page_addr;
  addr_t va;
  pte32_t pte;
  struct list_head page_list;
};

struct saved_page {
  addr_t va;
  pte32_t pte;
  struct list_head page_list;
};


struct write_region {
  void * write_data;
  
  uint_t length;
  int (*write)(addr_t write_addr, void * src, uint_t length, void * priv_data);
  addr_t write_addr;
  void * private_data;
  
  struct list_head write_list;
};


struct emulation_state {
  uint_t num_emulated_pages;
  struct list_head emulated_pages;

  uint_t num_saved_pages;
  struct list_head saved_pages;

  uint_t num_write_regions;
  struct list_head write_regions;

  uint_t running : 1;
  uint_t instr_length;
};


int init_emulator(struct guest_info * info);


int v3_emulation_exit_handler(struct guest_info * info);

int v3_emulate_memory_write(struct guest_info * info, addr_t fault_gva,
			    int (*write)(addr_t write_addr, void * src, uint_t length, void * priv_data), 
			    addr_t write_addr, void * private_data);
int v3_emulate_memory_read(struct guest_info * info, addr_t fault_gva, 
			   int (*read)(addr_t read_addr, void * dst, uint_t length, void * priv_data), 
			   addr_t read_addr, void * private_data);


#endif // !__V3VEE__

#endif
