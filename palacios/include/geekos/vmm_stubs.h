#ifndef __VMM_STUBS_H
#define __VMM_STUBS_H


#include <geekos/mem.h>
#include <geekos/malloc.h>


struct guest_info;

void * Allocate_VMM_Pages(int num_pages);
void Free_VMM_Page(void * page);

void * VMM_Malloc(unsigned int size);
void VMM_Free(void * addr);

void * Identity(void *addr);


int hook_irq_stub(struct guest_info * info, int irq);
int ack_irq(int irq);

void Init_Stubs();
#endif
