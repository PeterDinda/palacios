/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_IO_H
#define __VMM_IO_H



#include <palacios/vmm_types.h>
#include <palacios/vmm_util.h>


struct guest_info;

int v3_unhook_io_port(struct guest_info * info, uint_t port);


/* External API */
int v3_hook_io_port(struct guest_info * info, uint_t port, 
		    int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data),
		    int (*write)(ushort_t port, void * src, uint_t length, void * priv_data), 
		    void * priv_data);




#ifdef __V3VEE__


struct vmm_io_hook;

struct vmm_io_map {
  uint_t num_ports;
  struct vmm_io_hook * head;

};


void init_vmm_io_map(struct guest_info * info);

// FOREACH_IO_HOOK(vmm_io_map_t * io_map, vmm_io_hook_t * io_hook)
#define FOREACH_IO_HOOK(io_map, io_hook) for (io_hook = (io_map).head; io_hook != NULL; io_hook = (io_hook)->next)


struct vmm_io_hook {
  ushort_t port;

  // Reads data into the IO port (IN, INS)
  int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data);

  // Writes data from the IO port (OUT, OUTS)
  int (*write)(ushort_t port, void * src, uint_t length, void * priv_data);

  void * priv_data;

  struct vmm_io_hook * next;
  struct vmm_io_hook * prev;

};


struct vmm_io_hook * v3_get_io_hook(struct vmm_io_map * io_map, uint_t port);


void PrintDebugIOMap(struct vmm_io_map * io_map);


#endif // !__V3VEE__





#endif
