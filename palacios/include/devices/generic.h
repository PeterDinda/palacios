/* Northwestern University */
/* (c) 2008, Peter Dinda <pdinda@northwestern.edu> */

#ifndef __GENERIC_H
#define __GENERIC_H

#include <palacios/vm_dev.h>

//
// The generic device simply hooks ranges of ports, addresses, and irqs
// if they are not already hooked
//
// for each hooked port, it simply executes reads and writes and the same physical port,
// for each hooked memory range, it simply executes reads and writes on the same
//    physical memory addresses
// for each hooked irq, it simply injects the irq into the VM
//
// These operations are also logged to serial (optionaly)
//
// If you attach a generic device *last*, you can capture all ops that are not
// already hooked, and capture a log of VM activity with respect to them.
//
// The effects of using the generic device should be identical to 
// doing passthrough I/O, but with logging, and, of course, slower
//


#define GENERIC_PRINT_AND_PASSTHROUGH 0
#define GENERIC_PRINT_AND_IGNORE      1

// A port range is low..high, inclusive, third value is one of the above
typedef uint_t generic_port_range_type[3];
// A memory range is low..high, inclusive, flags
typedef void *generic_address_range_type[3];
// An interrupt ory map range is low..high, inclusive, flags
typedef uint_t generic_irq_range_type[3];

// The lists given are null terminated
struct vm_device *create_generic(generic_port_range_type    port_ranges[], 
				 generic_address_range_type addess_ranges[],
				 generic_irq_range_type     irq_ranges[]);  

#endif
