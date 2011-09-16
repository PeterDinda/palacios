/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_CHKPT_STORES_H__
#define __VMM_CHKPT_STORES_H__


/*
 * This is a place holder to ensure that the _v3_extensions section gets created by gcc
 */
static struct {} null_store __attribute__((__used__))			\
    __attribute__((unused, __section__ ("_v3_chkpt_stores"),		\
                   aligned(sizeof(addr_t))));


#define register_chkpt_store(store)					\
    static struct v3_chkpt_interface * _v3_store_#store			\
    __attribute__((used))						\
	__attribute__((unused, __section__("_v3_chkpt_stores"),		\
		       aligned(sizeof(addr_t))))			\
	= store;




#ifdef V3_CONFIG_KEYED_STREAMS
#include <palacios/vmm_keyed_stream.h>

static void * keyed_stream_open_chkpt(char * url) {
    return v3_keyed_stream_open(url, V3_KS_WR_ONLY_CREATE);
}



static int keyed_stream_close_chkpt(void * store_data) {
    v3_keyed_stream_t stream = store_data;

    v3_keyed_stream_close(stream);

    return 0;
}

static void * keyed_stream_open_ctx(void * store_data, 
				    void * parent_ctx, 
				    char * name) {
    v3_keyed_stream_t stream = store_data;

    return v3_keyed_stream_open_key(stream, name);
}

static int keyed_stream_close_ctx(void * store_data, void * ctx) {
    v3_keyed_stream_t stream = store_data;

    v3_keyed_stream_close_key(stream, ctx);

    return 0;
}

static uint64_t keyed_stream_save(void * store_data, void * ctx, 
				  char * tag, uint64_t len, void * buf) {
    return v3_keyed_stream_write_key(store_data, ctx, buf, len);
}

static uint64_t keyed_stream_load(void * store_data, void * ctx, 
				  char * tag, uint64_t len, void * buf) {
    return v3_keyed_stream_read_key(store_data, ctx, buf, len);
}


static struct chkpt_interface keyed_stream_store = {
    .name = "KEYED_STREAM",
    .open_chkpt = keyed_stream_open_chkpt,
    .close_chkpt = keyed_stream_close_chkpt,
    .open_ctx = keyed_stream_open_ctx, 
    .close_ctx = keyed_stream_close_ctx,
    .save = keyed_stream_save,
    .load = keyed_stream_load
};

register_chkpt_store(&keyed_stream_store);



#endif






#endif
