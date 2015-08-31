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
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest_mem.h>
#include <devices/ide.h>
#include <devices/pci.h>
#include <devices/southbridge.h>
#include "ide-types.h"
#include "atapi-types.h"

#ifndef V3_CONFIG_DEBUG_IDE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define PRI_DEFAULT_IRQ 14
#define SEC_DEFAULT_IRQ 15


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


#define PRI_DEFAULT_DMA_PORT 0xc000
#define SEC_DEFAULT_DMA_PORT 0xc008

#define DATA_BUFFER_SIZE 2048

#define ATAPI_BLOCK_SIZE 2048
#define HD_SECTOR_SIZE 512


static const char * ide_pri_port_strs[] = {"PRI_DATA", "PRI_FEATURES", "PRI_SECT_CNT", "PRI_SECT_NUM", 
					  "PRI_CYL_LOW", "PRI_CYL_HIGH", "PRI_DRV_SEL", "PRI_CMD",
					   "PRI_CTRL", "PRI_ADDR_REG"};


static const char * ide_sec_port_strs[] = {"SEC_DATA", "SEC_FEATURES", "SEC_SECT_CNT", "SEC_SECT_NUM", 
					  "SEC_CYL_LOW", "SEC_CYL_HIGH", "SEC_DRV_SEL", "SEC_CMD",
					   "SEC_CTRL", "SEC_ADDR_REG"};

static const char * ide_dma_port_strs[] = {"DMA_CMD", NULL, "DMA_STATUS", NULL,
					   "DMA_PRD0", "DMA_PRD1", "DMA_PRD2", "DMA_PRD3"};


typedef enum {BLOCK_NONE, BLOCK_DISK, BLOCK_CDROM} v3_block_type_t;

static inline const char * io_port_to_str(uint16_t port) {
    if ((port >= PRI_DATA_PORT) && (port <= PRI_CMD_PORT)) {
	return ide_pri_port_strs[port - PRI_DATA_PORT];
    } else if ((port >= SEC_DATA_PORT) && (port <= SEC_CMD_PORT)) {
	return ide_sec_port_strs[port - SEC_DATA_PORT];
    } else if ((port == PRI_CTRL_PORT) || (port == PRI_ADDR_REG_PORT)) {
	return ide_pri_port_strs[port - PRI_CTRL_PORT + 8];
    } else if ((port == SEC_CTRL_PORT) || (port == SEC_ADDR_REG_PORT)) {
	return ide_sec_port_strs[port - SEC_CTRL_PORT + 8];
    } 
    return NULL;
}


static inline const char * dma_port_to_str(uint16_t port) {
    return ide_dma_port_strs[port & 0x7];
}



struct ide_cd_state {
    struct atapi_sense_data sense;

    uint8_t atapi_cmd;
    struct atapi_error_recovery err_recovery;
};

struct ide_hd_state {
    uint32_t accessed;

    /* this is the multiple sector transfer size as configured for read/write multiple sectors*/
    uint32_t mult_sector_num;

    /* This is the current op sector size:
     * for multiple sector ops this equals mult_sector_num
     * for standard ops this equals 1
     */
    uint64_t cur_sector_num;
};

struct ide_drive {
    // Command Registers

    v3_block_type_t drive_type;

    struct v3_dev_blk_ops * ops;

    union {
	struct ide_cd_state cd_state;
	struct ide_hd_state hd_state;
    };

    char model[41];

    // Where we are in the data transfer
    uint64_t transfer_index;

    // the length of a transfer
    // calculated for easy access
    uint64_t transfer_length;

    uint64_t current_lba;

    // We have a local data buffer that we use for IO port accesses
    uint8_t data_buf[DATA_BUFFER_SIZE];


    uint32_t num_cylinders;
    uint32_t num_heads;
    uint32_t num_sectors;


    struct lba48_state {
	// all start at zero
	uint64_t lba;                  
	uint16_t sector_count;            // for LBA48
	uint8_t  sector_count_state;      // two step write to 1f2/172 (high first)
	uint8_t  lba41_state;             // two step write to 1f3
        uint8_t  lba52_state;             // two step write to 1f4
        uint8_t  lba63_state;             // two step write to 15
    } lba48;

    void * private_data;
    
    union {
	uint8_t sector_count;             // 0x1f2,0x172  (ATA)
	struct atapi_irq_flags irq_flags; // (ATAPI ONLY)
    } __attribute__((packed));


    union {
	uint8_t sector_num;               // 0x1f3,0x173
	uint8_t lba0;
    } __attribute__((packed));

    union {
	uint16_t cylinder;
	uint16_t lba12;
	
	struct {
	    uint8_t cylinder_low;       // 0x1f4,0x174
	    uint8_t cylinder_high;      // 0x1f5,0x175
	} __attribute__((packed));
	
	struct {
	    uint8_t lba1;
	    uint8_t lba2;
	} __attribute__((packed));
	
	
	// The transfer length requested by the CPU 
	uint16_t req_len;
    } __attribute__((packed));

};



struct ide_channel {
    struct ide_drive drives[2];

    // Command Registers
    struct ide_error_reg error_reg;     // [read] 0x1f1,0x171

    struct ide_features_reg features;

    struct ide_drive_head_reg drive_head; // 0x1f6,0x176

    struct ide_status_reg status;       // [read] 0x1f7,0x177
    uint8_t cmd_reg;                // [write] 0x1f7,0x177

    int irq; // this is temporary until we add PCI support

    // Control Registers
    struct ide_ctrl_reg ctrl_reg; // [write] 0x3f6,0x376

    union {
	uint8_t dma_ports[8];
	struct {
	    struct ide_dma_cmd_reg dma_cmd;
	    uint8_t rsvd1;
	    struct ide_dma_status_reg dma_status;
	    uint8_t rsvd2;
	    uint32_t dma_prd_addr;
	} __attribute__((packed));
    } __attribute__((packed));

    uint32_t dma_tbl_index;
};



struct ide_internal {
    struct ide_channel channels[2];

    struct v3_southbridge * southbridge;
    struct vm_device * pci_bus;

    struct pci_device * ide_pci;

    struct v3_vm_info * vm;
};





/* Utility functions */

static inline uint16_t be_to_le_16(const uint16_t val) {
    uint8_t * buf = (uint8_t *)&val;
    return (buf[0] << 8) | (buf[1]) ;
}

static inline uint16_t le_to_be_16(const uint16_t val) {
    return be_to_le_16(val);
}


static inline uint32_t be_to_le_32(const uint32_t val) {
    uint8_t * buf = (uint8_t *)&val;
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static inline uint32_t le_to_be_32(const uint32_t val) {
    return be_to_le_32(val);
}


static inline int is_lba28(struct ide_channel * channel) {
    return channel->drive_head.lba_mode && channel->drive_head.rsvd1 && channel->drive_head.rsvd2;
}

static inline int is_lba48(struct ide_channel * channel) {
    return channel->drive_head.lba_mode && !channel->drive_head.rsvd1 && !channel->drive_head.rsvd2;
}

static inline int is_chs(struct ide_channel * channel) {
    return !channel->drive_head.lba_mode;
}

static inline int get_channel_index(ushort_t port) {
    if (((port & 0xfff8) == 0x1f0) ||
	((port & 0xfffe) == 0x3f6) || 
	((port & 0xfff8) == 0xc000)) {
	return 0;
    } else if (((port & 0xfff8) == 0x170) ||
	       ((port & 0xfffe) == 0x376) ||
	       ((port & 0xfff8) == 0xc008)) {
	return 1;
    }

    return -1;
}

static inline struct ide_channel * get_selected_channel(struct ide_internal * ide, ushort_t port) {
    int channel_idx = get_channel_index(port);    
    if (channel_idx >= 0) { 
	return &(ide->channels[channel_idx]);
    } else {
	PrintError(VM_NONE,VCORE_NONE,"ide: Cannot Determine Selected Channel\n");
	return 0;
    }
}

static inline struct ide_drive * get_selected_drive(struct ide_channel * channel) {
    return &(channel->drives[channel->drive_head.drive_sel]);
}




/* Drive Commands */
static void ide_raise_irq(struct ide_internal * ide, struct ide_channel * channel) {
    if (channel->ctrl_reg.irq_disable == 0) {

	PrintDebug(ide->vm,VCORE_NONE, "Raising IDE Interrupt %d\n", channel->irq);

        channel->dma_status.int_gen = 1;
        v3_raise_irq(ide->vm, channel->irq);
    } else {
	PrintDebug(ide->vm,VCORE_NONE, "IDE Interrupt %d cannot be raised as irq is disabled on channel\n",channel->irq);
    }
}


static void drive_reset(struct ide_drive * drive) {
    drive->sector_count = 0x01;
    drive->sector_num = 0x01;

    PrintDebug(VM_NONE,VCORE_NONE, "Resetting drive %s\n", drive->model);
    
    if (drive->drive_type == BLOCK_CDROM) {
	drive->cylinder = 0xeb14;
    } else {
	drive->cylinder = 0x0000;
	//drive->hd_state.accessed = 0;
    }


    memset(drive->data_buf, 0, sizeof(drive->data_buf));
    drive->transfer_index = 0;

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
    channel->cmd_reg = 0;  // NOP

    channel->ctrl_reg.irq_disable = 0;
}

static void channel_reset_complete(struct ide_channel * channel) {
    channel->status.busy = 0;
    channel->status.ready = 1;

    channel->drive_head.head_num = 0;    
    
    drive_reset(&(channel->drives[0]));
    drive_reset(&(channel->drives[1]));
}


static void ide_abort_command(struct ide_internal * ide, struct ide_channel * channel) {

    PrintDebug(VM_NONE,VCORE_NONE,"Aborting IDE Command\n");

    channel->status.val = 0x41; // Error + ready
    channel->error_reg.val = 0x04; // No idea...

    ide_raise_irq(ide, channel);
}


static int dma_read(struct guest_info * core, struct ide_internal * ide, struct ide_channel * channel);
static int dma_write(struct guest_info * core, struct ide_internal * ide, struct ide_channel * channel);


/* ATAPI functions */
#include "atapi.h"

/* ATA functions */
#include "ata.h"



static void print_prd_table(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_dma_prd prd_entry;
    int index = 0;

    V3_Print(VM_NONE, VCORE_NONE,"Dumping PRD table\n");

    while (1) {
	uint32_t prd_entry_addr = channel->dma_prd_addr + (sizeof(struct ide_dma_prd) * index);
	int ret = 0;

	ret = v3_read_gpa_memory(&(ide->vm->cores[0]), prd_entry_addr, sizeof(struct ide_dma_prd), (void *)&prd_entry);
	
	if (ret != sizeof(struct ide_dma_prd)) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not read PRD\n");
	    return;
	}

	V3_Print(VM_NONE, VCORE_NONE,"\tPRD Addr: %x, PRD Len: %d, EOT: %d\n", 
		   prd_entry.base_addr, 
		   (prd_entry.size == 0) ? 0x10000 : prd_entry.size, 
		   prd_entry.end_of_table);

	if (prd_entry.end_of_table) {
	    break;
	}

	index++;
    }

    return;
}


/* IO Operations */
static int dma_read(struct guest_info * core, struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    // This is at top level scope to do the EOT test at the end
    struct ide_dma_prd prd_entry = {};
    uint_t bytes_left = drive->transfer_length;

    // Read in the data buffer....
    // Read a sector/block at a time until the prd entry is full.

#ifdef V3_CONFIG_DEBUG_IDE
    print_prd_table(ide, channel);
#endif

    PrintDebug(core->vm_info, core, "DMA read for %d bytes\n", bytes_left);

    // Loop through the disk data
    while (bytes_left > 0) {
	uint32_t prd_entry_addr = channel->dma_prd_addr + (sizeof(struct ide_dma_prd) * channel->dma_tbl_index);
	uint_t prd_bytes_left = 0;
	uint_t prd_offset = 0;
	int ret;

	PrintDebug(core->vm_info, core, "PRD table address = %x\n", channel->dma_prd_addr);

	ret = v3_read_gpa_memory(core, prd_entry_addr, sizeof(struct ide_dma_prd), (void *)&prd_entry);

	if (ret != sizeof(struct ide_dma_prd)) {
	    PrintError(core->vm_info, core, "Could not read PRD\n");
	    return -1;
	}

	PrintDebug(core->vm_info, core, "PRD Addr: %x, PRD Len: %d, EOT: %d\n", 
		   prd_entry.base_addr, prd_entry.size, prd_entry.end_of_table);

	// loop through the PRD data....

	if (prd_entry.size == 0) {
	    // a size of 0 means 64k
	    prd_bytes_left = 0x10000;
	} else {
	    prd_bytes_left = prd_entry.size;
	}


	while (prd_bytes_left > 0) {
	    uint_t bytes_to_write = 0;

	    if (drive->drive_type == BLOCK_DISK) {
		bytes_to_write = (prd_bytes_left > HD_SECTOR_SIZE) ? HD_SECTOR_SIZE : prd_bytes_left;


		if (ata_read(ide, channel, drive->data_buf, 1) == -1) {
		    PrintError(core->vm_info, core, "Failed to read next disk sector\n");
		    return -1;
		}
	    } else if (drive->drive_type == BLOCK_CDROM) {
		if (atapi_cmd_is_data_op(drive->cd_state.atapi_cmd)) {
		    bytes_to_write = (prd_bytes_left > ATAPI_BLOCK_SIZE) ? ATAPI_BLOCK_SIZE : prd_bytes_left;

		    if (atapi_read_chunk(ide, channel) == -1) {
			PrintError(core->vm_info, core, "Failed to read next disk sector\n");
			return -1;
		    }
		} else {
		    /*
		    PrintError(core->vm_info, core, "How does this work (ATAPI CMD=%x)???\n", drive->cd_state.atapi_cmd);
		    return -1;
		    */
		    int cmd_ret = 0;

		    //V3_Print(core->vm_info, core, "DMA of command packet\n");

		    bytes_to_write = (prd_bytes_left > bytes_left) ? bytes_left : prd_bytes_left;
		    prd_bytes_left = bytes_to_write;


		    // V3_Print(core->vm_info, core, "Writing ATAPI cmd OP DMA (cmd=%x) (len=%d)\n", drive->cd_state.atapi_cmd, prd_bytes_left);
		    cmd_ret = v3_write_gpa_memory(core, prd_entry.base_addr + prd_offset, 
						  bytes_to_write, drive->data_buf); 

		    if (cmd_ret!=bytes_to_write) { 
			PrintError(core->vm_info, core, "Failed to write data to memory\n");
			return -1;
		    }



		    bytes_to_write = 0;
		    prd_bytes_left = 0;
		    drive->transfer_index += bytes_to_write;

		    channel->status.busy = 0;
		    channel->status.ready = 1;
		    channel->status.data_req = 0;
		    channel->status.error = 0;
		    channel->status.seek_complete = 1;

		    channel->dma_status.active = 0;
		    channel->dma_status.err = 0;

		    ide_raise_irq(ide, channel);
		    
		    return 0;
		}
	    }

	    PrintDebug(core->vm_info, core, "Writing DMA data to guest Memory ptr=%p, len=%d\n", 
		       (void *)(addr_t)(prd_entry.base_addr + prd_offset), bytes_to_write);

	    drive->current_lba++;

	    ret = v3_write_gpa_memory(core, prd_entry.base_addr + prd_offset, bytes_to_write, drive->data_buf); 

	    if (ret != bytes_to_write) {
		PrintError(core->vm_info, core, "Failed to copy data into guest memory... (ret=%d)\n", ret);
		return -1;
	    }

	    PrintDebug(core->vm_info, core, "\t DMA ret=%d, (prd_bytes_left=%d) (bytes_left=%d)\n", ret, prd_bytes_left, bytes_left);

	    drive->transfer_index += ret;
	    prd_bytes_left -= ret;
	    prd_offset += ret;
	    bytes_left -= ret;
	}

	channel->dma_tbl_index++;

	if (drive->drive_type == BLOCK_DISK) {
	    if (drive->transfer_index % HD_SECTOR_SIZE) {
		PrintError(core->vm_info, core, "We currently don't handle sectors that span PRD descriptors\n");
		return -1;
	    }
	} else if (drive->drive_type == BLOCK_CDROM) {
	    if (atapi_cmd_is_data_op(drive->cd_state.atapi_cmd)) {
		if (drive->transfer_index % ATAPI_BLOCK_SIZE) {
		    PrintError(core->vm_info, core, "We currently don't handle ATAPI BLOCKS that span PRD descriptors\n");
		    PrintError(core->vm_info, core, "transfer_index=%llu, transfer_length=%llu\n", 
			       drive->transfer_index, drive->transfer_length);
		    return -1;
		}
	    }
	}


	if ((prd_entry.end_of_table == 1) && (bytes_left > 0)) {
	    PrintError(core->vm_info, core, "DMA table not large enough for data transfer...\n");
	    return -1;
	}
    }

    /*
      drive->irq_flags.io_dir = 1;
      drive->irq_flags.c_d = 1;
      drive->irq_flags.rel = 0;
    */


    // Update to the next PRD entry

    // set DMA status

    if (prd_entry.end_of_table) {
	channel->status.busy = 0;
	channel->status.ready = 1;
	channel->status.data_req = 0;
	channel->status.error = 0;
	channel->status.seek_complete = 1;

	channel->dma_status.active = 0;
	channel->dma_status.err = 0;
    }

    ide_raise_irq(ide, channel);

    return 0;
}


static int dma_write(struct guest_info * core, struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    // This is at top level scope to do the EOT test at the end
    struct ide_dma_prd prd_entry = {};
    uint_t bytes_left = drive->transfer_length;


    PrintDebug(core->vm_info, core, "DMA write from %d bytes\n", bytes_left);

    // Loop through disk data
    while (bytes_left > 0) {
	uint32_t prd_entry_addr = channel->dma_prd_addr + (sizeof(struct ide_dma_prd) * channel->dma_tbl_index);
	uint_t prd_bytes_left = 0;
	uint_t prd_offset = 0;
	int ret;
	
	PrintDebug(core->vm_info, core, "PRD Table address = %x\n", channel->dma_prd_addr);

	ret = v3_read_gpa_memory(core, prd_entry_addr, sizeof(struct ide_dma_prd), (void *)&prd_entry);

	if (ret != sizeof(struct ide_dma_prd)) {
	    PrintError(core->vm_info, core, "Could not read PRD\n");
	    return -1;
	}

	PrintDebug(core->vm_info, core, "PRD Addr: %x, PRD Len: %d, EOT: %d\n", 
		   prd_entry.base_addr, prd_entry.size, prd_entry.end_of_table);


	if (prd_entry.size == 0) {
	    // a size of 0 means 64k
	    prd_bytes_left = 0x10000;
	} else {
	    prd_bytes_left = prd_entry.size;
	}

	while (prd_bytes_left > 0) {
	    uint_t bytes_to_write = 0;


	    bytes_to_write = (prd_bytes_left > HD_SECTOR_SIZE) ? HD_SECTOR_SIZE : prd_bytes_left;


	    ret = v3_read_gpa_memory(core, prd_entry.base_addr + prd_offset, bytes_to_write, drive->data_buf);

	    if (ret != bytes_to_write) {
		PrintError(core->vm_info, core, "Faild to copy data from guest memory... (ret=%d)\n", ret);
		return -1;
	    }

	    PrintDebug(core->vm_info, core, "\t DMA ret=%d (prd_bytes_left=%d) (bytes_left=%d)\n", ret, prd_bytes_left, bytes_left);


	    if (ata_write(ide, channel, drive->data_buf, 1) == -1) {
		PrintError(core->vm_info, core, "Failed to write data to disk\n");
		return -1;
	    }
	    
	    drive->current_lba++;

	    drive->transfer_index += ret;
	    prd_bytes_left -= ret;
	    prd_offset += ret;
	    bytes_left -= ret;
	}

	channel->dma_tbl_index++;

	if (drive->transfer_index % HD_SECTOR_SIZE) {
	    PrintError(core->vm_info, core, "We currently don't handle sectors that span PRD descriptors\n");
	    return -1;
	}

	if ((prd_entry.end_of_table == 1) && (bytes_left > 0)) {
	    PrintError(core->vm_info, core, "DMA table not large enough for data transfer...\n");
	    PrintError(core->vm_info, core, "\t(bytes_left=%u) (transfer_length=%llu)...\n", 
		       bytes_left, drive->transfer_length);
	    PrintError(core->vm_info, core, "PRD Addr: %x, PRD Len: %d, EOT: %d\n", 
		       prd_entry.base_addr, prd_entry.size, prd_entry.end_of_table);

	    print_prd_table(ide, channel);
	    return -1;
	}
    }

    if (prd_entry.end_of_table) {
	channel->status.busy = 0;
	channel->status.ready = 1;
	channel->status.data_req = 0;
	channel->status.error = 0;
	channel->status.seek_complete = 1;

	channel->dma_status.active = 0;
	channel->dma_status.err = 0;
    }

    ide_raise_irq(ide, channel);

    return 0;
}



#define DMA_CMD_PORT      0x00
#define DMA_STATUS_PORT   0x02
#define DMA_PRD_PORT0     0x04
#define DMA_PRD_PORT1     0x05
#define DMA_PRD_PORT2     0x06
#define DMA_PRD_PORT3     0x07

#define DMA_CHANNEL_FLAG  0x08

/*
  Note that DMA model is as follows:

    1. Write the PRD pointer to the busmaster (DMA engine)
    2. Start the transfer on the device
    3. Tell the busmaster to start shoveling data (active DMA)
*/

static int write_dma_port(struct guest_info * core, ushort_t port, void * src, uint_t length, void * private_data) {
    struct ide_internal * ide = (struct ide_internal *)private_data;
    uint16_t port_offset = port & (DMA_CHANNEL_FLAG - 1);
    uint_t channel_flag = (port & DMA_CHANNEL_FLAG) >> 3;
    struct ide_channel * channel = &(ide->channels[channel_flag]);

    PrintDebug(core->vm_info, core, "IDE: Writing DMA Port %x (%s) (val=%x) (len=%d) (channel=%d)\n", 
	       port, dma_port_to_str(port_offset), *(uint32_t *)src, length, channel_flag);

    switch (port_offset) {
	case DMA_CMD_PORT:
	    channel->dma_cmd.val = *(uint8_t *)src;
	    
	    PrintDebug(core->vm_info, core, "IDE: dma command write:  0x%x\n", channel->dma_cmd.val);

	    if (channel->dma_cmd.start == 0) {
		channel->dma_tbl_index = 0;
	    } else {
		// Launch DMA operation, interrupt at end

		channel->dma_status.active = 1;

		if (channel->dma_cmd.read == 1) {
		    // DMA Read the whole thing - dma_read will raise irq
		    if (dma_read(core, ide, channel) == -1) {
			PrintError(core->vm_info, core, "Failed DMA Read\n");
			return -1;
		    }
		} else {
		    // DMA write the whole thing - dma_write will raiase irw
		    if (dma_write(core, ide, channel) == -1) {
			PrintError(core->vm_info, core, "Failed DMA Write\n");
			return -1;
		    }
		}
		
		// DMA complete
		// Note that guest cannot abort a DMA transfer
		channel->dma_cmd.start = 0;
	    }

	    break;
	    
	case DMA_STATUS_PORT: {
	    // This is intended to clear status

	    uint8_t val = *(uint8_t *)src;

	    if (length != 1) {
		PrintError(core->vm_info, core, "Invalid write length for DMA status port\n");
		return -1;
	    }

	    // but preserve certain bits
	    channel->dma_status.val = ((val & 0x60) | 
				       (channel->dma_status.val & 0x01) |
				       (channel->dma_status.val & ~val & 0x06));

	    break;
	}	    
	case DMA_PRD_PORT0:
	case DMA_PRD_PORT1:
	case DMA_PRD_PORT2:
	case DMA_PRD_PORT3: {
	    uint_t addr_index = port_offset & 0x3;
	    uint8_t * addr_buf = (uint8_t *)&(channel->dma_prd_addr);
	    int i = 0;

	    if (addr_index + length > 4) {
		PrintError(core->vm_info, core, "DMA Port space overrun port=%x len=%d\n", port_offset, length);
		return -1;
	    }

	    for (i = 0; i < length; i++) {
		addr_buf[addr_index + i] = *((uint8_t *)src + i);
	    }

	    PrintDebug(core->vm_info, core, "Writing PRD Port %x (val=%x)\n", port_offset, channel->dma_prd_addr);

	    break;
	}
	default:
	    PrintError(core->vm_info, core, "IDE: Invalid DMA Port (%d) (%s)\n", port, dma_port_to_str(port_offset));
	    break;
    }

    return length;
}


static int read_dma_port(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * private_data) {
    struct ide_internal * ide = (struct ide_internal *)private_data;
    uint16_t port_offset = port & (DMA_CHANNEL_FLAG - 1);
    uint_t channel_flag = (port & DMA_CHANNEL_FLAG) >> 3;
    struct ide_channel * channel = &(ide->channels[channel_flag]);

    PrintDebug(core->vm_info, core, "Reading DMA port %d (%x) (channel=%d)\n", port, port, channel_flag);

    if (port_offset + length > 16) {
	PrintError(core->vm_info, core, "DMA Port Read: Port overrun (port_offset=%d, length=%d)\n", port_offset, length);
	return -1;
    }

    memcpy(dst, channel->dma_ports + port_offset, length);
    
    PrintDebug(core->vm_info, core, "\tval=%x (len=%d)\n", *(uint32_t *)dst, length);

    return length;
}



static int write_cmd_port(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct ide_internal * ide = priv_data;
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    if (length != 1) {
	PrintError(core->vm_info, core, "Invalid Write Length on IDE command Port %x\n", port);
	return -1;
    }

    PrintDebug(core->vm_info, core, "IDE: Writing Command Port %x (%s) (val=%x)\n", port, io_port_to_str(port), *(uint8_t *)src);
    
    channel->cmd_reg = *(uint8_t *)src;
    
    switch (channel->cmd_reg) {

	case ATA_PIDENTIFY: // ATAPI Identify Device Packet (CDROM)
	    if (drive->drive_type != BLOCK_CDROM) {
		drive_reset(drive);

		// JRL: Should we abort here?
		ide_abort_command(ide, channel);
	    } else {
		
		atapi_identify_device(drive);
		
		channel->error_reg.val = 0;
		channel->status.val = 0x58; // ready, data_req, seek_complete
	    
		ide_raise_irq(ide, channel);
	    }
	    break;

	case ATA_IDENTIFY: // Identify Device
	    if (drive->drive_type != BLOCK_DISK) {
		drive_reset(drive);

		// JRL: Should we abort here?
		ide_abort_command(ide, channel);
	    } else {
		ata_identify_device(drive);

		channel->error_reg.val = 0;
		channel->status.val = 0x58;

		ide_raise_irq(ide, channel);
	    }
	    break;

	case ATA_PACKETCMD: // ATAPI Command Packet (CDROM)
	    if (drive->drive_type != BLOCK_CDROM) {
		ide_abort_command(ide, channel);
	    }
	    
	    drive->sector_count = 1;

	    channel->status.busy = 0;
	    channel->status.write_fault = 0;
	    channel->status.data_req = 1;
	    channel->status.error = 0;

	    // reset the data buffer...
	    drive->transfer_length = ATAPI_PACKET_SIZE;
	    drive->transfer_index = 0;

	    break;

	case ATA_READ:      // Read Sectors with Retry
	case ATA_READ_ONCE: // Read Sectors without Retry
	case ATA_MULTREAD:  // Read multiple sectors per ire
	case ATA_READ_EXT:  // Read Sectors Extended (LBA48)

	    if (channel->cmd_reg==ATA_MULTREAD) { 
		drive->hd_state.cur_sector_num = drive->hd_state.mult_sector_num;
	    } else {
		drive->hd_state.cur_sector_num = 1;
	    }

	    if (ata_read_sectors(ide, channel) == -1) {
		PrintError(core->vm_info, core, "Error reading sectors\n");
		ide_abort_command(ide,channel);
	    }
	    break;

	case ATA_WRITE:            // Write Sector with retry
	case ATA_WRITE_ONCE: 	   // Write Sector without retry
	case ATA_MULTWRITE:        // Write multiple sectors per irq
	case ATA_WRITE_EXT:        // Write Sectors Extended (LBA48)

	    if (channel->cmd_reg==ATA_MULTWRITE) { 
		drive->hd_state.cur_sector_num = drive->hd_state.mult_sector_num;
	    } else {
		drive->hd_state.cur_sector_num = 1;
	    }

	    if (ata_write_sectors(ide, channel) == -1) {
		PrintError(core->vm_info, core, "Error writing sectors\n");
		ide_abort_command(ide,channel);
	    }
	    break;

	case ATA_READDMA:            // Read DMA with retry
	case ATA_READDMA_ONCE:       // Read DMA without retry
	case ATA_READDMA_EXT:      { // Read DMA (LBA48)
	    uint64_t sect_cnt;

	    if (ata_get_lba_and_size(ide, channel, &(drive->current_lba), &sect_cnt) == -1) {
                PrintError(core->vm_info, core, "Error getting LBA for DMA READ\n");
		ide_abort_command(ide, channel);
		return length;
	    }
	    
	    drive->hd_state.cur_sector_num = 1;  // Not used for DMA
	    
	    drive->transfer_length = sect_cnt * HD_SECTOR_SIZE;
	    drive->transfer_index = 0;

	    // Now we wait for the transfer to be intiated by flipping the 
	    // bus-master start bit
	    break;
	}

	case ATA_WRITEDMA:        // Write DMA with retry
	case ATA_WRITEDMA_ONCE:   // Write DMA without retry
	case ATA_WRITEDMA_EXT:  { // Write DMA (LBA48)

	    uint64_t sect_cnt;

	    if (ata_get_lba_and_size(ide, channel, &(drive->current_lba),&sect_cnt) == -1) {
		PrintError(core->vm_info,core,"Cannot get lba\n");
		ide_abort_command(ide, channel);
		return length;
	    }

	    drive->hd_state.cur_sector_num = 1;  // Not used for DMA

	    drive->transfer_length = sect_cnt * HD_SECTOR_SIZE;
	    drive->transfer_index = 0;

	    // Now we wait for the transfer to be intiated by flipping the 
	    // bus-master start bit
	    break;
	}

	case ATA_STANDBYNOW1: // Standby Now 1
	case ATA_IDLEIMMEDIATE: // Set Idle Immediate
	case ATA_STANDBY: // Standby
	case ATA_SETIDLE1: // Set Idle 1
	case ATA_SLEEPNOW1: // Sleep Now 1
	case ATA_STANDBYNOW2: // Standby Now 2
	case ATA_IDLEIMMEDIATE2: // Idle Immediate (CFA)
	case ATA_STANDBY2: // Standby 2
	case ATA_SETIDLE2: // Set idle 2
	case ATA_SLEEPNOW2: // Sleep Now 2
	    channel->status.val = 0;
	    channel->status.ready = 1;
	    ide_raise_irq(ide, channel);
	    break;

	case ATA_SETFEATURES: // Set Features
	    // Prior to this the features register has been written to. 
	    // This command tells the drive to check if the new value is supported (the value is drive specific)
	    // Common is that bit0=DMA enable
	    // If valid the drive raises an interrupt, if not it aborts.

	    // Do some checking here...

	    channel->status.busy = 0;
	    channel->status.write_fault = 0;
	    channel->status.error = 0;
	    channel->status.ready = 1;
	    channel->status.seek_complete = 1;
	    
	    ide_raise_irq(ide, channel);
	    break;

	case ATA_SPECIFY:  // Initialize Drive Parameters
	case ATA_RECAL:  // recalibrate?
	    channel->status.error = 0;
	    channel->status.ready = 1;
	    channel->status.seek_complete = 1;
	    ide_raise_irq(ide, channel);
	    break;

	case ATA_SETMULT: { // Set multiple mode (IDE Block mode) 
	    // This makes the drive transfer multiple sectors before generating an interrupt

	    if (drive->sector_count == 0) {
		PrintError(core->vm_info,core,"Attempt to set multiple to zero\n");
		drive->hd_state.mult_sector_num= 1;
		ide_abort_command(ide,channel);
		break;
	    } else {
		drive->hd_state.mult_sector_num = drive->sector_count;
	    }

	    channel->status.ready = 1;
	    channel->status.error = 0;

	    ide_raise_irq(ide, channel);

	    break;
	}

	case ATA_DEVICE_RESET: // Reset Device
	    drive_reset(drive);
    	    channel->error_reg.val = 0x01;
    	    channel->status.busy = 0;
    	    channel->status.ready = 1;
    	    channel->status.seek_complete = 1;
    	    channel->status.write_fault = 0;
    	    channel->status.error = 0;
    	    break;

	case ATA_CHECKPOWERMODE1: // Check power mode
    	    drive->sector_count = 0xff; /* 0x00=standby, 0x80=idle, 0xff=active or idle */
    	    channel->status.busy = 0;
    	    channel->status.ready = 1;
    	    channel->status.write_fault = 0;
    	    channel->status.data_req = 0;
    	    channel->status.error = 0;
    	    break;

	default:
	    PrintError(core->vm_info, core, "Unimplemented IDE command (%x)\n", channel->cmd_reg);
	    ide_abort_command(ide, channel);
	    break;
    }

    return length;
}




static int read_hd_data(uint8_t * dst, uint64_t length, struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    uint64_t data_offset = drive->transfer_index % HD_SECTOR_SIZE;


    PrintDebug(VM_NONE,VCORE_NONE, "Read HD data:  transfer_index %llu transfer length %llu current sector numer %llu\n",
	       drive->transfer_index, drive->transfer_length, 
	       drive->hd_state.cur_sector_num);

    if (drive->transfer_index >= drive->transfer_length && drive->transfer_index>=DATA_BUFFER_SIZE) {
	PrintError(VM_NONE, VCORE_NONE, "Buffer overrun... (xfer_len=%llu) (cur_idx=%llu) (post_idx=%llu)\n",
		   drive->transfer_length, drive->transfer_index,
		   drive->transfer_index + length);
	return -1;
    }


    if (data_offset + length > HD_SECTOR_SIZE) { 
       PrintError(VM_NONE,VCORE_NONE,"Read spans sectors (data_offset=%llu length=%llu)!\n",data_offset,length);
    }
   
    // For index==0, the read has been done in ata_read_sectors
    if ((data_offset == 0) && (drive->transfer_index > 0)) {
	// advance to next sector and read it
	
	drive->current_lba++;

	if (ata_read(ide, channel, drive->data_buf, 1) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not read next disk sector\n");
	    return -1;
	}
    }

    /*
      PrintDebug(VM_NONE, VCORE_NONE, "Reading HD Data (Val=%x), (len=%d) (offset=%d)\n", 
      *(uint32_t *)(drive->data_buf + data_offset), 
      length, data_offset);
    */
    memcpy(dst, drive->data_buf + data_offset, length);

    drive->transfer_index += length;


    /* This is the trigger for interrupt injection.
     * For read single sector commands we interrupt after every sector
     * For multi sector reads we interrupt only at end of the cluster size (mult_sector_num)
     * cur_sector_num is configured depending on the operation we are currently running
     * We also trigger an interrupt if this is the last byte to transfer, regardless of sector count
     */
    if (((drive->transfer_index % (HD_SECTOR_SIZE * drive->hd_state.cur_sector_num)) == 0) || 
	(drive->transfer_index == drive->transfer_length)) {
	if (drive->transfer_index < drive->transfer_length) {
	    // An increment is complete, but there is still more data to be transferred...
	    PrintDebug(VM_NONE, VCORE_NONE, "Increment Complete, still transferring more sectors\n");
	    channel->status.data_req = 1;
	} else {
	    PrintDebug(VM_NONE, VCORE_NONE, "Final Sector Transferred\n");
	    // This was the final read of the request
	    channel->status.data_req = 0;
	}

	channel->status.ready = 1;
	channel->status.busy = 0;

	ide_raise_irq(ide, channel);
    }


    return length;
}

static int write_hd_data(uint8_t * src, uint64_t length, struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    uint64_t data_offset = drive->transfer_index % HD_SECTOR_SIZE;


    PrintDebug(VM_NONE,VCORE_NONE, "Write HD data:  transfer_index %llu transfer length %llu current sector numer %llu\n",
	       drive->transfer_index, drive->transfer_length, 
	       drive->hd_state.cur_sector_num);

    if (drive->transfer_index >= drive->transfer_length) {
	PrintError(VM_NONE, VCORE_NONE, "Buffer overrun... (xfer_len=%llu) (cur_idx=%llu) (post_idx=%llu)\n",
		   drive->transfer_length, drive->transfer_index,
		   drive->transfer_index + length);
	return -1;
    }

    if (data_offset + length > HD_SECTOR_SIZE) { 
       PrintError(VM_NONE,VCORE_NONE,"Write spans sectors (data_offset=%llu length=%llu)!\n",data_offset,length);
    }

    // Copy data into our buffer - there will be room due to
    // (a) the ata_write test below is flushing sectors
    // (b) if we somehow get a sector-stradling write (an error), this will
    //     be OK since the buffer itself is >1 sector in memory
    memcpy(drive->data_buf + data_offset, src, length);

    drive->transfer_index += length;

    if ((data_offset+length) >= HD_SECTOR_SIZE) {
	// Write out the sector we just finished
	if (ata_write(ide, channel, drive->data_buf, 1) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not write next disk sector\n");
	    return -1;
	}

	// go onto next sector
	drive->current_lba++;
    }

    /* This is the trigger for interrupt injection.
     * For write single sector commands we interrupt after every sector
     * For multi sector reads we interrupt only at end of the cluster size (mult_sector_num)
     * cur_sector_num is configured depending on the operation we are currently running
     * We also trigger an interrupt if this is the last byte to transfer, regardless of sector count
     */
    if (((drive->transfer_index % (HD_SECTOR_SIZE * drive->hd_state.cur_sector_num)) == 0) || 
	(drive->transfer_index == drive->transfer_length)) {
	if (drive->transfer_index < drive->transfer_length) {
	    // An increment is complete, but there is still more data to be transferred...
	    PrintDebug(VM_NONE, VCORE_NONE, "Increment Complete, still transferring more sectors\n");
	    channel->status.data_req = 1;
	} else {
	    PrintDebug(VM_NONE, VCORE_NONE, "Final Sector Transferred\n");
	    // This was the final read of the request
	    channel->status.data_req = 0;
	}

	channel->status.ready = 1;
	channel->status.busy = 0;

	ide_raise_irq(ide, channel);
    }

    return length;
}



static int read_cd_data(uint8_t * dst, uint64_t length, struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    uint64_t data_offset = drive->transfer_index % ATAPI_BLOCK_SIZE;
    //  int req_offset = drive->transfer_index % drive->req_len;
    
    if (drive->cd_state.atapi_cmd != 0x28) {
        PrintDebug(VM_NONE, VCORE_NONE, "IDE: Reading CD Data (len=%llu) (req_len=%u)\n", length, drive->req_len);
	PrintDebug(VM_NONE, VCORE_NONE, "IDE: transfer len=%llu, transfer idx=%llu\n", drive->transfer_length, drive->transfer_index);
    }

    

    if (drive->transfer_index >= drive->transfer_length && drive->transfer_index>=DATA_BUFFER_SIZE) {
	PrintError(VM_NONE, VCORE_NONE, "Buffer Overrun... (xfer_len=%llu) (cur_idx=%llu) (post_idx=%llu)\n", 
		   drive->transfer_length, drive->transfer_index, 
		   drive->transfer_index + length);
	return -1;
    }

    
    if ((data_offset == 0) && (drive->transfer_index > 0)) {
	if (atapi_update_data_buf(ide, channel) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not update CDROM data buffer\n");
	    return -1;
	}
    }

    memcpy(dst, drive->data_buf + data_offset, length);
    
    drive->transfer_index += length;


    // Should the req_offset be recalculated here?????
    if (/*(req_offset == 0) &&*/ (drive->transfer_index > 0)) {
	if (drive->transfer_index < drive->transfer_length) {
	    // An increment is complete, but there is still more data to be transferred...
	    
	    channel->status.data_req = 1;

	    drive->irq_flags.c_d = 0;

	    // Update the request length in the cylinder regs
	    if (atapi_update_req_len(ide, channel, drive->transfer_length - drive->transfer_index) == -1) {
		PrintError(VM_NONE, VCORE_NONE, "Could not update request length after completed increment\n");
		return -1;
	    }
	} else {
	    // This was the final read of the request

	    drive->req_len = 0;
	    channel->status.data_req = 0;
	    channel->status.ready = 1;
	    
	    drive->irq_flags.c_d = 1;
	    drive->irq_flags.rel = 0;
	}

	drive->irq_flags.io_dir = 1;
	channel->status.busy = 0;

	ide_raise_irq(ide, channel);
    }

    return length;
}


static int read_drive_id( uint8_t * dst, uint_t length, struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    channel->status.busy = 0;
    channel->status.ready = 1;
    channel->status.write_fault = 0;
    channel->status.seek_complete = 1;
    channel->status.corrected = 0;
    channel->status.error = 0;
		
    
    memcpy(dst, drive->data_buf + drive->transfer_index, length);
    drive->transfer_index += length;
    
    if (drive->transfer_index >= drive->transfer_length) {
	channel->status.data_req = 0;
    }
    
    return length;
}



static int read_data_port(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct ide_internal * ide = priv_data;
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    //PrintDebug(core->vm_info, core, "IDE: Reading Data Port %x (len=%d)\n", port, length);

    if ((channel->cmd_reg == ATA_IDENTIFY) ||
	(channel->cmd_reg == ATA_PIDENTIFY)) {
	return read_drive_id((uint8_t *)dst, length, ide, channel);
    }

    if (drive->drive_type == BLOCK_CDROM) {
	if (read_cd_data((uint8_t *)dst, length, ide, channel) == -1) {
	    PrintError(core->vm_info, core, "IDE: Could not read CD Data (atapi cmd=%x)\n", drive->cd_state.atapi_cmd);
	    return -1;
	}
    } else if (drive->drive_type == BLOCK_DISK) {
	if (read_hd_data((uint8_t *)dst, length, ide, channel) == -1) {
	    PrintError(core->vm_info, core, "IDE: Could not read HD Data\n");
	    return -1;
	}
    } else {
	memset((uint8_t *)dst, 0, length);
    }

    return length;
}

// For the write side, we care both about
// direct PIO writes to a drive as well as 
// writes that pass a packet through to an CD
static int write_data_port(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct ide_internal * ide = priv_data;
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);

    PrintDebug(core->vm_info, core, "IDE: Writing Data Port %x (val=%x, len=%d)\n", 
            port, *(uint32_t *)src, length);

    if (drive->drive_type == BLOCK_CDROM) {
	if (channel->cmd_reg == ATA_PACKETCMD) { 
	    // short command packet - no check for space... 
	    memcpy(drive->data_buf + drive->transfer_index, src, length);
	    drive->transfer_index += length;
	    if (drive->transfer_index >= drive->transfer_length) {
		if (atapi_handle_packet(core, ide, channel) == -1) {
		    PrintError(core->vm_info, core, "Error handling ATAPI packet\n");
		    return -1;
		}
	    }
	} else {
	    PrintError(core->vm_info,core,"Unknown command %x on CD ROM\n",channel->cmd_reg);
	    return -1;
	}
    } else if (drive->drive_type == BLOCK_DISK) {
	if (write_hd_data((uint8_t *)src, length, ide, channel) == -1) {
	    PrintError(core->vm_info, core, "IDE: Could not write HD Data\n");
	    return -1;
	}
    } else {
	// nothing ... do not support writable cd
    }

    return length;
}

static int write_port_std(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct ide_internal * ide = priv_data;
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);
	    
    if (length != 1) {
	PrintError(core->vm_info, core, "Invalid Write length on IDE port %x\n", port);
	return -1;
    }

    PrintDebug(core->vm_info, core, "IDE: Writing Standard Port %x (%s) (val=%x)\n", port, io_port_to_str(port), *(uint8_t *)src);

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
	    // update CHS and LBA28 state
	    channel->drives[0].sector_count = *(uint8_t *)src;
	    channel->drives[1].sector_count = *(uint8_t *)src;

	    // update LBA48 state
	    if (is_lba48(channel)) {
		uint16_t val = *(uint8_t*)src; // top bits zero;
		if (!channel->drives[0].lba48.sector_count_state) { 
		    channel->drives[0].lba48.sector_count = val<<8;
		} else {
		    channel->drives[0].lba48.sector_count |= val;
		}
		channel->drives[0].lba48.sector_count_state ^= 1;
		if (!channel->drives[1].lba48.sector_count_state) { 
		    channel->drives[1].lba48.sector_count = val<<8;
		} else {
		    channel->drives[1].lba48.sector_count |= val;
		}
		channel->drives[0].lba48.sector_count_state ^= 1;
	    }
	    
	    break;

	case PRI_SECT_NUM_PORT:
	case SEC_SECT_NUM_PORT:
	    // update CHS and LBA28 state
	    channel->drives[0].sector_num = *(uint8_t *)src;
	    channel->drives[1].sector_num = *(uint8_t *)src;

	    // update LBA48 state
	    if (is_lba48(channel)) {
		uint64_t val = *(uint8_t *)src; // lob off top 7 bytes;
		if (!channel->drives[0].lba48.lba41_state) { 
		    channel->drives[0].lba48.lba |= val<<24; 
		} else {
		    channel->drives[0].lba48.lba |= val;
		}
		channel->drives[0].lba48.lba41_state ^= 1;
		if (!channel->drives[1].lba48.lba41_state) { 
		    channel->drives[1].lba48.lba |= val<<24; 
		} else {
		    channel->drives[1].lba48.lba |= val;
		}
		channel->drives[1].lba48.lba41_state ^= 1;
	    }

	    break;
	case PRI_CYL_LOW_PORT:
	case SEC_CYL_LOW_PORT:
	    // update CHS and LBA28 state
	    channel->drives[0].cylinder_low = *(uint8_t *)src;
	    channel->drives[1].cylinder_low = *(uint8_t *)src;

	    // update LBA48 state
	    if (is_lba48(channel)) {
		uint64_t val = *(uint8_t *)src; // lob off top 7 bytes;
		if (!channel->drives[0].lba48.lba52_state) { 
		    channel->drives[0].lba48.lba |= val<<32; 
		} else {
		    channel->drives[0].lba48.lba |= val<<8;
		}
		channel->drives[0].lba48.lba52_state ^= 1;
		if (!channel->drives[1].lba48.lba52_state) { 
		    channel->drives[1].lba48.lba |= val<<32; 
		} else {
		    channel->drives[1].lba48.lba |= val<<8;
		}
		channel->drives[1].lba48.lba52_state ^= 1;
	    }

	    break;

	case PRI_CYL_HIGH_PORT:
	case SEC_CYL_HIGH_PORT:
	    // update CHS and LBA28 state
	    channel->drives[0].cylinder_high = *(uint8_t *)src;
	    channel->drives[1].cylinder_high = *(uint8_t *)src;

	    // update LBA48 state
	    if (is_lba48(channel)) {
		uint64_t val = *(uint8_t *)src; // lob off top 7 bytes;
		if (!channel->drives[0].lba48.lba63_state) { 
		    channel->drives[0].lba48.lba |= val<<40; 
		} else {
		    channel->drives[0].lba48.lba |= val<<16;
		}
		channel->drives[0].lba48.lba63_state ^= 1;
		if (!channel->drives[1].lba48.lba63_state) { 
		    channel->drives[1].lba48.lba |= val<<40; 
		} else {
		    channel->drives[1].lba48.lba |= val<<16;
		}
		channel->drives[1].lba48.lba63_state ^= 1;
	    }

	    break;

	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT: {
	    struct ide_drive_head_reg nh, oh;

	    oh.val = channel->drive_head.val;
	    channel->drive_head.val = nh.val = *(uint8_t *)src;

	    // has LBA flipped?
	    if ((oh.val & 0xe0) != (nh.val & 0xe0)) {
		// reset LBA48 state
		channel->drives[0].lba48.sector_count_state=0;
		channel->drives[0].lba48.lba41_state=0;
		channel->drives[0].lba48.lba52_state=0;
		channel->drives[0].lba48.lba63_state=0;
		channel->drives[1].lba48.sector_count_state=0;
		channel->drives[1].lba48.lba41_state=0;
		channel->drives[1].lba48.lba52_state=0;
		channel->drives[1].lba48.lba63_state=0;
	    }
	    

	    drive = get_selected_drive(channel);

	    // Selecting a non-present device is a no-no
	    if (drive->drive_type == BLOCK_NONE) {
		PrintDebug(core->vm_info, core, "Attempting to select a non-present drive\n");
		channel->error_reg.abort = 1;
		channel->status.error = 1;
	    } else {
		channel->status.busy = 0;
		channel->status.ready = 1;
		channel->status.data_req = 0;
		channel->status.error = 0;
		channel->status.seek_complete = 1;
		
		channel->dma_status.active = 0;
		channel->dma_status.err = 0;
	    }

	    break;
	}
	default:
	    PrintError(core->vm_info, core, "IDE: Write to unknown Port %x\n", port);
	    return -1;
    }
    return length;
}


static int read_port_std(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct ide_internal * ide = priv_data;
    struct ide_channel * channel = get_selected_channel(ide, port);
    struct ide_drive * drive = get_selected_drive(channel);
    
    if (length != 1) {
	PrintError(core->vm_info, core, "Invalid Read length on IDE port %x\n", port);
	return -1;
    }
    
    PrintDebug(core->vm_info, core, "IDE: Reading Standard Port %x (%s)\n", port, io_port_to_str(port));

    if ((port == PRI_ADDR_REG_PORT) ||
	(port == SEC_ADDR_REG_PORT)) {
	// unused, return 0xff
	*(uint8_t *)dst = 0xff;
	return length;
    }


    // if no drive is present just return 0 + reserved bits
    if (drive->drive_type == BLOCK_NONE) {
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
	    PrintError(core->vm_info, core, "Invalid Port: %x\n", port);
	    return -1;
    }

    PrintDebug(core->vm_info, core, "\tVal=%x\n", *(uint8_t *)dst);

    return length;
}



static void init_drive(struct ide_drive * drive) {

    drive->sector_count = 0x01;
    drive->sector_num = 0x01;
    drive->cylinder = 0x0000;

    drive->drive_type = BLOCK_NONE;

    memset(drive->model, 0, sizeof(drive->model));

    drive->transfer_index = 0;
    drive->transfer_length = 0;
    memset(drive->data_buf, 0, sizeof(drive->data_buf));

    drive->num_cylinders = 0;
    drive->num_heads = 0;
    drive->num_sectors = 0;
    

    drive->private_data = NULL;
    drive->ops = NULL;
}

static void init_channel(struct ide_channel * channel) {
    int i = 0;

    channel->error_reg.val = 0x01;

    //** channel->features = 0x0;

    channel->drive_head.val = 0x00;
    channel->status.val = 0x00;
    channel->cmd_reg = 0x00;
    channel->ctrl_reg.val = 0x08;

    channel->dma_cmd.val = 0;
    channel->dma_status.val = 0;
    channel->dma_prd_addr = 0;
    channel->dma_tbl_index = 0;

    for (i = 0; i < 2; i++) {
	init_drive(&(channel->drives[i]));
    }

}


static int pci_config_update(struct pci_device * pci_dev, uint32_t reg_num, void * src, uint_t length, void * private_data) {
    PrintDebug(VM_NONE, VCORE_NONE, "PCI Config Update\n");
    /*
    struct ide_internal * ide = (struct ide_internal *)(private_data);

    PrintDebug(VM_NONE, VCORE_NONE, info, "\t\tInterupt register (Dev=%s), irq=%d\n", ide->ide_pci->name, ide->ide_pci->config_header.intr_line);
    */

    return 0;
}

static int init_ide_state(struct ide_internal * ide) {

    /* 
     * Check if the PIIX 3 actually represents both IDE channels in a single PCI entry 
     */

    init_channel(&(ide->channels[0]));
    ide->channels[0].irq = PRI_DEFAULT_IRQ ;

    init_channel(&(ide->channels[1]));
    ide->channels[1].irq = SEC_DEFAULT_IRQ ;


    return 0;
}




static int ide_free(struct ide_internal * ide) {

    // deregister from PCI?

    V3_Free(ide);

    return 0;
}

#ifdef V3_CONFIG_CHECKPOINT

#include <palacios/vmm_sprintf.h>

static int ide_save_extended(struct v3_chkpt *chkpt, char *id, void * private_data) {
    struct ide_internal * ide = (struct ide_internal *)private_data;
    struct v3_chkpt_ctx *ctx=0;
    int ch_num = 0;
    int drive_num = 0;
    char buf[128];
    

    ctx=v3_chkpt_open_ctx(chkpt,id);
    
    if (!ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to open context for save\n");
      goto savefailout;
    }

    // nothing saved yet
    
    v3_chkpt_close_ctx(ctx);ctx=0;
   

    for (ch_num = 0; ch_num < 2; ch_num++) {
	struct ide_channel * ch = &(ide->channels[ch_num]);

	snprintf(buf, 128, "%s-%d", id, ch_num);

	ctx = v3_chkpt_open_ctx(chkpt, buf);
	
	if (!ctx) { 
	  PrintError(VM_NONE, VCORE_NONE, "Unable to open context to save channel %d\n",ch_num);
	  goto savefailout;
	}

	V3_CHKPT_SAVE(ctx, "ERROR", ch->error_reg.val, savefailout);
	V3_CHKPT_SAVE(ctx, "FEATURES", ch->features.val, savefailout);
	V3_CHKPT_SAVE(ctx, "DRIVE_HEAD", ch->drive_head.val, savefailout);
	V3_CHKPT_SAVE(ctx, "STATUS", ch->status.val, savefailout);
	V3_CHKPT_SAVE(ctx, "CMD_REG", ch->cmd_reg, savefailout);
	V3_CHKPT_SAVE(ctx, "CTRL_REG", ch->ctrl_reg.val, savefailout);
	V3_CHKPT_SAVE(ctx, "DMA_CMD", ch->dma_cmd.val, savefailout);
	V3_CHKPT_SAVE(ctx, "DMA_STATUS", ch->dma_status.val, savefailout);
	V3_CHKPT_SAVE(ctx, "PRD_ADDR", ch->dma_prd_addr, savefailout);
	V3_CHKPT_SAVE(ctx, "DMA_TBL_IDX", ch->dma_tbl_index, savefailout);



	v3_chkpt_close_ctx(ctx); ctx=0;

	for (drive_num = 0; drive_num < 2; drive_num++) {
	    struct ide_drive * drive = &(ch->drives[drive_num]);
	    
	    snprintf(buf, 128, "%s-%d-%d", id, ch_num, drive_num);

	    ctx = v3_chkpt_open_ctx(chkpt, buf);
	    
	    if (!ctx) { 
	      PrintError(VM_NONE, VCORE_NONE, "Unable to open context to save drive %d\n",drive_num);
	      goto savefailout;
	    }

	    V3_CHKPT_SAVE(ctx, "DRIVE_TYPE", drive->drive_type, savefailout);
	    V3_CHKPT_SAVE(ctx, "SECTOR_COUNT", drive->sector_count, savefailout);
	    V3_CHKPT_SAVE(ctx, "SECTOR_NUM", drive->sector_num, savefailout);
	    V3_CHKPT_SAVE(ctx, "CYLINDER", drive->cylinder,savefailout);

	    V3_CHKPT_SAVE(ctx, "CURRENT_LBA", drive->current_lba, savefailout);
	    V3_CHKPT_SAVE(ctx, "TRANSFER_LENGTH", drive->transfer_length, savefailout);
	    V3_CHKPT_SAVE(ctx, "TRANSFER_INDEX", drive->transfer_index, savefailout);

	    V3_CHKPT_SAVE(ctx, "DATA_BUF",  drive->data_buf, savefailout);


	    /* For now we'll just pack the type specific data at the end... */
	    /* We should probably add a new context here in the future... */
	    if (drive->drive_type == BLOCK_CDROM) {
	      V3_CHKPT_SAVE(ctx, "ATAPI_SENSE_DATA", drive->cd_state.sense.buf, savefailout);
	      V3_CHKPT_SAVE(ctx, "ATAPI_CMD", drive->cd_state.atapi_cmd, savefailout);
	      V3_CHKPT_SAVE(ctx, "ATAPI_ERR_RECOVERY", drive->cd_state.err_recovery.buf, savefailout);
	    } else if (drive->drive_type == BLOCK_DISK) {
	      V3_CHKPT_SAVE(ctx, "ACCESSED", drive->hd_state.accessed, savefailout);
	      V3_CHKPT_SAVE(ctx, "MULT_SECT_NUM", drive->hd_state.mult_sector_num, savefailout);
	      V3_CHKPT_SAVE(ctx, "CUR_SECT_NUM", drive->hd_state.cur_sector_num, savefailout);
	    } else if (drive->drive_type == BLOCK_NONE) { 
	      // no drive connected, so no data
	    } else {
	      PrintError(VM_NONE, VCORE_NONE, "Invalid drive type %d\n",drive->drive_type);
	      goto savefailout;
	    }

	    V3_CHKPT_SAVE(ctx, "LBA48_LBA", drive->lba48.lba, savefailout);
	    V3_CHKPT_SAVE(ctx, "LBA48_SECTOR_COUNT", drive->lba48.sector_count, savefailout);
	    V3_CHKPT_SAVE(ctx, "LBA48_SECTOR_COUNT_STATE", drive->lba48.sector_count_state, savefailout);
	    V3_CHKPT_SAVE(ctx, "LBA48_LBA41_STATE", drive->lba48.lba41_state, savefailout);
	    V3_CHKPT_SAVE(ctx, "LBA48_LBA52_STATE", drive->lba48.lba52_state, savefailout);
	    V3_CHKPT_SAVE(ctx, "LBA48_LBA63_STATE", drive->lba48.lba63_state, savefailout);
	    
	    v3_chkpt_close_ctx(ctx); ctx=0;
	}
    }

// goodout:
    return 0;

 savefailout:
    PrintError(VM_NONE, VCORE_NONE, "Failed to save IDE\n");
    if (ctx) {v3_chkpt_close_ctx(ctx); }
    return -1;
}



static int ide_load_extended(struct v3_chkpt *chkpt, char *id, void * private_data) {
    struct ide_internal * ide = (struct ide_internal *)private_data;
    struct v3_chkpt_ctx *ctx=0;
    int ch_num = 0;
    int drive_num = 0;
    char buf[128];
    
    ctx=v3_chkpt_open_ctx(chkpt,id);
    
    if (!ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to open context for load\n");
      goto loadfailout;
    }

    // nothing saved yet
    
    v3_chkpt_close_ctx(ctx);ctx=0;
   

    for (ch_num = 0; ch_num < 2; ch_num++) {
	struct ide_channel * ch = &(ide->channels[ch_num]);

	snprintf(buf, 128, "%s-%d", id, ch_num);

	ctx = v3_chkpt_open_ctx(chkpt, buf);
	
	if (!ctx) { 
	  PrintError(VM_NONE, VCORE_NONE, "Unable to open context to load channel %d\n",ch_num);
	  goto loadfailout;
	}

	V3_CHKPT_LOAD(ctx, "ERROR", ch->error_reg.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "FEATURES", ch->features.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "DRIVE_HEAD", ch->drive_head.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "STATUS", ch->status.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "CMD_REG", ch->cmd_reg, loadfailout);
	V3_CHKPT_LOAD(ctx, "CTRL_REG", ch->ctrl_reg.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "DMA_CMD", ch->dma_cmd.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "DMA_STATUS", ch->dma_status.val, loadfailout);
	V3_CHKPT_LOAD(ctx, "PRD_ADDR", ch->dma_prd_addr, loadfailout);
	V3_CHKPT_LOAD(ctx, "DMA_TBL_IDX", ch->dma_tbl_index, loadfailout);

	v3_chkpt_close_ctx(ctx); ctx=0;

	for (drive_num = 0; drive_num < 2; drive_num++) {
	    struct ide_drive * drive = &(ch->drives[drive_num]);
	    
	    snprintf(buf, 128, "%s-%d-%d", id, ch_num, drive_num);

	    ctx = v3_chkpt_open_ctx(chkpt, buf);
	    
	    if (!ctx) { 
	      PrintError(VM_NONE, VCORE_NONE, "Unable to open context to load drive %d\n",drive_num);
	      goto loadfailout;
	    }

	    V3_CHKPT_LOAD(ctx, "DRIVE_TYPE", drive->drive_type, loadfailout);
	    V3_CHKPT_LOAD(ctx, "SECTOR_COUNT", drive->sector_count, loadfailout);
	    V3_CHKPT_LOAD(ctx, "SECTOR_NUM", drive->sector_num, loadfailout);
	    V3_CHKPT_LOAD(ctx, "CYLINDER", drive->cylinder,loadfailout);

	    V3_CHKPT_LOAD(ctx, "CURRENT_LBA", drive->current_lba, loadfailout);
	    V3_CHKPT_LOAD(ctx, "TRANSFER_LENGTH", drive->transfer_length, loadfailout);
	    V3_CHKPT_LOAD(ctx, "TRANSFER_INDEX", drive->transfer_index, loadfailout);

	    V3_CHKPT_LOAD(ctx, "DATA_BUF",  drive->data_buf, loadfailout);

	    
	    /* For now we'll just pack the type specific data at the end... */
	    /* We should probably add a new context here in the future... */
	    if (drive->drive_type == BLOCK_CDROM) {
	      V3_CHKPT_LOAD(ctx, "ATAPI_SENSE_DATA", drive->cd_state.sense.buf, loadfailout);
	      V3_CHKPT_LOAD(ctx, "ATAPI_CMD", drive->cd_state.atapi_cmd, loadfailout);
	      V3_CHKPT_LOAD(ctx, "ATAPI_ERR_RECOVERY", drive->cd_state.err_recovery.buf, loadfailout);
	    } else if (drive->drive_type == BLOCK_DISK) {
	      V3_CHKPT_LOAD(ctx, "ACCESSED", drive->hd_state.accessed, loadfailout);
	      V3_CHKPT_LOAD(ctx, "MULT_SECT_NUM", drive->hd_state.mult_sector_num, loadfailout);
	      V3_CHKPT_LOAD(ctx, "CUR_SECT_NUM", drive->hd_state.cur_sector_num, loadfailout);
	    } else if (drive->drive_type == BLOCK_NONE) { 
	      // no drive connected, so no data
	    } else {
	      PrintError(VM_NONE, VCORE_NONE, "Invalid drive type %d\n",drive->drive_type);
	      goto loadfailout;
	    }

	    V3_CHKPT_LOAD(ctx, "LBA48_LBA", drive->lba48.lba, loadfailout);
	    V3_CHKPT_LOAD(ctx, "LBA48_SECTOR_COUNT", drive->lba48.sector_count, loadfailout);
	    V3_CHKPT_LOAD(ctx, "LBA48_SECTOR_COUNT_STATE", drive->lba48.sector_count_state, loadfailout);
	    V3_CHKPT_LOAD(ctx, "LBA48_LBA41_STATE", drive->lba48.lba41_state, loadfailout);
	    V3_CHKPT_LOAD(ctx, "LBA48_LBA52_STATE", drive->lba48.lba52_state, loadfailout);
	    V3_CHKPT_LOAD(ctx, "LBA48_LBA63_STATE", drive->lba48.lba63_state, loadfailout);
	    
	}
    }
// goodout:
    return 0;

 loadfailout:
    PrintError(VM_NONE, VCORE_NONE, "Failed to load IDE\n");
    if (ctx) {v3_chkpt_close_ctx(ctx); }
    return -1;

}



#endif


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))ide_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save_extended = ide_save_extended,
    .load_extended = ide_load_extended
#endif
};




static int connect_fn(struct v3_vm_info * vm, 
		      void * frontend_data, 
		      struct v3_dev_blk_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {
    struct ide_internal * ide  = (struct ide_internal *)(frontend_data);  
    struct ide_channel * channel = NULL;
    struct ide_drive * drive = NULL;

    char * bus_str = v3_cfg_val(cfg, "bus_num");
    char * drive_str = v3_cfg_val(cfg, "drive_num");
    char * type_str = v3_cfg_val(cfg, "type");
    char * model_str = v3_cfg_val(cfg, "model");
    uint_t bus_num = 0;
    uint_t drive_num = 0;


    if ((!type_str) || (!drive_str) || (!bus_str)) {
	PrintError(vm, VCORE_NONE, "Incomplete IDE Configuration\n");
	return -1;
    }

    bus_num = atoi(bus_str);
    drive_num = atoi(drive_str);

    channel = &(ide->channels[bus_num]);
    drive = &(channel->drives[drive_num]);

    if (drive->drive_type != BLOCK_NONE) {
	PrintError(vm, VCORE_NONE, "Device slot (bus=%d, drive=%d) already occupied\n", bus_num, drive_num);
	return -1;
    }

    if (model_str != NULL) {
	strncpy(drive->model, model_str, sizeof(drive->model));
	drive->model[sizeof(drive->model)-1] = 0;
    }

    if (strcasecmp(type_str, "cdrom") == 0) {
	drive->drive_type = BLOCK_CDROM;

	while (strlen((char *)(drive->model)) < 40) {
	    strcat((char*)(drive->model), " ");
	}

    } else if (strcasecmp(type_str, "hd") == 0) {
	drive->drive_type = BLOCK_DISK;

	drive->hd_state.accessed = 0;
	drive->hd_state.mult_sector_num = 1;

	drive->num_sectors = 63;
	drive->num_heads = 16;
	drive->num_cylinders = (ops->get_capacity(private_data) / HD_SECTOR_SIZE) / (drive->num_sectors * drive->num_heads);
    } else {
	PrintError(vm, VCORE_NONE, "invalid IDE drive type\n");
	return -1;
    }
 
    drive->ops = ops;

    if (ide->ide_pci) {
	// Hardcode this for now, but its not a good idea....
	ide->ide_pci->config_space[0x41 + (bus_num * 2)] = 0x80;
    }
 
    drive->private_data = private_data;

    return 0;
}




static int ide_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct ide_internal * ide  = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    PrintDebug(vm, VCORE_NONE, "IDE: Initializing IDE\n");

    ide = (struct ide_internal *)V3_Malloc(sizeof(struct ide_internal));

    if (ide == NULL) {
	PrintError(vm, VCORE_NONE, "Error allocating IDE state\n");
	return -1;
    }

    memset(ide, 0, sizeof(struct ide_internal));

    ide->vm = vm;
    ide->pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));

    if (ide->pci_bus != NULL) {
	struct vm_device * southbridge = v3_find_dev(vm, v3_cfg_val(cfg, "controller"));

	if (!southbridge) {
	    PrintError(vm, VCORE_NONE, "Could not find southbridge\n");
	    V3_Free(ide);
	    return -1;
	}

	ide->southbridge = (struct v3_southbridge *)(southbridge->private_data);
    } else {
	PrintError(vm,VCORE_NONE,"Strange - you don't have a PCI bus\n");
    }

    PrintDebug(vm, VCORE_NONE, "IDE: Creating IDE bus x 2\n");

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, ide);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not attach device %s\n", dev_id);
	V3_Free(ide);
	return -1;
    }

    if (init_ide_state(ide) == -1) {
	PrintError(vm, VCORE_NONE, "Failed to initialize IDE state\n");
	v3_remove_device(dev);
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "Connecting to IDE IO ports\n");

    ret |= v3_dev_hook_io(dev, PRI_DATA_PORT, 
			  &read_data_port, &write_data_port);
    ret |= v3_dev_hook_io(dev, PRI_FEATURES_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, PRI_SECT_CNT_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, PRI_SECT_NUM_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, PRI_CYL_LOW_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, PRI_CYL_HIGH_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, PRI_DRV_SEL_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, PRI_CMD_PORT, 
			  &read_port_std, &write_cmd_port);

    ret |= v3_dev_hook_io(dev, SEC_DATA_PORT, 
			  &read_data_port, &write_data_port);
    ret |= v3_dev_hook_io(dev, SEC_FEATURES_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, SEC_SECT_CNT_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, SEC_SECT_NUM_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, SEC_CYL_LOW_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, SEC_CYL_HIGH_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, SEC_DRV_SEL_PORT, 
			  &read_port_std, &write_port_std);
    ret |= v3_dev_hook_io(dev, SEC_CMD_PORT, 
			  &read_port_std, &write_cmd_port);
  

    ret |= v3_dev_hook_io(dev, PRI_CTRL_PORT, 
			  &read_port_std, &write_port_std);

    ret |= v3_dev_hook_io(dev, SEC_CTRL_PORT, 
			  &read_port_std, &write_port_std);
  

    ret |= v3_dev_hook_io(dev, SEC_ADDR_REG_PORT, 
			  &read_port_std, &write_port_std);

    ret |= v3_dev_hook_io(dev, PRI_ADDR_REG_PORT, 
			  &read_port_std, &write_port_std);


    if (ret != 0) {
	PrintError(vm, VCORE_NONE, "Error hooking IDE IO port\n");
	v3_remove_device(dev);
	return -1;
    }


    if (ide->pci_bus) {
	struct v3_pci_bar bars[6];
	struct v3_southbridge * southbridge = (struct v3_southbridge *)(ide->southbridge);
	struct pci_device * sb_pci = (struct pci_device *)(southbridge->southbridge_pci);
	struct pci_device * pci_dev = NULL;
	int i;

	V3_Print(vm, VCORE_NONE, "Connecting IDE to PCI bus\n");

	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[4].type = PCI_BAR_IO;
	//	bars[4].default_base_port = PRI_DEFAULT_DMA_PORT;
	bars[4].default_base_port = -1;
	bars[4].num_ports = 16;

	bars[4].io_read = read_dma_port;
	bars[4].io_write = write_dma_port;
	bars[4].private_data = ide;

	pci_dev = v3_pci_register_device(ide->pci_bus, PCI_STD_DEVICE, 0, sb_pci->dev_num, 1, 
					 "PIIX3_IDE", bars,
					 pci_config_update, NULL, NULL, NULL, ide);

	if (pci_dev == NULL) {
	    PrintError(vm, VCORE_NONE, "Failed to register IDE BUS %d with PCI\n", i); 
	    v3_remove_device(dev);
	    return -1;
	}

	/* This is for CMD646 devices 
	   pci_dev->config_header.vendor_id = 0x1095;
	   pci_dev->config_header.device_id = 0x0646;
	   pci_dev->config_header.revision = 0x8f07;
	*/

	pci_dev->config_header.vendor_id = 0x8086;
	pci_dev->config_header.device_id = 0x7010;
	pci_dev->config_header.revision = 0x00;

	pci_dev->config_header.prog_if = 0x80; // Master IDE device
	pci_dev->config_header.subclass = PCI_STORAGE_SUBCLASS_IDE;
	pci_dev->config_header.class = PCI_CLASS_STORAGE;

	pci_dev->config_header.command = 0;
	pci_dev->config_header.status = 0x0280;

	ide->ide_pci = pci_dev;


    }

    if (v3_dev_add_blk_frontend(vm, dev_id, connect_fn, (void *)ide) == -1) {
	PrintError(vm, VCORE_NONE, "Could not register %s as frontend\n", dev_id);
	v3_remove_device(dev);
	return -1;
    }
    

    PrintDebug(vm, VCORE_NONE, "IDE Initialized\n");

    return 0;
}


device_register("IDE", ide_init)




int v3_ide_get_geometry(void * ide_data, int channel_num, int drive_num, 
			uint32_t * cylinders, uint32_t * heads, uint32_t * sectors) {

    struct ide_internal * ide  = ide_data;  
    struct ide_channel * channel = &(ide->channels[channel_num]);
    struct ide_drive * drive = &(channel->drives[drive_num]);
    
    if (drive->drive_type == BLOCK_NONE) {
	return -1;
    }

    *cylinders = drive->num_cylinders;
    *heads = drive->num_heads;
    *sectors = drive->num_sectors;

    return 0;
}
