/*
 * Palacios keyed stream interface
 *
 * Plus implementations for mem, file, and user space implementations
 *
 * (c) Peter Dinda, 2011 (interface, mem + file implementations + recooked user impl)
 * (c) Clint Sbisa, 2011 (initial user space implementation on which this is based)
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

/*
  This is an implementation of the Palacios keyed stream interface
  that supports three flavors of streams:

  "mem:"   Streams are stored in a hash table
  The values for this hash table are hash tables associated with 
  each stream.   

  "file:"  Streams are stored in files.  Each high-level
  open corresponds to a directory, while  key corresponds to
  a distinct file in that directory. 

  "user:" Stream requests are bounced to user space to be 
   handled there.  A rendezvous approach similar to the host 
   device userland support is used
   
*/

#define STREAM_GENERIC 0
#define STREAM_MEM     1
#define STREAM_FILE    2
#define STREAM_USER    3

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
	printk("palacios: illegitimate attempt to open memory stream \"%s\"\n",url);
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
		    printk("palacios: cannot allocate space for new in-memory keyed stream %s\n",url);
		    return 0;
		}

		strcpy(mykey,url+4);
		
		mks = (struct mem_keyed_stream *) kmalloc(sizeof(struct mem_keyed_stream),GFP_KERNEL);

		if (!mks) { 
		    kfree(mykey);
		    printk("palacios: cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}
	    
		mks->ht = (void*) palacios_create_htable(DEF_NUM_KEYS,hash_func,hash_comp);
		if (!mks->ht) { 
		    kfree(mks);
		    kfree(mykey);
		    printk("palacios: cannot allocate in-memory keyed stream %s\n",url);
		    return 0;
		}

		
		if (!palacios_htable_insert(mem_streams,(addr_t)(mykey),(addr_t)mks)) { 
		    palacios_free_htable(mks->ht,1,1);
		    kfree(mks);
		    kfree(mykey);
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
	char *mykey = kmalloc(strlen(key)+1,GFP_KERNEL);

	if (!mykey) { 
	    printk("palacios: cannot allocate copy of key for key %s\n",key);
	    return 0;
	}

	strcpy(mykey,key);

	m = create_mem_stream();
	
	if (!m) { 
	    kfree(mykey);
	    printk("palacios: cannot allocate mem keyed stream for key %s\n",key);
	    return 0;
	}

	if (!palacios_htable_insert(s,(addr_t)mykey,(addr_t)m)) {
	    destroy_mem_stream(m);
	    kfree(mykey);
	    printk("palacios: cannot insert mem keyed stream for key %s\n",key);
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
	    printk("palacios: cannot allocate key spce for preallocte for key %s\n",key);
	    return;
	}
	
	strcpy(mykey,key);
       
	m = create_mem_stream_internal(size);
	
	if (!m) { 
	    printk("palacios: cannot preallocate mem keyed stream for key %s\n",key);
	    return;
	}

	if (!palacios_htable_insert(s,(addr_t)mykey,(addr_t)m)) {
	    printk("palacios: cannot insert preallocated mem keyed stream for key %s\n",key);
	    destroy_mem_stream(m);
	    return;
	}
    } else {
	if (m->data_max < size) { 
	    if (expand_mem_stream(m,size)) { 
		printk("palacios: cannot expand key for preallocation for key %s\n",key);
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
	printk("palacios: user keyed stream request attempted while one is already in progress on %s\n",s->url);
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
	printk("palacios: user keyed stream response while no request is in progress on %s\n",s->url);
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

static int keyed_stream_ioctl_user(struct inode *inode, struct file *filp, unsigned int ioctl, unsigned long arg)
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
		printk("palacios: palacios user key size request failed to copy data\n");
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
		printk("palacios: palacios user key pull request when not waiting\n");
		return 0;
	    }

	    size =  sizeof(struct palacios_user_keyed_stream_op) + s->op->buf_len;


	    if (copy_to_user((void __user *) argp, s->op, size)) {
		spin_unlock_irqrestore(&(s->lock), flags);
		printk("palacios: palacios user key pull request failed to copy data\n");
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
	    printk("palacios: palacios user key push response when not waiting\n");
            return 0;
        }
	
        if (copy_from_user(&size, (void __user *) argp, sizeof(uint64_t))) {
	    printk("palacios: palacios user key push response failed to copy size\n");
            spin_unlock_irqrestore(&(s->lock), flags);
            return -EFAULT;
        }

	if (resize_op(&(s->op),size-sizeof(struct palacios_user_keyed_stream_op))) {
	    printk("palacios: unable to resize op in user key push response\n");
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
	printk("palacios: unknown ioctl in user keyed stream\n");

        return -EFAULT;

	break;
	
    }
}

static long keyed_stream_compat_ioctl_user(struct file * filp, unsigned int ioctl, unsigned long arg)
{
	return keyed_stream_ioctl_user(NULL, filp, ioctl, arg);
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
#ifdef HAVE_COMPAT_IOCTL
    .compat_ioctl = keyed_stream_compat_ioctl_user,
#else
    .ioctl = keyed_stream_ioctl_user,
#endif
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
	printk("palacios: no user space keyed streams!\n");
	return -1;
    }

    // get the url
    if (copy_from_user(&len,(void __user *)arg,sizeof(len))) { 
	printk("palacios: cannot copy url len from user\n");
	return -1;
    }

    url = kmalloc(len,GFP_KERNEL);
    
    if (!url) { 
	printk("palacios: cannot allocate url for user keyed stream\n");
	return -1;
    }

    if (copy_from_user(url,((void __user *)arg)+sizeof(len),len)) {
	printk("palacios: cannot copy url from user\n");
	return -1;
    }
    url[len-1]=0;
	
    
    // Check for duplicate handler
    spin_lock_irqsave(&(user_streams->lock), flags);
    list_for_each_entry(s, &(user_streams->streams), node) {
        if (!strncasecmp(url, s->url, len)) {
            printk("palacios: user keyed stream connection with url \"%s\" already exists\n", url);
	    kfree(url);
            return -1;
        }
    }
    spin_unlock_irqrestore(&(user_streams->lock), flags);
    
    // Create connection
    s = kmalloc(sizeof(struct user_keyed_stream), GFP_KERNEL);
    
    if (!s) {
	printk("palacios: cannot allocate new user keyed stream for %s\n",url);
	kfree(url);
        return -1;
    }
    
    
    // Get file descriptor
    fd = anon_inode_getfd("v3-kstream", &user_keyed_stream_fops, s, 0);

    if (fd < 0) {
	printk("palacios: cannot allocate file descriptor for new user keyed stream for %s\n",url);
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
	printk("palacios: no user space keyed streams available\n");
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
	printk("palacios: cannot open user stream %s as it does not exist yet\n",url);
        return NULL;
    }

    spin_lock_irqsave(&(s->lock), flags);

    if (s->waiting) {
        spin_unlock_irqrestore(&(s->lock), flags);
	printk("palacios: cannot open user stream %s as it is already in waiting state\n",url);
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
	printk("palacios: cannot resize op in opening key %s on user keyed stream %s\n",key,s->url);
	return NULL;
    }

    s->op->type = PALACIOS_KSTREAM_OPEN_KEY;
    s->op->buf_len = len;
    strncpy(s->op->buf,key,len);

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	printk("palacios: request/response handling failed\n");
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
	printk("palacios: cannot resize op in closing key 0x%p on user keyed stream %s\n",key,s->url);
	return;
    }

    s->op->type = PALACIOS_KSTREAM_CLOSE_KEY;
    s->op->buf_len = len;
    s->op->user_key = key;

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	printk("palacios: request/response handling failed\n");
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
	printk("palacios: attempt to read key from stream that is not in read state on %s\n",s->url);
    }	

    if (resize_op(&(s->op),len)) {
	spin_unlock_irqrestore(&(s->lock),flags);
	printk("palacios: cannot resize op in reading key 0x%p on user keyed stream %s\n",key,s->url);
	return -1;
    }

    s->op->type = PALACIOS_KSTREAM_READ_KEY;
    s->op->buf_len = len ;
    s->op->xfer = rlen;
    s->op->user_key = key;

    // enter with it locked
    if (do_request_to_response(s,&flags)) { 
	spin_unlock_irqrestore(&(s->lock),flags);
	printk("palacios: request/response handling failed\n");
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
	printk("palacios: attempt to write key on stream that is not in write state on %s\n",s->url);
    }	

    if (resize_op(&(s->op),len)) {
	spin_unlock_irqrestore(&(s->lock),flags);
	printk("palacios: cannot resize op in reading key 0x%p on user keyed stream %s\n",key,s->url);
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
	printk("palacios: request/response handling failed\n");
	return -1;
    }
    // return with it locked

    xfer=s->op->xfer;

    spin_unlock_irqrestore(&(s->lock),flags);

    return xfer;
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
	case STREAM_USER:
	    return close_stream_user(stream);
	    break;
	default:
	    printk("palacios: unknown stream type %d in close\n",gks->stype);
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
	default:
	    printk("palacios: unknown stream type %d in preallocate_hint_key\n",gks->stype);
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
	case STREAM_USER:
	    return close_key_user(stream,key);
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
	case STREAM_USER:
	    return write_key_user(stream,key,buf,len);
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
	case STREAM_USER:
	    return read_key_user(stream,key,buf,len);
	    break;
	default:
	    printk("palacios: unknown stream type %d in read_key\n",gks->stype);
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
	printk("palacios: failed to allocated stream pool for in-memory streams\n");
	return -1;
    }

    user_streams = kmalloc(sizeof(struct user_keyed_streams),GFP_KERNEL);

    if (!user_streams) { 
	printk("palacios: failed to allocated list for user streams\n");
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

    printk("Deinit of Palacios Keyed Streams likely leaked memory\n");

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
