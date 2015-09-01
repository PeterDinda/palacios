/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Daniel Zuo <pengzuo2014@u.northwestern.edu>
 *         Nikhat Karimi <nikhatkarimi@gmail.com>
 *         Ahalya Srinivasan <AhalyaSrinivasan2015@u.northwestern.edu>
 *         Peter Dinda <pdinda@northwestern.edu> (pinning, cleanup, integration, locking etc)
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

#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_direct_paging.h>

#include <palacios/vmm_xml.h>


/*

  <mem ... >N_MB</mem>             Size of memory in the GPA

  <swapping enable="y">
     <allocated>M_MB</allocated>   Allocated space (M_MB <= N_MB)
     <file>FILENAME</file>         Where to swap to
     <strategy>STRATEGY</strategy> Victim picker to use NEXT_FIT, RANDOM (default), LRU, DEFAULT 
  </swapping>

*/


#ifndef V3_CONFIG_DEBUG_SWAPPING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

int v3_init_swapping()
{
    PrintDebug(VM_NONE,VCORE_NONE, "swapper: init\n");
    return 0;

}

int v3_deinit_swapping()
{
    PrintDebug(VM_NONE,VCORE_NONE, "swapper: deinit\n");
    return 0;
}


static int write_all(v3_file_t fd, void *buf, uint64_t len, uint64_t offset)
{
    sint64_t thisop;

    while (len>0) { 
	thisop = v3_file_write(fd, buf, len, offset);
	if (thisop <= 0) { 
	    return -1;
	}
	buf+=thisop;
	offset+=thisop;
	len-=thisop;
    }
    return 0; 
}

static int read_all(v3_file_t fd, void *buf, uint64_t len, uint64_t offset)
{
    sint64_t thisop;

    while (len>0) { 
	thisop = v3_file_read(fd, buf, len, offset);
	if (thisop <= 0) { 
	    return -1;
	}
	buf+=thisop;
	offset+=thisop;
	len-=thisop;
    }
    return 0; 
}


#define REGION_WARN_THRESH 16

#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))

int v3_init_swapping_vm(struct v3_vm_info *vm, struct v3_xml *config)
{
    v3_cfg_tree_t *swap_config;
    char *enable;
    char *allocated;
    char *strategy;
    char *file;
    uint64_t alloc;
    extern uint64_t v3_mem_block_size;


    PrintDebug(vm, VCORE_NONE, "swapper: vm init\n");

    memset(&vm->swap_state,0,sizeof(struct v3_swap_impl_state));

    v3_lock_init(&(vm->swap_state.lock));

    vm->swap_state.enable_swapping=0;
    vm->swap_state.host_mem_size=vm->mem_size;
    
    if (!config || !(swap_config=v3_cfg_subtree(config,"swapping"))) {
	PrintDebug(vm,VCORE_NONE,"swapper: no swapping configuration found\n");
	return 0;
    }
    
    if (!(enable=v3_cfg_val(swap_config,"enable")) || strcasecmp(enable,"y")) {
	PrintDebug(vm,VCORE_NONE,"swapper: swapping configuration disabled\n");
	return 0;
    }

    allocated = v3_cfg_val(swap_config,"allocated");
    if (!allocated) { 
	PrintError(vm,VCORE_NONE,"swapper: swapping configuration must included allocated block\n");
	return -1;
    }
    alloc = ((uint64_t)atoi(allocated))*1024*1024;

    // make alloc an integer multiple of the memory block size
    alloc = CEIL_DIV(alloc, v3_mem_block_size) * v3_mem_block_size;

    PrintDebug(vm,VCORE_NONE,"swapper: adjusted allocated size is %llu\n",alloc);

    if (alloc > vm->mem_size) { 
	PrintError(vm,VCORE_NONE,"swapper: cannot allocate more than the VM's memory size....\n");
	return -1;
    }

    
    file = v3_cfg_val(swap_config,"file");
    if (!file) { 
	PrintError(vm,VCORE_NONE,"swapper: swapping configuration must included swap file name\n");
	return -1;
    }
    
    strategy = v3_cfg_val(swap_config,"strategy");
    if (!strategy) { 
	PrintDebug(vm,VCORE_NONE,"swapper: default strategy selected\n");
	strategy="default";
    }

    // Can we allocate the file?

    if (!(vm->swap_state.swapfd = v3_file_open(vm,file, FILE_OPEN_MODE_READ | FILE_OPEN_MODE_WRITE | FILE_OPEN_MODE_CREATE))) {
	PrintError(vm,VCORE_NONE,"swapper: cannot open or create swap file\n");
	return -1;
    } else {
	// Make sure we can write the whole thing
	uint64_t addr;
	char *buf = V3_Malloc(PAGE_SIZE_4KB);
	if (!buf) { 
	    PrintError(vm,VCORE_NONE,"swapper: unable to allocate space for writing file\n");
	    return -1;
	}
	memset(buf,0,PAGE_SIZE_4KB);
	for (addr=0;addr<vm->mem_size;addr+=PAGE_SIZE_4KB) { 
	    if (write_all(vm->swap_state.swapfd, 
			  buf,
			  PAGE_SIZE_4KB,
			  addr)) { 
		PrintError(vm,VCORE_NONE,"swapper: unable to write initial swap file\n");
		V3_Free(buf);
		v3_file_close(vm->swap_state.swapfd);
		return -1;
	    }
	}
	V3_Free(buf);
    }

    // We are now set - we have space to swap to
    vm->swap_state.enable_swapping=1;

    vm->swap_state.strategy = 
	!strcasecmp(strategy,"next_fit") ? V3_SWAP_NEXT_FIT :
	!strcasecmp(strategy,"random") ? V3_SWAP_RANDOM :
	!strcasecmp(strategy,"lru") ? V3_SWAP_LRU :
	!strcasecmp(strategy,"default") ? V3_SWAP_RANDOM :  // identical branches for clarity
	V3_SWAP_RANDOM;

    vm->swap_state.host_mem_size=alloc;
    vm->swap_state.swap_count=0;
    vm->swap_state.last_region_used=0;
    // already have set swapfd


    V3_Print(vm,VCORE_NONE,"swapper: swapping enabled (%llu allocated of %llu using %s on %s)\n",
	     (uint64_t)vm->swap_state.host_mem_size, (uint64_t) vm->mem_size, strategy, file);

    if (vm->swap_state.host_mem_size / v3_mem_block_size < REGION_WARN_THRESH) { 
	V3_Print(vm,VCORE_NONE,"swapper: WARNING: %llu regions is less than threshold of %llu, GUEST MAY FAIL TO MAKE PROGRESS\n",
		 (uint64_t)vm->swap_state.host_mem_size/v3_mem_block_size, (uint64_t)REGION_WARN_THRESH);
    }

    return 0;
    
}

int v3_deinit_swapping_vm(struct v3_vm_info *vm)
{
    PrintDebug(vm, VCORE_NONE, "swapper: vm deinit\n");

    if (vm->swap_state.enable_swapping) {
	v3_file_close(vm->swap_state.swapfd);
    }

    v3_lock_deinit(&(vm->swap_state.lock));

    return 0;
}


int v3_pin_region(struct v3_vm_info *vm, struct v3_mem_region *region)
{
    unsigned int flags;

    PrintDebug(vm,VCORE_NONE, "Pin Region GPA=%p to %p\n",(void*) region->guest_start, (void*)region->guest_end);

    if (!(region->flags.base)) { 
	PrintError(vm,VCORE_NONE,"Attempt to pin non-base region\n");
	return -1;
    }
    
    if (region->flags.pinned) { 
	return 0;
    }
	   
    flags = v3_lock_irqsave(vm->swap_state.lock);
    
    if (region->flags.swapped) {
	// can't pin since it's swapped out, swap it in an try again
	v3_unlock_irqrestore(vm->swap_state.lock, flags);
	if (v3_swap_in_region(vm,region)) { 
	    PrintError(vm,VCORE_NONE,"Cannot swap in during a pin operation\n");
	    return -1;
	} else {
	    return v3_pin_region(vm,region);
	}
    }
    
    // still holding lock if we got here, so we're the exclusive
    // manipulator of the swap state
    region->flags.pinned=1;
    
    v3_unlock_irqrestore(vm->swap_state.lock, flags);
    
    return 0;
}


int v3_unpin_region(struct v3_vm_info *vm, struct v3_mem_region *region)
{
    unsigned int flags = v3_lock_irqsave(vm->swap_state.lock);
    
    region->flags.pinned=0;

    v3_unlock_irqrestore(vm->swap_state.lock,flags);

    return 0;

}


#define SEARCH_LIMIT 1024

// Must be called with the lock held
static struct v3_mem_region * choose_random_victim(struct v3_vm_info * vm) 
{
    
    struct v3_mem_map * map = &(vm->mem_map);
    uint64_t num_base_regions = map->num_base_regions;
    uint64_t thetime;
    struct v3_mem_region *reg=0;
    uint32_t i=0;
	
    PrintDebug(vm, VCORE_NONE, "swapper: choosing random victim\n");

    for (i=0, reg=0 ; 
	 i<SEARCH_LIMIT && reg==0 ; 
	 i++) {

	// cycle counter used as pseudorandom number generator
	rdtscll(thetime);
	
	reg = &(map->base_regions[thetime % num_base_regions]);

	if (reg->flags.swapped || reg->flags.pinned) { 
	    // region is already swapped or is pinned - try again
	    reg = 0;
	} 
    }

    if (!reg) { 
	PrintError(vm,VCORE_NONE,"swapper: Unable to find a random victim\n");
    } else {
	PrintDebug(vm,VCORE_NONE,"swapper: Random victim GPA=%p to %p\n", (void*)reg->guest_start, (void*)reg->guest_end);
    }
    
    return reg;
}


// Must be called with the lock held
static struct v3_mem_region * choose_next_victim(struct v3_vm_info * vm) 
{
    struct v3_mem_map * map = &(vm->mem_map);
    uint64_t num_base_regions = map->num_base_regions;
    struct v3_mem_region *reg=0;
    uint32_t i=0;
	
    PrintDebug(vm, VCORE_NONE, "swapper: choosing next victim\n");

    // forward to end
    for (i=vm->swap_state.last_region_used+1, reg=0; 
	 i<num_base_regions && reg==0; 
	 i++) {

	reg = &(map->base_regions[i]);

	if (reg->flags.swapped || reg->flags.pinned) { 
	    // region is already swapped or is pinned - try again
	    reg = 0;
	} 
    }

    for (i=0; 
	 i < vm->swap_state.last_region_used+1 && reg==0;
	 i++) { 
	
	reg = &(map->base_regions[i]);

	if (reg->flags.swapped || reg->flags.pinned) { 
	    // region is already swapped or is pinned - try again
	    reg = 0;
	} 
    }

    if (!reg) { 
	PrintError(vm,VCORE_NONE,"swapper: Unable to find the next victim\n");
    } else {
	PrintDebug(vm,VCORE_NONE,"swapper: Next victim GPA=%p to %p\n", (void*)reg->guest_start, (void*)reg->guest_end);
    }
    
    return reg;
}

// Must be called with the lock held
static struct v3_mem_region * choose_lru_victim(struct v3_vm_info * vm) 
{
    struct v3_mem_map * map = &(vm->mem_map);
    uint64_t num_base_regions = map->num_base_regions;
    struct v3_mem_region *reg=0;
    struct v3_mem_region *oldest_reg=0;
    uint32_t i=0;
    uint64_t oldest_time;
	
    PrintDebug(vm, VCORE_NONE, "swapper: choosing pseudo-lru victim\n");


    for (i=0, oldest_time=0, oldest_reg=0;
	 i<num_base_regions; 
	 i++) {

	reg = &(map->base_regions[i]);

	if (reg->flags.swapped || reg->flags.pinned) {
	    if (!oldest_reg ||
		reg->swap_state.last_accessed < oldest_time) { 

		oldest_time = reg->swap_state.last_accessed;
		oldest_reg = reg;
	    }
	}
    }

    if (!oldest_reg) { 
	PrintError(vm,VCORE_NONE,"swapper: Unable to find pseudo-lru victim\n");
    } else {
	PrintDebug(vm,VCORE_NONE,"swapper: Pseudo-lru victim GPA=%p to %p\n", (void*)oldest_reg->guest_start, (void*)oldest_reg->guest_end);
    }
    
    return oldest_reg;
}


// Must be called with the lock held
static struct v3_mem_region * choose_victim(struct v3_vm_info * vm) 
{
    switch (vm->swap_state.strategy) { 
	case V3_SWAP_NEXT_FIT:
	    return choose_next_victim(vm);
	    break;
	case V3_SWAP_RANDOM:
	    return choose_random_victim(vm);
	    break;
	case V3_SWAP_LRU:
	    return choose_lru_victim(vm);
	    break;
	default:
	    return choose_random_victim(vm);
	    break;
    }
}


// swaps out region, and marks it as swapped and pinned
// no lock should be held
static int swap_out_region_internal(struct v3_vm_info *vm, struct v3_mem_region *victim, int ignore_pinning)
{
    unsigned int flags;
    int i; 
    int fail=0;

    flags = v3_lock_irqsave(vm->swap_state.lock);

    if (victim->flags.swapped) { 
	v3_unlock_irqrestore(vm->swap_state.lock,flags);
	PrintDebug(vm,VCORE_NONE,"swapper: swap out already swapped out region\n");
	return 0;
    }
    
    if (!ignore_pinning && victim->flags.pinned) { 
	v3_unlock_irqrestore(vm->swap_state.lock,flags);
	PrintError(vm,VCORE_NONE,"swapper: attempt to swap out pinned region\n");
	return -1;
    }

    // now mark it as pinned until we are done with it.
    victim->flags.pinned=1;

    // release lock - it's marked pinned so nothing else will touch it
    v3_unlock_irqrestore(vm->swap_state.lock,flags);
    
    // do NOT do this without irqs on... 
    if (write_all(vm->swap_state.swapfd, 
		  (uint8_t *)V3_VAddr((void *)victim->host_addr), 
		  victim->guest_end - victim->guest_start, 
		  victim->guest_start)) {
	PrintError(vm, VCORE_NONE, "swapper: failed to swap out victim"); //write victim to disk
	// note write only here - it returns unswapped and unpinned
	victim->flags.pinned=0;
	return -1;
    }

    // Now invalidate it
    
    //Invalidate the victim on all cores
    
    for (i=0, fail=0; i<vm->num_cores;i++ ) {
	struct guest_info * core = &(vm->cores[i]);
	int rc=0;

	if (core->shdw_pg_mode == SHADOW_PAGING) {
	    v3_mem_mode_t mem_mode = v3_get_vm_mem_mode(core);
	    if (mem_mode == PHYSICAL_MEM) {
		PrintDebug(vm, VCORE_NONE, "swapper: v3_invalidate_passthrough_addr_range() called for core %d",i);
		rc = v3_invalidate_passthrough_addr_range(core, victim->guest_start,  victim->guest_end-1,NULL,NULL ); 
	    } else {
		PrintDebug(vm, VCORE_NONE, "swapper: v3_invalidate_shadow_pts() called for core %d",i);
		rc = v3_invalidate_shadow_pts(core);
	    }
	} else if (core->shdw_pg_mode == NESTED_PAGING) {
	    PrintDebug(vm, VCORE_NONE, "swapper: v3_invalidate_nested_addr_range() called for core %d",i);
	    rc = v3_invalidate_nested_addr_range(core,  victim->guest_start,  victim->guest_end-1,NULL,NULL );
        }

	if (rc) { 
	    PrintError(vm,VCORE_NONE,"swapper: paging invalidation failed for victim on core %d.... continuing, but this is not good.\n", i);
	    fail=1;
	}
    }

    victim->flags.swapped=1;   // now it is in "swapped + pinned" state, meaning it has been written and is now holding for future use
    
    if (fail) { 
	return -1;
    } else {
	return 0;
    }
}


// swaps out region, and marks it as swapped
int v3_swap_out_region(struct v3_vm_info *vm, struct v3_mem_region *victim)
{
    if (!victim->flags.base) { 
	PrintError(vm, VCORE_NONE,"swapper: can only swap out base regions\n");
	return -1;
    }

    if (victim->flags.pinned) { 
	PrintError(vm, VCORE_NONE,"swapper: cannot swap out a pinned region\n");
	return -1;
    }
    
    if (swap_out_region_internal(vm,victim,0)) { 
	PrintError(vm, VCORE_NONE,"swapper: failed to swap out victim....  bad\n");
	return -1;
    }

    // victim now has its old info, and is marked swapped and pinned

    victim->host_addr = 0;
    victim->flags.pinned = 0;  

    // now is simply swapped
    
    return 0;
}


int v3_swap_in_region(struct v3_vm_info *vm, struct v3_mem_region *perp)
{
    unsigned int flags;
    struct v3_mem_region *victim;

    flags = v3_lock_irqsave(vm->swap_state.lock);

    if (!perp->flags.base) { 
	v3_unlock_irqrestore(vm->swap_state.lock,flags);
	PrintError(vm,VCORE_NONE,"swapper: can only swap in base regions\n");
	return -1;
    }

    if (!perp->flags.swapped) { 
	v3_unlock_irqrestore(vm->swap_state.lock,flags);
	PrintDebug(vm,VCORE_NONE,"swapper: region is already swapped in\n");
	return 0;
    }

    // while still holding the lock, we will pin it to make sure no one 
    // else will attempt to swap in a race with us
    perp->flags.pinned=1;
    
    victim = choose_victim(vm);

    if (!victim) { 
	perp->flags.pinned=0;  // leave perp swapped 
	v3_unlock_irqrestore(vm->swap_state.lock,flags);
	PrintError(vm,VCORE_NONE,"swapper: cannot find victim\n");
	return -1;
    }

    victim->flags.pinned=1;


    // update the next fit info
    // pointer arith in units of relevant structs... 
    vm->swap_state.last_region_used = (victim - &(vm->mem_map.base_regions[0])); 
    

    // Now we hold both the perp and the victim (pinned)
    // and so can release the lcok
    v3_unlock_irqrestore(vm->swap_state.lock,flags);

    
    if (swap_out_region_internal(vm,victim,1)) {  // ignore that the victim is marked pinned
	PrintError(vm, VCORE_NONE,"swapper: failed to swap out victim....  bad\n");
	return -1;
    }

    // victim is still marked pinned

    // mug the victim
    perp->host_addr = victim->host_addr;
    victim->host_addr = 0;
    // and we're done, so release it
    victim->flags.swapped=1;
    victim->flags.pinned=0;


    // Now swap in the perp
    
    if (read_all(vm->swap_state.swapfd, 
		 (uint8_t *)V3_VAddr((void *)perp->host_addr), 
		 perp->guest_end - perp->guest_start, 
		 perp->guest_start)) {
	
	PrintError(vm, VCORE_NONE, "swapper: swap in of region failed!\n"); 
	// leave it swapped, but unpin the memory... 
	perp->flags.pinned = 0; 
	return -1;
    } else {
	perp->flags.swapped = 0;  // perp is now OK, so release it
	perp->flags.pinned = 0; 
	vm->swap_state.swap_count++;
	return 0;
    }
}



void v3_touch_region(struct v3_vm_info *vm, struct v3_mem_region *region)
{
    // should be uniform host time, not per core...
    rdtscll(region->swap_state.last_accessed);
}


