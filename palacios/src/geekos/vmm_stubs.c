#include <geekos/vmm_stubs.h>
#include <geekos/serial.h>


void * Allocate_VMM_Pages(int num_pages) {
  void * start_page = Alloc_Page();
  SerialPrint("Allocating Page: %x (%d of %d)\n",start_page, 1, num_pages); 
  int i = 1;

  while (i < num_pages) {
    void * tmp_page = Alloc_Page();
    SerialPrint("Allocating Page: %x (%d of %d)\n",tmp_page, i+1, num_pages); 
    
    if (tmp_page != start_page + (PAGE_SIZE * i)) {
      //we have to start over...;
      while (i >= 0) {
	Free_Page(start_page + (PAGE_SIZE * i));
	i--;
      }
      start_page = Alloc_Page();
      SerialPrint("Allocating Page: %x (%d of %d)\n",start_page, 1, num_pages);
      i = 1;
      continue;
    }
    i++;
  }

  return start_page;
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
