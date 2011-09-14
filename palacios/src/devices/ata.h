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

    // Make it the simplest drive possible (1 head, 1 cyl, 1 sect/track)
    drive_id->num_cylinders = drive->num_cylinders;
    drive_id->num_heads = drive->num_heads;
    drive_id->bytes_per_track = drive->num_sectors * HD_SECTOR_SIZE;
    drive_id->bytes_per_sector = HD_SECTOR_SIZE;
    drive_id->sectors_per_track = drive->num_sectors;


    // These buffers do not contain a terminating "\0"
    memcpy(drive_id->serial_num, serial_number, strlen(serial_number));
    memcpy(drive_id->firmware_rev, firmware, strlen(firmware));
    memcpy(drive_id->model_num, drive->model, 40);

    // 32 bits access
    drive_id->dword_io = 1;


    // enable DMA access
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
    drive_id->buf[63] = 0x0007;

    
    //    drive_id->buf[64] = 0x0001; // PIO
    drive_id->buf[65] = 0x00b4;
    drive_id->buf[66] = 0x00b4;
    drive_id->buf[67] = 0x012c;
    drive_id->buf[68] = 0x00b4;

    drive_id->buf[71] = 30; // faked
    drive_id->buf[72] = 30; // faked

    //    drive_id->buf[80] = 0x1e; // supports up to ATA/ATAPI-4
    drive_id->major_rev_num = 0x0040; // supports up to ATA/ATAPI-6


    drive_id->buf[83] |= 0x0400; // supports 48 bit LBA


    drive_id->dma_ultra = 0x2020; // Ultra_DMA_Mode_5_Selected | Ultra_DMA_Mode_5_Supported;

}


static int ata_read(struct ide_internal * ide, struct ide_channel * channel, uint8_t * dst, uint_t sect_cnt) {
    struct ide_drive * drive = get_selected_drive(channel);

    if (drive->hd_state.accessed == 0) {
	drive->current_lba = 0;
	drive->hd_state.accessed = 1;
    }

    PrintDebug("Reading Drive LBA=%d (count=%d)\n", (uint32_t)(drive->current_lba), sect_cnt);

    int ret = drive->ops->read(dst, drive->current_lba * HD_SECTOR_SIZE, sect_cnt * HD_SECTOR_SIZE, drive->private_data);
    
    if (ret == -1) {
	PrintError("IDE: Error reading HD block (LBA=%p)\n", (void *)(addr_t)(drive->current_lba));
	return -1;
    }

    return 0;
}


static int ata_write(struct ide_internal * ide, struct ide_channel * channel, uint8_t * src, uint_t sect_cnt) {
    struct ide_drive * drive = get_selected_drive(channel);

    PrintDebug("Writing Drive LBA=%d (count=%d)\n", (uint32_t)(drive->current_lba), sect_cnt);

    int ret = drive->ops->write(src, drive->current_lba * HD_SECTOR_SIZE, sect_cnt * HD_SECTOR_SIZE, drive->private_data);

    if (ret == -1) {
	PrintError("IDE: Error writing HD block (LBA=%p)\n", (void *)(addr_t)(drive->current_lba));
	return -1;
    }

    return 0;
}



static int ata_get_lba(struct ide_internal * ide, struct ide_channel * channel, uint64_t * lba) {
    struct ide_drive * drive = get_selected_drive(channel);
    // The if the sector count == 0 then read 256 sectors (cast up to handle that value)
    uint32_t sect_cnt = (drive->sector_count == 0) ? 256 : drive->sector_count;

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


    if ((lba_addr.addr + sect_cnt) > 
	drive->ops->get_capacity(drive->private_data) / HD_SECTOR_SIZE) {
	PrintError("IDE: request size exceeds disk capacity (lba=%d) (sect_cnt=%d) (ReadEnd=%d) (capacity=%p)\n", 
		   lba_addr.addr, sect_cnt, 
		   lba_addr.addr + (sect_cnt * HD_SECTOR_SIZE),
		   (void *)(addr_t)(drive->ops->get_capacity(drive->private_data)));
	return -1;
    }

    *lba = lba_addr.addr;
    return 0;
}


// 28 bit LBA
static int ata_read_sectors(struct ide_internal * ide,  struct ide_channel * channel) {
    struct ide_drive * drive = get_selected_drive(channel);
    // The if the sector count == 0 then read 256 sectors (cast up to handle that value)
    uint32_t sect_cnt = (drive->sector_count == 0) ? 256 : drive->sector_count;

    if (ata_get_lba(ide, channel, &(drive->current_lba)) == -1) {
	ide_abort_command(ide, channel);
	return 0;
    }

    
    if (ata_read(ide, channel, drive->data_buf, 1) == -1) {
	PrintError("Could not read disk sector\n");
	return -1;
    }

    drive->transfer_length = sect_cnt * HD_SECTOR_SIZE;
    drive->transfer_index = 0;

    channel->status.busy = 0;
    channel->status.ready = 0;
    channel->status.write_fault = 0;
    channel->status.data_req = 1;
    channel->status.error = 0;

    drive->irq_flags.io_dir = 1;
    drive->irq_flags.c_d = 0;
    drive->irq_flags.rel = 0;


    ide_raise_irq(ide, channel);

    PrintDebug("Returning from read sectors\n");

    return 0;
}


// 48 bit LBA
static int ata_read_sectors_ext(struct ide_internal * ide, struct ide_channel * channel) {
    //struct ide_drive * drive = get_selected_drive(channel);
    // The if the sector count == 0 then read 256 sectors (cast up to handle that value)
    //uint32_t sector_count = (drive->sector_count == 0) ? 256 : drive->sector_count;

    PrintError("Extended Sector read not implemented\n");

    return -1;
}
