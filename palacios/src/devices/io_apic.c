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

#include <devices/io_apic.h>
#include <palacios/vmm.h>

#include <devices/apic.h>

/*
#ifndef DEBUG_IO_APIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif
*/


#define IO_APIC_BASE_ADDR 0xfec00000


#define IOAPIC_ID_REG 0x00
#define IOAPIC_VER_REG 0x01
#define IOAPIC_ARB_REG 0x02

#define IOAPIC_REDIR_BASE_REG 0x10

#define REDIR_LO_MASK  ~0x00005000

struct ioapic_reg_sel {
  union {
    uint32_t val;
    struct {
      uint_t reg_addr     : 8;
      uint_t rsvd         : 24;
    } __attribute__((packed));
  } __attribute__((packed));
} __attribute__((packed));

struct ioapic_id_reg {
  union {
    uint32_t val;
    struct {
      uint_t rsvd1      : 24;
      uint_t id         : 4;
      uint_t rsvd2      : 4;
    } __attribute__((packed));
  } __attribute__((packed));
} __attribute__((packed));

struct ioapic_ver_reg {
  union {
    uint32_t val;
    struct {
      uint_t version    : 8;
      uint_t rsvd1      : 8;
      uint_t max_redir  : 8;
      uint_t rsvd2      : 8;
    } __attribute__((packed));
  } __attribute__((packed));
} __attribute__((packed));


struct ioapic_arb_reg {
  union {
    uint32_t val;
    struct {
      uint_t rsvd1      : 24;
      uint_t max_redir  : 4;
      uint_t rsvd2      : 4;
    } __attribute__((packed));
  } __attribute__((packed));
} __attribute__((packed));


struct redir_tbl_entry {
  union {
    uint64_t val;
    struct {
      uint32_t lo;
      uint32_t hi;
    } __attribute__((packed));
    struct {
      uint_t vec        : 8;

#define FIXED        0x0
#define LOWEST_PRIOR 0x1
#define SMI          0x2
#define NMI          0x4
#define INIT         0x5
#define EXTINT       0x7
      uint_t del_mode   : 3;

#define PHSYICAL_DST_MODE 0
#define LOGICAL_DST_MODE 1
      uint_t dst_mode   : 1;
      uint_t del_status : 1;

#define HIGH_ACTIVE 0
#define LOW_ACTIVE 1
      uint_t intr_pol   : 1;
      uint_t rem_irr    : 1;
      uint_t trig_mode  : 1;
      uint_t mask       : 1;
      uint64_t rsvd     : 39;
      uint_t dst_field  : 8;
    } __attribute__((packed));
  } __attribute__((packed));
} __attribute__((packed));



struct io_apic_state {
  addr_t base_addr;

  uint32_t index_reg;

  struct ioapic_id_reg ioapic_id;
  struct ioapic_ver_reg ioapic_ver;
  struct ioapic_arb_reg ioapic_arb_id;
  
  struct redir_tbl_entry redir_tbl[24];

  // This is a temporary method of communication between the IOAPIC and the LAPIC
  struct vm_device * apic;
  
};


static void init_ioapic_state(struct io_apic_state * ioapic) {
  int i = 0;
  ioapic->base_addr = IO_APIC_BASE_ADDR;
  ioapic->index_reg = 0;

  ioapic->ioapic_id.val = 0x00000000;
  ioapic->ioapic_ver.val = 0x00170011;
  ioapic->ioapic_arb_id.val = 0x00000000;

  for (i = 0; i < 24; i++) {
    ioapic->redir_tbl[i].val = 0x0001000000000000LL;
    // Mask all interrupts until they are enabled....
    ioapic->redir_tbl[i].mask = 1;
  }
}


static int ioapic_read(addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
  struct vm_device * dev = (struct vm_device *)priv_data;
  struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);
  uint32_t reg_tgt = guest_addr - ioapic->base_addr;
  uint32_t * op_val = (uint32_t *)dst;

  PrintDebug("IOAPIC Read at %p\n", (void *)guest_addr);

  if (reg_tgt == 0x00) {
    *op_val = ioapic->index_reg;
  } else if (reg_tgt == 0x10) {
    // IOWIN register
    switch (ioapic->index_reg) {
    case IOAPIC_ID_REG:
      *op_val = ioapic->ioapic_id.val;
      break;
    case IOAPIC_VER_REG:
      *op_val = ioapic->ioapic_ver.val;
      break;
    case IOAPIC_ARB_REG:
      *op_val = ioapic->ioapic_arb_id.val;
      break;
    default:
      {
	uint_t redir_index = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) & 0xfffffffe;
	uint_t hi_val = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) % 1;

	if (redir_index > 0x3f) {
	  PrintError("Invalid redirection table entry %x\n", (uint32_t)redir_index);
	  return -1;
	}
	if (hi_val) {
	  *op_val = ioapic->redir_tbl[redir_index].hi;
	} else {
	  *op_val = ioapic->redir_tbl[redir_index].lo;
	}
      }
    }
  }

  return length;
}


static int ioapic_write(addr_t guest_addr, void * src, uint_t length, void * priv_data) {
  struct vm_device * dev = (struct vm_device *)priv_data;
  struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);
  uint32_t reg_tgt = guest_addr - ioapic->base_addr;
  uint32_t op_val = *(uint32_t *)src;

  PrintDebug("IOAPIC Write at %p (val = %d)\n", (void *)guest_addr, *(uint32_t *)src);

  if (reg_tgt == 0x00) {
    ioapic->index_reg = op_val;
  } else if (reg_tgt == 0x10) {
    // IOWIN register
    switch (ioapic->index_reg) {
    case IOAPIC_ID_REG:
      ioapic->ioapic_id.val = op_val;
      break;
    case IOAPIC_VER_REG:
      // GPF/PageFault/Ignore?
      PrintError("Writing to read only IOAPIC register\n");
      return -1;
    case IOAPIC_ARB_REG:
      ioapic->ioapic_arb_id.val = op_val;
      break;
    default:
      {
	uint_t redir_index = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) & 0xfffffffe;
	uint_t hi_val = (ioapic->index_reg - IOAPIC_REDIR_BASE_REG) % 1;




	if (redir_index > 0x3f) {
	  PrintError("Invalid redirection table entry %x\n", (uint32_t)redir_index);
	  return -1;
	}
	if (hi_val) {
	  PrintDebug("Writing to hi of pin %d\n", redir_index);
	  ioapic->redir_tbl[redir_index].hi = op_val;
	} else {
	  PrintDebug("Writing to lo of pin %d\n", redir_index);
	  op_val &= REDIR_LO_MASK;
	  ioapic->redir_tbl[redir_index].lo &= ~REDIR_LO_MASK;
	  ioapic->redir_tbl[redir_index].lo |= op_val;
	}
      }
    }
  }

  return length;
}

/* Interrupt controller functions */
static int ioapic_intr_pending(void * private_data) {
  return 0;
}


static int ioapic_get_intr_number(void * private_data) {
  return 0;
}

static int ioapic_begin_irq(void * private_data, int irq) {
  return 0;
}

static int ioapic_raise_irq(void * private_data, int irq) {
  struct vm_device * dev = (struct vm_device *)private_data;
  struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);  
  struct redir_tbl_entry * irq_entry = NULL;

  if (irq > 24) {
    PrintError("IRQ out of range of IO APIC\n");
    return -1;
  }

  irq_entry = &(ioapic->redir_tbl[irq]);

  if (irq_entry->mask == 0) {
    PrintDebug("IOAPIC Signalling APIC to raise INTR %d\n", irq_entry->vec);
    v3_apic_raise_intr(ioapic->apic, irq_entry->vec);
  }

  return 0;
}

/* I don't know if we can do anything here.... */
static int ioapic_lower_irq(void * private_data, int irq) {
  return 0;
}

static struct intr_ctrl_ops intr_ops = {
  .intr_pending = ioapic_intr_pending,
  .get_intr_number = ioapic_get_intr_number,
  .raise_intr = ioapic_raise_irq,
  .begin_irq = ioapic_begin_irq,
  .lower_intr = ioapic_lower_irq, 
};


static int io_apic_init(struct vm_device * dev) {
  struct guest_info * info = dev->vm;
  struct io_apic_state * ioapic = (struct io_apic_state *)(dev->private_data);

  v3_register_intr_controller(dev->vm, &intr_ops, dev);
  init_ioapic_state(ioapic);

  v3_hook_full_mem(info, ioapic->base_addr, ioapic->base_addr + PAGE_SIZE_4KB, 
		   ioapic_read, ioapic_write, dev);
  
  return 0;
}


static int io_apic_deinit(struct vm_device * dev) {
  //  struct guest_info * info = dev->vm;

  return 0;
}


static struct vm_device_ops dev_ops = {
  .init = io_apic_init, 
  .deinit = io_apic_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};



struct vm_device * v3_create_io_apic(struct vm_device * apic) {
  PrintDebug("Creating IO APIC\n");

  struct io_apic_state * ioapic = (struct io_apic_state *)V3_Malloc(sizeof(struct io_apic_state));
  ioapic->apic = apic;

  struct vm_device * device = v3_create_device("IOAPIC", &dev_ops, ioapic);

  return device;
}
