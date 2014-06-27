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
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_mem_track.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_time.h>


#ifndef V3_CONFIG_DEBUG_MEM_TRACK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))


// This should be identical across cores, but this
// implementation surely is not
static uint64_t host_time()
{
    return v3_get_host_time(NULL);
}

static uint8_t *alloc_bitmap(struct v3_vm_info *vm)
{
    uint8_t *b;
    
    if (!(b =  V3_Malloc(CEIL_DIV(CEIL_DIV(vm->mem_size,PAGE_SIZE_4KB),8)))) {
	return NULL;
    }

    return b;
}


static void free_bitmap(uint8_t *b)
{
    if (b) { 
	V3_Free(b);
    }

}

int v3_mem_track_deinit(struct v3_vm_info *vm)
{
    int i;

    for (i=0;i<vm->num_cores;i++) {
	free_bitmap(vm->cores[i].memtrack_state.access_bitmap);
	memset(&(vm->cores[i].memtrack_state),0,sizeof(struct v3_core_mem_track));
    }

    PrintDebug(vm,VCORE_NONE,"Memory tracking deinitialized\n");

    return 0;
}

int v3_mem_track_init(struct v3_vm_info *vm)
{
    int i;


    memset(&(vm->memtrack_state),0,sizeof(struct v3_vm_mem_track));

    for (i=0;i<vm->num_cores;i++) {
	memset(&(vm->cores[i].memtrack_state),0,sizeof(struct v3_core_mem_track));
	vm->cores[i].memtrack_state.num_pages=CEIL_DIV(vm->mem_size,PAGE_SIZE_4KB);
	if (!(vm->cores[i].memtrack_state.access_bitmap = alloc_bitmap(vm))) {
	    PrintError(vm,VCORE_NONE,"Unable to allocate for memory tracking\n");
	    v3_mem_track_deinit(vm);
	    return -1;
	}
    }

    PrintDebug(vm,VCORE_NONE,"Memory tracking initialized\n");

    return 0;
}



//
// Note use of old-style callbacks here
//
static int shadow_paging_callback(struct guest_info *core, 
				  struct v3_shdw_pg_event *event,
				  void      *priv_data)
{
    

    if (event->event_type==SHADOW_PAGEFAULT &&
	event->event_order==SHADOW_POSTIMPL) {

	addr_t gpa;

	PrintDebug(core->vm_info,core,"Memory tracking: shadow callback gva=%p\n",(void*)event->gva);

	if (!v3_gva_to_gpa(core,event->gva,&gpa)) {
	    // note the assumption here that it is for a 4KB page... 
	    PrintDebug(core->vm_info,core,"Memory tracking: shadow callback corresponding gpa=%p\n",(void*)gpa);
	    SET_BIT(core->memtrack_state.access_bitmap,gpa/PAGE_SIZE_4KB);
	} else {
	    // no worries, this isn't physical memory
	}
    } else {
	// we don't care about other events
    }
    
    return 0;
}


static int passthrough_paging_callback(struct guest_info *core, 
				       struct v3_passthrough_pg_event *event,
				       void      *priv_data)
{
    uint64_t page_start, page_end, page;
    

    if (event->event_type==PASSTHROUGH_PAGEFAULT &&
	event->event_order==PASSTHROUGH_POSTIMPL) {

	PrintDebug(core->vm_info,core,"Memory tracking: passthrough callback gpa=%p..%p\n",(void*)event->gpa_start,(void*)event->gpa_end);

	page_start = event->gpa_start/PAGE_SIZE_4KB;
	page_end = event->gpa_end/PAGE_SIZE_4KB;
	
	for (page=page_start; page<=page_end;page++) { 
	    SET_BIT(core->memtrack_state.access_bitmap,page);
	}
    } else {
	// we don't care about other events
    }
    
    return 0;
}

static int nested_paging_callback(struct guest_info *core, 
				  struct v3_nested_pg_event *event,
				  void      *priv_data)
{
    uint64_t page_start, page_end, page;

    
    if (event->event_type==NESTED_PAGEFAULT &&
	event->event_order==NESTED_POSTIMPL) {

	PrintDebug(core->vm_info,core,"Memory tracking: nested callback gpa=%p..%p\n",(void*)event->gpa_start,(void*)event->gpa_end);

	page_start = event->gpa_start/PAGE_SIZE_4KB;
	page_end = event->gpa_end/PAGE_SIZE_4KB;
	
	for (page=page_start; page<=page_end;page++) { 
	    SET_BIT(core->memtrack_state.access_bitmap,page);
	}
    } else {
	// we don't care about other events
    }
    
    return 0;
}




static void restart(struct guest_info *core)
{

    core->memtrack_state.start_time=host_time();

    PrintDebug(core->vm_info,core,"memtrack: restart at %llu\n",core->memtrack_state.start_time);

    memset(core->memtrack_state.access_bitmap,0,CEIL_DIV(core->memtrack_state.num_pages,8));

    if (core->shdw_pg_mode==SHADOW_PAGING) { 
	v3_invalidate_shadow_pts(core);
	v3_invalidate_passthrough_addr_range(core,0,core->vm_info->mem_size,NULL,NULL);
    } else if (core->shdw_pg_mode==NESTED_PAGING) { 
	v3_invalidate_nested_addr_range(core,0,core->vm_info->mem_size,NULL,NULL);
    }
    
    PrintDebug(core->vm_info,core,"memtrack: restart complete at %llu\n",host_time());
}

int v3_mem_track_start(struct v3_vm_info *vm, v3_mem_track_access_t access, v3_mem_track_reset_t reset, uint64_t period)
{
    int i;
    int unwind=0;


    PrintDebug(vm,VCORE_NONE,"Memory tracking: start access=0x%x, reset=0x%x, period=%llu\n",
	       access,reset,period);

    if (vm->memtrack_state.started) { 
	PrintError(vm,VCORE_NONE,"Memory tracking already started!\n");
	return -1;
    }

    if (access != V3_MEM_TRACK_ACCESS) { 
	PrintError(vm,VCORE_NONE,"Unsupported access mode\n");
	return -1;
    }

    vm->memtrack_state.access_type=access;
    vm->memtrack_state.reset_type=reset;
    vm->memtrack_state.period=period;

    vm->memtrack_state.started=1;

    for (i=0;i<vm->num_cores;i++) {
	if (vm->cores[i].shdw_pg_mode==SHADOW_PAGING) { 
	    if (v3_register_shadow_paging_event_callback(vm,shadow_paging_callback,NULL)) { 
		PrintError(vm,VCORE_NONE,"Mem track cannot register for shadow paging event\n");
		unwind=i+1;
		goto fail;
	    }

	    if (v3_register_passthrough_paging_event_callback(vm,passthrough_paging_callback,NULL)) { 
		PrintError(vm,VCORE_NONE,"Mem track cannot register for passthrough paging event\n");
		unwind=i+1;
		goto fail;
	    }
	} else if (vm->cores[i].shdw_pg_mode==NESTED_PAGING) { 
	    if (v3_register_nested_paging_event_callback(vm,nested_paging_callback,NULL)) { 
		PrintError(vm,VCORE_NONE,"Mem track cannot register for nested paging event\n");
		unwind=i+1;
		goto fail;
	    }
	}
	restart(&vm->cores[i]);
    }
    
    return 0;

 fail:

    for (i=0;i<unwind;i++) {
	if (vm->cores[i].shdw_pg_mode==SHADOW_PAGING) { 
	    v3_unregister_shadow_paging_event_callback(vm,shadow_paging_callback,NULL);
	    v3_unregister_passthrough_paging_event_callback(vm,passthrough_paging_callback,NULL);
	} else if (vm->cores[0].shdw_pg_mode==NESTED_PAGING) { 
	    v3_unregister_nested_paging_event_callback(vm,nested_paging_callback,NULL);
	}
    }
    
    return -1;

}

int v3_mem_track_stop(struct v3_vm_info *vm)
{
    int i;

    PrintDebug(vm,VCORE_NONE,"Memory tracking: stop\n");

    if (!vm->memtrack_state.started) { 
	PrintError(vm, VCORE_NONE, "Memory tracking was not started!\n");
	return -1;
    }

    vm->memtrack_state.started=0;

    for (i=0;i<vm->num_cores;i++) {
	if (vm->cores[i].shdw_pg_mode==SHADOW_PAGING) { 
	    v3_unregister_shadow_paging_event_callback(vm,shadow_paging_callback,NULL);
	    v3_unregister_passthrough_paging_event_callback(vm,passthrough_paging_callback,NULL);
	} else if (vm->cores[0].shdw_pg_mode==NESTED_PAGING) { 
	    v3_unregister_nested_paging_event_callback(vm,nested_paging_callback,NULL);
	}
    }
    
    return 0;

}

void v3_mem_track_free_snapshot(v3_mem_track_snapshot *s)
{
    int i;

    PrintDebug(VM_NONE,VCORE_NONE,"Memory tracking: free snapshot %p\n",s);

    if (s) {
	for (i=0;i<s->num_cores;i++) {
	    free_bitmap(s->core[i].access_bitmap);
	}
	V3_Free(s);
    }
}

v3_mem_track_snapshot *v3_mem_track_take_snapshot(struct v3_vm_info *vm)
{
    int i;
    v3_mem_track_snapshot *s;

    PrintDebug(vm,VCORE_NONE,"Memory tracking: take snapshot\n");

    s = V3_Malloc(sizeof(v3_mem_track_snapshot) + sizeof(struct v3_core_mem_track) * vm->num_cores);
    
    if (!s) { 
	PrintError(vm,VCORE_NONE,"Cannot allocate memory for memory tracking snapshot\n");
	return NULL;
    }

    memset(s,0,sizeof(v3_mem_track_snapshot) + sizeof(struct v3_core_mem_track) * vm->num_cores);
    
    for (i=0;i<vm->num_cores;i++) {
	if (!(s->core[i].access_bitmap = alloc_bitmap(vm))) { 
	    PrintError(vm,VCORE_NONE,"Unable to allocate for memory tracking snapshot\n");
	    v3_mem_track_free_snapshot(s);
	    return NULL;
	}
    }

    s->access_type=vm->memtrack_state.access_type;
    s->reset_type=vm->memtrack_state.reset_type;
    s->period=vm->memtrack_state.period;
    s->num_cores=vm->num_cores;
    
    for (i=0;i<vm->num_cores;i++) { 
	s->core[i].start_time=vm->cores[i].memtrack_state.start_time;
	s->core[i].end_time=host_time(); // now - note, should not race...
	s->core[i].num_pages=vm->cores[i].memtrack_state.num_pages;
	memcpy(s->core[i].access_bitmap,vm->cores[i].memtrack_state.access_bitmap,CEIL_DIV(vm->cores[i].memtrack_state.num_pages,8));
	PrintDebug(vm,VCORE_NONE,"memtrack: copied %llu bytes\n",CEIL_DIV(vm->cores[i].memtrack_state.num_pages,8));
	uint64_t j, sum;
	sum=0;
	for (j=0;j<CEIL_DIV(vm->cores[i].memtrack_state.num_pages,8);j++) {
	    sum+=!!vm->cores[i].memtrack_state.access_bitmap[i];
	}
	PrintDebug(vm,VCORE_NONE,"memtrack: have %llu nonzero bytes\n",sum);
    }
    
    return s;
}


int v3_mem_track_get_sizes(struct v3_vm_info *vm, uint64_t *num_cores, uint64_t *num_pages)
{
    *num_cores = vm->num_cores;
    *num_pages = vm->mem_size / PAGE_SIZE_4KB;
    
    return 0;
}
    

// Called only in the core thread context
void v3_mem_track_entry(struct guest_info *core)
{
    struct v3_vm_info *vm = core->vm_info;
    uint64_t ht = host_time();

    if (vm->memtrack_state.started) { 
	if ((ht - core->memtrack_state.start_time) >= vm->memtrack_state.period) { 
	    // drive periodic if needed
	    PrintDebug(core->vm_info, core, "memtrack: start_time=%llu, period=%llu,  host_time=%llu, diff=%llu\n",
		       core->memtrack_state.start_time, vm->memtrack_state.period, ht, ht-core->memtrack_state.start_time);

	    if (vm->memtrack_state.reset_type==V3_MEM_TRACK_PERIODIC) { 
		restart(core);
	    } else {
		v3_mem_track_stop(core->vm_info);
	    }
	}
    }

}

// Called only in the core thread context
void v3_mem_track_exit(struct guest_info *core)
{
    // nothing yet
}
