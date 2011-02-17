/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Robert Deloatch <rtdeloatch@gmail.com>
 * Copyright (c) 2009, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Robdert Deloatch <rtdeloatch@gmail.com>
 *         Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <devices/cirrus_gfx_card.h>
#include <palacios/vmm.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_instr_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_socket.h>
#include <palacios/vmm_host_events.h>
#include <devices/pci.h>
#include <devices/pci_types.h>

//#include "network_console.h"



/* #ifndef DEBUG_CIRRUS_GFX_CARD
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif */


#define START_ADDR 0xA0000
#define END_ADDR 0xC0000

#define SIZE_OF_REGION (END_ADDR - START_ADDR)

#define PASSTHROUGH 1
#define PORT_OFFSET 0x3B0

struct video_internal {
    addr_t video_memory_pa;
    uint8_t * video_memory;
    
    struct vm_device * pci_bus;
    struct pci_device * pci_dev;

    int client_fd;

    uint_t screen_bottom;
    uint_t ports[44];
    uint8_t reg_3C4[256];
    uint8_t reg_3CE[256];
    uint16_t start_addr_offset;
    uint16_t old_start_addr_offset;
    uint16_t cursor_addr;
    uint8_t reg_3D5[25];
    
};

void video_do_in(uint16_t port, void * src, uint_t length){
#if PASSTHROUGH
    uint_t i;
    switch (length) {
	case 1:
	    ((uint8_t *)src)[0] = v3_inb(port);
	    break;
	case 2:
	    ((uint16_t *)src)[0] = v3_inw(port);
	    break;
	case 4:
	    ((uint32_t *)src)[0] = v3_indw(port);
	    break;
	default:
	    for (i = 0; i < length; i++) { 
		((uint8_t *)src)[i] = v3_inb(port);
	    }
    }
#endif
}

void video_do_out(uint16_t port, void * src, uint_t length){
#if PASSTHROUGH
    uint_t i;
    switch (length) {
	case 1:
	    v3_outb(port,((uint8_t *)src)[0]);
	    break;
	case 2:
	    v3_outw(port,((uint16_t *)src)[0]);
	    break;
	case 4:
	    v3_outdw(port,((uint32_t *)src)[0]);
	    break;
	default:
	    for (i = 0; i < length; i++) { 
		v3_outb(port, ((uint8_t *)src)[i]);
	    }
    }
#endif
}

static int video_write_mem(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct video_internal * data = (struct video_internal *)dev->private_data;
    addr_t write_offset = guest_addr - START_ADDR;
//    uint_t difference = 0x18000;
//    int i = 0;

    /*
    PrintDebug("\n\nInside Video Write Memory.\n\n");
    PrintDebug("Guest address: %p length = %d\n", (void *)guest_addr, length);

    PrintDebug("Write offset: 0x%x\n", (uint_t)write_offset);
    PrintDebug("Video_Memory: ");

    for (i = 0; i < length; i += 2) {
	PrintDebug("%c", ((char *)(V3_VAddr((void *)guest_addr)))[i]);
    }
    */
#if PASSTHROUGH
    memcpy(data->video_memory + write_offset, V3_VAddr((void*)guest_addr), length);
#endif


/*    if (send_update(data->client_fd, data->video_memory + difference, write_offset-difference, data->start_addr_offset, length) == -1) {
	PrintError("Error sending update to client\n");
	return -1;
    }
*/
    // PrintDebug(" Done.\n");
    return length;
}

static int video_read_port(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data ) {
    PrintDebug("Video: Read port 0x%x\n",port);
    video_do_in(port, dest, length);
    return length;
}

static int video_read_port_generic(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data) {
    memset(dest, 0, length);
    video_do_in(port, dest, length);
    return length;
}


static int video_write_port(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data) {
    
      PrintDebug("Video: write port 0x%x...Wrote: ", port);
      uint_t i;
      for(i = 0; i < length; i++){
      PrintDebug("%x", ((uint8_t*)dest)[i]);
      }
      PrintDebug("...Done\n");
    video_do_out(port, dest, length);
    return length;
}

static int video_write_port_store(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data) {
    
      PrintDebug("Entering video_write_port_store...port 0x%x\n", port);
      uint_t i;
      for(i = 0; i < length; i++){
      PrintDebug("%x", ((uint8_t*)dest)[i]);
      }
      PrintDebug("...Done\n"); 
    
    struct video_internal * video_state = (struct video_internal *)priv_data;

    video_state->ports[port - PORT_OFFSET] = 0;
    memcpy(video_state->ports + (port - PORT_OFFSET), dest, length); 
    video_do_out(port, dest, length);

    return length;
}

static int video_write_port_3D5(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = (struct video_internal *)priv_data;
    uint8_t new_start = 0;
    uint_t index = 0;

    PrintDebug("Video: write port 0x%x...Wrote: ", port);
    {
	uint_t i = 0;
	for (i = 0; i < length; i++){
	    PrintDebug("%x", ((uint8_t*)dest)[i]);
	}
	PrintDebug("...Done\n");
    }

    video_state->ports[port - PORT_OFFSET] = 0;

    memcpy(video_state->ports + (port - PORT_OFFSET), dest, length);

    memcpy(&(video_state->reg_3D5[index]), dest, length);

    index = video_state->ports[port - 1 - PORT_OFFSET];

    // JRL: Add length check
    new_start = *((uint8_t *)dest);


    switch (index) {
	case 0x0c:
	    video_state->old_start_addr_offset = video_state->start_addr_offset;

	    video_state->start_addr_offset = (new_start << 8);

	    break;
	case 0x0d: {
	    int diff = 0;

	    video_state->start_addr_offset += new_start;

	    diff = video_state->start_addr_offset - video_state->old_start_addr_offset;
	    diff /= 80;

	    PrintDebug("Scroll lines = %d\n", diff);

//	    send_scroll(video_state->client_fd, diff, video_state->video_memory);

	    break;
	}
	case 0x0E:
	    video_state->cursor_addr = new_start << 8;

	    break;
	case 0x0F: {
	    uint_t x = 0;
	    uint_t y = 0;

	    video_state->cursor_addr += new_start;
	   
	    x = ((video_state->cursor_addr) % 80) + 1;
	    y = (((video_state->cursor_addr) / 80) - ((video_state->start_addr_offset / 80))) + 1;
	    
	    PrintDebug("New Cursor Location; X=%d Y=%d\n", x, y);
	    
//	    send_cursor_update(video_state->client_fd, x, y);
	    break;
	}
	default:
	    break;
    }

    video_do_out(port, dest, length);

    return length;
}


static int video_write_port_3C5(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data ) {
    struct video_internal * video_state = (struct video_internal *)priv_data;
    uint_t index = 0;


    PrintDebug("Entering write_port_3C5....port 0x%x\n", port);
    {
	uint_t i = 0;
	for(i = 0; i < length; i++){
	    PrintDebug("%x", ((uint8_t*)dest)[i]);
	}
	PrintDebug("...Done\n");
    }

    video_state->ports[port - PORT_OFFSET] = 0;
    memcpy(video_state->ports + (port - PORT_OFFSET), dest, length); 

    index = video_state->ports[port - 1 - PORT_OFFSET];

    memcpy(&(video_state->reg_3C4[index]), dest, length);
    video_do_out(port, dest, length);

    return length;
}

static int video_write_port_3CF(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = (struct video_internal *)priv_data;

    PrintDebug("Entering write_port_3CF....port 0x%x\n", port);

    {
	uint_t i = 0;
	for(i = 0; i < length; i++){
	    PrintDebug("%x", ((uint8_t*)dest)[i]);
	}
	PrintDebug("...Done\n");
    }

    video_state->ports[port - PORT_OFFSET] = 0;
    memcpy(video_state->ports + (port - PORT_OFFSET), dest, length); 

    uint_t index = video_state->ports[port - 1 - PORT_OFFSET];
    memcpy(&(video_state->reg_3CE[index]), dest, length);
    video_do_out(port, dest, length);
    return length;
}

static int video_write_port_3D4(struct guest_info * dev, uint16_t port, void * dest, uint_t length, void * priv_data){
    struct video_internal * video_state = (struct video_internal *)priv_data;

#if 1
    if (length == 1) {

	video_state->ports[port - PORT_OFFSET] = 0;
	memcpy(video_state->ports + (port - PORT_OFFSET), dest, length);

    } else if (length == 2) {
	uint16_t new_start = *((uint16_t *)dest);
	uint16_t cursor_start = *((uint16_t *)dest);;

	//Updating the cursor
	if ((cursor_start & 0x00FF) == 0x000E) {

	    cursor_start = (cursor_start & 0xFF00);
	    video_state->cursor_addr = cursor_start;

	} else if ((cursor_start & 0x00FF) == 0x000F) {
	    uint_t x = 0;
	    uint_t y = 0;

	    video_state->cursor_addr += ((cursor_start >> 8) & 0x00FF);

	    x = ((video_state->cursor_addr) % 80) + 1;
	    y = (((video_state->cursor_addr) / 80) - ((video_state->start_addr_offset / 80))) + 1;

	    PrintDebug("New Cursor Location; X=%d Y=%d\n", x, y);

//	    send_cursor_update(video_state->client_fd, x, y);
	}

	//Checking to see if scrolling is needed
	if ((new_start & 0x00FF) == 0x000C) {

	    video_state->old_start_addr_offset = video_state->start_addr_offset;
	    new_start = (new_start & 0xFF00);
	    video_state->start_addr_offset = new_start;

	} else if ((new_start & 0x00FF) == 0x000D) {
	    int diff = 0;

	    video_state->start_addr_offset += ((new_start >> 8) & 0x00FF);

	    diff =  video_state->start_addr_offset - video_state->old_start_addr_offset;
	    diff /= 80;

	    PrintDebug("Scroll lines = %d\n", diff);

//	    send_scroll(video_state->client_fd, diff, video_state->video_memory+0x18000);
	}
    } else {
	// JRL ??
	return -1;
    }
#endif 
    video_do_out(port, dest, length);
    return length;
}

static int video_write_mem_region(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data) {;
    PrintDebug("Video write mem region guest_addr: 0x%p, src: 0x%p, length: %d, Value?= %x\n", (void *)guest_addr, dest, length, *((uint32_t *)V3_VAddr((void *)guest_addr)));
    return length;
}

static int video_read_mem_region(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data){
    PrintDebug("Video:  Within video_read_mem_region\n");
    return length;
}

static int video_write_io_region(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data){
    PrintDebug("Video:  Within video_write_io_region\n");
    return length;
}

static int video_read_io_region(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data){
    PrintDebug("Video:  Within video_read_io_region\n");
    return length;
}

static int cirrus_gfx_card_free(struct vm_device * dev) {
    v3_unhook_mem(dev->vm, V3_MEM_CORE_ANY, START_ADDR);
    return 0;
}

/*
static int cirrus_gfx_card_reset_device(struct vm_device * dev) {
    PrintDebug("Video: reset device\n");
    return 0;
}

static int cirrus_gfx_card_start_device(struct vm_device * dev) {
    PrintDebug("Video: start device\n");
    return 0;
}

static int cirrus_gfx_card_stop_device(struct vm_device * dev) {
    PrintDebug("Video: stop device\n");
    return 0;
}
*/
static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))cirrus_gfx_card_free,
//    .reset = cirrus_gfx_card_reset_device,
//    .start = cirrus_gfx_card_start_device,
//    .stop = cirrus_gfx_card_stop_device,
};

static int cirrus_gfx_card_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg){
    struct video_internal * video_state = (struct video_internal *)V3_Malloc(sizeof(struct video_internal));
//    struct vm_device * pci_bus = v3_find_dev(vm, (char *)cfg_data);
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    char * dev_id = v3_cfg_val(cfg, "ID");

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, video_state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	return -1;
    }

    PrintDebug("video: init_device\n");
    PrintDebug("Num Pages=%d\n", SIZE_OF_REGION / 4096);

    video_state->video_memory_pa = (addr_t)V3_AllocPages(SIZE_OF_REGION / 4096);
    video_state->video_memory = V3_VAddr((void *)video_state->video_memory_pa);

    memset(video_state->video_memory, 0, SIZE_OF_REGION);


    v3_dev_hook_io(dev, 0x3b0, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b1, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b2, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b3, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b4, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b5, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b6, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b7, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b8, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3b9, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3ba, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3bb, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3c0, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3c1, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3c2, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3c3, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3c4, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3c5, &video_read_port, &video_write_port_3C5);
    v3_dev_hook_io(dev, 0x3c6, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3c7, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3c8, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3c9, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3ca, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3cb, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3cc, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3cd, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3ce, &video_read_port, &video_write_port_store);
    v3_dev_hook_io(dev, 0x3cf, &video_read_port, &video_write_port_3CF);
    v3_dev_hook_io(dev, 0x3d0, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d1, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d2, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d3, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d4, &video_read_port, &video_write_port_3D4);
    v3_dev_hook_io(dev, 0x3d5, &video_read_port, &video_write_port_3D5);
    v3_dev_hook_io(dev, 0x3d6, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d7, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d8, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3d9, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3da, &video_read_port_generic, &video_write_port);
    v3_dev_hook_io(dev, 0x3db, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3dc, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3dd, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3de, &video_read_port, &video_write_port);
    v3_dev_hook_io(dev, 0x3df, &video_read_port, &video_write_port);


    PrintDebug("PA of array: %p\n", (void *)video_state->video_memory_pa);


#if PASSTHROUGH
    if (v3_hook_write_mem(vm, V3_MEM_CORE_ANY, START_ADDR, END_ADDR, START_ADDR, &video_write_mem, dev) == -1){
	PrintDebug("\n\nVideo Hook failed.\n\n");
    }
#else
    if (v3_hook_write_mem(vm, V3_MEM_CORE_ANY, START_ADDR, END_ADDR, video_memory_pa, &video_write_mem, dev) == -1){
	PrintDebug("\n\nVideo Hook failed.\n\n");
    }
#endif

    PrintDebug("Video: Getting client connection\n");

    //video_state->client_fd = get_client_connection(vm);

    PrintDebug("Video: Client connection established\n");

    video_state->screen_bottom = 25;

    video_state->pci_bus = pci_bus;

    if (video_state->pci_bus == NULL) {
	PrintError("Could not find PCI device\n");
	return -1;
    } else {
	struct v3_pci_bar bars[6];
	struct pci_device * pci_dev = NULL;

	int i;
	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_MEM32;
	bars[0].num_pages = 8192;
	bars[0].default_base_addr = 0xf0000000;

	bars[0].mem_read = video_read_mem_region;
	bars[0].mem_write = video_write_mem_region;

	bars[1].type = PCI_BAR_MEM32;
	bars[1].num_pages = 1;
	bars[1].default_base_addr = 0xf2000000;
    
	bars[1].mem_read = video_read_io_region;
	bars[1].mem_write = video_write_io_region;
	//-1 Means autoassign
	//                                                     Not sure if STD
	pci_dev = v3_pci_register_device(video_state->pci_bus, PCI_STD_DEVICE, 0,
					 //or0  1st null could be pci_config_update
					 -1, 0, "CIRRUS_GFX_CARD", bars, NULL, NULL,
					 NULL, dev);

	if (pci_dev == NULL) {
	    PrintError("Failed to register VIDEO %d with PCI\n", i);
	    return -1;
	} else{
	    PrintDebug("Registering PCI_VIDEO succeeded\n");
	}
	//Need to set some pci_dev->config_header.vendor_id type variables

	pci_dev->config_header.vendor_id = 0x1013;
	pci_dev->config_header.device_id = 0x00B8;
	pci_dev->config_header.revision = 0x00;

	//If we treat video as a VGA device than below is correct
	//If treated as a VGA compatible controller, which has mapping
	//0xA0000-0xB0000 and I/O addresses 0x3B0-0x3BB than change
	//#define from VGA to 0

	//pci_dev->config_header.class = 0x00;
	//pci_dev->config_header.subclass = 0x01;

	pci_dev->config_header.class = 0x03;
	pci_dev->config_header.subclass = 0x00;
	pci_dev->config_header.prog_if = 0x00;


	//We have a subsystem ID, but optional to provide:  1AF4:1100
	pci_dev->config_header.subsystem_vendor_id = 0x1AF4;
	pci_dev->config_header.subsystem_id = 0x1100;
	//pci_dev->config_header.header_type = 0x00;
	pci_dev->config_header.command = 0x03;
	video_state->pci_dev = pci_dev;
    }

    PrintDebug("Video: init complete\n");
    return 0;
}

device_register("CIRRUS_GFX_CARD", cirrus_gfx_card_init)
