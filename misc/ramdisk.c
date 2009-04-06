/* 
 *
 *   Copyright (C) 2002  MandrakeSoft S.A.
 *
 *     MandrakeSoft S.A.
 *     43, rue d'Aboukir
 *     75002 Paris - France
 *     http://www.linux-mandrake.com/
 *     http://www.mandrakesoft.com/
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Major modifications made for the V3VEE project
 * 
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 * 
 * Copyright (c) 2008, Zheng Cui <cuizheng@cs.unm.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved for original changes
 * 
 */


#include <devices/ramdisk.h>
#include <palacios/vmm.h>
#include <devices/cdrom.h>
#include <devices/ide.h>
#include <devices/pci.h>

#ifndef TRACE_RAMDISK
#undef PrintTrace
#define PrintTrace(fmt, args...)
#endif


#ifndef DEBUG_RAMDISK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif





/*
 * Data type definitions
 *
 */
#define INDEX_PULSE_CYCLE 10




#define INTR_REASON_BIT_ERR           0x01
#define UNABLE_FIND_TAT_CHANNEL_ERR   0x02
#define DRQ_ERR                       0x03
#define READ_BUF_GT_512               0x04



#define PRI_DATA_PORT         0x1f0
#define PRI_FEATURES_PORT     0x1f1
#define PRI_SECT_CNT_PORT     0x1f2
#define PRI_SECT_ADDR1_PORT   0x1f3
#define PRI_SECT_ADDR2_PORT   0x1f4
#define PRI_SECT_ADDR3_PORT   0x1f5
#define PRI_DRV_SEL_PORT      0x1f6
#define PRI_CMD_PORT          0x1f7
#define PRI_CTRL_PORT         0x3f6
#define PRI_ADDR_REG_PORT     0x3f7

#define SEC_DATA_PORT         0x170
#define SEC_FEATURES_PORT     0x171
#define SEC_SECT_CNT_PORT     0x172
#define SEC_SECT_ADDR1_PORT   0x173
#define SEC_SECT_ADDR2_PORT   0x174
#define SEC_SECT_ADDR3_PORT   0x175
#define SEC_DRV_SEL_PORT      0x176
#define SEC_CMD_PORT          0x177
#define SEC_CTRL_PORT         0x376
#define SEC_ADDR_REG_PORT     0x377


#define PACKET_SIZE 12



static const char cdrom_str[] = "CD-ROM";
static const char harddisk_str[] = "HARDDISK";
static const char none_str[] = "NONE";


static inline const char * device_type_to_str(device_type_t type) {
    switch (type) {
	case IDE_DISK:
	    return harddisk_str;
	case IDE_CDROM:
	    return cdrom_str;
	case IDE_NONE:
	    return none_str;
	default:
	    return NULL;
    }
}


static inline void write_features(struct channel_t * channel, uchar_t value) {
    channel->drives[0].controller.features = value;
    channel->drives[1].controller.features = value;
}


static inline void write_sector_count(struct channel_t * channel, uchar_t value) {
    channel->drives[0].controller.sector_count = value;
    channel->drives[1].controller.sector_count = value;
}

static inline void write_sector_number(struct channel_t * channel, uchar_t value) {
    channel->drives[0].controller.sector_no = value;
    channel->drives[1].controller.sector_no = value;
}


static inline void write_cylinder_low(struct channel_t * channel, uchar_t value) {
    channel->drives[0].controller.cylinder_no &= 0xff00;
    channel->drives[0].controller.cylinder_no |= value;
    channel->drives[1].controller.cylinder_no &= 0xff00;
    channel->drives[1].controller.cylinder_no |= value;
}

static inline void write_cylinder_high(struct channel_t * channel, uchar_t value) {
    ushort_t val2 = value;
    val2 = val2 << 8;
    channel->drives[0].controller.cylinder_no &= 0x00ff;
    channel->drives[0].controller.cylinder_no |= (val2 & 0xff00);

    channel->drives[1].controller.cylinder_no &= 0x00ff;
    channel->drives[1].controller.cylinder_no |= (val2 & 0xff00);
}

static inline void write_head_no(struct channel_t * channel, uchar_t value) {
    channel->drives[0].controller.head_no = value;
    channel->drives[1].controller.head_no = value;
}

static inline void write_lba_mode(struct channel_t * channel, uchar_t value) {
    channel->drives[0].controller.lba_mode = value;
    channel->drives[1].controller.lba_mode = value;
}


static inline uint_t get_channel_no(struct ramdisk_t * ramdisk, struct channel_t * channel) {
    return (((uchar_t *)channel - (uchar_t *)(ramdisk->channels)) / sizeof(struct channel_t));
}

static inline uint_t get_drive_no(struct channel_t * channel, struct drive_t * drive) {
    return (((uchar_t *)drive - (uchar_t*)(channel->drives)) /  sizeof(struct drive_t));
}

static inline struct drive_t * get_selected_drive(struct channel_t * channel) {
    return &(channel->drives[channel->drive_select]);
}


static inline int is_primary_port(struct ramdisk_t * ramdisk, ushort_t port) {
    switch(port) 
	{
	    case PRI_DATA_PORT:
	    case PRI_FEATURES_PORT:
	    case PRI_SECT_CNT_PORT:
	    case PRI_SECT_ADDR1_PORT:
	    case PRI_SECT_ADDR2_PORT:
	    case PRI_SECT_ADDR3_PORT:
	    case PRI_DRV_SEL_PORT:
	    case PRI_CMD_PORT:
	    case PRI_CTRL_PORT:
		return 1;
	    default:
		return 0;
	}
}



static inline int is_secondary_port(struct ramdisk_t * ramdisk, ushort_t port) {
    switch(port) 
	{
	    case SEC_DATA_PORT:
	    case SEC_FEATURES_PORT:
	    case SEC_SECT_CNT_PORT:
	    case SEC_SECT_ADDR1_PORT:
	    case SEC_SECT_ADDR2_PORT:
	    case SEC_SECT_ADDR3_PORT:
	    case SEC_DRV_SEL_PORT:
	    case SEC_CMD_PORT:
	    case SEC_CTRL_PORT:
		return 1;
	    default:
		return 0;
	}
}

static inline int num_drives_on_channel(struct channel_t * channel) {
    if ((channel->drives[0].device_type == IDE_NONE) &&
	(channel->drives[1].device_type == IDE_NONE)) {
	return 0;
    } else if ((channel->drives[0].device_type != IDE_NONE) &&
	       (channel->drives[1].device_type != IDE_NONE)) {
	return 2;
    } else {
	return 1;
    }
}



static inline uchar_t extract_bits(uchar_t * buf, uint_t buf_offset, uint_t bit_offset, uint_t num_bits) {
    uchar_t val = buf[buf_offset];
    val = val >> bit_offset;
    val &= ((1 << num_bits) -1);
    return val;
}


static inline uchar_t get_packet_field(struct channel_t * channel, uint_t packet_offset, uint_t bit_offset, uint_t num_bits) {
    struct drive_t * drive = get_selected_drive(channel);
    return extract_bits(drive->controller.buffer, packet_offset, bit_offset, num_bits);
}


static inline uchar_t get_packet_byte(struct channel_t * channel, uint_t offset) {
    struct drive_t * drive = get_selected_drive(channel);
    return drive->controller.buffer[offset];
}

static inline uint16_t get_packet_word(struct channel_t * channel, uint_t offset) {
    struct drive_t * drive = get_selected_drive(channel);
    uint16_t val = drive->controller.buffer[offset];
    val = val << 8;
    val |= drive->controller.buffer[offset + 1];
    return val;
}


static inline uint16_t rd_read_16bit(const uint8_t* buf) {
    return (buf[0] << 8) | buf[1];
}



static inline uint32_t rd_read_32bit(const uint8_t* buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

////////////////////////////////////////////////////////////////////////////


/*
 * ATAPI routines
 */


static void rd_init_mode_sense_single(struct vm_device * dev, struct channel_t * channel, const void * src, int size);

static void rd_command_aborted(struct vm_device * dev, struct channel_t * channel, unsigned value);




static int handle_atapi_packet_command(struct vm_device * dev, 
				       struct channel_t * channel, 
				       ushort_t val);

static int rd_init_send_atapi_command(struct vm_device * dev, 
				      struct channel_t * channel, 
				      Bit8u command, int req_length, 
				      int alloc_length, bool lazy);

static void rd_ready_to_send_atapi(struct vm_device * dev, 
				   struct channel_t * channel);

static void rd_atapi_cmd_error(struct vm_device * dev, 
			       struct channel_t * channel, 
			       sense_t sense_key, asc_t asc);

static void rd_atapi_cmd_nop(struct vm_device * dev, struct channel_t * channel);
static void rd_identify_ATAPI_drive(struct vm_device * dev, struct channel_t * channel);



/*
 * Interrupt handling
 */
static void rd_raise_interrupt(struct vm_device * dev, struct channel_t * channel);
static void rd_lower_irq(struct vm_device *dev, struct channel_t * channel);



/*
 * Helper routines
 */



#ifdef DEBUG_RAMDISK
static void rd_print_state(struct ramdisk_t *ramdisk);
#endif


////////////////////////////////////////////////////////////////////





int v3_ramdisk_register_cdrom(struct vm_device * dev, uint_t busID, uint_t driveID, struct cdrom_ops* cd, void * private_data) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = &(ramdisk->channels[busID]);
    struct drive_t * drive = &(channel->drives[driveID]);
    struct controller_t * controller = &(drive->controller);


  
    if (drive->device_type != IDE_NONE) {
	PrintError("Device already registered at this location\n");
	return -1;
    }


    channel->irq =  15;

    // Make model string
    strncpy((char*)(drive->model_no), "V3VEE Ramdisk", 40);

    while (strlen((char *)(drive->model_no)) < 40) {
	strcat ((char*)(drive->model_no), " ");
    }
  
    PrintDebug("CDROM on target %d/%d\n", busID, driveID);
  
    drive->device_type = IDE_CDROM;
    drive->cdrom.locked = 0;
    drive->sense.sense_key = SENSE_NONE;
    drive->sense.asc = 0;
    drive->sense.ascq = 0;
  
    drive->private_data = private_data;

    controller->sector_count = 0;

    drive->cdrom.cd = cd;
  
    PrintDebug("\t\tCD on ata%d-%d: '%s'\n", 
	       busID, 
	       driveID, "");
  
    if(drive->cdrom.cd->insert_cdrom(drive->private_data)) {
	PrintDebug("\t\tMedia present in CD-ROM drive\n");
	drive->cdrom.ready = 1;
	drive->cdrom.capacity = drive->cdrom.cd->capacity(drive->private_data);
	PrintDebug("\t\tCDROM capacity is %d\n", drive->cdrom.capacity);
    } else {		    
	PrintDebug("\t\tCould not locate CD-ROM, continuing with media not present\n");
	drive->cdrom.ready = 0;
    }
  
    return 0;
}


static Bit32u rd_init_hardware(struct ramdisk_t *ramdisk) {
    uint_t channel_num; 
    uint_t device;
    struct channel_t *channels = (struct channel_t *)(&(ramdisk->channels));

    PrintDebug("[rd_init_harddrive]\n");

    for (channel_num = 0; channel_num < MAX_ATA_CHANNEL; channel_num++) {
	memset((char *)(channels + channel_num), 0, sizeof(struct channel_t));
    }

    for (channel_num = 0; channel_num < MAX_ATA_CHANNEL; channel_num++){
	struct channel_t * channel = &(channels[channel_num]);

	channel->ioaddr1 = 0x0;
	channel->ioaddr2 = 0x0;
	channel->irq = 0;

	for (device = 0; device < 2; device++){
	    struct drive_t * drive = &(channel->drives[device]);
	    struct controller_t * controller = &(drive->controller);

	    controller->status.busy = 0;
	    controller->status.drive_ready = 1;
	    controller->status.write_fault = 0;
	    controller->status.seek_complete = 1;
	    controller->status.drq = 0;
	    controller->status.corrected_data = 0;
	    controller->status.index_pulse = 0;
	    controller->status.index_pulse_count = 0;
	    controller->status.err = 0;

	    controller->error_register = 0x01; // diagnostic code: no error
	    controller->head_no = 0;
	    controller->sector_count = 1;
	    controller->sector_no = 1;
	    controller->cylinder_no = 0;
	    controller->current_command = 0x00;
	    controller->buffer_index = 0;

	    controller->control.reset = 0;
	    controller->control.disable_irq = 0;
	    controller->reset_in_progress = 0;

	    controller->sectors_per_block = 0x80;
	    controller->lba_mode = 0;
      
      
	    controller->features = 0;
	
	    // If not present
	    drive->device_type = IDE_NONE;

	    // Make model string
	    strncpy((char*)(drive->model_no), "", 40);
	    while(strlen((char *)(drive->model_no)) < 40) {
		strcat ((char*)(drive->model_no), " ");
	    }

	}
    }

#ifdef DEBUG_RAMDISK
    rd_print_state(ramdisk);
#endif
    return 0;
}


/*
  static void rd_reset_harddrive(struct ramdisk_t *ramdisk, unsigned type) {
  return;
  }

*/
static void rd_close_harddrive(struct ramdisk_t *ramdisk) {
    return;
}


////////////////////////////////////////////////////////////////////



static int read_data_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * drive = NULL;
    struct controller_t * controller = NULL;



    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }
  
    drive = get_selected_drive(channel);
    controller = &(drive->controller);


    PrintTrace("[read_data_handler] IO Read at 0x%x, on drive %d/%d current cmd=0x%x\n", 
	       port, 
	       get_channel_no(ramdisk, channel),
	       get_drive_no(channel, drive), 
	       controller->current_command);

    switch (controller->current_command) {
	case 0xec:    // IDENTIFY DEVICE
	case 0xa1:
	    {
		controller->status.busy = 0;
		controller->status.drive_ready = 1;
		controller->status.write_fault = 0;
		controller->status.seek_complete = 1;
		controller->status.corrected_data = 0;
		controller->status.err = 0;
		
		/*
		  value32 = controller->buffer[index];
		  index++;
	
		  if (io_len >= 2) {
		  value32 |= (controller->buffer[index] << 8);
		  index++;
		  }
	
		  if (io_len == 4) {
		  value32 |= (controller->buffer[index] << 16);
		  value32 |= (controller->buffer[index+1] << 24);
		  index += 2;
		  }
	
		  controller->buffer_index = index;
		*/
		/* JRL */
		memcpy(dst, controller->buffer + controller->buffer_index, length);
		controller->buffer_index += length;
      
		if (controller->buffer_index >= 512) {
		    controller->status.drq = 0;
		}
		
		return length;
	    }
	case 0xa0: //send packet cmd 
	    {
		uint_t index = controller->buffer_index;

      
		PrintTrace("\t\tatapi.command(%02x), index(%d), cdrom.remaining_blocks(%d)\n", 
			   drive->atapi.command, 
			   index, 
			   drive->cdrom.remaining_blocks);
      
		// Load block if necessary
		if (index >= 2048) {
	
		    if (index > 2048) {
			PrintError("\t\tindex > 2048 : 0x%x\n", index);
			return -1;
		    }
	
		    switch (drive->atapi.command) {
			case 0x28: // read (10)
			case 0xa8: // read (12)
			    {
    
				if (!(drive->cdrom.ready)) {
				    PrintError("\t\tRead with CDROM not ready\n");
				    return -1;
				} 
	    
				drive->cdrom.cd->read_block(drive->private_data, controller->buffer,
							    drive->cdrom.next_lba);
				drive->cdrom.next_lba++;
				drive->cdrom.remaining_blocks--;
	    
	    
				if (!(drive->cdrom.remaining_blocks)) {
				    PrintDebug("\t\tLast READ block loaded {CDROM}\n");
				} else {
				    PrintDebug("\t\tREAD block loaded (%d remaining) {CDROM}\n",
					       drive->cdrom.remaining_blocks);
				}
	    
				// one block transfered, start at beginning
				index = 0;
				break;
			    }
			default: // no need to load a new block
			    break;
		    }
		}
    

		/*
		  increment = 0;
		  value32 = controller->buffer[index + increment];
		  increment++;
	
		  if (io_len >= 2) {
		  value32 |= (controller->buffer[index + increment] << 8);
		  increment++;
		  }
	
		  if (io_len == 4) {
		  value32 |= (controller->buffer[index + increment] << 16);
		  value32 |= (controller->buffer[index + increment + 1] << 24);
		  increment += 2;
		  }

		  controller->buffer_index = index + increment;
		  controller->drq_index += increment;

		*/
		/* JRL: CHECK THAT there is enough data in the buffer to copy.... */
		{      
		    memcpy(dst, controller->buffer + index, length);
	
		    controller->buffer_index  = index + length;
		    controller->drq_index += length;
		}
      
		/* *** */
      
		if (controller->drq_index >= (unsigned)drive->atapi.drq_bytes) {
		    controller->status.drq = 0;
		    controller->drq_index = 0;
	
		    drive->atapi.total_bytes_remaining -= drive->atapi.drq_bytes;
	
		    if (drive->atapi.total_bytes_remaining > 0) {
			// one or more blocks remaining (works only for single block commands)
	  
			PrintDebug("\t\tPACKET drq bytes read\n");
			controller->interrupt_reason.i_o = 1;
			controller->status.busy = 0;
			controller->status.drq = 1;
			controller->interrupt_reason.c_d = 0;
	  
			// set new byte count if last block
			if (drive->atapi.total_bytes_remaining < controller->byte_count) {
			    controller->byte_count = drive->atapi.total_bytes_remaining;
			}
			drive->atapi.drq_bytes = controller->byte_count;
	  
			rd_raise_interrupt(dev, channel);
		    } else {
			// all bytes read
			PrintDebug("\t\tPACKET all bytes read\n");
	  
			controller->interrupt_reason.i_o = 1;
			controller->interrupt_reason.c_d = 1;
			controller->status.drive_ready = 1;
			controller->interrupt_reason.rel = 0;
			controller->status.busy = 0;
			controller->status.drq = 0;
			controller->status.err = 0;
	  
			rd_raise_interrupt(dev, channel);
		    }
		}
		return length;
		break;
	    }

	default:
	    PrintError("\t\tunsupported command: %02x\n", controller->current_command);
	    break;
    }

    return -1;
}




static int write_data_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * drive = NULL;
    struct controller_t * controller = NULL;

    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }
  
    drive = get_selected_drive(channel);
    controller = &(drive->controller);


    PrintDebug("[write_data_handler] IO write at 0x%x, current_cmd = 0x%02x\n", 
	       port, controller->current_command);

 

    //PrintDebug("[write_data_handler]\n");
    switch (controller->current_command) {
	case 0x30: // WRITE SECTORS
	    PrintError("\t\tneed to implement 0x30(write sector) to port 0x%x\n", port);
	    return -1;
    
	case 0xa0: // PACKET
    
	    if (handle_atapi_packet_command(dev, channel, *(ushort_t *)src) == -1) {
		PrintError("Error sending atapi packet command in PACKET write to data port\n");
		return -1;
	    }

	    return length;
    
	default:
	    PrintError("\t\tIO write(0x%x): current command is %02xh\n", 
		       port, controller->current_command);

	    return -1;
    }


    return -1;
}







static int read_status_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * drive = NULL;
    struct controller_t * controller = NULL;




    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }
  
    drive = get_selected_drive(channel);
    controller = &(drive->controller);

 
    PrintDebug("[read_status_handler] IO read at 0x%x, on drive %d/%d\n", 
	       port, get_channel_no(ramdisk, channel), 
	       channel->drive_select);


    if (num_drives_on_channel(channel) == 0) {
	PrintDebug("Setting value to zero because 0 devices on channel\n");
	// (mch) Just return zero for these registers
	memset(dst, 0, length);

    } else {
	uchar_t val = (
		       (controller->status.busy << 7)            |
		       (controller->status.drive_ready << 6)     |
		       (controller->status.write_fault << 5)     |
		       (controller->status.seek_complete << 4)   |
		       (controller->status.drq << 3)             |
		       (controller->status.corrected_data << 2)  |
		       (controller->status.index_pulse << 1)     |
		       (controller->status.err) );


	memcpy(dst, &val, length);

	controller->status.index_pulse_count++;
	controller->status.index_pulse = 0;
    
	if (controller->status.index_pulse_count >= INDEX_PULSE_CYCLE) {
	    controller->status.index_pulse = 1;
	    controller->status.index_pulse_count = 0;
	}
    }
  
    if ((port == SEC_CMD_PORT) || (port == PRI_CMD_PORT)) {
	rd_lower_irq(dev, channel);
    }
  
    PrintDebug("\t\tRead STATUS = 0x%x\n", *(uchar_t *)dst);

    return length;
  
}


static int write_cmd_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * drive = NULL;
    struct controller_t * controller = NULL;
    uchar_t value = *(uchar_t *)src;

    if (length != 1) {
	PrintError("Invalid Command port write length: %d (port=%d)\n", length, port);
	return -1;
    }

    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }
  
    drive = get_selected_drive(channel);
    controller = &(drive->controller);


    PrintDebug("[write_command_handler] IO write at 0x%x, on drive %d/%d (val = 0x%x)\n", 
	       port, get_channel_no(ramdisk, channel), 
	       get_drive_no(channel, drive), 
	       value);

    switch (value) {
#if 0
	case 0xec: // IDENTIFY DEVICE
	    {

		if (drive->device_type == IDE_NONE) {
		    PrintError("\t\tError: disk ata%d-%d not present, aborting\n", 
			       get_channel_no(ramdisk, channel), 
			       get_drive_no(channel, drive));
		    rd_command_aborted(dev, channel, value);
		    break;
		} else if (drive->device_type == IDE_CDROM) {
		    PrintDebug("Identifying CDROM...Going to abort????\n");
		    controller->head_no        = 0;
		    controller->sector_count   = 1;
		    controller->sector_no      = 1;
		    controller->cylinder_no    = 0xeb14;
		    rd_command_aborted(dev, channel, 0xec);
		} else {
		    PrintError("\t\tError: Want to identify HDD!!\n");
		    /*
		      SELECTED_CONTROLLER(channel).current_command = value;
		      SELECTED_CONTROLLER(channel).error_register = 0;
	  
		      // See ATA/ATAPI-4, 8.12
		      SELECTED_CONTROLLER(channel).status.busy  = 0;
		      SELECTED_CONTROLLER(channel).status.drive_ready = 1;
		      SELECTED_CONTROLLER(channel).status.write_fault = 0;
		      SELECTED_CONTROLLER(channel).status.drq   = 1;
		      SELECTED_CONTROLLER(channel).status.err   = 0;
	  
		      SELECTED_CONTROLLER(channel).status.seek_complete = 1;
		      SELECTED_CONTROLLER(channel).status.corrected_data = 0;
	  
		      SELECTED_CONTROLLER(channel).buffer_index = 0;
		      raise_interrupt(channel);
		      identify_drive(channel);
		    */
		}

		break;
	    }
#endif
	    // ATAPI commands
	case 0xa1: // IDENTIFY PACKET DEVICE
	    {
		if (drive->device_type == IDE_CDROM) {
		    controller->current_command = value;
		    controller->error_register = 0;
	
		    controller->status.busy = 0;
		    controller->status.drive_ready = 1;
		    controller->status.write_fault = 0;
		    controller->status.drq   = 1;
		    controller->status.err   = 0;
	
		    controller->status.seek_complete = 1;
		    controller->status.corrected_data = 0;
	
		    controller->buffer_index = 0;
		    rd_raise_interrupt(dev, channel);
		    rd_identify_ATAPI_drive(dev, channel);
		} else {
		    PrintError("Identifying non cdrom device not supported - ata %d/%d\n", 
			       get_channel_no(ramdisk, channel),
			       get_drive_no(channel, drive));
		    rd_command_aborted(dev, channel, 0xa1);
		}
		break;
	    }
	case 0xa0: // SEND PACKET (atapi)
	    {
		if (drive->device_type == IDE_CDROM) {
		    // PACKET
	
		    if (controller->features & (1 << 0)) {
			PrintError("\t\tPACKET-DMA not supported");
			return -1;
		    }
	
		    if (controller->features & (1 << 1)) {
			PrintError("\t\tPACKET-overlapped not supported");
			return -1;
		    }
	
		    // We're already ready!
		    controller->sector_count = 1;
		    controller->status.busy = 0;
		    controller->status.write_fault = 0;

		    // serv bit??
		    controller->status.drq = 1;
		    controller->status.err = 0;
	
		    // NOTE: no interrupt here
		    controller->current_command = value;
		    controller->buffer_index = 0;
		} else {
		    PrintError("Sending packet to non cdrom device not supported\n");
		    rd_command_aborted (dev, channel, 0xa0);
		}
		break;
	    }
	default:
	    PrintError("\t\tneed translate command %2x - ata %d\%d\n", value, 
		       get_channel_no(ramdisk, channel), 
		       get_drive_no(channel, drive));
	    //return -1;
	    /* JRL THIS NEEDS TO CHANGE */
	    return length;

    }
    return length;
}


static int write_ctrl_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * master_drive = NULL;
    struct drive_t * slave_drive = NULL;
    struct controller_t * controller = NULL;
    uchar_t value = *(uchar_t *)src;
    rd_bool prev_control_reset;

    if (length != 1) {
	PrintError("Invalid Status port read length: %d (port=%d)\n", length, port);
	return -1;
    }

    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }

    master_drive = &(channel->drives[0]);
    slave_drive = &(channel->drives[1]);

    controller = &(get_selected_drive(channel)->controller);


    PrintDebug("[write_control_handler] IO write at 0x%x, on drive %d/%d (val = 0x%x)\n", 
	       port, get_channel_no(ramdisk, channel), 
	       channel->drive_select, 
	       value);

    // (mch) Even if device 1 was selected, a write to this register
    // goes to device 0 (if device 1 is absent)
  
    prev_control_reset = controller->control.reset;


    if (value & 0x04) {
	PrintDebug("RESET Signaled\n");
    }

    master_drive->controller.control.reset         = value & 0x04;
    slave_drive->controller.control.reset         = value & 0x04;

    // CGS: was: SELECTED_CONTROLLER(channel).control.disable_irq    = value & 0x02;
    master_drive->controller.control.disable_irq = value & 0x02;
    slave_drive->controller.control.disable_irq = value & 0x02;
  
    PrintDebug("\t\tadpater control reg: reset controller = %d\n",
	       (unsigned) (controller->control.reset) ? 1 : 0);
    PrintDebug("\t\tadpater control reg: disable_irq(X) = %d\n",
	       (unsigned) (controller->control.disable_irq) ? 1 : 0);
  
    if ((!prev_control_reset) && (controller->control.reset)) {
	uint_t id = 0;

	// transition from 0 to 1 causes all drives to reset
	PrintDebug("\t\thard drive: RESET\n");
    
	// (mch) Set BSY, drive not ready
	for (id = 0; id < 2; id++) {
	    struct controller_t * ctrl = NULL;

	    if (id == 0) {
		ctrl = &(master_drive->controller);
	    } else if (id == 1) {
		ctrl = &(slave_drive->controller);
	    }

	    ctrl->status.busy           = 1;
	    ctrl->status.drive_ready    = 0;
	    ctrl->reset_in_progress     = 1;
      
	    ctrl->status.write_fault    = 0;
	    ctrl->status.seek_complete  = 1;
	    ctrl->status.drq            = 0;
	    ctrl->status.corrected_data = 0;
	    ctrl->status.err            = 0;
      
	    ctrl->error_register = 0x01; // diagnostic code: no error
      
	    ctrl->current_command = 0x00;
	    ctrl->buffer_index = 0;
      
	    ctrl->sectors_per_block = 0x80;
	    ctrl->lba_mode          = 0;
      
	    ctrl->control.disable_irq = 0;
	}

	rd_lower_irq(dev, channel);

    } else if ((controller->reset_in_progress) &&
	       (!controller->control.reset)) {
	uint_t id;
	// Clear BSY and DRDY
	PrintDebug("\t\tReset complete {%s}\n", device_type_to_str(get_selected_drive(channel)->device_type));

	for (id = 0; id < 2; id++) {
	    struct controller_t * ctrl = NULL;
	    struct drive_t * drv = NULL;

	    if (id == 0) {
		ctrl = &(master_drive->controller);
		drv = master_drive;
	    } else if (id == 1) {
		ctrl = &(slave_drive->controller);
		drv = slave_drive;
	    }

	    ctrl->status.busy           = 0;
	    ctrl->status.drive_ready    = 1;
	    ctrl->reset_in_progress     = 0;
      
	    // Device signature
	    if (drv->device_type == IDE_DISK) {
		PrintDebug("\t\tdrive %d/%d is harddrive\n", get_channel_no(ramdisk, channel), id);
		ctrl->head_no        = 0;
		ctrl->sector_count   = 1;
		ctrl->sector_no      = 1;
		ctrl->cylinder_no    = 0;
	    } else {
		ctrl->head_no        = 0;
		ctrl->sector_count   = 1;
		ctrl->sector_no      = 1;
		ctrl->cylinder_no    = 0xeb14;
	    }
	}
    }

    PrintDebug("\t\ts[0].controller.control.disable_irq = %02x\n", 
	       master_drive->controller.control.disable_irq);
    PrintDebug("\t\ts[1].controller.control.disable_irq = %02x\n", 
	       slave_drive->controller.control.disable_irq);
    return length;
}


static int read_general_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * drive = NULL;
    struct controller_t * controller = NULL;


    if (length != 1) {
	PrintError("Invalid Status port read length: %d (port=%d)\n", length, port);
	return -1;
    }

    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }
  
    drive = get_selected_drive(channel);
    controller = &(drive->controller);


    PrintDebug("[read_general_handler] IO read addr at %x, on drive %d/%d, curcmd = %02x\n", 
	       port, get_channel_no(ramdisk, channel), 
	       channel->drive_select, 
	       controller->current_command);
  

    switch (port) {
	case PRI_FEATURES_PORT:
	case SEC_FEATURES_PORT: // hard disk error register 0x1f1
	    {    
		uchar_t val = (drive->device_type == IDE_NONE) ? 0 : controller->error_register;
      
		controller->status.err = 0;
      
		PrintDebug("\t\tRead FEATURES = 0x%x\n", val);

		*(uchar_t *)dst = val;
		return length;
      
		break;
	    }

	case PRI_SECT_CNT_PORT:
	case SEC_SECT_CNT_PORT:  // hard disk sector count / interrupt reason 0x1f2
	    {
		uchar_t val = (drive->device_type == IDE_NONE) ? 0 : controller->sector_count;
		PrintDebug("\t\tRead SECTOR COUNT = 0x%x\n", val);
		*(uchar_t *)dst = val;
		return length;

		break;
	    }
	case PRI_SECT_ADDR1_PORT:
	case SEC_SECT_ADDR1_PORT: // sector number 0x1f3
	    { 
		uchar_t val = (drive->device_type == IDE_NONE) ? 0 : controller->sector_no;

		PrintDebug("\t\tRead SECTOR ADDR1 = 0x%x\n", val);

		*(uchar_t *)dst = val;
		return length;

		break;
	    }

	case PRI_SECT_ADDR2_PORT:
	case SEC_SECT_ADDR2_PORT:  // cylinder low 0x1f4  
	    {
		// -- WARNING : On real hardware the controller registers are shared between drives. 
		// So we must respond even if the select device is not present. Some OS uses this fact 
		// to detect the disks.... minix2 for example
		uchar_t val = (num_drives_on_channel(channel) == 0) ? 0 : (controller->cylinder_no & 0x00ff);

		PrintDebug("\t\tRead SECTOR ADDR2 = 0x%x\n", val);

		*(uchar_t *)dst = val;
		return length;

		break;      
	    }

	case PRI_SECT_ADDR3_PORT:
	case SEC_SECT_ADDR3_PORT: // cylinder high 0x1f5
	    {
		// -- WARNING : On real hardware the controller registers are shared between drives. 
		// So we must respond even if the select device is not present. Some OS uses this fact 
		// to detect the disks.... minix2 for example
		uchar_t val = (num_drives_on_channel(channel) == 0) ? 0 : (controller->cylinder_no >> 8);

		PrintDebug("\t\tRead SECTOR ADDR3 = 0x%x\n", val);

		*(uchar_t *)dst = val;
		return length;

		break;    
	    }
	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT:  // hard disk drive and head register 0x1f6
	    {
		// b7 Extended data field for ECC
		// b6/b5: Used to be sector size.  00=256,01=512,10=1024,11=128
		//   Since 512 was always used, bit 6 was taken to mean LBA mode:
		//     b6 1=LBA mode, 0=CHS mode
		//     b5 1
		// b4: DRV
		// b3..0 HD3..HD0
		uchar_t val = ((1 << 7)                          |
			       ((controller->lba_mode > 0) << 6) |
			       (1 << 5)                          |            // 01b = 512 sector size
			       (channel->drive_select << 4)      |
			       (controller->head_no << 0));
      
		PrintDebug("\t\tRead DRIVE SELECT = 0x%x\n", val);
		*(uchar_t *)dst = val;
		return length;

		break;
	    }
	case PRI_ADDR_REG_PORT:
	case SEC_ADDR_REG_PORT: // Hard Disk Address Register 0x3f7
	    {
		// Obsolete and unsupported register.  Not driven by hard
		// disk controller.  Report all 1's.  If floppy controller
		// is handling this address, it will call this function
		// set/clear D7 (the only bit it handles), then return
		// the combined value
		*(uchar_t *)dst = 0xff;
		return length;
	    }

	default:
	    PrintError("Invalid Port: %d\n", port);
	    return -1;
    }
}




static int write_general_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct channel_t * channel = NULL;
    struct drive_t * drive = NULL;
    struct controller_t * controller = NULL;
    uchar_t value = *(uchar_t *)src;

    if (length != 1) {
	PrintError("Invalid Status port read length: %d (port=%d)\n", length, port);
	return -1;
    }

    if (is_primary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[0]);
    } else if (is_secondary_port(ramdisk, port)) {
	channel = &(ramdisk->channels[1]);
    } else {
	PrintError("Invalid Port: %d\n", port);
	return -1;
    }
  
    drive = get_selected_drive(channel);
    controller = &(drive->controller);


    PrintDebug("[write_general_handler] IO write to port %x (val=0x%02x), channel = %d\n", 
	       port, value, get_channel_no(ramdisk, channel));

    switch (port) {

	case PRI_FEATURES_PORT:
	case SEC_FEATURES_PORT: // hard disk write precompensation 0x1f1
	    {
		write_features(channel, value);
		break;
	    }
	case PRI_SECT_CNT_PORT:
	case SEC_SECT_CNT_PORT: // hard disk sector count 0x1f2
	    {
		write_sector_count(channel, value);
		break;
	    }
	case PRI_SECT_ADDR1_PORT:
	case SEC_SECT_ADDR1_PORT: // hard disk sector number 0x1f3
	    {
		write_sector_number(channel, value);
		break;
	    }
	case PRI_SECT_ADDR2_PORT:
	case SEC_SECT_ADDR2_PORT: // hard disk cylinder low 0x1f4
	    {
		write_cylinder_low(channel, value);
		break;
	    }
	case PRI_SECT_ADDR3_PORT:
	case SEC_SECT_ADDR3_PORT: // hard disk cylinder high 0x1f5
	    {
		write_cylinder_high(channel, value);
		break;
	    }
	case PRI_DRV_SEL_PORT:
	case SEC_DRV_SEL_PORT: // hard disk drive and head register 0x1f6
	    {
		// b7 Extended data field for ECC
		// b6/b5: Used to be sector size.  00=256,01=512,10=1024,11=128
		//   Since 512 was always used, bit 6 was taken to mean LBA mode:
		//     b6 1=LBA mode, 0=CHS mode
		//     b5 1
		// b4: DRV
		// b3..0 HD3..HD0

		// 1x1xxxxx

		PrintDebug("\tDrive Select value=%x\n", value);

		if ((value & 0xa0) != 0xa0) { 
		    PrintDebug("\t\tIO write 0x%x (%02x): not 1x1xxxxxb\n", port, (unsigned) value);
		}
      
		write_head_no(channel, value & 0xf);
		if ((controller->lba_mode == 0) && (((value >> 6) & 1) == 1)) {
		    PrintDebug("\t\tenabling LBA mode\n");
		}

		write_lba_mode(channel, (value >> 6) & 1);



		if (drive->cdrom.cd) {
		    PrintDebug("\t\tSetting LBA on CDROM: %d\n", (value >> 6) & 1);
		    drive->cdrom.cd->set_LBA(drive->private_data, (value >> 6) & 1);
		}
      

		channel->drive_select = (value >> 4) & 0x01;
		drive = get_selected_drive(channel);

		if (drive->device_type == IDE_NONE) {
		    PrintError("\t\tError: device set to %d which does not exist! channel = 0x%x\n",
			       channel->drive_select, get_channel_no(ramdisk, channel));

		    controller->error_register = 0x04; // aborted
		    controller->status.err = 1;
		}
      
		break;
	    }
	default:
	    PrintError("\t\thard drive: io write to unhandled port 0x%x  (value = %c)\n", port, value);
	    //return -1;
    }

    return length;
}


 


static void rd_raise_interrupt(struct vm_device * dev, struct channel_t * channel) {
    //  struct ramdisk_t * ramdisk = (struct ramdisk_t *)(dev->private_data);
    struct drive_t * drive = get_selected_drive(channel);
    struct controller_t * controller = &(drive->controller);

    PrintDebug("[raise_interrupt] disable_irq = 0x%02x\n", controller->control.disable_irq);

    if (!(controller->control.disable_irq)) {
 
	PrintDebug("\t\tRaising interrupt %d {%s}\n\n", channel->irq, device_type_to_str(drive->device_type));

	v3_raise_irq(dev->vm, channel->irq);
    } else {
	PrintDebug("\t\tRaising irq but irq is disabled\n");
    }
  
    return;
}

static void rd_lower_irq(struct vm_device *dev, struct channel_t * channel) {
    PrintDebug("[lower_irq] irq = %d\n", channel->irq);
    v3_lower_irq(dev->vm, channel->irq);
}







//////////////////////////////////////////////////////////////////////////

/*
 * ATAPI subroutines
 */



int handle_atapi_packet_command(struct vm_device * dev, struct channel_t * channel, ushort_t value) {
    struct ramdisk_t * ramdisk  = (struct ramdisk_t *)(dev->private_data);
    struct drive_t * drive = get_selected_drive(channel);
    struct controller_t * controller = &(drive->controller);

    if (controller->buffer_index >= PACKET_SIZE) {
	PrintError("ATAPI packet exceeded maximum length: buffer_index (%d) >= PACKET_SIZE\n", 
		   controller->buffer_index);
	return -1;
    }

    controller->buffer[controller->buffer_index] = value;
    controller->buffer[controller->buffer_index + 1] = (value >> 8);
    controller->buffer_index += 2;
  
  
    /* if packet completely writtten */
    if (controller->buffer_index >= PACKET_SIZE) {
	// complete command received
	Bit8u atapi_command = controller->buffer[0];
    
	PrintDebug("\t\tcdrom: ATAPI command 0x%x started\n", atapi_command);
    
	switch (atapi_command) {
	    case 0x00: // test unit ready
		{
		    PrintDebug("Testing unit ready\n");
		    if (drive->cdrom.ready) {
			rd_atapi_cmd_nop(dev, channel);
		    } else {
			PrintError("CDROM not ready in test unit ready\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
		    }
	
		    rd_raise_interrupt(dev, channel);
	
		    break;
		}
	    case 0x03:  // request sense
		{
		    int alloc_length = controller->buffer[4];

		    if (rd_init_send_atapi_command(dev, channel, atapi_command, 18, alloc_length, false) == -1) {
			PrintError("Error sending atapi command in Request Sense\n");
			return -1;
		    }
	
		    // sense data
		    controller->buffer[0] = 0x70 | (1 << 7);
		    controller->buffer[1] = 0;
		    controller->buffer[2] = drive->sense.sense_key;
		    controller->buffer[3] = drive->sense.information.arr[0];
		    controller->buffer[4] = drive->sense.information.arr[1];
		    controller->buffer[5] = drive->sense.information.arr[2];
		    controller->buffer[6] = drive->sense.information.arr[3];
		    controller->buffer[7] = 17 - 7;
		    controller->buffer[8] = drive->sense.specific_inf.arr[0];
		    controller->buffer[9] = drive->sense.specific_inf.arr[1];
		    controller->buffer[10] = drive->sense.specific_inf.arr[2];
		    controller->buffer[11] = drive->sense.specific_inf.arr[3];
		    controller->buffer[12] = drive->sense.asc;
		    controller->buffer[13] = drive->sense.ascq;
		    controller->buffer[14] = drive->sense.fruc;
		    controller->buffer[15] = drive->sense.key_spec.arr[0];
		    controller->buffer[16] = drive->sense.key_spec.arr[1];
		    controller->buffer[17] = drive->sense.key_spec.arr[2];
	
		    rd_ready_to_send_atapi(dev, channel);
		    break;
		}
	    case 0x1b:  // start stop unit
		{
		    //bx_bool Immed = (controller->buffer[1] >> 0) & 1;
		    rd_bool LoEj = (controller->buffer[4] >> 1) & 1;
		    rd_bool Start = (controller->buffer[4] >> 0) & 1;

		    // stop the disc
		    if ((!LoEj) && (!Start)) { 
			PrintError("FIXME: Stop disc not implemented\n");

			rd_atapi_cmd_nop(dev, channel);
			rd_raise_interrupt(dev, channel);

		    } else if (!LoEj && Start) { // start (spin up) the disc
	  
			drive->cdrom.cd->start_cdrom(drive->private_data);
	  
			PrintError("FIXME: ATAPI start disc not reading TOC\n");
			rd_atapi_cmd_nop(dev, channel);
			rd_raise_interrupt(dev, channel);

		    } else if (LoEj && !Start) { // Eject the disc
			rd_atapi_cmd_nop(dev, channel);
			PrintDebug("Ejecting Disk\n");
			if (drive->cdrom.ready) {
	    
			    drive->cdrom.cd->eject_cdrom(drive->private_data);
	    
			    drive->cdrom.ready = 0;
			    //bx_options.atadevice[channel][SLAVE_SELECTED(channel)].Ostatus->set(EJECTED);
			    //bx_gui->update_drive_status_buttons();
			}
			rd_raise_interrupt(dev, channel);

		    } else { // Load the disc
			// My guess is that this command only closes the tray, that's a no-op for us
			rd_atapi_cmd_nop(dev, channel);
			rd_raise_interrupt(dev, channel);
		    }
		    break;
		}
	    case 0xbd: // mechanism status
		{
		    uint16_t alloc_length = rd_read_16bit(controller->buffer + 8);
	
		    if (alloc_length == 0) {
			PrintError("Zero allocation length to MECHANISM STATUS not impl.\n");
			return -1;
		    }
	
		    if (rd_init_send_atapi_command(dev, channel, atapi_command, 8, alloc_length, false) == -1) {
			PrintError("Error sending atapi command in mechanism status\n");
			return -1;
		    }
	
		    controller->buffer[0] = 0; // reserved for non changers
		    controller->buffer[1] = 0; // reserved for non changers
	
		    controller->buffer[2] = 0; // Current LBA (TODO!)
		    controller->buffer[3] = 0; // Current LBA (TODO!)
		    controller->buffer[4] = 0; // Current LBA (TODO!)
	
		    controller->buffer[5] = 1; // one slot
	
		    controller->buffer[6] = 0; // slot table length
		    controller->buffer[7] = 0; // slot table length
	
		    rd_ready_to_send_atapi(dev, channel);
		    break;
		}
	    case 0x5a:  // mode sense
		{
		    uint16_t alloc_length = rd_read_16bit(controller->buffer + 7);
	
		    Bit8u PC = controller->buffer[2] >> 6;
		    Bit8u PageCode = controller->buffer[2] & 0x3f;
	
		    switch (PC) {
			case 0x0: // current values
			    {
				switch (PageCode) {
				    case 0x01: // error recovery
					{
		
					    if (rd_init_send_atapi_command(dev, channel, atapi_command, sizeof(struct error_recovery_t) + 8, alloc_length, false) == -1) {
						PrintError("Error sending atapi command in mode sense error recovery\n");
						return -1;
					    }
		
					    rd_init_mode_sense_single(dev, channel, &(drive->cdrom.current.error_recovery),
								      sizeof(struct error_recovery_t));
					    rd_ready_to_send_atapi(dev, channel);
					    break;
					}
				    case 0x2a: // CD-ROM capabilities & mech. status
					{

					    if (rd_init_send_atapi_command(dev, channel, atapi_command, 28, alloc_length, false) == -1) {
						PrintError("Error sending atapi command in CDROM caps/mech mode-sense\n");
						return -1;
					    }

					    rd_init_mode_sense_single(dev, channel, &(controller->buffer[8]), 28);
		
					    controller->buffer[8] = 0x2a;
					    controller->buffer[9] = 0x12;
					    controller->buffer[10] = 0x00;
					    controller->buffer[11] = 0x00;
					    // Multisession, Mode 2 Form 2, Mode 2 Form 1
					    controller->buffer[12] = 0x70; 
					    controller->buffer[13] = (3 << 5);
					    controller->buffer[14] = (unsigned char) (1 |
										      (drive->cdrom.locked ? (1 << 1) : 0) |
										      (1 << 3) |
										      (1 << 5));
					    controller->buffer[15] = 0x00;
					    controller->buffer[16] = (706 >> 8) & 0xff;
					    controller->buffer[17] = 706 & 0xff;
					    controller->buffer[18] = 0;
					    controller->buffer[19] = 2;
					    controller->buffer[20] = (512 >> 8) & 0xff;
					    controller->buffer[21] = 512 & 0xff;
					    controller->buffer[22] = (706 >> 8) & 0xff;
					    controller->buffer[23] = 706 & 0xff;
					    controller->buffer[24] = 0;
					    controller->buffer[25] = 0;
					    controller->buffer[26] = 0;
					    controller->buffer[27] = 0;
					    rd_ready_to_send_atapi(dev, channel);
					    break;
					}
				    case 0x0d: // CD-ROM
				    case 0x0e: // CD-ROM audio control
				    case 0x3f: // all
					{
					    PrintError("Ramdisk: cdrom: MODE SENSE (curr), code=%x not implemented yet\n",
						       PageCode);
					    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
							       ASC_INV_FIELD_IN_CMD_PACKET);
					    rd_raise_interrupt(dev, channel);
					    break;
					}
				    default:
					{
					    // not implemeted by this device
					    PrintError("\t\tcdrom: MODE SENSE PC=%x, PageCode=%x, not implemented by device\n",
						       PC, PageCode);
					    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
							       ASC_INV_FIELD_IN_CMD_PACKET);
					    rd_raise_interrupt(dev, channel);
					    break;
					}
				}
				break;
			    }
			case 0x1: // changeable values
			    {
				switch (PageCode) {
				    case 0x01: // error recovery
				    case 0x0d: // CD-ROM
				    case 0x0e: // CD-ROM audio control
				    case 0x2a: // CD-ROM capabilities & mech. status
				    case 0x3f: // all
					{
					    PrintError("cdrom: MODE SENSE (chg), code=%x not implemented yet\n",
						       PageCode);
					    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
							       ASC_INV_FIELD_IN_CMD_PACKET);
					    rd_raise_interrupt(dev, channel);
					    break;
					}
				    default:
					{
					    // not implemeted by this device
					    PrintError("Changeable values of mode sense not supported by cdrom\n");
					    PrintDebug("\t\tcdrom: MODE SENSE PC=%x, PageCode=%x, not implemented by device\n",
						       PC, PageCode);
					    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
							       ASC_INV_FIELD_IN_CMD_PACKET);
					    rd_raise_interrupt(dev, channel);
					    break;
					}
				}
				break;
			    }
			case 0x2: // default values
			    {
				switch (PageCode) {
				    case 0x01: // error recovery
				    case 0x0d: // CD-ROM
				    case 0x0e: // CD-ROM audio control
				    case 0x2a: // CD-ROM capabilities & mech. status
				    case 0x3f: // all
					PrintError("Default values of mode sense not supported by cdrom\n");
					PrintDebug("cdrom: MODE SENSE (dflt), code=%x\n",
						   PageCode);
					return -1;
	      
				    default:
					{
					    PrintError("Default values of mode sense not implemented in cdrom\n");
					    // not implemeted by this device
					    PrintDebug("cdrom: MODE SENSE PC=%x, PageCode=%x, not implemented by device\n",
						       PC, PageCode);
					    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
							       ASC_INV_FIELD_IN_CMD_PACKET);
					    rd_raise_interrupt(dev, channel);
					    break;
					}
				}
				break;
			    }
			case 0x3: // saved values not implemented
			    {
				PrintError("\t\tSaved values not implemented in mode sense\n");
				rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_SAVING_PARAMETERS_NOT_SUPPORTED);
				rd_raise_interrupt(dev, channel);
				break;
			    }
			default:
			    {
				PrintError("Unsupported Mode sense value\n");
				return -1;
				break;
			    }
		    }
		    break;
		}
	    case 0x12: // inquiry
		{ 
		    uint8_t alloc_length = controller->buffer[4];
	
		    if (rd_init_send_atapi_command(dev, channel, atapi_command, 36, alloc_length, false) == -1) {
			PrintError("Error sending atapi command in inquiry\n");
			return -1;
		    }
	
		    controller->buffer[0] = 0x05; // CD-ROM
		    controller->buffer[1] = 0x80; // Removable
		    controller->buffer[2] = 0x00; // ISO, ECMA, ANSI version
		    controller->buffer[3] = 0x21; // ATAPI-2, as specified
		    controller->buffer[4] = 31; // additional length (total 36)
		    controller->buffer[5] = 0x00; // reserved
		    controller->buffer[6] = 0x00; // reserved
		    controller->buffer[7] = 0x00; // reserved
	
		    // Vendor ID
		    const char* vendor_id = "VTAB    ";
		    int i;
		    for (i = 0; i < 8; i++) {
			controller->buffer[8+i] = vendor_id[i];
		    }

		    // Product ID
		    const char* product_id = "Turbo CD-ROM    ";
		    for (i = 0; i < 16; i++) {
			controller->buffer[16+i] = product_id[i];
		    }

		    // Product Revision level
		    const char* rev_level = "1.0 ";	
		    for (i = 0; i < 4; i++) {
			controller->buffer[32 + i] = rev_level[i];
		    }

		    rd_ready_to_send_atapi(dev, channel);
		    break;
		}
	    case 0x25:  // read cd-rom capacity
		{
		    // no allocation length???
		    if (rd_init_send_atapi_command(dev, channel, atapi_command, 8, 8, false) == -1) {
			PrintError("Error sending atapi command in read cdrom capacity\n");
			return -1;
		    }
	
		    if (drive->cdrom.ready) {
			uint32_t capacity = drive->cdrom.capacity;

			PrintDebug("\t\tCapacity is %d sectors (%d bytes)\n", capacity, capacity * 2048);

			controller->buffer[0] = (capacity >> 24) & 0xff;
			controller->buffer[1] = (capacity >> 16) & 0xff;
			controller->buffer[2] = (capacity >> 8) & 0xff;
			controller->buffer[3] = (capacity >> 0) & 0xff;
			controller->buffer[4] = (2048 >> 24) & 0xff;
			controller->buffer[5] = (2048 >> 16) & 0xff;
			controller->buffer[6] = (2048 >> 8) & 0xff;
			controller->buffer[7] = (2048 >> 0) & 0xff;

			rd_ready_to_send_atapi(dev, channel);
		    } else {
			PrintError("CDROM not ready in read cdrom capacity\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
			rd_raise_interrupt(dev, channel);
		    }
		    break;
		}
      
      
	    case 0xbe:  // read cd
		{
		    if (drive->cdrom.ready) {
			PrintError("Read CD with CD present not implemented\n");
			rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
			rd_raise_interrupt(dev, channel);
		    } else {
			PrintError("Drive not ready in read cd with CD present\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
			rd_raise_interrupt(dev, channel);
		    }
		    break;
		}
	    case 0x43: // read toc
		{ 
		    if (drive->cdrom.ready) {
			int toc_length = 0;  
			bool msf = (controller->buffer[1] >> 1) & 1;
			uint8_t starting_track = controller->buffer[6];
	  
			uint16_t alloc_length = rd_read_16bit(controller->buffer + 7);
	  
			uint8_t format = (controller->buffer[9] >> 6);
			int i;

			PrintDebug("Reading CDROM TOC: Format=%d (byte count=%d) (toc length:%d)\n", 
				   format, controller->byte_count, toc_length);

			switch (format) {
			    case 0:
				if (!(drive->cdrom.cd->read_toc(drive->private_data, controller->buffer,
								&toc_length, msf, starting_track))) {
				    PrintError("CDROM: Reading Table of Contents Failed\n");
				    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
						       ASC_INV_FIELD_IN_CMD_PACKET);
				    rd_raise_interrupt(dev, channel);
				    break;
				}

	      
				if (rd_init_send_atapi_command(dev, channel, atapi_command, toc_length, alloc_length, false) == -1) {
				    PrintError("Failed to init send atapi command in read toc (fmt=%d)\n", format);
				    return -1;
				}

				rd_ready_to_send_atapi(dev, channel);    

				break;

			    case 1:
				// multi session stuff. we ignore this and emulate a single session only

				if (rd_init_send_atapi_command(dev, channel, atapi_command, 12, alloc_length, false) == -1) {
				    PrintError("Failed to init send atapi command in read toc (fmt=%d)\n", format);
				    return -1;
				}
	    
				controller->buffer[0] = 0;
				controller->buffer[1] = 0x0a;
				controller->buffer[2] = 1;
				controller->buffer[3] = 1;

				for (i = 0; i < 8; i++) {
				    controller->buffer[4 + i] = 0;
				}

				rd_ready_to_send_atapi(dev, channel);
				break;
	    
			    case 2:
			    default:
				PrintError("(READ TOC) Format %d not supported\n", format);
				return -1;
			}
		    } else {
			PrintError("CDROM not ready in read toc\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
			rd_raise_interrupt(dev, channel);
		    }
		    break;
		}
	    case 0x28: // read (10)
	    case 0xa8: // read (12)
		{ 
	
		    uint32_t transfer_length;
		    if (atapi_command == 0x28) {
			transfer_length = rd_read_16bit(controller->buffer + 7);
		    } else {
			transfer_length = rd_read_32bit(controller->buffer + 6);
		    }

		    uint32_t lba = rd_read_32bit(controller->buffer + 2);
	
		    if (!(drive->cdrom.ready)) {
			PrintError("CDROM Error: Not Ready (ATA%d/%d)\n", 
				   get_channel_no(ramdisk, channel), get_drive_no(channel, drive));
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
			rd_raise_interrupt(dev, channel);
			break;
		    }
	
		    if (transfer_length == 0) {
			PrintError("READ(%d) with transfer length 0, ok\n", 
				   (atapi_command == 0x28) ? 10 : 12);
			rd_atapi_cmd_nop(dev, channel);
			rd_raise_interrupt(dev, channel);
			break;
		    }
	
		    if (lba + transfer_length > drive->cdrom.capacity) {
			PrintError("CDROM Error: Capacity exceeded [capacity=%d] (ATA%d/%d)\n",
				   drive->cdrom.capacity,
				   get_channel_no(ramdisk, channel), get_drive_no(channel, drive));
			rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_LOGICAL_BLOCK_OOR);
			rd_raise_interrupt(dev, channel);
			break;
		    }
	
		    PrintDebug("\t\tcdrom: READ (%d) LBA=%d LEN=%d\n", 
			       (atapi_command == 0x28) ? 10 : 12, 
			       lba, transfer_length);
	
		    // handle command
		    if (rd_init_send_atapi_command(dev, channel, atapi_command, transfer_length * 2048,
						   transfer_length * 2048, true) == -1) {
			PrintError("CDROM Error: Atapi command send error\n");
			return -1;
		    }

		    drive->cdrom.remaining_blocks = transfer_length;
		    drive->cdrom.next_lba = lba;
		    rd_ready_to_send_atapi(dev, channel);
		    break;
		}
	    case 0x2b:  // seek
		{
		    uint32_t lba = rd_read_32bit(controller->buffer + 2);

		    if (!(drive->cdrom.ready)) {
			PrintError("CDROM not ready in seek\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
			rd_raise_interrupt(dev, channel);
			break;
		    }
	
		    if (lba > drive->cdrom.capacity) {
			PrintError("LBA is greater than CDROM capacity in seek\n");
			rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_LOGICAL_BLOCK_OOR);
			rd_raise_interrupt(dev, channel);
			break;
		    }
	
		    PrintError("\t\tcdrom: SEEK (ignored)\n");

		    rd_atapi_cmd_nop(dev, channel);
		    rd_raise_interrupt(dev, channel);

		    break;
		}
	    case 0x1e:  // prevent/allow medium removal
		{

		    if (drive->cdrom.ready) {
			drive->cdrom.locked = controller->buffer[4] & 1;
			rd_atapi_cmd_nop(dev, channel);
		    } else {
			PrintError("CD not ready in prevent/allow medium removal\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
		    }

		    rd_raise_interrupt(dev, channel);

		    break;
		}
	    case 0x42:  // read sub-channel
		{
		    //bool msf = get_packet_field(channel, 1, 1, 1);
		    bool sub_q = get_packet_field(channel, 2, 6, 1);
		    //uint8_t data_format = get_packet_byte(channel, 3);
		    //uint8_t track_number = get_packet_byte(channel, 6);
		    uint16_t alloc_length = get_packet_word(channel, 7);
	

		    /*
		      UNUSED(msf);
		      UNUSED(data_format);
		      UNUSED(track_number);
		    */
		    if (!(drive->cdrom.ready)) {
			PrintError("CDROM not ready in read sub-channel\n");
			rd_atapi_cmd_error(dev, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
			rd_raise_interrupt(dev, channel);
		    } else {
			controller->buffer[0] = 0;
			controller->buffer[1] = 0; // audio not supported
			controller->buffer[2] = 0;
			controller->buffer[3] = 0;
	  
			int ret_len = 4; // header size
	  
			if (sub_q) { // !sub_q == header only
			    PrintError("Read sub-channel with SubQ not implemented\n");
			    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST,
					       ASC_INV_FIELD_IN_CMD_PACKET);
			    rd_raise_interrupt(dev, channel);
			}
	  
			if (rd_init_send_atapi_command(dev, channel, atapi_command, ret_len, alloc_length, false) == -1) {
			    PrintError("Error sending atapi command in read sub-channel\n");
			    return -1;
			}
			rd_ready_to_send_atapi(dev, channel);
		    }
		    break;
		}
	    case 0x51:  // read disc info
		{
		    // no-op to keep the Linux CD-ROM driver happy
		    PrintError("Error: Read disk info no-op to keep the Linux CD-ROM driver happy\n");
		    rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
		    rd_raise_interrupt(dev, channel);
		    break;
		}
	    case 0x55: // mode select
	    case 0xa6: // load/unload cd
	    case 0x4b: // pause/resume
	    case 0x45: // play audio
	    case 0x47: // play audio msf
	    case 0xbc: // play cd
	    case 0xb9: // read cd msf
	    case 0x44: // read header
	    case 0xba: // scan
	    case 0xbb: // set cd speed
	    case 0x4e: // stop play/scan
	    case 0x46: // ???
	    case 0x4a: // ???
		PrintError("ATAPI command 0x%x not implemented yet\n",
			   atapi_command);
		rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
		rd_raise_interrupt(dev, channel);
		break;
	    default:
		PrintError("Unknown ATAPI command 0x%x (%d)\n",
			   atapi_command, atapi_command);
		// We'd better signal the error if the user chose to continue
		rd_atapi_cmd_error(dev, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
		rd_raise_interrupt(dev, channel);
		break;
	}
    }
       
	      
    return 0;
}




int rd_init_send_atapi_command(struct vm_device * dev, struct channel_t * channel, Bit8u command, int req_length, int alloc_length, bool lazy)
{
    struct drive_t * drive = &(channel->drives[channel->drive_select]);
    struct controller_t * controller = &(drive->controller);

    // controller->byte_count is a union of controller->cylinder_no;
    // lazy is used to force a data read in the buffer at the next read.
  
    PrintDebug("[rd_init_send_atapi_cmd]\n");

    if (controller->byte_count == 0xffff) {
	controller->byte_count = 0xfffe;
    }

    if ((controller->byte_count & 1) && 
	!(alloc_length <= controller->byte_count)) {
      
	PrintDebug("\t\tOdd byte count (0x%04x) to ATAPI command 0x%02x, using 0x%x\n", 
		   controller->byte_count, 
		   command, 
		   controller->byte_count - 1);
    
	controller->byte_count -= 1;
    }
  
    if (controller->byte_count == 0) {
	PrintError("\t\tATAPI command with zero byte count\n");
	return -1;
    }

    if (alloc_length < 0) {
	PrintError("\t\tAllocation length < 0\n");
	return -1;
    }

    if (alloc_length == 0) {
	alloc_length = controller->byte_count;
    }
  
    controller->interrupt_reason.i_o = 1;
    controller->interrupt_reason.c_d = 0;
    controller->status.busy = 0;
    controller->status.drq = 1;
    controller->status.err = 0;
  
    // no bytes transfered yet
    if (lazy) {
	controller->buffer_index = 2048;
    } else {
	controller->buffer_index = 0;
    }

    controller->drq_index = 0;
  
    if (controller->byte_count > req_length) {
	controller->byte_count = req_length;
    }

    if (controller->byte_count > alloc_length) {
	controller->byte_count = alloc_length;
    }  

    drive->atapi.command = command;
    drive->atapi.drq_bytes = controller->byte_count;
    drive->atapi.total_bytes_remaining = (req_length < alloc_length) ? req_length : alloc_length;
  
    // if (lazy) {
    // // bias drq_bytes and total_bytes_remaining
    // SELECTED_DRIVE(channel).atapi.drq_bytes += 2048;
    // SELECTED_DRIVE(channel).atapi.total_bytes_remaining += 2048;
    // }

    return 0;
}



void rd_ready_to_send_atapi(struct vm_device * dev, struct channel_t * channel) {
    PrintDebug("[rd_ready_to_send_atapi]\n");
  
    rd_raise_interrupt(dev, channel);
}





void rd_atapi_cmd_error(struct vm_device * dev, struct channel_t * channel, sense_t sense_key, asc_t asc)
{
    struct drive_t * drive = &(channel->drives[channel->drive_select]);
    struct controller_t * controller = &(drive->controller);


    struct ramdisk_t *ramdisk = (struct ramdisk_t *)(dev->private_data);
    PrintError("[rd_atapi_cmd_error]\n");
    PrintError("Error: atapi_cmd_error channel=%02x key=%02x asc=%02x\n", 
	       get_channel_no(ramdisk, channel), sense_key, asc);
  

    controller->error_register = sense_key << 4;
    controller->interrupt_reason.i_o = 1;
    controller->interrupt_reason.c_d = 1;
    controller->interrupt_reason.rel = 0;
    controller->status.busy = 0;
    controller->status.drive_ready = 1;
    controller->status.write_fault = 0;
    controller->status.drq = 0;
    controller->status.err = 1;
  
    drive->sense.sense_key = sense_key;
    drive->sense.asc = asc;
    drive->sense.ascq = 0;
}



void rd_atapi_cmd_nop(struct vm_device * dev, struct channel_t * channel)
{
    struct drive_t * drive = &(channel->drives[channel->drive_select]);
    struct controller_t * controller = &(drive->controller);

    PrintDebug("[rd_atapi_cmd_nop]\n");
    controller->interrupt_reason.i_o = 1;
    controller->interrupt_reason.c_d = 1;
    controller->interrupt_reason.rel = 0;
    controller->status.busy = 0;
    controller->status.drive_ready = 1;
    controller->status.drq = 0;
    controller->status.err = 0;
}




void rd_identify_ATAPI_drive(struct vm_device * dev, struct channel_t * channel)
{
    struct drive_t * drive = &(channel->drives[channel->drive_select]);
    struct controller_t * controller = &(drive->controller);


    uint_t i;
    const char* serial_number = " VT00001\0\0\0\0\0\0\0\0\0\0\0\0";
    const char* firmware = "ALPHA1  ";

    drive->id_drive[0] = (2 << 14) | (5 << 8) | (1 << 7) | (2 << 5) | (0 << 0); // Removable CDROM, 50us response, 12 byte packets

    for (i = 1; i <= 9; i++) {
	drive->id_drive[i] = 0;
    }

    for (i = 0; i < 10; i++) {
	drive->id_drive[10 + i] = ((serial_number[i * 2] << 8) |
				   (serial_number[(i * 2) + 1]));
    }

    for (i = 20; i <= 22; i++) {
	drive->id_drive[i] = 0;
    }

    for (i = 0; i < strlen(firmware) / 2; i++) {
	drive->id_drive[23 + i] = ((firmware[i * 2] << 8) |
				   (firmware[(i * 2) + 1]));
    }
    V3_ASSERT((23 + i) == 27);
  
    for (i = 0; i < strlen((char *)(drive->model_no)) / 2; i++) {
	drive->id_drive[27 + i] = ((drive->model_no[i * 2] << 8) |
				   (drive->model_no[(i * 2) + 1]));
    }

    V3_ASSERT((27 + i) == 47);

    drive->id_drive[47] = 0;
    drive->id_drive[48] = 1; // 32 bits access

    drive->id_drive[49] = (1 << 9); // LBA supported

    drive->id_drive[50] = 0;
    drive->id_drive[51] = 0;
    drive->id_drive[52] = 0;

    drive->id_drive[53] = 3; // words 64-70, 54-58 valid

    for (i = 54; i <= 62; i++) {
	drive->id_drive[i] = 0;
    }

    // copied from CFA540A
    drive->id_drive[63] = 0x0103; // variable (DMA stuff)
    drive->id_drive[64] = 0x0001; // PIO
    drive->id_drive[65] = 0x00b4;
    drive->id_drive[66] = 0x00b4;
    drive->id_drive[67] = 0x012c;
    drive->id_drive[68] = 0x00b4;

    drive->id_drive[69] = 0;
    drive->id_drive[70] = 0;
    drive->id_drive[71] = 30; // faked
    drive->id_drive[72] = 30; // faked
    drive->id_drive[73] = 0;
    drive->id_drive[74] = 0;

    drive->id_drive[75] = 0;

    for (i = 76; i <= 79; i++) {
	drive->id_drive[i] = 0;
    }

    drive->id_drive[80] = 0x1e; // supports up to ATA/ATAPI-4
    drive->id_drive[81] = 0;
    drive->id_drive[82] = 0;
    drive->id_drive[83] = 0;
    drive->id_drive[84] = 0;
    drive->id_drive[85] = 0;
    drive->id_drive[86] = 0;
    drive->id_drive[87] = 0;
    drive->id_drive[88] = 0;

    for (i = 89; i <= 126; i++) {
	drive->id_drive[i] = 0;
    }

    drive->id_drive[127] = 0;
    drive->id_drive[128] = 0;

    for (i = 129; i <= 159; i++) {
	drive->id_drive[i] = 0;
    }

    for (i = 160; i <= 255; i++) {
	drive->id_drive[i] = 0;
    }


    return;
}







static 
void rd_init_mode_sense_single(struct vm_device * dev, 
			       struct channel_t * channel, const void* src, int size)
{
    struct drive_t * drive = &(channel->drives[channel->drive_select]);
    struct controller_t * controller = &(drive->controller);

    PrintDebug("[rd_init_mode_sense_single]\n");

    // Header
    controller->buffer[0] = (size + 6) >> 8;
    controller->buffer[1] = (size + 6) & 0xff;
    controller->buffer[2] = 0x70; // no media present
    controller->buffer[3] = 0; // reserved
    controller->buffer[4] = 0; // reserved
    controller->buffer[5] = 0; // reserved
    controller->buffer[6] = 0; // reserved
    controller->buffer[7] = 0; // reserved
  
    // Data
    memcpy(controller->buffer + 8, src, size);
}



static void rd_command_aborted(struct vm_device * dev, 
			       struct channel_t * channel, unsigned value) {
    struct drive_t * drive = &(channel->drives[channel->drive_select]);
    struct controller_t * controller = &(drive->controller);

    PrintError("[rd_command_aborted]\n");
    PrintError("\t\taborting on command 0x%02x {%s}\n", value, device_type_to_str(drive->device_type));

    controller->current_command = 0;
    controller->status.busy = 0;
    controller->status.drive_ready = 1;
    controller->status.err = 1;
    controller->error_register = 0x04; // command ABORTED
    controller->status.drq = 0;
    controller->status.seek_complete = 0;
    controller->status.corrected_data = 0;
    controller->buffer_index = 0;

    rd_raise_interrupt(dev, channel);
}


/*    
static void init_pci(struct ramdisk_t * ramdisk) {
struct v3_pci_bar bars[6];
    struct pci_device * pci_dev;
    int i;
    
    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
	bars[i].mem_hook = 0;
	bars[i].num_pages = 0;
	bars[i].bar_update = NULL;
    }

    bars[4].type = PCI_BAR_MEM32;
    bars[4].mem_hook = 0;
    bars[4].num_pages = 1;
    bars[4].bar_update = NULL;

    pci_dev = v3_pci_register_device(ramdisk->pci, PCI_STD_DEVICE, 0, "IDE", -1, bars, NULL, NULL, NULL, NULL);


    pci_dev->config_header.vendor_id = 0x8086;
    pci_dev->config_header.device_id = 0x2421;


}
    */
static int ramdisk_init_device(struct vm_device *dev) {
    struct ramdisk_t *ramdisk= (struct ramdisk_t *)dev->private_data;

    PrintDebug("Initializing Ramdisk\n");


    rd_init_hardware(ramdisk);


    v3_dev_hook_io(dev, PRI_CTRL_PORT, 
		   &read_status_port, &write_ctrl_port);

    v3_dev_hook_io(dev, PRI_DATA_PORT, 
		   &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, PRI_FEATURES_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, PRI_SECT_CNT_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, PRI_SECT_ADDR1_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, PRI_SECT_ADDR2_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, PRI_SECT_ADDR3_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, PRI_DRV_SEL_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, PRI_CMD_PORT, 
		   &read_status_port, &write_cmd_port);


    v3_dev_hook_io(dev, SEC_CTRL_PORT, 
		   &read_status_port, &write_ctrl_port);

    v3_dev_hook_io(dev, SEC_DATA_PORT, 
		   &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, SEC_FEATURES_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, SEC_SECT_CNT_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, SEC_SECT_ADDR1_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, SEC_SECT_ADDR2_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, SEC_SECT_ADDR3_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, SEC_DRV_SEL_PORT, 
		   &read_general_port, &write_general_port);
    v3_dev_hook_io(dev, SEC_CMD_PORT, 
		   &read_status_port, &write_cmd_port);
  
  

    v3_dev_hook_io(dev, SEC_ADDR_REG_PORT, 
		   &read_general_port, &write_general_port);

    v3_dev_hook_io(dev, PRI_ADDR_REG_PORT, 
		   &read_general_port, &write_general_port);





    return 0;

}


static int ramdisk_deinit_device(struct vm_device *dev) {
    struct ramdisk_t *ramdisk = (struct ramdisk_t *)(dev->private_data);
    rd_close_harddrive(ramdisk);
    return 0;
}

static struct vm_device_ops dev_ops = {
    .init = ramdisk_init_device,
    .deinit = ramdisk_deinit_device,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};




struct vm_device * v3_create_ramdisk(struct vm_device * pci)
{

    struct ramdisk_t *ramdisk;
    ramdisk = (struct ramdisk_t *)V3_Malloc(sizeof(struct ramdisk_t));  
    V3_ASSERT(ramdisk != NULL);  

    //    ramdisk->pci = pci;

    PrintDebug("[create_ramdisk]\n");

    struct vm_device * device = v3_create_device("RAMDISK", &dev_ops, ramdisk);

    

    return device;
}




#ifdef DEBUG_RAMDISK

static void rd_print_state(struct ramdisk_t * ramdisk) {
    uchar_t channel; 
    uchar_t device;
    struct channel_t * channels = (struct channel_t *)(&(ramdisk->channels));

    /*
      for (channel = 0; channel < MAX_ATA_CHANNEL; channel++) {
      memset((char *)(channels + channel), 0, sizeof(struct channel_t));
      }
    */



    for (channel = 0; channel < MAX_ATA_CHANNEL; channel++){
  
	for (device = 0; device < 2; device++){
                  
	    // Initialize controller state, even if device is not present
	    PrintDebug("channels[%d].drives[%d].controller.status.busy = %d\n",
		       channel, device, 
		       channels[channel].drives[device].controller.status.busy);
	    PrintDebug("channels[%d].drives[%d].controller.status.drive_ready = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.drive_ready);
	    PrintDebug("channels[%d].drives[%d].controller.status.write_fault = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.write_fault);
	    PrintDebug("channels[%d].drives[%d].controller.status.seek_complete = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.seek_complete);
	    PrintDebug("channels[%d].drives[%d].controller.status.drq = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.drq);
	    PrintDebug("channels[%d].drives[%d].controller.status.corrected_data = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.corrected_data);
	    PrintDebug("channels[%d].drives[%d].controller.status.index_pulse = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.index_pulse);
	    PrintDebug("channels[%d].drives[%d].controller.status.index_pulse_count = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.index_pulse_count);
	    PrintDebug("channels[%d].drives[%d].controller.status.err = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.status.err);


	    PrintDebug("channels[%d].drives[%d].controller.error_register = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.error_register);
	    PrintDebug("channels[%d].drives[%d].controller.head_no = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.head_no);
	    PrintDebug("channels[%d].drives[%d].controller.sector_count = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.sector_count);
	    PrintDebug("channels[%d].drives[%d].controller.sector_no = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.sector_no);
	    PrintDebug("channels[%d].drives[%d].controller.cylinder_no = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.cylinder_no);
	    PrintDebug("channels[%d].drives[%d].controller.current_command = %02x\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.current_command);
	    PrintDebug("channels[%d].drives[%d].controller.buffer_index = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.buffer_index);


	    PrintDebug("channels[%d].drives[%d].controller.control.reset = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.control.reset);
	    PrintDebug("channels[%d].drives[%d].controller.control.disable_irq = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.control.disable_irq);


	    PrintDebug("channels[%d].drives[%d].controller.reset_in_progress = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.reset_in_progress);
	    PrintDebug("channels[%d].drives[%d].controller.sectors_per_block = %02x\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.sectors_per_block); 
	    PrintDebug("channels[%d].drives[%d].controller.lba_mode = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.lba_mode); 
	    PrintDebug("channels[%d].drives[%d].controller.features = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.features); 


	    PrintDebug("channels[%d].drives[%d].model_no = %s\n", 
		       channel, device, 
		       channels[channel].drives[device].model_no); 
	    PrintDebug("channels[%d].drives[%d].device_type = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].device_type); 
	    PrintDebug("channels[%d].drives[%d].cdrom.locked = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].cdrom.locked); 
	    PrintDebug("channels[%d].drives[%d].sense.sense_key = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].sense.sense_key); 
	    PrintDebug("channels[%d].drives[%d].sense.asc = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].sense.asc); 
	    PrintDebug("channels[%d].drives[%d].sense.ascq = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].sense.ascq); 



	    PrintDebug("channels[%d].drives[%d].controller.interrupt_reason.c_d = %02x\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.interrupt_reason.c_d);

	    PrintDebug("channels[%d].drives[%d].controller.interrupt_reason.i_o = %02x\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.interrupt_reason.i_o);

	    PrintDebug("channels[%d].drives[%d].controller.interrupt_reason.rel = %02x\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.interrupt_reason.rel);

	    PrintDebug("channels[%d].drives[%d].controller.interrupt_reason.tag = %02x\n", 
		       channel, device, 
		       channels[channel].drives[device].controller.interrupt_reason.tag);

	    PrintDebug("channels[%d].drives[%d].cdrom.ready = %d\n", 
		       channel, device, 
		       channels[channel].drives[device].cdrom.ready);
      
	}  //for device
    }  //for channel
  
    return;
}


#endif
