/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_hypercall.h>

#include <palacios/vmm_xml.h>

#include <palacios/vm_guest_mem.h>

#include <palacios/vmm_debug.h>


/*

  In a Pal file:

  <files> 
    <file id="multibootelf" filename="multibootelf.o" />
  </files>

  <multiboot enable="y" file_id="multibootelf" />


*/

#ifndef V3_CONFIG_DEBUG_MULTIBOOT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


int v3_init_multiboot()
{
    PrintDebug(VM_NONE,VCORE_NONE, "multiboot: init\n");
    return 0;
}

int v3_deinit_multiboot()
{
    PrintDebug(VM_NONE,VCORE_NONE, "multiboot: deinit\n");
    return 0;
}



#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))

int v3_init_multiboot_vm(struct v3_vm_info *vm, struct v3_xml *config)
{
    v3_cfg_tree_t *mb_config;
    char *enable;
    char *mb_file_id=0;

    PrintDebug(vm, VCORE_NONE, "multiboot: vm init\n");

    memset(&vm->mb_state,0,sizeof(struct v3_vm_multiboot));
    vm->mb_state.is_multiboot=0;

    if (!config || !(mb_config=v3_cfg_subtree(config,"multiboot"))) {
	PrintDebug(vm,VCORE_NONE,"multiboot: no multiboot configuration found - normal boot will occur\n");
	goto out_ok;
    }
    
    if (!(enable=v3_cfg_val(mb_config,"enable")) || strcasecmp(enable,"y")) {
	PrintDebug(vm,VCORE_NONE,"multiboot: multiboot configuration disabled\n");
	goto out_ok;
    }

    if (!(mb_file_id=v3_cfg_val(mb_config,"file_id"))) { 
	PrintError(vm,VCORE_NONE,"multiboot: multiboot block without file_id...\n");
	return -1;
    }

    vm->mb_state.mb_file = v3_cfg_get_file(vm,mb_file_id);
    
    if (!vm->mb_state.mb_file) { 
	PrintError(vm,VCORE_NONE,"multiboot: multiboot block contains bad file_id (%s)\n",mb_file_id);
	return -1;
    }

    vm->mb_state.is_multiboot=1;

 out_ok:
    if (vm->mb_state.is_multiboot) {
	V3_Print(vm,VCORE_NONE,"multiboot: file_id=%s (tag %s)]\n",
		 mb_file_id,
		 vm->mb_state.mb_file->tag);
    } else {
	V3_Print(vm,VCORE_NONE,"multiboot: This is not a multiboot VM\n");
    }
    return 0;
    
}


int v3_deinit_multiboot_vm(struct v3_vm_info *vm)
{
    PrintDebug(vm, VCORE_NONE, "multiboot: multiboot VM deinit\n");

    return 0;
}

int v3_init_multiboot_core(struct guest_info *core)
{
    PrintDebug(core->vm_info, VCORE_NONE, "multiboot: multiboot core init\n");

    // Nothing to do at this point

    return 0;
}

int v3_deinit_multiboot_core(struct guest_info *core)
{
    PrintDebug(core->vm_info, VCORE_NONE, "multiboot: multiboot core deinit\n");

    return 0;
}




#define ERROR(fmt, args...) PrintError(VM_NONE,VCORE_NONE,"multiboot: " fmt,##args)
#define INFO(fmt, args...) PrintDebug(VM_NONE,VCORE_NONE,"multiboot: " fmt,##args)


static int is_elf(uint8_t *data, uint64_t size)
{
    if (*((uint32_t*)data)==ELF_MAGIC) {
	return 1;
    } else { 
	return 0;
    }
}

static mb_header_t *find_mb_header(uint8_t *data, uint64_t size)
{
    uint64_t limit = size > 32768 ? 32768 : size;
    uint64_t i;

    // Scan for the .boot magic cookie
    // must be in first 32K, assume 4 byte aligned
    for (i=0;i<limit;i+=4) { 
	if (*((uint32_t*)&data[i])==MB2_MAGIC) {
	    INFO("Found multiboot header at offset 0x%llx\n",i);
	    return (mb_header_t *) &data[i];
	}
    }
    return 0;
}

static int checksum4_ok(uint32_t *data, uint64_t size)
{
    int i;
    uint32_t sum=0;

    for (i=0;i<size;i++) {
	sum+=data[i];
    }

    return sum==0;
}

static int parse_multiboot_kernel(uint8_t *data, uint64_t size, mb_data_t *mb)
{
    uint64_t i;

    mb_header_t *mb_header=0;
    mb_tag_t *mb_tag=0;
    mb_info_t *mb_inf=0;
    mb_addr_t *mb_addr=0;
    mb_entry_t *mb_entry=0;
    mb_flags_t *mb_flags=0;
    mb_framebuf_t *mb_framebuf=0;
    mb_modalign_t *mb_modalign=0;
    mb_mb64_hrt_t *mb_mb64_hrt=0;


    if (!is_elf(data,size)) { 
	ERROR("HRT is not an ELF\n");
	return -1;
    }

    mb_header = find_mb_header(data,size);

    if (!mb_header) { 
	ERROR("No multiboot header found\n");
	return -1;
    }

    // Checksum applies only to the header itself, not to 
    // the subsequent tags... 
    if (!checksum4_ok((uint32_t*)mb_header,4)) { 
	ERROR("Multiboot header has bad checksum\n");
	return -1;
    }

    INFO("Multiboot header: arch=0x%x, headerlen=0x%x\n", mb_header->arch, mb_header->headerlen);

    mb_tag = (mb_tag_t*)((void*)mb_header+16);

    while (!(mb_tag->type==0 && mb_tag->size==8)) {
	INFO("tag: type 0x%x flags=0x%x size=0x%x\n",mb_tag->type, mb_tag->flags,mb_tag->size);
	switch (mb_tag->type) {
	    case MB_TAG_INFO: {
		if (mb_inf) { 
		    ERROR("Multiple info tags found!\n");
		    return -1;
		}
		mb_inf = (mb_info_t*)mb_tag;
		INFO(" info request - types follow\n");
		for (i=0;(mb_tag->size-8)/4;i++) {
		    INFO("  %llu: type 0x%x\n", i, mb_inf->types[i]);
		}
	    }
		break;

	    case MB_TAG_ADDRESS: {
		if (mb_addr) { 
		    ERROR("Multiple address tags found!\n");
		    return -1;
		}
		mb_addr = (mb_addr_t*)mb_tag;
		INFO(" address\n");
		INFO("  header_addr     =  0x%x\n", mb_addr->header_addr);
		INFO("  load_addr       =  0x%x\n", mb_addr->load_addr);
		INFO("  load_end_addr   =  0x%x\n", mb_addr->load_end_addr);
		INFO("  bss_end_addr    =  0x%x\n", mb_addr->bss_end_addr);
	    }
		break;

	    case MB_TAG_ENTRY: {
		if (mb_entry) { 
		    ERROR("Multiple entry tags found!\n");
		    return -1;
		}
		mb_entry=(mb_entry_t*)mb_tag;
		INFO(" entry\n");
		INFO("  entry_addr      =  0x%x\n", mb_entry->entry_addr);
	    }
		break;
		
	    case MB_TAG_FLAGS: {
		if (mb_flags) { 
		    ERROR("Multiple flags tags found!\n");
		    return -1;
		}
		mb_flags = (mb_flags_t*)mb_tag;
		INFO(" flags\n");
		INFO("  console_flags   =  0x%x\n", mb_flags->console_flags);
	    }
		break;
		
	    case MB_TAG_FRAMEBUF: {
		if (mb_framebuf) { 
		    ERROR("Multiple framebuf tags found!\n");
		    return -1;
		}
		mb_framebuf = (mb_framebuf_t*)mb_tag;
		INFO(" framebuf\n");
		INFO("  width           =  0x%x\n", mb_framebuf->width);
		INFO("  height          =  0x%x\n", mb_framebuf->height);
		INFO("  depth           =  0x%x\n", mb_framebuf->depth);
	    }
		break;

	    case MB_TAG_MODALIGN: {
		if (mb_modalign) { 
		    ERROR("Multiple modalign tags found!\n");
		    return -1;
		}
		mb_modalign = (mb_modalign_t*)mb_tag;
		INFO(" modalign\n");
		INFO("  size            =  0x%x\n", mb_modalign->size);
	    }
		break;

#if V3_CONFIG_HVM
	    case MB_TAG_MB64_HRT: {
		if (mb_mb64_hrt) { 
		    ERROR("Multiple mb64_hrt tags found!\n");
		    return -1;
		}
		mb_mb64_hrt = (mb_mb64_hrt_t*)mb_tag;
		INFO(" mb64_hrt\n");
	    }
		break;
#endif
	    default: 
		INFO("Unknown tag... Skipping...\n");
		break;
	}
	mb_tag = (mb_tag_t *)(((void*)mb_tag) + mb_tag->size);
    }

    // copy out to caller
    mb->header=mb_header;
    mb->info=mb_inf;
    mb->addr=mb_addr;
    mb->entry=mb_entry;
    mb->flags=mb_flags;
    mb->framebuf=mb_framebuf;
    mb->modalign=mb_modalign;
    mb->mb64_hrt=mb_mb64_hrt;

    return 0;
}


int v3_parse_multiboot_header(struct v3_cfg_file *file, mb_data_t *result)
{
    return parse_multiboot_kernel(file->data,file->size,result);
}


#define APIC_BASE     0xfee00000
#define IOAPIC_BASE   0xfec00000

/*
  MB_INFO_HEADER
  MB_HRT  (if this is an HVM
  MB_BASIC_MEMORY
  MB_MEMORY_MAP
    0..640K  RAM
    640K..1024 reserved
    1024..ioapic_base RAM
    ioapic_base to ioapic_base+page reserved
    ioapic_base+page to apic_base ram
    apic_base oto apic_base+page reserved
    apic_base+page to total RAM

   
 The multiboot structure that is written reflects the 
 perspective of the core given the kind of VM it is part of.

 Regular VM
    - core does not matter 
    - all memory visible

 HVM
   ROS core
    - only ROS memory visible
    - regular multiboot or bios boot assumed
   HRT core
    - all memory visible
    - HRT64 multiboot assumed

*/

uint64_t v3_build_multiboot_table(struct guest_info *core, uint8_t *dest, uint64_t size)
{
    struct v3_vm_info *vm = core->vm_info;
    mb_info_header_t *header=0;
#ifdef V3_CONFIG_HVM
    mb_info_hrt_t *hrt=0;
#endif
    mb_info_mem_t *mem=0;
    mb_info_memmap_t *memmap=0;
    mb_info_tag_t *tag=0;
    uint64_t num_mem=0, cur_mem=0;
    
    uint64_t total_mem = vm->mem_size;

#ifdef V3_CONFIG_HVM
    if (vm->hvm_state.is_hvm) { 
	if (v3_is_hvm_ros_core(core)) {
	    PrintDebug(core->vm_info,core,"multiboot: hvm: building mb table from ROS core perspective\n");
	    total_mem = v3_get_hvm_ros_memsize(vm);
	} else {
	    PrintDebug(core->vm_info,core,"multiboot: hvm: building mb table from HRT core perspective\n");
	    total_mem = v3_get_hvm_hrt_memsize(vm);	
	}
    }
#endif

    // assume we have > 1 MB + apic+ioapic
    num_mem = 5;
    if (total_mem>IOAPIC_BASE+PAGE_SIZE) {
	num_mem++;
    }
    if (total_mem>APIC_BASE+PAGE_SIZE) {
	num_mem++;
    }


    uint64_t needed = 
	sizeof(mb_info_header_t) +
#ifdef V3_CONFIG_HVM
	core->vm_info->hvm_state.is_hvm && core->hvm_state.is_hrt ? sizeof(mb_info_hrt_t) : 0 
#endif
	+ 
	sizeof(mb_info_mem_t) + 
	sizeof(mb_info_memmap_t) + 
	sizeof(mb_info_memmap_entry_t) * num_mem  +
	sizeof(mb_info_tag_t);

    if (needed>size) { 
	return 0;
    }

    uint8_t *next;

    if (needed>size) {
	ERROR("Cannot fit MB info in needed space\n");
	return -1;
    }

    next = dest;

    header = (mb_info_header_t*)next;
    next += sizeof(mb_info_header_t);

#if V3_CONFIG_HVM
    if (core->vm_info->hvm_state.is_hvm && v3_is_hvm_hrt_core(core)) { 
	hrt = (mb_info_hrt_t*)next;
	next += sizeof(mb_info_hrt_t);
    }
#endif

    mem = (mb_info_mem_t*)next;
    next += sizeof(mb_info_mem_t);

    memmap = (mb_info_memmap_t*)next;
    next += sizeof(mb_info_memmap_t) + num_mem * sizeof(mb_info_memmap_entry_t);

    tag = (mb_info_tag_t*)next;
    next += sizeof(mb_info_tag_t);

    header->totalsize = (uint32_t)(next - dest);
    header->reserved = 0;

#ifdef V3_CONFIG_HVM
    if (core->vm_info->hvm_state.is_hvm && v3_is_hvm_hrt_core(core)) { 
	v3_build_hrt_multiboot_tag(core,hrt);
    }
#endif

    mem->tag.type = MB_INFO_MEM_TAG;
    mem->tag.size = sizeof(mb_info_mem_t);
    mem->mem_lower = 640; // thank you, bill gates
    mem->mem_upper = (total_mem  - 1024 * 1024) / 1024;

    memmap->tag.type = MB_INFO_MEMMAP_TAG;
    memmap->tag.size = sizeof(mb_info_memmap_t) + num_mem * sizeof(mb_info_memmap_entry_t);
    memmap->entry_size = 24;
    memmap->entry_version = 0;

    cur_mem=0;

    // first 640K
    memmap->entries[cur_mem].base_addr = 0;
    memmap->entries[cur_mem].length = 640*1024;
    memmap->entries[cur_mem].type = MEM_RAM;
    memmap->entries[cur_mem].reserved = 0;
    cur_mem++;

    // legacy io (640K->1 MB)
    memmap->entries[cur_mem].base_addr = 640*1024;
    memmap->entries[cur_mem].length = 384*1024;
    memmap->entries[cur_mem].type = MEM_RESV;
    memmap->entries[cur_mem].reserved = 1;
    cur_mem++;

    // first meg to ioapic
    memmap->entries[cur_mem].base_addr = 1024*1024;
    memmap->entries[cur_mem].length = (total_mem < IOAPIC_BASE ? total_mem : IOAPIC_BASE) - 1024*1024;
    memmap->entries[cur_mem].type = MEM_RAM;
    memmap->entries[cur_mem].reserved = 0;
    cur_mem++;

    // ioapic reservation
    memmap->entries[cur_mem].base_addr = IOAPIC_BASE;
    memmap->entries[cur_mem].length = PAGE_SIZE;
    memmap->entries[cur_mem].type = MEM_RESV;
    memmap->entries[cur_mem].reserved = 1;
    cur_mem++;

    if (total_mem > (IOAPIC_BASE + PAGE_SIZE)) {
	// memory between ioapic and apic
	memmap->entries[cur_mem].base_addr = IOAPIC_BASE+PAGE_SIZE;
	memmap->entries[cur_mem].length = (total_mem < APIC_BASE ? total_mem : APIC_BASE) - (IOAPIC_BASE+PAGE_SIZE);;
	memmap->entries[cur_mem].type = MEM_RAM;
	memmap->entries[cur_mem].reserved = 0;
	cur_mem++;
    } 

    // apic
    memmap->entries[cur_mem].base_addr = APIC_BASE;
    memmap->entries[cur_mem].length = PAGE_SIZE;
    memmap->entries[cur_mem].type = MEM_RESV;
    memmap->entries[cur_mem].reserved = 1;
    cur_mem++;

    if (total_mem > (APIC_BASE + PAGE_SIZE)) {
	// memory after apic
	memmap->entries[cur_mem].base_addr = APIC_BASE+PAGE_SIZE;
	memmap->entries[cur_mem].length = total_mem - (APIC_BASE+PAGE_SIZE);
	memmap->entries[cur_mem].type = MEM_RAM;
	memmap->entries[cur_mem].reserved = 0;
	cur_mem++;
    } 

    for (cur_mem=0;cur_mem<num_mem;cur_mem++) { 
	PrintDebug(vm, VCORE_NONE,
		   "multiboot: entry %llu: %p (%llx bytes) - type %x %s\n",
		   cur_mem, 
		   (void*) memmap->entries[cur_mem].base_addr,
		   memmap->entries[cur_mem].length,
		   memmap->entries[cur_mem].type,
		   memmap->entries[cur_mem].reserved ? "reserved" : "");
    }



    // This demarcates end of list
    tag->type = 0;
    tag->size = 8;

    return header->totalsize;

}


int v3_write_multiboot_kernel(struct v3_vm_info *vm, mb_data_t *mb, struct v3_cfg_file *file,
			      void *base, uint64_t limit)
{
    uint32_t offset=0;
    uint32_t header_offset = (uint32_t) ((uint64_t)(mb->header) - (uint64_t)(file->data));
    uint32_t size;

    if (!mb->addr || !mb->entry) { 
	PrintError(vm,VCORE_NONE, "multiboot: kernel is missing address or entry point\n");
	return -1;
    }

    if (((void*)(uint64_t)(mb->addr->header_addr) < base ) ||
	((void*)(uint64_t)(mb->addr->load_end_addr) > base+limit) ||
	((void*)(uint64_t)(mb->addr->bss_end_addr) > base+limit)) { 
	PrintError(vm,VCORE_NONE, "multiboot: kernel is not within the allowed portion of VM\n");
	return -1;
    }

    offset = header_offset - (mb->addr->header_addr - mb->addr->load_addr);
    size = mb->addr->load_end_addr - mb->addr->load_addr;
    
    if (size != file->size-offset) { 
	V3_Print(vm,VCORE_NONE,"multiboot: strange: size computed as %u, but file->size-offset = %llu\n",size,file->size-offset);
    }

    // We are trying to do as little ELF loading here as humanly possible
    v3_write_gpa_memory(&vm->cores[0],
			(addr_t)(mb->addr->load_addr),
			size,
			file->data+offset);

    PrintDebug(vm,VCORE_NONE,
	       "multiboot: wrote 0x%llx bytes starting at offset 0x%llx to %p\n",
	       (uint64_t) size,
	       (uint64_t) offset,
	       (void*)(addr_t)(mb->addr->load_addr));

    size = mb->addr->bss_end_addr - mb->addr->load_end_addr + 1;

    // Now we need to zero the BSS
    v3_set_gpa_memory(&vm->cores[0],
		      (addr_t)(mb->addr->load_end_addr),
		      size,
		      0);
		      
    PrintDebug(vm,VCORE_NONE,
	       "multiboot: zeroed 0x%llx bytes starting at %p\n",
	       (uint64_t) size,
	       (void*)(addr_t)(mb->addr->load_end_addr));
		      

    return 0;

}


static int setup_multiboot_kernel(struct v3_vm_info *vm)
{
    void *base = 0;
    uint64_t limit = vm->mem_size;


    if (vm->mb_state.mb_file->size > limit) { 
	PrintError(vm,VCORE_NONE,"multiboot: Cannot map kernel because it is too big (%llu bytes, but only have %llu space\n", vm->mb_state.mb_file->size, (uint64_t)limit);
	return -1;
    }

    if (!is_elf(vm->mb_state.mb_file->data,vm->mb_state.mb_file->size)) { 
	PrintError(vm,VCORE_NONE,"multiboot: supplied kernel is not an ELF\n");
	return -1;
    } else {
	if (find_mb_header(vm->mb_state.mb_file->data,vm->mb_state.mb_file->size)) { 
	    PrintDebug(vm,VCORE_NONE,"multiboot: appears to be a multiboot kernel\n");
	    if (v3_parse_multiboot_header(vm->mb_state.mb_file,&vm->mb_state.mb_data)) { 
		PrintError(vm,VCORE_NONE,"multiboot: cannot parse multiboot kernel header\n");
		return -1;
	    }
	    if (v3_write_multiboot_kernel(vm, &(vm->mb_state.mb_data),vm->mb_state.mb_file,base,limit)) { 
		PrintError(vm,VCORE_NONE,"multiboot: multiboot kernel setup failed\n");
		return -1;
	    } 
	} else {
	    PrintError(vm,VCORE_NONE,"multiboot: multiboot kernel has no header\n");
	    return -1;
	}
    }
    
    return 0;
    
}

// 32 bit GDT entries
//
//         base24-31    flags2  limit16-19 access8  base16-23   base0-15   limit0-15
// null       0           0          0       0         0           0           0
// code       0           1100       f     10011010    0           0         ffff
// data       0           1100       f     10010010    0           0         ffff
//
// null =   00 00 00 00 00 00 00 00
// code =   00 cf 9a 00 00 00 ff ff 
// data =   00 cf 92 00 00 00 ff ff
//
static uint64_t gdt32[3] = {
    0x0000000000000000, /* null */
    0x00cf9a000000ffff, /* code (note lme=0) */
    0x00cf92000000ffff, /* data */
};

static void write_gdt(struct v3_vm_info *vm, void *base, uint64_t limit)
{
    v3_write_gpa_memory(&vm->cores[0],(addr_t)base,limit,(uint8_t*) gdt32);

    PrintDebug(vm,VCORE_NONE,"multiboot: wrote GDT at %p\n",base);
}

	
static void write_tss(struct v3_vm_info *vm, void *base, uint64_t limit)
{
    v3_set_gpa_memory(&vm->cores[0],(addr_t)base,limit,0);

    PrintDebug(vm,VCORE_NONE,"multiboot: wrote TSS at %p\n",base);
}

static void write_table(struct v3_vm_info *vm, void *base, uint64_t limit)
{
    uint64_t size;
    uint8_t buf[256];

    limit = limit < 256 ? limit : 256;

    size = v3_build_multiboot_table(&vm->cores[0], buf, limit);

    if (size>256 || size==0) { 
	PrintError(vm,VCORE_NONE,"multiboot: cannot build multiboot table\n");
	return;
    }
    
    v3_write_gpa_memory(&vm->cores[0],(addr_t)base,size,buf);

}



/*
  GPA layout:

  GDT
  TSS
  MBinfo   
  Kernel at its desired load address (or error)

*/


int v3_setup_multiboot_vm_for_boot(struct v3_vm_info *vm)
{
    void *kernel_start_gpa;
    void *kernel_end_gpa;
    void *mb_gpa;
    void *tss_gpa;
    void *gdt_gpa;

    if (!vm->mb_state.is_multiboot) { 
	PrintDebug(vm,VCORE_NONE,"multiboot: skipping multiboot setup for boot as this is not a multiboot VM\n");
	return 0;
    }

    
    if (setup_multiboot_kernel(vm)) {
	PrintError(vm,VCORE_NONE,"multiboot: failed to setup kernel\n");
	return -1;
    } 

    kernel_start_gpa = (void*) (uint64_t) (vm->mb_state.mb_data.addr->load_addr);
    kernel_end_gpa = (void*) (uint64_t) (vm->mb_state.mb_data.addr->bss_end_addr);

    // Is there room below the kernel? 
    if ((uint64_t)kernel_start_gpa > 19*4096 ) {
	// at least 3 pages between 64K and start of kernel 
	// place at 64K
	mb_gpa=(void*)(16*4096);
    } else {
	// is there room above the kernel?
	if ((uint64_t)kernel_end_gpa < vm->mem_size-4*4096) { 
	    if (((uint64_t)kernel_end_gpa + 4 * 4096) <= 0xffffffff) { 
		mb_gpa=(void*) (4096*((uint64_t)kernel_end_gpa/4096 + 1));
	    } else {
		PrintError(vm,VCORE_NONE,"multiboot: no room for mb data below 4 GB\n");
		return -1;
	    } 
	} else {
	    PrintError(vm,VCORE_NONE,"multiboot: no room for mb data above kernel\n");
	    return -1;
	}
    }

    PrintDebug(vm,VCORE_NONE,"multiboot: mb data will start at %p\n",mb_gpa);

    vm->mb_state.mb_data_gpa=mb_gpa;

    tss_gpa = mb_gpa + 1 * 4096;
    gdt_gpa = mb_gpa + 2 * 4096;

    write_table(vm,mb_gpa,4096);
    
    write_tss(vm,tss_gpa,4096);

    write_gdt(vm,gdt_gpa,4096);

    PrintDebug(vm,VCORE_NONE,"multiboot: setup of memory done\n");

    return 0;
}

/*
  On entry:

   IDTR not set
   GDTR points to stub GDT
   TR   points to stub TSS
   CR0  has PE and not PG
   EIP  is entry point to kernel
   EBX  points to multiboot info
   EAX  multiboot magic cookie

*/
int v3_setup_multiboot_core_for_boot(struct guest_info *core)
{
    void *base;
    uint64_t limit;

    if (!core->vm_info->mb_state.is_multiboot) {
	PrintDebug(core->vm_info,core,"multiboot: skipping mb core setup as this is not an mb VM\n");
	return 0;
    }
	
    if (core->vcpu_id != 0) {
	PrintDebug(core->vm_info,core,"multiboot: skipping mb core setup as this is not the BSP core\n");
	return 0;
    }


    PrintDebug(core->vm_info, core, "multiboot: setting up MB BSP core for boot\n");

    
    memset(&core->vm_regs,0,sizeof(core->vm_regs));
    memset(&core->ctrl_regs,0,sizeof(core->ctrl_regs));
    memset(&core->dbg_regs,0,sizeof(core->dbg_regs));
    memset(&core->segments,0,sizeof(core->segments));    
    memset(&core->msrs,0,sizeof(core->msrs));    
    memset(&core->fp_state,0,sizeof(core->fp_state));    

    // We need to be in protected mode at ring zero
    core->cpl = 0; // we are going right into the kernel
    core->cpu_mode = PROTECTED;
    core->mem_mode = PHYSICAL_MEM; 
    // default run-state is fine, we are core zero
    // core->core_run_state = CORE_RUNNING ;

    // right into the kernel
    core->rip = (uint64_t) core->vm_info->mb_state.mb_data.entry->entry_addr;

    // Setup CRs for protected mode
    // CR0:  PE (but no PG)
    core->ctrl_regs.cr0 = 0x1;
    core->shdw_pg_state.guest_cr0 = core->ctrl_regs.cr0;

    // CR2: don't care (output from #PF)
    // CR3: don't care (no paging)
    core->ctrl_regs.cr3 = 0;
    core->shdw_pg_state.guest_cr3 = core->ctrl_regs.cr3;

    // CR4: no features 
    core->ctrl_regs.cr4 = 0x0;
    core->shdw_pg_state.guest_cr4 = core->ctrl_regs.cr4;
    // CR8 as usual
    // RFLAGS zeroed is fine: come in with interrupts off
    // EFER needs SVME and LME but not LMA (last 16 bits: 0 0 0 1 0 1 0 0   0 0 0 0 0 0 0 0
    core->ctrl_regs.efer = 0x1400;
    core->shdw_pg_state.guest_efer.value = core->ctrl_regs.efer;


    /* 
       Notes on selectors:

       selector is 13 bits of index, 1 bit table indicator 
       (0=>GDT), 2 bit RPL
       
       index is scaled by 8, even in long mode, where some entries 
       are 16 bytes long.... 
          -> code, data descriptors have 8 byte format
             because base, limit, etc, are ignored (no segmentation)
          -> interrupt/trap gates have 16 byte format 
             because offset needs to be 64 bits
    */
    
    // There is no IDTR set and interrupts are disabled

    // Install our stub GDT
    core->segments.gdtr.selector = 0;
    core->segments.gdtr.base = (addr_t) core->vm_info->mb_state.mb_data_gpa+2*4096;
    core->segments.gdtr.limit = 4096-1;
    core->segments.gdtr.type = 0x6;
    core->segments.gdtr.system = 1; 
    core->segments.gdtr.dpl = 0;
    core->segments.gdtr.present = 1;
    core->segments.gdtr.long_mode = 0;
    
    // And our TSS
    core->segments.tr.selector = 0;
    core->segments.tr.base = (addr_t) core->vm_info->mb_state.mb_data_gpa+1*4096;
    core->segments.tr.limit = 4096-1;
    core->segments.tr.type = 0x6;
    core->segments.tr.system = 1; 
    core->segments.tr.dpl = 0;
    core->segments.tr.present = 1;
    core->segments.tr.long_mode = 0;
    
    base = 0x0;
    limit = -1;

    // And CS
    core->segments.cs.selector = 0x8 ; // entry 1 of GDT (RPL=0)
    core->segments.cs.base = (addr_t) base;
    core->segments.cs.limit = limit;
    core->segments.cs.type = 0xa;
    core->segments.cs.system = 1; 
    core->segments.cs.dpl = 0;
    core->segments.cs.present = 1;
    core->segments.cs.long_mode = 0;
    core->segments.cs.db = 1; // 32 bit operand and address size
    core->segments.cs.granularity = 1; // pages

    // DS, SS, etc are identical
    core->segments.ds.selector = 0x10; // entry 2 of GDT (RPL=0)
    core->segments.ds.base = (addr_t) base;
    core->segments.ds.limit = limit;
    core->segments.ds.type = 0x2;
    core->segments.ds.system = 1; 
    core->segments.ds.dpl = 0;
    core->segments.ds.present = 1;
    core->segments.ds.long_mode = 0;
    core->segments.ds.db = 1; // 32 bit operand and address size
    core->segments.ds.granularity = 1; // pages

    memcpy(&core->segments.ss,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.es,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.fs,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.gs,&core->segments.ds,sizeof(core->segments.ds));
    


    // Now for our magic - this signals
    // the kernel that a multiboot loader loaded it
    // and that rbx points to its offered data
    core->vm_regs.rax = MB2_INFO_MAGIC;

    core->vm_regs.rbx = (uint64_t) (core->vm_info->mb_state.mb_data_gpa);

    // reset paging here for shadow... 

    if (core->shdw_pg_mode != NESTED_PAGING) { 
	PrintError(core->vm_info, core, "multiboot: shadow paging guest... this will end badly\n");
	return -1;
    }


    return 0;
}


int v3_handle_multiboot_reset(struct guest_info *core)
{
    int rc;

    if (core->core_run_state!=CORE_RESETTING) { 
	return 0;
    }

    if (!core->vm_info->mb_state.is_multiboot) { 
	return 0;
    }

    // wait for everyone
    v3_counting_barrier(&core->vm_info->reset_barrier);

    if (core->vcpu_id==0) {
	// I am leader (this is true if I am a ROS core or this is a non-HVM)
	core->vm_info->run_state = VM_RESETTING;
    }

    rc=0;
	
    if (core->vcpu_id==0) {
	// we will recopy the image
	rc |= v3_setup_multiboot_vm_for_boot(core->vm_info);
    }

    rc |= v3_setup_multiboot_core_for_boot(core);

    if (core->vcpu_id==0) { 
	core->core_run_state = CORE_RUNNING;
	core->vm_info->run_state = VM_RUNNING;
    } else {
	// for APs, we need to bring them back to the init state
	core->cpu_mode = REAL;
	core->mem_mode = PHYSICAL_MEM;
	core->core_run_state = CORE_STOPPED;
    }

    // sync on the way out
    v3_counting_barrier(&core->vm_info->reset_barrier);

    if (rc<0) {
	return rc;
    } else {
	return 1; // reboot
    }
}
	
