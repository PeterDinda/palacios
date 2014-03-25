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
 * Author: Maciek Swiech <dotpyfe@u.northwestern.edu>
 *          Marcel Flores <marcel-flores@u.northwestern.edu>
 *          Zachary Bischof <zbischof@u.northwestern.edu>
 *          Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 *
 
RTM Implementation Wishlist (roughly in order of priority)
Kyle Hale, Maciek Swiech 2014

From Intel Architecture Instruction Set Extensions Programming Reference, Section 8.3, p.8-6
link: http://software.intel.com/sites/default/files/m/9/2/3/41604

- architectural registers need to be saved / restored 
- exceptions that misuse of TSX instructions can raise
- abort on interrupts, asynchronous events
- abort on CPUID, PAUSE
- abort on non-writeback memory ops, including ifetches to uncacheable mem
- RTM-debugger support
- RTM nesting
- parameterized cache model, for generating hardware configuration-based aborts

- to be able to model specific implementations, add options (runtime or compiletime) to abort on:
    * x86/mmx state changes, (also fxstor, fxsave),
    * cli, sti, popfd, popfq, clts
    * mov to segment regs, pop segment regs, lds, les, lfs, lgs, lss, swapgs, wrfsbase, wrgsbase, lgdt, sgdt, lidt, sidt, lldt, sldt, ltr, 
      str, far call, far jmp, far ret, far iret, mov to DRx, mov to cr0-4, cr8 lmsw
    * sysenter, syscall, sysexit, sysret
    * clflush, invd, wbinvd, invlpg, invpcid
    * memory instructions with temporal hints (e.g. movntdqa)
    * xsave, xsaveopt, xrstor
    * interrupts: INTn, INTO
    * IO: in, ins, rep ins, out, outs, rep outs, and variants
    * VMX instructions
    * smx: getsec
    * ud2, rsm, rdmsr, wrmsr, hlt, monitor, mwait, xsetbv, vzeroupper, maskmovq, v/maskmovdqu
    
 *
 *
 * We claim that we can have a single, shared "cache"-like box
 * that handles all writes and reads when TM is on on any core.  The
 * idea is that if TM is on on any core, we redirect reads/writes
 * that we get to the box, and it records them internally for 
 * future playback, and tells us whether an abort condition has
 * occured or not:
 *
 * error = handle_start_tx(boxstate,vcorenum);
 * error = handle_abort(boxstate,vcorenum);
 * error = handle_commit(boxstate,vcorenum);
 *
 * should_abort = handle_write(boxstate, vcorenum, physaddr, data, datalen);
 * should_abort = handle_read(boxstate, vcorenum,physaddr, *data, datalen);
 *
 * One implementation:
 *
 * struct rec {
 *    enum {READ,WRITE,BEGIN,ABORT,END} op;
 *    addr_t vcorenum,
 *           physaddr,
 *           datalen ;
 *    struct rec *next;
 * }
 *
 * struct cache_model {
 *    void *init(xml spec);  // make a cache, return ptr to state
 *    int write(void *priv, physaddr, datalen, int (*change_cb(int core,
 *                             physaddrstart, len));
 *    // similiar for read
 *
 * // Idea is that we pass writes to cache model, it calls us back to say which
 * lines on which cores have changed
 * }
 *
 *
 * struct boxstate {
 *    struct cache_model *model; //
 *    lock_t     global_lock; // any handle_* func acquires this first
 *    uint64_t   loglen;
 *    uint64_t   numtransactionsactive;
 *    struct rec *first;
 * }
 *
 * int handle_write(box,vcore,physaddr,data,datalen) {
 *
 */

#ifndef __TRANS_MEM_H__
#define __TRANS_MEM_H__

#include <palacios/vmm_lock.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_paging.h>

#define MAX_CORES 32

#define TM_KICKBACK_CALL 0x1337

#define HTABLE_SEARCH(h, k) ({ addr_t ret; v3_lock(h##_lock); ret = v3_htable_search((h), (k)); v3_unlock(h##_lock); ret; })
#define HTABLE_INSERT(h, k, v) ({ addr_t ret; v3_lock(h##_lock); ret = v3_htable_insert((h), (k), (addr_t)(v)); v3_unlock(h##_lock); ret; })

#define INSTR_INJECT_LEN 10
#define INSTR_BUF_SZ  15
#define ERR_STORE_MUST_ABORT -2
#define ERR_STORE_FAIL -1
#define ERR_DECODE_FAIL -1
#define ERR_TRANS_FAULT_FAIL 0
#define TRANS_FAULT_OK 1
#define TRANS_HCALL_FAIL -1 
#define TRANS_HCALL_OK 0

/* conflict checking codes */
#define ERR_CHECK_FAIL -1
#define CHECK_MUST_ABORT -2
#define CHECK_IS_CONFLICT 1
#define CHECK_NO_CONFLICT 0

/* RTM instruction handling */
#define XBEGIN_INSTR_LEN 0x6
#define XEND_INSTR_LEN   0x3
#define XABORT_INSTR_LEN 0x3
#define XTEST_INSTR_LEN  0x3

/* abort status definitions (these are bit indeces) */
#define ABORT_XABORT     0x0 // xabort instr
#define ABORT_RETRY      0x1 // may succeed on retry (must be clear if bit 0 set)
#define ABORT_CONFLICT   0x2 // another process accessed memory in the transaction
#define ABORT_OFLOW      0x3 // internal buffer overflowed
#define ABORT_BKPT       0x4 // debug breakpoint was hit
#define ABORT_NESTED     0x5 // abort occured during nested transaction (not currently used)


typedef enum tm_abrt_cause {
    TM_ABORT_XABORT,
    TM_ABORT_CONFLICT,
    TM_ABORT_INTERNAL,
    TM_ABORT_BKPT,
    TM_ABORT_UNSPECIFIED,
} tm_abrt_cause_t;

struct v3_tm_access_type {
    uint8_t r : 1;
    uint8_t w : 1;
} __attribute__((packed));

struct v3_ctxt_tuple {
    void * gva;
    void * core_id;
    void * core_lt;
} __attribute__((packed));

/* 441-tm: Are we currently in a transaction */
enum TM_MODE_E { 
    TM_OFF = 0, 
    TM_ON = 1,
};

/* 441-tm: Current state of the transaction state machine */
enum TM_STATE_E {
    TM_NULL = 0,
    TM_IFETCH = 1,
    TM_EXEC = 2
//    TM_ABORT = 3
};

typedef enum v3_tm_op {
    OP_TYPE_READ,
    OP_TYPE_WRITE
} v3_tm_op_t;

struct v3_trans_mem {
    /* current transaction */
    uint64_t t_num;

    /* 441-tm: linked list to store core's reads and writes */
    struct list_head trans_r_list;
    struct list_head trans_w_list;

    /* 441-tm: hash tables of addresses */
    struct hashtable * addr_ctxt;       // records the core transaction context at time of address use
    v3_lock_t addr_ctxt_lock;
    uint64_t addr_ctxt_entries;

    struct hashtable * access_type;     // hashes addr:corenum:t_num for each address use
    v3_lock_t access_type_lock;
    uint64_t access_type_entries;

    /* 441-tm: lets remember things about the next instruction */
    uint8_t dirty_instr_flag;
    addr_t  dirty_hva;
    addr_t  dirty_gva;
    uchar_t dirty_instr[15];
    int     cur_instr_len;

    enum TM_MODE_E TM_MODE;
    enum TM_STATE_E TM_STATE;
    uint64_t TM_ABORT;

    struct shadow_page_data * staging_page;

    /* 441-tm: Remember the failsafe addr */
    addr_t  fail_call;

    /* 441-tm: Save the rax we are about to ruin */
    v3_reg_t clobbered_rax;

    // branching instrs
    int to_branch;
    addr_t offset;

    // timing info
    uint64_t entry_time;
    uint64_t exit_time;
    uint64_t entry_exits;

    // cache box
    struct cache_box * box;

    struct guest_info * ginfo;

};


struct v3_tm_state {
    v3_lock_t lock;
    enum TM_MODE_E TM_MODE;
    uint64_t cores_active;

    uint64_t  * last_trans;
};

struct hash_chain {
    uint64_t * curr_lt;

    struct list_head lt_node;
};

// called from #PF handler, stages entries, catches reads / writes
addr_t v3_handle_trans_mem_fault(struct guest_info *core, 
                                 addr_t fault_addr, 
                                 pf_error_t error);

// restores instruction after core->rip
int v3_restore_dirty_instr(struct guest_info *core);

// restores instruction after core->rip
int v3_restore_abort_instr(struct guest_info *core);

// handles abort cleanup, called from INT/EXCP or XABORT
int v3_handle_trans_abort(struct guest_info *core,
                         tm_abrt_cause_t cause,
                         uint8_t xabort_reason);

// record a memory access in hashes
int tm_record_access (struct v3_trans_mem * tm, 
                      uint8_t write, 
                      addr_t gva);

// garbage collect hash recordings
int tm_hash_gc (struct v3_trans_mem * tm);

// check address for conflicts
int tm_check_conflict(struct   v3_vm_info * vm_info,
                      addr_t   gva,
                      v3_tm_op_t op_type,
                      uint64_t core_num, 
                      uint64_t curr_ctxt);

// increment transaction number
int v3_tm_inc_tnum(struct v3_trans_mem * tm);


/* exception-related functions */
int v3_tm_handle_exception(struct guest_info * info, addr_t exit_code);

void v3_tm_set_excp_intercepts(vmcb_ctrl_t * ctrl_area);

void v3_tm_check_intr_state(struct guest_info * info, 
        vmcb_ctrl_t * guest_ctrl, 
        vmcb_saved_state_t * guest_state);


/* paging-related functions */
int v3_tm_handle_pf_64 (struct guest_info * info,
                        pf_error_t error_code,
                        addr_t fault_addr,
                        addr_t * page_to_use);

void v3_tm_handle_usr_tlb_miss(struct guest_info * info,
                               pf_error_t error_code,
                               addr_t page_to_use,
                               addr_t * shadow_pa);

void v3_tm_handle_read_fault(struct guest_info * info,
                             pf_error_t error_code,
                             pte64_t * shadow_pte);

#include <palacios/vmm_decoder.h>

/* decoding-related functions */
int v3_tm_decode_rtm_instrs(struct guest_info * info, 
                            addr_t instr_ptr, 
                            struct x86_instr * instr);


#endif
