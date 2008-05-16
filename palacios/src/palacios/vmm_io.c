#include <palacios/vmm_io.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm.h>

extern struct vmm_os_hooks * os_hooks;





void init_vmm_io_map(vmm_io_map_t * io_map) {
  io_map->num_ports = 0;
  io_map->head = NULL;
}



int add_io_hook(vmm_io_map_t * io_map, vmm_io_hook_t * io_hook) {

  if (!(io_map->head)) {
    io_map->head = io_hook;
    io_map->num_ports = 1;
    return 0;
  } else if (io_map->head->port > io_hook->port) {
    io_hook->next = io_map->head;

    io_map->head->prev = io_hook;
    io_map->head = io_hook;
    io_map->num_ports++;

    return 0;
  } else {
    vmm_io_hook_t * tmp_hook = io_map->head;
    
    while ((tmp_hook->next)  && 
	   (tmp_hook->next->port <= io_hook->port)) {
	tmp_hook = tmp_hook->next;
    }
    
    if (tmp_hook->port == io_hook->port) {
      //tmp_hook->read = io_hook->read;
      //tmp_hook->write = io_hook->write;
      //V3_Free(io_hook);
      return -1;
    } else {
      io_hook->prev = tmp_hook;
      io_hook->next = tmp_hook->next;

      if (tmp_hook->next) {
	tmp_hook->next->prev = io_hook;
      }

      tmp_hook->next = io_hook;

      io_map->num_ports++;
      return 0;
    }
  }
  return -1;
}

int remove_io_hook(vmm_io_map_t * io_map, vmm_io_hook_t * io_hook) {
  if (io_map->head == io_hook) {
    io_map->head = io_hook->next;
  } else if (io_hook->prev) {
    io_hook->prev->next = io_hook->next;
  } else {
    return -1;
    // data corruption failure
  }
  
  if (io_hook->next) {
    io_hook->next->prev = io_hook->prev;
  }

  io_map->num_ports--;

  return 0;
}



/* FIX ME */
static int default_write(ushort_t port, void *src, uint_t length, void * priv_data) {
  /*
    
  if (length == 1) {
  __asm__ __volatile__ (
  "outb %b0, %w1"
  :
  : "a" (*dst), "Nd" (port)
  );
  } else if (length == 2) {
  __asm__ __volatile__ (
  "outw %b0, %w1"
  :
  : "a" (*dst), "Nd" (port)
  );
  } else if (length == 4) {
  __asm__ __volatile__ (
  "outw %b0, %w1"
  :
  : "a" (*dst), "Nd" (port)
  );
  }
  */
  return 0;
}

static int default_read(ushort_t port, void * dst, uint_t length, void * priv_data)
{

  /*    
	uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
  */

  return 0;
}

int hook_io_port(vmm_io_map_t * io_map, uint_t port, 
		 int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data),
		 int (*write)(ushort_t port, void * src, uint_t length, void * priv_data), 
		 void * priv_data) {
  vmm_io_hook_t * io_hook = os_hooks->malloc(sizeof(vmm_io_hook_t));

  io_hook->port = port;

  if (!read) {
    io_hook->read = &default_read;
  } else {
    io_hook->read = read;
  }

  if (!write) {
    io_hook->write = &default_write;
  } else {
    io_hook->write = write;
  }

  io_hook->next = NULL;
  io_hook->prev = NULL;

  io_hook->priv_data = priv_data;

  if (add_io_hook(io_map, io_hook) != 0) {
    V3_Free(io_hook);
    return -1;
  }

  return 0;
}

int unhook_io_port(vmm_io_map_t * io_map, uint_t port) {
  vmm_io_hook_t * hook = get_io_hook(io_map, port);

  if (hook == NULL) {
    return -1;
  }

  remove_io_hook(io_map, hook);
  return 0;
}


vmm_io_hook_t * get_io_hook(vmm_io_map_t * io_map, uint_t port) {
  vmm_io_hook_t * tmp_hook;
  FOREACH_IO_HOOK(*io_map, tmp_hook) {
    if (tmp_hook->port == port) {
      return tmp_hook;
    }
  }
  return NULL;
}



void PrintDebugIOMap(vmm_io_map_t * io_map) {
  vmm_io_hook_t * iter = io_map->head;

  PrintDebug("VMM IO Map (Entries=%d)\n", io_map->num_ports);

  while (iter) {
    PrintDebug("IO Port: %hu (Read=%x) (Write=%x)\n", iter->port, iter->read, iter->write);
  }
}
