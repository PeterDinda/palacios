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


/* ATAPI sucks...
 * The OS will write to the cylinder register the number of bytes it wants to read 
 * however the device can change that value 
 * 
 */
static int atapi_update_req_len(struct ide_internal * ide, struct ide_channel * channel, uint_t xfer_len) {
    struct ide_drive * drive = get_selected_drive(channel);

    //PrintDebug("\tUpdating request length (pre=%d)\n", drive->req_len);

    if (drive->req_len == 0) {
	PrintError("ATAPI Error: request of length 0\n");
	return -1;
    }


    channel->status.busy = 0;
    channel->status.data_req = 1;
    channel->status.error = 0;

    drive->irq_flags.io_dir = 1;
    drive->irq_flags.c_d = 0;

    // make count even
    if (drive->req_len % 2) {
	drive->req_len -= 1;
    }

    // if the device can't return as much as the OS requested
    // this is actually a decrement of the req_len by the amount requested by the OS
    if (drive->req_len > xfer_len) {
	drive->req_len = xfer_len;
    }

    //    PrintDebug("\tUpdating request length (post=%d)\n", drive->req_len);

    return 0;
}



// This is for simple commands that don't need to sanity check the req_len
static void atapi_setup_cmd_resp(struct ide_internal * ide, struct ide_channel * channel, uint_t xfer_len) {
    struct ide_drive * drive = get_selected_drive(channel);

    drive->transfer_length = xfer_len;
    drive->transfer_index = 0;
    drive->req_len = drive->transfer_length;

    drive->irq_flags.io_dir = 1;
    drive->irq_flags.c_d = 0;

    channel->status.busy = 0;
    channel->status.error = 0;

    if (drive->transfer_length > 0) {
	channel->status.data_req = 1;
    }

    ide_raise_irq(ide, channel);
}

static void atapi_cmd_error(struct ide_internal * ide, struct ide_channel * channel, 
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


    drive->irq_flags.io_dir = 1;
    drive->irq_flags.c_d = 1;
    drive->irq_flags.rel = 0;

    ide_raise_irq(ide, channel);
}


static void atapi_cmd_nop(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    channel->status.busy = 0;
    channel->status.ready = 1;
    channel->status.data_req = 0;
    channel->status.error = 0;

    drive->irq_flags.io_dir = 1;
    drive->irq_flags.c_d = 1;
    drive->irq_flags.rel = 0;

    ide_raise_irq(ide, channel);
}



static int atapi_read_chunk(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    int ret = drive->ops->read(drive->data_buf, 
			       drive->current_lba * ATAPI_BLOCK_SIZE, 
			       ATAPI_BLOCK_SIZE, drive->private_data);
    
    if (ret == -1) {
	PrintError("IDE: Error reading CD block (LBA=%p)\n", (void *)(addr_t)(drive->current_lba));
	return -1;
    }

    return 0;
}


static int atapi_update_data_buf(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);    
    
    switch (drive->cd_state.atapi_cmd) {
	case 0x28: // read(10)
	case 0xa8: // read(12)

	    // Update lba address to point to next block  
	    drive->current_lba++;

	    // read the next block
	    return atapi_read_chunk(ide, channel);

	default:
	    PrintError("Unhandled ATAPI command in update buffer %x\n", drive->cd_state.atapi_cmd);
	    return -1;
    }

    return 0;
}

static int atapi_read10(struct guest_info * core, 
			struct ide_internal * ide,
			struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_read10_cmd * cmd = (struct atapi_read10_cmd *)(drive->data_buf);
    uint32_t lba =  be_to_le_32(cmd->lba);
    uint16_t xfer_len = be_to_le_16(cmd->xfer_len);

    PrintDebug("READ10: XferLen=%d ; LBA=%x \n", xfer_len, lba );

    /* Check if cd is ready
     * if not: atapi_cmd_error(... ATAPI_SEN_NOT_RDY, ASC_MEDIA_NOT_PRESENT)
     */
    
    if (xfer_len == 0) {
	atapi_cmd_nop(ide, channel);
	return 0;
    }
    
    if ((lba + xfer_len) > (drive->ops->get_capacity(drive->private_data) / ATAPI_BLOCK_SIZE)) {
	PrintError("IDE: xfer len exceeded capacity (lba=%d) (xfer_len=%d) (ReadEnd=%d) (capacity=%d)\n", 
		   lba, xfer_len, lba + xfer_len, 
		   (uint32_t)drive->ops->get_capacity(drive->private_data));
	atapi_cmd_error(ide, channel, ATAPI_SEN_ILL_REQ, ASC_LOG_BLK_OOR);
	ide_raise_irq(ide, channel);
	return 0;
    }
	
    // PrintDebug("Reading %d blocks from LBA 0x%x\n", xfer_len, lba);
    drive->current_lba = lba;
	
    // Update the request length value in the cylinder registers
    drive->transfer_length = xfer_len * ATAPI_BLOCK_SIZE;
    drive->transfer_index = 0;	

    if (channel->features.dma) {

	if (channel->dma_status.active == 1) {
	    if (dma_read(core, ide, channel) == -1) {
		PrintError("Error in DMA read for CD Read10 command\n");
		return -1;
	    }
	}
	return 0;
    }

    if (atapi_read_chunk(ide, channel) == -1) {
	PrintError("IDE: Could not read initial chunk from CD\n");
	return -1;
    }
	
    // Length of ATAPI buffer sits in cylinder registers
    // This is weird... The host sets this value to say what it would like to transfer, 
    // if it is larger than the correct size, the device shrinks it to the correct size
    if (atapi_update_req_len(ide, channel, ATAPI_BLOCK_SIZE) == -1) {
	PrintError("Could not update initial request length\n");
	return -1;
    }
    
    ide_raise_irq(ide, channel);

    return 0;
}



static void atapi_req_sense(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);

    memcpy(drive->data_buf, drive->cd_state.sense.buf, sizeof(drive->cd_state.sense.buf));
   
    atapi_setup_cmd_resp(ide, channel, 18);
}



static int atapi_get_capacity(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_rd_capacity_resp * resp = (struct atapi_rd_capacity_resp *)(drive->data_buf);
    uint32_t capacity = drive->ops->get_capacity(drive->private_data);

    resp->lba = le_to_be_32((capacity / ATAPI_BLOCK_SIZE) - 1);
    resp->block_len = le_to_be_32(ATAPI_BLOCK_SIZE);

    atapi_setup_cmd_resp(ide, channel, sizeof(struct atapi_rd_capacity_resp));

    return 0;
}

static int atapi_get_config(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_config_cmd * cmd = (struct atapi_config_cmd *)(drive->data_buf);
    uint16_t alloc_len = be_to_le_16(cmd->alloc_len);
    struct atapi_config_resp * resp = (struct atapi_config_resp *)(drive->data_buf);
    int xfer_len = 8;

    memset(resp, 0, sizeof(struct atapi_config_resp));

    resp->data_len = le_to_be_32(xfer_len - 4);

    if (alloc_len < xfer_len) {
	xfer_len = alloc_len;
    }
    
    V3_Print("ATAPI Get config: xfer_len=%d\b", xfer_len);

    atapi_setup_cmd_resp(ide, channel, xfer_len);
    
    return 0;
}


static int atapi_read_toc(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_rd_toc_cmd * cmd = (struct atapi_rd_toc_cmd *)(drive->data_buf);
    uint16_t alloc_len = be_to_le_16(cmd->alloc_len);
    struct atapi_rd_toc_resp * resp = (struct atapi_rd_toc_resp *)(drive->data_buf);

    int xfer_len = 12;

    memset(resp, 0, sizeof(struct atapi_rd_toc_resp));
    
    resp->data_len = le_to_be_16(10);
    resp->first_track_num = 1;
    resp->last_track_num = 1;

    // we don't handle multi session
    // we'll just treat it the same as single session
    if ((cmd->format == 0) || (cmd->format == 1)) {
	memset(&(resp->track_descs[0]), 0, 8);
	
	if (alloc_len < xfer_len) {
	    xfer_len = alloc_len;
	}

	atapi_setup_cmd_resp(ide, channel, xfer_len);
    } else {
	PrintError("Unhandled Format (%d)\n", cmd->format);
	return -1;
    }

    return 0;
}


static int atapi_mode_sense_cur_values(struct ide_internal * ide, struct ide_channel * channel, 
				       struct atapi_mode_sense_cmd * sense_cmd) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_mode_sense_hdr * hdr = (struct atapi_mode_sense_hdr *)(drive->data_buf);
    uint_t resp_len = sizeof(struct atapi_mode_sense_hdr);
    uint16_t alloc_len = be_to_le_16(sense_cmd->alloc_len);
    PrintDebug("Page Code: %x\n", sense_cmd->page_code);
    PrintDebug("Alloc len: %d\n", alloc_len);

    switch (sense_cmd->page_code) {

	case 0x01: {	// error recovery
	    struct atapi_error_recovery * err = NULL;
	    err = (struct atapi_error_recovery *)(drive->data_buf + 
						  sizeof(struct atapi_mode_sense_hdr));


	    memcpy(err, &(drive->cd_state.err_recovery), sizeof(struct atapi_error_recovery));

	    resp_len += sizeof(struct atapi_error_recovery);

	    PrintError("mode sense (error recovery) resp_len=%d\n", resp_len);


	    hdr->mode_data_len = le_to_be_16(resp_len - 2);
	    
	    break;
	}
	case 0x2a: { // CDROM caps and mech. status

	    uint8_t * buf = drive->data_buf;



	    PrintError("mode sense (caps/mechs v2) resp_len=%d\n", resp_len);

	    *((uint16_t *)buf) = le_to_be_16(28 + 6);
	    buf[2] = 0x70;
	    buf[3] = 0;
	    buf[4] = 0;
	    buf[5] = 0;
	    buf[6] = 0;
	    buf[7] = 0;

	    buf[8] = 0x2a;
	    buf[9] = 0x12;
	    buf[10] = 0x00;
	    buf[11] = 0x00;

	    /* Claim PLAY_AUDIO capability (0x01) since some Linux
	       code checks for this to automount media. */
	    buf[12] = 0x71;
	    buf[13] = 3 << 5;
	    buf[14] = (1 << 0) | (1 << 3) | (1 << 5);

	    buf[6] |= 1 << 1;
	    buf[15] = 0x00;
	    *((uint16_t *)&(buf[16])) = le_to_be_16(706);
	    buf[18] = 0;
	    buf[19] = 2;
	    *((uint16_t *)&(buf[20])) = le_to_be_16(512);
	    *((uint16_t *)&(buf[22])) = le_to_be_16(706);
	    buf[24] = 0;
	    buf[25] = 0;
	    buf[26] = 0;
	    buf[27] = 0;

	    resp_len = 28;

#if 0
	    struct atapi_cdrom_caps * caps = NULL;
	    caps = (struct atapi_cdrom_caps *)(drive->data_buf + sizeof(struct atapi_mode_sense_hdr));
	    



	    memset(caps, 0, sizeof(struct atapi_cdrom_caps));

	    resp_len += sizeof(struct atapi_cdrom_caps);

	    hdr->mode_data_len = le_to_be_16(resp_len - 2);


	    PrintError("mode sense (caps/mechs v2) resp_len=%d\n", resp_len);

	    caps->page_code = 0x2a;
	    caps->page_len = 0x12;
	    caps->mode2_form1 = 1;
	    caps->mode2_form2 = 1;
	    caps->multisession = 1;
	    caps->isrc = 1;
	    caps->upc = 1;

	    /* JRL TODO: These are dynamic caps */
	    caps->lock = 1;
	    caps->lock_state = 0;
	    caps->eject = 1;

 	    caps->lmt = 1;
	    caps->obsolete1 = le_to_be_16(0x2c2);
	    caps->num_vols_supp = le_to_be_16(2);

	    caps->lun_buf_size = le_to_be_16(512);
	    caps->obsolete2 = le_to_be_16(0x2c2);

#endif

	    break;
	}
	case 0x0d:
	case 0x0e:
	case 0x3f:
	default:
	    PrintError("ATAPI: Mode sense Page Code not supported (%x)\n", sense_cmd->page_code);
	    atapi_cmd_error(ide, channel, ATAPI_SEN_ILL_REQ, ASC_INV_CMD_FIELD);
	    ide_raise_irq(ide, channel);
	    return 0;
    }


    // We do this after error checking, because its only valid if everything worked
    //    memset(hdr, 0, sizeof(struct atapi_mode_sense_hdr));
    // hdr->media_type_code = 0x70;

    PrintDebug("resp_len=%d\n", resp_len);

    drive->transfer_length = (resp_len > alloc_len) ? alloc_len : resp_len;
    drive->transfer_index = 0;
    atapi_update_req_len(ide, channel, drive->transfer_length);

    ide_raise_irq(ide, channel);

    return 0;
}


static int atapi_mode_sense(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_mode_sense_cmd * sense_cmd = (struct atapi_mode_sense_cmd *)(drive->data_buf);

    switch (sense_cmd->page_ctrl) {
	case 0x00: // Current values
	    return atapi_mode_sense_cur_values(ide, channel, sense_cmd);
	case 0x01: // Changeable values
	case 0x02: // default values
	case 0x03: // saved values
	default:
	    PrintError("ATAPI: Mode sense mode not supported (%x)\n", sense_cmd->page_ctrl);
	    return -1;
    }
    return 0;
}



static int atapi_inquiry(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_inquiry_cmd * inquiry_cmd = (struct atapi_inquiry_cmd *)(drive->data_buf);
    uint16_t alloc_len = be_to_le_16(inquiry_cmd->alloc_len);
    struct atapi_inquiry_resp * resp = (struct atapi_inquiry_resp *)(drive->data_buf);
    int xfer_len = sizeof(struct atapi_inquiry_resp);
    const char * vendor_id = "VTAB    ";
    const char * product_id = "Turbo CD-ROM    ";
    const char * product_rev = "1.0 ";

    memset(resp, 0, sizeof(struct atapi_inquiry_resp));
    
    resp->dev_type = DEV_TYPE_CDROM;
    resp->removable_media = 1;
    resp->resp_data_fmt = 0x1;
    resp->atapi_trans_ver = 0x2;
    resp->additional_len = 31;

    memcpy(resp->t10_vendor_id, vendor_id, strlen(vendor_id));
    memcpy(resp->product_id, product_id, strlen(product_id));
    memcpy(resp->product_rev, product_rev, strlen(product_rev));
    
    if (alloc_len < xfer_len) {
	xfer_len = alloc_len;
    }

    atapi_setup_cmd_resp(ide, channel, xfer_len);

    return 0;
}


static int atapi_mech_status(struct ide_internal * ide, struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    struct atapi_mech_status_cmd * status_cmd = (struct atapi_mech_status_cmd *)(drive->data_buf);
    uint16_t alloc_len = be_to_le_16(status_cmd->alloc_len);
    struct atapi_mech_status_resp * resp = (struct atapi_mech_status_resp *)(drive->data_buf);
    int xfer_len = sizeof(struct atapi_mech_status_resp);

    memset(resp, 0, sizeof(struct atapi_mech_status_resp));

    resp->lba = le_to_be_32(1);
    resp->slot_table_len = le_to_be_16(0);
    
    if (alloc_len < xfer_len) {
	xfer_len = alloc_len;
    }

    atapi_setup_cmd_resp(ide, channel, xfer_len);

    return 0;
}


static int atapi_cmd_is_data_op(uint8_t cmd) {
    switch (cmd) {
	case 0x28: // read (10)
	case 0xa8: // read (12)
	case 0x2a: // write (10)
	case 0xaa: // write (12)
	    return 1;
	default:
	    return 0;
    } 
}


static int atapi_handle_packet(struct guest_info * core, struct ide_internal * ide, struct ide_channel * channel) {
   struct ide_drive * drive = get_selected_drive(channel);
   uint8_t cmd = drive->data_buf[0];

   PrintDebug("IDE: ATAPI Command %x\n", cmd);

   drive->cd_state.atapi_cmd = cmd;

   switch (cmd) {
       case 0x00: // test unit ready
	   atapi_cmd_nop(ide, channel);

	   /* if drive not ready: 
	      atapi_cmd_error(... ATAPI_SEN_NOT_RDY, ASC_MEDIA_NOT_PRESENT)
 	   */
	   break;
       case 0x03: // request sense
	   PrintError("IDE: Requesting Sense (0x3)\n");
	   atapi_req_sense(ide, channel);
	   break;

       case 0x1e: // lock door
	   atapi_cmd_nop(ide, channel);
	   break;

       case 0x28: // read(10)
	   if (atapi_read10(core, ide, channel) == -1) {
	       PrintError("IDE: Error in ATAPI read (%x)\n", cmd);
	       return -1;
	   }
	   break;

       case 0x5a: // mode sense
	   if (atapi_mode_sense(ide, channel) == -1) {
	       PrintError("IDE: Error in ATAPI mode sense (%x)\n", cmd);
	       return -1;
	   }
	   break;


       case 0x25: // read cdrom capacity
	   if (atapi_get_capacity(ide, channel) == -1) {
	       PrintError("IDE: Error getting CDROM capacity (%x)\n", cmd);
	       return -1;
	   }
	   break;


       case 0x43: // read TOC
	   if (atapi_read_toc(ide, channel) == -1) {
	       PrintError("IDE: Error getting CDROM TOC (%x)\n", cmd);
	       return -1;
	   }
	   break;

       case 0x46: // get configuration
	   if (atapi_get_config(ide, channel) == -1) {
	       PrintError("IDE: Error getting CDROM Configuration (%x)\n", cmd);
	       return -1;
	   }
	   break;


       case 0x4a: // Get Status/event
       case 0x51: // read disk info
	   // no-op to keep the Linux CD-ROM driver happy
	   PrintDebug("Error: Read disk info no-op to keep the Linux CD-ROM driver happy\n");
	   atapi_cmd_error(ide, channel, ATAPI_SEN_ILL_REQ, ASC_INV_CMD_FIELD);
	   ide_raise_irq(ide, channel);
	   break;

       case 0x12: // inquiry
	   if (atapi_inquiry(ide, channel) == -1) {
	       PrintError("IDE: Error in ATAPI inquiry (%x)\n", cmd);
	       return -1;
	   }
	   break;

       case 0xbd: // mechanism status 
	   if (atapi_mech_status(ide, channel) == -1) {
	       PrintError("IDE: error in ATAPI Mechanism status query (%x)\n", cmd);
	       return -1;
	   }
	   break;


       case 0xa8: // read(12)


       case 0x1b: // start/stop drive

       case 0xbe: // read cd



       case 0x2b: // seek

       case 0x42: // read sub-channel


	   
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

       default:
	   PrintError("Unhandled ATAPI command %x\n", cmd);
	   atapi_cmd_error(ide, channel, ATAPI_SEN_ILL_REQ, ASC_INV_CMD_FIELD);
	   ide_raise_irq(ide, channel);
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

    // enable DMA access
    /* Disabled until command packet DMA is fixed */
    drive_id->dma_enable = 1;

    // enable LBA access
    drive_id->lba_enable = 1;
    
    drive_id->rw_multiples = 0x80ff;

    // words 64-70, 54-58 valid
    /* Disabled until command packet DMA is fixed */
        drive_id->field_valid = 0x0007; // DMA + pkg cmd valid

    // copied from CFA540A
    /* Disabled until command packet DMA is fixed */
     drive_id->buf[63] = 0x0103; // variable (DMA stuff)
       

    /* uncommented to disable dma(?) */
     // drive_id->buf[64] = 0x0001; // PIO


    drive_id->buf[65] = 0x00b4;
    drive_id->buf[66] = 0x00b4;
    drive_id->buf[67] = 0x012c;
    drive_id->buf[68] = 0x00b4;

    drive_id->buf[71] = 30; // faked
    drive_id->buf[72] = 30; // faked

    //    drive_id->buf[80] = 0x1e; // supports up to ATA/ATAPI-4
    drive_id->major_rev_num = 0x0040; // supports up to ATA/ATAPI-6

    /* Disabled until command packet DMA is fixed */
    drive_id->dma_ultra = 0x2020; // Ultra_DMA_Mode_5_Selected | Ultra_DMA_Mode_5_Supported;
}
