/* Copyright (c) 2007,2008 Sandia National Laboratories */

#ifndef _LWK_PALACIOS_H_
#define _LWK_PALACIOS_H_

#ifdef CONFIG_V3VEE

#include <palacios/vmm.h>

extern uint8_t rombios_start, rombios_end;
extern uint8_t vgabios_start, vgabios_end;


/*
 * OS Hooks required to interface with the V3VEE library
 */
extern void
v3vee_print_config(
	const char * fmt,
	...
) __attribute__((format(printf,1,2)));


extern void
v3vee_print_debug(
	const char * fmt,
	...
) __attribute__((format(printf,1,2)));


extern void
v3vee_print_trace(
	const char * fmt,
	...
) __attribute__((format(printf,1,2)));


extern void *
v3vee_allocate_pages( int num_pages );


extern void
v3vee_free_page( void * page );


extern void *
v3vee_malloc( unsigned int size );


extern void
v3vee_free( void * addr );


extern void *
v3vee_paddr_to_vaddr( void * addr );


extern void *
v3vee_vaddr_to_paddr( void * addr );


extern int
v3vee_hook_interrupt(
	struct guest_info *	vm,
	unsigned int		irq
);


extern int
v3vee_ack_irq(
	int			irq
);


unsigned int
v3vee_get_cpu_khz( void );


void
v3vee_start_kernel_thread( void );


void
v3vee_yield_cpu( void );


#endif // CONFIG_V3VEE

#endif
