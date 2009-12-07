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

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sym_swap.h>


#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif


/* This is the first page that linux writes to the swap area */
/* Taken from Linux */
union swap_header {
    struct {
	char reserved[PAGE_SIZE - 10];
	char magic[10];			/* SWAP-SPACE or SWAPSPACE2 */
    } magic;
    struct {
	char	         	bootbits[1024];	/* Space for disklabel etc. */
	uint32_t		version;
	uint32_t		last_page;
	uint32_t		nr_badpages;
	unsigned char   	sws_uuid[16];
	unsigned char	        sws_volume[16];
	uint32_t                type;           // The index into the swap_map
	uint32_t		padding[116];
	//		uint32_t		padding[117];
	uint32_t		badpages[1];
    } info;
};



struct swap_state {
    int active;

    uint_t swapped_pages;
    uint_t unswapped_pages;


    union swap_header * hdr;

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
    uint32_t pages_in;
    uint32_t pages_out;
#endif


    uint64_t capacity;
    uint8_t * swap_space;
    addr_t swap_base_addr;

    struct guest_info * vm;

    uint8_t usage_map[0]; // This must be the last structure member
};




static inline void set_index_usage(struct swap_state * swap, uint32_t index, int used) {
    int major = index / 8;
    int minor = index % 8;

    if (used) {
	swap->usage_map[major] |= (1 << minor);
    } else {
	swap->usage_map[major] &= ~(1 << minor);
    }
}

static inline int get_index_usage(struct swap_state * swap, uint32_t index) {
    int major = index / 8;
    int minor = index % 8;

    return (swap->usage_map[major] & (1 << minor));
}


static inline uint32_t get_swap_index_from_offset(uint32_t offset) {
    // CAREFUL: The index might be offset by 1, because the first 4K is the header
    return (offset / 4096);
}


/*
  static inline uint32_t get_swap_index(uint32_t offset) {
  // CAREFUL: The index might be offset by 1, because the first 4K is the header
  return (swap_addr - swap->swap_space) / 4096;
  }
*/


static inline void * get_swap_entry(uint32_t pg_index, void * private_data) {
    struct swap_state * swap = (struct swap_state *)private_data;
    void * pg_addr = NULL;
    // int ret = 0;

    //    if ((ret = get_index_usage(swap, pg_index))) {
    // CAREFUL: The index might be offset by 1, because the first 4K is the header
    pg_addr = (void *)(swap->swap_space + (pg_index * 4096));
    //    }

    return pg_addr;
}



static uint64_t swap_get_capacity(void * private_data) {
    struct swap_state * swap = (struct swap_state *)private_data;

    PrintDebug("SymSwap: Getting Capacity %d\n", (uint32_t)(swap->capacity));

    return swap->capacity;
}


static struct v3_swap_ops swap_ops = {
    .get_swap_entry = get_swap_entry,
};



static int swap_read(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct swap_state * swap = (struct swap_state *)private_data;
    uint32_t offset = lba;
    uint32_t length = num_bytes;

  
    /*  
	PrintDebug("SymSwap: Reading %d bytes to %p from %p\n", length,
	buf, (void *)(swap->swap_space + offset));
    */

    if (length % 4096) {
	PrintError("Swapping in length that is not a page multiple\n");
    }

    memcpy(buf, swap->swap_space + offset, length);

    swap->unswapped_pages += (length / 4096);

    if ((swap->active == 1) && (offset != 0)) {
	int i = 0;
	// Notify the shadow paging layer

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
	swap->pages_in += length / 4096;
#endif

	for (i = 0; i < length; i += 4096) {
	    set_index_usage(swap, get_swap_index_from_offset(offset + i), 0);
	    v3_swap_in_notify(swap->vm, get_swap_index_from_offset(offset + i), swap->hdr->info.type);
	}
    }

    return 0;
}




static int swap_write(uint8_t * buf,  uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct swap_state * swap = (struct swap_state *)private_data;
    uint32_t offset = lba;
    uint32_t length = num_bytes;

    /*
      PrintDebug("SymSwap: Writing %d bytes to %p from %p\n", length, 
      (void *)(swap->swap_space + offset), buf);
    */

    if (length % 4096) {
	PrintError("Swapping out length that is not a page multiple\n");
    }

    if ((swap->active == 0) && (offset == 0)) {
	// This is the swap header page

	if (length != 4096) {
	    PrintError("Initializing Swap space by not writing page multiples. This sucks...\n");
	    return -1;
	}

	swap->active = 1;

	PrintDebug("Swap Type=%d (magic=%s)\n", swap->hdr->info.type, swap->hdr->magic.magic);

	if (v3_register_swap_disk(swap->vm, swap->hdr->info.type, &swap_ops, swap) == -1) {
	    PrintError("Error registering symbiotic swap disk\n");
	    return -1;
	}
    }

    memcpy(swap->swap_space + offset, buf, length);

    swap->swapped_pages += (length / 4096);

    if ((swap->active == 1) && (offset != 0)) {
	int i = 0;

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
	swap->pages_out += length / 4096;
#endif

	for (i = 0; i < length; i += 4096) {
	    set_index_usage(swap, get_swap_index_from_offset(offset + i), 1);
	}
    }

    return 0;
}


static int swap_free(struct vm_device * dev) {
    return -1;
}


static struct v3_dev_blk_ops blk_ops = {
    .read = swap_read, 
    .write = swap_write, 
    .get_capacity = swap_get_capacity,
};



static struct v3_device_ops dev_ops = {
    .free = swap_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
static void telemetry_cb(struct guest_info * info, void * private_data, char * hdr) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct swap_state * swap = (struct swap_state *)(dev->private_data);

    V3_Print("%sSwap Device:\n", hdr);
    V3_Print("%s\tPages Swapped in=%d\n", hdr, swap->pages_in);
    V3_Print("%s\tPages Swapped out=%d\n", hdr, swap->pages_out);

}
#endif





static int swap_init(struct guest_info * vm, v3_cfg_tree_t * cfg) {
    struct swap_state * swap = NULL;
    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    uint32_t capacity = atoi(v3_cfg_val(cfg, "size")) * 1024 * 1024;
    char * name = v3_cfg_val(cfg, "name");

    if (!frontend_cfg) {
	PrintError("Initializing sym swap without a frontend device\n");
	return -1;
    }

    PrintDebug("Creating Swap Device (size=%dMB)\n", capacity / (1024 * 1024));

    swap = (struct swap_state *)V3_Malloc(sizeof(struct swap_state) + ((capacity / 4096) / 8));

    swap->vm = vm;

    swap->capacity = capacity;

    swap->swapped_pages = 0;
    swap->unswapped_pages = 0;

    swap->active = 0;
    swap->hdr = (union swap_header *)swap;

    swap->swap_base_addr = (addr_t)V3_AllocPages(swap->capacity / 4096);
    swap->swap_space = (uint8_t *)V3_VAddr((void *)(swap->swap_base_addr));
    memset(swap->swap_space, 0, swap->capacity);

    memset(swap->usage_map, 0, ((swap->capacity / 4096) / 8));

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, swap);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }

    if (v3_dev_connect_blk(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &blk_ops, frontend_cfg, swap) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   name, v3_cfg_val(frontend_cfg, "tag"));
	return -1;
    }

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
    if (vm->enable_telemetry) {
	v3_add_telemetry_cb(vm, telemetry_cb, dev);
    }
#endif

    return 0;
}

device_register("SYM_SWAP", swap_init)
