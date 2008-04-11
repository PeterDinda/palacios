#ifndef __VMM_IO_H
#define __VMM_IO_H

#include <palacios/vmm_types.h>

#include <palacios/vmm_util.h>

// FOREACH_IO_HOOK(vmm_io_map_t * io_map, vmm_io_hook_t * io_hook)
#define FOREACH_IO_HOOK(io_map, io_hook) for (io_hook = (io_map).head; io_hook != NULL; io_hook = (io_hook)->next)


typedef struct vmm_io_hook {
  ushort_t port;

  // Reads data into the IO port (IN, INS)
  int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data);

  // Writes data from the IO port (OUT, OUTS)
  int (*write)(ushort_t port, void * src, uint_t length, void * priv_data);

  void * priv_data;

  struct vmm_io_hook * next;
  struct vmm_io_hook * prev;

} vmm_io_hook_t;


typedef struct vmm_io_map {
  uint_t num_ports;


  vmm_io_hook_t * head;

} vmm_io_map_t;


void add_io_hook(vmm_io_map_t * io_map, vmm_io_hook_t * io_hook);



vmm_io_hook_t * get_io_hook(vmm_io_map_t * io_map, uint_t port);


/* External API */
void hook_io_port(vmm_io_map_t * io_map, uint_t port, 
		  int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data),
		  int (*write)(ushort_t port, void * src, uint_t length, void * priv_data), 
		  void * priv_data );

void init_vmm_io_map(vmm_io_map_t * io_map);

void PrintDebugIOMap(vmm_io_map_t * io_map);


#endif
