#include <geekos/vmm_stubs.h>



void * Allocate_VMM_Pages(int num_pages) {
  return Alloc_Page();
}

void Free_VMM_Page(void * page) {
  Free_Page(page);
}


void * VMM_Malloc(uint_t size) {
  return Malloc((ulong_t) size);
}


void VMM_Free(void * addr) {
  Free(addr);
}
