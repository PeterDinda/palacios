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
 *         Kyle Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 *
 */

#ifndef __TM_CACHE_H__
#define __TM_CACHE_H__

#ifdef __V3VEE__

enum TM_ERR_E {
    TM_OK  = 0,
    TM_WAR = 1,
    TM_WAW = 2,
    TM_RAW = 3
};

enum TM_OP {
    TM_READ  = 0,
    TM_WRITE = 1,
    TM_BEGIN = 2,
    TM_ABORT = 3,
    TM_END   = 4
};

// one record in the redo log linked list
struct rec {
    enum TM_OP op;
    addr_t vcorenum;
    addr_t physaddr;
    addr_t datalen;
    struct list_head rec_node;
};

struct flag_bits {
    uint8_t m : 1; // modified
    uint8_t e : 1; // exclusive
    uint8_t s : 1; // shared
    uint8_t i : 1; // exclusive
    uint8_t ws : 1;
    uint8_t rs : 1;
} __attribute__((packed));

struct cache_line {
    uint64_t tag;
    struct flag_bits * flag;;
};

struct cache_spec {
    uint64_t line_size;         // line size in bytes
    uint64_t size;              // cache size in kb
    uint64_t num_lines;
    uint64_t associativity;
    enum cache_policy policy;
};

// cache hardware we are emulating
struct cache_box {
    int (*init) (struct cache_spec * spec, struct cache_box * self);
    struct cache_spec * spec;
    struct cache_line ** cache_table;

    enum TM_ERR_E (*read)  (struct guest_info *core, addr_t hva, addr_t len, struct cache_box * self);
    enum TM_ERR_E (*write) (struct guest_info *core, addr_t hva, addr_t len, struct cache_box * self);
    uint64_t (*invalidate) (struct guest_info *core, addr_t hva, addr_t len, struct cache_box * self);
};

// redo logger
// TODO: dont need this anymore?
/*
struct logger {
    // emulated cache
    struct cache_box *model;
    lock_t   global_lock;
    uint64_t loglen;
    uint64_t num_trans_active;

    enum TM_ERR_E (*read) (struct guest_info *core, addr_t hva, addr_t len);
    enum TM_ERR_E (*write) (struct guest_info *core, addr_t hva, addr_t len);

    log_rec  *head;
};
*/
/*
 * error = handle_start_tx(logger,vcorenum);
 * error = handle_abort(logger,vcorenum);
 * error = handle_commit(logger,vcorenum);
 *
 * should_abort = handle_write(logger, vcorenum, physaddr, data, datalen);
 * should_abort = handle_read(logger, vcorenum, physaddr, *data, datalen);
 *
 */

/* FN SKEL
 *
 * handle_start_tx(logger,vcorenum) {
 *  logger.record(BEGIN,vcorenum)
 * }
 *
 * handle_abort(logger,vcorenum) {
 *  logger.record(ABORT,vcorenum)
 * }
 *
 * handle_commit(logger,vcorenum) {
 *  logger.record(END,vcorenum)
 *  logger.commit(vcorenum)
 * }
 *
 * record(head,type,vcorenum,physaddr,datalen,data) {
 *  new rec = {type, vcorenum, physaddr, datalen, data,head}
 *  head = new rec
 *  err = conflict_check(head,vcorenum)
 * }
 *
 * read(logger,core,addr,*data,datalen) {
 *  logger.record(READ,vcorenum)
 *
 *  // hmm, we want the most recent entry, should we keep track of tail as
 *  // well?? or just keep a seperate log of current values?
 *  cur = head
 *  while cur {
 *    if cur->addr == addr
 *      data = cur->data
 *      return
 *    cur = cur->next
 *  }
 *
 *  read_mem(data)
 *  return
 * }
 *
 * write(logger,core,addr,data,datalen) {
 *  logger.record(WRITE,vcorenum,data)
 * }
 *
 */

#endif // ! __V3VEE__

#endif
