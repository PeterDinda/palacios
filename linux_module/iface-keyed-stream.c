/*
 * Palacios keyed stream interface
 *
 * Plus implementations for mem, file, and user space implementations
 *
 * (c) Peter Dinda, 2011 (interface, mem + file implementations + recooked user impl)
 * (c) Clint Sbisa, 2011 (initial user space implementation on which this is based)
 * (c) Diana Palsetia & Steve Rangel, 2012 (network based implementation)	
 * (c) Peter Dinda, 2012 (updated interface, textfile)
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>

#include "palacios.h"
#include "util-hashtable.h"
#include "linux-exts.h"
#include "vm.h"

#define sint64_t int64_t
#include <interfaces/vmm_keyed_stream.h>

#include "iface-keyed-stream-user.h"

#include <interfaces/vmm_socket.h>

#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/inet.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/syscalls.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/slab.h>


/*
  This is an implementation of the Palacios keyed stream interface
  that supports four flavors of streams:

  "mem:"   Streams are stored in a hash table
  The values for this hash table are hash tables associated with 
  each stream.   A key maps to an expanding memory buffer.
  Data is stored in a buffer like:

  [boundarytag][taglen][tag][datalen][data]
  [boundarytag][taglen][tag][datalen][data]
  ...

  "file:"  Streams are stored in files.  Each high-level
  open corresponds to a directory, while a key corresponds to
  a distinct file in that directory.   Data is stored in a file
  like:

  [boundarytag][taglen][tag][datalen][data]
  [boundarytag][taglen][tag][datalen][data]
  ...

  "textfile:" Same as file, but data is stored in text format, like a
  windows style .ini file.  A key maps to a file, and data is stored
  in a file like:

  [key]
  tag=data_in_hex
  tag=data_in_hex

  This format makes it possible to concentenate the files to
  produce a single "ini" file with "sections".


  "net:"  Streams are carried over the network.  Each
   high level open corresponds to a TCP connection, while
   each key corresponds to a context on the stream.
      "net:a:<ip>:<port>" => Bind to <ip>:<port> and accept a connection
      "net:c:<ip>:<port>" => Connect to <ip>:<port>
   "c" (client) 
     open_stream: connect
   "a" (server) 
     open_stream: accept
   "c" or "a":
     open_key:  send [keylen-lastbyte-high-bit][key] (writer)
           or   recv (same format as above)          (reader)
     close_key: send [keylen-lastbyte-high-bit][key] (writer)
           or   recv (same format as above)          (reader)
     write_key: send [boundarytag][taglen][tag][datalen][data]
     read_key:  recv (same format as above)
     close_stream: close socket

  "user:" Stream requests are bounced to user space to be 
   handled there.  A rendezvous approach similar to the host 
   device userland support is used

   All keyed streams store the tags.
   
*/

#define STREAM_GENERIC 0
#define STREAM_MEM     1
#define STREAM_FILE    2
#define STREAM_USER    3
#define STREAM_NETWORK 4
#define STREAM_TEXTFILE 5

/*
  All keyed streams and streams indicate their implementation type within the first field
 */
struct generic_keyed_stream {
    int stype;
};

struct generic_stream {
    int stype;
};
  
/*
  boundary tags are used for some othe raw formats. 
*/
static uint32_t BOUNDARY_TAG=0xabcd0123;


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

static struct mem_stream *create_mem_stream_internal(uint64_t size)
{
    struct mem_stream *m = palacios_alloc(sizeof(struct mem_stream));

    if (!m) {
	return 0;
    }


    m->data = palacios_valloc(size);
    
    if (!m->data) { 
	palacios_free(m);
	return 0;
    }

    m->stype = STREAM_MEM;
    m->size=size;
    m->ptr=0;
    m->data_max=0;
    
    return m;
}


static struct mem_stream *create_mem_stream(void)
{
    return create_mem_stream_internal(DEF_SIZE);
}

static void destroy_mem_stream(struct mem_stream *m)
{
    if (m) {
	if (m->data) {
	    palacios_vfree(m->data);
	}
	m->data=0;
	palacios_free(m);
    }
}
    
static int expand_mem_stream(struct mem_stream *m, uint32_t new_size)
{
    void *data = palacios_valloc(new_size);
    uint32_t nc;

    if (!data) { 
	return -1;
    }
    
    nc = (new_size<m->data_max) ? new_size : m->data_max;

    memcpy(data,m->data,nc);

    palacios_vfree(m->data);

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
static struct hashtable *mem_streams=0;


static v3_keyed_stream_t open_stream_mem(char *url,
					 v3_keyed_stream_open_t ot)
{

    if (strncasecmp(url,"mem:",4)) { 
	WARNING("illegitimate attempt to open memory stream \"%s\"\n",url);
	return 0;
    }

    switch (ot) { 
	case V3_KS_RD_ONLY:
	case V3_KS_WR_ONLY: {
	    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) palacios_htable_search(mem_streams,(addr_t)(url+4));
	    if (mks) { 
		mks->ot=ot;
	    }
	    return (v3_keyed_stream_t) mks;
	}
	    break;

	case V3_KS_WR_ONLY_CREATE: {
	    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) palacios_htable_search(mem_streams,(addr_t)(url+4));
	    if (!mks) { 
		char *mykey;

		mykey = palacios_alloc(strlen(url+4)+1);

		if (!mykey) { 
		    ERROR("cannot allocate space for new in-memory keyed stream %s\n",url);
		    return 0;
		}

		strcpy(mykey,url+4);
		
		mks = (struct mem_keyed_stream *) palacios_alloc(sizeof(struct mem_keyed_stream));

		if (!mks) { 
		    palacios_free(mykey);
		    ERROR("cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}
	    
		mks->ht = (void*) palacios_create_htable(DEF_NUM_KEYS,hash_func,hash_comp);
		if (!mks->ht) { 
		    palacios_free(mks);
		    palacios_free(mykey);
		    ERROR("cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}

		
		if (!palacios_htable_insert(mem_streams,(addr_t)(mykey),(addr_t)mks)) { 
		    palacios_free_htable(mks->ht,1,1);
		    palacios_free(mks);
		    palacios_free(mykey);
		    ERROR("cannot insert in-memory keyed stream %s\n",url);
		    return 0;
		}
		mks->stype=STREAM_MEM;
	    }

	    mks->ot=V3_KS_WR_ONLY;
	    
	    return mks;
	    
	}
	    break;

	default:
	    ERROR("unsupported open type in open_stream_mem\n");
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
	char *mykey = palacios_alloc(strlen(key)+1);

	if (!mykey) { 
	    ERROR("cannot allocate copy of key for key %s\n",key);
	    return 0;
	}

	strcpy(mykey,key);

	m = create_mem_stream();
	
	if (!m) { 
	    palacios_free(mykey);
	    ERROR("cannot allocate mem keyed stream for key %s\n",key);
	    return 0;
	}

	if (!palacios_htable_insert(s,(addr_t)mykey,(addr_t)m)) {
	    destroy_mem_stream(m);
	    palacios_free(mykey);
	    ERROR("cannot insert mem keyed stream for key %s\n",key);
	    return 0;
	}
    }

    reset_mem_stream(m);
    return m;

}


static void preallocate_hint_key_mem(v3_keyed_stream_t stream,
				     char *key,
				     uint64_t size)
{
    struct mem_keyed_stream *mks = (struct mem_keyed_stream *) stream;
    struct hashtable *s = mks->ht;

    struct mem_stream *m;

    if (mks->ot != V3_KS_WR_ONLY) { 
	return;
    }

    m = (struct mem_stream *) palacios_htable_search(s,(addr_t)key);

    if (!m) {
	char *mykey;
	
	mykey=palacios_alloc(strlen(key)+1);
	
	if (!mykey) { 
	    ERROR("cannot allocate key space for preallocte for key %s\n",key);
	    return;
	}
	
	strcpy(mykey,key);
       
	m = create_mem_stream_internal(size);
	
	if (!m) { 
	    ERROR("cannot preallocate mem keyed stream for key %s\n",key);
	    return;
	}

	if (!palacios_htable_insert(s,(addr_t)mykey,(addr_t)m)) {
	    ERROR("cannot insert preallocated mem keyed stream for key %s\n",key);
	    destroy_mem_stream(m);
	    return;
	}
    } else {
	if (m->data_max < size) { 
	    if (expand_mem_stream(m,size)) { 
		ERROR("cannot expand key for preallocation for key %s\n",key);
		return;
	    }
	}
    }

    return;

}

static void close_key_mem(v3_keyed_stream_t stream, 
			  v3_keyed_stream_key_t key)
{
    // nothing to do
    return;
}

static sint64_t write_key_mem(v3_keyed_stream_t stream, 
			      v3_keyed_stream_key_t key,
			      void *tag,
			      sint64_t taglen,
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
  
  if (taglen<0 || len<0) { 
    ERROR("Negative taglen or data len\n");
    return -1;
  }
  
  if (taglen>0xffffffffULL || len>0xffffffffULL) { 
    ERROR("taglen or data len is too large\n");
    return -1;
  }
      
  writelen=write_mem_stream(m,&BOUNDARY_TAG,sizeof(BOUNDARY_TAG));
  
  if (writelen!=sizeof(BOUNDARY_TAG)) { 
    ERROR("failed to write all data for boundary tag\n");
    return -1;
  }
  
  writelen=write_mem_stream(m,&taglen,sizeof(taglen));
  
  if (writelen!=sizeof(taglen)) { 
    ERROR("failed to write taglen\n");
    return -1;
  }
  
  mylen = (uint32_t) taglen;
  
  writelen=write_mem_stream(m,tag,mylen);
  
  if (writelen!=mylen) { 
    ERROR("failed to write all data for tag\n");
    return -1;
  } 

  writelen=write_mem_stream(m,&len,sizeof(len));
  
  if (writelen!=sizeof(len)) { 
    ERROR("failed to write datalen\n");
    return -1;
  }
  
  mylen = (uint32_t) len;

  writelen=write_mem_stream(m,buf,mylen);

  if (writelen!=mylen) { 
    ERROR("failed to write all data for key\n");
    return -1;
  } else {
    return (sint64_t)writelen;
  }
}

static sint64_t read_key_mem(v3_keyed_stream_t stream, 
			     v3_keyed_stream_key_t key,
			     void *tag,
			     sint64_t taglen,
			     void *buf,
			     sint64_t len)
{
  struct mem_keyed_stream *mks = (struct mem_keyed_stream *) stream;
  struct mem_stream *m = (struct mem_stream *) key;
  uint32_t mylen;
  uint32_t readlen;
  void *temptag;
  uint32_t tempbt;
  sint64_t templen;
  
  
  if (mks->ot!=V3_KS_RD_ONLY) {
    return -1;
  }
  
  if (len<0 || taglen<0) { 
    ERROR("taglen or data len is negative\n");
    return -1;
  }

  if (len>0xffffffffULL || taglen>0xffffffffULL) { 
    ERROR("taglen or data len is too large\n");
    return -1;
  }
  
  readlen=read_mem_stream(m,&tempbt,sizeof(tempbt));
  
  if (readlen!=sizeof(tempbt)) { 
    ERROR("failed to read all data for boundary tag\n");
    return -1;
  } 
  
  if (tempbt!=BOUNDARY_TAG) { 
    ERROR("boundary tag not found (read 0x%x)\n",tempbt);
    return -1;
  }
  
  readlen=read_mem_stream(m,&templen,sizeof(templen));

  if (readlen!=sizeof(templen)) { 
    ERROR("failed to read all data for taglen\n");
    return -1;
  } 

  if (templen!=taglen) { 
    ERROR("tag size mismatch (requested=%lld, actual=%lld)\n",taglen,templen);
    return -1;
  }
  
  temptag = palacios_alloc(taglen);
    
  if (!temptag) { 
    ERROR("cannot allocate temporary tag\n");
    return -1;
  }

  mylen = (uint32_t) len;
    
  readlen=read_mem_stream(m,temptag,mylen);
    
  if (readlen!=mylen) { 
    ERROR("failed to read all data for tag\n");
    palacios_free(temptag);
    return -1;
  } 

  if (memcmp(tag,temptag,taglen)) { 
    ERROR("tag mismatch\n");
    palacios_free(temptag);
    return -1;
  }
  
  palacios_free(temptag);

  readlen=read_mem_stream(m,&templen,sizeof(templen));

  if (readlen!=sizeof(templen)) { 
    ERROR("failed to read all data for data len\n");
    return -1;
  } 
  
  if (templen!=len) { 
    ERROR("data size mismatch (requested=%lld, actual=%lld)\n",len,templen);
    return -1;
  }

  mylen = (uint32_t) len;
  
  readlen=read_mem_stream(m,buf,mylen);
  
  if (readlen!=mylen) { 
    ERROR("failed to read all data for key\n");
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
	WARNING("illegitimate attempt to open file stream \"%s\"\n",url);
	return 0;
    }

    fks = palacios_alloc(sizeof(struct file_keyed_stream));
    
    if (!fks) { 
	ERROR("cannot allocate space for file stream\n");
	return 0;
    }

    fks->path = (char*)palacios_alloc(strlen(url+5)+1);
    
    if (!(fks->path)) { 
	ERROR("cannot allocate space for file stream\n");
	palacios_free(fks);
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
	    ERROR("attempt to open %s, which does not exist\n",fks->path);
	    goto fail_out;

	} else {

	    // We are being asked to create it

	    struct dentry *de;
	    int err;

	    // Find its parent
	    if (path_lookup(fks->path,LOOKUP_PARENT|LOOKUP_FOLLOW,&nd)) { 
		ERROR("attempt to create %s failed because its parent cannot be looked up\n",fks->path);
		goto fail_out;
	    }

	    // Can we write to the parent?

	    if (inode_permission(nd.path.dentry->d_inode, MAY_WRITE | MAY_EXEC)) { 
		ERROR("attempt to open %s, which has the wrong permissions for directory creation\n",fks->path);
		goto fail_out;
	    }

	    // OK, we can, so let's create it

	    de = lookup_create(&nd,1);

	    if (IS_ERR(de)) { 
		ERROR("cannot allocate dentry\n");
		goto fail_out;
	    }

	    err = vfs_mkdir(nd.path.dentry->d_inode, de, 0700);

	    // lookup_create locks this for us!

	    mutex_unlock(&(nd.path.dentry->d_inode->i_mutex));

	    if (err) {
		ERROR("attempt to create %s failed because mkdir failed\n",fks->path);
		goto fail_out;
	    }

 	    // now the directory should exist and have reasonable permissions
	    return (v3_keyed_stream_t) fks;
	}
    } 

    
    // we must be in V3_KS_RD_ONLY or V3_KS_WR_ONLY, 
    // and the directory exists, so we must check the permissions

    if (inode_permission(nd.path.dentry->d_inode, MAY_EXEC | (ot==V3_KS_RD_ONLY ? MAY_READ : MAY_WRITE))) {
	ERROR("attempt to open %s, which has the wrong permissions\n",fks->path);
	goto fail_out;
    } else {
	return (v3_keyed_stream_t) fks;
    }


 fail_out:
    palacios_free(fks->path);
    palacios_free(fks);
    return 0;

}

static void close_stream_file(v3_keyed_stream_t stream)
{
    struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
    
    palacios_free(fks->path);
    palacios_free(fks);

}

static void preallocate_hint_key_file(v3_keyed_stream_t stream,
				      char *key,
				      uint64_t size)
{
    return;
}

static v3_keyed_stream_key_t open_key_file(v3_keyed_stream_t stream,
					   char *key)
{
    struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
    struct file_stream *fs;
    char *path;

    // the path is the stream's path plus the key name
    // file:/home/foo + "regext" => "/home/foo/regext"
    path = (char *) palacios_alloc(strlen(fks->path)+strlen(key)+2);
    if (!path) {				
	ERROR("cannot allocate file keyed stream for key %s\n",key);
	return 0;
    }
    strcpy(path,fks->path);
    strcat(path,"/");
    strcat(path,key);
    
    fs = (struct file_stream *) palacios_alloc(sizeof(struct file_stream *));
    
    if (!fs) { 
	ERROR("cannot allocate file keyed stream for key %s\n",key);
	palacios_free(path);
	return 0;
    }

    fs->stype=STREAM_FILE;

    fs->f = filp_open(path,O_RDWR|O_CREAT|O_LARGEFILE,0600);

    if (IS_ERR(fs->f)) {
	ERROR("cannot open relevent file \"%s\" for stream \"file:%s\" and key \"%s\"\n",path,fks->path,key);
	palacios_free(fs);
	palacios_free(path);
	return 0;
    }

    palacios_free(path);

    return fs;
}


static void close_key_file(v3_keyed_stream_t stream, 
			   v3_keyed_stream_key_t key)
{
    struct file_stream *fs = (struct file_stream *) key;

    filp_close(fs->f,NULL);

    palacios_free(fs);
}


static sint64_t write_file(struct file_stream *fs, void *buf, sint64_t len)
{
    ssize_t done, left, total;
    mm_segment_t old_fs;

    total=len;
    left=len;

    while (left>0) {
        old_fs = get_fs();
        set_fs(get_ds());
	done = fs->f->f_op->write(fs->f, buf+(total-left), left, &(fs->f->f_pos));
        set_fs(old_fs);
	if (done<=0) {
	    return -1;
	} else {
	    left -= done;
	}
    }

    return len;
}

static sint64_t write_key_file(v3_keyed_stream_t stream, 
			       v3_keyed_stream_key_t key,
			       void *tag,
			       sint64_t taglen,
			       void *buf,
			       sint64_t len)
{
  struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
  struct file_stream *fs = (struct file_stream *) key;
  sint64_t writelen;
  
  if (fks->ot!=V3_KS_WR_ONLY) { 
    return -1;
  }
  
  if (taglen<0 || len<0) { 
    ERROR("Negative taglen or data len\n");
    return -1;
  }
  
  writelen=write_file(fs,&BOUNDARY_TAG,sizeof(BOUNDARY_TAG));
  
  if (writelen!=sizeof(BOUNDARY_TAG)) { 
    ERROR("failed to write all data for boundary tag\n");
    return -1;
  }
  
  writelen=write_file(fs,&taglen,sizeof(taglen));
  
  if (writelen!=sizeof(taglen)) { 
    ERROR("failed to write taglen\n");
    return -1;
  }
  
  if (write_file(fs,tag,taglen)!=taglen) { 
    ERROR("failed to write tag\n");
    return -1;
  }

  writelen=write_file(fs,&len,sizeof(len));
  
  if (writelen!=sizeof(len)) { 
    ERROR("failed to write data len\n");
    return -1;
  }
  
  return write_file(fs,buf,len);
}

static sint64_t read_file(struct file_stream *fs, void *buf, sint64_t len)
{
    ssize_t done, left, total;
    mm_segment_t old_fs;

    total=len;
    left=len;


    while (left>0) {
        old_fs = get_fs();
        set_fs(get_ds());
	done = fs->f->f_op->read(fs->f, buf+(total-left), left, &(fs->f->f_pos));
        set_fs(old_fs);
	if (done<=0) {
	    return -1;
	} else {
	    left -= done;
	}
    }

    return len;
}


static sint64_t read_key_file(v3_keyed_stream_t stream, 
			      v3_keyed_stream_key_t key,
			      void *tag,
			      sint64_t taglen,
			      void *buf,
			      sint64_t len)
{
  struct file_keyed_stream *fks = (struct file_keyed_stream *) stream;
  struct file_stream *fs = (struct file_stream *) key;
  void *temptag;
  uint32_t tempbt;
  sint64_t templen;
  sint64_t readlen;
  
  if (fks->ot!=V3_KS_RD_ONLY) { 
    return -1;
  }
  
  if (len<0 || taglen<0) { 
    ERROR("taglen or data len is negative\n");
    return -1;
  }

  readlen=read_file(fs,&tempbt,sizeof(tempbt));
  
  if (readlen!=sizeof(tempbt)) { 
    ERROR("failed to read all data for boundary tag\n");
    return -1;
  } 
  
  if (tempbt!=BOUNDARY_TAG) { 
    ERROR("boundary tag not found (read 0x%x)\n",tempbt);
    return -1;
  }

  readlen=read_file(fs,&templen,sizeof(templen));
  
  if (readlen!=sizeof(templen)) { 
    ERROR("failed to read all data for tag len\n");
    return -1;
  } 

  if (templen!=taglen) { 
    ERROR("tag size mismatch (requested=%lld, actual=%lld)\n",taglen,templen);
    return -1;
  }

  temptag=palacios_alloc(taglen);

  if (!temptag) { 
    ERROR("Cannot allocate temptag\n");
    return -1;
  }
  
  if (read_file(fs,temptag,taglen)!=taglen) { 
    ERROR("Cannot read tag\n");
    palacios_free(temptag);
    return -1;
  }

  if (memcmp(temptag,tag,taglen)) { 
    ERROR("Tag mismatch\n");
    palacios_free(temptag);
    return -1;
  }
  
  palacios_free(temptag);

  readlen=read_file(fs,&templen,sizeof(templen));
  
  if (readlen!=sizeof(templen)) { 
    ERROR("failed to read all data for data len\n");
    return -1;
  } 

  if (templen!=len) { 
    ERROR("datasize mismatch (requested=%lld, actual=%lld)\n",len,templen);
    return -1;
  }

  return read_file(fs,buf,len);

}


/***************************************************************************************************
  Textfile-based implementation  ("textfile:")

  Note that this implementation uses the internal structure and functions of the 
  "file:" implementation
*************************************************************************************************/

// optimize the reading and decoding of hex data
// this weakens the parser, so that:
// tag =0A0B0D 
// will work, but
// tag = 0A 0B 0D
// will not.  Note the leading whitespace
#define TEXTFILE_OPT_HEX 0

//
// The number of bytes handled at a time by the hex putter and getter
//
#define MAX_HEX_SEQ 64

/*
  A text file keyed stream is a file_keyed_stream,
  only with a different stype
*/

#define PAUSE()  
//#define PAUSE() ssleep(5) 


typedef struct file_keyed_stream textfile_keyed_stream;

typedef struct file_stream textfile_stream;


static v3_keyed_stream_t open_stream_textfile(char *url,
					      v3_keyed_stream_open_t ot)
{
  textfile_keyed_stream *me;

  if (strncasecmp(url,"textfile:",9)) { 
    WARNING("illegitimate attempt to open textfile stream \"%s\"\n",url);
    return 0;
  }

  me = (textfile_keyed_stream *) open_stream_file(url+4, ot);

  if (!me) {
    ERROR("could not create underlying file stream\n");
    return 0;
  }

  me->stype=STREAM_TEXTFILE;

  return me;
}

  

static void close_stream_textfile(v3_keyed_stream_t stream)
{
  textfile_keyed_stream *me = stream;

  me->stype=STREAM_FILE;

  close_stream_file(me);

}

static void preallocate_hint_key_textfile(v3_keyed_stream_t stream,
					  char *key,
					  uint64_t size)
{
  textfile_keyed_stream *me = stream;
  
  me->stype=STREAM_FILE;

  preallocate_hint_key_file(me,key,size);
  
  me->stype=STREAM_TEXTFILE;
 
}


static inline int isoneof(char c, char *sl, int m)
{
  int i;
  
  for (i=0;i<m;i++) { 
    if (c==sl[i]) { 
      return 1;
    }
  }
  
  return 0;
}

static char get_next_char(textfile_stream *s)
{
  char buf;
  if (read_file(s,&buf,1)!=1) { 
    return -1;
  } 
  return buf;
}

static char hexify_nybble(char c)
{
  if (c>=0 && c<=9) { 
    return '0'+c;
  } else if (c>=0xa && c<=0xf) { 
    return 'a'+(c-0xa);
  } else {
    return -1;
  }
}

static int hexify_byte(char *c, char b)
{
  char n;
  n = hexify_nybble( (b >> 4) & 0xf);
  if (n==-1) { 
    return -1;
  }
  c[0] = n;
  n = hexify_nybble( b & 0xf);
  if (n==-1) { 
    return -1;
  }
  c[1] = n;
  return 0;
}


static char dehexify_nybble(char c)
{
  if (c>='0' && c<='9') { 
    return c-'0';
  } else if (c>='a' && c<='f') {
    return 0xa + (c-'a');
  } else if (c>='A' && c<='F') { 
    return 0xa + (c-'A');
  } else {
    return -1;
  }
}

static int dehexify_byte(char *c, char *b)
{
  char n;
  n = dehexify_nybble(c[0]);
  if (n==-1) { 
    return -1;
  }
  *b = n << 4;
  n = dehexify_nybble(c[1]);
  if (n==-1) { 
    return -1;
  }
  *b |= n;
  return 0;
}


#if TEXTFILE_OPT_HEX


// Here the sl array, of length m is the number
static int get_hexbytes_as_data(textfile_stream *s, char *buf, int n)
{
  char rbuf[MAX_HEX_SEQ*2];
  int left = n;
  int off = 0;
  int cur = 0;
  int i;

  while (left>0) {
    cur = left > MAX_HEX_SEQ ? MAX_HEX_SEQ : left;
    if (read_file(s,rbuf,cur*2)!=cur*2) { 
      ERROR("Cannot read data in getting hexbytes as data\n");
      return -1;
    }
    
    for (i=0;i<cur;i++) {
      if (dehexify_byte(rbuf+(i*2),buf+off+i)==-1) { 
	ERROR("Cannot decode data as hex in getting hexbytes as data\n");
	return -1;
      }
    }
    left-=cur;
    off+=cur;
  } 

  return 0;
}    

#endif

// Here the sl array, of length m is the set of characters to skip (e.g., whitespace)
static int get_hexbytes_as_data_skip(textfile_stream *s, char *buf, int n, char *sl, int m)
{
  char rbuf[2];
  int which = 0;
  int cur=0;
    
  while (cur<n) {
    which=0;
    while (which<2) { 
      rbuf[which] = get_next_char(s);
      if (rbuf[which]==-1) { 
	ERROR("Cannot read char in getting hexbytes as data with skiplist");
	return -1;
      }
      if (isoneof(rbuf[which],sl,m)) { 
	continue;
      } else {
	which++;
      }
    }
    if (dehexify_byte(rbuf,buf+cur)==-1) { 
      ERROR("Cannot decode data as hex in getting hexbytes as data with skiplist\n");
      return -1;
    } else {
      cur++;
    }
  }
  return 0;
}    

/*
static int put_next_char(textfile_stream *s, char d)
{
  return write_file(s,&d,1);
}
*/


static int put_data_as_hexbytes(textfile_stream *s, char *buf, int n)
{
  char rbuf[MAX_HEX_SEQ*2];
  int left = n;
  int off = 0;
  int cur = 0;
  int i;

  while (left>0) {
    cur = left > MAX_HEX_SEQ ? MAX_HEX_SEQ : left;
    for (i=0;i<cur;i++) {
      if (hexify_byte(rbuf+(i*2),*(buf+off+i))==-1) { 
	ERROR("Cannot encode data as hex in putting data as hexbytes\n");
	return -1;
      }
    }
    if (write_file(s,rbuf,cur*2)!=cur*2) { 
      ERROR("Cannot write data in putting data as hexbytes\n");
      return -1;
    }
    left-=cur;
    off+=cur;
  } 

  return 0;
}    


static int put_string_n(textfile_stream *s, char *buf, int n)
{
  int rc;

  rc = write_file(s,buf,n);
  
  if (rc!=n) { 
    return -1;
  } else {
    return 0;
  }
}

static int put_string(textfile_stream *s, char *buf)
{
  int n=strlen(buf);

  return put_string_n(s,buf,n);
}



static int search_for(textfile_stream *s, char d)
{
  char c;
  do {
    c=get_next_char(s);
  } while (c!=-1 && c!=d);
  
  if (c==d) { 
    return 0;
  } else {
    return -1;
  }
}

/*
static int skip_matching(textfile_stream *s, char *m, int n)
{
  char c;
  int rc = 0;
  int i;

  while (rc==0) { 
    c=get_next_char(s);
    if (c==-1) { 
      rc=-1;
    } else {
      for (i=0;i<n;i++) { 
	if (c==m[i]) {
	  rc=1;
	  break;
	} 
      }
    }
  }
  
  if (rc==1) { 
    return 0;  // found
  } else {
    return rc; // unknown
  }
}

*/


static int token_scan(textfile_stream *s, char *token, int n, char *sl, int m)
{
  char c;
  int cur;

  // Skip whitespace
  do {
    c=get_next_char(s);
    if (c==-1) { 
      ERROR("Failed to get character during token scan (preceding whitespace)\n");
      return -1;
    }
  } while (isoneof(c,sl,m));


  token[0]=c;
  
  // Record
  cur=1;
  while (cur<(n-1)) { 
    c=get_next_char(s);
    if (c==-1) { 
      ERROR("Failed to get character during token scan (token)\n");
      return -1;
    }
    if (isoneof(c,sl,m)) { 
      break;
    } else {
      token[cur]=c;
      cur++;
    } 
  }
  token[cur]=0;
  return 0;
}



static v3_keyed_stream_key_t open_key_textfile(v3_keyed_stream_t stream,
					       char *key)
{
  textfile_keyed_stream *mks = stream;
  textfile_stream *ms;

  mks->stype=STREAM_FILE;

  ms = open_key_file(mks,key);

  if (!ms) { 
    ERROR("cannot open underlying file keyed stream for key %s\n",key);
    mks->stype=STREAM_TEXTFILE;
    return 0;
  }

  if (mks->ot==V3_KS_WR_ONLY) { 
    
    // Now we write the section header

    ms->stype=STREAM_FILE;
    
    if (put_string(ms,"[")) { 
      close_key_file(mks,ms);
      mks->stype=STREAM_TEXTFILE;
      return 0;
    }
    
    if (put_string(ms,key)) { 
      close_key_file(mks,ms);
      mks->stype=STREAM_TEXTFILE;
      return 0;
    }
    
    if (put_string(ms,"]\n")) {
      close_key_file(mks,ms);
      mks->stype=STREAM_TEXTFILE;
      return 0;
    }
    
    
    mks->stype=STREAM_TEXTFILE;
    ms->stype=STREAM_TEXTFILE;

    return ms;

  } else if (mks->ot == V3_KS_RD_ONLY) {
    // Now we readthe section header
    int keylen=strlen(key);
    char *tempkey = palacios_alloc(keylen+3);

    ms->stype=STREAM_FILE;

    if (!tempkey) { 
      ERROR("Allocation failed in opening key\n");
      close_key_file(mks,ms);
      mks->stype=STREAM_FILE;
      return 0;
    }


    if (token_scan(ms,tempkey,keylen+3,"\t\r\n",3)) { 
      ERROR("Cannot scan for token (key search)\n");
      close_key_file(mks,ms);
      mks->stype=STREAM_TEXTFILE;
      palacios_free(tempkey);
      return 0;
    }
    
    tempkey[keylen+2] = 0;
    
    // Should now have [key]0

    if (tempkey[0]!='[' ||
	tempkey[keylen+1]!=']' ||
	memcmp(key,tempkey+1,keylen)) {
      ERROR("key mismatch: target key=%s, found %s\n",key,tempkey);
      palacios_free(tempkey);
      close_key_file(mks,ms);
      mks->stype=STREAM_TEXTFILE;
      return 0;
    }

    // key match done, success

    mks->stype=STREAM_TEXTFILE;
    ms->stype=STREAM_TEXTFILE;

    palacios_free(tempkey);

    return ms;
    
  } else {
    ERROR("Unknown open type in open_key_textfile\n");
    ms->stype=STREAM_FILE;
    close_key_file(mks,ms);
    return 0;
  }

}



static void close_key_textfile(v3_keyed_stream_t stream, 
			       v3_keyed_stream_key_t key)
{
  textfile_keyed_stream *mks = stream;
  textfile_stream *ms=key;

  mks->stype=STREAM_FILE;
  ms->stype=STREAM_FILE;

  close_key_file(mks,ms);

  mks->stype=STREAM_TEXTFILE;

}


static sint64_t read_key_textfile(v3_keyed_stream_t stream, 
				  v3_keyed_stream_key_t key,
				  void *tag,
				  sint64_t taglen,
				  void *buf,
				  sint64_t len)
{
    textfile_stream *ms = (textfile_stream *) key;
    char tags[32];
    char *temptag;



    memcpy(tags,tag,taglen<31 ? taglen : 31);
    tags[taglen<32? taglen : 31 ]=0;
    
    temptag=palacios_alloc(taglen+1);
    if (!temptag) { 
      ERROR("Unable to allocate temptag in textfile read key\n");
      return -1;
    }

    ms->stype=STREAM_FILE;
    
    if (token_scan(ms,temptag,taglen+1," \t\r\n=",5)) { 
      ERROR("Cannot scan for token (tag search)\n");
      ms->stype=STREAM_TEXTFILE;
      palacios_free(temptag);
      return -1;
    }

    if (memcmp(tag,temptag,taglen)) { 
      ERROR("Tag mismatch in reading tag from textfile: desired tag=%s, actual tag=%s\n",tags,temptag);
      ms->stype=STREAM_TEXTFILE;
      palacios_free(temptag);
      return -1;
    }

    // tag matches, let's go and find our =
    palacios_free(temptag);

    if (search_for(ms,'=')) { 
      ERROR("Unable to find = sign in tag data parse (tag=%s)\n", tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }


#if TEXTFILE_OPT_HEX
    if (get_hexbytes_as_data(ms,buf,len)) { 
      ERROR("Cannot read data in hex format (opt path) in textfile for tag %s\n",tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }
#else
    if (get_hexbytes_as_data_skip(ms,buf,len," \t\r\n",4)) { 
      ERROR("Cannot read data in hex format (unopt path) in textfile for tag %s\n",tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }
#endif

    ms->stype=STREAM_TEXTFILE;

    return len;
}

static sint64_t write_key_textfile(v3_keyed_stream_t stream, 
				   v3_keyed_stream_key_t key,
				   void *tag,
				   sint64_t taglen,
				   void *buf,
				   sint64_t len)
{
    textfile_stream *ms = (textfile_stream *) key;
    char tags[32];



    memcpy(tags,tag,taglen<31 ? taglen : 31);
    tags[taglen<32? taglen : 31 ]=0;

    /*    if (taglen>100000 || len>100000) { 
      ERROR("Too big\n");
      return -1;
    }
    */

    ms->stype=STREAM_FILE;

    if (put_string_n(ms,tag,taglen)) { 
      ERROR("Cannot write tag %s in textfile\n",tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }

    if (put_string(ms,"=")) { 
      ERROR("Cannot write = in textfile for tag %s\n",tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }

    if (put_data_as_hexbytes(ms,buf,len)) { 
      ERROR("Cannot write data in hex format in textfile for tag %s\n",tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }

    if (put_string(ms,"\n")) { 
      ERROR("Cannot write trailing lf in textfile for tag %s\n",tags);
      ms->stype=STREAM_TEXTFILE;
      return -1;
    }

    ms->stype=STREAM_TEXTFILE;

    return len;
}



/***************************************************************************************************
  User implementation   ("user:")
*************************************************************************************************/


// List of all user keyed stream connections for the guest
struct user_keyed_streams {
    spinlock_t lock;
    struct list_head streams;
};


// A single keyed stream connection to user space
struct user_keyed_stream {
    int stype;
    v3_keyed_stream_open_t otype;

    char *url;
    spinlock_t lock;
    int waiting;

    wait_queue_head_t user_wait_queue;
    wait_queue_head_t host_wait_queue;

    struct palacios_user_keyed_stream_op *op;

    struct list_head node;
};


//
// List of all of the user streams
//
static struct user_keyed_streams *user_streams;



static int resize_op(struct palacios_user_keyed_stream_op **op, uint64_t buf_len)
{
    struct palacios_user_keyed_stream_op *old = *op;
    struct palacios_user_keyed_stream_op *new;
    
    if (!old) {
	new = palacios_alloc(sizeof(struct palacios_user_keyed_stream_op)+buf_len);
	if (!new) { 
	    return -1;
	} else {
	    new->len=sizeof(struct palacios_user_keyed_stream_op)+buf_len;
	    new->buf_len=buf_len;
	    *op=new;
	    return 0;
	}
    } else {
	if ((old->len-sizeof(struct palacios_user_keyed_stream_op)) >= buf_len) { 
	    old->buf_len=buf_len;
	    return 0;
	} else {
	    palacios_free(old);
	    *op = 0 ;
	    return resize_op(op,buf_len);
	}
    }
}

//
// The assumption is that we enter this with the stream locked
// and we will return with it locked;  additionally, the op structure
// will be overwritten with the response
// 
static int do_request_to_response(struct user_keyed_stream *s, unsigned long *flags)
{

    if (s->waiting) {
	ERROR("user keyed stream request attempted while one is already in progress on %s\n",s->url);
        return -1;
    }

    // we are now waiting for a response
    s->waiting = 1;

    // release the stream
    palacios_spinlock_unlock_irqrestore(&(s->lock), *flags);

    // wake up anyone waiting on it
    wake_up_interruptible(&(s->user_wait_queue));

    // wait for someone to give us a response
    while (wait_event_interruptible(s->host_wait_queue, (s->waiting == 0)) != 0) {}

    // reacquire the lock for our called
    palacios_spinlock_lock_irqsave(&(s->lock), *flags);

    return 0;
}

//
// The assumption is that we enter this with the stream locked
// and we will return with it UNlocked
// 
static int do_response_to_request(struct user_keyed_stream *s, unsigned long *flags)
{

    if (!(s->waiting)) {
	ERROR("user keyed stream response while no request is in progress on %s\n",s->url);
        return -1;
    }

    // we are now waiting for a request
    s->waiting = 0;

    // release the stream
    palacios_spinlock_unlock_irqrestore(&(s->lock), *flags);

    // wake up anyone waiting on it
    wake_up_interruptible(&(s->host_wait_queue));
    
    return 0;
}



static unsigned int keyed_stream_poll_user(struct file *filp, poll_table *wait)
{
    struct user_keyed_stream *s = (struct user_keyed_stream *) (filp->private_data);
    unsigned long flags;
    
    if (!s) {
        return POLLERR;
    }
    
    palacios_spinlock_lock_irqsave(&(s->lock), flags);

    poll_wait(filp, &(s->user_wait_queue), wait);

    if (s->waiting) {
	palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
	return POLLIN | POLLRDNORM;
    }
    
    palacios_spinlock_unlock_irqrestore(&(s->lock), flags);

    return 0;
}

static long keyed_stream_ioctl_user(struct file * filp, unsigned int ioctl, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    unsigned long flags;
    uint64_t size;
    
    struct user_keyed_stream *s = (struct user_keyed_stream *) (filp->private_data);
    
    switch (ioctl) {

	case V3_KSTREAM_REQUEST_SIZE_IOCTL:
	    
	    // inform request size
	    
	    palacios_spinlock_lock_irqsave(&(s->lock), flags);
	    
	    if (!(s->waiting)) {
		palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
		return 0;
	    }

	    size =  sizeof(struct palacios_user_keyed_stream_op) + s->op->buf_len;
	    
	    if (copy_to_user((void * __user) argp, &size, sizeof(uint64_t))) {
		palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
		ERROR("palacios user key size request failed to copy data\n");
		return -EFAULT;
	    }
	    
	    palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
	    
	    return 1;
	    
	    break;

	case V3_KSTREAM_REQUEST_PULL_IOCTL: 
		
	    // pull the request
	    
	    palacios_spinlock_lock_irqsave(&(s->lock), flags);

	    if (!(s->waiting)) {
		palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
		ERROR("palacios user key pull request when not waiting\n");
		return 0;
	    }

	    size =  sizeof(struct palacios_user_keyed_stream_op) + s->op->buf_len;


	    if (copy_to_user((void __user *) argp, s->op, size)) {
		palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
		ERROR("palacios user key pull request failed to copy data\n");
		return -EFAULT;
	    }

	    palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
	    
	    return 1;
	    
	 
	    break;

    case V3_KSTREAM_RESPONSE_PUSH_IOCTL:

        // push the response

        palacios_spinlock_lock_irqsave(&(s->lock), flags);

        if (!(s->waiting)) {
            palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
	    ERROR("palacios user key push response when not waiting\n");
            return 0;
        }
	
        if (copy_from_user(&size, (void __user *) argp, sizeof(uint64_t))) {
	    ERROR("palacios user key push response failed to copy size\n");
            palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
            return -EFAULT;
        }

	if (resize_op(&(s->op),size-sizeof(struct palacios_user_keyed_stream_op))) {
	    ERROR("unable to resize op in user key push response\n");
            palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
	    return -EFAULT;
	}

        if (copy_from_user(s->op, (void __user *) argp, size)) {
            palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
            return -EFAULT;
        }

	do_response_to_request(s,&flags);
	// this will have unlocked s for us

        return 1;

        break;
	
    default:
	ERROR("unknown ioctl in user keyed stream\n");

        return -EFAULT;

	break;
	
    }
}


static int keyed_stream_release_user(struct inode *inode, struct file *filp)
{
    struct user_keyed_stream *s = filp->private_data;
    unsigned long f1,f2;

    palacios_spinlock_lock_irqsave(&(user_streams->lock),f1);
    palacios_spinlock_lock_irqsave(&(s->lock), f2);

    list_del(&(s->node));

    palacios_spinlock_unlock_irqrestore(&(s->lock), f2);
    palacios_spinlock_unlock_irqrestore(&(user_streams->lock), f1);
    
    palacios_free(s->url);
    palacios_free(s);

    return 0;
}

static struct file_operations user_keyed_stream_fops = {
    .poll = keyed_stream_poll_user,
    .compat_ioctl = keyed_stream_ioctl_user,
    .unlocked_ioctl = keyed_stream_ioctl_user,
    .release = keyed_stream_release_user,
};


/*
  user_keyed_streams are allocated on user connect, and deallocated on user release
  
  palacios-side opens and closes only manipulate the open type
*/

int keyed_stream_connect_user(struct v3_guest *guest, unsigned int cmd, unsigned long arg, void *priv_data)
{
    int fd;
    unsigned long flags;
    char *url;
    uint64_t len;
    struct user_keyed_stream *s;
    
    if (!user_streams) { 
	ERROR("no user space keyed streams!\n");
	return -1;
    }

    // get the url
    if (copy_from_user(&len,(void __user *)arg,sizeof(len))) { 
	ERROR("cannot copy url len from user\n");
	return -1;
    }

    url = palacios_alloc(len);
    
    if (!url) { 
	ERROR("cannot allocate url for user keyed stream\n");
	return -1;
    }

    if (copy_from_user(url,((void __user *)arg)+sizeof(len),len)) {
	ERROR("cannot copy url from user\n");
	return -1;
    }
    url[len-1]=0;
	
    
    // Check for duplicate handler
    palacios_spinlock_lock_irqsave(&(user_streams->lock), flags);
    list_for_each_entry(s, &(user_streams->streams), node) {
        if (!strncasecmp(url, s->url, len)) {
            ERROR("user keyed stream connection with url \"%s\" already exists\n", url);
	    palacios_free(url);
            return -1;
        }
    }
    palacios_spinlock_unlock_irqrestore(&(user_streams->lock), flags);
    
    // Create connection
    s = palacios_alloc(sizeof(struct user_keyed_stream));
    
    if (!s) {
	ERROR("cannot allocate new user keyed stream for %s\n",url);
	palacios_free(url);
        return -1;
    }
    
    
    // Get file descriptor
    fd = anon_inode_getfd("v3-kstream", &user_keyed_stream_fops, s, 0);

    if (fd < 0) {
	ERROR("cannot allocate file descriptor for new user keyed stream for %s\n",url);
        palacios_free(s);
	palacios_free(url);
        return -1;
    }
    
    memset(s, 0, sizeof(struct user_keyed_stream));
    
    s->stype=STREAM_USER;
    s->url=url;
    
    init_waitqueue_head(&(s->user_wait_queue));
    init_waitqueue_head(&(s->host_wait_queue));
    
    // Insert connection into list
    palacios_spinlock_lock_irqsave(&(user_streams->lock), flags);
    list_add(&(s->node), &(user_streams->streams));
    palacios_spinlock_unlock_irqrestore(&(user_streams->lock), flags);
    
    return fd;
}
    
static struct user_keyed_stream *keyed_stream_user_find(char *url)
{
    unsigned long flags;
    struct user_keyed_stream *s;
    
    if (!user_streams) { 
	ERROR("no user space keyed streams available\n");
	return NULL;
    }
    
    palacios_spinlock_lock_irqsave(&(user_streams->lock), flags);
    list_for_each_entry(s, &(user_streams->streams), node) {
        if (!strcasecmp(url, s->url)) {
            palacios_spinlock_unlock_irqrestore(&(user_streams->lock), flags);
            return s;
        }
    }
    
    palacios_spinlock_unlock_irqrestore(&(user_streams->lock), flags);
    
    return NULL;
}
    
    
static v3_keyed_stream_t open_stream_user(char *url, v3_keyed_stream_open_t ot)
{
    unsigned long flags;
    struct user_keyed_stream *s;
    
    s = keyed_stream_user_find(url);
    
    if (!s) {
	ERROR("cannot open user stream %s as it does not exist yet\n",url);
        return NULL;
    }

    palacios_spinlock_lock_irqsave(&(s->lock), flags);

    if (s->waiting) {
        palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
	ERROR("cannot open user stream %s as it is already in waiting state\n",url);
        return NULL;
    }
    
    s->otype = ot==V3_KS_WR_ONLY_CREATE ? V3_KS_WR_ONLY : ot;
    
    palacios_spinlock_unlock_irqrestore(&(s->lock), flags);
    
    return s;
    
}
    
// close stream does not do anything.  Creation of the stream and its cleanup
// are driven by the user side, not the palacios side
// might eventually want to reference count this, though
static void close_stream_user(v3_keyed_stream_t stream)
{
    return;
}

static void preallocate_hint_key_user(v3_keyed_stream_t stream,
				      char *key,
				      uint64_t size)
{
    return;
}




static v3_keyed_stream_key_t open_key_user(v3_keyed_stream_t stream, char *key)
{
    unsigned long flags;
    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    uint64_t   len = strlen(key)+1;
    void *user_key;

    palacios_spinlock_lock_irqsave(&(s->lock), flags);


    if (resize_op(&(s->op),len)) {
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in opening key %s on user keyed stream %s\n",key,s->url);
	return NULL;
    }

    s->op->type = PALACIOS_KSTREAM_OPEN_KEY;
    s->op->buf_len = len;
    strncpy(s->op->buf,key,len);

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return NULL;
    }
    // return with it locked

    user_key=s->op->user_key;

    palacios_spinlock_unlock_irqrestore(&(s->lock),flags);

    return user_key;
}

static void close_key_user(v3_keyed_stream_t stream, v3_keyed_stream_key_t key)
{
    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    uint64_t   len = 0;
    unsigned long flags;
    
    palacios_spinlock_lock_irqsave(&(s->lock), flags);

    if (resize_op(&(s->op),len)) {
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in closing key 0x%p on user keyed stream %s\n",key,s->url);
	return;
    }

    s->op->type = PALACIOS_KSTREAM_CLOSE_KEY;
    s->op->buf_len = len;
    s->op->user_key = key;

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return;
    }
    // return with it locked

    palacios_spinlock_unlock_irqrestore(&(s->lock),flags);

    return;
}



static sint64_t read_key_user(v3_keyed_stream_t stream, v3_keyed_stream_key_t key,
			      void *tag,
			      sint64_t taglen,
			      void *buf, sint64_t rlen)
{

    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    uint64_t   len = taglen ;
    sint64_t   xfer;
    unsigned long flags;

    palacios_spinlock_lock_irqsave(&(s->lock), flags);

    if (s->otype != V3_KS_RD_ONLY) { 
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("attempt to read key from stream that is not in read state on %s\n",s->url);
    }	


    if (resize_op(&(s->op),len)) {
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in reading key 0x%p on user keyed stream %s\n",key,s->url);
	return -1;
    }

    s->op->type = PALACIOS_KSTREAM_READ_KEY;
    s->op->buf_len = len ;
    s->op->xfer = rlen;
    s->op->user_key = key;
    s->op->data_off = taglen;

    memcpy(s->op->buf,tag,taglen);

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return -1;
    }
    // return with it locked


    if (s->op->xfer>0) { 
        // data_off must be zero
	memcpy(buf,s->op->buf,s->op->xfer);
    }

    xfer=s->op->xfer;

    palacios_spinlock_unlock_irqrestore(&(s->lock),flags);

    return xfer;
}


static sint64_t write_key_user(v3_keyed_stream_t stream, v3_keyed_stream_key_t key,
			       void *tag,
			       sint64_t taglen,
			       void *buf, sint64_t wlen)
{

    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    uint64_t   len = taglen + wlen ;
    sint64_t   xfer;
    unsigned long flags;


    palacios_spinlock_lock_irqsave(&(s->lock), flags);

    if (s->otype != V3_KS_WR_ONLY) { 
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("attempt to write key on stream that is not in write state on %s\n",s->url);
    }	

    if (resize_op(&(s->op),len)) {
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in reading key 0x%p on user keyed stream %s\n",key,s->url);
	return -1;
    }

    s->op->type = PALACIOS_KSTREAM_WRITE_KEY;
    s->op->buf_len = len;
    s->op->xfer = wlen;
    s->op->user_key = key;
    s->op->data_off = taglen;

    memcpy(s->op->buf,tag,taglen);
    memcpy(s->op->buf+taglen,buf,wlen);

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	palacios_spinlock_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return -1;
    }
    // return with it locked

    // no data comes back, xfer should be size of data write (not tag)

    xfer=s->op->xfer;

    palacios_spinlock_unlock_irqrestore(&(s->lock),flags);

    return xfer;
}



/****************************************************************************************
 *    Network-based implementation  ("net:")
 *****************************************************************************************/


#define NET_MAX_KEY_LEN 128

struct net_keyed_stream {
    int stype;
    int ot;
    struct net_stream * ns;
};

struct net_stream {
    int stype;
    struct socket *sock;
};


//ignore the arguments given here currently
static struct net_stream * create_net_stream(void) 
{
    struct net_stream * ns = NULL;

    ns = palacios_alloc(sizeof(struct net_stream));
    
    if (!ns) { 
	ERROR("Cannot allocate a net_stream\n");
	return 0;
    }

    memset(ns, 0, sizeof(struct net_stream));

    ns->stype = STREAM_NETWORK;

    return ns;
}

static void close_socket(v3_keyed_stream_t stream)
{
    struct net_keyed_stream *nks = (struct net_keyed_stream *) stream;

    if (nks) { 
	struct net_stream *ns = nks->ns;

	if (ns) {
	    ns->sock->ops->release(ns->sock);
	    palacios_free(ns);
	    ERROR("Close Socket\n");
	}
	
	palacios_free(ns);
    }
}


static void close_stream_net(v3_keyed_stream_t stream)
{
	close_socket(stream);
}

static int connect_to_ip(struct net_stream *ns, int hostip, int port)
{
    struct sockaddr_in client;

    if (ns == NULL) {
	return -1;
    }

    if (sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&(ns->sock))<0) { 
	ERROR("Cannot create accept socket\n");
	return -1;
    }
  	

    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    client.sin_addr.s_addr = hostip;//in_aton(hostip);

    return ns->sock->ops->connect(ns->sock, (struct sockaddr *)&client, sizeof(client), 0);
}

static int send_msg(struct net_stream *ns,  char * buf, int len)
{
    int left=len;

    if (!ns) { 
	ERROR("Send message on null net_stream\n");
	return -1;
    }

    if (!(ns->sock)) { 
	ERROR("Send message on net_stream without socket\n");
	return -1;
    }

    while (left>0) {

	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
  	int err = 0;
	

	msg.msg_flags = MSG_NOSIGNAL;//MSG_DONTWAIT;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	iov.iov_base = (char *)&(buf[len-left]);
	iov.iov_len = (size_t)left;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	err = sock_sendmsg(ns->sock, &msg, (size_t)left);

	set_fs(oldfs);
	
	if (err<0) {
	    ERROR("Send msg error %d\n",err);
	    return err;
	} else {
	    left-=len;
	}
    }

    return len;
}



static int recv_msg(struct net_stream *ns, char * buf, int len)
{

    int left=len;

    if (!ns) { 
	ERROR("Receive message on null net_stream\n");
	return -1;
    }

    if (!(ns->sock)) { 
	ERROR("Receive  message on net_stream without socket\n");
	return -1;
    }
    
    
    while (left>0) {
	
	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
	int err;
	
	msg.msg_flags = 0;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	iov.iov_base = (void *)&(buf[len-left]);
	iov.iov_len = (size_t)left;
	
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	
	err = sock_recvmsg(ns->sock, &msg, (size_t)left, 0);
	
	set_fs(oldfs);
	
	if (err<0) { 
	    return err;
	} else {
	    left -= err;
	}
    }
    return len;
}

static struct net_stream * accept_once(struct net_stream * ns, const int port)
{
    struct socket *accept_sock;
    struct sockaddr_in addr;
    int err;
    
    if (!ns) { 
	ERROR("Accept called on null net_stream\n");
	return 0;
    }
    
    if (sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&accept_sock)<0) { 
	ERROR("Cannot create accept socket\n"); 
	return NULL;
    }
    

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    err = accept_sock->ops->bind(accept_sock, (struct sockaddr *)&addr, sizeof(addr));
    
    if (err<0) {
	ERROR("Bind err: %d\n",err);
	return NULL;
    }

    err = accept_sock->ops->listen(accept_sock,2);
    
    if (err<0) {
	ERROR("Listen err: %d\n",err);
	return NULL;
    }
    
    // Init the socket in the network strream

    if (sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&(ns->sock))<0) { 
	ERROR("Cannot create socket\n");
	return NULL;
    }
    
    
    // Do the actual accept 

    if (accept_sock->ops->accept(accept_sock,ns->sock,0)<0) {
	ERROR("accept failed");
	return NULL;
    }
    
    // close the accept socket
    accept_sock->ops->release(accept_sock);
    palacios_free(accept_sock);

    return ns;
}


static struct v3_keyed_stream_t * open_stream_net(char * url,v3_keyed_stream_open_t ot)
{
    struct net_keyed_stream * nks;
    int url_len;
    int i;
    int delimit[3];
    int k;
    char mode;
    int ip_len;
    int port_len;

    nks = palacios_alloc(sizeof(struct net_keyed_stream)); 

    if (!nks) { 
	ERROR("Could not allocate space in open_stream_net\n");
	return 0;
    }
    
    nks->ot = ot == V3_KS_WR_ONLY_CREATE ? V3_KS_WR_ONLY : ot;

    nks->stype = STREAM_NETWORK; 

    nks->ns = create_net_stream();
    
    if (!(nks->ns)) { 
	ERROR("Could not create network stream\n");
	palacios_free(nks);
	return 0;
    }

    url_len=strlen(url);
    k=0;


    for(i = 0; i < url_len;i++){
	if(url[i] == ':'){
	    delimit[k] = i;
	    k++;	
	}
    }

    mode = url[delimit[0] + 1];
    ip_len = delimit[2] - delimit[1];
    port_len = url_len - delimit[2];


    {
	char ip[ip_len];
	char port[port_len];
	int host_ip;
	int host_port;


	strncpy(ip,url + delimit[1]+1,ip_len-1);
	ip[ip_len-1]='\0';
	
	host_ip = in_aton(ip);
	
	strncpy(port,url+ delimit[2]+1,port_len-1);
	port[port_len-1]='\0';
	
	host_port = simple_strtol(port,NULL,10);

	INFO("ip is %s\n",ip); 
	INFO("host_ip is %x\n", host_ip);
	INFO("port is %s (%d)\n",port,host_port);
	
	if (mode == 'a'){
	    // accept a request
	    INFO("Accepting Connection on INADDR_ANY port:%d\n",host_port);
	    nks->ns = accept_once(nks->ns, host_port);
	} else if (mode == 'c'){
	    // call connect to ip
	    INFO("Connecting to %s:%d\n",ip,host_port);
	    connect_to_ip(nks->ns,host_ip, host_port);
	} else {
	    ERROR("Mode not recognized\n");
	    palacios_free(nks);
	    return NULL;
	}
	
	return (v3_keyed_stream_t)nks;
    }
}

static void preallocate_hint_key_net(v3_keyed_stream_t stream, char *key,uint64_t size)
{
    //do nothing
}

static v3_keyed_stream_key_t open_key_net(v3_keyed_stream_t stream,char *key)
{
   struct net_keyed_stream * nks = (struct net_keyed_stream *)stream;

   // reciever of the key name 
   if (nks->ot==V3_KS_WR_ONLY)
   {
       unsigned short keylen = strlen(key);

       if (keylen>NET_MAX_KEY_LEN || keylen>=32768) { 
           ERROR("Key is too long\n");
           return NULL;
       }

       {
	   // on-stack allocation here demands that we
	   // keep key length low...
	   char msg[keylen+3];
	   int next = 0;
	   
	   // Opening a key for writing sends a notice of the 
	   // key length and the key name on the channel
	   
	   msg[next++]=keylen & 0xFF;
	   msg[next]=(keylen>>8) & 0xFF;
	   // Set flag bit
	   msg[next]=msg[next] | 0x80; // 0x80 is 128 and OR will flip leading bit to 1
	   
	   strncpy(msg+2,key,keylen);  // will also copy trailing zero
	   
	   if (send_msg(nks->ns,msg,keylen+2) != keylen+2) { 
	       ERROR("Unable to open key for writing on net_stream (send key len+name)\n");
	       return NULL;
	   }
       }
   }

   if (nks->ot==V3_KS_RD_ONLY)   {
       char msg_info[2];
       int next = 0;
       int keylen = 0;

       if (recv_msg(nks->ns,msg_info,2) != 2) { 
	   ERROR("Unable to open key for reading on net_stream (recv key len)\n");
	   return NULL;
       }

       next = 0;
       keylen = 0;

       keylen |= msg_info[next++];

       if ((msg_info[next] & 0x80) != 0x80)  {
	   ERROR("Flag bit not set on receive of key length\n");
           return NULL;
       } else {
           msg_info[next] &= 0x7F; // flip the msb back to zero (clear flag)
       }
       
       keylen |= msg_info[next]<<8;

       if (keylen > NET_MAX_KEY_LEN) { 
	   ERROR("Received key length is too big\n");
	   return NULL;
       }
       
       {
	   
	   char msg[keylen+1];
	   
	   if (recv_msg(nks->ns,msg,keylen) != keylen) { 
	       ERROR("Unable to receive key\n");
	       return NULL;
	   }
	   msg[keylen]=0;
	   
	   if (strncmp(key,msg,keylen)!=0) {
	       ERROR("Key mismatch in open_key_net - expect %s but got %s\n",key,msg);
	       return NULL;
	   }
       }
   }
   
   return (v3_keyed_stream_key_t)key;
}

static void close_key_net(v3_keyed_stream_t stream, v3_keyed_stream_key_t input_key)
{
    char * key = (char*)input_key;
    struct net_keyed_stream * nks = (struct net_keyed_stream *)stream;

    
    if (nks->ot==V3_KS_WR_ONLY) {
	unsigned short keylen = strlen(key);

	if (keylen > NET_MAX_KEY_LEN || keylen>=32768) {
	    ERROR("Key length too long in close_key_net\n");
	    return;
	}

	{
	    char msg[keylen+3];
	    int next = 0;
	    
	    msg[next++]=keylen & 0xFF;
	    msg[next]=(keylen>>8) & 0xFF;
	    // flag
	    msg[next]=msg[next] | 0x80; // 0x80 is 128 and OR will filp leading bit to 1
	    strncpy(msg+2,key,keylen); // will copy the zero
	    msg[keylen+2]=0;
	    if (send_msg(nks->ns,msg,keylen+2)!=keylen+2) { 
		ERROR("Cannot send key on close_key_net\n");
		return;
	    }
	}
    }
    
    if (nks->ot==V3_KS_RD_ONLY)   {
	char msg_info[2];
	int next;
	int keylen;
	
	if (recv_msg(nks->ns,msg_info,2) != 2) { 
	    ERROR("Cannot recv key length on close_key_net\n");
	    return;
	}
	
	next = 0;
	keylen = 0;
	
	keylen |= msg_info[next++];
	
	if ((msg_info[next] & 0x80) != 0x80) {
	    ERROR("Missing flag in close_key_net receive\n");
	    return;
	} 
	
	msg_info[next] &= 0x7F; // flip the msb back to zero
	
	keylen |= msg_info[next]<<8;
	
	{
	    char msg[keylen+1];
	    
	    if (recv_msg(nks->ns,msg,keylen)!=keylen) { 
		ERROR("Did not receive all of key in close_key_net receive\n");
		return;
	    }
	    
	    msg[keylen]=0;
	    
	    if (strncmp(key,msg,keylen)!=0)  {
		ERROR("Key mismatch in close_key_net - expect %s but got %s\n",key,msg);
		return;
	    }
	}
    }
}

static sint64_t write_key_net(v3_keyed_stream_t stream, v3_keyed_stream_key_t key, 
			      void *tag,
			      sint64_t taglen,
			      void *buf, sint64_t len) 
{
    struct net_keyed_stream * nks = (struct net_keyed_stream *)stream;

    if (!buf) { 
	ERROR("Buf is NULL in write_key_net\n");
	return -1;
    }

    if (!tag) { 
	ERROR("Tag is NULL in write_key_net\n");
	return -1;
    }

    if (len<0) {
	ERROR("len is negative in write_key_net\n");
	return -1;
    }

    if (taglen<0) {
	ERROR("taglen is negative in write_key_net\n");
	return -1;
    }
    
    if (!key){
       ERROR("write_key: key is NULL in write_key_net\n");
       return -1;
    }
    
    
    if (!nks)  {
	ERROR("nks is NULL in write_key_net\n");
	return -1;
    }
    
    if (nks->ot==V3_KS_WR_ONLY) {
        if (send_msg(nks->ns,(char*)(&BOUNDARY_TAG),sizeof(BOUNDARY_TAG))!=sizeof(BOUNDARY_TAG)) { 
   	    ERROR("Could not send boundary tag in write_key_net\n");
	    return -1;
	} 
	if (send_msg(nks->ns,(char*)(&taglen),sizeof(taglen))!=sizeof(taglen)) { 
	    ERROR("Could not send tag length in write_key_net\n");
	    return -1;
	} 
	if (send_msg(nks->ns,tag,taglen)!=len) { 
	    ERROR("Could not send tag in write_key_net\n");
	    return -1;
	}
	if (send_msg(nks->ns,(char*)(&len),sizeof(len))!=sizeof(len)) { 
	    ERROR("Could not send data length in write_key_net\n");
	    return -1;
	} 
	if (send_msg(nks->ns,buf,len)!=len) { 
	    ERROR("Could not send data in write_key_net\n");
	    return -1;
	}
    }  else {
	ERROR("Permission not correct in write_key_net\n");
	return -1;
    }
    
    return len;
}


static sint64_t read_key_net(v3_keyed_stream_t stream, v3_keyed_stream_key_t key,
			     void *tag,
			     sint64_t taglen,
			     void *buf, sint64_t len)
{
    struct net_keyed_stream * nks = (struct net_keyed_stream *)stream;
    void *temptag;

    if (!buf) {
	ERROR("Buf is NULL in read_key_net\n");
	return -1;
    }

    if (!tag) {
	ERROR("Tag is NULL in read_key_net\n");
	return -1;
    }
    
    if(len<0) {
	ERROR("len is negative in read_key_net\n");
	return -1;
    }

    if(taglen<0) {
	ERROR("taglen is negative in read_key_net\n");
	return -1;
    }
    
    if (!key) {
	ERROR("read_key: key is NULL in read_key_net\n");
	return -1;
    }


    if (nks->ot==V3_KS_RD_ONLY) {
        
        sint64_t slen;
	uint32_t tempbt;
	
	if (recv_msg(nks->ns,(char*)(&tempbt),sizeof(tempbt))!=sizeof(tempbt)) { 
	    ERROR("Cannot receive boundary tag in read_key_net\n");
	    return -1;
	}

	if (tempbt!=BOUNDARY_TAG) { 
	  ERROR("Invalid boundary tag (received 0x%x\n",tempbt);
	  return -1;
	}
           
        temptag=palacios_alloc(taglen);
        if (!temptag) {
	  ERROR("failed to allocate temptag\n");
	  return -1;
        }

	if (recv_msg(nks->ns,(char*)(&slen),sizeof(slen))!=sizeof(slen)) { 
	    ERROR("Cannot receive tag len in read_key_net\n");
	    palacios_free(temptag);
	    return -1;
	}

	if (slen!=taglen) {
	    ERROR("Tag len expected does not matched tag len decoded in read_key_net\n");
	    palacios_free(temptag);
	    return -1;
	}

	if (recv_msg(nks->ns,temptag,taglen)!=taglen) { 
	    ERROR("Cannot recieve tag in read_key_net\n");
	    palacios_free(temptag);
	    return -1;
	}

	if (memcmp(temptag,tag,taglen)) { 
	  ERROR("Tag mismatch\n");
	  palacios_free(temptag);
	  return -1;
	}

	if (recv_msg(nks->ns,(char*)(&slen),sizeof(slen))!=sizeof(slen)) { 
	    ERROR("Cannot receive data len in read_key_net\n");
	    palacios_free(temptag);
	    return -1;
	}

	if (slen!=len) {
	    ERROR("Data len expected does not matched data len decoded in read_key_net\n");
	    palacios_free(temptag);
	    return -1;
	}

	if (recv_msg(nks->ns,buf,len)!=len) { 
	    ERROR("Cannot recieve data in read_key_net\n");
	    palacios_free(temptag);
	    return -1;
	}

	palacios_free(temptag);
	
    } else {
        ERROR("Permissions incorrect for the stream in read_key_net\n");
        return -1;
    }

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
    } else if (!strncasecmp(url,"user:",5)) { 
	return open_stream_user(url,ot);
    } else if (!strncasecmp(url,"net:",4)){
	return open_stream_net(url,ot);
    } else if (!strncasecmp(url,"textfile:",9)) { 
        return open_stream_textfile(url,ot);
    } else {
	ERROR("unsupported type in attempt to open keyed stream \"%s\"\n",url);
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
	case STREAM_TEXTFILE:
	    return close_stream_textfile(stream);
	    break;
	case STREAM_USER:
	    return close_stream_user(stream);
	    break;
	case STREAM_NETWORK:
	    return close_stream_net(stream);
	    break;
	default:
	    ERROR("unknown stream type %d in close\n",gks->stype);
	    break;
    }
}

static void preallocate_hint_key(v3_keyed_stream_t stream,
				 char *key,
				 uint64_t size)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    preallocate_hint_key_mem(stream,key,size);
	    break;
	case STREAM_FILE:
	    preallocate_hint_key_file(stream,key,size);
	    break;
	case STREAM_TEXTFILE:
	    preallocate_hint_key_textfile(stream,key,size);
	    break;
	case STREAM_USER:
	    return preallocate_hint_key_user(stream,key,size);
	    break;
	case STREAM_NETWORK:
	    return preallocate_hint_key_net(stream,key,size);
	    break;
	default:
	    ERROR("unknown stream type %d in preallocate_hint_key\n",gks->stype);
	    break;
    }
    return;
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
	case STREAM_TEXTFILE:
	    return open_key_textfile(stream,key);
	    break;
	case STREAM_USER:
	    return open_key_user(stream,key);
	    break;
        case STREAM_NETWORK:
	    return open_key_net(stream, key);
	    break;
	default:
	    ERROR("unknown stream type %d in open_key\n",gks->stype);
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
	case STREAM_TEXTFILE:
	    return close_key_textfile(stream,key);
	    break;
	case STREAM_USER:
	    return close_key_user(stream,key);
	    break;
	 case STREAM_NETWORK:
            return close_key_net(stream, key);
            break;	
	default:
	    ERROR("unknown stream type %d in close_key\n",gks->stype);
	    break;
    }
    // nothing to do
    return;
}

static sint64_t write_key(v3_keyed_stream_t stream, 
			  v3_keyed_stream_key_t key,
			  void *tag,
			  sint64_t taglen,
			  void *buf,
			  sint64_t len)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	    return write_key_mem(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_FILE:
	    return write_key_file(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_TEXTFILE:
	    return write_key_textfile(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_USER:
	    return write_key_user(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_NETWORK:
	    return write_key_net(stream,key,tag,taglen,buf,len);
	    break;
	default:
	    ERROR("unknown stream type %d in write_key\n",gks->stype);
	    return -1;
	    break;
    }
    return -1;
}


static sint64_t read_key(v3_keyed_stream_t stream, 
			 v3_keyed_stream_key_t key,
			 void *tag,
			 sint64_t taglen,
			 void *buf,
			 sint64_t len)
{
    struct generic_keyed_stream *gks = (struct generic_keyed_stream *) stream;
    switch (gks->stype){ 
	case STREAM_MEM:
	  return read_key_mem(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_FILE:
	    return read_key_file(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_TEXTFILE:
	    return read_key_textfile(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_USER:
	    return read_key_user(stream,key,tag,taglen,buf,len);
	    break;
	case STREAM_NETWORK:
	    return read_key_net(stream,key,tag,taglen,buf,len);
	    break;
	default:
	    ERROR("unknown stream type %d in read_key\n",gks->stype);
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
    .preallocate_hint_key = preallocate_hint_key,
    .open_key = open_key,
    .close_key = close_key,
    .read_key = read_key,
    .write_key = write_key
};


static int init_keyed_streams( void )
{
    mem_streams = palacios_create_htable(DEF_NUM_STREAMS,hash_func,hash_comp);

    if (!mem_streams) { 
	ERROR("failed to allocated stream pool for in-memory streams\n");
	return -1;
    }

    user_streams = palacios_alloc(sizeof(struct user_keyed_streams));

    if (!user_streams) { 
	ERROR("failed to allocated list for user streams\n");
	return -1;
    }

    INIT_LIST_HEAD(&(user_streams->streams));
    
    palacios_spinlock_init(&(user_streams->lock));

    V3_Init_Keyed_Streams(&hooks);

    return 0;

}

static int deinit_keyed_streams( void )
{
    palacios_free_htable(mem_streams,1,1);

    palacios_spinlock_deinit(&(user_streams->lock));

    palacios_free(user_streams);

    WARNING("Deinit of Palacios Keyed Streams likely leaked memory\n");

    return 0;
}


static int guest_init_keyed_streams(struct v3_guest * guest, void ** vm_data ) 
{
    
    add_guest_ctrl(guest, V3_VM_KSTREAM_USER_CONNECT, keyed_stream_connect_user, 0);
    
    return 0;
}


static int guest_deinit_keyed_streams(struct v3_guest * guest, void * vm_data)
{

    return 0;
}




static struct linux_ext key_stream_ext = {
    .name = "KEYED_STREAM_INTERFACE",
    .init = init_keyed_streams,
    .deinit = deinit_keyed_streams,
    .guest_init = guest_init_keyed_streams,
    .guest_deinit = guest_deinit_keyed_streams, 
};


register_extension(&key_stream_ext);
