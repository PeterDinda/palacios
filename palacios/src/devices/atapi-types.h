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


#ifndef __DEVICES_ATAPI_TYPES_H__
#define __DEVICES_ATAPI_TYPES_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>

typedef enum {
    ATAPI_SEN_NONE = 0, 
    ATAPI_SEN_NOT_RDY = 2, 
    ATAPI_SEN_ILL_REQ = 5,
    ATAPI_SEN_UNIT_ATTNT = 6
} atapi_sense_key_t ;

typedef enum  {
    ASC_INV_CMD_FIELD = 0x24,
    ASC_MEDIA_NOT_PRESENT = 0x3a,
    ASC_SAVE_PARAM_NOT_SUPPORTED = 0x39,    
    ASC_LOG_BLK_OOR = 0x21                  /* LOGICAL BLOCK OUT OF RANGE */
} atapi_add_sense_code_t ; 

struct atapi_irq_flags {
    uint8_t c_d    : 1; 
    uint8_t io_dir : 1; 
    uint8_t rel    : 1; 
    uint8_t tag    : 5;
} __attribute__((packed));



struct atapi_sense_data {
    union {
	uint8_t buf[18];
	struct {
	    uint8_t header;
	    uint8_t rsvd1;
	    uint8_t sense_key; // atapi_sense_key_t
	    uint8_t info[4];
	    uint8_t read_len; // num bytes past this point
	    uint8_t spec_info[4];
	    uint8_t asc;   // atapi_add_sense_code_t
	    uint8_t ascq; // ??
	    uint8_t fruc; // ??
	    uint8_t key_spec[3];
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct atapi_error_recovery {
    union {
	uint8_t buf[12];
	struct {
	    uint8_t page_code     : 6;
	    uint8_t rsvd          : 1;
	    uint8_t page_ctrl     : 1;
	    uint8_t page_len;
	    uint8_t dcr           : 1;
	    uint8_t dte           : 1;
	    uint8_t per           : 1;
	    uint8_t rsvd1         : 1;
	    uint8_t rc            : 1;
	    uint8_t tb            : 1;
	    uint8_t arre          : 1;
	    uint8_t awre          : 1;
	    uint8_t rd_retry_cnt;
	    uint8_t correct_spin;
	    uint8_t head_offset;
	    uint8_t data_strobe_offset;
	    uint8_t emcdr         : 2;
	    uint8_t rsvd2         : 6;
	    uint8_t wr_retry_cnt;
	    uint8_t rsvd3;
	    uint16_t recovery_time_limit;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




struct atapi_read10_cmd {
    uint8_t atapi_cmd;
    uint8_t rel_addr       : 1;
    uint8_t rsvd1          : 2;
    uint8_t force_access   : 1; // can't use cache for data
    uint8_t disable_pg_out : 1;
    uint8_t lun            : 3;
    uint32_t lba;
    uint8_t rsvd2;
    uint16_t xfer_len;
    uint8_t ctrl;
} __attribute__((packed));


struct atapi_mode_sense_cmd {
    uint8_t atapi_cmd;  // 0x5a
    uint8_t rsvd1            : 3;
    uint8_t disable_blk_desc : 1;
    uint8_t long_lba_acc     : 1;
    uint8_t lun              : 3;
    uint8_t page_code        : 6;
    uint8_t page_ctrl        : 2;
    uint8_t sub_page_code;
    uint8_t rsvd2[3];
    uint16_t alloc_len;
    uint8_t link             : 1;
    uint8_t flag             : 1;
    uint8_t naca             : 1;
    uint8_t rsvd3            : 3;
    uint8_t vendor_specific  : 2;
} __attribute__((packed));

struct atapi_mode_sense_hdr {
    uint16_t mode_data_len;
    uint8_t media_type_code;
    uint8_t rsvd[2];
    uint16_t blk_desc_len;
} __attribute__((packed));


struct atapi_rd_capacity_cmd {
    uint8_t atapi_cmd;  // 0x25
    uint8_t obsolete         : 1;
    uint8_t rsvd1            : 4;
    uint8_t lun              : 3;
    uint32_t lba;
    uint16_t rsvd2;
    uint8_t pmi;
    uint8_t rsvd3            : 7;
    uint8_t link             : 1;
    uint8_t flag             : 1;
    uint8_t naca             : 1;
    uint8_t rsvd4            : 3;
    uint8_t vendor_spec      : 2;
} __attribute__((packed));


struct atapi_rd_capacity_resp {
    uint32_t lba;
    uint32_t block_len;
} __attribute__((packed));

struct atapi_config_cmd {
    uint8_t atapi_cmd;  // 0x46
    uint8_t rt               : 2;
    uint8_t rsvd1            : 3;
    uint8_t lun              : 3;
    uint16_t start_feature_num;
    uint8_t rsvd2[3];
    uint16_t alloc_len;
    uint8_t link             : 1;
    uint8_t flag             : 1;
    uint8_t naca             : 1;
    uint8_t rsvd3            : 3;
    uint8_t vendor_specific  : 2;
} __attribute__((packed));

struct atapi_config_resp {
    uint32_t data_len;
    uint16_t rsvd;
    uint16_t cur_profile;
} __attribute__((packed));


struct atapi_rd_toc_cmd {
    uint8_t atapi_cmd;  // 0x43
    uint8_t rsvd1            : 1;
    uint8_t msf              : 1;
    uint8_t rsvd2            : 3;
    uint8_t lun              : 3;
    uint8_t format           : 4;
    uint8_t rsvd3            : 4;
    uint8_t rsvd4[3];
    uint8_t track_num;
    uint16_t alloc_len;
    uint8_t link             : 1;
    uint8_t flag             : 1;
    uint8_t naca             : 1;
    uint8_t rsvd5            : 3;
    uint8_t vendor_specific  : 2;
} __attribute__((packed));

struct atapi_rd_toc_resp {
    uint16_t data_len;
    uint8_t first_track_num;
    uint8_t last_track_num;
    
    struct {
	uint8_t rsvd;
	uint8_t ctrl         : 4;
	uint8_t adr          : 4;
	uint8_t track_num;
	uint8_t rsvd2;
	uint32_t start_addr;
    } track_descs[0] __attribute__((packed)) ;

} __attribute__((packed));


struct atapi_mech_status_cmd {
    uint8_t atapi_cmd;   // 0xbd
    uint8_t rsvd1        : 5;
    uint8_t lun          : 3;
    uint8_t rsvd2[6];
    uint16_t alloc_len;
    uint8_t rsvd3;
    uint8_t link             : 1;
    uint8_t flag             : 1;
    uint8_t naca             : 1;
    uint8_t rsvd5            : 3;
    uint8_t vendor_specific  : 2;
} __attribute__((packed));

struct atapi_mech_status_resp {
    uint8_t cur_slot          : 5;
    uint8_t changer_state     : 2;
    uint8_t fault             : 1;
    uint8_t rsvd1             : 4;
    uint8_t door_open         : 1;
    uint8_t cd_dvd_mech_state : 3;
    uint32_t lba;
    uint8_t num_slots         : 6;
    uint8_t rsvd2             : 2;
    uint16_t slot_table_len;
} __attribute__((packed));


struct atapi_inquiry_cmd {
    uint8_t atapi_cmd;  // 0x12
    uint8_t evpd           : 1; 
    uint8_t obsolete       : 1;
    uint8_t rsvd           : 3;
    uint8_t lun            : 3;
    uint8_t pg_op_code;
    uint16_t alloc_len;
    uint8_t link             : 1;
    uint8_t flag             : 1;
    uint8_t naca             : 1;
    uint8_t rsvd5            : 3;
    uint8_t vendor_specific  : 2;
} __attribute__((packed));


struct atapi_inquiry_resp {
#define DEV_TYPE_CDROM 0x05 
    uint8_t dev_type         : 5;
    uint8_t rsvd1            : 3; // not used in ATAPI
    uint8_t rsvd2            : 7;
    uint8_t removable_media  : 1;
    uint8_t version;
    uint8_t resp_data_fmt    : 4;
    uint8_t atapi_trans_ver  : 4;
    uint8_t additional_len;
    uint8_t protect          : 1;
    uint8_t rsvd3            : 2;
    uint8_t spc_3pc          : 1; // no idea
    uint8_t tpgs             : 2;
    uint8_t acc              : 1;
    uint8_t sccs             : 1;
    uint8_t addr16           : 1;
    uint8_t addr32           : 1;
    uint8_t ack_req_q        : 1;
    uint8_t media_changer    : 1;
    uint8_t multi_port       : 1;
    uint8_t vs               : 1;
    uint8_t enc_services     : 1;
    uint8_t basic_queueing   : 1;
    uint8_t rsvd4;
    uint8_t vs2              : 1;
    uint8_t cmd_queue        : 1;
    uint8_t tran_dis         : 1;
    uint8_t linked           : 1;
    uint8_t sync             : 1;
    uint8_t wbus_16          : 1;
    uint8_t wbus_32          : 1;
    uint8_t rel_addr         : 1;
    uint8_t rsvd5;
    uint8_t t10_vendor_id[8];
    uint8_t product_id[16];
    uint8_t product_rev[4];
    /* We'll ignore these for now...
      uint8_t vendor_specific[20];
      uint8_t ius              : 1;
      uint8_t qas              : 1;
      uint8_t clocking         : 2;
      uint8_t rsvd6            : 4;
      uint8_t rsvd7;
      uint16_t version_desc[8];
    */
} __attribute__((packed));





struct atapi_cdrom_caps {
    uint8_t page_code     : 6;
    uint8_t rsvd          : 1;
    uint8_t page_ctrl     : 1;
    uint8_t page_len;
    uint8_t cdr_rd        : 1;
    uint8_t cdrw_rd       : 1;
    uint8_t mthd_2        : 1;
    uint8_t dvdrom_rd     : 1;
    uint8_t dvdr_rd       : 1;
    uint8_t dvdram_rd     : 1;
    uint8_t rsvd1         : 2;
    uint8_t cdr_wr        : 1;
    uint8_t cdrw_wr       : 1;
    uint8_t tst_wr        : 1;
    uint8_t rsvd2         : 1;
    uint8_t dvdr_wr       : 1;
    uint8_t dvdram_wr     : 1;
    uint8_t rsvd3         : 2;
    uint8_t audio_play    : 1;
    uint8_t composite     : 1;
    uint8_t digi_port1    : 1;
    uint8_t digi_port2    : 1;
    uint8_t mode2_form1   : 1;
    uint8_t mode2_form2   : 1;
    uint8_t multisession  : 1;
    uint8_t BUF           : 1;
    uint8_t cd_da         : 1;
    uint8_t cdda_str_acc  : 1;
    uint8_t rw_supported  : 1;
    uint8_t rw_dc         : 1;
    uint8_t c2_ptrs_supp  : 1;
    uint8_t isrc          : 1;
    uint8_t upc           : 1;
    uint8_t rd_bar_cd_cap : 1;
    uint8_t lock          : 1;
    uint8_t lock_state    : 1;
    uint8_t prevent_jmpr  : 1;
    uint8_t eject         : 1;
    uint8_t rsvd4         : 1;
    uint8_t lmt           : 3;
    uint8_t sep_vol       : 1;
    uint8_t sep_chnl_mute : 1;
    uint8_t sdp           : 1;
    uint8_t sss           : 1;
    uint8_t side_chg_cap  : 1;
    uint8_t rw_in_lead_rd : 1;
    uint8_t rsvd5         : 2;
    uint16_t obsolete1;
    uint16_t num_vols_supp;
    uint16_t lun_buf_size; // in KBytes
    uint16_t obsolete2;
    uint8_t obsolete3;
    uint8_t rsvd6         : 1;
    uint8_t bckf          : 1;
    uint8_t rck           : 1;
    uint8_t lsbf          : 1;
    uint8_t len           : 2;
    uint8_t rsvd7         : 2;
    uint16_t obsolete4[2];
    uint16_t cp_mgmnt_rev_supp;
    uint8_t rsvd8;
    uint8_t rot_ctrl_sel  : 2;
    uint8_t rsvd9         : 6;
    uint16_t cur_wr_spd; // KBytes/sec
    uint16_t num_lun_wr_spd_dsc_tbls;
    // lun write speed descriptor tables
} __attribute__((packed));

#endif

#endif
