#ifndef _memcheck
#define _memcheck

typedef enum {
  PALACIOS_KMALLOC, 
  PALACIOS_VMALLOC,
  PALACIOS_PAGE_ALLOC
} palacios_memcheck_memtype_t;

#ifdef V3_CONFIG_DEBUG_MEM_ALLOC

// Maxmimum number of simultaneous allocations to handle
#define NUM_ALLOCS        8192

//
// The following macros are used
// in the stub functions to call back to the memory
// checker - if memory allocation hecking is not enabled, these 
// turn into nothing
//
#define MEMCHECK_INIT() palacios_memcheck_init()
#define MEMCHECK_KMALLOC(ptr,size) palacios_memcheck_alloc(ptr,size,PALACIOS_KMALLOC)
#define MEMCHECK_KFREE(ptr)  palacios_memcheck_free(ptr,0,PALACIOS_KMALLOC)
#define MEMCHECK_VMALLOC(ptr,size) palacios_memcheck_alloc(ptr,size,PALACIOS_VMALLOC)
#define MEMCHECK_VFREE(ptr)  palacios_memcheck_free(ptr,0,PALACIOS_VMALLOC)
#define MEMCHECK_ALLOC_PAGES(ptr,size) palacios_memcheck_alloc(ptr,size,PALACIOS_PAGE_ALLOC)
#define MEMCHECK_FREE_PAGES(ptr,size)  palacios_memcheck_free(ptr,size,PALACIOS_PAGE_ALLOC)
#define MEMCHECK_DEINIT() palacios_memcheck_deinit()

void palacios_memcheck_init(void);
void palacios_memcheck_alloc(void *ptr, unsigned long size, palacios_memcheck_memtype_t type);
void palacios_memcheck_free(void *ptr, unsigned long size, palacios_memcheck_memtype_t type);
void palacios_memcheck_deinit(void);

#else

//
// The following is what happens when lock checking is not on
//
#define MEMCHECK_INIT()
#define MEMCHECK_KMALLOC(ptr,size) 
#define MEMCHECK_KFREE(ptr)  
#define MEMCHECK_VMALLOC(ptr,size) 
#define MEMCHECK_VFREE(ptr)  
#define MEMCHECK_ALLOC_PAGES(ptr,size) 
#define MEMCHECK_FREE_PAGES(ptr,size)  
#define MEMCHECK_DEINIT()

#endif


#endif
