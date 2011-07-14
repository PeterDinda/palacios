/*
 * Palacios keyed stream interface
 * (c) Peter Dinda, 2011
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/namei.h>

#include "palacios.h"
#include "util-hashtable.h"
#include "linux-exts.h"

#define sint64_t int64_t
#include <interfaces/vmm_keyed_stream.h>

/*
  This is an implementation of the Palacios keyed stream interface
  that supports two flavors of streams:

  "mem:"   Streams are stored in a hash table
  The values for this hash table are hash tables associated with 
  each stream.   

  "file:"  Streams are stored in files.  Each high-level
  open corresponds to a directory, while  key corresponds to
  a distinct file in that directory. 

*/

#define STREAM_GENERIC 0
#define STREAM_MEM     1
#define STREAM_FILE    2


/*
  All keyed streams and streams indicate their implementation type within the first field
 */
struct generic_keyed_stream {
    int stype;
};

struct generic_stream {
    int stype;
};
  



/****************************************************************************************
   Memory-based implementation  ("mem:")
****************************************************************************************/

#define DEF_NUM_STREAMS 16
#define DEF_NUM_KEYS    128
#define DEF_SIZE        128

/*
  A memory keyed stream is a pointer to the underlying hash table
  while a memory stream contains an extensible buffer for the stream
 */
struct mem_keyed_stream {
    int stype;
    v3_keyed_stream_open_t ot;
    struct hashtable *ht;
};

struct mem_stream {
    int       stype;
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

    m->stype = STREAM_MEM;
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


// This stores all the memory keyed streams streams
static struct hashtable *streams=0;


static v3_keyed_stream_t open_stream_mem(char *url,
					 v3_keyed_stream_open_t ot)
{

    if (strncasecmp(url,"mem:",4)) { 
	printk("palacios: illegitimate attempt to open memory stream \"%s\"\n",url);
	return 0;
    }

    switch (ot) { 
	case V3_KS_RD_ONLY:
	case V3_KS_WR_ONLY: {
	    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) palacios_htable_search(streams,(addr_t)(url+4));
	    if (mks) { 
		mks->ot=ot;
	    }
	    return (v3_keyed_stream_t) mks;
	}
	    break;

	case V3_KS_WR_ONLY_CREATE: {
	    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) palacios_htable_search(streams,(addr_t)(url+4));
	    if (!mks) { 
		mks = (struct mem_keyed_stream *) kmalloc(sizeof(struct mem_keyed_stream),GFP_KERNEL);
		if (!mks) { 
		    printk("palacios: cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}
	    
		mks->ht = (void*) palacios_create_htable(DEF_NUM_KEYS,hash_func,hash_comp);
		if (!mks->ht) { 
		    kfree(mks);
		    printk("palacios: cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}
		
		if (!palacios_htable_insert(streams,(addr_t)(url+4),(addr_t)mks)) { 
		    palacios_free_htable(mks->ht,1,1);
		    kfree(mks);
		    printk("palacios: cannot insert in-memory keyed stream %s\n",url);
		    return 0;
		}
		mks->stype=STREAM_MEM;
	    }

	    mks->ot=V3_KS_WR_ONLY;
	    
	    return mks;
	    
	}
	    break;

	default:
	    printk("palacios: unsupported open type in open_stream_mem\n");
	    break;
    }
    
    return 0;
	
}



static void close_stream_mem(v3_keyed_stream_t stream)
{
    // nothing to do
    return;
}


static v3_keyed_stream_key_t open_key_mem(v3_keyed_stream_t stream,
					  char *key)
{
    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) stream;
    struct hashtable *s = mks->ht;

    struct mem_stream *m;

    m = (struct mem_stream *) palacios_htable_search(s,(addr_t)key);

    if (!m) { 
	m = create_mem_stream();
	
	if (!m) { 
	    printk("palacios: cannot allocate mem keyed stream for key %s\n",key);
	    return 0;
	}

	if (!palacios_htable_insert(s,(addr_t)key,(addr_t)m)) {
	    printk("palacios: cannot insert mem keyed stream for key %s\n",key);
	    destroy_mem_stream(m);
	    return 0;
	}
    }

    reset_mem_stream(m);
    return m;

}

static void close_key_mem(v3_keyed_stream_t stream, 
			  v3_keyed_stream_key_t key)
{
    // nothing to do
    return;
}

static sint64_t write_key_mem(v3_keyed_stream_t stream, 
			      v3_keyed_stream_key_t key,
			      void *buf,
			      sint64_t len)
{
    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) stream;
    struct mem_stream *m = (struct mem_stream *) key;
    uint32_t mylen;
    uint32_t writelen;

    if (mks->ot!=V3_KS_WR_ONLY) {
	return -1;
    }

    if (len<0) { 
	return -1;
    }
    
    mylen = (uint32_t) len;

    writelen=write_mem_stream(m,buf,mylen);

    if (writelen!=mylen) { 
	printk("palacios: failed to write all data for key\n");
	return -1;
    } else {
	return (sint64_t)writelen;
    }
}

static sint64_t read_key_mem(v3_keyed_stream_t stream, 
			     v3_keyed_stream_key_t key,
			     void *buf,
			     sint64_t len)
{
    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) stream;
    struct mem_stream *m = (struct mem_stream *) key;
    uint32_t mylen;
    uint32_t readlen;
    
    if (mks->ot!=V3_KS_RD_ONLY) {
	return -1;
    }

    if (len<0) { 
	return -1;
    }
    
    mylen = (uint32_t) len;
    
    readlen=read_mem_stream(m,buf,mylen);
    
    if (readlen!=mylen) { 
	printk("palacios: failed to read all data for key\n");
	return -1;
    } else {
	return (sint64_t)readlen;
    }
}


/***************************************************************************************************
  File-based implementation  ("file:")
*************************************************************************************************/

/*
  A file keyed stream contains the fd of the directory
  and a path
*/

struct file_keyed_stream {
    int   stype;
    v3_keyed_stream_open_t ot;
    char  *path;
};

struct file_stream {
    int   stype;
    struct file *f;   // the opened file
};


static v3_keyed_stream_t open_stream_file(char *url,
					  v3_keyed_stream_open_t ot)
{
    struct file_keyed_stream *fks;
    struct nameidata nd;

    if (strncasecmp(url,"file:",5)) { 
	printk("palacios: illegitimate attempt to open file stream \"%s\"\n",url);
	return 0;
    }

    fks = kmalloc(sizeof(struct file_keyed_stream),GFP_KERNEL);
    
    if (!fks) { 
	printk("palacios: cannot allocate space for file stream\n");
	return 0;
    }

    fks->path = (char*)kmalloc(strlen(url+5)+1,GFP_KERNEL);
    
    if (!(fks->path)) { 
	printk("palacios: cannot allocate space for file stream\n");
	kfree(fks);
	return 0;
    }
    
    strcpy(fks->path,url+5);
    
    fks->stype=STREAM_FILE;

    fks->ot= ot==V3_KS_WR_ONLY_CREATE ? V3_KS_WR_ONLY : ot;

    // Does the directory exist, and can we read/write it?
   
    if (path_lookup(fks->path,LOOKUP_DIRECTORY|LOOKUP_FOLLOW,&nd)) { 

	// directory does does not exist.  

	if (ot==V3_KS_RD_ONLY || ot==V3_KS_WR_ONLY) { 

	    // we are not being asked to create it
	    printk("palacios: attempt to open %s, which does not exist\n",fks->path);
	    goto fail_out;

	} else {

	    // We are being asked to create it

	    struct dentry *de;
	    int err;

	    // Find its parent
	    if (path_lookup(fks->path,LOOKUP_PARENT|LOOKUP_FOLLOW,&nd)) { 
		printk("palacios: attempt to create %s failed because its parent cannot be looked up\n",fks->path);
		goto fail_out;
	    }

	    // Can we write to the parent?

	    if (inode_permission(nd.path.dentry->d_inode, MAY_WRITE | MAY_EXEC)) { 
		printk("palacios: attempt to open %s, which has the wrong permissions for directory creation\n",fks->path);
		goto fail_out;
	    }

	    // OK, we can, so let's create it

	    de = lookup_create(&nd,1);

	    if (IS_ERR(de)) { 
		printk("palacios: cannot allocate dentry\n");
		goto fail_out;
	    }

	    err = vfs_mkdir(nd.path.dentry->d_inode, de, 0700);

	    // lookup_create locks this for us!

	    mutex_unlock(&(nd.path.dentry->d_inode->i_mutex));

	    if (err) {
		printk("palacios: attempt to create %s failed because mkdir failed\n",fks->path);
		goto fail_out;
	    }

 	    // now the directory should exist and have reasonable permissions
	    return (v3_keyed_stream_t) fks;
	}
    } 

    
    // we must be in V3_KS_RD_ONLY or V3_KS_WR_ONLY, 
    // and the directory exists, so we must check the permissions

    if (inode_permission(nd.path.dentry->d_inode, MAY_EXEC | (ot==V3_KS_RD_ONLY ? MAY_READ : MAY_WRITE))) {
	printk("palacios: attempt to open %s, which has the wrong permissions\n",fks->path);
	goto fail_out;
    } else {
	return (v3_keyed_stream_t) fks;
    }


 fail_out:
    kfree(fks->path);
    kfree(fks);
    return 0;

}

static void close_stream_file(v3_keyed_stream_t stream)
{
    struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
    
    kfree(fks->path);
    kfree(fks);

}

static v3_keyed_stream_key_t open_key_file(v3_keyed_stream_t stream,
					   char *key)
{
    struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
    struct file_stream *fs;
    char *path;

    // the path is the stream's path plus the key name
    // file:/home/foo + "regext" => "/home/foo/regext"
    path = (char *) kmalloc(strlen(fks->path)+strlen(key)+2,GFP_KERNEL);
    if (!path) {				
	printk("palacios: cannot allocate file keyed stream for key %s\n",key);
	return 0;
    }
    strcpy(path,fks->path);
    strcat(path,"/");
    strcat(path,key);
    
    fs = (struct file_stream *) kmalloc(sizeof(struct file_stream *),GFP_KERNEL);
    
    if (!fs) { 
	printk("palacios: cannot allocate file keyed stream for key %s\n",key);
	kfree(path);
	return 0;
    }

    fs->stype=STREAM_FILE;

    fs->f = filp_open(path,O_RDWR|O_CREAT,0600);
    
    if (IS_ERR(fs->f)) {
	printk("palacios: cannot open relevent file \"%s\" for stream \"file:%s\" and key \"%s\"\n",path,fks->path,key);
	kfree(fs);
	kfree(path);
	return 0;
    }

    kfree(path);

    return fs;
}


static void close_key_file(v3_keyed_stream_t stream, 
			   v3_keyed_stream_key_t key)
{
    struct file_stream *fs = (struct file_stream *) key;

    filp_close(fs->f,NULL);

    kfree(fs);
}

static sint64_t write_key_file(v3_keyed_stream_t stream, 
			       v3_keyed_stream_key_t key,
			       void *buf,
			       sint64_t len)
{
    struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
    struct file_stream *fs = (struct file_stream *) key;
    mm_segment_t old_fs;
    ssize_t done, left, total;
    
    if (fks->ot!=V3_KS_WR_ONLY) { 
	return -1;
    }
    
    if (len<0) { 
	return -1;
    }

    total=len;
    left=len;

    old_fs = get_fs();
    set_fs(get_ds());

    while (left>0) {
	done = fs->f->f_op->write(fs->f, buf+(total-left), left, &(fs->f->f_pos));
	if (done<=0) {
	    return -1;
	} else {
	    left -= done;
	}
    }
    set_fs(old_fs);

    return len;
}



static sint64_t read_key_file(v3_keyed_stream_t stream, 
			      v3_keyed_stream_key_t key,
			      void *buf,
			      sint64_t len)
{
    struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
    struct file_stream *fs = (struct file_stream *) key;
    mm_segment_t old_fs;
    ssize_t done, left, total;
    
    if (fks->ot!=V3_KS_RD_ONLY) { 
	return -1;
    }

    if (len<0) { 
	return -1;
    }

    total=len;
    left=len;

    old_fs = get_fs();
    set_fs(get_ds());

    while (left>0) {
	done = fs->f->f_op->read(fs->f, buf+(total-left), left, &(fs->f->f_pos));
	if (done<=0) {
	    return -1;
	} else {
	    left -= done;
	}
    }
    set_fs(old_fs);

    return len;

}




/***************************************************************************************************
  Generic interface
*************************************************************************************************/

static v3_keyed_stream_t open_stream(char *url,
				     v3_keyed_stream_open_t ot)
{
    if (!strncasecmp(url,"mem:",4)) { 
	return open_stream_mem(url,ot);
    } else if (!strncasecmp(url,"file:",5)) { 
	return open_stream_file(url,ot);
    } else {
	printk("palacios: unsupported type in attempt to open keyed stream \"%s\"\n",url);
	return 0;
    }
}

static void close_stream(v3_keyed_stream_t stream)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    return close_stream_mem(stream);
	    break;
	case STREAM_FILE:
	    return close_stream_file(stream);
	    break;
	default:
	    printk("palacios: unknown stream type %d in close\n",gks->stype);
	    break;
    }
}

static v3_keyed_stream_key_t open_key(v3_keyed_stream_t stream,
				      char *key)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    return open_key_mem(stream,key);
	    break;
	case STREAM_FILE:
	    return open_key_file(stream,key);
	    break;
	default:
	    printk("palacios: unknown stream type %d in open_key\n",gks->stype);
	    break;
    }
    return 0;
}


static void close_key(v3_keyed_stream_t stream, 
		      v3_keyed_stream_key_t key)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    return close_key_mem(stream,key);
	    break;
	case STREAM_FILE:
	    return close_key_file(stream,key);
	    break;
	default:
	    printk("palacios: unknown stream type %d in close_key\n",gks->stype);
	    break;
    }
    // nothing to do
    return;
}

static sint64_t write_key(v3_keyed_stream_t stream, 
			  v3_keyed_stream_key_t key,
			  void *buf,
			  sint64_t len)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    return write_key_mem(stream,key,buf,len);
	    break;
	case STREAM_FILE:
	    return write_key_file(stream,key,buf,len);
	    break;
	default:
	    printk("palacios: unknown stream type %d in write_key\n",gks->stype);
	    return -1;
	    break;
    }
    return -1;
}


static sint64_t read_key(v3_keyed_stream_t stream, 
			 v3_keyed_stream_key_t key,
			 void *buf,
			 sint64_t len)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    return read_key_mem(stream,key,buf,len);
	    break;
	case STREAM_FILE:
	    return read_key_file(stream,key,buf,len);
	    break;
	default:
	    printk("palacios: unknown stream type %d in write_key\n",gks->stype);
	    return -1;
	    break;
    }
    return -1;
}




/***************************************************************************************************
  Hooks to palacios and inititialization
*************************************************************************************************/

    
static struct v3_keyed_stream_hooks hooks = {
    .open = open_stream,
    .close = close_stream,
    .open_key = open_key,
    .close_key = close_key,
    .read_key = read_key,
    .write_key = write_key
};


static int init_keyed_streams( void )
{
    streams = palacios_create_htable(DEF_NUM_STREAMS,hash_func,hash_comp);

    if (!streams) { 
	printk("palacios: failed to allocated stream pool for in-memory streams\n");
	return -1;
    }

    V3_Init_Keyed_Streams(&hooks);


    return 0;

}

static int deinit_keyed_streams( void )
{
    printk("DEINIT OF PALACIOS KEYED STREAMS NOT IMPLEMENTED - WE HAVE JUST LEAKED MEMORY and/or file handles!\n");
    return -1;
}


static struct linux_ext key_stream_ext = {
    .name = "KEYED_STREAM_INTERFACE",
    .init = init_keyed_streams,
    .deinit = deinit_keyed_streams,
    .guest_init = NULL,
    .guest_deinit = NULL
};


register_extension(&key_stream_ext);
