/* Palacios memory manager 
 * (c) Jack Lange, 2010
 */

#ifndef PALACIOS_MM_H
#define PALACIOS_MM_H



uintptr_t alloc_palacios_pgs(u64 num_pages, u32 alignment, int node_id, int (*filter_func)(void *paddr, void *filter_state), void *filter_state);
void free_palacios_pg(uintptr_t base_addr);
void free_palacios_pgs(uintptr_t base_addr, u64 num_pages);

uintptr_t get_palacios_base_addr(void);
u64 get_palacios_num_pages(void);


int add_palacios_memory(struct v3_mem_region *reg);
int remove_palacios_memory(struct v3_mem_region *reg);
int palacios_init_mm( void );
int palacios_deinit_mm( void );



#endif
