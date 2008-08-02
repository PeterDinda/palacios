#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_decoder.h>



void init_shadow_region(struct shadow_region * entry,
			addr_t               guest_addr_start,
			addr_t               guest_addr_end,
			guest_region_type_t  guest_region_type,
			host_region_type_t   host_region_type)
{
  entry->guest_type = guest_region_type;
  entry->guest_start = guest_addr_start;
  entry->guest_end = guest_addr_end;
  entry->host_type = host_region_type;
  entry->host_addr = 0;
  entry->next=entry->prev = NULL;
}

int add_shadow_region_passthrough( struct guest_info *  guest_info,
				   addr_t               guest_addr_start,
				   addr_t               guest_addr_end,
				   addr_t               host_addr)
{
  struct shadow_region * entry = (struct shadow_region *)V3_Malloc(sizeof(struct shadow_region));

  init_shadow_region(entry, guest_addr_start, guest_addr_end, 
		     GUEST_REGION_PHYSICAL_MEMORY, HOST_REGION_PHYSICAL_MEMORY);
  entry->host_addr = host_addr;

  return add_shadow_region(&(guest_info->mem_map), entry);
}

int hook_guest_mem(struct guest_info * info, addr_t guest_addr_start, addr_t guest_addr_end,
		   int (*read)(addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		   int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data),
		   void * priv_data) {
  
  struct shadow_region * entry = (struct shadow_region *)V3_Malloc(sizeof(struct shadow_region));
  struct vmm_mem_hook * hook = (struct vmm_mem_hook *)V3_Malloc(sizeof(struct vmm_mem_hook));

  memset(hook, 0, sizeof(struct vmm_mem_hook));

  hook->read = read;
  hook->write = write;
  hook->region = entry;
  hook->priv_data = priv_data;


  init_shadow_region(entry, guest_addr_start, guest_addr_end, 
		     GUEST_REGION_PHYSICAL_MEMORY, HOST_REGION_HOOK);

  entry->host_addr = (addr_t)hook;

  return add_shadow_region(&(info->mem_map), entry);
}


struct vmm_mem_hook * get_mem_hook(struct guest_info * info, addr_t guest_addr) {
  struct shadow_region * region = get_shadow_region_by_addr(&(info->mem_map), guest_addr);

  if (region == NULL) {
    PrintDebug("Could not find shadow region for addr: %x\n", guest_addr);
    return NULL;
  }

  return (struct vmm_mem_hook *)(region->host_addr);
}


/* mem_addr is the guest physical memory address */
static int mem_hook_dispatch(struct guest_info * info, addr_t fault_addr, addr_t guest_phys_page,  pf_error_t access_info, struct vmm_mem_hook * hook) {

  // emulate and then dispatch 
  // or dispatch and emulate


  if (access_info.write == 1) {
    void * src = NULL;
    uint_t length = 0;

    PrintDebug("Memory hook write\n");
    return -1;

    if (hook->write(fault_addr, src, length, hook->priv_data) != length) {
      return -1;
    }
  } else {
    PrintDebug("Memory hook read\n");
    return -1;
  }    

  return -1;
}


int handle_special_page_fault(struct guest_info * info, addr_t fault_addr, addr_t guest_phys_page, pf_error_t access_info) {
  struct shadow_region * reg = get_shadow_region_by_addr(&(info->mem_map), guest_phys_page);

  switch (reg->host_type) {
  case HOST_REGION_HOOK:
    return mem_hook_dispatch(info, fault_addr, guest_phys_page, access_info, (struct vmm_mem_hook *)(reg->host_addr));
  default:
    return -1;
  }

  return 0;

}



void init_shadow_map(struct guest_info * info) {
  struct shadow_map * map = &(info->mem_map);

  map->num_regions = 0;

  map->head = NULL;
}


void free_shadow_map(struct shadow_map * map) {
  struct shadow_region * cursor = map->head;
  struct shadow_region * tmp = NULL;

  while(cursor) {
    tmp = cursor;
    cursor = cursor->next;
    V3_Free(tmp);
  }

  V3_Free(map);
}




int add_shadow_region(struct shadow_map * map,
		      struct shadow_region * region) 
{
  struct shadow_region * cursor = map->head;

  PrintDebug("Adding Shadow Region: (0x%x-0x%x)\n", region->guest_start, region->guest_end);

  if ((!cursor) || (cursor->guest_start >= region->guest_end)) {
    region->prev = NULL;
    region->next = cursor;
    map->num_regions++;
    map->head = region;
    return 0;
  }

  while (cursor) {
    // Check if it overlaps with the current cursor
    if ((cursor->guest_end > region->guest_start) && (cursor->guest_start < region->guest_start)) {
      // overlaps not allowed
      return -1;
    }
    
    if (!(cursor->next)) {
      // add to the end of the list
      cursor->next = region;
      region->prev = cursor;
      region->next = NULL;
      map->num_regions++;
      return 0;
    } else if (cursor->next->guest_start >= region->guest_end) {
      // add here
      region->next = cursor->next;
      region->prev = cursor;
      
      cursor->next->prev = region;
      cursor->next = region;

      map->num_regions++;
      
      return 0;
    } else if (cursor->next->guest_end <= region->guest_start) {
      cursor = cursor->next;
    } else {
      // This cannot happen!
      // we should panic here
      return -1;
    }
  }
  
  // This cannot happen
  // We should panic here
  return -1;
}


int delete_shadow_region(struct shadow_map * map,
			 addr_t guest_start,
			 addr_t guest_end) {
  return -1;
}



struct shadow_region *get_shadow_region_by_index(struct shadow_map *  map,
						 uint_t index) {
  struct shadow_region * reg = map->head;
  uint_t i = 0;

  while (reg) { 
    if (i == index) { 
      return reg;
    }
    reg = reg->next;
    i++;
  }
  return NULL;
}


struct shadow_region * get_shadow_region_by_addr(struct shadow_map * map,
						 addr_t addr) {
  struct shadow_region * reg = map->head;

  while (reg) {
    if ((reg->guest_start <= addr) && (reg->guest_end > addr)) {
      return reg;
    } else if (reg->guest_start > addr) {
      return NULL;
    } else {
      reg = reg->next;
    }
  }
  return NULL;
}


host_region_type_t get_shadow_addr_type(struct guest_info * info, addr_t guest_addr) {
  struct shadow_region * reg = get_shadow_region_by_addr(&(info->mem_map), guest_addr);

  if (!reg) {
    return HOST_REGION_INVALID;
  } else {
    return reg->host_type;
  }
}

addr_t get_shadow_addr(struct guest_info * info, addr_t guest_addr) {
  struct shadow_region * reg = get_shadow_region_by_addr(&(info->mem_map), guest_addr);

  if (!reg) {
    return 0;
  } else {
    return (guest_addr - reg->guest_start) + reg->host_addr;
  }
}


host_region_type_t lookup_shadow_map_addr(struct shadow_map * map, addr_t guest_addr, addr_t * host_addr) {
  struct shadow_region * reg = get_shadow_region_by_addr(map, guest_addr);

  if (!reg) {
    // No mapping exists
    return HOST_REGION_INVALID;
  } else {
    switch (reg->host_type) {
    case HOST_REGION_PHYSICAL_MEMORY:
     *host_addr = (guest_addr - reg->guest_start) + reg->host_addr;
     return reg->host_type;
    case HOST_REGION_MEMORY_MAPPED_DEVICE:
    case HOST_REGION_UNALLOCATED:
      // ... 
    default:
      *host_addr = 0;
      return reg->host_type;
    }
  }
}


void print_shadow_map(struct shadow_map * map) {
  struct shadow_region * cur = map->head;
  int i = 0;

  PrintDebug("Memory Layout (regions: %d) \n", map->num_regions);

  while (cur) {
    PrintDebug("%d:  0x%x - 0x%x (%s) -> ", i, cur->guest_start, cur->guest_end - 1,
	       cur->guest_type == GUEST_REGION_PHYSICAL_MEMORY ? "GUEST_REGION_PHYSICAL_MEMORY" :
	       cur->guest_type == GUEST_REGION_NOTHING ? "GUEST_REGION_NOTHING" :
	       cur->guest_type == GUEST_REGION_MEMORY_MAPPED_DEVICE ? "GUEST_REGION_MEMORY_MAPPED_DEVICE" :
	       "UNKNOWN");
    if (cur->host_type == HOST_REGION_PHYSICAL_MEMORY || 
	cur->host_type == HOST_REGION_UNALLOCATED ||
	cur->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) { 
      PrintDebug("0x%x", cur->host_addr);
    }
    PrintDebug("(%s)\n",
	       cur->host_type == HOST_REGION_PHYSICAL_MEMORY ? "HOST_REGION_PHYSICAL_MEMORY" :
	       cur->host_type == HOST_REGION_UNALLOCATED ? "HOST_REGION_UNALLOACTED" :
	       cur->host_type == HOST_REGION_HOOK ? "HOST_REGION_HOOK" :
	       cur->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE ? "HOST_REGION_MEMORY_MAPPED_DEVICE" :
	       cur->host_type == HOST_REGION_REMOTE ? "HOST_REGION_REMOTE" : 
	       cur->host_type == HOST_REGION_SWAPPED ? "HOST_REGION_SWAPPED" :
	       "UNKNOWN");
    cur = cur->next;
    i++;
  }
}










#ifdef VMM_MEM_TEST


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>





struct vmm_os_hooks * os_hooks;

void * TestMalloc(uint_t size) {
  return malloc(size);
}

void * TestAllocatePages(int size) {
  return malloc(4096 * size);
}


void TestPrint(const char * fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

int mem_list_add_test_1(  vmm_mem_list_t * list) {

  uint_t offset = 0;

  PrintDebug("\n\nTesting Memory List\n");

  init_mem_list(list);

  offset = PAGE_SIZE * 6;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 10));
  add_mem_list_pages(list, offset, 10);
  print_mem_list(list);


  offset = 0;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + PAGE_SIZE * 4);
  add_mem_list_pages(list, offset, 4);
  print_mem_list(list);

  offset = PAGE_SIZE * 20;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 1));
  add_mem_list_pages(list, offset, 1);
  print_mem_list(list);

  offset = PAGE_SIZE * 21;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 3));
  add_mem_list_pages(list, offset, 3);
  print_mem_list(list);


  offset = PAGE_SIZE * 10;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 30));
  add_mem_list_pages(list, offset, 30);
  print_mem_list(list);


  offset = PAGE_SIZE * 5;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 1));
  add_mem_list_pages(list, offset, 1);
  print_mem_list(list);

 

  return 0;
}


int mem_layout_add_test_1(vmm_mem_layout_t * layout) {

  
  uint_t start = 0;
  uint_t end = 0;

  PrintDebug("\n\nTesting Memory Layout\n");

  init_mem_layout(layout);

  start = 0x6000;
  end = 0x10000;;
  PrintDebug("Adding 0x%x - 0x%x\n", start, end);
  add_guest_mem_range(layout, start, end);
  print_mem_layout(layout);


  start = 0x1000;
  end = 0x3000;
  PrintDebug("Adding 0x%x - 0x%x\n", start, end);
  add_guest_mem_range(layout, start, end);
  print_mem_layout(layout);

  start = 0x2000;
  end = 0x6000;
  PrintDebug("Adding 0x%x - 0x%x\n", start, end);
  add_guest_mem_range(layout, start, end);
  print_mem_layout(layout);

  start = 0x4000;
  end = 0x5000;
  PrintDebug("Adding 0x%x - 0x%x\n", start, end);
  add_guest_mem_range(layout, start, end);
  print_mem_layout(layout);


  start = 0x5000;
  end = 0x7000;
  PrintDebug("Adding 0x%x - 0x%x\n", start, end);
  add_guest_mem_range(layout, start, end);
  print_mem_layout(layout);




  return 0;
}



int main(int argc, char ** argv) {
  struct vmm_os_hooks dummy_hooks;
  os_hooks = &dummy_hooks;

  vmm_mem_layout_t layout;
  vmm_mem_list_t list;

  os_hooks->malloc = &TestMalloc;
  os_hooks->free = &free;
  os_hooks->print_debug = &TestPrint;
  os_hooks->allocate_pages = &TestAllocatePages;



  printf("mem_list_add_test_1: %d\n", mem_list_add_test_1(&list));
  printf("layout_add_test_1: %d\n", mem_layout_add_test_1(&layout));

  return 0;
}
#endif






