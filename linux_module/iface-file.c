/* Palacios file interface 
 * (c) Jack Lange, 2010
 */


#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/version.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include "palacios.h"
#include "linux-exts.h"

#include <interfaces/vmm_file.h>

static struct list_head global_files;

#define isprint(a) ((a >= ' ') && (a <= '~'))

struct palacios_file {
    struct file * filp;

    char * path;
    int mode;
    
    spinlock_t lock;

    struct v3_guest * guest;
    

    struct list_head file_node;
};


// Currently this just holds the list of open files
struct vm_file_state {
    struct list_head open_files;
};



static int palacios_file_mkdir(const char * pathname, unsigned short perms, int recurse);

static int mkdir_recursive(const char * path, unsigned short perms) {
    char * tmp_str = NULL;
    char * dirname_ptr;
    char * tmp_iter;

    tmp_str = kmalloc(strlen(path) + 1, GFP_KERNEL);
    memset(tmp_str, 0, strlen(path) + 1);
    strncpy(tmp_str, path, strlen(path));

    dirname_ptr = tmp_str;
    tmp_iter = tmp_str;

    // parse path string, call palacios_file_mkdir recursively.


    while (dirname_ptr != NULL) {
	int done = 0;

	while ((*tmp_iter != '/') && 
	       (*tmp_iter != '\0')) {

	    if ( (!isprint(*tmp_iter))) {
		printk("Invalid character in path name (%d)\n", *tmp_iter);
		return -1;
	    } else {
		tmp_iter++;
	    }
	}

	if (*tmp_iter == '/') {
	    *tmp_iter = '\0';
	} else {
	    done = 1;
	}

	// Ignore empty directories
	if ((tmp_iter - dirname_ptr) > 1) {
	    if (palacios_file_mkdir(tmp_str, perms, 0) != 0) {
		printk("Could not create directory (%s)\n", tmp_str);
		return -1;
	    }
	}

	if (done) {
	    break;
	} else {
	    *tmp_iter = '/';
	}
	
	tmp_iter++;

	dirname_ptr = tmp_iter;
    }
    
    kfree(tmp_str);

    return 0;
}

static int palacios_file_mkdir(const char * pathname, unsigned short perms, int recurse) {
    /* Welcome to the jungle... */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,41)
    /* DO NOT REFERENCE THIS VARIABLE */
    /* It only exists to provide version compatibility */
    struct path tmp_path; 
#endif

    struct path * path_ptr = NULL;
    struct dentry * dentry;
    int ret = 0;



    if (recurse != 0) {
	return mkdir_recursive(pathname, perms);
    } 

    /* Before Linux 3.1 this was somewhat more difficult */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,41)
    {
	struct nameidata nd;

	// I'm not 100% sure about the version here, but it was around this time that the API changed
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,37) 
	ret = kern_path_parent(pathname, &nd);
#else 

	if (path_lookup(pathname, LOOKUP_DIRECTORY | LOOKUP_FOLLOW, &nd) == 0) {
	    return 0;
	}

	if (path_lookup(pathname, LOOKUP_PARENT | LOOKUP_FOLLOW, &nd) != 0) {
	    return -1;
	}
#endif

	if (ret != 0) {
	    printk("%s:%d - Error: kern_path_parent() returned error for (%s)\n", __FILE__, __LINE__, 
		   pathname);
	    return -1;
	}
	
	dentry = lookup_create(&nd, 1);
	path_ptr = &(nd.path);
    }
#else 
    {
	dentry = kern_path_create(AT_FDCWD, pathname, &tmp_path, 1);
	
	if (IS_ERR(dentry)) {
	    return 0;
	}
	
	path_ptr = &tmp_path;
    }
#endif    


    if (!IS_ERR(dentry)) {
	ret = vfs_mkdir(path_ptr->dentry->d_inode, dentry, perms);
    }

    mutex_unlock(&(path_ptr->dentry->d_inode->i_mutex));
    path_put(path_ptr);

    return ret;
}


static void * palacios_file_open(const char * path, int mode, void * private_data) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct palacios_file * pfile = NULL;	
    struct vm_file_state * vm_state = NULL;

    if (guest != NULL) {
	vm_state = get_vm_ext_data(guest, "FILE_INTERFACE");
	
	if (vm_state == NULL) {
	    printk("ERROR: Could not locate vm file state for extension FILE_INTERFACE\n");
	    return NULL;
	}
    }
    
    pfile = kmalloc(sizeof(struct palacios_file), GFP_KERNEL);
    memset(pfile, 0, sizeof(struct palacios_file));

    if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE)) { 
	pfile->mode = O_RDWR;
    } else if (mode & FILE_OPEN_MODE_READ) { 
	pfile->mode = O_RDONLY;
    } else if (mode & FILE_OPEN_MODE_WRITE) { 
	pfile->mode = O_WRONLY;
    } 
    
    if (mode & FILE_OPEN_MODE_CREATE) {
	pfile->mode |= O_CREAT;
    }


    pfile->mode |= O_LARGEFILE;


    pfile->filp = filp_open(path, pfile->mode, 0);
    
    if (IS_ERR(pfile->filp)) {
	printk("Cannot open file: %s\n", path);
	return NULL;
    }

    pfile->path = kmalloc(strlen(path) + 1, GFP_KERNEL);
    strncpy(pfile->path, path, strlen(path));
    pfile->guest = guest;
    
    spin_lock_init(&(pfile->lock));

    if (guest == NULL) {
	list_add(&(pfile->file_node), &(global_files));
    } else {
	list_add(&(pfile->file_node), &(vm_state->open_files));
    } 


    return pfile;
}

static int palacios_file_close(void * file_ptr) {
    struct palacios_file * pfile = (struct palacios_file *)file_ptr;

    filp_close(pfile->filp, NULL);
    
    list_del(&(pfile->file_node));

    kfree(pfile->path);    
    kfree(pfile);

    return 0;
}

static long long palacios_file_size(void * file_ptr) {
    struct palacios_file * pfile = (struct palacios_file *)file_ptr;
    struct file * filp = pfile->filp;
    struct kstat s;
    int ret;
    
    ret = vfs_getattr(filp->f_path.mnt, filp->f_path.dentry, &s);

    if (ret != 0) {
	printk("Failed to fstat file\n");
	return -1;
    }

    return s.size;
}

static long long palacios_file_read(void * file_ptr, void * buffer, long long length, long long offset){
    struct palacios_file * pfile = (struct palacios_file *)file_ptr;
    struct file * filp = pfile->filp;
    ssize_t ret;
    mm_segment_t old_fs;
	
    old_fs = get_fs();
    set_fs(get_ds());
	
    ret = vfs_read(filp, buffer, length, &offset);
	
    set_fs(old_fs);
	
    if (ret <= 0) {
	printk("sys_read of %p for %lld bytes failed\n", filp, length);		
    }
	
    return ret;
}


static long long palacios_file_write(void * file_ptr, void * buffer, long long length, long long offset) {
    struct palacios_file * pfile = (struct palacios_file *)file_ptr;
    struct file * filp = pfile->filp;
    mm_segment_t old_fs;
    ssize_t ret;

    old_fs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(filp, buffer, length, &offset);
	
    set_fs(old_fs);

 
    if (ret <= 0) {
	printk("sys_write failed\n");		
    }
	
    return ret;
}


static struct v3_file_hooks palacios_file_hooks = {
	.open		= palacios_file_open,
	.close		= palacios_file_close,
	.read		= palacios_file_read,
	.write		= palacios_file_write,
	.size		= palacios_file_size,
	.mkdir          = palacios_file_mkdir,
};



static int file_init( void ) {
    INIT_LIST_HEAD(&(global_files));

    V3_Init_File(&palacios_file_hooks);

    return 0;
}


static int file_deinit( void ) {
    if (!list_empty(&(global_files))) {
	printk("Error removing module with open files\n");
    }

    return 0;
}

static int guest_file_init(struct v3_guest * guest, void ** vm_data) {
    struct vm_file_state * state = kmalloc(sizeof(struct vm_file_state), GFP_KERNEL);
    
    INIT_LIST_HEAD(&(state->open_files));

    *vm_data = state;


    return 0;
}


static int guest_file_deinit(struct v3_guest * guest, void * vm_data) {
    
    return 0;
}


static struct linux_ext file_ext = {
    .name = "FILE_INTERFACE",
    .init = file_init, 
    .deinit = file_deinit,
    .guest_init = guest_file_init,
    .guest_deinit = guest_file_deinit
};


register_extension(&file_ext);
