#include "palacios.h"
#include "palacios-keyed-stream.h"
#include "palacios-hashtable.h"

#define sint64_t int64_t
#include <interfaces/vmm_keyed_stream.h>

/*
  Streams are stored in a hash table
  The values for this hash table are hash tables associted with 
  each stream.   A keyed stream for a "mem:" stream is 
  an instance of the structure given here 
*/

#define DEF_NUM_STREAMS 16
#define DEF_NUM_KEYS    128
#define DEF_SIZE        128

struct mem_stream {
    char     *data;
    uint32_t  size;
    uint32_t  data_max;
    uint32_t  ptr;
};

static struct mem_stream *create_mem_stream(void)
{
    struct mem_stream *m = kmalloc(sizeof(struct mem_stream),GFP_KERNEL);

    if (!m) {
	return 0;
    }

    m->data = kmalloc(DEF_SIZE,GFP_KERNEL);
    
    if (!m->data) { 
	kfree(m);
	return 0;
    }

    m->size=DEF_SIZE;
    m->ptr=0;
    m->data_max=0;
    
    return m;
}

static void destroy_mem_stream(struct mem_stream *m)
{
    if (m) {
	if (m->data) {
	    kfree(m->data);
	}
	m->data=0;
	kfree(m);
    }
}
    
static int expand_mem_stream(struct mem_stream *m, uint32_t new_size)
{
    void *data = kmalloc(new_size,GFP_KERNEL);
    uint32_t nc;

    if (!data) { 
	return -1;
    }
    
    nc = (new_size<m->data_max) ? new_size : m->data_max;

    memcpy(data,m->data,nc);

    kfree(m->data);

    m->data=data;
    m->size=new_size;
    if (m->size<m->data_max) { 
	m->data_max=m->size;
    }
   
    return 0;
}

static uint32_t write_mem_stream(struct mem_stream *m,
				 void *data,
				 uint32_t len)
{
    if ((m->ptr + len) > m->size) { 
	if (expand_mem_stream(m,m->ptr + len)) { 
	    return 0;
	}
    }
    memcpy(m->data+m->ptr,data,len);
    m->ptr+=len;
    m->data_max=m->ptr;
    
    return len;

}

static uint32_t read_mem_stream(struct mem_stream *m,
				void *data,
				uint32_t len)
{
    if ((m->ptr + len) > m->data_max) { 
	return 0;
    }
    memcpy(data,m->data+m->ptr,len);
    m->ptr+=len;
    
    return len;

}


static void reset_mem_stream(struct mem_stream *m)
{
    m->ptr=0;
}


static inline uint_t hash_func(addr_t key)
{
    return palacios_hash_buffer((uchar_t*)key,strlen((uchar_t*)key));
}

static inline int hash_comp(addr_t k1, addr_t k2)
{
    return strcasecmp((char*)k1,(char*)k2)==0;
}


// This stores all the streams
static struct hashtable *streams=0;


static v3_keyed_stream_t open_stream(char *url,
				     v3_keyed_stream_open_t ot)
{
    if (strncasecmp(url,"mem:",4)) { 
	printk("Only in-memory streams are currently supported\n");
	return 0;
    }

    switch (ot) { 
	case V3_KS_RD_ONLY:
	case V3_KS_WR_ONLY:
	    return (v3_keyed_stream_t) palacios_htable_search(streams,(addr_t)(url+4));
	    break;
	case V3_KS_WR_ONLY_CREATE: {
	    struct hashtable *s = (struct hashtable *) palacios_htable_search(streams,(addr_t)(url+4));

	    if (!s) { 
		 s = palacios_create_htable(DEF_NUM_KEYS,hash_func,hash_comp);
		 if (!s) { 
		     printk("Cannot allocate in-memory keyed stream %s\n",url);
		     return 0;
		 }
		 if (!palacios_htable_insert(streams,(addr_t)(url+4),(addr_t)s)) { 
		     printk("Cannot insert in-memory keyed stream %s\n",url);
		     return 0;
		 }
	    }

	    return s;
	    
	}

	    break;
    }
    
    return 0;
    
}


static void close_stream(v3_keyed_stream_t stream)
{
    // nothing to do
    return;
}

static v3_keyed_stream_key_t open_key(v3_keyed_stream_t stream,
				      char *key)
{
    struct hashtable *s = (struct hashtable *) stream;

    struct mem_stream *m;

    m = (struct mem_stream *) palacios_htable_search(s,(addr_t)key);

    if (!m) { 
	m = create_mem_stream();
	
	if (!m) { 
	    printk("Cannot allocate keyed stream for key %s\n",key);
	    return 0;
	}

	if (!palacios_htable_insert(s,(addr_t)key,(addr_t)m)) {
	    printk("Cannot insert keyed stream for key %s\n",key);
	    destroy_mem_stream(m);
	    return 0;
	}
    }

    reset_mem_stream(m);
    return m;

}

static void close_key(v3_keyed_stream_t stream, 
		      v3_keyed_stream_key_t key)
{
    // nothing to do
    return;
}

static sint64_t write_key(v3_keyed_stream_t stream, 
			  v3_keyed_stream_key_t key,
			  void *buf,
			  sint64_t len)
{
    struct mem_stream *m = (struct mem_stream *) key;
    uint32_t mylen;
    uint32_t writelen;

    if (len<0) { 
	return len;
    }
    
    mylen = (uint32_t) len;

    writelen=write_mem_stream(m,buf,mylen);

    if (writelen!=mylen) { 
	printk("Failed to write all data for key\n");
	return -1;
    } else {
	return (sint64_t)writelen;
    }
}

static sint64_t read_key(v3_keyed_stream_t stream, 
			 v3_keyed_stream_key_t key,
			 void *buf,
			 sint64_t len)
{
    struct mem_stream *m = (struct mem_stream *) key;
    uint32_t mylen;
    uint32_t readlen;
    
    if (len<0) { 
	return len;
    }
    
    mylen = (uint32_t) len;

    readlen=read_mem_stream(m,buf,mylen);

    if (readlen!=mylen) { 
	printk("Failed to read all data for key\n");
	return -1;
    } else {
	return (sint64_t)readlen;
    }
}
    
static struct v3_keyed_stream_hooks hooks = {
    .open = open_stream,
    .close = close_stream,
    .open_key = open_key,
    .close_key = close_key,
    .read_key = read_key,
    .write_key = write_key
};


int palacios_init_keyed_streams()
{
    streams = palacios_create_htable(DEF_NUM_STREAMS,hash_func,hash_comp);

    if (!streams) { 
	printk("Failed to allocated stream pool\n");
	return -1;
    }

    V3_Init_Keyed_Streams(&hooks);
    
    return 0;

}

int palacios_deinit_keyed_streams()
{
    printk("DEINIT OF PALACIOS KEYED STREAMS NOT IMPLEMENTED - WE HAVE JUST LEAKED MEMORY!\n");
    return -1;
}
