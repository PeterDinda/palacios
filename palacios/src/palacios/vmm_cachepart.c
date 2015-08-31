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

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <interfaces/vmm_cache_info.h>

#ifndef V3_CONFIG_DEBUG_CACHEPART
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

/*

  <cachepart 
        block_size=BLOCK_SIZE 
        num_colors=NUM_COLORS
        min_color=MIN_COLOR
        max_color=MAX_COLOR />

  Cache partitioning support

  We are partioning the last level shared cache here. 

  Cache partitioning is applied to the guest physical memory allocation
  and page allocations for nested and shadow paging.   To the extent
  possible, we also try to apply this paritioning to other allocations
  and as early as possible during the creation of the VM

  BLOCK_SIZE      =  required v3_mem_block_size needed for partitioning in bytes
                     this is typically one page (4096 bytes)
  NUM_COLORS      =  the number of page colors the cache is expected to hold
                     this is used to make the min/max color fields sensible
  [MIN_COLOR,MAX_COLOR]
                  =  the range of allowed page colors allowed for this VM
                     with respect to the range [0,NUMCOLORS)
*/

static int inited=0;    
static struct v3_cache_info cache_info;

#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))
#define FLOOR_DIV(x,y) (((x)/(y)))
#define DIVIDES(x,y) (!((x)%(y)))

                 
static inline int getcacheinfo() 
{
    if (inited) {
	return 0;
    } else {
	if (v3_get_cache_info(V3_CACHE_COMBINED,0xffffffff,&cache_info)) { 
	    PrintError(VM_NONE,VCORE_NONE,"Unable to get information about cache\n");
	    return -1;
	}
	V3_Print(VM_NONE,VCORE_NONE,"cachepart: last level combined cache: 0x%x bytes, 0x%x blocksize, 0x%x associativity\n",cache_info.size,cache_info.blocksize,cache_info.associativity);
	inited=1;
	if (!(DIVIDES(cache_info.size,cache_info.blocksize) &&
	      DIVIDES(cache_info.size,cache_info.associativity) &&
	      DIVIDES(cache_info.size/cache_info.blocksize,cache_info.associativity))) {
	    PrintError(VM_NONE,VCORE_NONE,"cachepart: CACHE INFO IS NONSENSICAL\n");
	    return -1;
	}
	return 0;
    }
}

int v3_init_cachepart()
{
    PrintDebug(VM_NONE,VCORE_NONE,"cachepart: init\n");
    return 0;
}

int v3_deinit_cachepart()
{
    PrintDebug(VM_NONE,VCORE_NONE,"cachepart: deinit\n");
    return 0;
}

/*
static int bitcount(uint64_t x)
{
    int c=0;

    while (x) {
	c+=x&0x1;
	x>>=1;
    }
    return c;
}

static int is_pow2(x)
{
    int c = bitcount(x);

    return c==0 || c==1;
}
*/

static uint64_t log2(uint64_t x)
{
    uint64_t i=-1;
    
    while (x) { 
	i++;
	x>>=1;
    }

    return i;
}

static uint64_t pow2(uint64_t n)
{
    uint64_t x = 1;
    while (n) {
	x*=2;
	n--;
    }
    return x;
}

static uint64_t num_lines()
{
    return FLOOR_DIV(cache_info.size,cache_info.blocksize);
}

static uint64_t num_sets()
{
    return FLOOR_DIV(num_lines(),cache_info.associativity);
}


static void build_colors(v3_cachepart_t *c)
{
    uint64_t bo_bits, set_bits, bs_bits;
    
    // number of bits in block offset
    bo_bits = log2(cache_info.blocksize);
    set_bits = log2(num_sets());
    bs_bits = log2(c->mem_block_size);
    
    if (bs_bits<bo_bits || bs_bits >= (bo_bits+set_bits)) { 
	c->actual_num_colors = 1;
	c->color_mask=0;
	c->color_shift=0;
	c->min_color=0;
	c->max_color=0;
    } else {
	c->actual_num_colors = pow2(bo_bits+set_bits-bs_bits);
	c->color_mask = (c->actual_num_colors-1);
	c->color_shift = bs_bits;
	c->min_color = FLOOR_DIV(c->min_color,CEIL_DIV(c->expected_num_colors,c->actual_num_colors));
	c->max_color = FLOOR_DIV(c->max_color,CEIL_DIV(c->expected_num_colors,c->actual_num_colors));
    }
}

int v3_init_cachepart_vm(struct v3_vm_info *vm, struct v3_xml *config)
{
    extern uint64_t v3_mem_block_size;
    v3_cfg_tree_t *cp;
    char *block_size_s;
    char *num_colors_s;
    char *min_color_s;
    char *max_color_s;
    uint64_t req_block_size, req_num_colors, req_min_color, req_max_color;
    v3_cachepart_t *c = &(vm->cachepart_state);

    if (getcacheinfo()) { 
	PrintError(VM_NONE,VCORE_NONE,"cachepart: getcacheinfo failed!\n");
	return -1;
    }

    if (!config || !(cp=v3_cfg_subtree(config,"cachepart"))) { 
	PrintDebug(vm,VCORE_NONE,"cachepart: no cachepart configuration found\n");
	return 0;
    }
    
    if (!(block_size_s=v3_cfg_val(cp,"block_size"))) { 
	PrintError(vm,VCORE_NONE,"cachepart: missing block_size parameter\n");
	return 0;
    }

    req_block_size = atoi(block_size_s);

    if (!(num_colors_s=v3_cfg_val(cp,"num_colors"))) { 
	PrintError(vm,VCORE_NONE,"cachepart: missing num_colors parameter\n");
	return -1;
    }
    
    req_num_colors=atoi(num_colors_s);

    if (!(min_color_s=v3_cfg_val(cp,"min_color"))) { 
	PrintError(vm,VCORE_NONE,"cachepart: missing min_color parameter\n");
	return -1;
    }

    req_min_color=atoi(min_color_s);

    if (!(max_color_s=v3_cfg_val(cp,"max_color"))) { 
	PrintError(vm,VCORE_NONE,"cachepart: missing max_color parameter\n");
	return -1;
    }
    
    req_max_color=atoi(max_color_s);
	
    PrintDebug(vm,VCORE_NONE,"cachepart: request block_size=0x%llx, num_colors=0x%llx, min_color=0x%llx, max_color=0x%llx\n", req_block_size, req_num_colors, req_min_color, req_max_color);

    if (req_block_size!=v3_mem_block_size) { 
	PrintError(vm,VCORE_NONE,"cachepart: requested block size is %llu, but palacios is configured with v3_mem_block_size=%llu\n",req_block_size,v3_mem_block_size);
	return -1;
    }

    if (req_max_color>=req_num_colors || req_min_color>req_max_color) {
	PrintError(vm,VCORE_NONE,"cachepart: min/max color request is nonsensical\n");
	return -1;
    }
    
    memset(c,0,sizeof(*c));

    c->mem_block_size=req_block_size;
    c->expected_num_colors=req_num_colors;
    c->min_color=req_min_color;
    c->max_color=req_max_color;

    // Now update the color structure to reflect the cache constraints
    build_colors(c);


    PrintDebug(vm,VCORE_NONE,"cachepart: cache has 0x%llx colors so finalized to block_size=0x%llx, num_colors=0x%llx, min_color=0x%llx, max_color=0x%llx, color_shift=0x%llx, color_mask=0x%llx\n", c->actual_num_colors, c->mem_block_size, c->actual_num_colors, c->min_color, c->max_color, c->color_shift, c->color_mask);

    if (vm->resource_control.pg_filter_func || vm->resource_control.pg_filter_state) { 
	PrintError(vm,VCORE_NONE, "cachepart: paging filter functions and state are already installed...\n");
	return -1;
    }
    
    vm->resource_control.pg_filter_func = (int (*)(void *,void*)) v3_cachepart_filter;
    vm->resource_control.pg_filter_state = &vm->cachepart_state;


    //    V3_Sleep(50000000);

    return 0;
    
}

int v3_deinit_cachepart_vm(struct v3_vm_info *vm)
{
    // nothing to do
    return 0;
}

int v3_init_cachepart_core(struct guest_info *core)
{
    // nothing to do yet
    return 0;
}

int v3_deinit_cachepart_core(struct guest_info *core)
{
    // nothing to do yet
    return 0;
}

static unsigned count=0;

int v3_cachepart_filter(void *paddr, v3_cachepart_t *c)
{
    uint64_t a = (uint64_t)paddr;
    uint64_t color = (a >> c->color_shift) & c->color_mask;

    PrintDebug(VM_NONE,VCORE_NONE,"cachepart: %p is color 0x%llx required colors: [0x%llx,0x%llx] %s\n",paddr,color,c->min_color,c->max_color, color>=c->min_color && color<=c->max_color ? "ACCEPT" : "REJECT");

    /*
    if (count<10) { 
	V3_Sleep(5000000);
	count++;
    }
    */

    return color>=c->min_color && color<=c->max_color;

}

