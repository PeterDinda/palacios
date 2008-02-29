#ifndef __VMM_STUBS_H
#define __VMM_STUBS_H


#include <geekos/mem.h>


void * Allocate_VMM_Pages(int num_pages);
void Free_VMM_Page(void * page);




#endif
