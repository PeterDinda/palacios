/* Palacios file interface 
 * (c) Jack Lange, 2010
 */


#include <linux/fs.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include "palacios.h"

#include <palacios/vmm_file.h>

static struct list_head global_files;

struct palacios_file {
    struct file * filp;

    char * path;
    int mode;
    
    spinlock_t lock;

    struct v3_guest * guest;
    

    struct list_head file_node;
};



static void * palacios_file_open(const char * path, int mode, void * private_data) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct palacios_file * pfile = NULL;
    
    pfile = kmalloc(sizeof(struct palacios_file), GFP_KERNEL);
    memset(pfile, 0, sizeof(struct palacios_file));

    if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE)) { 
	pfile->mode = O_RDWR;
    } else if (mode & FILE_OPEN_MODE_READ) { 
	pfile->mode = O_RDONLY;
    } else if (mode & FILE_OPEN_MODE_WRITE) { 
	pfile->mode = O_WRONLY;
    } 

    pfile->filp = filp_open(path, pfile->mode, 0);
    
    if (pfile->filp == NULL) {
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
	list_add(&(pfile->file_node), &(guest->files));
    } 


    return pfile;
}

static int palacios_file_close(void * file_ptr) {
    struct palacios_file * pfile = (struct palacios_file *)file_ptr;

    filp_close(pfile->filp, NULL);
    
    list_del(&(pfile->file_node));

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
};


int palacios_file_init( void ) {
    INIT_LIST_HEAD(&(global_files));

    V3_Init_File(&palacios_file_hooks);

    return 0;
}
