#ifndef __VMM_STUBS_H
#define __VMM_STUBS_H


#include <geekos/mem.h>
#include <geekos/malloc.h>


void * Allocate_VMM_Pages(int num_pages);
void Free_VMM_Page(void * page);

void * VMM_Malloc(unsigned int size);
void VMM_Free(void * addr);

void * Identity(void *addr);

void Hook_IRQ(int irq, void (*handler)());

#endif
