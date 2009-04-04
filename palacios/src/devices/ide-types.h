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


#ifndef __DEVICES_IDE_TYPES_H__
#define __DEVICES_IDE_TYPES_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>


struct ide_error_reg {
    union {
	uint8_t val;
	struct {
	    uint_t addr_mark_nf     : 1;
	    uint_t track0_nf        : 1;
	    uint_t abort            : 1;
	    uint_t rsvd0            : 1;
	    uint_t ID_nf            : 1;
	    uint_t rsvd1            : 1;
	    uint_t data_error       : 1;
	    uint_t bad_block        : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



struct ide_drive_head_reg {
    union {
	uint8_t val;
	struct {
	    uint_t head_num      : 4;
	    uint_t drive_sel     : 1;
	    uint_t rsvd1         : 1;
	    uint_t lba_mode      : 1;
	    uint_t rsvd2         : 1;
	} __attribute__((packed));

	struct {
	    uint_t lba3      : 4;
	    uint_t rsvd3     : 4;
	} __attribute__((packed));
	
    } __attribute__((packed));
} __attribute__((packed));



struct ide_status_reg {
    union {
	uint8_t val;
	struct {
	    uint_t error         : 1;
	    uint_t index         : 1;
	    uint_t corrected     : 1;
	    uint_t data_req      : 1;
	    uint_t seek_complete : 1;
	    uint_t write_fault   : 1;
	    uint_t ready         : 1;
	    uint_t busy          : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ide_ctrl_reg {
    union {
	uint8_t val;
	struct {
	    uint_t rsvd0        : 1;
	    uint_t irq_disable   : 1;
	    uint_t soft_reset   : 1;
	    uint_t rsvd1        : 5;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ide_features_reg {
    union {
	uint8_t val;
	uint8_t dma; // 1 == DMA, 0 = PIO
    } __attribute__((packed));
} __attribute__((packed));


struct ide_dma_cmd_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t start : 1;
	    uint8_t rsvd1 : 2;
	    uint8_t read  : 1;
	    uint8_t rsvd2 : 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



struct ide_dma_status_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t active  : 1;
	    uint8_t err     : 1;
	    uint8_t int_gen : 1;
	    uint8_t rsvd1   : 4;
	    uint8_t prd_int_status : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct ide_dma_prd {
    uint32_t base_addr;
    uint16_t size;
    uint16_t rsvd           : 15;
    uint8_t end_of_table    : 1;
} __attribute__((packed));



typedef enum {IDE_CTRL_NOT_SPECIFIED, 
	      IDE_CTRL_SINGLE_PORT, 
	      IDE_CTRL_DUAL_PORT, 
	      IDE_CTRL_DUAL_PORT_CACHE} ide_controller_type;

struct ide_drive_id {
    union {
	uint16_t buf[256];
	struct {
	    // 0
	    uint_t rsvd1           : 1;
	    uint_t hard_sectors    : 1;
	    uint_t no_soft_sectors : 1;
	    uint_t no_mfm_enc      : 1;
	    uint_t head_switch_time : 1;
	    uint_t spnd_mot_ctrl   : 1;
	    uint_t fixed_drive     : 1;
	    uint_t removable_media : 1;
	    uint_t disk_speed1     : 1;
	    uint_t disk_speed2     : 1;
	    uint_t disk_speed3     : 1;
	    uint_t rpm_tolerance   : 1;
	    uint_t data_strobe_offset : 1;
	    uint_t track_offset_option : 1;
	    uint_t fmt_speed_tol   : 1;
	    uint_t cdrom_flag      : 1;

	    // 1
	    uint16_t num_cylinders;

	    // 2
	    uint16_t rsvd2;
	    
	    // 3
	    uint16_t num_heads;
	    // 4
	    uint16_t bytes_per_track;
	    // 5
	    uint16_t bytes_per_sector;
	    // 6
	    uint16_t sectors_per_track;

	    // 7
	    uint16_t sector_gap;
	    
	    // 8
	    uint8_t phase_lock_bytes;
	    uint8_t rsvd3;

	    // 9
	    uint16_t num_vendor_wds;
	    
	    // 10
	    uint8_t serial_num[20]; // right aligned, padded with 0x20
	    
	    // 20
	    uint16_t controller_type;

	    // 21
	    uint16_t buffer_size; // in 512 byte chunks
	    
	    // 22
	    uint16_t num_ecc_bytes;

	    // 23
	    uint8_t firmware_rev[8]; // space padded

	    // 27
	    uint8_t model_num[40]; // space padded
	    
	    // 47
	    uint16_t rw_multiples;

	    // 48
	    uint16_t dword_io;

	    // 49
	    uint8_t rsvd4;
 	    uint8_t dma_enable : 1;
 	    uint8_t lba_enable : 1;
	    uint8_t IORDYsw    : 1;
	    uint8_t IORDYsup   : 1;
	    uint8_t rsvd5      : 4;

	    
	    // 50
	    uint16_t rsvd6;
	    
	    // 51
	    uint16_t min_PIO_cycle; // 0=slow, 1=medium, 2=fast
	    // 52
	    uint16_t min_DMA_cycle; // 0=slow, 1=medium, 2=fast

	    // 53
	    uint16_t field_valid; //  2:	ultra_ok	word  88
				  //  1:	eide_ok		words 64-70
				  //  0:	cur_ok		words 54-58
	    // 54
	    uint16_t cur_cyls;
	    // 55 
	    uint16_t cur_heads;
	    // 56
	    uint16_t cur_sectors;
	    // 57
	    uint16_t cur_capacity0;
	    // 58
	    uint16_t cur_capacity1;

	    // 59
	    uint8_t cur_mult_sect_cnt;
	    uint8_t mult_sect_valid; // bit0==0: valid

	    // 60
	    uint32_t lba_capacity;

	    // 62
	    uint16_t dma_lword;
	    // 63
	    uint16_t dma_mword;

	    // 64
	    uint16_t eide_pio_modes; // 0: (mode 3), 1: (mode 4)
	    // 65
	    uint16_t eide_dma_min; /* min mword dma cycle time (ns) */
	    // 66 
	    uint16_t eide_dma_time; /* recommended mword dma cycle time (ns) */
	    // 67
	    uint16_t eide_pio; /* min cycle time (ns), no IORDY  */
	    // 68
	    uint16_t eide_pio_iordy; /* min cycle time (ns), with IORDY */

	    // 69
	    uint16_t rsvd7[6];

	    // 75
	    uint16_t queue_depth;

	    // 76
	    uint16_t rsvd8[4];

	    // 80
	    uint16_t major_rev_num;
	    // 81
	    uint16_t minor_rev_num;
	    // 82
	    uint16_t cmd_set_1; /*  15:	Obsolete
				 * 14:	NOP command
				 * 13:	READ_BUFFER
				 * 12:	WRITE_BUFFER
				 * 11:	Obsolete
				 * 10:	Host Protected Area
				 *  9:	DEVICE Reset
				 *  8:	SERVICE Interrupt
				 *  7:	Release Interrupt
				 *  6:	look-ahead
				 *  5:	write cache
				 *  4:	PACKET Command
				 *  3:	Power Management Feature Set
				 *  2:	Removable Feature Set
				 *  1:	Security Feature Set
				 *  0:	S
				 */

	    // 83
	    uint16_t cmd_set_2; /*  15:	Shall be ZERO
				 * 14:	Shall be ONE
				 * 13:	FLUSH CACHE EXT
				 * 12:	FLUSH CACHE
				 * 11:	Device Configuration Overlay
				 * 10:	48-bit Address Feature Set
				 *  9:	Automatic Acoustic Management
				 *  8:	SET MAX security
				 *  7:	reserved 1407DT PARTIES
				 *  6:	SetF sub-command Power-Up
				 *  5:	Power-Up in Standby Feature Set
				 *  4:	Removable Media Notification
				 *  3:	APM Feature Set
				 *  2:	CFA Feature Set
				 *  1:	READ/WRITE DMA QUEUED
				 *  0:	Download MicroCode
				 */

	    // 84
	    uint16_t cfsse; 	/*  cmd set-feature supported extensions
				 * 15:	Shall be ZERO
				 * 14:	Shall be ONE
				 * 13:6	reserved
				 *  5:	General Purpose Logging
				 *  4:	Streaming Feature Set
				 *  3:	Media Card Pass Through
				 *  2:	Media Serial Number Valid
				 *  1:	SMART selt-test supported
				 *  0:	SMART error logging
				 */

	    // 85
	    uint16_t cfs_enable_1; 	/* command set-feature enabled
					 * 15:	Obsolete
					 * 14:	NOP command
					 * 13:	READ_BUFFER
					 * 12:	WRITE_BUFFER
					 * 11:	Obsolete
					 * 10:	Host Protected Area
					 *  9:	DEVICE Reset
					 *  8:	SERVICE Interrupt
					 *  7:	Release Interrupt
					 *  6:	look-ahead
					 *  5:	write cache
					 *  4:	PACKET Command
					 *  3:	Power Management Feature Set
					 *  2:	Removable Feature Set
					 *  1:	Security Feature Set
					 *  0:	SMART Feature Set
					 */
	    // 86
	    uint16_t cfs_enable_2;	/* command set-feature enabled
					 * 15:	Shall be ZERO
					 * 14:	Shall be ONE
					 * 13:	FLUSH CACHE EXT
					 * 12:	FLUSH CACHE
					 * 11:	Device Configuration Overlay
					 * 10:	48-bit Address Feature Set
					 *  9:	Automatic Acoustic Management
					 *  8:	SET MAX security
					 *  7:	reserved 1407DT PARTIES
					 *  6:	SetF sub-command Power-Up
					 *  5:	Power-Up in Standby Feature Set
					 *  4:	Removable Media Notification
					 *  3:	APM Feature Set
					 *  2:	CFA Feature Set
					 *  1:	READ/WRITE DMA QUEUED
					 *  0:	Download MicroCode
					 */
	    // 87
	    uint16_t csf_default;	/* command set-feature default
					 * 15:	Shall be ZERO
					 * 14:	Shall be ONE
					 * 13:6	reserved
					 *  5:	General Purpose Logging enabled
					 *  4:	Valid CONFIGURE STREAM executed
					 *  3:	Media Card Pass Through enabled
					 *  2:	Media Serial Number Valid
					 *  1:	SMART selt-test supported
					 *  0:	SMART error logging
					 */
	    // 88
	    uint16_t dma_ultra; 
	    // 89
	    uint16_t trs_euc;   	/* time required for security erase */
	    // 90
	    uint16_t trs_Euc;		/* time required for enhanced erase */
	    // 91
	    uint16_t cur_apm_values;	/* current APM values */
	    // 92
	    uint16_t mprc;		/* master password revision code */
	    // 93
	    uint16_t hw_config; 	/* hardware config (word 93)
					 * 15:	Shall be ZERO
					 * 14:	Shall be ONE
					 *  0:	Shall be ONE
					 */
	    // 94
	    uint16_t acoustic;
	    // 95
	    uint16_t msrqs;	        /* min stream request size */
	    // 96
	    uint16_t sxfert;		/* stream transfer time */
	    // 97
	    uint16_t sal;		/* stream access latency */
	    // 98
	    uint32_t spg;		/* stream performance granularity */
	    // 100
	    uint64_t lba_capacity_2; /* 48-bit total number of sectors */
	    // 104
	    uint16_t rsvd9[22];

	    // 126
	    uint16_t last_lun;
	    // 127
	    uint16_t feature_set; //  Removable Media Notification

	    // 128
	    uint16_t dlf; /* device lock function
			   * 15:9	reserved
			   *  8	security level 1:max 0:high
			   *  7:6	reserved
			   *  5	enhanced erase
			   *  4	expire
			   *  3	frozen
			   *  2	locked
			   *  1	en/disabled
			   *  0	capability
			   */
	    // 129
	    uint16_t csfo; /* current set features options
			    * 15:4	reserved
			    *  3:	auto reassign
			    *  2:	reverting
			    *  1:	read-look-ahead
			    *  0:	write cache
			    */
	    
	    // 130
	    uint16_t rsvd10[30];
	    
	    // 160
	    uint16_t cfa_power;

	    // 161 
	    uint16_t cfa[15];
	    // 176
	    uint16_t cur_media_ser_num[30];
	    // 206
	    uint16_t rsvd11[49];
	    // 255
	    uint16_t integrity; /* 15:8 Checksum
				 *  7:0 Signature
				 */
	    

	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


#endif // ! __V3VEE__

#endif
