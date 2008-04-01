#include <geekos/vmm_mem.h>
#include <geekos/vmm.h>
#include <geekos/vmm_util.h>

extern struct vmm_os_hooks * os_hooks;


void init_shadow_region(shadow_region_t * entry,
			addr_t               guest_addr_start,
			addr_t               guest_addr_end,
			guest_region_type_t  guest_region_type,
			host_region_type_t   host_region_type)
{
  entry->guest_type = guest_region_type;
  entry->guest_start = guest_addr_start;
  entry->guest_end = guest_addr_end;
  entry->host_type = host_region_type;
  entry->next=entry->prev = NULL;
}

void init_shadow_region_physical(shadow_region_t * entry,
				 addr_t               guest_addr_start,
				 addr_t               guest_addr_end,
				 guest_region_type_t  guest_region_type,
				 addr_t               host_addr_start,
				 host_region_type_t   host_region_type)
{
  init_shadow_region(entry, guest_addr_start, guest_addr_end, guest_region_type, host_region_type);
  entry->host_addr.phys_addr.host_start = host_addr_start;

}
		    

void init_shadow_map(shadow_map_t * map) {
  map->num_regions = 0;

  map->head = NULL;
}


void free_shadow_map(shadow_map_t * map) {
  shadow_region_t * cursor = map->head;
  shadow_region_t * tmp = NULL;

  while(cursor) {
    tmp = cursor;
    cursor = cursor->next;
    VMMFree(tmp);
  }

  VMMFree(map);
}



/* This is slightly different semantically from the mem list, in that
 * we don't allow overlaps we could probably allow overlappig regions
 * of the same type... but I'll let someone else deal with that
 */
int add_shadow_region(shadow_map_t * map,
		      shadow_region_t * region) 
{
  shadow_region_t * cursor = map->head;

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
    } else if (cursor->next->guest_end < region->guest_start) {
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


int delete_shadow_region(shadow_map_t * map,
			 addr_t guest_start,
			 addr_t guest_end) {
  return -1;
}



shadow_region_t *get_shadow_region_by_index(shadow_map_t *  map,
					       uint_t index) {
  shadow_region_t * reg = map->head;
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


shadow_region_t * get_shadow_region_by_addr(shadow_map_t * map,
					       addr_t addr) {
  shadow_region_t * reg = map->head;

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



host_region_type_t lookup_shadow_map_addr(shadow_map_t * map, addr_t guest_addr, addr_t * host_addr) {
  shadow_region_t * reg = get_shadow_region_by_addr(map, guest_addr);

  if (!reg) {
    // No mapping exists
    return HOST_REGION_INVALID;
  } else {
    switch (reg->host_type) {
    case HOST_REGION_PHYSICAL_MEMORY:
     *host_addr = (guest_addr - reg->guest_start) + reg->host_addr.phys_addr.host_start;
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


void print_shadow_map(shadow_map_t * map) {
  shadow_region_t * cur = map->head;
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
      PrintDebug("0x%x", cur->host_addr.phys_addr.host_start);
    }
    PrintDebug("(%s)\n",
	       cur->host_type == HOST_REGION_PHYSICAL_MEMORY ? "HOST_REGION_PHYSICAL_MEMORY" :
	       cur->host_type == HOST_REGION_UNALLOCATED ? "HOST_REGION_UNALLOACTED" :
	       cur->host_type == HOST_REGION_NOTHING ? "HOST_REGION_NOTHING" :
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






