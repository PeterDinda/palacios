#ifndef __VMM_STUBS_H
#define __VMM_STUBS_H


#include <geekos/mem.h>
#include <geekos/malloc.h>


void * Allocate_VMM_Pages(int num_pages);
void Free_VMM_Page(void * page);

void * VMM_Malloc(uint_t size);
void VMM_Free(void * addr);



#endif
