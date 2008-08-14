/*
 * Zheng Cui
 * cuizheng@cs.unm.edu
 * July 2008
 */

#ifndef __DEVICES_RAMDISK_H_
#define __DEVICES_RAMDISK_H_

#include <stddef.h> //for off_t in C99
#include <sys/types.h> //for size_t 
#include <geekos/ktypes.h>
#include <devices/cdrom.h>
#include <palacios/vm_dev.h>

#define INDEX_PULSE_CYCLE 10

#define MAX_ATA_CHANNEL 4
#define RD_LITTLE_ENDIAN


#define INTR_REASON_BIT_ERR           0x01
#define UNABLE_FIND_TAT_CHANNEL_ERR   0x02
#define DRQ_ERR                       0x03
#define READ_BUF_GT_512               0x04


typedef enum _sense {
      SENSE_NONE = 0, SENSE_NOT_READY = 2, SENSE_ILLEGAL_REQUEST = 5,
      SENSE_UNIT_ATTENTION = 6
} sense_t ;

typedef enum _asc {
      ASC_INV_FIELD_IN_CMD_PACKET = 0x24,
      ASC_MEDIUM_NOT_PRESENT = 0x3a,
      ASC_SAVING_PARAMETERS_NOT_SUPPORTED = 0x39,
      ASC_LOGICAL_BLOCK_OOR = 0x21
} asc_t ;


// FLAT MODE
// Open a image. Returns non-negative if successful.
//int open (const char* pathname);

// Open an image with specific flags. Returns non-negative if successful.
int rd_open (const char* pathname, int flags);

// Close the image.
void rd_close ();

// Position ourselves. Return the resulting offset from the
// beginning of the file.
off_t rd_lseek (off_t offset, int whence);

// Read count bytes to the buffer buf. Return the number of
// bytes read (count).
ssize_t rd_read (void* buf, size_t count);

// Write count bytes from buf. Return the number of bytes
// written (count).
ssize_t rd_write (const void* buf, size_t count);


typedef struct  {

      unsigned cylinders               ;
      unsigned heads                   ;
      unsigned sectors                 ;

  //iso file descriptor
      int fd                           ;
} device_image_t;



struct  controller_t {
  struct  {
    rd_bool busy                       ;
    rd_bool drive_ready                ;
    rd_bool write_fault                ;
    rd_bool seek_complete              ;
    rd_bool drq                        ;
    rd_bool corrected_data             ;
    rd_bool index_pulse                ;
    unsigned int index_pulse_count     ;
    rd_bool err                        ;
    } status;
  Bit8u    error_register              ;
  Bit8u    head_no                     ;
  union  {
    Bit8u    sector_count              ;
    struct  {
#ifdef RD_LITTLE_ENDIAN
      unsigned  c_d : 1; 
      unsigned  i_o : 1; 
      unsigned  rel : 1; 
      unsigned  tag : 5; 

#else  /* RD_BIG_ENDIAN */
      unsigned tag : 5;
      unsigned rel : 1;
      unsigned i_o : 1;
      unsigned c_d : 1;
#endif
      
    } interrupt_reason;
  };
  Bit8u    sector_no                   ;
  union  {
    Bit16u   cylinder_no               ;
    Bit16u   byte_count                ;
  };
  Bit8u    buffer[2048];               ; 
  Bit32u   buffer_index                ;
  Bit32u   drq_index                   ;
  Bit8u    current_command             ;
  Bit8u    sectors_per_block           ;
  Bit8u    lba_mode                    ;
  struct  {
    // 0=normal, 1=reset controller
    rd_bool reset                      ;       
    // 0=allow irq, 1=disable irq
    rd_bool disable_irq                ; 
    } control;
  Bit8u    reset_in_progress           ;
  Bit8u    features                    ;
  };

struct  sense_info_t{
  sense_t sense_key                    ;
  struct  {
    Bit8u arr[4]                       ;
  } information;
  struct  {
    Bit8u arr[4]                       ;
  } specific_inf;
  struct  {
    Bit8u arr[3]                       ;
  } key_spec;
  Bit8u fruc                           ;
  Bit8u asc                            ;
  Bit8u ascq                           ;
};

struct  error_recovery_t {
  unsigned char data[8]                ;

  //  error_recovery_t ();
};

uint16 rd_read_16bit(const uint8* buf); //__attribute__(regparm(1))
uint32 rd_read_32bit(const uint8* buf); //__attribute__(regparm(1))

struct  cdrom_t {
  rd_bool ready                                      ;
  rd_bool locked                                     ;


  struct cdrom_interface *cd                         ;

  uint32 capacity                                    ;
  int next_lba                                       ;
  int remaining_blocks                               ;
  struct  currentStruct {
    struct error_recovery_t error_recovery           ;
  } current;
};

struct  atapi_t {
  uint8 command                                      ;
  int drq_bytes                                      ;
  int total_bytes_remaining                          ;
};


typedef enum {
      IDE_NONE, IDE_DISK, IDE_CDROM
} device_type_t ;


  // FIXME:
  // For each ATA channel we should have one controller struct
  // and an array of two drive structs
struct  channel_t {
    struct  drive_t {
      device_image_t  hard_drive                     ;
      device_type_t device_type                      ;
      // 512 byte buffer for ID drive command
      // These words are stored in native word endian format, as
      // they are fetched and returned via a return(), so
      // there's no need to keep them in x86 endian format.
      Bit16u id_drive[256]                           ;

      struct controller_t controller                 ;
      struct cdrom_t cdrom                           ;
      struct sense_info_t sense                      ;
      struct atapi_t atapi                           ;

      Bit8u model_no[41]                             ;
      } drives[2];
    unsigned drive_select                            ;

    Bit16u ioaddr1                                   ;
    Bit16u ioaddr2                                   ;
    Bit8u  irq                                       ;
};

struct ramdisk_t;

struct ramdisk_ctrl_ops {
  Bit32u (*init)(struct ramdisk_t *ramdisk,
		 struct vm_device *dev);
  void   (*close)(struct ramdisk_t *ramdisk);
  void   (*reset)(struct ramdisk_t *ramdisk, unsigned type);

};

struct ramdisk_emu_ops {

  uint_t (*read_port)(ushort_t port,
		      void *src,
		      uint_t length,
		      struct vm_device *dev);

  uint_t (*write_port)(ushort_t port,
		       void *src,
		       uint_t length,
		       struct vm_device *dev);

  uint_t (*read_port_ignore)(ushort_t port,
			     void *src,
			     uint_t length,
			     struct vm_device *dev);

  uint_t (*write_port_ignore)(ushort_t port,
			      void *src,
			      uint_t length,
			      struct vm_device *dev);
};


struct  ramdisk_t {
  
  struct channel_t channels[MAX_ATA_CHANNEL]         ;

  struct ramdisk_ctrl_ops cops;

  struct ramdisk_emu_ops eops;

  void *private_data                                 ;
  //  struct vm_device *dev;
};

struct ramdisk_t * create_ramdisk(void);

#endif
