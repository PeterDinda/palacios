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




#ifndef DEBUG_IO
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static int default_write(ushort_t port, void *src, uint_t length, void * priv_data);
static int default_read(ushort_t port, void * dst, uint_t length, void * priv_data);


void v3_init_io_map(struct guest_info * info) {
  info->io_map.rb_node = NULL;
}





static inline struct v3_io_hook * __insert_io_hook(struct guest_info * info, struct v3_io_hook * hook) {
  struct rb_node ** p = &(info->io_map.rb_node);
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


static inline struct v3_io_hook * insert_io_hook(struct guest_info * info, struct v3_io_hook * hook) {
  struct v3_io_hook * ret;

  if ((ret = __insert_io_hook(info, hook))) {
    return ret;
  }

  v3_rb_insert_color(&(hook->tree_node), &(info->io_map));

  return NULL;
}


struct v3_io_hook * v3_get_io_hook(struct guest_info * info, uint_t port) {
  struct rb_node * n = info->io_map.rb_node;
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






int v3_hook_io_port(struct guest_info * info, uint_t port, 
		    int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data),
		    int (*write)(ushort_t port, void * src, uint_t length, void * priv_data), 
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

  if (insert_io_hook(info, io_hook)) {
    V3_Free(io_hook);
    return -1;
  }

  return 0;
}

int v3_unhook_io_port(struct guest_info * info, uint_t port) {
  struct v3_io_hook * hook = v3_get_io_hook(info, port);

  if (hook == NULL) {
    return -1;
  }

  v3_rb_erase(&(hook->tree_node), &(info->io_map));

  V3_Free(hook);

  return 0;
}






void v3_print_io_map(struct guest_info * info) {
  struct v3_io_hook * tmp_hook = NULL;
  struct rb_node * node = v3_rb_first(&(info->io_map));

  PrintDebug("VMM IO Map (Entries=%d)\n", io_map->num_ports);

  do {
    tmp_hook = rb_entry(node, struct v3_io_hook, tree_node);

    PrintDebug("IO Port: %hu (Read=%p) (Write=%p)\n", 
	       tmp_hook->port, 
	       (void *)(tmp_hook->read), (void *)(tmp_hook->write));
  } while ((node = v3_rb_next(node)));
}



/*
 * Write a byte to an I/O port.
 */
void v3_outb(ushort_t port, uchar_t value) {
    __asm__ __volatile__ (
	"outb %b0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a byte from an I/O port.
 */
uchar_t v3_inb(ushort_t port) {
    uchar_t value;

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
void v3_outw(ushort_t port, ushort_t value) {
    __asm__ __volatile__ (
	"outw %w0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a word from an I/O port.
 */
ushort_t v3_inw(ushort_t port) {
    ushort_t value;

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
void v3_outdw(ushort_t port, uint_t value) {
    __asm__ __volatile__ (
	"outl %0, %1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a double word from an I/O port.
 */
uint_t v3_indw(ushort_t port) {
    uint_t value;

    __asm__ __volatile__ (
	"inl %1, %0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
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

static int default_read(ushort_t port, void * dst, uint_t length, void * priv_data) {

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
