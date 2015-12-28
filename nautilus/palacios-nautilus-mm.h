#ifndef _PALACIOS_MM_H
#define _PALACIOS_MM_H

#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <nautilus/mm.h>
#include <nautilus/buddy.h>



#include "palacios.h"

#define PALACIOS_PAGE_SIZE 4096

static inline int get_page_bit(int index);
static inline void set_page_bit(int index);
static uintptr_t alloc_contig_pgs(u64 num_pages, u32 alignment);


int init_palacios_nautilus_mm(uint64_t memsize);
int deinit_palacios_nautilus_mm(void);
uintptr_t alloc_palacios_pgs(u64 num_pages, u32 alignment, int node_id, int (*filter_func)(void *paddr, void *filter_state), void *filter_state);
void free_palacios_pgs(uintptr_t base_addr, u64 num_pages);
void free_palacios_pg(uintptr_t base_addr);

#endif
