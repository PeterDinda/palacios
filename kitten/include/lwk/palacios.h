/* Copyright (c) 2007,2008 Sandia National Laboratories */

#ifndef _LWK_PALACIOS_H_
#define _LWK_PALACIOS_H_

#ifdef CONFIG_V3VEE

#include <lwk/types.h>
#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>



extern int
RunVMM( void );

extern struct v3_os_hooks v3vee_os_hooks;

/**** 
 * 
 * stubs called by geekos....
 * 
 ***/
extern void Init_Stubs(struct guest_info * info);
void send_key_to_vmm(unsigned char status, unsigned char scancode);
void send_mouse_to_vmm(unsigned char packet[3]);
void send_tick_to_vmm(unsigned int period_us);


/* Location of the ROM Bios and VGA Bios used by palacios */
extern uint8_t rombios_start, rombios_end;
extern uint8_t vgabios_start, vgabios_end;
extern paddr_t initrd_start, initrd_end;


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
