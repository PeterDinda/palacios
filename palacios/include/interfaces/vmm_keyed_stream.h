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


#ifndef __VMM_KEYED_STREAM_H__
#define __VMM_KEYED_STREAM_H__

#include <palacios/vmm.h>


/*
  A keyed stream essentially supports this:

  URL => {collection of key->stream pairs}

  If you open a key for reading, you get the read pointer set to its beginning.   
  You can then make repeated reads to advance the read pointer.  You cannot seek.

  Writing works similarly.

  You cannot both read and write. 

*/

/* A keyed stream and its components are opaque to palacios */
typedef void * v3_keyed_stream_t;
typedef void * v3_keyed_stream_key_t;

typedef enum {V3_KS_RD_ONLY,V3_KS_WR_ONLY,V3_KS_WR_ONLY_CREATE} v3_keyed_stream_open_t;

#ifdef __V3VEE__


v3_keyed_stream_t     v3_keyed_stream_open(char *url, v3_keyed_stream_open_t open_type);
void                  v3_keyed_stream_close(v3_keyed_stream_t stream);
v3_keyed_stream_key_t v3_keyed_stream_open_key(v3_keyed_stream_t stream, char *key);
void                  v3_keyed_stream_close_key(v3_keyed_stream_t stream,  char *key);
sint64_t              v3_keyed_stream_write_key(v3_keyed_stream_t stream,  
						v3_keyed_stream_key_t key,
						void *buf, 
						sint64_t len);
sint64_t              v3_keyed_stream_read_key(v3_keyed_stream_t stream,
					       v3_keyed_stream_key_t key,
					       void *buf, 
					       sint64_t len);

#define STD_SAVE(stream,ks,x)			\
    do { \
	if (sizeof((x)) != v3_keyed_stream_write_key((stream), (ks), &(x), sizeof((x)))) { \
	    v3_keyed_stream_close_key((stream),(ks)); \
	    return -1;				      \
	} \
    } while (0)

#define STD_LOAD(stream,ks,x)			\
    do {								\
	if (sizeof((x)) != v3_keyed_stream_read_key((stream), (ks), &(x), sizeof((x)))) { \
	    v3_keyed_stream_close_key((stream),(ks)); \
	    return -1;				       \
	} \
    } while (0)
#endif


struct v3_keyed_stream_hooks {
    // url is meaningful only to the host implementation
    v3_keyed_stream_t (*open)(char *url,
			      v3_keyed_stream_open_t open_type);
			      
    void (*close)(v3_keyed_stream_t stream);

    v3_keyed_stream_key_t (*open_key)(v3_keyed_stream_t stream,
				      char *key);

    void (*close_key)(v3_keyed_stream_t stream, 
		      v3_keyed_stream_key_t key);
    

    sint64_t (*write_key)(v3_keyed_stream_t stream,
			  v3_keyed_stream_key_t key,
			  void *buf, 
			  sint64_t len);

    sint64_t (*read_key)(v3_keyed_stream_t stream,
			 v3_keyed_stream_key_t key,
			 void *buf, 
			 sint64_t len);
    
};


extern void V3_Init_Keyed_Streams(struct v3_keyed_stream_hooks *hooks);

#endif
