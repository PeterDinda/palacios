#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>

#include "palacios.h"
#include "memtrack.h"
#include "linux-exts.h"
#include "vm.h"


static int memtrack_size(struct v3_guest *guest,
			 unsigned int ioctl,
			 unsigned long arg,
			 void *priv_data)
{
    struct v3_mem_track_sizes size;
    
    v3_mem_track_get_sizes(guest->v3_ctx,&size.num_cores,&size.num_pages);
    

    if (copy_to_user((void __user *)arg, &size, sizeof(struct v3_mem_track_sizes))) {
	ERROR("palacios: unable to copy sizes to user\n");
	return -EFAULT;
    }

    return 0;

}

static int memtrack_cmd(struct v3_guest *guest,
			unsigned int ioctl,
			unsigned long arg,
			void *priv_data)
{
    struct v3_mem_track_cmd cmd;
    
    if (copy_from_user(&cmd,(void __user *)arg,sizeof(struct v3_mem_track_cmd))) { 
	ERROR("palacios: unable to copy memory tracking command from user\n");
	return -EFAULT;
    }
    
    if (cmd.request == V3_MEM_TRACK_START) { 
	if (v3_mem_track_start(guest->v3_ctx, cmd.config.access_type, cmd.config.reset_type, cmd.config.period)) {
	    ERROR("palacios: unable to start memory tracking\n");
	    return -EFAULT;
	}
    } else if (cmd.request == V3_MEM_TRACK_STOP) { 
	if (v3_mem_track_stop(guest->v3_ctx)) { 
	    ERROR("palacios: unable to stop memory tracking\n");
	    return -EFAULT;
	}
    } else {
	ERROR("palacios: unknown memory tracking request\n");
	return -EFAULT;
    }
    
    return 0;

}


#define MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define CEIL_DIV(x,y) ( ( (x) / (y) ) + !!( (x) % (y) ) )


static int memtrack_snap(struct v3_guest *guest,
			 unsigned int ioctl,
			 unsigned long arg,
			 void *priv_data)
{
    v3_mem_track_snapshot temp;
    struct v3_core_mem_track   ctemp;
    uint32_t num_cores;
    uint32_t i;
    uint64_t header_size;
    uint64_t core_header_size;
    uint64_t offset_to_num_cores;

    v3_mem_track_snapshot *user_snap=0;
    v3_mem_track_snapshot *sys_snap=0;

    
    offset_to_num_cores = (uint64_t)&(temp.num_cores) - (uint64_t)&temp;
    header_size = (uint64_t)&(temp.core[0]) - (uint64_t)&temp;
    core_header_size = (uint64_t)&(ctemp.access_bitmap) - (uint64_t)&ctemp;

    //    INFO("offset_to_num_cores=%llu header_size=%llu core_header_size=%llu\n",
    //	 offset_to_num_cores, header_size, core_header_size);
    
    if (copy_from_user(&num_cores,((void __user *)arg) + offset_to_num_cores,sizeof(num_cores))) { 
	ERROR("palacios: cannot copy number of cores from user\n");
	goto fail;
    }
    
    //INFO("num_cores=%u",num_cores);

    // overflow possible here, but only for an insane number of cores
    if (!(user_snap=palacios_alloc(sizeof(v3_mem_track_snapshot) + num_cores * sizeof(struct v3_core_mem_track)))) {
	ERROR("palacios: cannot allocate memory for copying user snapshot\n");
	goto fail;
    }

    if (copy_from_user(user_snap,(void __user *)arg,sizeof(v3_mem_track_snapshot) + num_cores*sizeof(struct v3_core_mem_track))) { 
	ERROR("palacios: cannot copy user memory track snapshot request\n");
	goto fail;
    }

    // Now we have the user's target - note that num_pages and access_bitmap need to be supplied

    if (!(sys_snap=v3_mem_track_take_snapshot(guest->v3_ctx))) { 
	ERROR("palacios: unable to get snapshot from core\n");
	goto fail;
    }

    //INFO("snapshot: numcores=%u, core[0].num_pages=%llu, request_numcores=%u\n", 
    // sys_snap->num_cores, sys_snap->core[0].num_pages, num_cores);

    // Copy the meta data
    if (copy_to_user((void __user *)arg, sys_snap, header_size)) {
	ERROR("palacios: unable to copy meta data\n");
	goto fail;
    }

    // Now per core
    for (i=0;i<MIN(num_cores,sys_snap->num_cores);i++) { 
	// copy metadata
	if (copy_to_user((void __user *)(arg + header_size + i * sizeof(struct v3_core_mem_track)),
			 &sys_snap->core[i],
			 core_header_size)) { 
	    ERROR("palacios: unable to copy core meta data\n");
	    goto fail;
	}

	// copy the bitmap
	if (copy_to_user((void __user *) user_snap->core[i].access_bitmap,
			 sys_snap->core[i].access_bitmap,
			 CEIL_DIV(MIN(user_snap->core[i].num_pages,sys_snap->core[i].num_pages),8))) { 
	    ERROR("palacios: unable to copy core data\n");
	    return -1;
	}
    }

    v3_mem_track_free_snapshot(sys_snap);
    palacios_free(user_snap);
    return 0;

 fail:
    if (sys_snap) { 
	v3_mem_track_free_snapshot(sys_snap);
    }
    if (user_snap) { 
	palacios_free(user_snap);
    }
    return -EFAULT;
}

	    
    
    




static int memtrack_init( void ) 
{
    // nothing yet
    return 0;
}


static int memtrack_deinit( void ) {

    // nothing yet
    return 0;
}



static int guest_memtrack_init(struct v3_guest * guest, void ** vm_data) 
{

    add_guest_ctrl(guest, V3_VM_MEM_TRACK_SIZE, memtrack_size, guest);
    add_guest_ctrl(guest, V3_VM_MEM_TRACK_CMD, memtrack_cmd, guest);
    add_guest_ctrl(guest, V3_VM_MEM_TRACK_SNAP, memtrack_snap, guest);

    return 0;
}


static int guest_memtrack_deinit(struct v3_guest * guest, void * vm_data) {

    remove_guest_ctrl(guest,V3_VM_MEM_TRACK_SNAP);
    remove_guest_ctrl(guest,V3_VM_MEM_TRACK_CMD);
    remove_guest_ctrl(guest, V3_VM_MEM_TRACK_SIZE);

    return 0;
}



static struct linux_ext memtrack_ext = {
    .name = "MEMTRACK_EXTENSION",
    .init = memtrack_init,
    .deinit = memtrack_deinit,
    .guest_init = guest_memtrack_init,
    .guest_deinit = guest_memtrack_deinit
};


register_extension(&memtrack_ext);
