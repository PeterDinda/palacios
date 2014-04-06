/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, NWU EECS 441 Transactional Memory Team  
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Maciek Swiech <dotpyfe@u.northwestern.edu>
 *          Marcel Flores <marcel-flores@u.northwestern.edu>
 *          Zachary Bischof <zbischof@u.northwestern.edu>
 *          Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/svm.h>
#include <palacios/vmm_excp.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_hashtable.h>

#include <extensions/trans_mem.h>
#include <extensions/tm_util.h>

extern void v3_stgi();
extern void v3_clgi();

#if !V3_CONFIG_DEBUG_TM_FUNC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

/* TM Read/Write List data structure and API *********************************
 */

static void free_mem_op_list (struct list_head * list) {
    struct mem_op * curr = NULL;
    struct mem_op * tmp  = NULL;

    list_for_each_entry_safe(curr, tmp, list, op_node) {
        list_del(&(curr->op_node));
        V3_Free(curr);
    }
}


void v3_clear_tm_lists (struct v3_trans_mem * tm) {
    free_mem_op_list(&(tm->trans_w_list));
    free_mem_op_list(&(tm->trans_r_list));
}


int add_mem_op_to_list (struct list_head * list, addr_t guest_addr) {
    struct mem_op * new;

    new = list_contains_guest_addr(list, guest_addr);

    if (new) {
        new->current = 0;
        return 0;
    }

    new = (struct mem_op *)V3_Malloc(sizeof(struct mem_op));
    if (!new) {
        return -1;
    }

    new->guest_addr = guest_addr;
    new->current    = 0;

    list_add_tail(&(new->op_node), list);

    return 0;
}


struct mem_op * list_contains_guest_addr (struct list_head * list, addr_t guest_addr) {
    struct mem_op * curr = NULL;
    struct mem_op * tmp = NULL;

    list_for_each_entry_safe(curr, tmp, list, op_node) {
        if (curr->guest_addr == guest_addr) {
            return curr;
        }
    }

    return NULL;
}




int update_list(struct v3_trans_mem * tm, struct list_head * list) {
    struct mem_op * curr = NULL;
    struct mem_op * tmp  = NULL;
    void * sp_loc;
    addr_t v_sp_loc;

    list_for_each_entry_safe(curr, tmp, list, op_node) {
        if (!curr->current) {
            /* we do not have the most current entry! grab it from the staging
             * page
             */
            sp_loc = (void *)((addr_t)(tm->staging_page) + (curr->guest_addr % PAGE_SIZE));
            if ((curr->guest_addr % PAGE_SIZE_4KB) > (PAGE_SIZE_4KB - 8)) {
                PrintError(tm->ginfo->vm_info, tm->ginfo,"++ TM UDATE LIST ++ data ref spans page boundary\n");
                return -1;
            }

            if (v3_hpa_to_hva((addr_t)(sp_loc), &v_sp_loc) == -1) {
                PrintError(tm->ginfo->vm_info, tm->ginfo,"Could not convert address on staging page to virtual address\n");
                return -1;
            }

            memcpy((void*)(&(curr->data)), (void*)v_sp_loc, sizeof(uint64_t));
            curr->current = 1;
        }
    }

    return 0;
}


int stage_entry (struct v3_trans_mem * tm, struct list_head * list, addr_t guest_addr) {
    void * sp_loc;
    addr_t v_sp_loc;
    struct mem_op * curr = list_contains_guest_addr(list, guest_addr);

    if (!curr) {
        PrintDebug(tm->ginfo->vm_info, tm->ginfo,"tried to stage entry from addr %p that doesn't exist in this list\n", (void*)guest_addr);
        return -1;
    }

    sp_loc = (void*)((addr_t)(tm->staging_page) + (guest_addr % PAGE_SIZE_4KB));

    if ((curr->guest_addr % PAGE_SIZE_4KB) > (PAGE_SIZE_4KB - 8)) {
        PrintError(tm->ginfo->vm_info, tm->ginfo,"++ TM UDATE LIST ++ data ref spans page boundary\n");
        return -1;
    }

    if (v3_hpa_to_hva((addr_t)(sp_loc), &v_sp_loc) == -1) {
        PrintError(tm->ginfo->vm_info, tm->ginfo,"Could not convert address on staging page to virt addr\n");
        return -1;
    }

    /* write data back to the data page */
    memcpy((void*)v_sp_loc,(void*)(&(curr->data)), sizeof(uint64_t));

    /* mark entry as not current so we grab it back later */
    curr->current = 0;
    return 0;
}


int copy_add_entry(struct list_head * list, addr_t guest_addr, uint64_t data){
    struct mem_op * new;

    // Don't repeatedly add
    new = list_contains_guest_addr(list, guest_addr);

    if (new) {
        new->current = 1;
        new->data = data;
    } else {
        new = (struct mem_op*)V3_Malloc(sizeof(struct mem_op));

        if (!new) {
            return -1;
        }

        new->guest_addr = guest_addr;
        new->current = 1;
        new->data = data;
        list_add_tail(&(new->op_node), list);
    }
    return 0;
}


int commit_list(struct guest_info * core, struct v3_trans_mem * tm) {
    // We should not be interruptable here, needs to happen atomically
    PrintDebug(core->vm_info, core,"-- TM COMMIT -- commiting data\n");
    v3_clgi();

    struct mem_op * curr = NULL;
    struct mem_op * tmp  = NULL;

    list_for_each_entry_safe(curr, tmp, &(tm->trans_w_list), op_node) {
        addr_t v_ga_loc;
        
        if (v3_gva_to_hva(core, (addr_t)(curr->guest_addr), &v_ga_loc) == -1) {
            PrintError(core->vm_info, core,"Could not translate gva to hva\n");
            return -1;
        }        

        PrintDebug(core->vm_info, core,"\tValue being copied: %p\n", (void*)(curr->data));
        memcpy((void*)v_ga_loc, (void*)(&(curr->data)) , sizeof(uint64_t));
    }

    v3_stgi();
    return 0;
}


int v3_copy_lists(struct guest_info *core) {
    PrintError(core->vm_info, core, "TM: unimplemented (%s)\n", __FUNCTION__);
    return -1;
}


/* TM State functions ********************************************************
 *
 * int v3_set_tm(struct guest_info *core)
 * int v3_clr_tm(struct guest_info *core)
 * int v3_clr_vtlb(struct guest_info *core)
 * int v3_tm_set_abrt(struct guest_info *core)
 *
 */

int v3_set_tm (struct v3_trans_mem * tm) {
    struct v3_tm_state * tms = (struct v3_tm_state *)v3_get_extension_state(tm->ginfo->vm_info, "trans_mem");
    if (tm->TM_MODE == TM_ON) {
        PrintError(tm->ginfo->vm_info, tm->ginfo,"++ TM SET ++ tried to set tm but it was already on\n");
        return -1;
    }

    tm->TM_MODE = TM_ON;
    tm->TM_STATE = TM_NULL;

    addr_t flags;
    enum TM_MODE_E sys_tm;
    
    flags = v3_lock_irqsave(tms->lock);
    (tms->cores_active)++;
    sys_tm = tms->TM_MODE;
    v3_unlock_irqrestore(tms->lock, flags);

    // need to flush everyone elses VTLB to get them to start single stepping IF THEY ARENT ALREADY

    if (sys_tm == TM_OFF) {
        int core_num;
        for (core_num = 0; core_num < tm->ginfo->vm_info->num_cores; core_num++) {
            if (core_num == tm->ginfo->vcpu_id) {
                continue;
            }

            struct guest_info * r_core = &(tm->ginfo->vm_info->cores[core_num]);

            // TODO: what if this happens at an inopportune time?
            v3_clr_vtlb(r_core);
        }
    }
    flags = v3_lock_irqsave(tms->lock);
    tms->TM_MODE = TM_ON;
    v3_unlock_irqrestore(tms->lock, flags);

    return 0;
}

int v3_clr_tm (struct v3_trans_mem * tm) {
    PrintDebug(tm->ginfo->vm_info, tm->ginfo,"++ CLR TM ++ clearing tm state\n");

    struct v3_tm_state * tms = (struct v3_tm_state *)v3_get_extension_state(tm->ginfo->vm_info, "trans_mem");
    tm->TM_MODE = TM_OFF;
    tm->TM_STATE = TM_NULL;
    tm->cur_instr_len = -1;

    // last core to turn off?
    addr_t flags;
    int num_act;
    
    flags = v3_lock_irqsave(tms->lock);
    num_act = --(tms->cores_active);
    v3_unlock_irqrestore(tms->lock, flags);

    if (num_act == 0) {
        PrintDebug(tm->ginfo->vm_info, tm->ginfo,"++ CLR TM ++ we are the last tm->ginfo in TM, turn off system state\n");
        tms->TM_MODE = TM_OFF;
    }
    return 1;
}

int v3_clr_vtlb (struct guest_info * core) {
    PrintDebug(core->vm_info, core,"++ TM VTLB ++ flushing core %d's VTLB\n", core->vcpu_id);
    v3_invalidate_shadow_pts(core);
    return 0;
}

/*
int v3_tm_set_abrt(struct v3_trans_mem * tm) {
    tm->TM_STATE = TM_ABORT;
    return 0;
}
*/

/* TM extra ******************************************************************
 */

int v3_free_staging_page(struct v3_trans_mem * tm) {
    if (!(tm->staging_page)) {
        PrintDebug(tm->ginfo->vm_info, tm->ginfo,"++ %d : TM FREE ++ tried to dealloc null staging page\n", tm->ginfo->vcpu_id);
        return 0;
    }
    V3_FreePages(tm->staging_page, 1);
    tm->staging_page = NULL; 
    return 0;
}
