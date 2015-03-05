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

#ifndef _DEVICES_ATA_H_
#define _DEVICES_ATA_H_

#define MAX_MULT_SECTORS  255





static void ata_identify_device(struct ide_drive * drive) {
    struct ide_drive_id * drive_id = (struct ide_drive_id *)(drive->data_buf);
    const char* serial_number = " VT00001\0\0\0\0\0\0\0\0\0\0\0\0";
    const char* firmware = "ALPHA1  ";

    drive->transfer_length = 512;
    drive->transfer_index = 0;


    memset(drive_id->buf, 0, sizeof(drive_id->buf));

    drive_id->fixed_drive = 1;
    drive_id->removable_media = 0;

    // Black magic...
    drive_id->disk_speed1 = 1;
    drive_id->disk_speed3 = 1;

    drive_id->cdrom_flag = 0;

    drive_id->num_cylinders = drive->num_cylinders;
    drive_id->num_heads = drive->num_heads;
    drive_id->bytes_per_track = drive->num_sectors * HD_SECTOR_SIZE;
    drive_id->bytes_per_sector = HD_SECTOR_SIZE;
    drive_id->sectors_per_track = drive->num_sectors;


    // These buffers do not contain a terminating "\0"
    memcpy(drive_id->serial_num, serial_number, strlen(serial_number));
    memcpy(drive_id->firmware_rev, firmware, strlen(firmware));
    memcpy(drive_id->model_num, drive->model, 40);

    // 32 bits access for PIO supported
    drive_id->dword_io = 1;


    // enable DMA access
    // We want guest to assume UDMA5
    // but any DMA model looks the same to the guest
    drive_id->dma_enable = 1; 

    // enable LBA access
    drive_id->lba_enable = 1;
    
    // Drive Capacity (28 bit LBA)
    drive_id->lba_capacity = drive->ops->get_capacity(drive->private_data) / HD_SECTOR_SIZE;
    
    // Drive Capacity (48 bit LBA)
    drive_id->lba_capacity_2 = drive->ops->get_capacity(drive->private_data) / HD_SECTOR_SIZE;


    // lower byte is the maximum multiple sector size...
    drive_id->rw_multiples = 0x8000 | MAX_MULT_SECTORS;


    // words 64-70, 54-58 valid
    drive_id->field_valid = 0x0007; // DMA + pkg cmd valid


    // copied from CFA540A
    // drive_id->buf[63] = 0x0103; // variable (DMA stuff)
    //drive_id->buf[63] = 0x0000; // variable (DMA stuff)
    // 0x0007 => MWDMA modes 0..2 supported - none selected
    drive_id->buf[63] = 0x0007;

    
    // We wll support PIO mode 0
    // Maybe revisit this later to allow advanced modes
    // We really want the guest to use DMA
    drive_id->buf[64] = 0x0001; // PIO

    // MWDMA transfer min cycle time
    drive_id->buf[65] = 0x00b4;
    // MWDMA transfer time recommended
    drive_id->buf[66] = 0x00b4;
    // minimum pio transfer time without flow control
    drive_id->buf[67] = 0x012c;
    // minimum pio transfer time with IORDY flow control
    drive_id->buf[68] = 0x00b4;

    drive_id->buf[71] = 30; // faked
    drive_id->buf[72] = 30; // faked

    // queue depth set to one
    // We should not expect queued DMAs

    //    drive_id->buf[80] = 0x1e; // supports up to ATA/ATAPI-4
    drive_id->major_rev_num = 0x0040; // supports up to ATA/ATAPI-6


    drive_id->buf[83] |= 0x0400; // supports 48 bit LBA

    // No special features supported

    // Pretend drive is already autoconfed to UDMA5
    drive_id->dma_ultra = 0x2020; // Ultra_DMA_Mode_5_Selected | Ultra_DMA_Mode_5_Supported;

}


static int ata_read(struct ide_internal * ide, struct ide_channel * channel, uint8_t * dst, uint_t sect_cnt) {
    struct ide_drive * drive = get_selected_drive(channel);

    if (drive->hd_state.accessed == 0) {
	drive->current_lba = 0;
	drive->hd_state.accessed = 1;
    }

    PrintDebug(VM_NONE, VCORE_NONE,"Reading Drive LBA=%d (count=%d)\n", (uint32_t)(drive->current_lba), sect_cnt);

    int ret = drive->ops->read(dst, drive->current_lba * HD_SECTOR_SIZE, sect_cnt * HD_SECTOR_SIZE, drive->private_data);
    
    if (ret == -1) {
	PrintError(VM_NONE, VCORE_NONE,"IDE: Error reading HD block (LBA=%p)\n", (void *)(addr_t)(drive->current_lba));
	return -1;
    }

    return 0;
}



static int ata_write(struct ide_internal * ide, struct ide_channel * channel, uint8_t * src, uint_t sect_cnt) {
    struct ide_drive * drive = get_selected_drive(channel);

    if (drive->hd_state.accessed == 0) {
	PrintError(VM_NONE,VCORE_NONE,"Reseting lba...\n");
	drive->current_lba = 0;
	drive->hd_state.accessed = 1;
    }

    PrintDebug(VM_NONE, VCORE_NONE,"Writing Drive LBA=%d (count=%d)\n", (uint32_t)(drive->current_lba), sect_cnt);

    int ret = drive->ops->write(src, drive->current_lba * HD_SECTOR_SIZE, sect_cnt * HD_SECTOR_SIZE, drive->private_data);

    if (ret == -1) {
	PrintError(VM_NONE, VCORE_NONE,"IDE: Error writing HD block (LBA=%p)\n", (void *)(addr_t)(drive->current_lba));
	return -1;
    }

    return 0;
}


//
// Grand unified conversion for various addressing modes:
//
// CHS    => 64 bit LBA (8 bit sector count)
// LBA28  => 64 bit LBA (8 bit sector count)
// LBA48  => 64 bit LBA (16 bit sector count)
static int ata_get_lba_and_size(struct ide_internal * ide, struct ide_channel * channel, uint64_t * lba, uint64_t *num_sects) {
    struct ide_drive * drive = get_selected_drive(channel);

    if (is_lba48(channel)) {
	*num_sects = drive->lba48.sector_count;
	*lba = drive->lba48.lba;
	PrintDebug(VM_NONE,VCORE_NONE,"get_lba: lba48: lba=%llu, num_sects=%llu\n",*lba,*num_sects);
    } else {
	// LBA48 or CHS
	// The if the sector count == 0 then read 256 sectors (cast up to handle that value)
	*num_sects = (drive->sector_count == 0) ? 256 : drive->sector_count;
	
	if (is_lba28(channel)) { 
	    union {
		uint32_t addr;
		uint8_t buf[4];
	    } __attribute__((packed)) lba_addr;
	    
	    /* LBA addr bits:
	       0-8: sector number reg  (drive->lba0)
	       8-16: low cylinder reg (drive->lba1)
	       16-24: high cylinder reg (drive->lba2)
	       24-28:  low 4 bits of drive_head reg (channel->drive_head.head_num)
	    */
	    
	    
	    lba_addr.buf[0] = drive->lba0;
	    lba_addr.buf[1] = drive->lba1;
	    lba_addr.buf[2] = drive->lba2;
	    lba_addr.buf[3] = channel->drive_head.lba3;
	    
	    *lba = lba_addr.addr;
	    
	    PrintDebug(VM_NONE,VCORE_NONE,"get_lba: lba28: lba0=%u (sect), lba1=%u (cyllow), lba2=%u (cylhigh), lba3=%d (head) => lba=%llu numsects=%llu\n", drive->lba0, drive->lba1, drive->lba2, channel->drive_head.lba3, *lba, *num_sects);
	    
	} else {
	    // we are in CHS mode....
	    
	    *lba = 
		(drive->cylinder * drive->num_heads + 
		 channel->drive_head.head_num) * drive->num_sectors +
		// sector number is 1 based
		(drive->sector_num - 1);
	    
	    
	    PrintDebug(VM_NONE,VCORE_NONE,"get_lba: Huh, 1995 has returned - CHS (%u,%u,%u) addressing on drive of (%u,%u,%u) translated as lba=%llu num_sects=%llu....\n", 
		       drive->cylinder, channel->drive_head.head_num, drive->sector_num,
		       drive->num_cylinders, drive->num_heads, drive->num_sectors, *lba,*num_sects );
	}
    }
	
    if ((*lba + *num_sects) > 
	drive->ops->get_capacity(drive->private_data) / HD_SECTOR_SIZE) {
	PrintError(VM_NONE, VCORE_NONE,"IDE: request size exceeds disk capacity (lba=%llu) (num_sects=%llu) (ReadEnd=%llu) (capacity=%llu)\n", 
		   *lba, *num_sects, 
		   *lba + (*num_sects * HD_SECTOR_SIZE),
		   drive->ops->get_capacity(drive->private_data));
	return -1;
    }
    
    return 0;
}


static int ata_write_sectors(struct ide_internal * ide,  struct ide_channel * channel) {
    uint64_t sect_cnt;
    struct ide_drive * drive = get_selected_drive(channel);

    if (ata_get_lba_and_size(ide, channel, &(drive->current_lba), &sect_cnt ) == -1) {
	PrintError(VM_NONE,VCORE_NONE,"Cannot get lba+size\n");
        ide_abort_command(ide, channel);
        return 0;
    }

    PrintDebug(VM_NONE,VCORE_NONE,"ata write sectors: lba=%llu sect_cnt=%llu\n", drive->current_lba, sect_cnt);

    drive->transfer_length = sect_cnt * HD_SECTOR_SIZE ;
    drive->transfer_index = 0;
    channel->status.busy = 0;
    channel->status.ready = 0;
    channel->status.write_fault = 0;
    channel->status.data_req = 1;
    channel->status.error = 0;

    PrintDebug(VM_NONE, VCORE_NONE, "IDE: Returning from write sectors\n");

    return 0;
}


// 28 bit LBA or CHS
static int ata_read_sectors(struct ide_internal * ide,  struct ide_channel * channel) {
    uint64_t sect_cnt;
    struct ide_drive * drive = get_selected_drive(channel);

    if (ata_get_lba_and_size(ide, channel, &(drive->current_lba),&sect_cnt) == -1) {
	PrintError(VM_NONE,VCORE_NONE,"Cannot get lba+size\n");
	ide_abort_command(ide, channel);
	return 0;
    }

    
    if (ata_read(ide, channel, drive->data_buf, 1) == -1) {
	PrintError(VM_NONE, VCORE_NONE,"Could not read disk sector\n");
	return -1;
    }

    drive->transfer_length = sect_cnt * HD_SECTOR_SIZE;
    drive->transfer_index = 0;

    channel->status.busy = 0;
    channel->status.ready = 0;
    channel->status.write_fault = 0;
    channel->status.data_req = 1;
    channel->status.error = 0;

    ide_raise_irq(ide, channel);

    PrintDebug(VM_NONE, VCORE_NONE,"Returning from read sectors\n");

    return 0;
}


/* ATA COMMANDS as per */
/* ACS-2 T13/2015-D Table B.2 Command codes */
#define ATA_NOP				0x00
#define CFA_REQ_EXT_ERROR_CODE		0x03 
#define ATA_DSM                         0x06
#define ATA_DEVICE_RESET		0x08
#define ATA_RECAL                       0x10 
#define ATA_READ			0x20 
#define ATA_READ_ONCE                   0x21 
#define ATA_READ_EXT			0x24 
#define ATA_READDMA_EXT			0x25 
#define ATA_READDMA_QUEUED_EXT          0x26 
#define ATA_READ_NATIVE_MAX_EXT		0x27 
#define ATA_MULTREAD_EXT		0x29 
#define ATA_WRITE			0x30 
#define ATA_WRITE_ONCE                  0x31 
#define ATA_WRITE_EXT			0x34 
#define ATA_WRITEDMA_EXT		0x35 
#define ATA_WRITEDMA_QUEUED_EXT		0x36 
#define ATA_SET_MAX_EXT                 0x37 
#define ATA_SET_MAX_EXT			0x37 
#define CFA_WRITE_SECT_WO_ERASE		0x38 
#define ATA_MULTWRITE_EXT		0x39 
#define ATA_WRITE_VERIFY                0x3C 
#define ATA_VERIFY			0x40 
#define ATA_VERIFY_ONCE                 0x41 
#define ATA_VERIFY_EXT			0x42 
#define ATA_SEEK                        0x70 
#define CFA_TRANSLATE_SECTOR		0x87 
#define ATA_DIAGNOSE			0x90
#define ATA_SPECIFY                     0x91 
#define ATA_DOWNLOAD_MICROCODE		0x92
#define ATA_STANDBYNOW2                 0x94 
#define ATA_IDLEIMMEDIATE2              0x95 
#define ATA_STANDBY2                    0x96 
#define ATA_SETIDLE2                    0x97 
#define ATA_CHECKPOWERMODE2             0x98 
#define ATA_SLEEPNOW2                   0x99 
#define ATA_PACKETCMD			0xA0 
#define ATA_PIDENTIFY			0xA1 
#define ATA_QUEUED_SERVICE              0xA2 
#define ATA_SMART			0xB0 
#define CFA_ACCESS_METADATA_STORAGE	0xB8
#define CFA_ERASE_SECTORS       	0xC0 
#define ATA_MULTREAD			0xC4 
#define ATA_MULTWRITE			0xC5 
#define ATA_SETMULT			0xC6 
#define ATA_READDMA			0xC8 
#define ATA_READDMA_ONCE                0xC9 
#define ATA_WRITEDMA			0xCA 
#define ATA_WRITEDMA_ONCE               0xCB 
#define ATA_WRITEDMA_QUEUED		0xCC 
#define CFA_WRITE_MULTI_WO_ERASE	0xCD 
#define ATA_GETMEDIASTATUS              0xDA 
#define ATA_DOORLOCK                    0xDE 
#define ATA_DOORUNLOCK                  0xDF 
#define ATA_STANDBYNOW1			0xE0
#define ATA_IDLEIMMEDIATE		0xE1 
#define ATA_STANDBY             	0xE2 
#define ATA_SETIDLE1			0xE3
#define ATA_READ_BUFFER			0xE4 
#define ATA_CHECKPOWERMODE1		0xE5
#define ATA_SLEEPNOW1			0xE6
#define ATA_FLUSH_CACHE			0xE7
#define ATA_WRITE_BUFFER		0xE8 
#define ATA_FLUSH_CACHE_EXT		0xEA 
#define ATA_IDENTIFY			0xEC 
#define ATA_MEDIAEJECT                  0xED 
#define ATA_SETFEATURES			0xEF 
#define IBM_SENSE_CONDITION             0xF0 
#define ATA_SECURITY_SET_PASS		0xF1
#define ATA_SECURITY_UNLOCK		0xF2
#define ATA_SECURITY_ERASE_PREPARE	0xF3
#define ATA_SECURITY_ERASE_UNIT		0xF4
#define ATA_SECURITY_FREEZE_LOCK	0xF5
#define CFA_WEAR_LEVEL                  0xF5 
#define ATA_SECURITY_DISABLE		0xF6

#endif
