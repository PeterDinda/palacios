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

//#include <palacios/vmm_types.h>

/*
 * This is a place holder to ensure that the _v3_extensions section gets created by gcc
 */
static struct {} null_store __attribute__((__used__))			\
    __attribute__((unused, __section__ ("_v3_chkpt_stores"),		\
                   aligned(sizeof(addr_t))));


#define register_chkpt_store(store)					\
    static struct chkpt_interface * _v3_store_##store			\
    __attribute__((used))						\
	__attribute__((unused, __section__("_v3_chkpt_stores"),		\
		       aligned(sizeof(addr_t))))			\
	= &store;





#include <palacios/vmm_util.h>


static void * debug_open_chkpt(char * url, chkpt_mode_t mode) {
   
    if (mode == LOAD) {
	V3_Print("Cannot load from debug store\n");
	return NULL;
    }

    V3_Print("Opening Checkpoint: %s\n", url);

    return (void *)1;
}



static int debug_close_chkpt(void * store_data) {
    V3_Print("Closing Checkpoint\n");
    return 0;
}

static void * debug_open_ctx(void * store_data, 
			     void * parent_ctx, 
			     char * name) {
    V3_Print("[%s]\n", name);
    return (void *)1;
}

static int debug_close_ctx(void * store_data, void * ctx) {
    V3_Print("[CLOSE]\n"); 
    return 0;
}

static int debug_save(void * store_data, void * ctx, 
		      char * tag, uint64_t len, void * buf) {
    V3_Print("%s:\n", tag);

    if (len > 100) {
	len = 100;
    }

    v3_dump_mem(buf, len);
    
    return 0;
}

static int debug_load(void * store_data, void * ctx, 
				  char * tag, uint64_t len, void * buf) {
    V3_Print("Loading not supported !!!\n");
    return 0;
}


static struct chkpt_interface debug_store = {
    .name = "DEBUG",
    .open_chkpt = debug_open_chkpt,
    .close_chkpt = debug_close_chkpt,
    .open_ctx = debug_open_ctx, 
    .close_ctx = debug_close_ctx,
    .save = debug_save,
    .load = debug_load
};

register_chkpt_store(debug_store);




#ifdef V3_CONFIG_KEYED_STREAMS
#include <interfaces/vmm_keyed_stream.h>

static void * keyed_stream_open_chkpt(char * url, chkpt_mode_t mode) {
    if (mode == SAVE) {
	return v3_keyed_stream_open(url, V3_KS_WR_ONLY_CREATE);
    } else if (mode == LOAD) {
	return v3_keyed_stream_open(url, V3_KS_RD_ONLY);
    }

    // Shouldn't get here
    return NULL;
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

static int keyed_stream_save(void * store_data, void * ctx, 
				  char * tag, uint64_t len, void * buf) {
    return v3_keyed_stream_write_key(store_data, ctx, buf, len);
}

static int keyed_stream_load(void * store_data, void * ctx, 
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

register_chkpt_store(keyed_stream_store);



#endif



#ifdef V3_CONFIG_FILE
#include <interfaces/vmm_file.h>


struct file_ctx {
    v3_file_t file;
    uint64_t offset;
    char * filename;
};
    

static void * dir_open_chkpt(char * url, chkpt_mode_t mode) {
    if (mode == SAVE) {
	if (v3_mkdir(url, 0755, 1) != 0) {
	    return NULL;
	}
    }

    return url;
}



static int dir_close_chkpt(void * store_data) {
    return 0;
}

static void * dir_open_ctx(void * store_data, 
			   void * parent_ctx, 
			   char * name) {

    char * url = store_data;
    struct file_ctx * ctx = NULL;


    ctx = V3_Malloc(sizeof(struct file_ctx));
    memset(ctx, 0, sizeof(struct file_ctx));

    ctx->filename = V3_Malloc(strlen(url) + strlen(name) + 5);
    memset(ctx->filename, 0, strlen(url) + strlen(name) + 5);

    snprintf(ctx->filename, strlen(url) + strlen(name) + 5, "%s/%s", url, name);


    ctx->file = v3_file_open(NULL, ctx->filename, FILE_OPEN_MODE_READ | FILE_OPEN_MODE_WRITE | FILE_OPEN_MODE_CREATE);
   
    return ctx;
}

static int dir_close_ctx(void * store_data, void * ctx) {
    struct file_ctx * file_ctx = ctx;

    v3_file_close(file_ctx->file);

    V3_Free(file_ctx->filename);
    V3_Free(file_ctx);

    return 0;
}

static int dir_save(void * store_data, void * ctx, 
		    char * tag, uint64_t len, void * buf) {
    struct file_ctx * file_ctx = ctx;
    uint64_t ret = 0;
   
    ret = v3_file_write(file_ctx->file, buf, len, file_ctx->offset);

    file_ctx->offset += ret;
    
    return 0;
}

static int dir_load(void * store_data, void * ctx, 
		    char * tag, uint64_t len, void * buf) {
    struct file_ctx * file_ctx = ctx;
    uint64_t ret = 0;
    
    ret = v3_file_read(file_ctx->file, buf, len, file_ctx->offset);

    file_ctx->offset += ret;

    return 0;
}


static struct chkpt_interface dir_store = {
    .name = "DIR",
    .open_chkpt = dir_open_chkpt,
    .close_chkpt = dir_close_chkpt,
    .open_ctx = dir_open_ctx, 
    .close_ctx = dir_close_ctx,
    .save = dir_save,
    .load = dir_load
};

register_chkpt_store(dir_store);



#endif







#endif
