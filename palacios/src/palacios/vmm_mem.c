/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_decoder.h>



void init_shadow_region(struct shadow_region * entry,
			addr_t               guest_addr_start,
			addr_t               guest_addr_end,
			shdw_region_type_t   shdw_region_type)
{
  entry->guest_start = guest_addr_start;
  entry->guest_end = guest_addr_end;
  entry->host_type = shdw_region_type;
  entry->host_addr = 0;
  entry->next = entry->prev = NULL;
}

int add_shadow_region_passthrough( struct guest_info *  guest_info,
				   addr_t               guest_addr_start,
				   addr_t               guest_addr_end,
				   addr_t               host_addr)
{
  struct shadow_region * entry = (struct shadow_region *)V3_Malloc(sizeof(struct shadow_region));

  init_shadow_region(entry, guest_addr_start, guest_addr_end, 
		     SHDW_REGION_ALLOCATED);
  entry->host_addr = host_addr;

  return add_shadow_region(&(guest_info->mem_map), entry);
}

int v3_hook_write_mem(struct guest_info * info, addr_t guest_addr_start, addr_t guest_addr_end, 
		      addr_t host_addr,
		      int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data),
		      void * priv_data) {

  struct shadow_region * entry = (struct shadow_region *)V3_Malloc(sizeof(struct shadow_region));

  init_shadow_region(entry, guest_addr_start, guest_addr_end, 
		     SHDW_REGION_WRITE_HOOK);

  entry->write_hook = write;
  entry->read_hook = NULL;
  entry->host_addr = host_addr;
  entry->priv_data = priv_data;

  return add_shadow_region(&(info->mem_map), entry);  
}

int v3_hook_full_mem(struct guest_info * info, addr_t guest_addr_start, addr_t guest_addr_end,
		     int (*read)(addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		     int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data),
		     void * priv_data) {
  
  struct shadow_region * entry = (struct shadow_region *)V3_Malloc(sizeof(struct shadow_region));

  init_shadow_region(entry, guest_addr_start, guest_addr_end, 
		     SHDW_REGION_FULL_HOOK);

  entry->write_hook = write;
  entry->read_hook = read;
  entry->priv_data = priv_data;

  entry->host_addr = 0;

  return add_shadow_region(&(info->mem_map), entry);
}




int handle_special_page_fault(struct guest_info * info, 
			      addr_t fault_gva, addr_t fault_gpa, 
			      pf_error_t access_info) 
{
 struct shadow_region * reg = get_shadow_region_by_addr(&(info->mem_map), fault_gpa);

  PrintDebug("Handling Special Page Fault\n");

  switch (reg->host_type) {
  case SHDW_REGION_WRITE_HOOK:
    return v3_handle_mem_wr_hook(info, fault_gva, fault_gpa, reg, access_info);
  case SHDW_REGION_FULL_HOOK:
    return v3_handle_mem_full_hook(info, fault_gva, fault_gpa, reg, access_info);
  default:
    return -1;
  }

  return 0;

}

int v3_handle_mem_wr_hook(struct guest_info * info, addr_t guest_va, addr_t guest_pa, 
			  struct shadow_region * reg, pf_error_t access_info) {

  addr_t write_src_addr = 0;

  int write_len = v3_emulate_write_op(info, guest_va, guest_pa, &write_src_addr);

  if (write_len == -1) {
    PrintError("Emulation failure in write hook\n");
    return -1;
  }


  if (reg->write_hook(guest_pa, (void *)write_src_addr, write_len, reg->priv_data) != write_len) {
    PrintError("Memory write hook did not return correct value\n");
    return -1;
  }

  return 0;
}

int v3_handle_mem_full_hook(struct guest_info * info, addr_t guest_va, addr_t guest_pa, 
			    struct shadow_region * reg, pf_error_t access_info) {
  return -1;
}



struct shadow_region * v3_get_shadow_region(struct guest_info * info, addr_t addr) {
  struct shadow_region * reg = info->mem_map.head;

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

  PrintDebug("Adding Shadow Region: (0x%p-0x%p)\n", 
	     (void *)region->guest_start, (void *)region->guest_end);

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


shdw_region_type_t get_shadow_addr_type(struct guest_info * info, addr_t guest_addr) {
  struct shadow_region * reg = get_shadow_region_by_addr(&(info->mem_map), guest_addr);

  if (!reg) {
    return SHDW_REGION_INVALID;
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


shdw_region_type_t lookup_shadow_map_addr(struct shadow_map * map, addr_t guest_addr, addr_t * host_addr) {
  struct shadow_region * reg = get_shadow_region_by_addr(map, guest_addr);

  if (!reg) {
    // No mapping exists
    return SHDW_REGION_INVALID;
  } else {
    switch (reg->host_type) {
    case SHDW_REGION_ALLOCATED:
    case SHDW_REGION_WRITE_HOOK:
     *host_addr = (guest_addr - reg->guest_start) + reg->host_addr;
     return reg->host_type;
    case SHDW_REGION_UNALLOCATED:
    case SHDW_REGION_FULL_HOOK:
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
    PrintDebug("%d:  0x%p - 0x%p -> ", i, 
	       (void *)cur->guest_start, (void *)(cur->guest_end - 1));
    if (cur->host_type == SHDW_REGION_ALLOCATED || 
	cur->host_type == SHDW_REGION_UNALLOCATED) {
      PrintDebug("0x%p", (void *)(cur->host_addr));
    }
    PrintDebug("(%s)\n", shdw_region_type_to_str(cur->host_type));
    cur = cur->next;
    i++;
  }
}


static const uchar_t  SHDW_REGION_INVALID_STR[] = "SHDW_REGION_INVALID";
static const uchar_t  SHDW_REGION_WRITE_HOOK_STR[] = "SHDW_REGION_WRITE_HOOK";
static const uchar_t  SHDW_REGION_FULL_HOOK_STR[] = "SHDW_REGION_FULL_HOOK";
static const uchar_t  SHDW_REGION_ALLOCATED_STR[] = "SHDW_REGION_ALLOCATED";
static const uchar_t  SHDW_REGION_UNALLOCATED_STR[] = "SHDW_REGION_UNALLOCATED";



const uchar_t * shdw_region_type_to_str(shdw_region_type_t type) {
  switch (type) {
  case SHDW_REGION_INVALID: 
    return SHDW_REGION_INVALID_STR;
  case SHDW_REGION_WRITE_HOOK:
    return SHDW_REGION_WRITE_HOOK_STR;
  case SHDW_REGION_FULL_HOOK:
    return SHDW_REGION_FULL_HOOK_STR;
  case SHDW_REGION_ALLOCATED:
    return SHDW_REGION_ALLOCATED_STR;
  case SHDW_REGION_UNALLOCATED:
    return SHDW_REGION_UNALLOCATED_STR;
  default:
    return SHDW_REGION_INVALID_STR;
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






