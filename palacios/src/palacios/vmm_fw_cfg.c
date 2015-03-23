/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Alexander Kudryavtsev <alexk@ispras.ru>
 *         Implementation of FW_CFG interface 
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *         NUMA modifications
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_fw_cfg.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>


/*
  This subsystem of Palacios interacts with the SEABIOS in order to 
  create highly customized configurations for the guest.  Currently,
  the primary purpose of such configuration is to pass a NUMA configuration
  to the guest via ACPI.  Currently, we are able to create NUMA domains,
  map regions of guest physical addresses to them, and map vcores to them.
  Additionally, these virtual NUMA domains are then mapped to physical
  (host) NUMA domains.   Other elements of Palacios handle vcore to 
  physical core mapping, as well as guest memory allocation such that
  the needed physical NUMA domain mapping is correct.

  The following describes how the XML configuration of a virtual NUMA guest
  works.

  <mem_layout vnodes=n>  (How many numa domains the guest will see)
      (guest physical addresses x to y-1 are numa domain i and 
       numa domain i is mapped to host numa domain j)
     <region vnode=i start_addr=x end_addr=y node=j> 
     ...
  <mem_layout>
  
  For example, a 4 virtual domain guest mapped toa 2 domain host:
  
  <mem_layout vnodes="4">
    <region vnode="0" start_addr="0x00000000" end_addr="0x10000000" node="0" />
    <region vnode="1" start_addr="0x10000000" end_addr="0x20000000" node="1" />
    <region vnode="2" start_addr="0x20000000" end_addr="0x30000000" node="0" />
    <region vnode="3" start_addr="0x30000000" end_addr="0x40000000" node="1" />
  </mem_layout>
	
  You also need to map the virtual cores to the domains, which is 
  done with the <cores> tag.  This usually also indicates which physical core
  the virtual core maps to, so that the NUMA topology the guest sees has 
  performance characteristics that make sense.
       
  <cores count=m>  (How many virtual cores we have)
     <core vnode=i target_cpu=q> (vcore 0 maps to virtual numa zone i and pcore q)
     <core vnode=j target_cpu=r> (vcore 1 maps to virtual numa zone j and pcore r)
      ...
   <cores>

   For example, here are 8 virtual cores maped across our numa domains, pairwise

   <cores count="8">
      <core target_cpu="1" vnode="0"/>
      <core target_cpu="2" vnode="0"/>
      <core target_cpu="3" vnode="1"/>
      <core target_cpu="4" vnode="1"/>
      <core target_cpu="5" vnode="2"/>
      <core target_cpu="6" vnode="2"/>
      <core target_cpu="7" vnode="3"/>
      <core target_cpu="8" vnode="3"/>
   </cores>

*/



#define FW_CFG_CTL_PORT     0x510
#define FW_CFG_DATA_PORT    0x511

#define FW_CFG_SIGNATURE        0x00
#define FW_CFG_ID               0x01
#define FW_CFG_UUID             0x02
#define FW_CFG_RAM_SIZE         0x03
#define FW_CFG_NOGRAPHIC        0x04
#define FW_CFG_NB_CPUS          0x05
#define FW_CFG_MACHINE_ID       0x06
#define FW_CFG_KERNEL_ADDR      0x07
#define FW_CFG_KERNEL_SIZE      0x08
#define FW_CFG_KERNEL_CMDLINE   0x09
#define FW_CFG_INITRD_ADDR      0x0a
#define FW_CFG_INITRD_SIZE      0x0b
#define FW_CFG_BOOT_DEVICE      0x0c
#define FW_CFG_NUMA             0x0d
#define FW_CFG_BOOT_MENU        0x0e
#define FW_CFG_MAX_CPUS         0x0f
#define FW_CFG_KERNEL_ENTRY     0x10
#define FW_CFG_KERNEL_DATA      0x11
#define FW_CFG_INITRD_DATA      0x12
#define FW_CFG_CMDLINE_ADDR     0x13
#define FW_CFG_CMDLINE_SIZE     0x14
#define FW_CFG_CMDLINE_DATA     0x15
#define FW_CFG_SETUP_ADDR       0x16
#define FW_CFG_SETUP_SIZE       0x17
#define FW_CFG_SETUP_DATA       0x18
#define FW_CFG_FILE_DIR         0x19

#define FW_CFG_WRITE_CHANNEL    0x4000
#define FW_CFG_ARCH_LOCAL       0x8000
#define FW_CFG_ENTRY_MASK       ~(FW_CFG_WRITE_CHANNEL | FW_CFG_ARCH_LOCAL)

#define FW_CFG_ACPI_TABLES (FW_CFG_ARCH_LOCAL + 0)
#define FW_CFG_SMBIOS_ENTRIES (FW_CFG_ARCH_LOCAL + 1)
#define FW_CFG_IRQ0_OVERRIDE (FW_CFG_ARCH_LOCAL + 2)
#define FW_CFG_E820_TABLE (FW_CFG_ARCH_LOCAL + 3)
#define FW_CFG_HPET (FW_CFG_ARCH_LOCAL + 4)

#define FW_CFG_INVALID          0xffff




/*
enum v3_e820_types {
    E820_TYPE_FREE      = 1,
    E820_TYPE_RESV      = 2,
    E820_TYPE_ACPI_RECL = 3,
    E820_TYPE_ACPI_NVS  = 4,
    E820_TYPE_BAD       = 5
};

#define E820_MAX_COUNT 128
struct e820_entry_packed {
    uint64_t addr;
    uint64_t size;
    uint32_t type;
} __attribute__((packed));

struct e820_table {
    uint32_t count;
    struct e820_entry_packed entry[E820_MAX_COUNT];
} __attribute__((packed)) __attribute((__aligned__(4)));

*/

//
// Internal version assumes data is allocated
//
static int fw_cfg_add_bytes_internal(struct v3_fw_cfg_state * cfg_state, uint16_t key, uint8_t * data, uint32_t len)
{
    int arch = !!(key & FW_CFG_ARCH_LOCAL);
    // JRL: Well this is demented... Its basically generating a 1 or 0 from a mask operation

    key &= FW_CFG_ENTRY_MASK;

    if (key >= FW_CFG_MAX_ENTRY) {
        return 0;
    }

    cfg_state->entries[arch][key].data = data;
    cfg_state->entries[arch][key].len = len;

    return 1;
}

//
// General purpose version will allocate a temp
//
//
static int fw_cfg_add_bytes(struct v3_fw_cfg_state * cfg_state, uint16_t key, uint8_t * data, uint32_t len)
{
    // must make a copy of the data so that the deinit function will work correctly...

    uint16_t * copy = NULL;

    copy = V3_Malloc(len);
    if (!copy) { 
	PrintError(VM_NONE,VCORE_NONE,"Failed to allocate temp\n");
	return 0;
    }
    memcpy(copy,data,len);
    return fw_cfg_add_bytes_internal(cfg_state, key, (uint8_t *)copy, sizeof(uint16_t));
}

static int fw_cfg_add_i16(struct v3_fw_cfg_state * cfg_state, uint16_t key, uint16_t value)
{
    uint16_t * copy = NULL;

    copy = V3_Malloc(sizeof(uint16_t));
    if (!copy) { 
	PrintError(VM_NONE,VCORE_NONE,"Failed to allocate temp\n");
	return 0;
    }
    *copy = value;
    return fw_cfg_add_bytes_internal(cfg_state, key, (uint8_t *)copy, sizeof(uint16_t));
}

static int fw_cfg_add_i32(struct v3_fw_cfg_state * cfg_state, uint16_t key, uint32_t value)
{
    uint32_t * copy = NULL;

    copy = V3_Malloc(sizeof(uint32_t));
    if (!copy) { 
	PrintError(VM_NONE,VCORE_NONE,"Failed to allocate temp\n");
	return 0;
    }
    *copy = value;
    return fw_cfg_add_bytes_internal(cfg_state, key, (uint8_t *)copy, sizeof(uint32_t));
}

static int fw_cfg_add_i64(struct v3_fw_cfg_state * cfg_state, uint16_t key, uint64_t value)
{
    uint64_t * copy = NULL;

    copy = V3_Malloc(sizeof(uint64_t));
    if (!copy) { 
	PrintError(VM_NONE,VCORE_NONE,"Failed to allocate temp\n");
	return 0;
    }
    *copy = value;
    return fw_cfg_add_bytes_internal(cfg_state, key, (uint8_t *)copy, sizeof(uint64_t));
}

static int fw_cfg_ctl_read(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    return length;
}

static int fw_cfg_ctl_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    V3_ASSERT(core->vm_info, core, length == 2);

    struct v3_fw_cfg_state * cfg_state = (struct v3_fw_cfg_state *)priv_data;
    uint16_t key = *(uint16_t *)src;
    int ret = 0;

    cfg_state->cur_offset = 0;

    if ((key & FW_CFG_ENTRY_MASK) >= FW_CFG_MAX_ENTRY) {
        cfg_state->cur_entry = FW_CFG_INVALID;
        ret = 0;
    } else {
        cfg_state->cur_entry = key;
        ret = 1;
    }

    return length;
}


static int fw_cfg_data_read(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    V3_ASSERT(core->vm_info, core, length == 1);

    struct v3_fw_cfg_state * cfg_state = (struct v3_fw_cfg_state *)priv_data;
    int arch = !!(cfg_state->cur_entry & FW_CFG_ARCH_LOCAL);
    struct v3_fw_cfg_entry * cfg_entry = &cfg_state->entries[arch][cfg_state->cur_entry & FW_CFG_ENTRY_MASK];
    uint8_t ret;

    if ( (cfg_state->cur_entry == FW_CFG_INVALID) || 
	 (cfg_entry->data == NULL) || 
	 (cfg_state->cur_offset >= cfg_entry->len)) {

        ret = 0;
    } else {
        ret = cfg_entry->data[cfg_state->cur_offset++];
    }

    *(uint8_t *)src = ret;

    return length;
}

static int fw_cfg_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    V3_ASSERT(core->vm_info, core, length == 1);

    struct v3_fw_cfg_state * cfg_state = (struct v3_fw_cfg_state *)priv_data;
    int arch = !!(cfg_state->cur_entry & FW_CFG_ARCH_LOCAL);
    struct v3_fw_cfg_entry * cfg_entry = &cfg_state->entries[arch][cfg_state->cur_entry & FW_CFG_ENTRY_MASK];

    if ( (cfg_state->cur_entry & FW_CFG_WRITE_CHANNEL) && 
	 (cfg_entry->callback != NULL) &&
	 (cfg_state->cur_offset < cfg_entry->len)) {

        cfg_entry->data[cfg_state->cur_offset++] = *(uint8_t *)src;

        if (cfg_state->cur_offset == cfg_entry->len) {
            cfg_entry->callback(cfg_entry->callback_opaque, cfg_entry->data);
            cfg_state->cur_offset = 0;
        }
    }
    return length;
}

/*
static struct e820_table * e820_populate(struct v3_vm_info * vm) {
    struct v3_e820_entry * entry = NULL;
    struct e820_table * e820 = NULL;
    int i = 0;

    if (vm->mem_map.e820_count > E820_MAX_COUNT) {
        PrintError(vm, VCORE_NONE,"Too much E820 table entries! (max is %d)\n", E820_MAX_COUNT);
        return NULL;
    }

    e820 = V3_Malloc(sizeof(struct e820_table));

    if (e820 == NULL) {
        PrintError(vm, VCORE_NONE, "Out of memory!\n");
        return NULL;
    }

    e820->count = vm->mem_map.e820_count;

    list_for_each_entry(entry, &vm->mem_map.e820_list, list) {
        e820->entry[i].addr = e->addr;
        e820->entry[i].size = e->size;
        e820->entry[i].type = e->type;
        ++i;
    }

    return e820;
}
*/

void v3_fw_cfg_deinit(struct v3_vm_info *vm) {
    struct v3_fw_cfg_state * cfg_state = &(vm->fw_cfg_state);
    int i, j;

    for (i = 0; i < 2; ++i) {
        for (j = 0; j < FW_CFG_MAX_ENTRY; ++j) {
            if (cfg_state->entries[i][j].data != NULL)
                V3_Free(cfg_state->entries[i][j].data);
        }
    }

    v3_unhook_io_port(vm, FW_CFG_CTL_PORT);
    v3_unhook_io_port(vm, FW_CFG_DATA_PORT);

}

int v3_fw_cfg_init(struct v3_vm_info * vm) {



    struct v3_fw_cfg_state * cfg_state = &(vm->fw_cfg_state);
    int ret = 0;
    uint64_t mem_size = vm->mem_size;
    uint32_t num_cores = vm->num_cores;

#ifdef V3_CONFIG_HVM
    mem_size = v3_get_hvm_ros_memsize(vm);
    num_cores = v3_get_hvm_ros_cores(vm);
#endif

    
    // Be paranoid about starting this as all "unallocated"
    memset(cfg_state,0,sizeof(struct v3_fw_cfg_state));

#ifndef V3_CONFIG_SEABIOS
    V3_Print(vm,VCORE_NONE,"Warning: Configuring SEABIOS firmware, but SEABIOS is not being used in this build of Palacios.  Configuration will be dormant.\n");
#endif

    /* 
       struct e820_table * e820 = e820_populate(vm);

       if (e820 == NULL) {
        PrintError(vm, VCORE_NONE, "Failed to populate E820 for FW interface!\n");
        return -1;
	}

    */


    ret |= v3_hook_io_port(vm, FW_CFG_CTL_PORT, fw_cfg_ctl_read, &fw_cfg_ctl_write, cfg_state);
    ret |= v3_hook_io_port(vm, FW_CFG_DATA_PORT, fw_cfg_data_read, &fw_cfg_data_write, cfg_state);

    if (ret != 0) {
	//  V3_Free(e820);
        PrintError(vm, VCORE_NONE, "Failed to hook FW CFG ports!\n");
	v3_fw_cfg_deinit(vm);
        return -1;
    }

    fw_cfg_add_bytes(cfg_state, FW_CFG_SIGNATURE, (uint8_t *)"QEMU", 4);
    //fw_cfg_add_bytes(cfg_state, FW_CFG_UUID, qemu_uuid, 16);
    fw_cfg_add_i16(cfg_state, FW_CFG_NOGRAPHIC, /*(uint16_t)(display_type == DT_NOGRAPHIC)*/ 0);
    fw_cfg_add_i16(cfg_state, FW_CFG_NB_CPUS, (uint16_t)num_cores);
    fw_cfg_add_i16(cfg_state, FW_CFG_MAX_CPUS, (uint16_t)num_cores);
    fw_cfg_add_i16(cfg_state, FW_CFG_BOOT_MENU, (uint16_t)1);
    //fw_cfg_bootsplash(cfg_state);

    fw_cfg_add_i32(cfg_state, FW_CFG_ID, 1);
    fw_cfg_add_i64(cfg_state, FW_CFG_RAM_SIZE, mem_size / (1024 * 1024));

    //fw_cfg_add_bytes(cfg_state, FW_CFG_ACPI_TABLES, (uint8_t *)acpi_tables,
    //       acpi_tables_len);

    fw_cfg_add_i32(cfg_state, FW_CFG_IRQ0_OVERRIDE, 1);

    /*
      smbios_table = smbios_get_table(&smbios_len);
    
      if (smbios_table) {
           fw_cfg_add_bytes(cfg_state, FW_CFG_SMBIOS_ENTRIES,
                            smbios_table, smbios_len);
      }

      fw_cfg_add_bytes(cfg_state, FW_CFG_E820_TABLE, (uint8_t *)e820,
                     sizeof(struct e820_table));

      fw_cfg_add_bytes(cfg_state, FW_CFG_HPET, (uint8_t *)&hpet_cfg,
                     sizeof(struct hpet_fw_config));
    */



    /* NUMA layout */
    {
	v3_cfg_tree_t * layout_cfg = v3_cfg_subtree(vm->cfg_data->cfg, "mem_layout");
	char * num_nodes_str = v3_cfg_val(layout_cfg, "vnodes");
	int num_nodes = 0;
	
	/* locations in fw_cfg NUMA array for each info region. */
	int node_offset = 0;
	int core_offset = 1;
	int mem_offset = 1 + num_cores;
	
	if (num_nodes_str) {
	    num_nodes = atoi(num_nodes_str);
	}

	if (num_nodes > 0) {
	    uint64_t * numa_fw_cfg = NULL;
	    int i = 0;

	    // Allocate the global NUMA configuration array
	    numa_fw_cfg = V3_Malloc((1 + num_cores + num_nodes) * sizeof(uint64_t));

	    if (numa_fw_cfg == NULL) {
		PrintError(vm, VCORE_NONE, "Could not allocate fw_cfg NUMA config space\n");
		v3_fw_cfg_deinit(vm);
		return -1;
	    }

	    memset(numa_fw_cfg, 0, (1 + num_cores + num_nodes) * sizeof(uint64_t));

	    // First 8 bytes is the number of NUMA zones
	    numa_fw_cfg[node_offset] = num_nodes;
	    
	    
	    // Next region is array of core->node mappings
	    for (i = 0; i < num_cores; i++) {
		char * vnode_str = v3_cfg_val(vm->cores[i].core_cfg_data, "vnode");
		
		if (vnode_str == NULL) {
		    // if no cpu was specified then NUMA layout is randomized, and we're screwed...
		    numa_fw_cfg[core_offset + i] = 0;
		} else {
		    numa_fw_cfg[core_offset + i] = (uint64_t)atoi(vnode_str);
		}
	    }



	    /* Final region is an array of node->mem_size mappings
	     * this assumes that memory is assigned to NUMA nodes in consecutive AND contiguous blocks
	     * NO INTERLEAVING ALLOWED
	     * e.g. node 0 points to the first x bytes of memory, node 1 points to the next y bytes, etc
	     *     The array only stores the x,y,... values, indexed by the node ID
	     *     We should probably fix this, but that will require modifications to SEABIOS
	     * 
	     *
	     * For now we will assume that the xml data is set accordingly, so we will just walk through the mem regions specified there.
	     *   NOTE: This will overwrite configurations if multiple xml regions are defined for each node
	     */

	    {
		v3_cfg_tree_t * region_desc = v3_cfg_subtree(layout_cfg, "region");
		
		while (region_desc) {
		    char * start_addr_str = v3_cfg_val(region_desc, "start_addr");
		    char * end_addr_str = v3_cfg_val(region_desc, "end_addr");
		    char * vnode_id_str = v3_cfg_val(region_desc, "vnode");
		    
		    addr_t start_addr = 0;
		    addr_t end_addr = 0;
		    int vnode_id = 0;

		    if ((!start_addr_str) || (!end_addr_str) || (!vnode_id_str)) {
			PrintError(vm, VCORE_NONE, "Invalid memory layout in configuration\n");
			v3_fw_cfg_deinit(vm);
			return -1;
		    }
		    
		    start_addr = atox(start_addr_str);
		    end_addr = atox(end_addr_str);
		    vnode_id = atoi(vnode_id_str);
		    
		    numa_fw_cfg[mem_offset + vnode_id] = end_addr - start_addr;

		    region_desc = v3_cfg_next_branch(region_desc);
		}
	    }


	    /* Print the NUMA mapping being passed in */
	    {
		uint64_t region_start = 0;
		
		V3_Print(vm, VCORE_NONE, "NUMA CONFIG: (nodes=%llu)\n", numa_fw_cfg[0]);
	
		for (i = 0; i < num_cores; i++) {
		    V3_Print(vm, VCORE_NONE, "\tCore %d -> Node %llu\n", i, numa_fw_cfg[core_offset + i]);
		}
	
		for (i = 0; i < num_nodes; i++) {
		    V3_Print(vm, VCORE_NONE, "\tMem (%p - %p) -> Node %d\n", (void *)region_start, 
			     (void *)numa_fw_cfg[mem_offset + i], i);
		    
		    region_start += numa_fw_cfg[mem_offset + i];
		}
	    }


	    // Register the NUMA cfg array with the FW_CFG interface
	    fw_cfg_add_bytes_internal(cfg_state, FW_CFG_NUMA, (uint8_t *)numa_fw_cfg,
				      (1 + num_cores + num_nodes) * sizeof(uint64_t));

	}
    }


    return 0;
}




/* E820 code for HVM enabled bochs bios:  */
#if 0
/* E820 location in HVM virtual address space. Taken from VMXASSIST. */
#define HVM_E820_PAGE        0x00090000
#define HVM_E820_NR_OFFSET   0x000001E8
#define HVM_E820_OFFSET      0x000002D0
    // Copy E820 to BIOS. See rombios.c, copy_e820_table function.
    addr_t e820_ptr = (addr_t)V3_VAddr((void *)(vm->mem_map.base_region.host_addr + HVM_E820_PAGE));

    *(uint16_t *)(e820_ptr + HVM_E820_NR_OFFSET) = e820->count;
    memcpy((void *)(e820_ptr + HVM_E820_OFFSET), &e820->entry[0], sizeof(e820->entry[0]) * e820->count);
    V3_Free(e820);

    return 0;
#endif
