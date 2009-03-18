/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <devices/ide.h>
#include "ide-types.h"



#define PRI_DATA_PORT         0x1f0
#define PRI_FEATURES_PORT     0x1f1
#define PRI_SECT_CNT_PORT     0x1f2
#define PRI_SECT_NUM_PORT     0x1f3
#define PRI_CYL_LOW_PORT      0x1f4
#define PRI_CYL_HIGH_PORT     0x1f5
#define PRI_DRV_SEL_PORT      0x1f6
#define PRI_CMD_PORT          0x1f7
#define PRI_CTRL_PORT         0x3f6
#define PRI_ADDR_REG_PORT     0x3f7

#define SEC_DATA_PORT         0x170
#define SEC_FEATURES_PORT     0x171
#define SEC_SECT_CNT_PORT     0x172
#define SEC_SECT_NUM_PORT     0x173
#define SEC_CYL_LOW_PORT      0x174
#define SEC_CYL_HIGH_PORT     0x175
#define SEC_DRV_SEL_PORT      0x176
#define SEC_CMD_PORT          0x177
#define SEC_CTRL_PORT         0x376
#define SEC_ADDR_REG_PORT     0x377


typedef enum {IDE_DISK, IDE_CDROM, IDE_NONE} ide_dev_type_t;
static const char * ide_dev_type_strs[] = {"HARDDISK", "CDROM", "NONE"};


static inline const char * device_type_to_str(ide_dev_type_t type) {
    if (type > 2) {
	return NULL;
    }

    return ide_dev_type_strs[type];
}





struct ide_drive {
    // Command Registers

    ide_dev_type_t drive_type;


    uint8_t sector_count;               // 0x1f2,0x172
    uint8_t sector_num;               // 0x1f3,0x173
    union {
	uint16_t cylinder;
	struct {
	    uint8_t cylinder_low;       // 0x1f4,0x174
	    uint8_t cylinder_high;      // 0x1f5,0x175
	} __attribute__((packed));
    } __attribute__((packed));


};



struct ide_channel {
    struct ide_drive drives[2];

    // Command Registers
    struct ide_error_reg error_reg;     // [read] 0x1f1,0x171

    struct ide_features_reg features;

    struct ide_drive_head_reg drive_head; // 0x1f6,0x176

    struct ide_status_reg status;       // [read] 0x1f7,0x177
    uint8_t command_reg;                // [write] 0x1f7,0x177

    // Control Registers
    struct ide_ctrl_reg ctrl_reg; // [write] 0x3f6,0x376
};



struct ide_internal {
    struct ide_channel channels[2];
};




static inline int get_channel_index(ushort_t port) {
    if (((port & 0xfff8) == 0x1f0) ||
	((port & 0xfffe) == 0x3f6)) {
	return 0;
    } else if (((port & 0xfff8) == 0x170) ||
	       ((port & 0xfffe) == 0x376)) {
	return 1;
    }

    return -1;
}

static inline struct ide_channel * get_selected_channel(struct ide_internal * ide, ushort_t port) {
    int channel_idx = get_channel_index(port);    
    return &(ide->channels[channel_idx]);
}

static inline struct ide_drive * get_selected_drive(struct ide_channel * channel) {
    return &(channel->drives[channel->drive_head.drive_sel]);
}


static inline int is_lba_enabled(struct ide_channel * channel) {
    return channel->drive_head.lba_mode;
}


static void drive_reset(struct ide_drive * drive) {
    drive->sector_count = 0x01;
    drive->sector_num = 0x01;
    
    if (drive->drive_type == IDE_CDROM) {
	drive->cylinder = 0xeb14;
    } else {
	drive->cylinder = 0x0000;
    }

    // Send the reset signal to the connected device callbacks
    //     channel->drives[0].reset();
    //    channel->drives[1].reset();
}

static void channel_reset(struct ide_channel * channel) {
    
    // set busy and seek complete flags
    channel->status.val = 0x90;

    // Clear errors
    channel->error_reg.val = 0x01;

    // clear commands
    channel->command_reg = 0x00;

    channel->ctrl_reg.irq_enable = 0;
}

static void channel_reset_complete(struct ide_channel * channel) {
    channel->status.busy = 0;
    channel->status.ready = 1;

    channel->drive_head.head_num = 0;    
    
    drive_reset(&(channel->drives[0]));
    drive_reset(&(channel->drives[1]));
}



static int write_cmd_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    PrintDebug("IDE: Writing Command Port %x (val=%x)\n", port, *(uint8_t *)src);
    return -1;
}


static int write_data_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    PrintDebug("IDE: Writing Data Port %x (val=%x)\n", port, *(uint8_t *)src);
    return -1;
}


static int read_data_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    PrintDebug("IDE: Reading Data Port %x\n", port);
    return -1;
}

static int write_port_std(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);
	    
    if (length != 1) {
	PrintError("Invalid Write length on IDE port %x\n", port);
	return -1;
    }

    PrintDebug("IDE: Writing Standard Port %x (val=%x)\n", port, *(uint8_t *)src);


    switch (port) {
	// reset and interrupt enable
	case PRI_CTRL_PORT:
	case SEC_CTRL_PORT: {
	    struct ide_ctrl_reg * tmp_ctrl = (struct ide_ctrl_reg *)src;

	    // only reset channel on a 0->1 reset bit transition
	    if ((!channel->ctrl_reg.soft_reset) && (tmp_ctrl->soft_reset)) {
		channel_reset(channel);
	    } else if ((channel->ctrl_reg.soft_reset) && (!tmp_ctrl->soft_reset)) {
		channel_reset_complete(channel);
	    }

	    channel->ctrl_reg.val = tmp_ctrl->val;	    
	    break;
	}
	case PRI_FEATURES_PORT:
	case SEC_FEATURES_PORT:
	    channel->features.val = *(uint8_t *)src;
	    break;

	case PRI_SECT_CNT_PORT:
	case SEC_SECT_CNT_PORT:
	    drive->sector_count = *(uint8_t *)src;
	    break;

	case PRI_SECT_NUM_PORT:
	case SEC_SECT_NUM_PORT:
	    drive->sector_num = *(uint8_t *)src;

	case PRI_CYL_LOW_PORT:
	case SEC_CYL_LOW_PORT:
	    drive->cylinder_low = *(uint8_t *)src;
	    break;

	case PRI_CYL_HIGH_PORT:
	case SEC_CYL_HIGH_PORT:
	    drive->cylinder_high = *(uint8_t *)src;
	    break;

	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT: {
	    channel->drive_head.val = *(uint8_t *)src;
	    
	    // make sure the reserved bits are ok..
	    // JRL TODO: check with new ramdisk to make sure this is right...
	    channel->drive_head.val |= 0xa0;

	    drive = get_selected_drive(channel);

	    // Selecting a non-present device is a no-no
	    if (drive->drive_type == IDE_NONE) {
		PrintDebug("Attempting to select a non-present drive\n");
		channel->error_reg.abort = 1;
		channel->status.error = 1;
	    }

	    break;
	}
	default:
	    PrintError("IDE: Write to unknown Port %x\n", port);
	    return -1;
    }
    return length;
}


static int read_port_std(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);
    
    if (length != 1) {
	PrintError("Invalid Read length on IDE port %x\n", port);
	return -1;
    }

    PrintDebug("IDE: Reading Standard Port %x\n", port);


    if ((port == PRI_ADDR_REG_PORT) ||
	(port == SEC_ADDR_REG_PORT)) {
	// unused, return 0xff
	*(uint8_t *)dst = 0xff;
	return length;
    }


    // if no drive is present just return 0 + reserved bits
    if (drive->drive_type == IDE_NONE) {
	if ((port == PRI_DRV_SEL_PORT) ||
	    (port == SEC_DRV_SEL_PORT)) {
	    *(uint8_t *)dst = 0xa0;
	} else {
	    *(uint8_t *)dst = 0;
	}

	return length;
    }

    switch (port) {

	// This is really the error register.
	case PRI_FEATURES_PORT:
	case SEC_FEATURES_PORT:
	    *(uint8_t *)dst = channel->error_reg.val;
	    break;
	    
	case PRI_SECT_CNT_PORT:
	case SEC_SECT_CNT_PORT:
	    *(uint8_t *)dst = drive->sector_count;
	    break;

	case PRI_SECT_NUM_PORT:
	case SEC_SECT_NUM_PORT:
	    *(uint8_t *)dst = drive->sector_num;
	    break;

	case PRI_CYL_LOW_PORT:
	case SEC_CYL_LOW_PORT:
	    *(uint8_t *)dst = drive->cylinder_low;
	    break;


	case PRI_CYL_HIGH_PORT:
	case SEC_CYL_HIGH_PORT:
	    *(uint8_t *)dst = drive->cylinder_high;
	    break;

	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT:  // hard disk drive and head register 0x1f6
	    *(uint8_t *)dst = channel->drive_head.val;
	    break;

	case PRI_CTRL_PORT:
	case SEC_CTRL_PORT:
	case PRI_CMD_PORT:
	case SEC_CMD_PORT:
	    // Something about lowering interrupts here....
	    *(uint8_t *)dst = channel->status.val;
	    break;

	default:
	    PrintError("Invalid Port: %x\n", port);
	    return -1;
    }

    return length;
}



static void init_drive(struct ide_drive * drive) {

    drive->sector_count = 0x01;
    drive->sector_num = 0x01;
    drive->cylinder = 0x0000;


    drive->drive_type = IDE_NONE;

}

static void init_channel(struct ide_channel * channel) {
    int i = 0;

    channel->error_reg.val = 0x01;
    channel->drive_head.val = 0x00;
    channel->status.val = 0x00;
    channel->command_reg = 0x00;
    channel->ctrl_reg.val = 0x08;

    for (i = 0; i < 2; i++) {
	init_drive(&(channel->drives[i]));
    }

}

static void init_ide_state(struct ide_internal * ide) {
    int i;

    for (i = 0; i < 2; i++) {
	init_channel(&(ide->channels[i]));
    }
}



static int init_ide(struct vm_device * dev) {
    struct ide_internal * ide = (struct ide_internal *)(dev->private_data);

    PrintDebug("IDE: Initializing IDE\n");

    init_ide_state(ide);


    v3_dev_hook_io(dev, PRI_CTRL_PORT, 
		   &read_port_std, &write_port_std);

    v3_dev_hook_io(dev, PRI_DATA_PORT, 
		   &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, PRI_FEATURES_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_SECT_CNT_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_SECT_NUM_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_CYL_LOW_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_CYL_HIGH_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_DRV_SEL_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, PRI_CMD_PORT, 
		   &read_port_std, &write_cmd_port);


    v3_dev_hook_io(dev, SEC_CTRL_PORT, 
		   &read_port_std, &write_port_std);

    v3_dev_hook_io(dev, SEC_DATA_PORT, 
		   &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, SEC_FEATURES_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_SECT_CNT_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_SECT_NUM_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_CYL_LOW_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_CYL_HIGH_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_DRV_SEL_PORT, 
		   &read_port_std, &write_port_std);
    v3_dev_hook_io(dev, SEC_CMD_PORT, 
		   &read_port_std, &write_cmd_port);
  
  

    v3_dev_hook_io(dev, SEC_ADDR_REG_PORT, 
		   &read_port_std, &write_port_std);

    v3_dev_hook_io(dev, PRI_ADDR_REG_PORT, 
		   &read_port_std, &write_port_std);

    return 0;
}


static int deinit_ide(struct vm_device * dev) {
    // unhook io ports....
    // deregister from PCI?
    return 0;
}


static struct vm_device_ops dev_ops = {
    .init = init_ide,
    .deinit = deinit_ide,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


struct vm_device *  v3_create_ide() {
    struct ide_internal * ide  = (struct ide_internal *)V3_Malloc(sizeof(struct ide_internal));  
    struct vm_device * device = v3_create_device("IDE", &dev_ops, ide);

    //    ide->pci = pci;

    PrintDebug("IDE: Creating IDE bus x 2\n");

    return device;
}
