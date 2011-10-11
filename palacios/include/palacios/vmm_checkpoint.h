/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Madhav Suresh <madhav@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Authors: Madhav Suresh <madhav@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_CHECKPOINT_H__
#define __VMM_CHECKPOINT_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>


struct v3_chkpt;


struct v3_chkpt_ctx {
    struct v3_chkpt * chkpt;
    struct v3_chkpt_ctx * parent;
    void * store_ctx;
};

/* Temporary */
#define  V3_CHKPT_STD_SAVE(ctx,x) v3_chkpt_save(ctx,#x,sizeof(x),&(x))
#define  V3_CHKPT_STD_LOAD(ctx,x) v3_chkpt_load(ctx,#x,sizeof(x),&(x))



int v3_chkpt_save(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf);
int v3_chkpt_load(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf);

static inline int v3_chkpt_save_64(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_save(ctx, tag, 8, buf);
}
static inline int v3_chkpt_save_32(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_save(ctx, tag, 4, buf);
}
static inline int v3_chkpt_save_16(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_save(ctx, tag, 2, buf);
}
static inline int v3_chkpt_save_8(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_save(ctx, tag, 1, buf);
}

static inline int v3_chkpt_load_64(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_load(ctx, tag, 8, buf);
}
static inline int v3_chkpt_load_32(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_load(ctx, tag, 4, buf);
}
static inline int v3_chkpt_load_16(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_load(ctx, tag, 2, buf);
}
static inline int v3_chkpt_load_8(struct v3_chkpt_ctx * ctx, char * tag, void * buf) {
    return v3_chkpt_load(ctx, tag, 1, buf);
}



int v3_chkpt_close_ctx(struct v3_chkpt_ctx * ctx);
struct v3_chkpt_ctx * v3_chkpt_open_ctx(struct v3_chkpt * chkpt, struct v3_chkpt_ctx * parent, char * name);

int v3_chkpt_save_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_chkpt_load_vm(struct v3_vm_info * vm, char * store, char * url);

int V3_init_checkpoint();
int V3_deinit_checkpoint();

#endif

#endif
