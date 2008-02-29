#include <geekos/vmm_stubs.h>



void * Allocate_VMM_Pages(int num_pages) {
  return Alloc_Page();
}




void Free_VMM_Page(void * page) {
  Free_Page(page);
}
