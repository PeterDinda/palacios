#ifndef __VM_GUEST_MEM_H
#define __VM_GUEST_MEM_H

#include <geekos/vm_guest.h>
#include <geekos/vmm_mem.h>


int guest_va_to_guest_pa(guest_info_t * guest_info, addr_t guest_va, addr_t * guest_pa);
int guest_pa_to_guest_va(guest_info_t * guest_info, addr_t guest_pa, addr_t * guest_va);
int guest_va_to_host_va(guest_info_t * guest_info, addr_t guest_va, addr_t * host_va);
int guest_pa_to_host_pa(guest_info_t * guest_info, addr_t guest_pa, addr_t * host_pa);
int guest_pa_to_host_va(guest_info_t * guest_info, addr_t guest_pa, addr_t * host_va);

int host_va_to_guest_pa(guest_info_t * guest_info, addr_t host_va, addr_t * guest_pa);
int host_pa_to_guest_va(guest_info_t * guest_info, addr_t host_pa, addr_t * guest_va);

int host_va_to_host_pa(addr_t host_va, addr_t * host_pa);
int host_pa_to_host_va(addr_t host_pa, addr_t * host_va);



int read_guest_va_memory(guest_info_t * guest_info, addr_t guest_va, int count, char * dest);
int read_guest_pa_memory(guest_info_t * guest_info, addr_t guest_pa, int count, char * dest);



#endif
