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

#define ATAPI_PACKET_SIZE 12

#include "atapi-types.h"

static void atapi_cmd_error(struct vm_device * dev, struct ide_channel * channel, 
		     atapi_sense_key_t  sense_key, atapi_add_sense_code_t asc) {
    struct ide_drive * drive = get_selected_drive(channel);

    // overload error register with ATAPI value
    channel->error_reg.val = sense_key << 4;
    
    channel->status.busy = 0;
    channel->status.ready = 1;
    channel->status.write_fault = 0;
    channel->status.data_req = 0;
    channel->status.error = 1;
  
    drive->cd_state.sense.header = 0xf0;
    drive->cd_state.sense.rsvd1 = 0x00;
    drive->cd_state.sense.read_len = 0x0a;
    drive->cd_state.sense.sense_key = sense_key;
    drive->cd_state.sense.asc = asc;

    ide_raise_irq(dev, channel);
}


static void atapi_cmd_nop(struct vm_device * dev, struct ide_channel * channel) {
    channel->status.busy = 0;
    channel->status.ready = 1;
    channel->status.data_req = 0;
    channel->status.error = 0;

    ide_raise_irq(dev, channel);
}



static int atapi_read_chunk(struct vm_device * dev, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    int ret = drive->cd_ops->read(drive->data_buf, drive->cd_state.current_lba, drive->private_data);

    if (ret == -1) {
	PrintError("IDE: Error reading CD block (LBA=%x)\n", drive->cd_state.current_lba);
	return -1;
    }

    return 0;
}

static int atapi_read10(struct vm_device * dev, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_read10_cmd * cmd = (struct atapi_read10_cmd *)(drive->data_buf);
    uint32_t lba =  be_to_le_32(cmd->lba);
    uint16_t xfer_len = be_to_le_16(cmd->xfer_len);
    
    /* Check if cd is ready
     * if not: atapi_cmd_error(... ATAPI_SEN_NOT_RDY, ASC_MEDIA_NOT_PRESENT)
     */

    if (xfer_len == 0) {
	atapi_cmd_nop(dev, channel);
	return 0;
    }
	
    if ((lba + xfer_len) > drive->cd_ops->get_capacity(drive->private_data)) {
	atapi_cmd_error(dev, channel, ATAPI_SEN_ILL_REQ, ASC_LOG_BLK_OOR);
    }

    drive->cd_state.current_lba = lba;


    if (atapi_read_chunk(dev, channel) == -1) {
	PrintError("IDE: Could not read initial chunk from CD\n");
	return -1;
    }
    
    drive->transfer_length = xfer_len;
    drive->transfer_index = 0;

    channel->status.busy = 0;
    channel->status.data_req = 1;
    channel->status.error = 0;

    ide_raise_irq(dev, channel);

    return 0;
}



static void atapi_req_sense(struct vm_device * dev, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    drive->transfer_length = 18;
    drive->transfer_index = 0;
    memcpy(drive->data_buf, drive->cd_state.sense.buf, sizeof(drive->cd_state.sense.buf));
   
    ide_raise_irq(dev, channel);
}

static int atapi_handle_packet(struct vm_device * dev, struct ide_channel * channel) {
   struct ide_drive * drive = get_selected_drive(channel);
   uint8_t command = drive->data_buf[0];

   PrintDebug("IDE: ATAPI Command %x\n", command);

   switch (command) {
       case 0x00: // test unit ready
	   atapi_cmd_nop(dev, channel);

	   /* if drive not ready: 
	      atapi_cmd_error(... ATAPI_SEN_NOT_RDY, ASC_MEDIA_NOT_PRESENT)
 	   */
	   break;
       case 0x03: // request sense
	   atapi_req_sense(dev, channel);
	   break;

       case 0x28: // read(10)
	   if (atapi_read10(dev, channel) == -1) {
	       PrintError("IDE: Error in ATAPI read (%x)\n", command);
	       return -1;
	   }

	   break;
       case 0xa8: // read(12)


       case 0x1b: // start/stop drive
       case 0xbd: // mechanism status 
       case 0x5a: // mode sense
       case 0x12: // inquiry
       case 0x25: // read cdrom capacity
       case 0xbe: // read cd
       case 0x43: // read TOC


       case 0x2b: // seek
       case 0x1e: // lock door
       case 0x42: // read sub-channel
       case 0x51: // read disk info

	   
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
       default:
	   PrintError("Unhandled ATAPI command %x\n", command);
	   atapi_cmd_error(dev, channel, ATAPI_SEN_ILL_REQ, ASC_INV_CMD_FIELD);
	   ide_raise_irq(dev, channel);
	   return -1;
   }
   
   return 0;
}


static void atapi_identify_device(struct ide_drive * drive) {
    struct ide_drive_id * drive_id = (struct ide_drive_id *)(drive->data_buf);
    const char* serial_number = " VT00001\0\0\0\0\0\0\0\0\0\0\0\0";
    const char* firmware = "ALPHA1  ";

    drive->transfer_length = 512;
    drive->transfer_index = 0;
    memset(drive_id->buf, 0, sizeof(drive_id->buf));

    drive_id->fixed_drive = 1;
    drive_id->removable_media = 1;

    // Black magic...
    drive_id->disk_speed1 = 1;
    drive_id->disk_speed3 = 1;

    drive_id->cdrom_flag = 1;

    // These buffers do not contain a terminating "\0"
    memcpy(drive_id->serial_num, serial_number, strlen(serial_number));
    memcpy(drive_id->firmware_rev, firmware, strlen(firmware));
    memcpy(drive_id->model_num, drive->model, 40);

    // 32 bits access
    drive_id->dword_io = 1;

    // enable LBA access
    drive_id->lba_enable = 1;
    

    // words 64-70, 54-58 valid
    drive_id->buf[53] = 0x0003;

    // copied from CFA540A
    drive_id->buf[63] = 0x0103; // variable (DMA stuff)
    drive_id->buf[64] = 0x0001; // PIO
    drive_id->buf[65] = 0x00b4;
    drive_id->buf[66] = 0x00b4;
    drive_id->buf[67] = 0x012c;
    drive_id->buf[68] = 0x00b4;

    drive_id->buf[71] = 30; // faked
    drive_id->buf[72] = 30; // faked

    drive_id->buf[80] = 0x1e; // supports up to ATA/ATAPI-4
}
