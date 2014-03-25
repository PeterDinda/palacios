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
 *          Kyle C. Hale <kh@u.northwestern.edu>
 *          Marcel Flores <marcel-flores@u.northwestern.edu>
 *          Zachary Bischof <zbischof@u.northwestern.edu>
 *          
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
#include <palacios/vmm_paging.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/svm.h>
#include <palacios/svm_handler.h>
#include <palacios/vmm_excp.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_hashtable.h>

#include <extensions/trans_mem.h>
#include <extensions/tm_util.h>

#if !V3_CONFIG_DEBUG_TM_FUNC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

/* TODO LIST: 
 * - save/restore register state on XBEGIN/XABORT
 * - put status codes in RAX
 * - Implement proper exceptions for failed XBEGINS etc.
 */

/* this includes a mov to rax */
static const char * vmmcall_bytes = "\x48\xc7\xc0\x37\x13\x00\x00\x0f\x01\xd9"; 
static struct v3_tm_state * tm_global_state = NULL;


static void 
tm_translate_rip (struct guest_info * core, addr_t * target) 
{

    if (core->mem_mode == PHYSICAL_MEM) {
        v3_gpa_to_hva(core, 
                get_addr_linear(core, core->rip, &(core->segments.cs)), 
                target);
    } else if (core->mem_mode == VIRTUAL_MEM) {
        v3_gva_to_hva(core, 
                get_addr_linear(core, core->rip, &(core->segments.cs)), 
                target);
    }

}


static void 
tm_read_instr (struct guest_info * core, 
                           addr_t addr, 
                           uchar_t * dst, 
                           uint_t size) 
{

    if (core->mem_mode == PHYSICAL_MEM) {
        v3_read_gpa_memory(core, 
                get_addr_linear(core, addr , &(core->segments.cs)), 
                size, 
                dst);

    } else { 
       v3_read_gva_memory(core, 
                get_addr_linear(core, addr, &(core->segments.cs)), 
                size, 
                dst);
    }

}


static int 
tm_handle_decode_fail (struct guest_info * core) 
{
    addr_t cur_rip;
    uint_t core_num;

    tm_translate_rip(core, &cur_rip);

#ifdef V3_CONFIG_DEBUG_TM_FUNC
    v3_dump_mem((uint8_t *)cur_rip, INSTR_BUF_SZ);
#endif

    /* If we can't decode an instruction, we treat it as a catastrophic event, aborting *everyone* */
    for (core_num = 0; core_num < core->vm_info->num_cores; core_num++ ) {
        struct v3_trans_mem * remote_tm;

        /* skip local core */
        if (core_num == core->vcpu_id) {
            continue;
        }
        
        remote_tm = v3_get_ext_core_state(&(core->vm_info->cores[core_num]), "trans_mem");
        if (!remote_tm) {
            TM_ERR(core,DECODE,"couldnt get remote_tm\n");
            return -1;
        }

        /* skip cores who aren't in transacitonal context */
        if (remote_tm->TM_MODE == TM_OFF) {
            continue;
        }

        TM_DBG(core,DECODE,"setting abort for core %d due to decoding error\n", core_num);
        remote_tm->TM_ABORT = 1;
    }

    return 0;
}
                                  

/* special casing for control-flow instructions
 * returns 1 if we need to jump 
 * returns -1 on error
 */
static int 
tm_handle_ctrl_flow (struct guest_info * core,
                                 struct v3_trans_mem * tm,
                                 addr_t * instr_location,
                                 struct x86_instr * struct_instr)

{
    /* special casing for control flow instructions */
    struct rflags * flags = (struct rflags *)&(core->ctrl_regs.rflags);
    addr_t offset;
    int to_jmp = 0;

    switch (struct_instr->op_type) {

        case V3_OP_JLE:
            TM_DBG(core,DECODE, "!!++ JLE\n");
            to_jmp = (flags->zf || flags->sf != flags->of);
            offset = struct_instr->dst_operand.operand;

            *instr_location = core->rip + tm->cur_instr_len + (to_jmp ? offset : 0);
            tm->offset = offset;
            tm->to_branch = to_jmp;
            break;
        case V3_OP_JAE:
            TM_DBG(core,DECODE,"!!++ JAE\n");
            to_jmp = (flags->cf == 0);
            offset = struct_instr->dst_operand.operand;

            *instr_location = core->rip + tm->cur_instr_len + (to_jmp ? offset : 0);
            tm->offset = offset;
            tm->to_branch = to_jmp;
            break;
        case V3_OP_JMP:
            TM_DBG(core,DECODE,"!!++ JMP\n");
            to_jmp = 1;
            offset = struct_instr->dst_operand.operand;

            *instr_location = core->rip + tm->cur_instr_len + (to_jmp ? offset : 0);
            tm->offset = offset;
            tm->to_branch = to_jmp;
            break;
        case V3_OP_JNZ:
            TM_DBG(core,DECODE,"!!++ JNZ\n");
            to_jmp = (flags->zf == 0);
            offset = struct_instr->dst_operand.operand;

            *instr_location = core->rip + tm->cur_instr_len + (to_jmp ? offset : 0);
            tm->offset = offset;
            tm->to_branch = to_jmp;
            break;
        case V3_OP_JL:
            TM_DBG(core,DECODE,"!!++ JL\n");
            to_jmp = (flags->sf != flags->of);
            offset = struct_instr->dst_operand.operand;

            *instr_location = core->rip + tm->cur_instr_len + (to_jmp ? offset : 0);
            tm->offset = offset;
            tm->to_branch = to_jmp;
            break;
        case V3_OP_JNS:
            TM_DBG(core,DECODE,"!!++ JNS\n");
            to_jmp = (flags->sf == 0);
            offset = struct_instr->dst_operand.operand;

            *instr_location = core->rip + tm->cur_instr_len + (to_jmp ? offset : 0);
            tm->offset = offset;
            tm->to_branch = to_jmp;
            break;
        default:
            *instr_location = core->rip + tm->cur_instr_len;
            break;
    }
    return to_jmp;
}


/* entry points :
 *
 * called inside #UD and VMMCALL handlers
 * only affects global state in case of quix86 fall over
 *  -> set other cores TM_ABORT to 1, return -2
 */
static int 
v3_store_next_instr (struct guest_info * core, struct v3_trans_mem * tm) 
{
    struct x86_instr struct_instr;
    uchar_t cur_instr[INSTR_BUF_SZ];
    addr_t  instr_location;

    // Fetch the current instruction
    tm_read_instr(core, core->rip, cur_instr, INSTR_BUF_SZ);

    TM_DBG(core,STORE,"storing next instruction, current rip: %llx\n", (uint64_t)core->rip);

    /* Attempt to decode current instruction to determine its length */
    if (v3_decode(core, (addr_t)cur_instr, &struct_instr) == ERR_DECODE_FAIL) {
        
        TM_ERR(core,Error,"Could not decode currrent instruction (at %llx)\n", (uint64_t)core->rip);

        /* this will attempt to abort all the remote cores */
        if (tm_handle_decode_fail(core) == -1) {
            TM_ERR(core,Error,"Could not handle failed decode\n");
            return ERR_STORE_FAIL;
        }

        /* we need to trigger a local abort */
        return ERR_STORE_MUST_ABORT;
    }


    /* we can't currently handle REP prefixes, abort */
    if (struct_instr.op_type != V3_INVALID_OP &&
            (struct_instr.prefixes.repne ||
             struct_instr.prefixes.repnz ||
             struct_instr.prefixes.rep   ||
             struct_instr.prefixes.repe  ||
             struct_instr.prefixes.repz)) {

        TM_ERR(core,DECODE,"Encountered REP prefix, aborting\n");
        return ERR_STORE_MUST_ABORT;
    }

    tm->cur_instr_len = struct_instr.instr_length;

    /* handle jump instructions */
    tm_handle_ctrl_flow(core, tm, &instr_location, &struct_instr);

    /* save next 10 bytes after current instruction, we'll put vmmcall here */
    tm_read_instr(core, instr_location, cur_instr, INSTR_INJECT_LEN);

    /* store the next instruction and its length in info */
    memcpy(tm->dirty_instr, cur_instr, INSTR_INJECT_LEN);

    return 0;
}


static int 
v3_overwrite_next_instr (struct guest_info * core, struct v3_trans_mem * tm) 
{
    addr_t ptr;

    // save rax
    tm->clobbered_rax = (core->vm_regs).rax;

    ptr = core->rip;

    /* we can't currently handle instructions that span page boundaries */
    if ((ptr + tm->cur_instr_len) % PAGE_SIZE_4KB < (ptr % PAGE_SIZE_4KB)) {
        TM_ERR(core,OVERWRITE,"emulated instr straddling page boundary\n");
        return -1;
    }

    ptr = core->rip + tm->cur_instr_len + (tm->to_branch ? tm->offset : 0);

    if ((ptr + INSTR_INJECT_LEN) % PAGE_SIZE_4KB < (ptr % PAGE_SIZE_4KB)) {
        TM_ERR(core,OVERWRITE,"injected instr straddling page boundary\n");
        return -1;
    }

    if (v3_gva_to_hva(core,
                get_addr_linear(core, ptr, &(core->segments.cs)),
                &ptr) == -1) {

        TM_ERR(core,Error,"Calculating next rip hva failed\n");
        return -1;
    }

    TM_DBG(core,REPLACE,"Replacing next instruction at addr %llx with vmm hyper call, len=%d\n",
            core->rip + tm->cur_instr_len + (tm->to_branch ? tm->offset : 0), (int)tm->cur_instr_len );

    /* Copy VMM call into the memory address of beginning of next instruction (ptr) */
    memcpy((char*)ptr, vmmcall_bytes, INSTR_INJECT_LEN);

    /* KCH: flag that we've dirtied an instruction, and store its host address */
    tm->dirty_instr_flag = 1;
    tm->dirty_gva        = core->rip + tm->cur_instr_len + (tm->to_branch ? tm->offset : 0);
    tm->dirty_hva        = ptr;
    tm->to_branch        = 0;

    return 0;
}


/* entry points:
 *
 * this should only be called if TM_STATE == TM_NULL, additionally we check if our dirtied flag if set
 */
int 
v3_restore_dirty_instr (struct guest_info * core) 
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(core, "trans_mem");

    /* Restore next instruction, transition to IFETCH state */
    TM_DBG(core,RESTORE,"Restoring next instruction.\n");

    /* check if we've actually done an instruction overwrite */
    if (!(tm->dirty_instr_flag)) {
        TM_DBG(core,RESTORE,"nothing to restore here...\n");
        return 0;
    }

    // Actually restore instruction
    memcpy((char*)tm->dirty_hva, tm->dirty_instr, INSTR_INJECT_LEN);

    // Put rax back
    (core->vm_regs).rax = tm->clobbered_rax; 

    // Scoot rip back up
    TM_DBG(core,RESTORE,"RIP in vmmcall: %llx\n", core->rip);
    core->rip = tm->dirty_gva;

    // clean up
    tm->dirty_instr_flag = 0;
    tm->dirty_gva = 0;
    tm->dirty_hva = 0;
    memset(tm->dirty_instr, 0, 15);

    TM_DBG(core,RESTORE,"RIP after scooting it back up: %llx\n", core->rip);

    return 0;
}


static addr_t 
tm_handle_fault_ifetch (struct guest_info * core, 
                        struct v3_trans_mem * tm)
{
    int sto;

    TM_DBG(core,IFETCH,"Page fault caused by IFETCH: rip is the same as faulting address, we must be at an ifetch.\n");

    sto = v3_store_next_instr(core, tm);

    if (sto == ERR_STORE_FAIL) {
        TM_ERR(core,EXIT,"Could not store next instruction in transaction\n");
        return ERR_TRANS_FAULT_FAIL;
    } else if (sto == ERR_STORE_MUST_ABORT) {
        TM_DBG(core,EXIT,"aborting for some reason\n");
        v3_handle_trans_abort(core, TM_ABORT_UNSPECIFIED, 0);
        return TRANS_FAULT_OK;
    }

    if (v3_overwrite_next_instr(core, tm) == -1) {
        TM_ERR(core,PF,"problem overwriting instruction\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    tm->TM_STATE = TM_EXEC;

    return TRANS_FAULT_OK;
}


static addr_t
tm_handle_fault_read (struct guest_info * core, 
                      struct v3_trans_mem * tm,
                      addr_t fault_addr,
                      pf_error_t error)

{
    // This page fault was caused by a read to memory in the current instruction for a core in TM mode
    TM_DBG(core,DATA,"Page fault caused by read.\n");
    TM_DBG(core,PF,"Adding %p to read list and hash\n", (void*)fault_addr);

    if (add_mem_op_to_list(&(tm->trans_r_list), fault_addr) == -1) {
        TM_ERR(core,PF,"problem adding to list\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    if (tm_record_access(tm, error.write, fault_addr) == -1) {
        TM_ERR(core,PF,"problem recording access\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    /* if we have previously written to this address, we need to update our
     * staging page and map it in */
    if (list_contains_guest_addr(&(tm->trans_w_list), fault_addr)) {

        TM_DBG(core,PF,"Saw a read from something in the write list\n");

        /* write the value from linked list to staging page */
        if (stage_entry(tm, &(tm->trans_w_list), fault_addr) == -1) {
            TM_ERR(core,PF, "could not stage entry!\n");
            return ERR_TRANS_FAULT_FAIL;
        }

        /* Hand it the staging page */
        return (addr_t)(tm->staging_page);                

    } else {

        //Add it to the read set
        addr_t shadow_addr = 0;

        TM_DBG(core,PF,"Saw a read from a fresh address\n");

        if (v3_gva_to_hva(core, (uint64_t)fault_addr, &shadow_addr) == -1) {
            TM_ERR(core,PF,"Could not translate gva to hva for transaction read\n");
            return ERR_TRANS_FAULT_FAIL;
        }

    }

    return TRANS_FAULT_OK;
}


static addr_t
tm_handle_fault_write (struct guest_info * core,
                       struct v3_trans_mem * tm,
                       addr_t fault_addr,
                       pf_error_t error)
{
        void * data_loc;
        addr_t virt_data_loc;
        addr_t shadow_addr = 0;

        TM_DBG(core,DATA,"Page fault cause by write\n");
        TM_DBG(core,PF,"Adding %p to write list and hash\n", (void*)fault_addr);

        if (add_mem_op_to_list(&(tm->trans_w_list), fault_addr) == -1) {
            TM_ERR(core,WRITE,"could not add to list!\n");
            return ERR_TRANS_FAULT_FAIL;
        }

        if (tm_record_access(tm, error.write, fault_addr) == -1) {
            TM_ERR(core,WRITE,"could not record access!\n");
            return ERR_TRANS_FAULT_FAIL;
        }

        if (v3_gva_to_hva(core, (uint64_t)fault_addr, &shadow_addr) == -1) {
            TM_ERR(core,WRITE,"could not translate gva to hva for transaction read\n");
            return ERR_TRANS_FAULT_FAIL;
        }

        // Copy existing values to the staging page, populating that field
        // This avoids errors in optimized code such as ++, where the original
        // value is not read, but simply incremented
        data_loc = (void*)((addr_t)(tm->staging_page) + (shadow_addr % PAGE_SIZE_4KB)); 
        
        if (v3_hpa_to_hva((addr_t)(data_loc), &virt_data_loc) == -1) {
            TM_ERR(core,WRITE,"Could not convert address on staging page to virt addr\n");
            return ERR_TRANS_FAULT_FAIL;
        }

        TM_DBG(core,WRITE,"\tValue being copied (core %d): %p\n", core->vcpu_id, *((void**)(virt_data_loc)));
        //memcpy((void*)virt_data_loc, (void*)shadow_addr, sizeof(uint64_t));
        *(uint64_t*)virt_data_loc = *(uint64_t*)shadow_addr;

        return (addr_t)(tm->staging_page);                
}


static addr_t
tm_handle_fault_extern_ifetch (struct guest_info * core,
                               struct v3_trans_mem * tm,
                               addr_t fault_addr,
                               pf_error_t error)
{
    int sto;

    // system is in tm state, record the access
    TM_DBG(core,IFETCH,"Page fault caused by IFETCH: we are not in TM, recording.\n");

    sto = v3_store_next_instr(core,tm);

    if (sto == ERR_STORE_FAIL) {
        TM_ERR(core,Error,"Could not store next instruction in transaction\n");
        return ERR_TRANS_FAULT_FAIL;

    } else if (sto == ERR_STORE_MUST_ABORT) {
        TM_ERR(core,IFETCH,"decode failed, going out of single stepping\n");
        v3_handle_trans_abort(core, TM_ABORT_UNSPECIFIED, 0);
        return TRANS_FAULT_OK;
    }

    if (v3_overwrite_next_instr(core, tm) == -1) {
        TM_ERR(core,IFETCH,"could not overwrite next instr!\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    tm->TM_STATE = TM_EXEC;

    if (tm_record_access(tm, error.write, fault_addr) == -1) {
        TM_ERR(core,IFETCH,"could not record access!\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    return TRANS_FAULT_OK;
}


static addr_t
tm_handle_fault_extern_access (struct guest_info * core,
                               struct v3_trans_mem * tm,
                               addr_t fault_addr,
                               pf_error_t error)
{
    TM_DBG(core,PF_HANDLE,"recording access\n");
    if (tm_record_access(tm, error.write, fault_addr) == -1) {
        TM_ERR(core,PF_HANDLE,"could not record access!\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    return TRANS_FAULT_OK;
}


static addr_t
tm_handle_fault_tmoff (struct guest_info * core)
{
    TM_DBG(core,PF_HANDLE, "in pf handler but noone is in tm mode anymore (core %d), i should try to eliminate hypercalls\n", core->vcpu_id);

    if (v3_restore_dirty_instr(core) == -1) {
        TM_ERR(core,PF_HANDLE,"could not restore dirty instr!\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    return TRANS_FAULT_OK;
}


/* entry points:
 *
 * called from MMU - should mean at least tms->TM_MODE is on
 *
 * tm->on : ifetch -> store instr, overwrite instr
 *          r/w    -> record hash, write log, store instr, overwrite instr
 * tm->off: ifetch -> store instr, overwrite instr
 *          r/w    -> record hash, store instr, overwrite instr
 *
 *          returns ERR_TRANS_FAULT_FAIL on error
 *          TRANS_FAULT_OK when things are fine
 *          addr when we're passing back a staging page
 *
 */
addr_t 
v3_handle_trans_mem_fault (struct guest_info * core, 
                                  addr_t fault_addr, 
                                  pf_error_t error) 
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(core, "trans_mem");
    struct v3_tm_state * tms = (struct v3_tm_state *)v3_get_extension_state(core->vm_info, "trans_mem");

    if (!tm) {
        TM_ERR(core,ERROR,": coudln't get core state\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    if (!tms) {
        TM_ERR(core,ERROR,": couldn't get vm trans_mem state\n");
        return ERR_TRANS_FAULT_FAIL;
    }

    TM_DBG(core,PF,"PF handler core->mode : %d, system->mode : %d\n", tm->TM_MODE, tms->TM_MODE);

    if ((tm->TM_MODE == TM_ON) && 
        ((void *)fault_addr == (void *)(core->rip))) {

        return tm_handle_fault_ifetch(core, tm);

    } else if ((tm->TM_MODE == TM_ON)    && 
               (tm->TM_STATE == TM_EXEC) && 
               (error.write == 0)) {

        return tm_handle_fault_read(core, tm, fault_addr, error);

    } else if ((tm->TM_MODE == TM_ON)    && 
               (tm->TM_STATE == TM_EXEC) && 
               (error.write == 1)) {

        return tm_handle_fault_write(core, tm, fault_addr, error);


    } else if ((tms->TM_MODE == TM_ON) &&
              ((void *)fault_addr == (void *)(core->rip))) {

        return tm_handle_fault_extern_ifetch(core, tm, fault_addr, error);

    } else if ((tms->TM_MODE == TM_ON) && 
               (tm->TM_STATE == TM_EXEC)) {

        return tm_handle_fault_extern_access(core, tm, fault_addr, error);
    } else {

        return tm_handle_fault_tmoff(core);

    }

    return TRANS_FAULT_OK;
}


static int 
tm_handle_hcall_tmoff (struct guest_info * core, struct v3_trans_mem * tm)
{
    if (tm->TM_MODE == TM_ON) {
        TM_ERR(core,EXIT,"we are in tm mode but system is not!\n");
        return TRANS_HCALL_FAIL;
    }

    // we got to an exit when things were off!
    TM_DBG(core,EXIT,"system is off, restore the instruction and go away\n");

    if (v3_restore_dirty_instr(core) == -1) {
        TM_ERR(core,HCALL,"could not restore dirty instr!\n");
        return TRANS_HCALL_FAIL;
    }

    tm->TM_STATE = TM_NULL;

    return TRANS_HCALL_OK;
}


static int
tm_handle_hcall_dec_abort (struct guest_info * core, 
                           struct v3_trans_mem * tm)
{
    // only ever get here from TM DECODE
    TM_DBG(core,EXIT,"we are in ABORT, call the abort handler\n");
    tm->TM_ABORT = 0;

    v3_handle_trans_abort(core, TM_ABORT_UNSPECIFIED, 0);

    TM_DBG(core,EXIT,"RIP after abort: %p\n", ((void*)(core->rip)));

    return TRANS_HCALL_OK;
}


static int
tm_handle_hcall_ifetch_start (struct guest_info * core, 
                              struct v3_trans_mem * tm)
{
    tm->TM_STATE = TM_IFETCH;

    TM_DBG(core,EXIT,"VMEXIT after TM_EXEC, blast away VTLB and go into TM_IFETCH\n");

    // Finally, invalidate the shadow page table 
    v3_invalidate_shadow_pts(core);

    return TRANS_HCALL_OK;
}


static int 
tm_check_list_conflict (struct guest_info * core,
                        struct v3_trans_mem * tm,
                        struct list_head * access_list,
                        v3_tm_op_t op_type)
{
    struct mem_op * curr = NULL;
    struct mem_op * tmp  = NULL;
    int conflict = 0;

    list_for_each_entry_safe(curr, tmp, access_list, op_node) {

        conflict = tm_check_conflict(tm->ginfo->vm_info, curr->guest_addr, op_type, core->vcpu_id, tm->t_num);

        if (conflict == ERR_CHECK_FAIL) {

            TM_ERR(core,EXIT,"error checking for conflicts\n");
            return TRANS_HCALL_FAIL;

        } else if (conflict == CHECK_IS_CONFLICT) {

            TM_DBG(core,EXIT,"we have a conflict, aborting\n");
            v3_handle_trans_abort(core, TM_ABORT_CONFLICT, 0);
            return CHECK_MUST_ABORT;

        }

    }

    return TRANS_HCALL_OK;
}


static int 
tm_handle_hcall_check_conflicts (struct guest_info * core,
                                 struct v3_trans_mem * tm)
{
    int ret;

    TM_DBG(core,EXIT,"still TM_ON\n");
    TM_DBG(core,EXIT,"checking for conflicts\n");

    if ((ret = tm_check_list_conflict(core, tm, &(tm->trans_w_list), OP_TYPE_WRITE)) == TRANS_HCALL_FAIL) {
        return TRANS_HCALL_FAIL;
    } else if (ret == CHECK_MUST_ABORT) {
        return TRANS_HCALL_OK;
    }
    
    if ((ret = tm_check_list_conflict(core, tm, &(tm->trans_r_list), OP_TYPE_READ)) == TRANS_HCALL_FAIL) {
        return TRANS_HCALL_FAIL;
    } else if (ret == CHECK_MUST_ABORT) {
        return TRANS_HCALL_OK;
    }

    tm->TM_STATE = TM_IFETCH;

    return TRANS_HCALL_OK;
}


/* trans mem hypercall handler 
 * entry points:
 *
 * running mime (tm or tms on)
 *   update record log
 *   restore instr
 *   overwrite instr
 *   check for conflicts
 *   flush vtlb
 * abort (due to quix86)
 *   restore instr
 *   set all to abort
 */
static int 
tm_handle_hcall (struct guest_info * core, 
                 unsigned int hcall_id, 
                 void * priv_data) 
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(core, "trans_mem");
    struct v3_tm_state * tms = (struct v3_tm_state *)v3_get_extension_state(core->vm_info, "trans_mem");

    if (tms->TM_MODE == TM_OFF) {
        return tm_handle_hcall_tmoff(core, tm);
    }

    // Previous instruction has finished, copy staging page back into linked list!
    if (update_list(tm, &(tm->trans_w_list)) == -1) {
        TM_ERR(core,HCALL,"could not update_list!\n");
        return TRANS_HCALL_FAIL;
    }

    // Done handling previous instruction, must put back the next instruction, reset %rip and go back to IFETCH state
    TM_DBG(core,EXIT,"saw VMEXIT, need to restore previous state and proceed\n");

    if (v3_restore_dirty_instr(core) == -1) {
        TM_ERR(core,HCALL,"could not restore dirty instr!\n");
        return TRANS_HCALL_FAIL;
    }
    
    /* Check TM_STATE */
    if (tm->TM_ABORT == 1 && 
        tms->TM_MODE == TM_ON) {

        return tm_handle_hcall_dec_abort(core, tm);

    } else if (tm->TM_STATE == TM_EXEC) {
        return tm_handle_hcall_ifetch_start(core, tm);
    }

    /* Check TM_MODE */
    if (tm->TM_MODE == TM_ON && 
        tms->TM_MODE == TM_ON) {

        return tm_handle_hcall_check_conflicts(core, tm);

    } else if (tm->TM_MODE == TM_OFF) {
        TM_DBG(core,EXIT,"we are in TM_OFF\n");
    }

    return TRANS_HCALL_OK;
}


int 
v3_tm_inc_tnum (struct v3_trans_mem * tm) 
{
    addr_t irqstate;
    uint64_t new_ctxt;
    uint64_t * lt;

    lt = tm_global_state->last_trans;

    // grab global last_trans
    irqstate = v3_lock_irqsave(tm_global_state->lock);
    new_ctxt = ++(lt[tm->ginfo->vcpu_id]);
    v3_unlock_irqrestore(tm_global_state->lock, irqstate);

    tm->t_num++;
    /*
    TM_DBG(tm->ginfo,INC TNUM,"global state is |%d|%d|, my tnum is %d\n", (int)lt[0],
                                                                        (int)lt[1], (int)tm->t_num);
                                                                        */
    if (new_ctxt != tm->t_num) {
        TM_ERR(tm->ginfo,TM_INC_TNUM,"misaligned global and local context value\n");
        return -1;
    }

    return 0;
}


static void
tm_set_abort_status (struct guest_info * core, 
                     tm_abrt_cause_t cause, 
                     uint8_t xabort_reason)
{
    core->vm_regs.rax = 0;

    switch (cause) {
        case TM_ABORT_XABORT:
            // we put the xabort immediate in eax 31:24
            // cause is zero
            core->vm_regs.rax |= (xabort_reason << 24);
            break;
        case TM_ABORT_CONFLICT:
            // if this was a conflict from another core, it may work
            // if we try again
            core->vm_regs.rax |= (1 << ABORT_CONFLICT) | (1 << ABORT_RETRY);
            break;
        case TM_ABORT_INTERNAL:
        case TM_ABORT_BKPT:
            core->vm_regs.rax |= (1 << cause);
            break;
        default:
            TM_ERR(core, ABORT, "invalid abort cause\n");
            break;
    }
}


// xabort_reason is only used for XABORT instruction
int 
v3_handle_trans_abort (struct guest_info * core, 
                       tm_abrt_cause_t cause, 
                       uint8_t xabort_reason)
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(core, "trans_mem");

    // Free the staging page
    if (v3_free_staging_page(tm) == -1) {
        TM_ERR(core,ABORT,"problem freeing staging page\n");
        return -1;
    }

    // Clear the VTLB which still has our staging page in it
    if (v3_clr_vtlb(core) == -1) {
        TM_ERR(core,ABORT,"problem clearing vtlb\n");
        return -1;
    }

    // Free the lists
    v3_clear_tm_lists(tm);

    TM_DBG(core,ABORT -- handler,"TM_MODE: %d | RIP: %llx | XABORT RIP: %llx\n", tm->TM_MODE, (uint64_t)core->rip, (uint64_t)tm->fail_call);

    if (tm->TM_MODE == TM_ON) {
        TM_DBG(core,ABORT,"Setting RIP to %llx\n", (uint64_t)tm->fail_call);
        core->rip = tm->fail_call;

        // Turn TM off
        v3_clr_tm(tm);

        // transaction # ++
        v3_tm_inc_tnum(tm);
    }
    
    tm_set_abort_status(core, cause, xabort_reason);

    // time to garbage collect
    if (tm_hash_gc(tm) == -1) {
        TM_ERR(core,GC,"could not gc!\n");
        return -1;
    }

    return 0;
}


static uint_t 
tm_hash_fn (addr_t key) 
{
    return v3_hash_long(key, sizeof(void *));
}


static int 
tm_eq_fn (addr_t key1, addr_t key2) 
{
    return (key1 == key2);
}


static uint_t 
tm_hash_buf_fn (addr_t key) 
{
    return v3_hash_long(key, sizeof(addr_t));
}


static int 
tm_eq_buf_fn(addr_t key1, addr_t key2) 
{
    return (key1 == key2);
}


/* this checks if the remote access was done on the same
 * local transaction number as the current one */
static int 
tm_check_context (struct v3_vm_info * vm,
                  addr_t gva,
                  uint64_t core_num,
                  uint64_t curr_ctxt,
                  uint64_t * curr_lt,
                  v3_tm_op_t op_type)
{
    uint64_t  core_id_sub;
    struct v3_tm_access_type * type = NULL;

    for (core_id_sub = 0; core_id_sub < vm->num_cores; core_id_sub++) {
        struct v3_trans_mem * remote_tm;
        void * buf[3];
        addr_t key;

        /* skip the core that's doing the checking */
        if (core_id_sub == core_num) {
            continue;
        }

        remote_tm = v3_get_ext_core_state(&(vm->cores[core_id_sub]), "trans_mem");
        if (!remote_tm) {
            PrintError(vm, VCORE_NONE, "Could not get ext core state for core %llu\n", core_id_sub);
            return ERR_CHECK_FAIL;
        }

        buf[0] = (void *)gva;
        buf[1] = (void *)core_id_sub;
        buf[2] = (void *)curr_lt[core_id_sub];

        key = v3_hash_buffer((uchar_t*)buf, sizeof(void*)*3);

        type = (struct v3_tm_access_type *)HTABLE_SEARCH(remote_tm->access_type, key);

        if (type) {
            // conflict!
            if ( (op_type == OP_TYPE_WRITE && (type->w || type->r)) || // so basically if write?
                    (op_type != OP_TYPE_WRITE && type->w)) {
                return CHECK_IS_CONFLICT;
            }
        }
    }

    return CHECK_NO_CONFLICT;
}


/* check all the contexts in the list for a conflict */
static int 
tm_check_all_contexts (struct v3_vm_info * vm,
                       struct list_head * hash_list,
                       addr_t   gva,
                       v3_tm_op_t  op_type,
                       uint64_t core_num, 
                       uint64_t curr_ctxt) 
{
    struct hash_chain * curr = NULL;
    struct hash_chain * tmp  = NULL;
    uint64_t * curr_lt       = NULL;
    int ret = 0;

    list_for_each_entry_safe(curr, tmp, hash_list, lt_node) {

        curr_lt = curr->curr_lt;

        if (curr_lt[core_num] == curr_ctxt) {

            ret = tm_check_context(vm, gva, core_num, curr_ctxt, curr_lt, op_type);

            if (ret == ERR_CHECK_FAIL) {
                return ERR_CHECK_FAIL;
            } else if (ret == CHECK_IS_CONFLICT) {
                return CHECK_IS_CONFLICT;
            }

        }

    }

    return CHECK_NO_CONFLICT;
}


/* The following access patterns trigger an abort:
 * We: Read     |   Anyone Else: Write
 * We: Write    |   Anyone Else: Read, Write
 *
 * (pg 8-2 of haswell manual)
 *
 * returns ERR_CHECK_FAIL on error
 * returns CHECK_IS_CONFLICT if there is a conflict
 * returns CHECK_NO_CONFLICT  if there isn't
 */
int 
tm_check_conflict (struct v3_vm_info * vm,
                   addr_t gva,
                   v3_tm_op_t op_type,
                   uint64_t core_num, 
                   uint64_t curr_ctxt) 
{
    uint64_t core_id;

    /* loop over other cores -> core_id */
    for (core_id = 0; core_id < vm->num_cores; core_id++) {

        struct guest_info * core = NULL;
        struct v3_trans_mem * tm = NULL;
        struct list_head * hash_list;

        /* only check other cores */
        if (core_id == core_num) {
            continue;
        }

        core = &(vm->cores[core_id]);
        tm = (struct v3_trans_mem*)v3_get_ext_core_state(core, "trans_mem");
        
        if (!tm) {
            PrintError(vm, VCORE_NONE, "+++ TM ERROR +++ Couldn't get core state for core %llu\n", core_id);
            return ERR_CHECK_FAIL;
        }

        /* this core didn't access the address, move on */
        if (!(hash_list = (struct list_head *)HTABLE_SEARCH(tm->addr_ctxt, gva))) {
            continue;

        } else {

            /* loop over chained hash for gva, find fields with curr_ctxt -> curr_lt*/
            int ret = tm_check_all_contexts(vm, hash_list, gva, op_type, core_num, curr_ctxt);

            if (ret == ERR_CHECK_FAIL) {
                return ERR_CHECK_FAIL;
            } else if (ret == CHECK_IS_CONFLICT) {
                return CHECK_IS_CONFLICT;
            } 

        }
    }

    return CHECK_NO_CONFLICT;
}


static int 
tm_need_to_gc (struct v3_trans_mem * tm,
               struct hash_chain * curr,
               uint64_t * lt_copy,
               uint64_t tmoff)
{
    uint64_t to_gc = 1;
    uint64_t i;

    /* if none of the cores are in transactional context, 
     * we know we can collect this context 
     */
    if (!tmoff) {

        for (i = 0; i < tm->ginfo->vm_info->num_cores; i++) {
            /* if *any* of the cores are active in a transaction 
             * number that is current (listed in this context),
             * we know we can't collect this context, as it 
             * will be needed when that core's transaction ends
             */
            if (curr->curr_lt[i] >= lt_copy[i]) {
                to_gc = 0;
                break;
            }
        }

    }
    return to_gc;
}


static void 
tm_del_stale_ctxt (struct hash_chain * curr)
{
        list_del(&(curr->lt_node));
        V3_Free(curr->curr_lt);
        V3_Free(curr);
}


static void 
tm_del_acc_entry (struct v3_trans_mem * tm, addr_t key)
{
    v3_htable_remove(tm->access_type, key, 0);
    (tm->access_type_entries)--;
}


static int 
tm_collect_context (struct v3_trans_mem * tm, 
                    struct hashtable_iter * ctxt_iter, 
                    struct hash_chain * curr, 
                    uint64_t * begin_time,
                    uint64_t * end_time,
                    addr_t gva)
{
        uint64_t i;

        for (i = 0; i < tm->ginfo->vm_info->num_cores; i++) {
            void * buf[3];
            struct v3_tm_access_type * type;
            addr_t key;

            rdtscll(*end_time);
            if ((*end_time - *begin_time) > 100000000) {
                TM_ERR(tm->ginfo,GC,"time threshhold exceeded, exiting!!!\n");
                return -1;
            }
            
            buf[0] = (void *)gva;
            buf[1] = (void *)i;
            buf[2] = (void *)curr->curr_lt[i];

            key = v3_hash_buffer((uchar_t*)buf, sizeof(void*)*3);

            type = (struct v3_tm_access_type *)v3_htable_search(tm->access_type, key);

            if (!type) { // something has gone terribly wrong
                TM_ERR(tm->ginfo,GC,"could not find accesstype entry to gc, THIS! IS! WRONG!\n");
                return -1;
            }

            /* delete the access type entry */
            tm_del_acc_entry(tm, key);
        }

        /* delete the stale context */
        tm_del_stale_ctxt(curr);

        return 0;
}


static int 
tm_collect_all_contexts (struct v3_trans_mem * tm,
                         struct hashtable_iter * ctxt_iter,
                         uint64_t tmoff,
                         uint64_t * lt_copy,
                         uint64_t * begin_time,
                         uint64_t * end_time)
{
    struct hash_chain * tmp;
    struct hash_chain * curr;
    struct list_head * chain_list;
    addr_t gva;

    gva = (addr_t)v3_htable_get_iter_key(ctxt_iter);
    
    chain_list = (struct list_head *)v3_htable_get_iter_value(ctxt_iter);

    /* this is a chained hash, so for each address, we will have
     * a list of contexts. We now check each context to see
     * whether or not it can be collected
     */
    list_for_each_entry_safe(curr, tmp, chain_list, lt_node) {

        uint64_t to_gc = tm_need_to_gc(tm, curr, lt_copy, tmoff);

        /* not garbage, go on to the next context in the list */
        if (!to_gc) {
            TM_DBG(tm->ginfo,GC,"not garbage collecting entries for address %llx\n", (uint64_t)gva);
            continue;
        }

        TM_DBG(tm->ginfo,GC,"garbage collecting entries for address %llx\n", (uint64_t)gva);

        /* found one, delete corresponding entries in access_type */
        if (tm_collect_context(tm, ctxt_iter, curr, begin_time, end_time, gva) == -1) {
            TM_ERR(tm->ginfo,GC,"ERROR collecting context\n");
            return -1;
        }

    }

    /* if context list (hash chain) is now empty, remove the hash entry */
    if (list_empty(chain_list)) {
        v3_htable_iter_remove(ctxt_iter, 0);
        (tm->addr_ctxt_entries)--;
    } else {
        v3_htable_iter_advance(ctxt_iter);
    }

    /* give the CPU away NONONO NEVER YIELD WHILE HOLDING A LOCK */
    //V3_Yield();

    return 0;
}


int 
tm_hash_gc (struct v3_trans_mem * tm) 
{
    addr_t irqstate, irqstate2;
    int rc = 0;
    uint64_t begin_time, end_time, tmoff;
    uint64_t * lt_copy;
    struct v3_tm_state * tms = NULL;
    struct hashtable_iter * ctxt_iter = NULL;

    tms = (struct v3_tm_state *)v3_get_extension_state(tm->ginfo->vm_info, "trans_mem");
    if (!tms) {
        TM_ERR(tm->ginfo,GC,"could not alloc tms\n");
        return -1;
    }

    TM_DBG(tm->ginfo,GC,"beginning garbage collection\n");
    TM_DBG(tm->ginfo,GC,"\t %d entries in addr_ctxt (pre)\n", (int)v3_htable_count(tm->addr_ctxt));
    TM_DBG(tm->ginfo,GC,"\t %d entries in access_type (pre)\n", (int)v3_htable_count(tm->access_type));

    tmoff = (tms->cores_active == 0);

    lt_copy = V3_Malloc(sizeof(uint64_t)*(tm->ginfo->vm_info->num_cores));
    if (!lt_copy) {
        TM_ERR(tm->ginfo,GC,"Could not allocate space for lt_copy\n");
        return -1;
    }

    memset(lt_copy, 0, sizeof(uint64_t)*(tm->ginfo->vm_info->num_cores));

    rdtscll(begin_time);

    /* lt_copy holds the last transaction number for each core */
    irqstate = v3_lock_irqsave(tm_global_state->lock);
    memcpy(lt_copy, tm_global_state->last_trans, sizeof(uint64_t)*(tm->ginfo->vm_info->num_cores));
    v3_unlock_irqrestore(tm_global_state->lock, irqstate);

    /* lock both hashes */
    irqstate = v3_lock_irqsave(tm->addr_ctxt_lock);
    irqstate2 = v3_lock_irqsave(tm->access_type_lock);

    /* loop over hash entries in addr_ctxt */
    ctxt_iter = v3_create_htable_iter(tm->addr_ctxt);
    if (!ctxt_iter) {
        TM_ERR(tm->ginfo,GC,"could not create htable iterator\n");
        rc = -1;
        goto out;
    }

    /* we check each address stored in the hash */
    while (ctxt_iter->entry) {
        /* NOTE: this call advances the hash iterator */
        if (tm_collect_all_contexts(tm, ctxt_iter, tmoff, lt_copy, &begin_time, &end_time) == -1) {
            rc = -1;
            goto out1;
        }
    }

out1:
    v3_destroy_htable_iter(ctxt_iter);
out:
    V3_Free(lt_copy);
    v3_unlock_irqrestore(tm->access_type_lock, irqstate);
    v3_unlock_irqrestore(tm->addr_ctxt_lock, irqstate2);

    rdtscll(end_time);

    if (rc == -1) {
        TM_ERR(tm->ginfo,GC,"garbage collection failed, time spent: %d cycles\n", (int)(end_time - begin_time));
    } else {
        TM_DBG(tm->ginfo,GC,"ended garbage collection succesfuly, time spent: %d cycles\n", (int)(end_time - begin_time));
    }

    TM_DBG(tm->ginfo,GC,"\t %d entries in addr_ctxt (post)\n", (int)v3_htable_count(tm->addr_ctxt));
    TM_DBG(tm->ginfo,GC,"\t %d entries in access_type (post)\n", (int)v3_htable_count(tm->access_type));

    return rc;
}
    

/* TODO: break out the for loops in these functions */
static int
tm_update_ctxt_list (struct v3_trans_mem * tm, 
                     uint64_t * lt_copy,
                     addr_t gva,
                     uint8_t write,
                     struct list_head * hash_list)
{
    struct hash_chain * curr = NULL;
    struct hash_chain * tmp  = NULL;
    uint64_t num_cores = tm->ginfo->vm_info->num_cores;
    uint64_t core_id;
    uint_t new_le = 1;
    uint_t new_e;

    list_for_each_entry_safe(curr, tmp, hash_list, lt_node) {
        uint_t i;
        uint8_t same = 1;

        for (i = 0; i < num_cores; i++) {
            if (curr->curr_lt[i] != lt_copy[i]) {
                same = 0;
                break;
            }
        }

        if (same) {
            new_le = 0;
            break;
        }

    }

    if (new_le) {
        struct hash_chain * new_l = V3_Malloc(sizeof(struct hash_chain));

        if (!new_l) {
            TM_ERR(tm->ginfo,HASH,"Could not allocate new list\n");
            return -1;
        }

        memset(new_l, 0, sizeof(struct hash_chain));

        new_l->curr_lt = lt_copy;

        list_add_tail(&(new_l->lt_node), hash_list);
    }

    for (core_id = 0; core_id < num_cores; core_id++) {
        struct v3_tm_access_type * type;
        struct v3_ctxt_tuple tup;
        tup.gva     = (void*)gva;
        tup.core_id = (void*)core_id;
        tup.core_lt = (void*)lt_copy[core_id];
        addr_t key;

        key = v3_hash_buffer((uchar_t*)&tup, sizeof(struct v3_ctxt_tuple));

        new_e = 0;

        type = (struct v3_tm_access_type *)HTABLE_SEARCH(tm->access_type, key);

        if (!type) {
            // no entry yet
            new_e = 1;
            type = V3_Malloc(sizeof(struct v3_tm_access_type));

            if (!type) {
                TM_ERR(tm->ginfo,HASH,"could not allocate type access struct\n");
                return -1;
            }
        }

        if (write) {
            type->w = 1;
        } else {
            type->r = 1;
        }

        if (new_e) {
            if (HTABLE_INSERT(tm->access_type, key, type) == 0) {
                TM_ERR(tm->ginfo,HASH,"problem inserting new mem access in htable\n");
                return -1;
            }
            (tm->access_type_entries)++;
        }
    }

    return 0;
}


/* no entry in addr-ctxt yet, create one */
static int
tm_create_ctxt_key (struct v3_trans_mem * tm,
                    uint64_t * lt_copy,
                    addr_t gva,
                    uint8_t write)
{
    struct list_head * hash_list = NULL;
    struct hash_chain * new_l = NULL;
    uint64_t num_cores = tm->ginfo->vm_info->num_cores;

    hash_list = (struct list_head *)V3_Malloc(sizeof(struct list_head));

    if (!hash_list) {
        TM_ERR(tm->ginfo,HASH,"Problem allocating hash_list\n");
        return -1;
    }

    INIT_LIST_HEAD(hash_list);

    new_l = V3_Malloc(sizeof(struct hash_chain));

    if (!new_l) {
        TM_ERR(tm->ginfo,HASH,"Problem allocating hash_chain\n");
        goto out_err;
    }

    memset(new_l, 0, sizeof(struct hash_chain));

    new_l->curr_lt = lt_copy;

    /* add the context to the hash chain */
    list_add_tail(&(new_l->lt_node), hash_list);

    if (!(HTABLE_INSERT(tm->addr_ctxt, gva, hash_list))) {
        TM_ERR(tm->ginfo,HASH CHAIN,"problem inserting new chain into hash\n");
        goto out_err1;
    }

    (tm->addr_ctxt_entries)++;

    uint64_t core_id;
    /* TODO: we need a way to unwind and deallocate for all cores on failure here */
    for (core_id = 0; core_id < num_cores; core_id++) {
        struct v3_tm_access_type * type = NULL;
        struct v3_ctxt_tuple tup;
        tup.gva     = (void*)gva;
        tup.core_id = (void*)core_id;
        tup.core_lt = (void*)lt_copy[core_id];
        addr_t key;

        type = V3_Malloc(sizeof(struct v3_tm_access_type));

        if (!type) {
            TM_ERR(tm->ginfo,HASH,"could not allocate access type struct\n");
            goto out_err1;
        }

        if (write) {
            type->w = 1;
        } else {
            type->r = 1;
        }

        key = v3_hash_buffer((uchar_t*)&tup, sizeof(struct v3_ctxt_tuple));

        if (HTABLE_INSERT(tm->access_type, key, type) == 0) {
            TM_ERR(tm->ginfo,HASH,"TM: problem inserting new mem access in htable\n");
            goto out_err1;
        }
        (tm->access_type_entries)++;
    }

    return 0;

out_err1:
    list_del(&(new_l->lt_node));
    V3_Free(new_l);
out_err:
    V3_Free(hash_list);
    return -1;
}


/* entry points:
 *
 * called during MIME execution
 * record memory access in conflict logs
 *   this locks the table during insertion
 */
int 
tm_record_access (struct  v3_trans_mem * tm, 
                  uint8_t write,
                  addr_t  gva) 
{
    uint64_t * lt_copy;
    struct list_head * hash_list;
    addr_t irqstate;
    uint64_t num_cores;

    num_cores = tm->ginfo->vm_info->num_cores;

    TM_DBG(tm->ginfo,REC,"recording addr %llx, addr-ctxt.cnt = %d, access-type.cnt = %d\n", (uint64_t)gva,
                                        (int)v3_htable_count(tm->addr_ctxt), (int)v3_htable_count(tm->access_type));
    //PrintDebug(tm->ginfo->vm_info, tm->ginfo,"\tWe think that addr-ctxt.cnt = %d, access-type.cnt = %d\n",(int)tm->addr_ctxt_entries,(int)tm->access_type_entries);

    lt_copy = V3_Malloc(sizeof(uint64_t)*num_cores);
    if (!lt_copy) {
        TM_ERR(tm->ginfo,REC,"Allocating array failed\n");
        return -1;
    }

    memset(lt_copy, 0, sizeof(uint64_t)*num_cores);

    irqstate = v3_lock_irqsave(tm_global_state->lock);
    memcpy(lt_copy, tm_global_state->last_trans, sizeof(uint64_t)*num_cores);
    v3_unlock_irqrestore(tm_global_state->lock, irqstate);

    if (!(hash_list = (struct list_head *)HTABLE_SEARCH(tm->addr_ctxt, gva))) {
        /* we haven't created a context list for this address yet, go do it */
        return tm_create_ctxt_key(tm, lt_copy, gva, write);

    } else {
        /* we have a context list for this addres already, do we need to create a new context? */
        return tm_update_ctxt_list(tm, lt_copy, gva, write, hash_list);
    }

    return 0;
}


static void
tm_prepare_cpuid (struct v3_vm_info * vm)
{

    V3_Print(vm, VCORE_NONE, "TM INIT | enabling RTM cap in CPUID\n");

    /* increase max CPUID function to 7 (extended feature flags enumeration) */
    v3_cpuid_add_fields(vm,0x0,    
            0xf, 0x7,      
            0, 0,
            0, 0,
            0, 0);


    /* do the same for AMD */
    v3_cpuid_add_fields(vm,0x80000000, 
            0xffffffff, 0x80000007,   
            0, 0,
            0, 0,
            0, 0);


    /* enable RTM (CPUID.07H.EBX.RTM = 1) */
    v3_cpuid_add_fields(vm, 0x07, 0, 0, (1<<11), 0, 0, 0, 0, 0);
    v3_cpuid_add_fields(vm, 0x80000007, 0, 0, (1<<11), 0, 0, 0, 0, 0);
}


static int 
init_trans_mem (struct v3_vm_info * vm, 
                v3_cfg_tree_t * cfg, 
                void ** priv_data) 
{
    struct v3_tm_state * tms; 
     
    PrintDebug(vm, VCORE_NONE, "Trans Mem. Init\n");

    tms = V3_Malloc(sizeof(struct v3_tm_state));
    if (!tms) {
        PrintError(vm, VCORE_NONE, "Problem allocating v3_tm_state\n");
        return -1;
    }

    memset(tms, 0, sizeof(struct v3_tm_state));

    if (v3_register_hypercall(vm, TM_KICKBACK_CALL, tm_handle_hcall, NULL) == -1) {
      PrintError(vm, VCORE_NONE, "TM could not register hypercall\n");
      goto out_err;
    }

    v3_lock_init(&(tms->lock));

    tms->TM_MODE      = TM_OFF;
    tms->cores_active = 0;

    uint64_t * lt = V3_Malloc(sizeof(uint64_t) * vm->num_cores);
    if (!lt) {
        PrintError(vm, VCORE_NONE, "Problem allocating last_trans array\n");
        goto out_err1;
    }

    memset(lt, 0, sizeof(uint64_t) * vm->num_cores);

    int i;
    for (i = 0; i < vm->num_cores; i++) {
        lt[i] = 0;
    }

    tms->last_trans = lt;

    *priv_data = tms;
    tm_global_state = tms;

    tm_prepare_cpuid(vm);

    return 0;

out_err1:
    v3_lock_deinit(&(tms->lock));
    v3_remove_hypercall(vm, TM_KICKBACK_CALL);
out_err:
    V3_Free(tms);
    return -1;
}


static int 
init_trans_mem_core (struct guest_info * core, 
                     void * priv_data, 
                     void ** core_data) 
{
    struct v3_trans_mem * tm = V3_Malloc(sizeof(struct v3_trans_mem));
 
    TM_DBG(core,INIT, "Trans Mem. Core Init\n");

    if (!tm) {
        TM_ERR(core,INIT, "Problem allocating TM state\n");
        return -1;
    }

    memset(tm, 0, sizeof(struct v3_trans_mem));

    INIT_LIST_HEAD(&tm->trans_r_list);
    INIT_LIST_HEAD(&tm->trans_w_list);

    tm->addr_ctxt  = v3_create_htable(0, tm_hash_fn, tm_eq_fn);
    if (!(tm->addr_ctxt)) {
        TM_ERR(core,INIT,"problem creating addr_ctxt\n");
        goto out_err;
    }

    tm->access_type = v3_create_htable(0, tm_hash_buf_fn, tm_eq_buf_fn);
    if (!(tm->access_type)) {
        TM_ERR(core,INIT,"problem creating access_type\n");
        goto out_err1;
    }
    
    v3_lock_init(&(tm->addr_ctxt_lock));
    v3_lock_init(&(tm->access_type_lock));

    tm->TM_STATE = TM_NULL;
    tm->TM_MODE  = TM_OFF;
    tm->TM_ABORT = 0;
    tm->ginfo    = core;
    tm->t_num = 0;
    tm->to_branch = 0;
    tm->offset = 0;
    tm->access_type_entries = 0;
    tm->addr_ctxt_entries = 0;
    tm->dirty_instr_flag = 0;

    /* TODO: Cache Model */
    //tm->box = (struct cache_box *)V3_Malloc(sizeof(struct cache_box *));
    //tm->box->init = init_cache;
    //tm->box->init(sample_spec, tm->box);

    *core_data = tm;

    return 0;

out_err1:
    v3_free_htable(tm->addr_ctxt, 0, 0);
out_err:
    V3_Free(tm);
    return -1;
}


static int 
deinit_trans_mem (struct v3_vm_info * vm, void * priv_data) 
{
    struct v3_tm_state * tms = (struct v3_tm_state *)priv_data;

    if (v3_remove_hypercall(vm, TM_KICKBACK_CALL) == -1) {
        PrintError(vm, VCORE_NONE, "Problem removing TM hypercall\n");
        return -1;
    }

    v3_lock_deinit(&(tms->lock));

    if (tms) {
        V3_Free(tms);
    }

    return 0;
}


static int 
deinit_trans_mem_core (struct guest_info * core, 
                       void * priv_data, 
                       void * core_data) 
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)core_data;
    struct hashtable_iter * ctxt_iter = NULL;

    v3_clear_tm_lists(tm);

    if (tm->staging_page) {
        TM_ERR(core,DEINIT CORE,"WARNING: staging page not freed!\n");
    }

    ctxt_iter = v3_create_htable_iter(tm->addr_ctxt);
    if (!ctxt_iter) {
        TM_DBG(core,DEINIT_CORE,"could not create htable iterator\n");
        return -1;
    }

    /* delete all context entries for each hashed address */
    while (ctxt_iter->entry) {
        struct hash_chain * tmp;
        struct hash_chain * curr;
        struct list_head * chain_list;
        addr_t gva;

        gva = (addr_t)v3_htable_get_iter_key(ctxt_iter);
        chain_list = (struct list_head *)v3_htable_get_iter_value(ctxt_iter);

        /* delete the context */
        list_for_each_entry_safe(curr, tmp, chain_list, lt_node) {
            tm_del_stale_ctxt(curr);
        }

        v3_htable_iter_advance(ctxt_iter);
    }

    v3_destroy_htable_iter(ctxt_iter);

    /* we've already deleted the values in this one */
    v3_free_htable(tm->addr_ctxt, 0, 0);

    /* KCH WARNING: we may not want to free access type values here */
    v3_free_htable(tm->access_type, 1, 0);

    v3_lock_deinit(&(tm->addr_ctxt_lock));
    v3_lock_deinit(&(tm->access_type_lock));

    if (tm) {
        V3_Free(tm);
    }

    return 0;
}


static struct v3_extension_impl trans_mem_impl = {
    .name = "trans_mem",
    .init = NULL,
    .vm_init = init_trans_mem,
    .vm_deinit = deinit_trans_mem,
    .core_init = init_trans_mem_core,
    .core_deinit = deinit_trans_mem_core,
    .on_entry = NULL,
    .on_exit = NULL
};

register_extension(&trans_mem_impl);


/* entry conditions
 * tms->on => commit our list, free sp, clear our lists, clr_tm will handle global state, then gc
 * tms->off => commit our list, free sp, clear our lists, clr_tm will handle global state, then gc
 */
static int 
tm_handle_xend (struct guest_info * core,
                struct v3_trans_mem * tm)
{
    rdtscll(tm->exit_time);

    /* XEND should raise a GPF when RTM mode is not on */
    if (tm->TM_MODE != TM_ON) {
        TM_ERR(core, UD, "Encountered XEND while not in a transactional region\n");
        v3_free_staging_page(tm);
        v3_clr_vtlb(core);
        v3_clear_tm_lists(tm);
        v3_raise_exception(core, GPF_EXCEPTION);
        return 0;
    }

    /* Our transaction finished! */
    /* Copy over data from the staging page */
    TM_DBG(core, UD,"Copying data from our staging page back into 'real' memory\n");

    if (commit_list(core, tm) == -1) {
        TM_ERR(core,UD,"error commiting tm list to memory\n");
        return -1;
    }

    TM_DBG(core,UD,"Freeing staging page and internal data structures\n");

    // Free the staging page
    if (v3_free_staging_page(tm) == -1) {
        TM_ERR(core,XEND,"couldnt free staging page\n");
        return -1;
    }

    // clear vtlb, as it may still contain our staging page
    if (v3_clr_vtlb(core) == -1) {
        TM_ERR(core,XEND,"couldnt clear vtlb\n");
        return -1;
    }

    // Clear the lists
    v3_clear_tm_lists(tm);

    /* Set the state and advance the RIP */
    TM_DBG(core,XEND,"advancing rip to %llx\n", core->rip + XEND_INSTR_LEN);
    core->rip += XEND_INSTR_LEN; 

    v3_clr_tm(tm);

    // time to garbage collect
    v3_tm_inc_tnum(tm);
    if (tm_hash_gc(tm) == -1) {
        TM_ERR(core,XEND,"could not gc!\n");
        return -1;
    }

    return 0;
}


/* entry conditions
 * tms->on => handle our abort code, handle_trans_abort will clear necessary state
 * tms->off => handle our abort code, handle_trans_abort will clear necessary state
 */
static int
tm_handle_xabort (struct guest_info * core,
                  struct v3_trans_mem * tm,
                  uchar_t * instr)
{
        uint8_t reason; 

        // we must reflect the immediate back into EAX 31:24
        reason = *(uint8_t*)(instr+2);

        /* TODO: this probably needs to move somewhere else */
        rdtscll(tm->exit_time);

        // Error checking! make sure that we have gotten here in a legitimate manner
        if (tm->TM_MODE != TM_ON) {
            TM_DBG(core, UD, "We got here while not in a transactional core!\n");
            v3_raise_exception(core, UD_EXCEPTION);
        }

        TM_DBG(core,UD,"aborting\n");

        if (tm->TM_STATE != TM_NULL) {
            v3_restore_dirty_instr(core);
        }

        // Handle the exit
        v3_handle_trans_abort(core, TM_ABORT_XABORT, reason);

        return 0;
}


/* entry conditions
 * tms->on => we set up our running env, set_tm will clear other vtlb's to start single stepping
 * tms->off => we set up our running env, set_tm will not clear anyone elses vtlb
 */
static int
tm_handle_xbegin (struct guest_info * core,
                  struct v3_trans_mem * tm,
                  uchar_t * instr)
{
    sint32_t rel_addr = 0;
    uint8_t out_of_bounds = 0;
    uint8_t in_compat_no_long = 0;

    if (tm->TM_MODE == TM_ON) {
        /* TODO: this is actually an indication of nesting, we'll fix this later */
        TM_ERR(core,UD,"We got here while already in a transactional region!");
        v3_raise_exception(core, UD_EXCEPTION);
        return -1;
    }

    // Save the fail_call address (first 2 bytes = opcode, last 4 = fail call addr)
    rel_addr = *(sint32_t*)(instr+2);

    /* raise a GPF if we're trying to set a fail call outside of code segment */
    in_compat_no_long = (core->cpu_mode == LONG_32_COMPAT) || ((struct efer_64*)&(core->ctrl_regs.efer))->lma == 0;
    out_of_bounds     = (core->rip + rel_addr > core->segments.cs.base + core->segments.cs.limit || 
                         core->rip + rel_addr < core->segments.cs.base);

    if (in_compat_no_long && out_of_bounds) {
        v3_raise_exception(core, GPF_EXCEPTION);
        return 0;
    }

    /* TODO: also raise GPF if we're in long mode and failcall isn't canonical */

    /* TODO: put this elsewhere */
    rdtscll(tm->entry_time);
    tm->entry_exits = core->num_exits;

    /* set the tm_mode for this core */
    v3_set_tm(tm);

    TM_DBG(core,UD,"Set the system in TM Mode, save fallback address");


    tm->fail_call = core->rip + XBEGIN_INSTR_LEN + rel_addr;

    TM_DBG(core,UD,"we set fail_call to %llx, rip is %llx, rel_addr is %x", (uint64_t)tm->fail_call,(uint64_t)core->rip,rel_addr);

    /* flush the shadow page tables */
    TM_DBG(core,UD,"Throwing out the shadow table");
    v3_clr_vtlb(core);

    // Increase RIP, ready to go to next instruction
    core->rip += XBEGIN_INSTR_LEN; 

    return 0;
}


/* entry conditions
 * tms->on => we set up our running env, set_tm will clear other vtlb's to start single stepping
 * tms->off => we set up our running env, set_tm will not clear anyone elses vtlb
 */
static int
tm_handle_xtest (struct guest_info * core,
                 struct v3_trans_mem * tm)
{
    // if we are in tm mode, set zf to 0, otherwise 1
    if (tm->TM_MODE == TM_ON) {
        core->ctrl_regs.rflags &= ~(1ULL << 6);
    } else {
        core->ctrl_regs.rflags |= (1ULL << 6);
    }

    core->rip += XTEST_INSTR_LEN;

    return 0;
}


/* instructions:
 * XBEGIN c7 f8 rel32
 * XABORT c6 f8 imm8
 * XEND   0f 01 d5
 */
static int 
tm_handle_ud (struct guest_info * core) 
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(core, "trans_mem");
    uchar_t instr[INSTR_BUF_SZ];
    uint8_t byte1, byte2, byte3;

    tm_read_instr(core, (addr_t)core->rip, instr, INSTR_BUF_SZ);

    byte1 = *(uint8_t *)((addr_t)instr);
    byte2 = *(uint8_t *)((addr_t)instr + 1);
    byte3 = *(uint8_t *)((addr_t)instr + 2);


    if (byte1 == 0xc7 && byte2 == 0xf8) {  /* third byte is an immediate */

        TM_DBG(core,UD,"Encountered Haswell-specific XBEGIN %x %x %d at %llx", byte1, byte2, byte3, (uint64_t)core->rip);

        if (tm_handle_xbegin(core, tm, instr) == -1) {
            TM_ERR(core, UD, "Problem handling XBEGIN\n");
            return -1;
        }

    } else if (byte1 == 0xc6 && byte2 == 0xf8) { /* third byte is an immediate */

        TM_DBG(core, UD, "Encountered Haswell-specific XABORT %x %x %d at %llx\n", byte1, byte2, byte3, (uint64_t)core->rip);

        if (tm_handle_xabort(core, tm, instr) == -1) {
            TM_ERR(core, UD, "Problem handling XABORT\n");
            return -1;
        }

    } else if (byte1 == 0x0f && byte2 == 0x01 && byte3 == 0xd5) {

        TM_DBG(core, UD, "Encountered Haswell-specific XEND %x %x %d at %llx\n", byte1, byte2, byte3, (uint64_t)core->rip);

        if (tm_handle_xend(core, tm) == -1) {
            TM_ERR(core, UD, "Problem handling XEND\n");
            return -1;
        }


    } else if (byte1 == 0x0f && byte2 == 0x01 && byte3 == 0xd6) {  /* third byte is an immediate */

        TM_DBG(core,UD,"Encountered Haswell-specific XTEST %x %x %x at %llx\n", byte1, byte2, byte3, (uint64_t)core->rip);

        if (tm_handle_xtest(core, tm) == -1) {
            TM_ERR(core, UD, "Problem handling XTEST\n");
            return -1;
        }

    } else {

        /* oh no, this is still unknown, pass the error back to the guest! */
        TM_DBG(core,UD,"Encountered:%x %x %x\n", byte1, byte2, byte3);
        v3_raise_exception(core, UD_EXCEPTION);
    }

    return 0;
}


int 
v3_tm_handle_exception (struct guest_info * info,
                        addr_t exit_code)
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");

    if (!tm) {
        TM_ERR(info,ERR,"TM extension state not found\n");
        return -1;
    } 

    switch (exit_code) {
        /* any of these exceptions should abort current transactions */
        case SVM_EXIT_EXCP6:
            if (tm_handle_ud(info) == -1) {
                return -1;
            }
            break;
        case SVM_EXIT_EXCP0:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, DE_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to DE exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP1:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, DB_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to DB exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP3:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, BP_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to BP exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP4:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, OF_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to OF exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP5:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, BR_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to BR exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP7:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, NM_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to NM exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP10:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, TS_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to TS exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP11:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, NP_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to NP exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP12:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, SS_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to SS exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP13:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, GPF_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to GPF exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP16:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, MF_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to MF exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP17:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, AC_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to AC exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;
        case SVM_EXIT_EXCP19:
            if (tm->TM_MODE != TM_ON) {
                v3_raise_exception(info, XF_EXCEPTION);
            }
            else {
                TM_DBG(info,EXCP,"aborting due to XF exception\n");
                v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            }
            break;

            TM_DBG(info,EXCP,"exception # %d\n", (int)exit_code - 0x40);
    }

    return 0;
}


void 
v3_tm_set_excp_intercepts (vmcb_ctrl_t * ctrl_area) 
{
    ctrl_area->exceptions.de = 1; // 0  : divide by zero
    ctrl_area->exceptions.db = 1; // 1  : debug
    ctrl_area->exceptions.bp = 1; // 3  : breakpoint
    ctrl_area->exceptions.of = 1; // 4  : overflow
    ctrl_area->exceptions.br = 1; // 5  : bound range
    ctrl_area->exceptions.ud = 1; // 6  : undefined opcode
    ctrl_area->exceptions.nm = 1; // 7  : device not available
    ctrl_area->exceptions.ts = 1; // 10 : invalid tss
    ctrl_area->exceptions.np = 1; // 11 : segment not present
    ctrl_area->exceptions.ss = 1; // 12 : stack
    ctrl_area->exceptions.gp = 1; // 13 : general protection
    ctrl_area->exceptions.mf = 1; // 16 : x87 exception pending
    ctrl_area->exceptions.ac = 1; // 17 : alignment check
    ctrl_area->exceptions.xf = 1; // 19 : simd floating point
}


extern void v3_stgi();
extern void v3_clgi();

/* 441-tm: if we are in TM mode, we need to check for any interrupts here,
 * and if there are any, need to do some aborting! Make sure not to die here
 * if we are already 'aborting', this results in infiloop
 */
void 
v3_tm_check_intr_state (struct guest_info * info, 
                        vmcb_ctrl_t * guest_ctrl,
                        vmcb_saved_state_t * guest_state)
                        
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");

    if (!tm) {
        TM_ERR(info,INTR,"TM extension state not found\n");
        v3_stgi();
        return;
    }

    /* TODO: work this in */
    if (0 && (tm->TM_MODE == TM_ON) && 
             (tm->TM_ABORT != 1)) {

        if (guest_ctrl->guest_ctrl.V_IRQ ||
            guest_ctrl->EVENTINJ.valid) {

            rdtscll(tm->exit_time);
            TM_DBG(info,INTR,"%lld exits happened, time delta is %lld",(info->num_exits - tm->entry_exits),(tm->entry_time - tm->exit_time));

            // We do indeed have pending interrupts
            v3_stgi();
            TM_DBG(info,INTR,"we have a pending interrupt!\n");

            v3_handle_trans_abort(info, TM_ABORT_UNSPECIFIED, 0);
            // Copy new RIP state into arch dependent structure
            guest_state->rip = info->rip;
            TM_DBG(info,INTR,"currently guest state rip is %llx\n",(uint64_t)guest_state->rip);
            v3_clgi();
        }

    }

}


int
v3_tm_handle_pf_64 (struct guest_info * info,
                    pf_error_t error_code,
                    addr_t fault_addr,
                    addr_t * page_to_use)
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");
    struct v3_tm_state * tms = (struct v3_tm_state *)v3_get_extension_state(info->vm_info, "trans_mem");

    if (!tm) {
        TM_ERR(info,HANDLE_PF, "couldn't get tm core state\n");
        return -1;
    }

    if (!tms) {
        TM_ERR(info,HANDLE_PF, "couldn't get tm global state\n");
        return -1;
    }

    if ((tms->TM_MODE == TM_ON) && 
            (error_code.user == 1)) {

        TM_DBG(info,PF,"Core reporting in, got a #PF (tms->mode is %d)\n", tms->TM_MODE);

        *page_to_use = v3_handle_trans_mem_fault(info, fault_addr,  error_code);

        if (*page_to_use == ERR_TRANS_FAULT_FAIL){
            TM_ERR(info,HANDLE_PF, "could not handle transaction page fault\n");
            return -1;
        }

        if ((tm->TM_MODE == TM_ON) && 
                (tm->staging_page == NULL)) {

            tm->staging_page = V3_AllocPages(1);

            if (!(tm->staging_page)) {
                TM_ERR(info,MMU,"Problem allocating staging page\n");
                return -1;
            }

            TM_DBG(info,MMU,"Created staging page at %p\n", (void *)tm->staging_page);
        }
    }

    return 0;
}


void 
v3_tm_handle_usr_tlb_miss (struct guest_info * info,
                           pf_error_t error_code,
                           addr_t page_to_use,
                           addr_t * shadow_pa)
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");

    /* TLB miss from user */
    if ((tm->TM_MODE == TM_ON) && 
            (error_code.user == 1)) {

        if (page_to_use > TRANS_FAULT_OK) {
            TM_DBG(info,MMU, "Using alternate page at: %llx\n", (uint64_t)page_to_use);
            *shadow_pa = page_to_use;
        }

    }

}


void
v3_tm_handle_read_fault (struct guest_info * info,
                         pf_error_t error_code,
                         pte64_t * shadow_pte)
{
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");
    struct v3_tm_state * tms = (struct v3_tm_state *)v3_get_extension_state(info->vm_info, "trans_mem");

    // If we are about to read, make it read only 
    if ((tms->TM_MODE == TM_ON) && 
        (tm->TM_STATE == TM_EXEC) && 
        (error_code.write == 0) && 
        (error_code.user == 1)) {

        TM_DBG(info,MMU, "Flagging the page read only\n");
        shadow_pte->writable = 0;
    }
}


int 
v3_tm_decode_rtm_instrs (struct guest_info * info,
                         addr_t instr_ptr,
                         struct x86_instr * instr)
{
    uint8_t byte1, byte2, byte3;
    struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");

    if (tm->TM_MODE == TM_ON) {

        byte1 = *(uint8_t *)(instr_ptr);
        byte2 = *(uint8_t *)(instr_ptr + 1);
        byte3 = *(uint8_t *)(instr_ptr + 2);

        if (byte1 == 0xc7 && 
            byte2 == 0xf8) {  /* third byte is an immediate */

            TM_DBG(info, DECODE,"Decoding XBEGIN %x %x %d\n", byte1, byte2, byte3);
            instr->instr_length = 6;
            return 0;

        } else if (byte1 == 0xc6 && 
                   byte2 == 0xf8) { /* third byte is an immediate */

            TM_DBG(info, DECODE, "Decoding XABORT %x %x %d\n", byte1, byte2, byte3);
            instr->instr_length = 3;
            return 0;

        } else if (byte1 == 0x0f && 
                   byte2 == 0x01 && 
                   byte3 == 0xd5) {

            TM_DBG(info, DECODE, "Decoding XEND %x %x %x\n", byte1, byte2, byte3);
            instr->instr_length = 3;
            return 0;

        }

    }

    return 0;
}


