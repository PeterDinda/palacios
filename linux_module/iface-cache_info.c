/*
 * Palacios cache information interface
 *
 *
 * (c) Peter Dinda, 2015
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>

#include "palacios.h"
#include "util-hashtable.h"
#include "linux-exts.h"
#include "vm.h"

#define sint64_t int64_t

#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/inet.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/syscalls.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/slab.h>

#include <palacios/vmm.h>
#include <interfaces/vmm_cache_info.h>


/*
  This is a simple implementation of the Palacios cache info 
  
*/


static inline void cpuid_string(u32 id, u32 dest[4]) {
  asm volatile("cpuid"
	       :"=a"(*dest),"=b"(*(dest+1)),"=c"(*(dest+2)),"=d"(*(dest+3))
	       :"a"(id));
}


static int get_cpu_vendor(char name[13])
{
  u32 dest[4];
  u32 maxid;

  cpuid_string(0,dest);
  maxid=dest[0];
  ((u32*)name)[0]=dest[1];
  ((u32*)name)[1]=dest[3];
  ((u32*)name)[2]=dest[2];
  name[12]=0;
   
  return maxid;
}

static int is_intel(void)
{
  char name[13];
  get_cpu_vendor(name);
  return !strcmp(name,"GenuineIntel");
}

static int is_amd(void)
{
  char name[13];
  get_cpu_vendor(name);
  return !strcmp(name,"AuthenticAMD");
}

static uint32_t decode_amd_l2l3_assoc(uint32_t val)
{
    switch (val) {
	case 0:
	case 1:
	case 2:
	case 4:
	    return val;
	case 6:
	    return 8;
	case 8:
	    return 16;
	case 0xa:
	    return 32;
	case 0xb:
	    return 48;
	case 0xc:
	    return 64;
	case 0xd:
	    return 96;
	case 0xe:
	    return 128;
	case 0xf:
	    return (uint32_t)-1;
	default:
	    ERROR("Unknown associativity encoding %x\n",val);
	    return 0;
    }
}

static int get_cache_level_amd_legacy(v3_cache_type_t type, uint32_t level, struct v3_cache_info *c)
{
    uint32_t eax, ebx, ecx, edx;
    uint32_t l1_dtlb_24_assoc;
    uint32_t l1_dtlb_24_entries;
    uint32_t l1_itlb_24_assoc;
    uint32_t l1_itlb_24_entries;
    uint32_t l1_dtlb_4k_assoc;
    uint32_t l1_dtlb_4k_entries;
    uint32_t l1_itlb_4k_assoc;
    uint32_t l1_itlb_4k_entries;
    uint32_t l1_dcache_size;
    uint32_t l1_dcache_assoc;
    uint32_t l1_dcache_linespertag;
    uint32_t l1_dcache_linesize;
    uint32_t l1_icache_size;
    uint32_t l1_icache_assoc;
    uint32_t l1_icache_linespertag;
    uint32_t l1_icache_linesize;
    uint32_t l2_dtlb_24_assoc;
    uint32_t l2_dtlb_24_entries;
    uint32_t l2_itlb_24_assoc;
    uint32_t l2_itlb_24_entries;
    uint32_t l2_dtlb_4k_assoc;
    uint32_t l2_dtlb_4k_entries;
    uint32_t l2_itlb_4k_assoc;
    uint32_t l2_itlb_4k_entries;
    uint32_t l2_cache_size;
    uint32_t l2_cache_assoc;
    uint32_t l2_cache_linespertag;
    uint32_t l2_cache_linesize;
    uint32_t l3_cache_size;
    uint32_t l3_cache_assoc;
    uint32_t l3_cache_linespertag;
    uint32_t l3_cache_linesize;

    // L1 caches and tlbs
    cpuid(0x80000005,&eax,&ebx,&ecx,&edx);

    l1_dtlb_24_assoc = (eax >> 24) & 0xff;
    l1_dtlb_24_entries = (eax >> 16) & 0xff;
    l1_itlb_24_assoc = (eax >> 8) & 0xff;
    l1_itlb_24_entries = (eax) & 0xff;
    l1_dtlb_4k_assoc = (ebx >> 24) & 0xff;
    l1_dtlb_4k_entries = (ebx >> 16) & 0xff;
    l1_itlb_4k_assoc = (ebx >> 8) & 0xff;
    l1_itlb_4k_entries = (ebx) & 0xff;
    l1_dcache_size = ((ecx >> 24) & 0xff) * 1024;
    l1_dcache_assoc = (ecx >> 16) & 0xff;
    l1_dcache_linespertag = (ecx >> 8) & 0xff;
    l1_dcache_linesize = ((ecx) & 0xff) * l1_dcache_linespertag;
    l1_icache_size = ((edx >> 24) & 0xff) * 1024;
    l1_icache_assoc = (edx >> 16) & 0xff;
    l1_icache_linespertag = (edx >> 8) & 0xff;
    l1_icache_linesize = ((edx) & 0xff) * l1_icache_linespertag;


    // L2 caches and tlbs plus L3 cache
    cpuid(0x80000006,&eax,&ebx,&ecx,&edx);

    l2_dtlb_24_assoc = decode_amd_l2l3_assoc((eax >> 28) & 0xf);
    l2_dtlb_24_entries = (eax >> 16) & 0xfff;
    l2_itlb_24_assoc = decode_amd_l2l3_assoc((eax >> 12) & 0xf);
    l2_itlb_24_entries = (eax) & 0xfff;
    l2_dtlb_4k_assoc = decode_amd_l2l3_assoc((ebx >> 28) & 0xf);
    l2_dtlb_4k_entries = (ebx >> 16) & 0xfff;
    l2_itlb_4k_assoc = decode_amd_l2l3_assoc((ebx >> 12) & 0xf);
    l2_itlb_4k_entries = (ebx) & 0xfff;
    l2_cache_size = ((ecx >> 16) & 0xffff) * 1024;
    l2_cache_assoc = decode_amd_l2l3_assoc((ecx >> 12) & 0xf);
    l2_cache_linespertag = (ecx >> 8) & 0xf;
    l2_cache_linesize = ((ecx) & 0xff) * l1_dcache_linespertag;
    l3_cache_size = ((edx >> 18) & 0x3fff) * 1024 * 512;
    l3_cache_assoc = decode_amd_l2l3_assoc((edx >> 12) & 0xf);
    l3_cache_linespertag = (edx >> 8) & 0xf;
    l3_cache_linesize = ((edx) & 0xff) * l3_cache_linespertag;
    
    
    INFO("L1 ITLB: 2/4MB: %u assoc, %u entries; 4KB: %u assoc %u entries\n",
	 l1_itlb_24_assoc,l1_itlb_24_entries,l1_itlb_4k_assoc,l1_itlb_4k_entries);
    INFO("L2 ITLB: 2/4MB: %u assoc, %u entries; 4KB: %u assoc %u entries\n",
	 l2_itlb_24_assoc,l2_itlb_24_entries,l2_itlb_4k_assoc,l2_itlb_4k_entries);
    
    INFO("L1 DTLB: 2/4MB: %u assoc, %u entries; 4KB: %u assoc %u entries\n",
	 l1_dtlb_24_assoc,l1_dtlb_24_entries,l1_dtlb_4k_assoc,l1_dtlb_4k_entries);
    INFO("L2 DTLB: 2/4MB: %u assoc, %u entries; 4KB: %u assoc %u entries\n",
	 l2_dtlb_24_assoc,l2_dtlb_24_entries,l2_dtlb_4k_assoc,l2_dtlb_4k_entries);
    
    INFO("L1 ICACHE: %u size, %u assoc, %u linesize %u linespertag\n",
	 l1_icache_size,l1_icache_assoc,l1_icache_linesize,l1_icache_linespertag);
    
    INFO("L1 DCACHE: %u size, %u assoc, %u linesize %u linespertag\n",
	 l1_dcache_size,l1_dcache_assoc,l1_dcache_linesize,l1_dcache_linespertag);
    
    INFO("L2 CACHE: %u size, %u assoc, %u linesize %u linespertag\n",
	 l2_cache_size,l2_cache_assoc,l2_cache_linesize,l2_cache_linespertag);
    
    INFO("L3 CACHE: %u size, %u assoc, %u linesize %u linespertag\n",
	 l3_cache_size,l3_cache_assoc,l3_cache_linesize,l3_cache_linespertag);
    
    if (!c) { 
	// debug
	return 0;
    }

    c->type=type;
    c->level=level;
    c->blocksize=0;
    c->associativity=0; // does not exist unless we say otherwise
    
    switch (type) {

	case V3_CACHE_CODE: 
	    if (level==1) { 
		c->size = l1_icache_size;
		c->blocksize = l1_icache_linesize;
		c->associativity = l1_icache_assoc == 0xff ? -1 : l1_icache_assoc;
	    } 		
	    break;

	case V3_CACHE_DATA: 
	    if (level==1) { 
		c->size = l1_dcache_size;
		c->blocksize = l1_dcache_linesize;
		c->associativity = l1_dcache_assoc == 0xff ? -1 : l1_dcache_assoc;
	    } 
	    break;
	    
	case V3_CACHE_COMBINED: 
	    if (level==2) { 
		c->size = l2_cache_size;
		c->blocksize = l2_cache_linesize;
		c->associativity = l2_cache_assoc;
	    } else if (level==3) { 
		c->size = l3_cache_size;
		c->blocksize = l3_cache_linesize;
		c->associativity = l3_cache_assoc;
	    } else if (level==-1) { 
		// find highest level combined cache that is enabled
		if (l3_cache_assoc) { 
		    c->size = l3_cache_size;
		    c->blocksize = l3_cache_linesize;
		    c->associativity = l3_cache_assoc;
		} else {
		    c->size = l2_cache_size;
		    c->blocksize = l2_cache_linesize;
		    c->associativity = l2_cache_assoc;
		}
	    }
	    break;

	case V3_TLB_CODE: 
	    if (level==1) { 
		c->size = l1_itlb_4k_entries;
		c->associativity = l1_itlb_4k_assoc == 0xff ? -1 : l1_itlb_4k_assoc;
	    } else if (level==2) { 
		c->size = l2_itlb_4k_entries;
		c->associativity = l2_itlb_4k_assoc;
	    }
	    break;
	    
	case V3_TLB_DATA: 
	    if (level==1) { 
		c->size = l1_dtlb_4k_entries;
		c->associativity = l1_dtlb_4k_assoc == 0xff ? -1 : l1_dtlb_4k_assoc;
	    } else if (level==2) { 
		c->size = l2_dtlb_4k_entries;
		c->associativity = l2_dtlb_4k_assoc;
	    }
	    break;
	
	case V3_TLB_COMBINED: 
	    // no combined TLB exposed on this machine;
	    break;
	    
	default:
	    ERROR("Don't know how to handle cache info request type %x\n",type);
	    return -1;
    }

    return 0;
}


static int get_cache_level_amd(v3_cache_type_t type, uint32_t level, struct v3_cache_info *c)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(0x80000000,&eax,&ebx,&ecx,&edx);

    if (eax < 0x80000006) { 
	ERROR("AMD processor does not support even legacy cache info\n");
	return -1;
    }

    cpuid(0x80000001,&eax,&ebx,&ecx,&edx);

    if ((ecx >> 22) & 0x1) {
	INFO("AMD Processor has Cache Topology Support - Legacy results may be inaccurate\n");
    }

    return get_cache_level_amd_legacy(type,level,c);
}

#define INTEL_MAX_CACHE 256

static int get_cache_level_intel_det(v3_cache_type_t type, uint32_t level, struct v3_cache_info *c)
{
    uint32_t i;
    uint32_t eax, ebx, ecx, edx;
    uint32_t ctype, clevel, cassoc, cparts, csets, clinesize, csize;

    if (type==V3_TLB_CODE || type==V3_TLB_DATA || type==V3_TLB_COMBINED) { 
	ERROR("TLB query unsupported on Intel\n");
	return -1;
    }

    if (c) { 
	c->type = type;
	c->level = 0;  // max level found so far
    }
    
    for (i=0;i<INTEL_MAX_CACHE;i++) {

	cpuid_count(4,i,&eax,&ebx,&ecx,&edx);

	ctype = eax & 0x1f;

	if (!ctype) { 
	    // no more caches
	    break;
	}

	clevel = (eax >> 5) & 0x7;
	cassoc = eax & 0x200 ? -1 : ((ebx>>22) & 0x3ff) + 1 ;
	cparts = ((ebx >> 12) & 0x3ff) + 1;
	clinesize = (ebx & 0xfff) + 1;
	csets = ecx + 1;
	csize = cassoc * cparts * clinesize * csets;

	INFO("Cache: index %u type %u level %u assoc %u parts %u linesize %u sets %u size %u\n",
	     i,ctype,clevel,cassoc,cparts,clinesize,csets,csize);

	if (c &&
	    (((ctype==1 && type==V3_CACHE_DATA) ||
	      (ctype==2 && type==V3_CACHE_CODE) ||
	      (ctype==3 && type==V3_CACHE_COMBINED)) &&
	     ((clevel==level) || 
	      (level==-1 && clevel>c->level)))) { 
	    
	    c->level = clevel;
	    c->size = csize;
	    c->blocksize = clinesize;
	    c->associativity = cassoc;
	} 
    }

    if (i==INTEL_MAX_CACHE) { 
	return -1;
    } else {
	return 0;
    }
}


static int get_cache_level_intel(v3_cache_type_t type, uint32_t level, struct v3_cache_info *c)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(0,&eax,&ebx,&ecx,&edx);
    
    if (eax < 4) { 
	ERROR("Intel Processor does not support deterministic cache parameters function\n");
	return -1;
    }

    return get_cache_level_intel_det(type,level,c);
}

static int get_cache_level(v3_cache_type_t type, uint32_t level, struct v3_cache_info *c)
{
    if (is_amd()) { 
	return get_cache_level_amd(type,level,c);
    } else if (is_intel()) { 
	return get_cache_level_intel(type,level,c);
    } else {
	ERROR("Cannot get cache information for unknown architecture\n");
	return -1;
    }
	
	
}


/***************************************************************************************************
  Hooks to palacios and inititialization
*************************************************************************************************/

    
static struct v3_cache_info_iface hooks = {
    .get_cache_level = get_cache_level,
};


static int init_cache_info(void)
{

    // just to see what's there - this should enumerate all
    // and fail immediately otherwise
    if (get_cache_level(-1,0,0)) { 
	ERROR("Cannot intialized cache information\n");
	return -1;
    }

    V3_Init_Cache_Info(&hooks);

    INFO("cache_info inited\n");

    return 0;

}

static int deinit_cache_info(void)
{
    INFO("cache_info deinited\n");

    return 0;
}





static struct linux_ext cache_info_ext = {
    .name = "CACHE_INFO_INTERFACE",
    .init = init_cache_info,
    .deinit = deinit_cache_info,
};


register_extension(&cache_info_ext);
