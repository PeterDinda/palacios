/* Host PCI interface 
 *  (c) Jack Lange, 2012
 *  jacklange@cs.pitt.edu 
 */

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "palacios.h"
#include "linux-exts.h"


#include <interfaces/host_pci.h>

static struct list_head device_list;
static spinlock_t lock;




struct pci_dev;
struct iommu_domain;

struct host_pci_device {
    char name[128];
    
    enum {PASSTHROUGH, USER} type; 
  
    enum {INTX_IRQ, MSI_IRQ, MSIX_IRQ} irq_type;
    uint32_t num_vecs;

    union {
	struct {    
	    u8 in_use;
	    u8 iommu_enabled;
	    
	    u32 bus;
	    u32 devfn;
	    
	    spinlock_t intx_lock;
	    u8 intx_disabled;

	    u32 num_msix_vecs;
	    struct msix_entry * msix_entries;
	    struct iommu_domain * iommu_domain;
	    
	    struct pci_dev * dev; 
	} hw_dev;

	//	struct user_dev_state user_dev;
    };

    struct v3_host_pci_dev v3_dev;

    struct list_head dev_node;
};


//#include "iface-host-pci-user.h"
#include "iface-host-pci-hw.h"


static struct host_pci_device * find_dev_by_name(char * name) {
    struct host_pci_device * dev = NULL;

    list_for_each_entry(dev, &device_list, dev_node) {
	if (strncmp(dev->name, name, 128) == 0) {
	    return dev;
	}
    }

    return NULL;
}



static struct v3_host_pci_dev * request_pci_dev(char * url, void * v3_ctx) {
   
    unsigned long flags;
    struct host_pci_device * host_dev = NULL;

    spin_lock_irqsave(&lock, flags);
    host_dev = find_dev_by_name(url);
    spin_unlock_irqrestore(&lock, flags);
    
    if (host_dev == NULL) {
	printk("Could not find host device (%s)\n", url);
	return NULL;
    }

    if (host_dev->type == PASSTHROUGH) {
	if (reserve_hw_pci_dev(host_dev, v3_ctx) == -1) {
	    printk("Could not reserve host device (%s)\n", url);
	    return NULL;
	}
    } else {
	printk("Unsupported Host device type\n");
	return NULL;
    }



    return &(host_dev->v3_dev);

}


static int host_pci_config_write(struct v3_host_pci_dev * v3_dev, unsigned int reg_num, 
				  void * src, unsigned int length) {
    struct host_pci_device * host_dev = v3_dev->host_data;

    if (host_dev->type == PASSTHROUGH) {
	return write_hw_pci_config(host_dev, reg_num, src, length);
    }
 
    printk("Error in config write handler\n");
    return -1;
}

static int host_pci_config_read(struct v3_host_pci_dev * v3_dev, unsigned int reg_num, 
				  void * dst, unsigned int length) {
    struct host_pci_device * host_dev = v3_dev->host_data;

    if (host_dev->type == PASSTHROUGH) {
	return read_hw_pci_config(host_dev, reg_num, dst, length);
    }
 
    printk("Error in config read handler\n");
    return -1;
}


static int host_pci_ack_irq(struct v3_host_pci_dev * v3_dev, unsigned int vector) {
    struct host_pci_device * host_dev = v3_dev->host_data;

    if (host_dev->type == PASSTHROUGH) {
	return hw_ack_irq(host_dev, vector);
    }
 
    printk("Error in config irq ack handler\n");
    return -1;
}



static int host_pci_cmd(struct v3_host_pci_dev * v3_dev, host_pci_cmd_t cmd, u64 arg) {
    struct host_pci_device * host_dev = v3_dev->host_data;

    if (host_dev->type == PASSTHROUGH) {
	return hw_pci_cmd(host_dev, cmd, arg);
    }
 
    printk("Error in config pci cmd handler\n");
    return -1;
    
}

static struct v3_host_pci_hooks pci_hooks = {
    .request_device = request_pci_dev,
    .config_write = host_pci_config_write,
    .config_read = host_pci_config_read,
    .ack_irq = host_pci_ack_irq,
    .pci_cmd = host_pci_cmd,

};



static int register_pci_hw_dev(unsigned int cmd, unsigned long arg) {
    void __user * argp = (void __user *)arg;
    struct v3_hw_pci_dev hw_dev_arg ;
    struct host_pci_device * host_dev = NULL;
    unsigned long flags;
    int ret = 0;

    if (copy_from_user(&hw_dev_arg, argp, sizeof(struct v3_hw_pci_dev))) {
	printk("%s(%d): copy from user error...\n", __FILE__, __LINE__);
	return -EFAULT;
    }

    host_dev = kzalloc(sizeof(struct host_pci_device), GFP_KERNEL);

    
    strncpy(host_dev->name, hw_dev_arg.name, 128);
    host_dev->v3_dev.host_data = host_dev;
    

    host_dev->type = PASSTHROUGH;
    host_dev->hw_dev.bus = hw_dev_arg.bus;
    host_dev->hw_dev.devfn = PCI_DEVFN(hw_dev_arg.dev, hw_dev_arg.func);
    

    spin_lock_irqsave(&lock, flags);
    if (!find_dev_by_name(hw_dev_arg.name)) {
	list_add(&(host_dev->dev_node), &device_list);
	ret = 1;
    }
    spin_unlock_irqrestore(&lock, flags);

    if (ret == 0) {
	// Error device already exists
	kfree(host_dev);
	return -EFAULT;
    }

    
    setup_hw_pci_dev(host_dev);

    return 0;
}


static int register_pci_user_dev(unsigned int cmd, unsigned long arg) {
    return 0;
}




static int host_pci_init( void ) {
    INIT_LIST_HEAD(&(device_list));
    spin_lock_init(&lock);

    V3_Init_Host_PCI(&pci_hooks);
    

    add_global_ctrl(V3_ADD_PCI_HW_DEV, register_pci_hw_dev);
    add_global_ctrl(V3_ADD_PCI_USER_DEV, register_pci_user_dev);

    return 0;
}



static struct linux_ext host_pci_ext = {
    .name = "HOST_PCI",
    .init = host_pci_init,
};



register_extension(&host_pci_ext);
