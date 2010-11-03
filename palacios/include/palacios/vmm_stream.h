/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda (pdinda@northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_STREAM_H__
#define __VMM_STREAM_H__

#include <palacios/vmm.h>


#ifdef __V3VEE__

#define V3_StreamOpen(path, mode)						\
    ({									\
	extern struct v3_stream_hooks *stream_hooks;				\
	((stream_hooks) && (stream_hooks)->stream_open) ?				\
	    (stream_hooks)->stream_open((path), (mode)) : NULL;		\
    })

#define V3_StreamRead(stream, b, l)					\
    ({									\
	extern struct v3_stream_hooks *stream_hooks;				\
	((stream_hooks) && (stream_hooks)->stream_read) ?			\
	    (stream_hooks)->stream_read((stream), (b), (l)) : -1;		\
    })

#define V3_StreamWrite(stream, b, l)					\
    ({									\
	extern struct v3_stream_hooks *stream_hooks;				\
	((stream_hooks) && (stream_hooks)->stream_write) ?			\
	    (stream_hooks)->stream_write((stream), (b), (l)) : -1;		\
    })


#define V3_StreamClose(stream)						\
    ({									\
	extern struct v3_stream_hooks *stream_hooks;				\
	((stream_hooks) && (stream_hooks)->stream_close) ?				\
	    (stream_hooks)->stream_close((stream), (mode)) : NULL;	\
    })


#endif

#define STREAM_OPEN_MODE_READ	(1 << 0)
#define STREAM_OPEN_MODE_WRITE	(1 << 1)

struct v3_stream_hooks {
    void *(*stream_open)(const char *path, int mode);
    int (*stream_read)(void *stream, char *buf, int len);
    int (*stream_write)(void *stream, char *buf, int len);
    int (*stream_close)(void *stream);

};


extern void V3_Init_Stream(struct v3_stream_hooks * hooks);

#endif
