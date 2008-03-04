#include <geekos/vmm_mem.h>
#include <geekos/vmm.h>
#include <geekos/vmm_util.h>

extern struct vmm_os_hooks * os_hooks;


void init_mem_list(vmm_mem_list_t * list) {
  list->num_pages = 0;
  list->long_mode = false;
  
  list->num_regions = 0;
  list->head = NULL;
}


void free_mem_list(vmm_mem_list_t * list) {
  mem_region_t * cursor = list->head;
  mem_region_t * tmp = NULL;

  while(cursor) {
    tmp = cursor;
    cursor = cursor->next;
    VMMFree(tmp);
  }

  VMMFree(list);
}

/*** FOR THE LOVE OF GOD WRITE SOME UNIT TESTS FOR THIS THING ***/



// Scan the current list, and extend an existing region if one exists
// Otherwise create a new region and merge it into the correct location in the list
//
// We scan to find the position at which to add the new region and insert it
// Then we clean up any region following the new region that overlaps
// 
// JRL: This is pretty hairy...
int add_mem_list_pages(vmm_mem_list_t * list, addr_t addr, uint_t num_pages) {

  uint_t num_new_pages = num_pages;
  addr_t new_end = addr + (num_pages * PAGE_SIZE) - 1;

  mem_region_t * cursor = get_mem_list_cursor(list, addr);


  // PrintDebug("Adding: 0x%x - 0x%x\n", addr, num_pages * PAGE_SIZE);


  // Make a new region at the head of the list
  if (cursor == NULL) {
    cursor = os_hooks->malloc(sizeof(mem_region_t));

    cursor->prev = NULL;
    cursor->addr = addr;
    cursor->num_pages = num_pages;

    cursor->next = list->head;
    list->head = cursor;

    if (cursor->next) {
      cursor->next->prev = cursor;
    }

    list->num_regions++;
  } else {
    addr_t cursor_end = cursor->addr + (cursor->num_pages * PAGE_SIZE) - 1;

    if (addr > cursor_end + 1) {
      // address falls after cursor region
      
      mem_region_t * new_region = os_hooks->malloc(sizeof(mem_region_t));

      new_region->prev = cursor;
      new_region->next = cursor->next;

      if (cursor->next) {
	cursor->next->prev = new_region;
      }
      cursor->next = new_region;

      new_region->addr = addr;
      new_region->num_pages = num_pages;

      list->num_regions++;

      cursor = new_region;
    } else if ((addr >= cursor->addr) && 
	       (addr <= cursor_end + 1)) {
      // address falls inside the cursor region


      // The region has already been added
      if (new_end <= cursor_end) {
	return -1;
      }

      // We need to extend the old region
      num_new_pages = (new_end - cursor_end) / PAGE_SIZE;
      cursor->num_pages += num_new_pages;

    }
  }

    
  // Clean up any overlaps that follow
  while ((cursor->next) && (cursor->next->addr <= new_end + 1)) {
    mem_region_t * overlap = cursor->next;
    addr_t overlap_end = overlap->addr + (overlap->num_pages * PAGE_SIZE) - 1;
    
    cursor->next = overlap->next;
    if (overlap->next) {
      overlap->next->prev = cursor;
    }
    
    if (overlap_end > new_end) {
      uint_t extension = (overlap_end - new_end) / PAGE_SIZE;

      cursor->num_pages += extension;
      num_new_pages -= (overlap->num_pages - extension);
    } else {
      num_new_pages -= overlap->num_pages;
    }
    
    VMMFree(overlap);
    
    list->num_regions--;
  }


  list->num_pages += num_new_pages;

  return 0;
}


/* this function returns a pointer to the location in the memory list that 
 * corresponds to addr.
 * Rules: 
 *     IF addr is in a region, a ptr to that region is returned
 *     IF addr is not in a region, a ptr to the previous region is returned
 *     IF addr is before all regions, returns NULL
 *     IF list is empty, returns NULL
 */
mem_region_t * get_mem_list_cursor(vmm_mem_list_t * list, addr_t addr) {
  mem_region_t * prev_region = list->head;

  while (prev_region != NULL) {
    if ( (addr >= prev_region->addr) && 
	 (addr < (prev_region->addr + (prev_region->num_pages * PAGE_SIZE) - 1)) ) {
      return prev_region;
    } else if (addr < prev_region->addr) {
      // If this region is the current head, then this should return NULL
      return prev_region->prev;
    } else if (addr >= (prev_region->addr + (prev_region->num_pages * PAGE_SIZE))) {
      if (prev_region->next) {
	prev_region = prev_region->next;
      } else {
	return prev_region;
      }
    }
  }

  return prev_region;
}



/* Returns the page address of page number 'index' in the memory list
 * If index is out of bounds... returns -1 (an invalid page address)
 */
addr_t get_mem_list_addr(vmm_mem_list_t * list, uint_t index) {
  mem_region_t * reg = list->head;
  uint_t i = index;

  // Memory List overrun
  if (index > list->num_pages - 1) {
    return -1;
  }

  while (i >= 0) {
    if (reg->num_pages <= index) {
      i -= reg->num_pages;
      reg = reg->next;
    } else {
      return reg->addr + (i * PAGE_SIZE);
    }
  }

  return -1;
}


void init_mem_layout(vmm_mem_layout_t * layout) {
  layout->num_pages = 0;
  layout->num_regions = 0;

  layout->head = NULL;
}


void free_mem_layout(vmm_mem_layout_t * layout) {
  layout_region_t * cursor = layout->head;
  layout_region_t * tmp = NULL;

  while(cursor) {
    tmp = cursor;
    cursor = cursor->next;
    VMMFree(tmp);
  }

  VMMFree(layout);

}


/* this function returns a pointer to the location in the layout list that 
 * corresponds to addr.
 * Rules: 
 *     IF addr is in a region, a ptr to that region is returned
 *     IF addr is not in a region, a ptr to the previous region is returned
 *     IF addr is before all regions, returns NULL
 *     IF list is empty, returns NULL
 */
layout_region_t * get_layout_cursor(vmm_mem_layout_t * layout, addr_t addr) {
  layout_region_t * prev_region = layout->head;


  while (prev_region != NULL) {
    if ( (addr >= prev_region->addr) && 
	 (addr < (prev_region->addr + (prev_region->num_pages * PAGE_SIZE))) ) {
      return prev_region;
    } else if (addr < prev_region->addr) {
      // If this region is the current head, then this should return NULL
      return prev_region->prev;
    } else if (addr >= (prev_region->addr + (prev_region->num_pages * PAGE_SIZE))) {
      if (prev_region->next) {
	prev_region = prev_region->next;
      } else {
	return prev_region;
      }
    }
  }

  return prev_region;
}


/* This is slightly different semantically from the mem list, in that we don't allow overlaps
 * we could probably allow overlappig regions of the same type... but I'll let someone else deal with that
 */
int add_mem_range(vmm_mem_layout_t * layout, layout_region_t * region) {
 
  layout_region_t * cursor = get_layout_cursor(layout, region->addr);

  if (cursor == NULL) {
    if (layout->head) {
      if (layout->head->addr < region->addr + (region->num_pages * PAGE_SIZE) - 1) {
	// overlaps not allowed
	return -1;
      }
      layout->head->prev = region;
    }

    region->prev = NULL;
    region->next = layout->head;
    layout->head = region;
    
    layout->num_regions++;
    layout->num_pages += region->num_pages;
  } else if ((region->addr >= cursor->addr) && 
	     (region->addr <= cursor->addr + (cursor->num_pages * PAGE_SIZE) - 1)) {
    // overlaps not allowed
    return -1;
  } else if (region->addr > cursor->addr + (cursor->num_pages * PAGE_SIZE) - 1) {
    // add region to layout
    region->next = cursor->next;
    region->prev = cursor;
    
    if (region->next) {
      region->next->prev = region;
    }
    cursor->next = region;

    layout->num_regions++;
    layout->num_pages += region->num_pages;
  } else {
    return -1;
  }


  return 0;
}





int add_shared_mem_range(vmm_mem_layout_t * layout, addr_t addr, uint_t num_pages, addr_t host_addr) {
  layout_region_t * shared_region = os_hooks->malloc(sizeof(layout_region_t));
  int ret;

  shared_region->next = NULL;
  shared_region->prev = NULL;
  shared_region->addr = addr;
  shared_region->num_pages = num_pages;
  shared_region->type = SHARED;
  shared_region->host_addr = host_addr;

  ret = add_mem_range(layout, shared_region);

  if (ret != 0) {
    VMMFree(shared_region);
  }

  return ret;
}

int add_unmapped_mem_range(vmm_mem_layout_t * layout, addr_t addr, uint_t num_pages) {
  layout_region_t * unmapped_region = os_hooks->malloc(sizeof(layout_region_t));
  int ret;  

  unmapped_region->next = NULL;
  unmapped_region->prev = NULL;
  unmapped_region->addr = addr;
  unmapped_region->num_pages = num_pages;
  unmapped_region->type = UNMAPPED;
  unmapped_region->host_addr = 0;

  ret = add_mem_range(layout, unmapped_region);

  if (ret != 0) {
    VMMFree(unmapped_region);
  }

  return ret;
}

int add_guest_mem_range(vmm_mem_layout_t * layout, addr_t addr, uint_t num_pages) {
  layout_region_t * guest_region = os_hooks->malloc(sizeof(layout_region_t));
  int ret;

  guest_region->next = NULL;
  guest_region->prev = NULL;
  guest_region->addr = addr;
  guest_region->num_pages = num_pages;
  guest_region->type = GUEST;
  guest_region->host_addr = 0;

  ret = add_mem_range(layout, guest_region);
  
  if (ret == 0) {
    layout->num_guest_pages += num_pages;
  } else {
    VMMFree(guest_region);
  }

  return ret;
}



/* Returns the page address of page number 'index' in the memory list
 * If index is out of bounds... returns -1 (an invalid page address)
 */
addr_t get_mem_layout_addr(vmm_mem_layout_t * layout, uint_t index) {
  layout_region_t * reg = layout->head;
  uint_t i = index;

  // Memory List overrun
  if (index > layout->num_pages - 1) {
    return -1;
  }

  while (i >= 0) {
    if (!reg) {
      return -1;
    }

    if (reg->num_pages <= index) {
      i -= reg->num_pages;
      reg = reg->next;
    } else {
      return reg->addr + (i * PAGE_SIZE);
    }
  }

  return -1;
}




void print_mem_list(vmm_mem_list_t * list) {
  mem_region_t * cur = list->head;
  int i = 0;

  PrintDebug("Memory Region List (regions: %d) (pages: %d)\n", list->num_regions, list->num_pages);

  while (cur) {
    PrintDebug("%d: 0x%x - 0x%x\n", i, cur->addr, cur->addr + (cur->num_pages * PAGE_SIZE) - 1);
    cur = cur->next;
    i++;
  }
  PrintDebug("\n");
}



void print_mem_layout(vmm_mem_layout_t * layout) {
  layout_region_t * cur = layout->head;
  int i = 0;

  PrintDebug("Memory Layout (regions: %d) (pages: %d)\n", layout->num_regions, layout->num_pages);

  while (cur) {
    PrintDebug("%d: 0x%x - 0x%x\n", i, cur->addr, cur->addr + (cur->num_pages * PAGE_SIZE) - 1);
    cur = cur->next;
    i++;
  }
  PrintDebug("\n");
}












#ifdef VMM_MEM_TEST


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <geekos/vmm_paging.h>



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


int mem_layout_add_test_1(vmm_mem_layout_t *layout) {

  uint_t offset = 0;

  PrintDebug("\n\nTesting Memory Layout\n");

  init_mem_layout(layout);

  offset = PAGE_SIZE * 6;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 10));
  add_guest_mem_range(layout, offset, 10);
  print_mem_layout(layout);


  offset = PAGE_SIZE * 20;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 1));
  add_guest_mem_range(layout, offset, 1);
  print_mem_layout(layout);


  offset = PAGE_SIZE * 16;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + PAGE_SIZE * 4);
  add_guest_mem_range(layout, offset, 4);
  print_mem_layout(layout);


  offset = PAGE_SIZE * 10;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 30));
  add_guest_mem_range(layout, offset, 30);
  print_mem_layout(layout);


  offset = 0;
  PrintDebug("Adding 0x%x - 0x%x\n", offset, offset + (PAGE_SIZE * 1));
  add_guest_mem_range(layout, offset, 1);
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


  pde_t * pde = generate_guest_page_tables(&layout, &list);
  PrintDebugPageTables(pde);

  return 0;
}
#endif






