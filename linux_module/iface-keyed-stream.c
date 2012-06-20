/*
 * Palacios keyed stream interface
 *
 * Plus implementations for mem, file, and user space implementations
 *
 * (c) Peter Dinda, 2011 (interface, mem + file implementations + recooked user impl)
 * (c) Clint Sbisa, 2011 (initial user space implementation on which this is based)
 * (c) Diana Palsetia & Steve Rangel, 2012 (network based implementation)	
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/vmalloc.h>
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
  each stream.   

  "file:"  Streams are stored in files.  Each high-level
  open corresponds to a directory, while a key corresponds to
  a distinct file in that directory. 

  "net:"  Streams are carried over the network.  Each
   high level open corresponds to a TCP connection, while
   each key corresponds to a context on the stream.
      "net:a:<ip>:<port>" => Bind to <ip>:<port> and accept a connection
      "net:c:<ip>:<port>" => Connect to <ip>:<port>

  "user:" Stream requests are bounced to user space to be 
   handled there.  A rendezvous approach similar to the host 
   device userland support is used
   
*/

#define STREAM_GENERIC 0
#define STREAM_MEM     1
#define STREAM_FILE    2
#define STREAM_USER    3
#define STREAM_NETWORK 4

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

static struct mem_stream *create_mem_stream_internal(uint64_t size)
{
    struct mem_stream *m = kmalloc(sizeof(struct mem_stream),GFP_KERNEL);

    if (!m) {
	return 0;
    }


    m->data = vmalloc(size);
    
    if (!m->data) { 
	kfree(m);
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
	    kfree(m->data);
	}
	m->data=0;
	kfree(m);
    }
}
    
static int expand_mem_stream(struct mem_stream *m, uint32_t new_size)
{
    void *data = vmalloc(new_size);
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

		mykey = kmalloc(strlen(url+4)+1,GFP_KERNEL);

		if (!mykey) { 
		    ERROR("cannot allocate space for new in-memory keyed stream %s\n",url);
		    return 0;
		}

		strcpy(mykey,url+4);
		
		mks = (struct mem_keyed_stream *) kmalloc(sizeof(struct mem_keyed_stream),GFP_KERNEL);

		if (!mks) { 
		    kfree(mykey);
		    ERROR("cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}
	    
		mks->ht = (void*) palacios_create_htable(DEF_NUM_KEYS,hash_func,hash_comp);
		if (!mks->ht) { 
		    kfree(mks);
		    kfree(mykey);
		    ERROR("cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}

		
		if (!palacios_htable_insert(mem_streams,(addr_t)(mykey),(addr_t)mks)) { 
		    palacios_free_htable(mks->ht,1,1);
		    kfree(mks);
		    kfree(mykey);
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
	char *mykey = kmalloc(strlen(key)+1,GFP_KERNEL);

	if (!mykey) { 
	    ERROR("cannot allocate copy of key for key %s\n",key);
	    return 0;
	}

	strcpy(mykey,key);

	m = create_mem_stream();
	
	if (!m) { 
	    kfree(mykey);
	    ERROR("cannot allocate mem keyed stream for key %s\n",key);
	    return 0;
	}

	if (!palacios_htable_insert(s,(addr_t)mykey,(addr_t)m)) {
	    destroy_mem_stream(m);
	    kfree(mykey);
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
	
	mykey=kmalloc(strlen(key)+1,GFP_KERNEL);
	
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
	ERROR("failed to write all data for key\n");
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

    fks = kmalloc(sizeof(struct file_keyed_stream),GFP_KERNEL);
    
    if (!fks) { 
	ERROR("cannot allocate space for file stream\n");
	return 0;
    }

    fks->path = (char*)kmalloc(strlen(url+5)+1,GFP_KERNEL);
    
    if (!(fks->path)) { 
	ERROR("cannot allocate space for file stream\n");
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
    path = (char *) kmalloc(strlen(fks->path)+strlen(key)+2,GFP_KERNEL);
    if (!path) {				
	ERROR("cannot allocate file keyed stream for key %s\n",key);
	return 0;
    }
    strcpy(path,fks->path);
    strcat(path,"/");
    strcat(path,key);
    
    fs = (struct file_stream *) kmalloc(sizeof(struct file_stream *),GFP_KERNEL);
    
    if (!fs) { 
	ERROR("cannot allocate file keyed stream for key %s\n",key);
	kfree(path);
	return 0;
    }

    fs->stype=STREAM_FILE;

    fs->f = filp_open(path,O_RDWR|O_CREAT,0600);
    
    if (IS_ERR(fs->f)) {
	ERROR("cannot open relevent file \"%s\" for stream \"file:%s\" and key \"%s\"\n",path,fks->path,key);
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
	new = kmalloc(sizeof(struct palacios_user_keyed_stream_op)+buf_len,GFP_ATOMIC);
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
	    kfree(old);
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
    spin_unlock_irqrestore(&(s->lock), *flags);

    // wake up anyone waiting on it
    wake_up_interruptible(&(s->user_wait_queue));

    // wait for someone to give us a response
    while (wait_event_interruptible(s->host_wait_queue, (s->waiting == 0)) != 0) {}

    // reacquire the lock for our called
    spin_lock_irqsave(&(s->lock), *flags);

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
    spin_unlock_irqrestore(&(s->lock), *flags);

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
    
    spin_lock_irqsave(&(s->lock), flags);

    if (s->waiting) {
	spin_unlock_irqrestore(&(s->lock), flags);
	return POLLIN | POLLRDNORM;
    }

    poll_wait(filp, &(s->user_wait_queue), wait);
    
    spin_unlock_irqrestore(&(s->lock), flags);

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
	    
	    spin_lock_irqsave(&(s->lock), flags);
	    
	    if (!(s->waiting)) {
		spin_unlock_irqrestore(&(s->lock), flags);
		return 0;
	    }

	    size =  sizeof(struct palacios_user_keyed_stream_op) + s->op->buf_len;
	    
	    if (copy_to_user((void * __user) argp, &size, sizeof(uint64_t))) {
		spin_unlock_irqrestore(&(s->lock), flags);
		ERROR("palacios user key size request failed to copy data\n");
		return -EFAULT;
	    }
	    
	    spin_unlock_irqrestore(&(s->lock), flags);
	    
	    return 1;
	    
	    break;

	case V3_KSTREAM_REQUEST_PULL_IOCTL: 
		
	    // pull the request
	    
	    spin_lock_irqsave(&(s->lock), flags);

	    if (!(s->waiting)) {
		spin_unlock_irqrestore(&(s->lock), flags);
		ERROR("palacios user key pull request when not waiting\n");
		return 0;
	    }

	    size =  sizeof(struct palacios_user_keyed_stream_op) + s->op->buf_len;


	    if (copy_to_user((void __user *) argp, s->op, size)) {
		spin_unlock_irqrestore(&(s->lock), flags);
		ERROR("palacios user key pull request failed to copy data\n");
		return -EFAULT;
	    }

	    spin_unlock_irqrestore(&(s->lock), flags);
	    
	    return 1;
	    
	 
	    break;

    case V3_KSTREAM_RESPONSE_PUSH_IOCTL:

        // push the response

        spin_lock_irqsave(&(s->lock), flags);

        if (!(s->waiting)) {
            spin_unlock_irqrestore(&(s->lock), flags);
	    ERROR("palacios user key push response when not waiting\n");
            return 0;
        }
	
        if (copy_from_user(&size, (void __user *) argp, sizeof(uint64_t))) {
	    ERROR("palacios user key push response failed to copy size\n");
            spin_unlock_irqrestore(&(s->lock), flags);
            return -EFAULT;
        }

	if (resize_op(&(s->op),size-sizeof(struct palacios_user_keyed_stream_op))) {
	    ERROR("unable to resize op in user key push response\n");
            spin_unlock_irqrestore(&(s->lock), flags);
	    return -EFAULT;
	}

        if (copy_from_user(s->op, (void __user *) argp, size)) {
            spin_unlock_irqrestore(&(s->lock), flags);
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

    spin_lock_irqsave(&(user_streams->lock),f1);
    spin_lock_irqsave(&(s->lock), f2);

    list_del(&(s->node));

    spin_unlock_irqrestore(&(s->lock), f2);
    spin_unlock_irqrestore(&(user_streams->lock), f1);
    
    kfree(s->url);
    kfree(s);

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

    url = kmalloc(len,GFP_KERNEL);
    
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
    spin_lock_irqsave(&(user_streams->lock), flags);
    list_for_each_entry(s, &(user_streams->streams), node) {
        if (!strncasecmp(url, s->url, len)) {
            ERROR("user keyed stream connection with url \"%s\" already exists\n", url);
	    kfree(url);
            return -1;
        }
    }
    spin_unlock_irqrestore(&(user_streams->lock), flags);
    
    // Create connection
    s = kmalloc(sizeof(struct user_keyed_stream), GFP_KERNEL);
    
    if (!s) {
	ERROR("cannot allocate new user keyed stream for %s\n",url);
	kfree(url);
        return -1;
    }
    
    
    // Get file descriptor
    fd = anon_inode_getfd("v3-kstream", &user_keyed_stream_fops, s, 0);

    if (fd < 0) {
	ERROR("cannot allocate file descriptor for new user keyed stream for %s\n",url);
        kfree(s);
	kfree(url);
        return -1;
    }
    
    memset(s, 0, sizeof(struct user_keyed_stream));
    
    s->stype=STREAM_USER;
    s->url=url;
    
    init_waitqueue_head(&(s->user_wait_queue));
    init_waitqueue_head(&(s->host_wait_queue));
    
    // Insert connection into list
    spin_lock_irqsave(&(user_streams->lock), flags);
    list_add(&(s->node), &(user_streams->streams));
    spin_unlock_irqrestore(&(user_streams->lock), flags);
    
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
    
    spin_lock_irqsave(&(user_streams->lock), flags);
    list_for_each_entry(s, &(user_streams->streams), node) {
        if (!strcasecmp(url, s->url)) {
            spin_unlock_irqrestore(&(user_streams->lock), flags);
            return s;
        }
    }
    
    spin_unlock_irqrestore(&(user_streams->lock), flags);
    
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

    spin_lock_irqsave(&(s->lock), flags);

    if (s->waiting) {
        spin_unlock_irqrestore(&(s->lock), flags);
	ERROR("cannot open user stream %s as it is already in waiting state\n",url);
        return NULL;
    }
    
    s->otype = ot==V3_KS_WR_ONLY_CREATE ? V3_KS_WR_ONLY : ot;
    
    spin_unlock_irqrestore(&(s->lock), flags);
    
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

    spin_lock_irqsave(&(s->lock), flags);


    if (resize_op(&(s->op),len)) {
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in opening key %s on user keyed stream %s\n",key,s->url);
	return NULL;
    }

    s->op->type = PALACIOS_KSTREAM_OPEN_KEY;
    s->op->buf_len = len;
    strncpy(s->op->buf,key,len);

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return NULL;
    }
    // return with it locked

    user_key=s->op->user_key;

    spin_unlock_irqrestore(&(s->lock),flags);

    return user_key;
}

static void close_key_user(v3_keyed_stream_t stream, v3_keyed_stream_key_t key)
{
    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    uint64_t   len = 0;
    unsigned long flags;
    
    spin_lock_irqsave(&(s->lock), flags);

    if (resize_op(&(s->op),len)) {
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in closing key 0x%p on user keyed stream %s\n",key,s->url);
	return;
    }

    s->op->type = PALACIOS_KSTREAM_CLOSE_KEY;
    s->op->buf_len = len;
    s->op->user_key = key;

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return;
    }
    // return with it locked

    spin_unlock_irqrestore(&(s->lock),flags);

    return;
}



static sint64_t read_key_user(v3_keyed_stream_t stream, v3_keyed_stream_key_t key,
			      void *buf, sint64_t rlen)
{

    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    uint64_t   len = 0 ;
    sint64_t   xfer;
    unsigned long flags;

    spin_lock_irqsave(&(s->lock), flags);

    if (s->otype != V3_KS_RD_ONLY) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("attempt to read key from stream that is not in read state on %s\n",s->url);
    }	

    if (resize_op(&(s->op),len)) {
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in reading key 0x%p on user keyed stream %s\n",key,s->url);
	return -1;
    }

    s->op->type = PALACIOS_KSTREAM_READ_KEY;
    s->op->buf_len = len ;
    s->op->xfer = rlen;
    s->op->user_key = key;

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return -1;
    }
    // return with it locked


    if (s->op->xfer>0) { 
	memcpy(buf,s->op->buf,s->op->xfer);
    }

    xfer=s->op->xfer;

    spin_unlock_irqrestore(&(s->lock),flags);

    return xfer;
}


static sint64_t write_key_user(v3_keyed_stream_t stream, v3_keyed_stream_key_t key,
			       void *buf, sint64_t wlen)
{

    struct user_keyed_stream *s = (struct user_keyed_stream *) stream;
    struct palacios_user_keyed_stream_op *op;
    uint64_t   len = wlen ;
    sint64_t   xfer;
    unsigned long flags;
    
    spin_lock_irqsave(&(s->lock), flags);

    if (s->otype != V3_KS_WR_ONLY) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("attempt to write key on stream that is not in write state on %s\n",s->url);
    }	

    if (resize_op(&(s->op),len)) {
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("cannot resize op in reading key 0x%p on user keyed stream %s\n",key,s->url);
	return -1;
    }

    op = s->op;

    s->op->type = PALACIOS_KSTREAM_WRITE_KEY;
    s->op->buf_len = len;
    s->op->xfer = wlen;
    s->op->user_key = key;

    memcpy(s->op->buf,buf,wlen);

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	ERROR("request/response handling failed\n");
	return -1;
    }
    // return with it locked

    xfer=s->op->xfer;

    spin_unlock_irqrestore(&(s->lock),flags);

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

    ns = kmalloc(sizeof(struct net_stream), GFP_KERNEL);
    
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
	    kfree(ns);
	    ERROR("Close Socket\n");
	}
	
	kfree(ns);
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
    kfree(accept_sock);

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

    nks = kmalloc(sizeof(struct net_keyed_stream),GFP_KERNEL); 

    if (!nks) { 
	ERROR("Could not allocate space in open_stream_net\n");
	return 0;
    }
    
    nks->ot = ot == V3_KS_WR_ONLY_CREATE ? V3_KS_WR_ONLY : ot;

    nks->stype = STREAM_NETWORK; 

    nks->ns = create_net_stream();
    
    if (!(nks->ns)) { 
	ERROR("Could not create network stream\n");
	kfree(nks);
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
	    kfree(nks);
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

static sint64_t write_key_net(v3_keyed_stream_t stream, v3_keyed_stream_key_t key, void *buf, sint64_t len) 
{
    struct net_keyed_stream * nks = (struct net_keyed_stream *)stream;

    if (!buf) { 
	ERROR("Buf is NULL in write_key_net\n");
	return -1;
    }

    if (len<0) {
	ERROR("len is negative in write_key_net\n");
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


static sint64_t read_key_net(v3_keyed_stream_t stream, v3_keyed_stream_key_t key, void *buf, sint64_t len)
{
    struct net_keyed_stream * nks = (struct net_keyed_stream *)stream;

    if (!buf) {
	ERROR("Buf is NULL in read_key_net\n");
	return -1;
    }
    
    if(len<0) {
	ERROR("len is negative in read_key_net\n");
	return -1;
    }
    
    if (!key) {
	ERROR("read_key: key is NULL in read_key_net\n");
	return -1;
    }


    if (nks->ot==V3_KS_RD_ONLY) {

	sint64_t slen;
           
	if (recv_msg(nks->ns,(char*)(&slen),sizeof(slen))!=sizeof(slen)) { 
	    ERROR("Cannot receive data len in read_key_net\n");
	    return -1;
	}

	if (slen!=len) {
	    ERROR("Data len expected does not matched data len decoded in read_key_net\n");
	    return -1;
	}

	if (recv_msg(nks->ns,buf,len)!=len) { 
	    ERROR("Cannot recieve data in read_key_net\n");
	    return -1;
	}
	
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
    } else if(!strncasecmp(url,"net:",4)){
	return open_stream_net(url,ot);
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
	case STREAM_USER:
	    return write_key_user(stream,key,buf,len);
	    break;
	case STREAM_NETWORK:
	    return write_key_net(stream,key,buf,len);
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
	case STREAM_USER:
	    return read_key_user(stream,key,buf,len);
	    break;
	case STREAM_NETWORK:
	    return read_key_net(stream,key,buf,len);
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

    user_streams = kmalloc(sizeof(struct user_keyed_streams),GFP_KERNEL);

    if (!user_streams) { 
	ERROR("failed to allocated list for user streams\n");
	return -1;
    }

    INIT_LIST_HEAD(&(user_streams->streams));
    
    spin_lock_init(&(user_streams->lock));

    V3_Init_Keyed_Streams(&hooks);

    return 0;

}

static int deinit_keyed_streams( void )
{
    palacios_free_htable(mem_streams,1,1);

    kfree(user_streams);

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
