/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <interfaces/vmm_keyed_stream.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

struct v3_keyed_stream_hooks * keyed_stream_hooks = 0;

v3_keyed_stream_t     v3_keyed_stream_open(char *url, v3_keyed_stream_open_t open_type)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->open != NULL);

    return keyed_stream_hooks->open(url,open_type);
}


	
void                  v3_keyed_stream_close(v3_keyed_stream_t stream)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->close != NULL);

    return keyed_stream_hooks->close(stream);

}


void v3_keyed_stream_preallocate_hint_key(v3_keyed_stream_t stream, char *key, uint64_t size)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->preallocate_hint_key != NULL);

    return keyed_stream_hooks->preallocate_hint_key(stream,key,size);
}

v3_keyed_stream_key_t v3_keyed_stream_open_key(v3_keyed_stream_t stream, char *key)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->open_key != NULL);

    return keyed_stream_hooks->open_key(stream,key);
}


void                  v3_keyed_stream_close_key(v3_keyed_stream_t stream,  char *key)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->close_key != NULL);

    return keyed_stream_hooks->close_key(stream,key);
}


sint64_t              v3_keyed_stream_write_key(v3_keyed_stream_t stream,  
						v3_keyed_stream_key_t key,
						void *buf, 
						sint64_t len)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->write_key != NULL);

    return keyed_stream_hooks->write_key(stream,key,buf,len);
}

sint64_t              v3_keyed_stream_read_key(v3_keyed_stream_t stream,
					       v3_keyed_stream_key_t key,
					       void *buf, 
					       sint64_t len)
{
    V3_ASSERT(keyed_stream_hooks != NULL);
    V3_ASSERT(keyed_stream_hooks->read_key != NULL);

    return keyed_stream_hooks->read_key(stream,key,buf,len);
}



void V3_Init_Keyed_Streams(struct v3_keyed_stream_hooks * hooks) {
    keyed_stream_hooks = hooks;
    PrintDebug("V3 keyed stream support inited\n");

    return;
}
