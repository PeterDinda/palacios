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

#include <palacios/vmm_io.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>



#ifndef V3_CONFIG_DEBUG_IO
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static int free_hook(struct v3_vm_info * vm, struct v3_io_hook * hook);

static int default_write(struct guest_info * core, uint16_t port, void *src, uint_t length, void * priv_data);
static int default_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data);


void v3_init_io_map(struct v3_vm_info * vm) {

  vm->io_map.map.rb_node = NULL;
  vm->io_map.arch_data = NULL;
  vm->io_map.update_map = NULL;

}

int v3_deinit_io_map(struct v3_vm_info * vm) {
    struct rb_node * node = v3_rb_first(&(vm->io_map.map));
    struct v3_io_hook * hook = NULL;
    struct rb_node * tmp_node = NULL;

    while (node) {
	hook = rb_entry(node, struct v3_io_hook, tree_node);
	tmp_node = node;
	node = v3_rb_next(node);

	free_hook(vm, hook);
    }

    return 0;
}




static inline struct v3_io_hook * __insert_io_hook(struct v3_vm_info * vm, struct v3_io_hook * hook) {
  struct rb_node ** p = &(vm->io_map.map.rb_node);
  struct rb_node * parent = NULL;
  struct v3_io_hook * tmp_hook = NULL;

  while (*p) {
    parent = *p;
    tmp_hook = rb_entry(parent, struct v3_io_hook, tree_node);

    if (hook->port < tmp_hook->port) {
      p = &(*p)->rb_left;
    } else if (hook->port > tmp_hook->port) {
      p = &(*p)->rb_right;
    } else {
      return tmp_hook;
    }
  }

  rb_link_node(&(hook->tree_node), parent, p);

  return NULL;
}


static inline struct v3_io_hook * insert_io_hook(struct v3_vm_info * vm, struct v3_io_hook * hook) {
  struct v3_io_hook * ret;

  if ((ret = __insert_io_hook(vm, hook))) {
    return ret;
  }

  v3_rb_insert_color(&(hook->tree_node), &(vm->io_map.map));

  return NULL;
}


struct v3_io_hook * v3_get_io_hook(struct v3_vm_info * vm, uint16_t port) {
  struct rb_node * n = vm->io_map.map.rb_node;
  struct v3_io_hook * hook = NULL;

  while (n) {
    hook = rb_entry(n, struct v3_io_hook, tree_node);
    
    if (port < hook->port) {
      n = n->rb_left;
    } else if (port > hook->port) {
      n = n->rb_right;
    } else {
      return hook;
    }
  }

  return NULL;
}





int v3_hook_io_port(struct v3_vm_info * vm, uint16_t port, 
		    int (*read)(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data),
		    int (*write)(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data), 
		    void * priv_data) {
  struct v3_io_hook * io_hook = (struct v3_io_hook *)V3_Malloc(sizeof(struct v3_io_hook));

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

  io_hook->priv_data = priv_data;

  if (insert_io_hook(vm, io_hook)) {
      PrintError("Could not insert IO hook for port %u (0x%x)\n", port, port);
      V3_Free(io_hook);
      return -1;
  }

  if (vm->io_map.update_map) {
      if (vm->io_map.update_map(vm, port, 
				  ((read == NULL) ? 0 : 1), 
				  ((write == NULL) ? 0 : 1)) == -1) {
	  PrintError("Could not update IO map for port %u (0x%x)\n", port, port);
	  V3_Free(io_hook);
	  return -1;
      }
  }

  return 0;
}


static int free_hook(struct v3_vm_info * vm, struct v3_io_hook * hook) {
    v3_rb_erase(&(hook->tree_node), &(vm->io_map.map));

    if (vm->io_map.update_map) {
	// set the arch map to default (this should be 1, 1)
	vm->io_map.update_map(vm, hook->port, 0, 0);
    }

    V3_Free(hook);

    return 0;
}

int v3_unhook_io_port(struct v3_vm_info * vm, uint16_t port) {
    struct v3_io_hook * hook = v3_get_io_hook(vm, port);

    if (hook == NULL) {
	PrintError("Could not find port to unhook %u (0x%x)\n", port, port);
	return -1;
    }

    free_hook(vm, hook);

    return 0;
}







void v3_refresh_io_map(struct v3_vm_info * vm) {
    struct v3_io_map * io_map = &(vm->io_map);
    struct v3_io_hook * tmp = NULL;
    
    if (io_map->update_map == NULL) {
	PrintError("Trying to refresh an io map with no backend\n");
	return;
    }

    v3_rb_for_each_entry(tmp, &(io_map->map), tree_node) {
	io_map->update_map(vm, tmp->port, 
			   ((tmp->read == NULL) ? 0 : 1), 
			   ((tmp->write == NULL) ? 0 : 1));
    }

}



void v3_print_io_map(struct v3_vm_info * vm) {
    struct v3_io_map * io_map = &(vm->io_map);
    struct v3_io_hook * tmp_hook = NULL;

    V3_Print("VMM IO Map\n");

    v3_rb_for_each_entry(tmp_hook, &(io_map->map), tree_node) {
	V3_Print("IO Port: %hu (Read=%p) (Write=%p)\n", 
		 tmp_hook->port, 
		 (void *)(tmp_hook->read), (void *)(tmp_hook->write));
    }
}



/*
 * Write a byte to an I/O port.
 */
void v3_outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__ (
	"outb %b0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a byte from an I/O port.
 */
uint8_t v3_inb(uint16_t port) {
    uint8_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}

/*
 * Write a word to an I/O port.
 */
void v3_outw(uint16_t port, uint16_t value) {
    __asm__ __volatile__ (
	"outw %w0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a word from an I/O port.
 */
uint16_t v3_inw(uint16_t port) {
    uint16_t value;

    __asm__ __volatile__ (
	"inw %w1, %w0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}

/*
 * Write a double word to an I/O port.
 */
void v3_outdw(uint16_t port, uint_t value) {
    __asm__ __volatile__ (
	"outl %0, %1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a double word from an I/O port.
 */
uint_t v3_indw(uint16_t port) {
    uint_t value;

    __asm__ __volatile__ (
	"inl %1, %0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}




/* FIX ME */
static int default_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    if (length == 1) {
	v3_outb(port, *(uint8_t *)src);
    } else if (length == 2) {
	v3_outw(port, *(uint16_t *)src);
    } else if (length == 4) {
	v3_outdw(port, *(uint32_t *)src);
    } else {
	return -1;
    }
    
    return length;
}

static int default_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data) {
    if (length == 1) {
	*(uint8_t *)dst = v3_inb(port);
    } else if (length == 2) {
	*(uint16_t *)dst = v3_inw(port);
    } else if (length == 4) {
	*(uint32_t *)dst = v3_indw(port);
    } else {
	return -1;
    }

    return length;
}
