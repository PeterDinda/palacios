/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <devices/pci.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_debug.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_paging.h>
#include <interfaces/vmm_graphics_console.h>


#ifndef V3_CONFIG_DEBUG_PARAGRAPH
#undef PrintDebug
#define PrintDebug(fmts, args...)
#endif

/*
  A paravirtualized graphics device is represented in a configuration file as:

  <device class="PARAGRAPH" id="pgraph">
     <bus>pci0</bus>
     <mode>mem|gcons_mem|gcons_direct</mode>
  </device>

   
  The purpose of this device is to project a graphics console
  to the guest as a PCI device with a single BAR.  The BAR maps
  the memory of the frame buffer.

  The mode spec means the following:
     mem = create a backing memory within the device  (dummy device)
     gcons_mem = like mem, but render to graphics console (memcpy)
     gcons_direct = use the graphics console as the memory

  the default mode is mem.

*/


#define VENDOR 0xf00f    
#define DEVICE 0xd00f   

#define MAXX   1024
#define MAXY   1024
#define MAXBPP    4


#define MAX_REGION_SIZE (MAXX*MAXY*MAXBPP)

#define DEFAULT_REGION_ADDR 0x80000000 // Right at 2 GB



struct paragraph_state {
  enum {MEM, GCONS_MEM, GCONS_DIRECT} mode;
  //  v3_lock_t          lock;      // my lock
  struct v3_vm_info *vm;        // my VM
  struct vm_device  *pci_bus;   // my PCI bus
  struct vm_device  *dev;       // me as a registered device
  struct pci_device *pci_dev;   // me as a registered PCI device

  void              *my_paddr;  // me as mapped into the guest (page aligned GPA)
  uint64_t           my_size_pages;   // mapped region in pages

  // If the graphics console is used, these store it and
  // the frame buffer spec it is using
  struct v3_frame_buffer_spec  target_spec;
  v3_graphics_console_t        host_cons;
  void                        *host_fb_vaddr;

  // If the local memory is used, this stores it and its
  // size
  void              *mem_paddr;     // my memory address, paddr
  void              *mem_vaddr;    // my memory address, vaddr
  uint64_t           mem_size;     // my memory size in bytes
};

static uint64_t ceil_pages(uint64_t size)
{
  return (size / PAGE_SIZE) + !!(size % PAGE_SIZE);
}

static uint64_t next_pow2(uint64_t size)
{
  uint64_t test;

  for (test=1; test < size; test<<=1) {}

  return test;
}


static int pci_bar_init(int bar_num, uint32_t * dst, void * private_data) 
{
    struct paragraph_state * state = (struct paragraph_state *)private_data;

    // I am going to show up as a PCI_BAR_MEM32 at my_paddr 
    // and going for my_size_pages pages and not prefetchable


    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar init %d 0x%x\n",bar_num, *dst);

    if (bar_num!=0) {
      PrintError(VM_NONE, VCORE_NONE, "paragraph: Strange - Bar Init for bar %d - ignored\n",bar_num);
      return 0;
    }

    if (state->mode==MEM || state->mode==GCONS_MEM) { 
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Adding shadow memory for 0x%p to 0x%p -> 0x%p\n", (void*)(state->my_paddr), (void*)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1), state->mem_paddr);

      if (v3_add_shadow_mem(state->vm, V3_MEM_CORE_ANY,
			    (addr_t)(state->my_paddr), 
			    (addr_t)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1), 
			    (addr_t) (state->mem_paddr))) {
	PrintError(VM_NONE, VCORE_NONE, "paragraph: Cannot add shadow memory for staging\n");
	return -1;
      }
    } else if (state->mode==GCONS_DIRECT) { 
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Adding shadow memory for 0x%p to 0x%p -> 0x%p\n", (void*)(state->my_paddr), (void*)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1), (void*)V3_PAddr(state->host_fb_vaddr));
      // Note the physical contiguous assumption here.... 
      if (v3_add_shadow_mem(state->vm, V3_MEM_CORE_ANY,
			    (addr_t)(state->my_paddr), 
			    (addr_t)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1), 
			    (addr_t)(V3_PAddr(state->host_fb_vaddr)))) {
	PrintError(VM_NONE, VCORE_NONE, "paragraph: Cannot add shadow memory for host fb\n");
	return -1;
      }
    }
    
    *dst = PCI_MEM32_BAR_VAL((addr_t)(state->my_paddr), 0);

    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar init done %d is now 0x%x\n",bar_num, *dst);
    
    return 0;
}
	
static int pci_bar_write(int bar_num, uint32_t * src, void * private_data) 
{
    struct paragraph_state * state = (struct paragraph_state *)private_data;
    struct v3_mem_region *old_reg;

    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar write %d 0x%x\n",bar_num, *src);

    if (*src==(uint32_t)0xffffffff) {
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar write is a size request - complying\n");
      *src = ~(state->my_size_pages*PAGE_SIZE-1);
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Returning size mask as %x\n",*src);
      return 0;
    }

    // This whacky cast should be ok - my_paddr will be <2^32 since this is a mem32 bar
    if (*src==(uint32_t)(uint64_t)(state->my_paddr)) { 
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar write maps to currently mapped address - done.\n");
      return 0;
    } 

    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar write is a remapping\n");

    if (!(old_reg=v3_get_mem_region(state->vm, V3_MEM_CORE_ANY, (addr_t)(state->my_paddr)))) {
      PrintError(VM_NONE,VCORE_NONE, "paragraph: cannot find old region in bar write...\n");
      return -1;
    }

    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Removing old region at 0x%p\n", (void*)(state->my_paddr));

    v3_delete_mem_region(state->vm, old_reg);

    state->my_paddr = (void*)(*src & ~(state->my_size_pages*PAGE_SIZE - 1));

    *src = PCI_MEM32_BAR_VAL((addr_t)(state->my_paddr), 0);

    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Moving paragraph to: start=0x%p, size=%llu\n",
	       state->my_paddr, state->my_size_pages*PAGE_SIZE);


    if (state->mode==MEM || state->mode==GCONS_MEM) { 
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Adding shadow memory for 0x%p to 0x%p -> 0x%p\n", (void*)(state->my_paddr), (void*)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1), state->mem_paddr);
      if (v3_add_shadow_mem(state->vm, V3_MEM_CORE_ANY,
			    (addr_t)(state->my_paddr),
			    (addr_t)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1),
			    (addr_t) (state->mem_paddr))) {
	PrintError(VM_NONE, VCORE_NONE, "paragraph: Cannot add shadow memory for staging\n");
	return -1;
      }
    } else if (state->mode==GCONS_DIRECT) { 
      // Note the physical contiguous assumption here.... 
      PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Adding shadow memory for 0x%p to 0x%p -> 0x%p\n", (void*)(state->my_paddr), (void*)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1), (void*)V3_PAddr(state->host_fb_vaddr));
      if (v3_add_shadow_mem(state->vm, V3_MEM_CORE_ANY,
			    (addr_t)(state->my_paddr),
			    (addr_t)(state->my_paddr+state->my_size_pages*PAGE_SIZE-1),
			    (addr_t)(V3_PAddr(state->host_fb_vaddr)))) {
	PrintError(VM_NONE, VCORE_NONE, "paragraph: Cannot add shadow memory for host fb\n");
	return -1;
      }
    }

    PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Bar write done %d 0x%x\n",bar_num, *src);

    return 0;
}

      
    


static int register_dev(struct paragraph_state *state)  
{
  int i;
  struct v3_pci_bar bars[6];
  uint64_t target_size;
  
  if (!(state->pci_bus)) { 
    PrintError(state->vm, VCORE_NONE, "paragraph: no pci bus!\n");
    return -1;
  }
  
  memset(bars,0,sizeof(struct v3_pci_bar)*6);

  for (i = 0; i < 6; i++) {
    bars[i].type = PCI_BAR_NONE;
  }

  // I will map my memory as a single MEM32 bar

  switch (state->mode) { 
  case MEM:
  case GCONS_MEM:
    target_size = state->mem_size;
    break;
  case GCONS_DIRECT:
      target_size = (uint64_t)state->target_spec.height*state->target_spec.width*state->target_spec.bytes_per_pixel;
    break;
  default:
    PrintError(state->vm, VCORE_NONE, "paragraph: Unknown mode\n");
    return -1;
  }
  
  if (target_size%PAGE_SIZE) { 
    PrintError(VM_NONE, VCORE_NONE, "paragraph: strange, target_size is not an integral number of pages\n");
  }
  
  if (target_size != next_pow2(target_size)) { 
    PrintError(VM_NONE, VCORE_NONE, "paragraph: strange, target_size is not a power of two\n");
  }

  target_size = next_pow2(target_size);

  target_size /= PAGE_SIZE;  // floor(target_size/page_size)

  state->my_paddr = (void*)DEFAULT_REGION_ADDR;
  state->my_size_pages = target_size;

  PrintDebug(VM_NONE, VCORE_NONE, "paragraph: Requesting passthrough bar at %p (%llu pages)\n", state->my_paddr, state->my_size_pages);

  bars[0].type = PCI_BAR_PASSTHROUGH;
  bars[0].private_data = state;
  bars[0].bar_init = pci_bar_init;
  bars[0].bar_write = pci_bar_write;

  state->pci_dev =
    v3_pci_register_device(state->pci_bus, // my bus interace
			   PCI_STD_DEVICE, // I'm a typical device
			   0,              // put me in bus zero
			   -1,             // put me in any slot you want
			   0,              // I am function 0 in that slot
			   "PARAGRAPH",    // My name 
			   bars,           // My bars
			   NULL,           // I don't care about config writes
			   NULL,           // I don't care about config reads
			   NULL,           // I don't care about cmd updates
			   NULL,           // I don't care about rom updates
			   state);         // this is my internal state


  if (!(state->pci_dev)) { 
    PrintError(state->vm, VCORE_NONE, "paragraph: Could not register PCI Device\n");
    return -1;
  }

  // Now lets set up my configuration space
  // to identify as the kind of pci device I am
	
  state->pci_dev->config_header.vendor_id = VENDOR;
  state->pci_dev->config_header.device_id = DEVICE;

  return 0;
}


static int paragraph_free_internal(struct paragraph_state *state)
{
  if (state->host_cons) { 
    v3_graphics_console_close(state->host_cons);
  }

  if (state->mem_paddr) { 
    V3_FreePages(state->mem_paddr,ceil_pages(state->mem_size));
  }

  V3_Free(state);

  return 0;
}

static int paragraph_free(void * private_data) 
{
    struct paragraph_state *state = (struct paragraph_state *)private_data;
    return paragraph_free_internal(state);
}


static int render_callback(v3_graphics_console_t cons,
			   void *priv)
{
  struct paragraph_state *state = (struct paragraph_state *) priv;
  
  PrintDebug(VM_NONE, VCORE_NONE, "paragraph: render due to callback\n");

  switch (state->mode) {
  case MEM:
    PrintError(state->vm, VCORE_NONE, "paragraph: Huh?  render callback when in mem mode?\n");
    return -1;
    break;
  case GCONS_MEM: {
    PrintDebug(state->vm, VCORE_NONE, "paragraph: render callback GCONS_MEM\n");

    void *fb = v3_graphics_console_get_frame_buffer_data_rw(state->host_cons,&(state->target_spec));
    uint64_t target_size = (uint64_t)state->target_spec.height*state->target_spec.width*state->target_spec.bytes_per_pixel;
    
    // must be smaller than the memory we have allocated
    target_size = target_size<state->mem_size ? target_size : state->mem_size;
    
    PrintDebug(state->vm, VCORE_NONE, "paragraph: render - copying %llu bytes from our vaddr 0x%p to fb vaddr 0x%p\n", target_size, state->mem_vaddr, fb);

    
    memcpy(fb,state->mem_vaddr,target_size);

    v3_graphics_console_release_frame_buffer_data_rw(state->host_cons);
    
    return 0;
  }					    
    break;
  case GCONS_DIRECT:
    PrintDebug(state->vm, VCORE_NONE, "paragraph: render callback GCONS_DIRECT\n");
    // nothing to do;
    return 0;
    break;
  default:
    PrintError(state->vm, VCORE_NONE, "paragraph: Huh?  render callback when in unknown mode\n");
    return -1;
    break;
  }

  return 0;
}

static int update_callback(v3_graphics_console_t cons,
			   void *priv)
{
  // Yes, Virginia, there is an update clause
  return 1;
}


static struct v3_device_ops dev_ops = {
    .free = paragraph_free,
};




static int paragraph_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
  struct vm_device         *bus;
  struct paragraph_state  *state;
  char *id;
  char *bus_id;
  char *mode;

  
  if (!(id = v3_cfg_val(cfg,"id"))) {
    PrintError(vm, VCORE_NONE, "paragraph: gnothi seauton!\n");  
    return -1;
  }
  
  if (!(bus_id = v3_cfg_val(cfg,"bus"))) { 
    PrintError(vm, VCORE_NONE, "paragraph: failed because there is no pci bus named\n");
    return -1;
  }
  
  if (!(bus = v3_find_dev(vm,bus_id))) { 
    PrintError(vm, VCORE_NONE, "paragraph: failed because there is no pci bus given\n");
    return -1;
  }
  
  if (!bus) { 
    PrintError(vm, VCORE_NONE, "paragraph: failed because there is no bus named %s\n",bus_id);
    return -1;
  }
 
  state = (struct paragraph_state *) V3_Malloc(sizeof(struct paragraph_state));

  if (!state) {
    PrintError(vm, VCORE_NONE, "paragraph: cannot allocate state\n");
    return -1;
  }

  memset(state, 0, sizeof(struct paragraph_state));

  if (!(mode=v3_cfg_val(cfg,"mode"))) { 
    V3_Print(vm, VCORE_NONE, "paragraph: no mode given, assuming you mean mem\n");
    state->mode=MEM;
  } else {
    if (!strncasecmp(mode,"mem",3)) { 
      state->mode=MEM;
      V3_Print(state->vm, VCORE_NONE, "paragraph: mode set to mem\n");
    } else if (!strncasecmp(mode,"gcons_mem",9)) { 
      state->mode=GCONS_MEM;
      V3_Print(state->vm, VCORE_NONE, "paragraph: mode set to gcons_mem\n");
    } else if (!strncasecmp(mode,"gcons_direct",12)) { 
      state->mode=GCONS_DIRECT;
      V3_Print(state->vm, VCORE_NONE, "paragraph: mode set to gcons_direct\n");
    } else {
      PrintError(state->vm, VCORE_NONE, "paragraph: Unknown mode %s\n",mode);
      paragraph_free_internal(state);
      return -1;
    }
  }
						
  state->vm = vm;
  state->pci_bus = bus;

  if (state->mode==MEM || state->mode==GCONS_MEM) { 
    state->mem_size=MAXX*MAXY*MAXBPP;
    PrintDebug(vm, VCORE_NONE, "paragraph: allocating %llu bytes for local framebuffer\n", state->mem_size);
    state->mem_paddr = V3_AllocPages(ceil_pages(state->mem_size));
    if (!state->mem_paddr) { 
      PrintError(state->vm, VCORE_NONE, "paragraph: Cannot allocate memory for framebuffer\n");
      paragraph_free_internal(state);
      return -1;
    }
    // the following assumes virtual address continuity
    state->mem_vaddr = V3_VAddr(state->mem_paddr);
   
    PrintDebug(vm, VCORE_NONE, "paragraph: staging memory (state->mem) at paddr 0x%p and vaddr 0x%p size=%llu bytes (%llu pages)\n",
	       state->mem_paddr, state->mem_vaddr,
	       state->mem_size, ceil_pages(state->mem_size));
  }
  
  if (state->mode==GCONS_MEM || state->mode==GCONS_DIRECT) { 
    struct v3_frame_buffer_spec req;

    PrintDebug(vm, VCORE_NONE, "paragraph: enabling host frame buffer console (GRAPHICS_CONSOLE)\n");
    memset(&req,0,sizeof(struct v3_frame_buffer_spec));

    req.height=MAXY;
    req.width=MAXX;
    req.bytes_per_pixel=MAXBPP;
    req.bits_per_channel=8;
    req.red_offset=0;
    req.green_offset=1;
    req.blue_offset=2;
    
    state->host_cons = v3_graphics_console_open(vm,&req,&(state->target_spec));
    
    if (!state->host_cons) { 
      PrintError(vm, VCORE_NONE, "paragraph: unable to open host OS's graphics console\n");
      paragraph_free_internal(state);
      return -1;
    }
    
    if (memcmp(&req,&(state->target_spec),sizeof(req))) {
      PrintDebug(vm, VCORE_NONE, "paragraph: warning: target spec differs from requested spec\n");
      PrintDebug(vm, VCORE_NONE, "paragraph: request: %u by %u by %u with %u bpc and r,g,b at %u, %u, %u\n", req.width, req.height, req.bytes_per_pixel, req.bits_per_channel, req.red_offset, req.green_offset, req.blue_offset);
      PrintDebug(vm, VCORE_NONE, "paragraph: response: %u by %u by %u with %u bpc and r,g,b at %u, %u, %u\n", state->target_spec.width, state->target_spec.height, state->target_spec.bytes_per_pixel, state->target_spec.bits_per_channel, state->target_spec.red_offset, state->target_spec.green_offset, state->target_spec.blue_offset);
    }

    if (state->mode==GCONS_DIRECT) { 
      PrintDebug(state->vm, VCORE_NONE, "paragraph: grabbing host console address\n");
      state->host_fb_vaddr = v3_graphics_console_get_frame_buffer_data_rw(state->host_cons,&(state->target_spec));
      if (!state->host_fb_vaddr) { 
	PrintError(state->vm, VCORE_NONE, "paragraph: Unable to acquire host's framebuffer address\n");
	paragraph_free_internal(state);
	return -1;
      }
      v3_graphics_console_release_frame_buffer_data_rw(state->host_cons);
      // we now assume the host FB will not move...
    }

    if (v3_graphics_console_register_render_request(state->host_cons, render_callback, state)!=0) {      PrintError(vm, VCORE_NONE, "paragraph: cannot install render callback\n");
      paragraph_free_internal(state);
      return -1;
    }
    if (v3_graphics_console_register_update_inquire(state->host_cons, update_callback, state)!=0) {
      PrintError(vm, VCORE_NONE, "paragraph: cannot install update inquire callback\n");
      paragraph_free_internal(state);
      return -1;
    }
  }
  
  state->dev = v3_add_device(vm, id, &dev_ops, state);

  if (!(state->dev)) {
    PrintError(state->vm, VCORE_NONE, "paragraph: could not attach device %s\n", id);
    paragraph_free_internal(state);
    return -1;
  }

  if (register_dev(state) !=0 ) { 
    PrintError(state->vm, VCORE_NONE, "paragraph: could not set up device for pci\n");
    paragraph_free_internal(state);
    return -1;
  }

  V3_Print(state->vm, VCORE_NONE, "paragraph: added device id %s\n",id);

  return 0;
}

device_register("PARAGRAPH", paragraph_init)
