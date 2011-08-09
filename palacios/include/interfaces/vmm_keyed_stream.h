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
void                  v3_keyed_stream_preallocate_hint_key(v3_keyed_stream_t stream, char *key, uint64_t size);
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



#define STD_SAVE_RAW(stream,ks,x)			\
    do { \
	if (sizeof((x)) != v3_keyed_stream_write_key((stream), (ks), &(x), sizeof((x)))) { \
	    v3_keyed_stream_close_key((stream),(ks)); \
	    return -1;				      \
	} \
    } while (0)

#define STD_LOAD_RAW(stream,ks,x)			\
    do {								\
	if (sizeof((x)) != v3_keyed_stream_read_key((stream), (ks), &(x), sizeof((x)))) { \
	    v3_keyed_stream_close_key((stream),(ks)); \
	    return -1;				       \
	} \
    } while (0)
#endif

#define KSTREAM_MAGIC_COOKIE 0xabcd0123

#define STD_SAVE_TAGGED(stream,ks,tag,size,x)				\
do {		         						\
uint32_t temp;								\
temp=KSTREAM_MAGIC_COOKIE;						\
if (sizeof(temp) != v3_keyed_stream_write_key((stream),(ks),&temp,sizeof(temp))) { \
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
temp=strlen(tag);							\
if (sizeof(temp) != v3_keyed_stream_write_key((stream),(ks),&temp,sizeof(temp))) { \
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
if (temp != v3_keyed_stream_write_key((stream),(ks),tag,temp)) {	\
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
temp=(size);								\
if (sizeof(temp) != v3_keyed_stream_write_key((stream),(ks),&temp,sizeof(temp))) { \
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
if ((size) != v3_keyed_stream_write_key((stream),(ks),&(x),(size))) {	\
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }                                                                      \
} while (0)

#define STD_LOAD_TAGGED(stream,ks,tag,size,x)	 			\
do {		         						\
uint32_t temp;								\
if (sizeof(temp) != v3_keyed_stream_read_key((stream),(ks),&temp,sizeof(temp))) { \
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
if (temp!=KSTREAM_MAGIC_COOKIE) {					\
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
}                                                                       \
if (sizeof(temp) != v3_keyed_stream_read_key((stream),(ks),&temp,sizeof(temp))) { \
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
if (strlen((tag))!=temp) {						\
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
}									\
{ char buf[temp+1];							\
    if (temp != v3_keyed_stream_read_key((stream),(ks),buf,temp)) {	\
        v3_keyed_stream_close_key((stream),(ks));			\
        return -1;							\
    }									\
    buf[temp]=0;							\
    if (strncasecmp(buf,tag,temp)) {					\
        v3_keyed_stream_close_key((stream),(ks));			\
        return -1;							\
    }									\
}                                                                       \
if (sizeof(temp) != v3_keyed_stream_read_key((stream),(ks),&temp,sizeof(temp))) { \
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }									\
if (temp!=(size)) {							\
    v3_keyed_stream_close_key((stream),(ks));                           \
    return -1;                                                          \
}                                                                       \
if ((size) != v3_keyed_stream_read_key((stream),(ks),&(x),(size))) {	\
    v3_keyed_stream_close_key((stream),(ks));				\
    return -1;								\
 }                                                                      \
} while (0)

#ifdef V3_CONFIG_KEYED_STREAMS_WITH_TAGS
#define STD_SAVE(stream,ks,x) STD_SAVE_TAGGED(stream,ks,#x,sizeof(x),x)
#define STD_LOAD(stream,ks,x) STD_LOAD_TAGGED(stream,ks,#x,sizeof(x),x)
#else
#define STD_SAVE(stream,ks,x) STD_SAVE_RAW(stream,ks,x)
#define STD_LOAD(stream,ks,x) STD_LOAD_RAW(stream,ks,x)
#endif



struct v3_keyed_stream_hooks {
    // url is meaningful only to the host implementation
    v3_keyed_stream_t (*open)(char *url,
			      v3_keyed_stream_open_t open_type);
			      
    void (*close)(v3_keyed_stream_t stream);

    void (*preallocate_hint_key)(v3_keyed_stream_t stream,
				 char *key,
				 uint64_t size);

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
