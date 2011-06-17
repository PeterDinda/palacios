/*
 * Shadow page cache implementation that has been stolen from Linux's KVM Implementation
 * This module is licensed under the GPL
 */

#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_ctrl_regs.h>

#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>

#include <palacios/vmm_paging.h>


#ifndef V3_CONFIG_DEBUG_SHDW_CACHE
#undef PrintDebug
#define PrintDebug(fmt, ...)
#endif

#ifdef V3_CONFIG_SHADOW_CACHE

struct pde_chain {
    addr_t shadow_pdes[NR_PTE_CHAIN_ENTRIES];
    struct hlist_node link;
};

struct rmap {
    addr_t shadow_ptes[RMAP_EXT];
    struct rmap * more;
};

static inline int activate_shadow_pt_32(struct guest_info * core);
static inline unsigned shadow_page_table_hashfn(addr_t guest_fn)
{
    return guest_fn;
}

static void *shadow_cache_alloc(struct shadow_cache *mc, size_t size)
{
    void *p;
    if (!mc->nobjs) {
	PrintDebug("at shadow_cache_alloc mc->nobjs non-exist\n");
    }

    p = mc->objects[--mc->nobjs];
    memset(p, 0, size);
    return p;

}

static void shadow_cache_free(struct shadow_cache *mc, void *obj)
{
    if (mc->nobjs < NR_MEM_OBJS) {
	mc->objects[mc->nobjs++] = obj;
    }
    else V3_Free(obj);
}

static struct rmap *shadow_alloc_rmap(struct guest_info *core)
{	
    return shadow_cache_alloc(&core->shadow_rmap_cache,sizeof(struct rmap));
}

static void shadow_free_rmap(struct guest_info *core,struct rmap *rd)
{
    return shadow_cache_free(&core->shadow_rmap_cache,rd);
}

int shadow_topup_cache(struct shadow_cache * cache, size_t objsize, int min) {

    void  *obj;

    if (cache->nobjs >= min) return 0;
    while (cache->nobjs < ARRAY_SIZE(cache->objects)) {
	obj = V3_Malloc(objsize);
	if (!obj) {
	    PrintDebug("at shadow_topup_cache obj alloc fail\n");
	    return -1;
	}
    	cache->objects[cache->nobjs++] = obj;
    }
    return 0;
		
}

static int shadow_topup_caches(struct guest_info * core) {
    int r;
	
    r = shadow_topup_cache(&core->shadow_pde_chain_cache, 
		sizeof(struct pde_chain), 4);

    if (r) goto out;

    r = shadow_topup_cache(&core->shadow_rmap_cache, 
		sizeof(struct rmap), 1);

out:
	return r;
}

static struct pde_chain *shadow_alloc_pde_chain(struct guest_info *core)
{
    return shadow_cache_alloc(&core->shadow_pde_chain_cache,
		sizeof(struct pde_chain));
}

static void shadow_free_pde_chain(struct guest_info *core, struct pde_chain *pc)
{
    PrintDebug("shdw_free_pdechain: start\n");
    shadow_cache_free(&core->shadow_pde_chain_cache, pc);
    PrintDebug("shdw_free_pdechain: return\n");
}


static void shadow_free_page (struct guest_info * core, struct shadow_page_cache_data * page) 
{
    list_del(&page->link);

    V3_FreePages((void *)page->page_pa, 1);
    page->page_pa=(addr_t)V3_AllocPages(1);
	
    list_add(&page->link,&core->free_pages);
    ++core->n_free_shadow_pages;	
	
}

static struct shadow_page_cache_data * shadow_alloc_page(struct guest_info * core, addr_t shadow_pde) {

    struct shadow_page_cache_data * page;

    if (list_empty(&core->free_pages)) return NULL;

    page = list_entry(core->free_pages.next, struct shadow_page_cache_data, link);
    list_del(&page->link);

    list_add(&page->link, &core->active_shadow_pages);
    page->multimapped = 0;
    page->shadow_pde = shadow_pde;
    --core->n_free_shadow_pages;
	
    PrintDebug("alloc_page: n_free_shdw_pg %d page_pa %p page_va %p\n",
		core->n_free_shadow_pages,(void *)(page->page_pa),V3_VAddr((void *)(page->page_pa)));

    addr_t shdw_page = (addr_t)V3_VAddr((void *)(page->page_pa));
    memset((void *)shdw_page, 0, PAGE_SIZE_4KB);
	
    return page;
	
}

static void shadow_zap_page(struct guest_info * core, struct shadow_page_cache_data * page);

static void free_shadow_pages(struct guest_info * core)
{
    struct shadow_page_cache_data *page;

    while (!list_empty(&core->active_shadow_pages)) {
	page = container_of(core->active_shadow_pages.next,
				    struct shadow_page_cache_data, link);
	shadow_zap_page(core, page);
    }
	
    while (!list_empty(&core->free_pages)) {
	page = list_entry(core->free_pages.next, struct shadow_page_cache_data, link);
	list_del(&page->link);
	V3_FreePages((void *)page->page_pa, 1);
	page->page_pa = ~(addr_t)0; //invalid address
    }
}

static int alloc_shadow_pages(struct guest_info * core)
{
    int i;
    struct shadow_page_cache_data * page_header = NULL;

    for (i = 0; i < NUM_SHADOW_PAGES; i++) {
	page_header = &core->page_header_buf[i];

	INIT_LIST_HEAD(&page_header->link);
	if (!(page_header->page_pa = (addr_t)V3_AllocPages(1))) {
	    goto error_1;
	}
	addr_t shdw_page = (addr_t)V3_VAddr((void *)(page_header->page_pa));
	memset((void *)shdw_page, 0, PAGE_SIZE_4KB);

	list_add(&page_header->link, &core->free_pages);
	++core->n_free_shadow_pages;
	PrintDebug("alloc_shdw_pg: n_free_shdw_pg %d page_pa %p\n",
		core->n_free_shadow_pages,(void*)page_header->page_pa);
    }
    return 0;

error_1:
    free_shadow_pages(core);
    return -1; //out of memory

}

static void shadow_page_add_shadow_pde(struct guest_info * core, 
	struct shadow_page_cache_data * page, addr_t shadow_pde) 
{
    struct pde_chain *pde_chain;
    struct hlist_node *node;
    int i;
    addr_t old;

    if(!shadow_pde) {
	return; 
    }

    if (!page->multimapped) {
	old = page->shadow_pde;

	if(!old) {
	    page->shadow_pde = shadow_pde;
	    return;
	}

	page->multimapped = 1;
	pde_chain = shadow_alloc_pde_chain(core);
	INIT_HLIST_HEAD(&page->shadow_pdes);
	hlist_add_head(&pde_chain->link,&page->shadow_pdes);
	pde_chain->shadow_pdes[0] = old;		
    }
	
    hlist_for_each_entry(pde_chain, node, &page->shadow_pdes, link) {
	if (pde_chain->shadow_pdes[NR_PTE_CHAIN_ENTRIES-1]) continue;
	for(i=0; i < NR_PTE_CHAIN_ENTRIES; ++i)
	    if (!pde_chain->shadow_pdes[i]) {
	    	pde_chain->shadow_pdes[i] = shadow_pde;
	    	return;
	    }
	}

	pde_chain = shadow_alloc_pde_chain(core);
	//error msg
	hlist_add_head(&pde_chain->link,&page->shadow_pdes);
	pde_chain->shadow_pdes[0] = shadow_pde;
	
}

static void shadow_page_remove_shadow_pde(struct guest_info * core, 
	struct shadow_page_cache_data * page, addr_t shadow_pde) 
{

    struct pde_chain * pde_chain;
    struct hlist_node * node;
    int i;

    PrintDebug("rm_shdw_pde: multimap %d\n", page->multimapped);
    if(!page->multimapped) {
	PrintDebug("rm_shdw_pde: no multimap\n");
	if(page->shadow_pde !=  shadow_pde) 
	    PrintDebug("rm_shdw_pde: error page->shadow_pde is not equal to shadow_pde\n");
	page->shadow_pde = 0;
	PrintDebug("rm_shdw_pde: return\n");
	return;
    }
	
    PrintDebug("rm_shdw_pde: multimap\n");

    hlist_for_each_entry (pde_chain, node, &page->shadow_pdes, link)
    for (i=0; i < NR_PTE_CHAIN_ENTRIES; ++i) {
	if(!pde_chain->shadow_pdes[i]) break;
	if(pde_chain->shadow_pdes[i] != shadow_pde) continue;

	PrintDebug("rm_shdw_pde: found shadow_pde at i %d\n",i);
	while (i+1 < NR_PTE_CHAIN_ENTRIES && pde_chain->shadow_pdes[i+1]) {
	    pde_chain->shadow_pdes[i] = pde_chain->shadow_pdes[i+1];
	    ++i;
	}
	pde_chain->shadow_pdes[i] = 0;

	if(i==0) {
	    PrintDebug("rm_shdw_pde: only one!\n");
	    hlist_del(&pde_chain->link);				
	    shadow_free_pde_chain(core, pde_chain);
	    if(hlist_empty(&page->shadow_pdes)) {
		page->multimapped = 0;
		page->shadow_pde = 0;
	    }
	}

	PrintDebug("rm_shdw_pde: return\n");
	return;
    }
    PrintDebug("rm_shdw_pde: return\n");
}

static void shadow_page_search_shadow_pde (struct guest_info* core, addr_t shadow_pde, 
	addr_t guest_pde, unsigned hlevel) {

    struct shadow_page_cache_data* shdw_page;
    unsigned index;
    struct hlist_head* bucket;
    struct hlist_node* node;
    int hugepage_access = 0;
    union shadow_page_role role;
    addr_t pt_base_addr = 0;
    int metaphysical = 0;

    PrintDebug("shadow_page_search_shadow_pde\n");
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    if (mode == PROTECTED) {

	PrintDebug("shadow_page_search_shadow_pde: PROTECTED\n");
	pt_base_addr = ((pde32_t*)guest_pde)->pt_base_addr;
	
	if(((pde32_t*)guest_pde)->large_page == 1) {
	    PrintDebug("shadow_page_search_shadow_pde: large page\n");
	    hugepage_access = (((pde32_4MB_t *) guest_pde)->writable) | (((pde32_4MB_t*)guest_pde)->user_page << 1);
	    metaphysical = 1;
	    pt_base_addr = (addr_t) PAGE_BASE_ADDR(BASE_TO_PAGE_ADDR_4MB(((pde32_4MB_t*)guest_pde)->page_base_addr));
	}
			
	role.word = 0; 
	role.glevels = PT32_ROOT_LEVEL; //max level
	role.hlevels = PT_PAGE_TABLE_LEVEL;
	role.metaphysical = metaphysical;
	role.hugepage_access = hugepage_access;
		
    } else if (mode == LONG_32_COMPAT || mode == LONG) {

	PrintDebug("shadow_page_search_shadow_pde: LONG_32_COMPAT/LONG\n");
	pt_base_addr = ((pde64_t*)guest_pde)->pt_base_addr;

		
	if(hlevel == PT_DIRECTORY_LEVEL) { 
	    if(((pde64_t*)guest_pde)->large_page == 1) {
		hugepage_access = (((pde64_2MB_t *) guest_pde)->writable) | (((pde64_2MB_t*)guest_pde)->user_page << 1);
		metaphysical = 1;
		pt_base_addr = (addr_t) PAGE_BASE_ADDR(BASE_TO_PAGE_ADDR_2MB(((pde64_2MB_t*)guest_pde)->page_base_addr));
	    }	
	    role.hlevels = PT_PAGE_TABLE_LEVEL;
		
	} else if(hlevel == PT32E_ROOT_LEVEL) {
	    if(((pdpe64_t*)guest_pde)->large_page == 1) {
		hugepage_access = (((pdpe64_1GB_t *) guest_pde)->writable) | (((pdpe64_1GB_t*)guest_pde)->user_page << 1);
		metaphysical = 1;
		pt_base_addr = (addr_t) PAGE_BASE_ADDR(BASE_TO_PAGE_ADDR_1GB(((pdpe64_1GB_t*)guest_pde)->page_base_addr));
	    }
	    role.hlevels = PT_DIRECTORY_LEVEL;
		
	} else if(hlevel == PT64_ROOT_LEVEL) {		
	    if(((pdpe64_t*)guest_pde)->large_page == 1) {
		hugepage_access = (((pdpe64_1GB_t *) guest_pde)->writable) | (((pdpe64_1GB_t*)guest_pde)->user_page << 1);
		metaphysical = 1;
		pt_base_addr = (addr_t) PAGE_BASE_ADDR(BASE_TO_PAGE_ADDR_1GB(((pdpe64_1GB_t*)guest_pde)->page_base_addr));
	    }
	    role.hlevels = PT32E_ROOT_LEVEL;

	}
			
	role.word = 0; 
	role.glevels = PT64_ROOT_LEVEL; //store numeric
	role.metaphysical = metaphysical;
	role.hugepage_access = hugepage_access;	

    }

    index = shadow_page_table_hashfn(pt_base_addr) % NUM_SHADOW_PAGES;
    bucket = &core->shadow_page_hash[index];

    hlist_for_each_entry(shdw_page, node, bucket, hash_link) 
    if (shdw_page->guest_fn == pt_base_addr  && shdw_page->role.word == role.word ) {
	PrintDebug("shadow_page_search_shadow_pde: found\n");
	shadow_page_remove_shadow_pde(core, shdw_page, (addr_t)shadow_pde);
	
    } 
    return;

}

static struct shadow_page_cache_data * shadow_page_lookup_page(struct guest_info *core, addr_t guest_fn, int opt) //purpose of this is write protection 
{
    unsigned index;
    struct hlist_head * bucket;
    struct shadow_page_cache_data * page;
    struct hlist_node * node;
	
    PrintDebug("lookup: guest_fn addr %p\n",(void *)BASE_TO_PAGE_ADDR(guest_fn));
	
    index = shadow_page_table_hashfn(guest_fn) % NUM_SHADOW_PAGES;
    bucket = &core->shadow_page_hash[index];
    PrintDebug("lookup: index %d bucket %p\n",index,(void*)bucket);

    hlist_for_each_entry(page, node, bucket, hash_link)
	if (opt == 0) {
	    PrintDebug("lookup: page->gfn %p gfn %p metaphysical %d\n",
	        (void*)BASE_TO_PAGE_ADDR(page->guest_fn),(void*)BASE_TO_PAGE_ADDR(guest_fn),page->role.metaphysical);
	    if (page->guest_fn == guest_fn && !page->role.metaphysical) {
		return page;
	    }
	}
	else if(page->guest_fn == guest_fn) { 
	    return page; 
	}
	
    return NULL;	
}

static void rmap_remove(struct guest_info * core, addr_t shadow_pte);
static void rmap_write_protect(struct guest_info * core, addr_t guest_fn);

struct shadow_page_cache_data * shadow_page_get_page(struct guest_info *core, 
														addr_t guest_fn,
														unsigned level, 
														int metaphysical,
														unsigned hugepage_access,
														addr_t shadow_pde,
														int force)  //0:default 1:off cache 2:off debug print
{
    struct shadow_page_cache_data *page;
    union shadow_page_role role;
    unsigned index;
    struct hlist_head *bucket;
    struct hlist_node *node;
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);
	
    role.word = 0; 
    if (mode == REAL || mode == PROTECTED) role.glevels = PT32_ROOT_LEVEL; 
	//exceptional, longterm there should be argument 
    else if (mode == PROTECTED_PAE) role.glevels = PT32E_ROOT_LEVEL;
    else if (mode == LONG || mode == LONG_32_COMPAT) role.glevels = PT64_ROOT_LEVEL;
    else return NULL;
	
	//store numeric
    role.hlevels = level;
    role.metaphysical = metaphysical;
    role.hugepage_access = hugepage_access;
	
    index = shadow_page_table_hashfn(guest_fn) % NUM_SHADOW_PAGES;
    bucket = &core->shadow_page_hash[index];

    if (force != 2) PrintDebug("get_page: lvl %d idx %d gfn %p role %x\n", level, index, (void *)guest_fn,role.word);

    hlist_for_each_entry(page, node, bucket, hash_link)
	if (page->guest_fn == guest_fn && page->role.word == role.word) {
	    shadow_page_add_shadow_pde(core, page, shadow_pde); //guest_fn is right there
	    if(force != 2) 
		PrintDebug("get_page: found guest_fn %p, index %d, multi %d, next %p\n", 
		    (void *)page->guest_fn, index, page->multimapped, (void *)page->hash_link.next);
	    if (force == 0 || force == 2) 
		return page;
	    else { 
		shadow_zap_page(core,page);
		goto new_alloc;
	    }
	} else {
	    if(force != 2) 
		PrintDebug("get_page: no found guest_fn %p, index %d, multimapped %d, next %p\n", 
		    (void *)page->guest_fn, index, page->multimapped, (void *)page->hash_link.next);
	}

    if (force != 2) 
	PrintDebug("get_page: no found\n");

new_alloc:

    page=shadow_alloc_page(core, shadow_pde);

    if (!page) return page; 

    page->guest_fn = guest_fn;
    page->role=role;
    page->multimapped = 0;
    page->shadow_pde = 0;
	
    if (force != 2) 
	PrintDebug("get_page: hadd h->first %p, n %p, n->next %p\n", 
	    (void *)bucket->first, (void *)&page->hash_link, (void *)page->hash_link.next);

    hlist_add_head(&page->hash_link, bucket);
    shadow_page_add_shadow_pde(core, page, shadow_pde);

    if (force != 2) PrintDebug("get_page: hadd h->first %p, n %p, n->next %p\n", 
	(void *)bucket->first, (void *)&page->hash_link, (void *)page->hash_link.next);	

    if (!metaphysical) rmap_write_protect(core, guest_fn); //in case rmapped guest_fn being allocated as pt or pd
    if (force != 2) PrintDebug("get_page: return\n");

    return page;

}

static void shadow_page_unlink_children (struct guest_info * core, struct shadow_page_cache_data * page) {
    unsigned i;

    uint32_t* shdw32_table;
    uint32_t* shdw32_entry;
    uint64_t* shdw64_table;
    uint64_t* shdw64_entry;

    uint32_t* guest32_table;
    uint32_t* guest32_entry;
    uint64_t* guest64_table;
    uint64_t* guest64_entry;

    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    if(page->role.hlevels == PT_PAGE_TABLE_LEVEL) {		

	if (mode == PROTECTED) {

	    shdw32_table = (uint32_t*) V3_VAddr((void *)(addr_t)CR3_TO_PDE32_PA(page->page_pa));		
	    PrintDebug("ulink_chil: pte lvl\n");

	    for (i = 0; i < PT32_ENT_PER_PAGE; ++i) {
		shdw32_entry = (uint32_t*)&(shdw32_table[i]);
		if (*shdw32_entry & PT_PRESENT_MASK) {
		    rmap_remove(core, (addr_t)shdw32_entry);
		    PrintDebug("ulink_chil: %d pte: shadow %x\n", i, *shdw32_entry);
		}
		memset((void *)shdw32_entry, 0, sizeof(uint32_t));
	    }
	    PrintDebug("ulink_chil: return pte\n");
	    return;	
			
	} else if (mode == LONG_32_COMPAT || mode == LONG) {

	    shdw64_table = (uint64_t*) V3_VAddr((void *)(addr_t)CR3_TO_PML4E64_PA(page->page_pa));		
	    PrintDebug("ulink_chil: pte lvl\n");

	    for (i = 0; i < PT_ENT_PER_PAGE; ++i) {			
	    	shdw64_entry = (uint64_t*)&(shdw64_table[i]);
	    	if (*shdw64_entry & PT_PRESENT_MASK) {
		    rmap_remove(core, (addr_t)shdw64_entry);
		    PrintDebug("ulink_chil: %d pte: shadow %p\n", i, (void*)*((uint64_t*)shdw64_entry));
	        }
	        memset((void *)shdw64_entry, 0, sizeof(uint64_t));
	    }

	    PrintDebug("ulink_chil: return pte\n");
	    return;				
	}
    }

    PrintDebug("ulink_chil: pde lvl\n");
    if (mode == PROTECTED) {
		
	shdw32_table = (uint32_t*) V3_VAddr((void*)(addr_t)CR3_TO_PDE32_PA(page->page_pa));

	if (guest_pa_to_host_va(core, BASE_TO_PAGE_ADDR(page->guest_fn), (addr_t*)&guest32_table) == -1) {
	    PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)BASE_TO_PAGE_ADDR(page->guest_fn));
	    return;
	} 
		
	for (i = 0; i < PT32_ENT_PER_PAGE; ++i) {
	    int present = 0;
	    shdw32_entry = (uint32_t*)&(shdw32_table[i]);
	    guest32_entry = (uint32_t*)&(guest32_table[i]);
	    present = *shdw32_entry & PT_PRESENT_MASK;
	    if(present) PrintDebug("ulink_chil: pde %dth: shadow %x\n", i, *((uint32_t*)shdw32_entry));
	    memset((void *)shdw32_entry, 0, sizeof(uint32_t));
	    if (present != 1) continue;

	    shadow_page_search_shadow_pde(core, (addr_t)shdw32_entry, (addr_t)guest32_entry, page->role.hlevels);
	}
	PrintDebug("ulink_child: before return at pde lvel\n");
	return;

    }else if(mode == LONG_32_COMPAT || mode == LONG)  {

	shdw64_table = (uint64_t*) V3_VAddr((void*)(addr_t)CR3_TO_PML4E64_PA(page->page_pa));

	if (guest_pa_to_host_va(core, BASE_TO_PAGE_ADDR(page->guest_fn), (addr_t*)&guest64_table) == -1) {
	    if(page->role.hlevels == PT_DIRECTORY_LEVEL) 
		PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)BASE_TO_PAGE_ADDR(page->guest_fn));
	    if(page->role.hlevels == PT32E_ROOT_LEVEL) 
		PrintError("Invalid Guest PDPE Address: 0x%p\n",  (void *)BASE_TO_PAGE_ADDR(page->guest_fn));
	    if(page->role.hlevels == PT64_ROOT_LEVEL) 
		PrintError("Invalid Guest PML4E Address: 0x%p\n",  (void *)BASE_TO_PAGE_ADDR(page->guest_fn));
	    return;	
	}

	for (i = 0; i < PT_ENT_PER_PAGE; ++i) {
	    int present = 0;
	    shdw64_entry = (uint64_t*)&(shdw64_table[i]);
	    guest64_entry = (uint64_t*)&(guest64_table[i]);
	    present = *shdw64_entry & PT_PRESENT_MASK;
	    if(present) PrintDebug("ulink_chil: pde: shadow %p\n",(void *)*((uint64_t *)shdw64_entry));
	    memset((void *)shdw64_entry, 0, sizeof(uint64_t));
	    if (present != 1) continue;

	    shadow_page_search_shadow_pde(core, (addr_t)shdw64_entry, (addr_t)guest64_entry, page->role.hlevels);
	}
	return;		

    }
    //PrintDebug("ulink_chil: return pde\n");

}

static void shadow_page_put_page(struct guest_info *core, struct shadow_page_cache_data * page, addr_t shadow_pde) { 

	PrintDebug("put_page: start\n");	
	shadow_page_remove_shadow_pde(core, page, shadow_pde);

	PrintDebug("put_page: end\n");

} 

static void shadow_zap_page(struct guest_info * core, struct shadow_page_cache_data * page) {

    addr_t shadow_pde;
    addr_t cr3_base_addr = 0;
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);
	
    PrintDebug("zap: multimapped %d, metaphysical %d\n", page->multimapped, page->role.metaphysical);
	
    while (page->multimapped || page->shadow_pde) {
	if (!page->multimapped) {
	    shadow_pde = page->shadow_pde;		
	} else {
	    struct pde_chain * chain;
	    chain = container_of(page->shadow_pdes.first, struct pde_chain, link);
	    shadow_pde = chain->shadow_pdes[0];
	}		
	shadow_page_put_page(core, page, shadow_pde);
	PrintDebug("zap_parent: pde: shadow %p\n",(void *)*((addr_t *)shadow_pde));
	memset((void *)shadow_pde, 0, sizeof(struct pde32));	
    }

    shadow_page_unlink_children(core, page);

    PrintDebug("zap: end of unlink\n");
	
    if (mode == PROTECTED) {
	cr3_base_addr =  ((struct cr3_32 *)&(core->shdw_pg_state.guest_cr3))->pdt_base_addr;
    } else if (mode == LONG_32_COMPAT || mode == LONG) {
	cr3_base_addr =  ((struct cr3_64 *)&(core->shdw_pg_state.guest_cr3))->pml4t_base_addr;
    }
    else return;	

    PrintDebug("zap: before hlist_del\n");
    PrintDebug("zap: page->guest_fn %p\n", (void*) page->guest_fn);

    if (page->guest_fn !=  (addr_t)(cr3_base_addr)) {
	PrintDebug("zap: first hlist_del\n");

	hlist_del(&page->hash_link);
	shadow_free_page(core, page);

    } else {
	PrintDebug("zap: second hlist_del\n");

	list_del(&page->link);
	list_add(&page->link,&core->active_shadow_pages);
    }		

    PrintDebug("zap: end hlist_del\n");
    return;
}

int shadow_zap_hierarchy_32(struct guest_info * core, struct shadow_page_cache_data * page) {

    unsigned i;
    pde32_t *shadow_pde;
    pde32_t *shadow_pd;
    pde32_t *guest_pde;
    pde32_t *guest_pd;
	
    if (page->role.hlevels != 2) return -1;

    shadow_pd = CR3_TO_PDE32_VA(page->page_pa);
    if (guest_pa_to_host_va(core, BASE_TO_PAGE_ADDR(page->guest_fn), (addr_t*)&guest_pd) == -1) {
	PrintError("Invalid Guest PDE Address: 0x%p\n", (void*)BASE_TO_PAGE_ADDR(page->guest_fn));
	return -1;
    }
	
    for (i=0; i < PT32_ENT_PER_PAGE; ++i) {
	int present = 0;
	shadow_pde = (pde32_t*)&(shadow_pd[i]);
	guest_pde = (pde32_t*)&(guest_pd[i]);
	present = shadow_pde->present;
	if (shadow_pde->present) PrintDebug("ulink_child: pde shadow %x\n", *((uint32_t*)shadow_pde));
	memset((void*)shadow_pde, 0, sizeof(struct pde32));
	if (present != 1) continue;

	struct shadow_page_cache_data *shdw_page;
	unsigned index;
	struct hlist_head *bucket;
	struct hlist_node *node;
	int hugepage_access =0;
	int metaphysical = 0;
	union shadow_page_role role;
	v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

	if (((pde32_t*)guest_pde)->large_page == 1) {
	    hugepage_access = (((pde32_4MB_t*)guest_pde)->writable) | (((pde32_4MB_t*)guest_pde)->user_page << 1);
	    metaphysical = 1;
	}
    	
    	role.word = 0; 
    	if (mode == REAL || mode == PROTECTED) role.glevels = PT32_ROOT_LEVEL; 
	//exceptional, longterm there should be argument 
    	else if (mode == PROTECTED_PAE) role.glevels = PT32E_ROOT_LEVEL;
    	else if (mode == LONG || mode == LONG_32_COMPAT) role.glevels = PT64_ROOT_LEVEL;
    	else return -1;
	
	role.hlevels = 1;
	role.metaphysical = metaphysical;
	role.hugepage_access = hugepage_access;

	index = shadow_page_table_hashfn(guest_pde->pt_base_addr) % NUM_SHADOW_PAGES;
	bucket = &core->shadow_page_hash[index];

	hlist_for_each_entry(shdw_page, node, bucket, hash_link)
	if (shdw_page->guest_fn == (guest_pde->pt_base_addr) && (shdw_page->role.word == role.word)) {
	    shadow_zap_page(core, shdw_page);
	}	
    }

    shadow_zap_page(core, page);
    return 0;
}


int shadow_unprotect_page(struct guest_info * core, addr_t guest_fn) {

    unsigned index;
    struct hlist_head * bucket;
    struct shadow_page_cache_data * page = NULL;
    struct hlist_node * node;
    struct hlist_node * n;
    int r;

    r = 0;
    index = shadow_page_table_hashfn(guest_fn) % NUM_SHADOW_PAGES;
    bucket = &core->shadow_page_hash[index];
    PrintDebug("unprotect: gfn %p\n",(void *) guest_fn);
	
    hlist_for_each_entry_safe(page, node, n, bucket, hash_link) {
    //hlist_for_each_entry(page, node, bucket, hash_link) {
	if ((page->guest_fn == guest_fn) && !(page->role.metaphysical)) {
	    PrintDebug("unprotect: match page.gfn %p page.role %x gfn %p\n",(void *) page->guest_fn,page->role.word,(void *)guest_fn);
	    shadow_zap_page(core, page);
	    r = 1;
	}
    }
	
    PrintDebug("at shadow_unprotect_page return %d\n",r);
    return r;
}

/*
reverse mapping data structures:
if page_private bit zero is zero, then page->private points to the shadow page table entry that points to page address
if page_private bit zero is one, then page->private & ~1 points to a struct rmap containing more mappings
*/

void rmap_add(struct guest_info *core, addr_t shadow_pte) {
    struct rmap *desc;
    addr_t page_private = 0;
    gen_pt_t * shadow_pte_gen;
    addr_t page_base_addr = 0;
    addr_t * mem_map;
    int i;
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    shadow_pte_gen = (gen_pt_t *) shadow_pte;

    if (mode == PROTECTED) {
	page_base_addr = ((pte32_t *)shadow_pte)->page_base_addr;
	PrintDebug("at rmap_add shadow_pte: %x\n", (uint32_t)*((uint32_t*)shadow_pte));

    } else if (mode == LONG_32_COMPAT || mode == LONG) {
	page_base_addr = ((pte64_t *)shadow_pte)->page_base_addr;
	PrintDebug("at rmap_add shadow_pte: %p\n", (void*)*((uint64_t*)shadow_pte));

    }
    else return;	
	
    PrintDebug("debug rmap: at rmap_add shadow_pte->page_base_addr (%p), shadow_pte_present %d, shadow_pte_writable %d\n", 
	(void *)BASE_TO_PAGE_ADDR(page_base_addr), (shadow_pte_gen->present), (shadow_pte_gen->writable));
	
    if (shadow_pte_gen->present == 0 || shadow_pte_gen->writable == 0)
	return;

    PrintDebug("at rmap_add host_fn %p\n", (void *)BASE_TO_PAGE_ADDR(page_base_addr));
		
    mem_map = core->vm_info.mem_map.base_region.mem_map;
    page_private = mem_map[page_base_addr];

    PrintDebug("at rmap_add page_private %p\n", (void *)page_private);
	
    if (!page_private) {
	PrintDebug("at rmap_add initial\n");
	mem_map[page_base_addr] = (addr_t)shadow_pte;
	PrintDebug("rmap_add: shadow_pte %p\n", (void *)shadow_pte);

    } else if (!(page_private & 1)) {
	PrintDebug("at rmap_add into multi\n");

	desc = shadow_alloc_rmap(core);
	desc->shadow_ptes[0] = page_private;
	desc->shadow_ptes[1] = shadow_pte;
	mem_map[page_base_addr] = (addr_t)desc | 1;
	desc->more = NULL;
	PrintDebug("rmap_add: desc %p desc|1 %p\n",(void *)desc,(void *)((addr_t)desc |1));

    } else {
	PrintDebug("at rmap_add multimap\n");
	desc = (struct rmap *)(page_private & ~1ul);

	while (desc->more && desc->shadow_ptes[RMAP_EXT-1]) desc = desc->more;
		
	if (desc->shadow_ptes[RMAP_EXT-1]) {
	    desc->more = shadow_alloc_rmap(core);
	    desc = desc->more;
	}

	for (i = 0; desc->shadow_ptes[i]; ++i) ;
	desc->shadow_ptes[i] = shadow_pte;		
    }
		
}

static void rmap_desc_remove_entry(struct guest_info *core,
				   addr_t * page_private,
				   struct rmap *desc,
				   int i,
				   struct rmap *prev_desc) 
{
    int j;

    for (j = RMAP_EXT - 1; !desc->shadow_ptes[j]  &&  j > i; --j) ;
    desc->shadow_ptes[i] = desc->shadow_ptes[j];
    desc->shadow_ptes[j] = 0;

    if (j != 0) {
	PrintDebug("rmap_desc_rm: i %d j %d\n",i,j);
	return;
    }

    if (!prev_desc && !desc->more) {
	PrintDebug("rmap_desc_rm: no more no less\n");
	*page_private = desc->shadow_ptes[0];
    } else {		//more should be null
	if (prev_desc) {
	    PrintDebug("rmap_desc_rm: no more\n");
	    prev_desc->more = desc->more; 
	} else {
	    PrintDebug("rmap_desc_rm: no less\n");
	    *page_private = (addr_t) desc->more | 1;
	}
    }
    shadow_free_rmap(core, desc);
}

static void rmap_remove(struct guest_info * core, addr_t shadow_pte) {
    struct rmap *desc;
    struct rmap *prev_desc;
    addr_t page_private = 0;
    gen_pt_t * shadow_pte_gen;
    addr_t page_base_addr = 0;
    addr_t * mem_map;
    int i;

    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    if (mode == PROTECTED) {
	PrintDebug("rmap_rm: PROTECTED %d\n", mode);
	page_base_addr = ((pte32_t *)shadow_pte)->page_base_addr;

    } else if (mode == LONG_32_COMPAT || mode == LONG) {
	PrintDebug("rmap_rm: LONG_32_COMPAT/LONG %d\n", mode);
	page_base_addr = ((pte64_t *)shadow_pte)->page_base_addr;		

    }	else {
    	PrintDebug("rmap_rm: mode %d\n", mode);
    	return;	
    }
    shadow_pte_gen = (gen_pt_t*)shadow_pte;

    if (shadow_pte_gen->present == 0 || shadow_pte_gen->writable == 0) {
	PrintDebug("rmap_rm: present %d, write %d, pte %p\n",
		shadow_pte_gen->present, shadow_pte_gen->writable,
		(void*)*((addr_t*)shadow_pte));
	return;
    }
    PrintDebug("rmap_rm: shadow_pte->page_base_addr (%p)\n", (void *)BASE_TO_PAGE_ADDR(page_base_addr));

    mem_map = core->vm_info.mem_map.base_region.mem_map;
    page_private = mem_map[page_base_addr];

    PrintDebug("rmap_rm: page_private %p page_private&1 %p\n",(void *)page_private,(void*)(page_private&1));
	
    if (!page_private) {		
	PrintDebug("rmap_rm: single page_prive %p\n",(void *)page_private);
	
    } else if (!(page_private & 1)) {	
	PrintDebug("rmap_rm: multi page_prive %p\n",(void *)page_private);
	mem_map[page_base_addr] = (addr_t)0;

    } else {
	PrintDebug("rmap_rm: multimap page_prive %p\n",(void *)page_private);
	desc = (struct rmap *)(page_private & ~1ul);
	prev_desc = NULL;
	
	while (desc) {
	    PrintDebug("rmap_rm: desc loop\n");
	    for (i = 0; i < RMAP_EXT && desc->shadow_ptes[i]; ++i)
	    if (desc->shadow_ptes[i] == shadow_pte) {
		PrintDebug("rmap_rm: rmap_desc_remove_entry i %d\n",i);
		rmap_desc_remove_entry(core, &mem_map[page_base_addr], desc, i, prev_desc);
		return;
	    }
	    prev_desc = desc;
	    desc = desc->more;
	}
    }
}

static inline int activate_shadow_pt_32(struct guest_info * core);

static void rmap_write_protect(struct guest_info * core, addr_t guest_fn) {
    struct rmap * desc;
    //pte32_t * shadow_pte;
    addr_t shadow_pte;
    addr_t page_private;
    addr_t host_pa;

    PrintDebug("rmap_wrprot: gfn %p\n",(void *) guest_fn);

    if (guest_pa_to_host_pa(core, BASE_TO_PAGE_ADDR(guest_fn), &host_pa)!=0) {
	PrintDebug("rmap_wrprot: error \n");
    }

    page_private = core->vm_info.mem_map.base_region.mem_map[PAGE_BASE_ADDR(host_pa)];

    PrintDebug("rmap_wrprot: host_fn %p\n",(void *)PAGE_BASE_ADDR(host_pa));
	
    while(page_private) {
	PrintDebug("rmap_wrprot: page_private %p\n", (void*)page_private);
	if(!(page_private & 1)) {
	    PrintDebug("rmap_wrprot: reverse desc single\n");
	    shadow_pte = page_private;
		
	} else {
	    desc = (struct rmap *) (page_private & ~1ul);
	    PrintDebug("rmap_wrprot: reverse desc multimap\n");
	    shadow_pte = desc->shadow_ptes[0];
	}
		
	PrintDebug("rmap_wrprot: pg_priv %p, host_fn %p, shdw_pte %p\n",
		(void *)page_private, (void *)PAGE_BASE_ADDR(host_pa), (void*)*((uint64_t*)shadow_pte));
	
	//CHECKPOINT
	rmap_remove(core, shadow_pte); 

	//PrintDebug("rmap_wrprot: shadow_pte->page_base_addr (%p)\n", 
	//	(void *)BASE_TO_PAGE_ADDR(shadow_pte->page_base_addr));

	((gen_pt_t *)shadow_pte)->writable = 0;
	PrintDebug("rmap_wrprot: %p\n",(void*)*((uint64_t *)shadow_pte));
				
	page_private = core->vm_info.mem_map.base_region.mem_map[PAGE_BASE_ADDR(host_pa)];

	PrintDebug("rmap_wrprot: page_private %p\n",(void*)page_private);
    }	

    PrintDebug("rmap_wrprot: done\n");

}

void shadow_page_pre_write(struct guest_info * core, addr_t guest_pa, int bytes, int force) {
//guest frame number is not guest physical address
    addr_t guest_fn = PAGE_BASE_ADDR(guest_pa);
    struct shadow_page_cache_data * page;
    struct hlist_node *node, *n;
    struct hlist_head * bucket;
    unsigned index;

    uint32_t* shdw32_table = NULL;
    uint32_t* shdw32_entry = NULL;
    uint64_t* shdw64_table = NULL;
    uint64_t* shdw64_entry = NULL;
	
    unsigned pte_size;
    unsigned offset = PAGE_OFFSET(guest_pa);
    unsigned misaligned = 0;
    int level;
    int flooded = 0;

    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    if (guest_fn == core->last_pt_write_guest_fn) {
	++core->last_pt_write_count;
	if (core->last_pt_write_count >= 3) flooded = 1;
    } else {
	core->last_pt_write_guest_fn = guest_fn;
	core->last_pt_write_count = 1;
    }

    PrintDebug("shdw_pre-write: gpa %p byte %d force %d flood %d last_gfn %p last_cnt %d\n",
	(void *)guest_pa,bytes,force,flooded,(void*)core->last_pt_write_guest_fn,core->last_pt_write_count);

    index = shadow_page_table_hashfn(guest_fn) % NUM_SHADOW_PAGES;
    bucket = &core->shadow_page_hash[index];

    PrintDebug("shdw_pre-write: check point after bucket\n");
	
    //hlist_for_each_entry_safe(page, node, bucket, hash_link) {
    hlist_for_each_entry_safe(page, node, n, bucket, hash_link) {

    	if (page->guest_fn != guest_fn || page->role.metaphysical) continue;

    	pte_size = 4; //because 32bit nonPAE for now
    	pte_size = page->role.glevels == 2 ? 4 : 8;

    	if (!force) misaligned = (offset & (offset + bytes -1)) & ~(pte_size -1);

    	if (misaligned || flooded || force) {
	    /*
	    * Misaligned accesses are too much trobule to fix up
	    * also they usually indicate a page is not used as a page table
	    */
	    PrintDebug("shdw_pre-write: misaligned\n");
	    shadow_zap_page(core, page);
	    continue;
    	}	

    	level = page->role.hlevels;		
		
    	PrintDebug("shdw_pre-write: found out one page at the level of %d\n", level);
	
   	if (mode == PROTECTED) {
	    shdw32_table = (uint32_t*)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(PAGE_BASE_ADDR(page->page_pa)));
	    shdw32_entry = (uint32_t*)&(shdw32_table[offset/sizeof(uint32_t)]);

	    if (*shdw32_entry & PT_PRESENT_MASK) {
	    	if (level == PT_PAGE_TABLE_LEVEL) {
		    PrintDebug("shdw_pre-write: pte idx %d\n", (unsigned int)(offset/sizeof(uint32_t)));
		    rmap_remove(core, (addr_t)shdw32_entry);
		    memset((void*)shdw32_entry, 0, sizeof(uint32_t));
		
	    	} else {
		    shadow_page_remove_shadow_pde(core, page, (addr_t)shdw32_entry);
		    memset((void*)shdw32_entry, 0, sizeof(uint32_t));			
	    	}
	    }
			
    	} else if (mode == LONG_32_COMPAT || mode == LONG) {

	    shdw64_table = (uint64_t*)V3_VAddr((void*)(addr_t)BASE_TO_PAGE_ADDR(PAGE_BASE_ADDR(page->page_pa)));
	    shdw64_entry = (uint64_t*)&(shdw64_table[offset/sizeof(uint64_t)]);

	    if (*shdw64_entry & PT_PRESENT_MASK) {
	    	if (level == PT_PAGE_TABLE_LEVEL) {
		    PrintDebug("shdw_pre-write: pte idx %d\n", (unsigned int)(offset/sizeof(uint64_t)));
		    rmap_remove(core, (addr_t)shdw64_entry);
		    memset((void*)shdw64_entry, 0, sizeof(uint64_t));
	    	} else {
		    shadow_page_remove_shadow_pde(core, page, (addr_t)shdw64_entry);
		    memset((void*)shdw64_entry, 0, sizeof(uint64_t));			
	    	}
	    }
    	}
    }
}

//emulation for synchronization
void shadow_page_post_write(struct guest_info * core, addr_t guest_pa)  {

}

int shadow_unprotect_page_virt(struct guest_info * core, addr_t guest_va) {
    addr_t guest_pa;

    if (guest_va_to_guest_pa(core, guest_va, &guest_pa) != 0) {
	PrintError("In GVA->HVA: Invalid GVA(%p)->GPA lookup\n", 
		(void *)guest_va);
	return -1;
    }
	
    return shadow_unprotect_page(core, PAGE_BASE_ADDR(guest_pa));
}

void shadow_free_some_pages(struct guest_info * core) {
    while (core->n_free_shadow_pages < REFILE_PAGES) {
	struct shadow_page_cache_data * page;
	page = container_of(core->active_shadow_pages.prev,
	    struct shadow_page_cache_data, link);
	shadow_zap_page(core,page);
    }		
}

void shadow_free_all_pages(struct guest_info *core) {

    struct shadow_page_cache_data * sp, *node;
    list_for_each_entry_safe(sp, node, &core->active_shadow_pages, link) {
	shadow_zap_page(core , sp);
    }
}


static struct shadow_page_cache_data * create_new_shadow_pt(struct guest_info * core);


#include "vmm_shdw_pg_cache_32.h"
#include "vmm_shdw_pg_cache_32pae.h"
#include "vmm_shdw_pg_cache_64.h"

static int vtlb_caching_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {

    V3_Print("VTLB Caching initialization\n");
    return 0;
}

static int vtlb_caching_deinit(struct v3_vm_info * vm) {
    return -1;
}

static int vtlb_caching_local_init(struct guest_info * core) {

    V3_Print("VTLB local initialization\n");

    INIT_LIST_HEAD(&core->active_shadow_pages);
    INIT_LIST_HEAD(&core->free_pages);

    alloc_shadow_pages(core);	

    shadow_topup_caches(core);

    core->prev_cr3_pdt_base = 0;
	
    return 0;
}


static int vtlb_caching_activate_shdw_pt(struct guest_info * core) {
    switch (v3_get_vm_cpu_mode(core)) {

	case PROTECTED:
	    return activate_shadow_pt_32(core);
	case PROTECTED_PAE:
	    return activate_shadow_pt_32pae(core);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    return activate_shadow_pt_64(core);
	default:
	    PrintError("Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
	    return -1;
    }

    return 0;
}

static int vtlb_caching_invalidate_shdw_pt(struct guest_info * core) {
    return vtlb_caching_activate_shdw_pt(core);
}


static int vtlb_caching_handle_pf(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {

	switch (v3_get_vm_cpu_mode(core)) {
	    case PROTECTED:
		return handle_shadow_pagefault_32(core, fault_addr, error_code);
		break;
	    case PROTECTED_PAE:
		return handle_shadow_pagefault_32pae(core, fault_addr, error_code);
	    case LONG:
	    case LONG_32_COMPAT:
	    case LONG_16_COMPAT:
		return handle_shadow_pagefault_64(core, fault_addr, error_code);
		break;
	    default:
		PrintError("Unhandled CPU Mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
		return -1;
	}
}


static int vtlb_caching_handle_invlpg(struct guest_info * core, addr_t vaddr) {

    switch (v3_get_vm_cpu_mode(core)) {
	case PROTECTED:
	    return handle_shadow_invlpg_32(core, vaddr);
	case PROTECTED_PAE:
	    return handle_shadow_invlpg_32pae(core, vaddr);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    return handle_shadow_invlpg_64(core, vaddr);
	default:
	    PrintError("Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
	    return -1;
    }
}

static struct v3_shdw_pg_impl vtlb_caching_impl =  {
    .name = "VTLB_CACHING",
    .init = vtlb_caching_init,
    .deinit = vtlb_caching_deinit,
    .local_init = vtlb_caching_local_init,
    .handle_pagefault = vtlb_caching_handle_pf,
    .handle_invlpg = vtlb_caching_handle_invlpg,
    .activate_shdw_pt = vtlb_caching_activate_shdw_pt,
    .invalidate_shdw_pt = vtlb_caching_invalidate_shdw_pt
};





register_shdw_pg_impl(&vtlb_caching_impl);

#endif
