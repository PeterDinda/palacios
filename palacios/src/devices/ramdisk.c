/*
 * Zheng Cui
 * cuizheng@cs.unm.edu
 * July 2008
 */

#include <devices/ramdisk.h>
#include <palacios/vmm.h>
#include <string.h>

#ifdef DEBUG_RAMDISK
//#define Ramdisk_Print(_f, _a...) PrintTrace("\nramdisk.c(%d) " _f, __LINE__, ## _a)
#define Ramdisk_Print(_f, _a...) PrintTrace("\nramdisk.c " _f, ## _a)
#else 
#define Ramdisk_Print(_f, _a...)
#endif

#define RD_PANIC(_f, _a...)     \
  do {                       \
    PrintDebug("ramdisk.c(%d) " _f, __LINE__, ## _a);	\
      while(1);              \
    }                        \
   while(0)                                                            
    
#define RD_ERROR(_f, _a...) PrintTrace("\nramdisk.c(%d) " _f, __LINE__, ## _a)



/*
 * Data type definitions
 *
 */
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

  int (*read_port)(ushort_t port,
		   void *dst,
		   uint_t length,
		   struct vm_device *dev);

  int (*write_port)(ushort_t port,
		    void *src,
		    uint_t length,
		    struct vm_device *dev);

  int (*read_port_ignore)(ushort_t port,
			  void *dst,
			  uint_t length,
			  struct vm_device *dev);

  int (*write_port_ignore)(ushort_t port,
			   void *src,
			   uint_t length,
			   struct vm_device *dev);
};


struct  ramdisk_t {
  
  struct channel_t channels[MAX_ATA_CHANNEL];

  struct ramdisk_ctrl_ops cops;

  struct ramdisk_emu_ops eops;

  void *private_data;
  //  struct vm_device *dev;
};


/*
 * Debug facilities
 */

#define ATA_DETECT                     0xf0 //0X3E8
#define ATA_RESET                      0xf1 //0X3E9
#define ATA_CMD_DATA_IN                0xf2 //0X3EA
#define ATA_CMD_DATA_OUT               0xf3 //0X3EB
#define ATA_CMD_PACKET                 0xf4 //0X3EC
#define ATAPI_GET_SENSE                0xf5 //0X3ED
#define ATAPI_IS_READY                 0xf6 //0X3EE
#define ATAPI_IS_CDROM                 0xf7 //0X3EF

#define CDEMU_INIT                     0xf8 //0X2E8
#define CDEMU_ISACTIVE                 0xf9 //0X2E9
#define CDEMU_EMULATED_DRIVE           0xfa //0X2EA
#define CDROM_BOOT                     0xfb //0X2EB


#define HARD_DRIVE_POST                0xfc //0X2EC


#define ATA_DEVICE_NO                  0xfd //0X2ED
#define ATA_DEVICE_TYPE                0xfe //0X2ED

#define INT13_HARDDISK                 0xff //0x2ef
#define INT13_CDROM                    0xe0 //0x2f8
#define INT13_CDEMU                    0xe1 //0x2f9
#define INT13_ELTORITO                 0xe2 //0x2fa
#define INT13_DISKETTE_FUNCTION        0xe3 //0x2fb

// some packet handling macros
#define EXTRACT_FIELD(arr,byte,start,num_bits) (((arr)[(byte)] >> (start)) & ((1 << (num_bits)) - 1))
#define get_packet_field(c,b,s,n) (EXTRACT_FIELD((SELECTED_CONTROLLER((c)).buffer),(b),(s),(n)))
#define get_packet_byte(c,b) (SELECTED_CONTROLLER((c)).buffer[(b)])
#define get_packet_word(c,b) (((uint16)SELECTED_CONTROLLER((c)).buffer[(b)] << 8) | SELECTED_CONTROLLER((c)).buffer[(b)+1])


#define CONTROLLER(c,a) (channels[(c)].drives[(a)]).controller
#define DRIVE(c,a) (channels[(c)].drives[(a)])
#define SELECTED_CONTROLLER(c) (CONTROLLER((c), channels[(c)].drive_select))
#define SELECTED_DRIVE(c) (DRIVE((c), channels[(c)].drive_select))


#define DRIVE_IS_PRESENT(c,a) (channels[(c)].drives[(a)].device_type != IDE_NONE)
#define DRIVE_IS_HD(c,a) (channels[(c)].drives[(a)].device_type == IDE_DISK)
#define DRIVE_IS_CD(c,a) (channels[(c)].drives[(a)].device_type == IDE_CDROM)
#define SELECTED_MODEL(c) (channels[(c)].drives[channels[(c)].drive_select].model_no)

#define MASTER_SELECTED(c) (!channels[(c)].drive_select)
#define SLAVE_SELECTED(c)  (channels[(c)].drive_select)

#define SELECTED_IS_PRESENT(c) (DRIVE_IS_PRESENT((c),SLAVE_SELECTED((c))))
#define SELECTED_IS_HD(c) (DRIVE_IS_HD((c),SLAVE_SELECTED((c))))
#define SELECTED_IS_CD(c) (DRIVE_IS_CD((c),SLAVE_SELECTED((c))))

#define ANY_IS_PRESENT(c) (DRIVE_IS_PRESENT((c),0) || DRIVE_IS_PRESENT((c),1))
#define SELECTED_TYPE_STRING(channel) ((SELECTED_IS_CD(channel)) ? "CD-ROM" : "NONE")

#define WRITE_FEATURES(c,a) do { uint8 _a = a; CONTROLLER((c),0).features = _a; CONTROLLER((c),1).features = _a; } while(0)
#define WRITE_SECTOR_COUNT(c,a) do { uint8 _a = a; CONTROLLER((c),0).sector_count = _a; CONTROLLER((c),1).sector_count = _a; } while(0)
#define WRITE_SECTOR_NUMBER(c,a) do { uint8 _a = a; CONTROLLER((c),0).sector_no = _a; CONTROLLER((c),1).sector_no = _a; } while(0)
#define WRITE_CYLINDER_LOW(c,a) do { uint8 _a = a; CONTROLLER((c),0).cylinder_no = (CONTROLLER((c),0).cylinder_no & 0xff00) | _a; CONTROLLER((c),1).cylinder_no = (CONTROLLER((c),1).cylinder_no & 0xff00) | _a; } while(0)
#define WRITE_CYLINDER_HIGH(c,a) do { uint16 _a = a; CONTROLLER((c),0).cylinder_no = (_a << 8) | (CONTROLLER((c),0).cylinder_no & 0xff); CONTROLLER((c),1).cylinder_no = (_a << 8) | (CONTROLLER((c),1).cylinder_no & 0xff); } while(0)
#define WRITE_HEAD_NO(c,a) do { uint8 _a = a; CONTROLLER((c),0).head_no = _a; CONTROLLER((c),1).head_no = _a; } while(0)
#define WRITE_LBA_MODE(c,a) do { uint8 _a = a; CONTROLLER((c),0).lba_mode = _a; CONTROLLER((c),1).lba_mode = _a; } while(0)



#define GOTO_RETURN_VALUE  if(io_len==4){\
                             goto return_value32;\
                             }\
                           else if(io_len==2){\
                             value16=(Bit16u)value32;\
                             goto return_value16;\
                             }\
                           else{\
                             value8=(Bit8u)value32;\
                             goto return_value8;\
                             }

#define UNUSED(x) ((void)x)

#define PACKET_SIZE 12

static struct ramdisk_t *ramdisk_state;






////////////////////////////////////////////////////////////////////////////

/*
 * Static routines
 */

static
int ramdisk_read_port(ushort_t port,
			 void *dst,
			 uint_t length,
			 struct vm_device *dev);

static
int ramdisk_write_port(ushort_t port,
			  void *src,
			  uint_t length,
			  struct vm_device *dev);

static
int ramdisk_read_port_ignore(ushort_t port,
				void *dst,
				uint_t length,
				struct vm_device *dev);

static
int ramdisk_write_port_ignore(ushort_t port,
				 void *src,
				 uint_t length,
				 struct vm_device *dev);


static
Bit32u rd_read_handler(struct channel_t *channels, Bit32u address, unsigned io_len);

static
void rd_write_handler(struct channel_t *channels, Bit32u address, 
		      Bit32u value, unsigned io_len);



/*
 * ATAPI routines
 */

static 
void rd_identify_ATAPI_drive(struct channel_t *channels, Bit8u channel);


static 
void rd_init_send_atapi_command(struct channel_t *channels, Bit8u channel, Bit8u command, int req_length, int alloc_length, bool lazy /*= false*/);

static 
void rd_ready_to_send_atapi(struct channel_t *channels, Bit8u channel);


static 
void rd_atapi_cmd_error(struct channel_t *channels, Bit8u channel, sense_t sense_key, asc_t asc);

static 
void rd_init_mode_sense_single(struct channel_t *channels, Bit8u channel, const void* src, int size);

static 
void rd_atapi_cmd_nop(struct channel_t *channels, Bit8u channel);

static
void rd_command_aborted(struct channel_t *channels, Bit8u channel, unsigned value);


/*
 * Interrupt handling
 */

static 
void rd_raise_interrupt(struct channel_t *channels, Bit8u channel);

static 
void rd_lower_irq(struct vm_device *dev, Bit32u irq);



/*
 * Helper routines
 */

uint16 rd_read_16bit(const uint8* buf) 
{
  return (buf[0] << 8) | buf[1];
}



uint32 rd_read_32bit(const uint8* buf) 
{
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

////////////////////////////////////////////////////////////////////


void rd_print_state(struct ramdisk_t *ramdisk, 
			 struct vm_device *dev)
//Bit32u   rd_init_harddrive(struct channel_t *channels)
{

  uchar_t channel; 
  uchar_t device;
  struct channel_t *channels = (struct channel_t *)(&(ramdisk->channels));

  for (channel = 0; channel < MAX_ATA_CHANNEL; channel++)
    memset((char *)(channels + channel), 0, sizeof(struct channel_t));

  Ramdisk_Print("sizeof(*channels) = %d\n", sizeof((*channels)));
  Ramdisk_Print("sizeof(channles->drives[0].controller) = %d\n", sizeof((channels->drives[0].controller)));
  Ramdisk_Print("sizeof(channles->drives[0].cdrom) = %d\n", sizeof((channels->drives[0].cdrom)));
  Ramdisk_Print("sizeof(channles->drives[0].sense) = %d\n", sizeof((channels->drives[0].sense)));
  Ramdisk_Print("sizeof(channles->drives[0].atapi) = %d\n", sizeof((channels->drives[0].atapi)));


  Ramdisk_Print("sizeof(channles->drives[0].controller.status) = %d\n", sizeof((channels->drives[0].controller.status)));
  Ramdisk_Print("sizeof(channles->drives[0].controller.sector_count) = %d\n", sizeof((channels->drives[0].controller.sector_count)));
  Ramdisk_Print("sizeof(channles->drives[0].controller.interrupt_reason) = %d\n", sizeof((channels->drives[0].controller.interrupt_reason)));

  Ramdisk_Print("sizeof(channles->drives[0].controller.cylinder_no) = %d\n", sizeof((channels->drives[0].controller.cylinder_no)));
  Ramdisk_Print("sizeof(channles->drives[0].controller.byte_count) = %d\n", sizeof((channels->drives[0].controller.byte_count)));


  Ramdisk_Print("sizeof(channles->drives[0].controller.control) = %d\n", sizeof((channels->drives[0].controller.control)));


  for (channel = 0; channel < MAX_ATA_CHANNEL; channel++){
  
    for (device = 0; device < 2; device++){
                  
      // Initialize controller state, even if device is not present
      Ramdisk_Print("channels[%d].drives[%d].controller.status.busy = %d\n",channel, device, channels[channel].drives[device].controller.status.busy);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.drive_ready = %d\n", channel, device, channels[channel].drives[device].controller.status.drive_ready);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.write_fault = %d\n", channel, device, channels[channel].drives[device].controller.status.write_fault);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.seek_complete = %d\n", channel, device, channels[channel].drives[device].controller.status.seek_complete);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.drq = %d\n", channel, device, channels[channel].drives[device].controller.status.drq);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.corrected_data = %d\n", channel, device, channels[channel].drives[device].controller.status.corrected_data);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.index_pulse = %d\n", channel, device, channels[channel].drives[device].controller.status.index_pulse);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.index_pulse_count = %d\n", channel, device, channels[channel].drives[device].controller.status.index_pulse_count);
      Ramdisk_Print("channels[%d].drives[%d].controller.status.err = %d\n", channel, device, channels[channel].drives[device].controller.status.err);


      Ramdisk_Print("channels[%d].drives[%d].controller.error_register = %d\n", channel, device, channels[channel].drives[device].controller.error_register);
      Ramdisk_Print("channels[%d].drives[%d].controller.head_no = %d\n", channel, device, channels[channel].drives[device].controller.head_no);
      Ramdisk_Print("channels[%d].drives[%d].controller.sector_count = %d\n", channel, device, channels[channel].drives[device].controller.sector_count);
      Ramdisk_Print("channels[%d].drives[%d].controller.sector_no = %d\n", channel, device, channels[channel].drives[device].controller.sector_no);
      Ramdisk_Print("channels[%d].drives[%d].controller.cylinder_no = %d\n", channel, device, channels[channel].drives[device].controller.cylinder_no);
      Ramdisk_Print("channels[%d].drives[%d].controller.current_command = %02x\n", channel, device, channels[channel].drives[device].controller.current_command);
      Ramdisk_Print("channels[%d].drives[%d].controller.buffer_index = %d\n", channel, device, channels[channel].drives[device].controller.buffer_index);


      Ramdisk_Print("channels[%d].drives[%d].controller.control.reset = %d\n", channel, device, channels[channel].drives[device].controller.control.reset);
      Ramdisk_Print("channels[%d].drives[%d].controller.control.disable_irq = %d\n", channel, device, channels[channel].drives[device].controller.control.disable_irq);


      Ramdisk_Print("channels[%d].drives[%d].controller.reset_in_progress = %d\n", channel, device, channels[channel].drives[device].controller.reset_in_progress);
      Ramdisk_Print("channels[%d].drives[%d].controller.sectors_per_block = %02x\n", channel, device, channels[channel].drives[device].controller.sectors_per_block); 
      Ramdisk_Print("channels[%d].drives[%d].controller.lba_mode = %d\n", channel, device, channels[channel].drives[device].controller.lba_mode); 
      Ramdisk_Print("channels[%d].drives[%d].controller.features = %d\n", channel, device, channels[channel].drives[device].controller.features); 


      Ramdisk_Print("channels[%d].drives[%d].model_no = %s\n", channel, device, channels[channel].drives[device].model_no); 
      Ramdisk_Print("channels[%d].drives[%d].device_type = %d\n", channel, device, channels[channel].drives[device].device_type); 
      Ramdisk_Print("channels[%d].drives[%d].cdrom.locked = %d\n", channel, device, channels[channel].drives[device].cdrom.locked); 
      Ramdisk_Print("channels[%d].drives[%d].sense.sense_key = %d\n", channel, device, channels[channel].drives[device].sense.sense_key); 
      Ramdisk_Print("channels[%d].drives[%d].sense.asc = %d\n", channel, device, channels[channel].drives[device].sense.asc); 
      Ramdisk_Print("channels[%d].drives[%d].sense.ascq = %d\n", channel, device, channels[channel].drives[device].sense.ascq); 



      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.c_d = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.c_d);

      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.i_o = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.i_o);

      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.rel = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.rel);

      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.tag = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.tag);

      Ramdisk_Print("channels[%d].drives[%d].cdrom.ready = %d\n", channel, device, channels[channel].drives[device].cdrom.ready);
      
    }//for device
  }//for channel
  
  return;
}


Bit32u rd_init_harddrive(struct ramdisk_t *ramdisk, 
			 struct vm_device *dev)
{

  
  uchar_t channel; 
  uchar_t device;
  struct channel_t *channels = (struct channel_t *)(&(ramdisk->channels));

  Ramdisk_Print("[rd_init_harddrive]\n");
  for (channel = 0; channel < MAX_ATA_CHANNEL; channel++)
    memset((char *)(channels + channel), 0, sizeof(struct channel_t));


  for (channel = 0; channel < MAX_ATA_CHANNEL; channel++){
  
    channels[channel].ioaddr1 = 0x0;
    channels[channel].ioaddr2 = 0x0;
    channels[channel].irq = 0;

    for (device = 0; device < 2; device++){
      

      CONTROLLER(channel,device).status.busy           = 0;
      CONTROLLER(channel,device).status.drive_ready    = 1;
      CONTROLLER(channel,device).status.write_fault    = 0;
      CONTROLLER(channel,device).status.seek_complete  = 1;
      CONTROLLER(channel,device).status.drq            = 0;
      CONTROLLER(channel,device).status.corrected_data = 0;
      CONTROLLER(channel,device).status.index_pulse    = 0;
      CONTROLLER(channel,device).status.index_pulse_count = 0;
      CONTROLLER(channel,device).status.err            = 0;

      CONTROLLER(channel,device).error_register = 0x01; // diagnostic code: no error
      CONTROLLER(channel,device).head_no        = 0;
      CONTROLLER(channel,device).sector_count   = 1;
      CONTROLLER(channel,device).sector_no      = 1;
      CONTROLLER(channel,device).cylinder_no    = 0;
      CONTROLLER(channel,device).current_command = 0x00;
      CONTROLLER(channel,device).buffer_index = 0;

      CONTROLLER(channel,device).control.reset       = 0;
      CONTROLLER(channel,device).control.disable_irq = 0;
      CONTROLLER(channel,device).reset_in_progress   = 0;

      CONTROLLER(channel,device).sectors_per_block   = 0x80;
      CONTROLLER(channel,device).lba_mode            = 0;
      
      CONTROLLER(channel,device).features            = 0;
	
	  // If not present
      channels[channel].drives[device].device_type           = IDE_NONE;

      // Make model string
      strncpy((char*)channels[channel].drives[device].model_no, 
	      "", 40);
      while(strlen((char *)channels[channel].drives[device].model_no) < 40) {
        strcat ((char*)channels[channel].drives[device].model_no, " ");
      }

//      Ramdisk_Print("channels[%d].drives[%d].controller.current_command = %02x\n", channel, device, channels[channel].drives[device].controller.current_command);
//      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.c_d = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.c_d);

//      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.i_o = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.i_o);

//      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.rel = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.rel);

//      Ramdisk_Print("channels[%d].drives[%d].controller.interrupt_reason.tag = %02x\n", channel, device, channels[channel].drives[device].controller.interrupt_reason.tag);
      
//      Ramdisk_Print("channels[%d].drives[%d].controller.control.disable_irq = %d\n", channel, device, channels[channel].drives[device].controller.control.disable_irq);
      
      
      
      if (channel == 1) {

	channels[channel].ioaddr1 = 0x170;
	channels[channel].ioaddr2 = 0x370;
	channels[channel].irq =  15;
	channels[channel].drive_select = 0;
       
	if (device == 0) {
	  // Make model string
	  strncpy((char*)channels[channel].drives[device].model_no, 
		  "Zheng's Ramdisk", 40);
	  while (strlen((char *)channels[channel].drives[device].model_no) < 40) {
	    strcat ((char*)channels[channel].drives[device].model_no, " ");
	  }

	  Ramdisk_Print("CDROM on target %d/%d\n", channel, device);
	  
	  channels[channel].drives[device].device_type = IDE_CDROM;
	  channels[channel].drives[device].cdrom.locked = 0;
	  channels[channel].drives[device].sense.sense_key = SENSE_NONE;
	  channels[channel].drives[device].sense.asc = 0;
	  channels[channel].drives[device].sense.ascq = 0;
	  

	  //Check bit fields
	  channels[channel].drives[device].controller.sector_count = 0;
	  channels[channel].drives[device].controller.interrupt_reason.c_d = 1;
	  if (channels[channel].drives[device].controller.sector_count != 0x01) {
	    Ramdisk_Print("interrupt reason bit field error\n");
	    return INTR_REASON_BIT_ERR;
	  }

	  channels[channel].drives[device].controller.sector_count = 0;
	  channels[channel].drives[device].controller.interrupt_reason.i_o = 1;
	  if (channels[channel].drives[device].controller.sector_count != 0x02) {
	    Ramdisk_Print("interrupt reason bit field error\n");
	    return INTR_REASON_BIT_ERR;
	  }
	  
	  channels[channel].drives[device].controller.sector_count = 0;
	  channels[channel].drives[device].controller.interrupt_reason.rel = 1;
	  if (channels[channel].drives[device].controller.sector_count != 0x04) {
	    Ramdisk_Print("interrupt reason bit field error\n");
	    return INTR_REASON_BIT_ERR;
	  }
	
	  channels[channel].drives[device].controller.sector_count = 0;
	  channels[channel].drives[device].controller.interrupt_reason.tag = 3;
	  if (channels[channel].drives[device].controller.sector_count != 0x18) {
	    Ramdisk_Print("interrupt reason bit field error\n");
	    return INTR_REASON_BIT_ERR;
	  }
	  
	  
	  channels[channel].drives[device].controller.sector_count = 0;

	  // allocate low level driver
	  channels[channel].drives[device].cdrom.cd = (struct cdrom_interface*)V3_Malloc(sizeof(struct cdrom_interface));
	  Ramdisk_Print("cd = %x\n", channels[channel].drives[device].cdrom.cd);
	  V3_ASSERT(channels[channel].drives[device].cdrom.cd != NULL);
	  
	  struct cdrom_interface *cdif = channels[channel].drives[device].cdrom.cd;
	  memset(cdif, 0, sizeof(struct cdrom_interface));
	  init_cdrom(cdif);
	  cdif->ops.init(cdif);

	  Ramdisk_Print("\t\tCD on ata%d-%d: '%s'\n",channel, device, "");
	  
	  if((channels[channel].drives[device].cdrom.cd->ops).insert_cdrom(cdif, NULL)) {
	    Ramdisk_Print("\t\tMedia present in CD-ROM drive\n");
	    channels[channel].drives[device].cdrom.ready = 1;
	    channels[channel].drives[device].cdrom.capacity = channels[channel].drives[device].cdrom.cd->ops.capacity(cdif);
	  } else {		    
	    Ramdisk_Print("\t\tCould not locate CD-ROM, continuing with media not present\n");
	    channels[channel].drives[device].cdrom.ready = 0;
	  }
	  
	}//if device = 0
      }//if channel = 0
    }//for device
  }//for channel

  ramdisk->private_data = dev;
  ramdisk_state = ramdisk;
  Ramdisk_Print("ramdisk_state = %x\n", ramdisk_state);
  Ramdisk_Print("ramdisk = %x\n", ramdisk);
  //  rd_print_state(ramdisk, dev);
  return 0;
}


void   rd_reset_harddrive(struct ramdisk_t *ramdisk, unsigned type)
{
  return;
}


void   rd_close_harddrive(struct ramdisk_t *ramdisk)
{
  return;
}


////////////////////////////////////////////////////////////////////


static
Bit32u rd_read_handler(struct channel_t *channels, Bit32u address, unsigned io_len)
{
  Bit8u value8;
  Bit16u value16;
  Bit32u value32;

  unsigned drive_select;
  unsigned index;	
  unsigned increment = 0;

  Bit8u  channel = MAX_ATA_CHANNEL;
  Bit32u port = 0xff; // undefined

  //Ramdisk_Print("[rd_read_handler]\n");

  for (channel=0; channel<MAX_ATA_CHANNEL; channel++) {
    if ((address & 0xfff8) == channels[channel].ioaddr1) {
      port = address - channels[channel].ioaddr1;
      break;
      }
    else if ((address & 0xfff8) == channels[channel].ioaddr2) {
      port = address - channels[channel].ioaddr2 + 0x10;
      break;
      }
    }

  if (channel == MAX_ATA_CHANNEL) {
    if ((address < 0x03f6) || (address > 0x03f7)) {
      RD_PANIC("Error: read: unable to find ATA channel, ioport=0x%04x\n", address);
      return 0;
    } else {
      channel = 0;
      port = address - 0x03e0;
    }
  }

  drive_select = channels[channel].drive_select;
  if (port != 0x00){
    Ramdisk_Print("[R_handler] IO read addr at %x, on drive %d/%d, curcmd = %02x\n", address, channel, drive_select, SELECTED_CONTROLLER(channel).current_command);
  }else{
    
  }


  struct cdrom_interface *cdif = channels[channel].drives[drive_select].cdrom.cd;
  switch (port) {

  case 0x00: // hard disk data (16bit) 0x1f0

    switch (SELECTED_CONTROLLER(channel).current_command) {
    case 0xec:    // IDENTIFY DEVICE
    case 0xa1:

      index = 0;
      SELECTED_CONTROLLER(channel).status.busy = 0;
      SELECTED_CONTROLLER(channel).status.drive_ready = 1;
      SELECTED_CONTROLLER(channel).status.write_fault = 0;
      SELECTED_CONTROLLER(channel).status.seek_complete = 1;
      SELECTED_CONTROLLER(channel).status.corrected_data = 0;
      SELECTED_CONTROLLER(channel).status.err = 0;
	
      index = SELECTED_CONTROLLER(channel).buffer_index;
      value32 = SELECTED_CONTROLLER(channel).buffer[index];
      index++;

      if (io_len >= 2) {
	value32 |= (SELECTED_CONTROLLER(channel).buffer[index] << 8);
	index++;
      }
      if (io_len == 4) {
	value32 |= (SELECTED_CONTROLLER(channel).buffer[index] << 16);
	value32 |= (SELECTED_CONTROLLER(channel).buffer[index+1] << 24);
	index += 2;
      }
      SELECTED_CONTROLLER(channel).buffer_index = index;
      
      if (SELECTED_CONTROLLER(channel).buffer_index >= 512) {
	
	SELECTED_CONTROLLER(channel).status.drq = 0;
      }
      GOTO_RETURN_VALUE;

    case 0xa0: //send packet cmd 

	index = SELECTED_CONTROLLER(channel).buffer_index;
	increment = 0;
	Ramdisk_Print("\t\tatapi.command(%02x), index(%d), cdrom.remaining_blocks(%d)\n", SELECTED_DRIVE(channel).atapi.command, index, SELECTED_DRIVE(channel).cdrom.remaining_blocks);
	// Load block if necessary
	if (index >= 2048) {
	  if (index > 2048)
	    RD_PANIC("\t\tindex > 2048 : 0x%x\n",index);
	  switch (SELECTED_DRIVE(channel).atapi.command) {
	  case 0x28: // read (10)
	  case 0xa8: // read (12)

	    if (!SELECTED_DRIVE(channel).cdrom.ready) {
	      RD_PANIC("\t\tRead with CDROM not ready\n");
	    } 
	    SELECTED_DRIVE(channel).cdrom.cd->ops.read_block(cdif, SELECTED_CONTROLLER(channel).buffer,
							    SELECTED_DRIVE(channel).cdrom.next_lba);
	    SELECTED_DRIVE(channel).cdrom.next_lba++;
	    SELECTED_DRIVE(channel).cdrom.remaining_blocks--;
	    
	    
	    if (!SELECTED_DRIVE(channel).cdrom.remaining_blocks)
	      Ramdisk_Print("\t\tLast READ block loaded {CDROM}\n");
	    else
	      Ramdisk_Print("\t\tREAD block loaded (%d remaining) {CDROM}\n",
			    SELECTED_DRIVE(channel).cdrom.remaining_blocks);
	    
	    // one block transfered, start at beginning
	    index = 0;
	    break;
	    
	  default: // no need to load a new block
	    break;
	  }
	}

	value32 = SELECTED_CONTROLLER(channel).buffer[index+increment];
	increment++;
	if (io_len >= 2) {
	  value32 |= (SELECTED_CONTROLLER(channel).buffer[index+increment] << 8);
	  increment++;
	}
	if (io_len == 4) {
	  value32 |= (SELECTED_CONTROLLER(channel).buffer[index+increment] << 16);
	  value32 |= (SELECTED_CONTROLLER(channel).buffer[index+increment+1] << 24);
	  increment += 2;
	}
	SELECTED_CONTROLLER(channel).buffer_index = index + increment;
	SELECTED_CONTROLLER(channel).drq_index += increment;
	
	if (SELECTED_CONTROLLER(channel).drq_index >= (unsigned)SELECTED_DRIVE(channel).atapi.drq_bytes) {
	  SELECTED_CONTROLLER(channel).status.drq = 0;
	  SELECTED_CONTROLLER(channel).drq_index = 0;
	  
	  SELECTED_DRIVE(channel).atapi.total_bytes_remaining -= SELECTED_DRIVE(channel).atapi.drq_bytes;
	  
	  if (SELECTED_DRIVE(channel).atapi.total_bytes_remaining > 0) {
	    // one or more blocks remaining (works only for single block commands)

	    Ramdisk_Print("\t\tPACKET drq bytes read\n");
	    SELECTED_CONTROLLER(channel).interrupt_reason.i_o = 1;
	    SELECTED_CONTROLLER(channel).status.busy = 0;
	    SELECTED_CONTROLLER(channel).status.drq = 1;
	    SELECTED_CONTROLLER(channel).interrupt_reason.c_d = 0;
	    
	    // set new byte count if last block
	    if (SELECTED_DRIVE(channel).atapi.total_bytes_remaining < SELECTED_CONTROLLER(channel).byte_count) {
	      SELECTED_CONTROLLER(channel).byte_count = SELECTED_DRIVE(channel).atapi.total_bytes_remaining;
	    }
	    SELECTED_DRIVE(channel).atapi.drq_bytes = SELECTED_CONTROLLER(channel).byte_count;
	    
	    rd_raise_interrupt(channels, channel);
	  } else {
	    // all bytes read
	    Ramdisk_Print("\t\tPACKET all bytes read\n");
	    SELECTED_CONTROLLER(channel).interrupt_reason.i_o = 1;
	    SELECTED_CONTROLLER(channel).interrupt_reason.c_d = 1;
	    SELECTED_CONTROLLER(channel).status.drive_ready = 1;
	    SELECTED_CONTROLLER(channel).interrupt_reason.rel = 0;
	    SELECTED_CONTROLLER(channel).status.busy = 0;
	    SELECTED_CONTROLLER(channel).status.drq = 0;
	    SELECTED_CONTROLLER(channel).status.err = 0;
	    
	    rd_raise_interrupt(channels, channel);
	  }
	}
	GOTO_RETURN_VALUE;
	break;
      
    default:
      Ramdisk_Print("\t\tread need support more command: %02x\n", SELECTED_CONTROLLER(channel).current_command);
      break;
    }
    ///////////////////////////////////////////

  case 0x01: // hard disk error register 0x1f1

    SELECTED_CONTROLLER(channel).status.err = 0;
    value8 = (!SELECTED_IS_PRESENT(channel)) ? 0 : SELECTED_CONTROLLER(channel).error_register;
    goto return_value8;
    break;
  case 0x02: // hard disk sector count / interrupt reason 0x1f2
    value8 = (!SELECTED_IS_PRESENT(channel)) ? 0 : SELECTED_CONTROLLER(channel).sector_count;
    goto return_value8;
    break;
  case 0x03: // sector number 0x1f3
    value8 = (!SELECTED_IS_PRESENT(channel)) ? 0 : SELECTED_CONTROLLER(channel).sector_no;
    goto return_value8;
  case 0x04: // cylinder low 0x1f4  
    // -- WARNING : On real hardware the controller registers are shared between drives. 
    // So we must respond even if the select device is not present. Some OS uses this fact 
    // to detect the disks.... minix2 for example
    value8 = (!ANY_IS_PRESENT(channel)) ? 0 : (SELECTED_CONTROLLER(channel).cylinder_no & 0x00ff);
    goto return_value8;
  case 0x05: // cylinder high 0x1f5
    // -- WARNING : On real hardware the controller registers are shared between drives. 
    // So we must respond even if the select device is not present. Some OS uses this fact 
    // to detect the disks.... minix2 for example
    value8 = (!ANY_IS_PRESENT(channel)) ? 0 : SELECTED_CONTROLLER(channel).cylinder_no >> 8;
    goto return_value8;
    
  case 0x06: // hard disk drive and head register 0x1f6
    // b7 Extended data field for ECC
    // b6/b5: Used to be sector size.  00=256,01=512,10=1024,11=128
    //   Since 512 was always used, bit 6 was taken to mean LBA mode:
    //     b6 1=LBA mode, 0=CHS mode
    //     b5 1
    // b4: DRV
    // b3..0 HD3..HD0
    value8 = (1 << 7) |
      ((SELECTED_CONTROLLER(channel).lba_mode>0) << 6) |
      (1 << 5) | // 01b = 512 sector size
      (channels[channel].drive_select << 4) |
      (SELECTED_CONTROLLER(channel).head_no << 0);
    goto return_value8;
    break;
    //CONTROLLER(channel,0).lba_mode
    
  case 0x07: // Hard Disk Status 0x1f7
  case 0x16: // Hard Disk Alternate Status 0x3f6
    if (!ANY_IS_PRESENT(channel)) {
      // (mch) Just return zero for these registers
      value8 = 0;
    } else {
      value8 = (
		(SELECTED_CONTROLLER(channel).status.busy << 7) |
		(SELECTED_CONTROLLER(channel).status.drive_ready << 6) |
		(SELECTED_CONTROLLER(channel).status.write_fault << 5) |
		(SELECTED_CONTROLLER(channel).status.seek_complete << 4) |
		(SELECTED_CONTROLLER(channel).status.drq << 3) |
		(SELECTED_CONTROLLER(channel).status.corrected_data << 2) |
		(SELECTED_CONTROLLER(channel).status.index_pulse << 1) |
		(SELECTED_CONTROLLER(channel).status.err) );
      SELECTED_CONTROLLER(channel).status.index_pulse_count++;
      SELECTED_CONTROLLER(channel).status.index_pulse = 0;
      if (SELECTED_CONTROLLER(channel).status.index_pulse_count >= INDEX_PULSE_CYCLE) {
        SELECTED_CONTROLLER(channel).status.index_pulse = 1;
        SELECTED_CONTROLLER(channel).status.index_pulse_count = 0;
      }
    }
    if (port == 0x07) {
      rd_lower_irq((struct vm_device *)(ramdisk_state->private_data), channels[channel].irq);
    }
    goto return_value8;
    break;
    
  case 0x17: // Hard Disk Address Register 0x3f7
    // Obsolete and unsupported register.  Not driven by hard
    // disk controller.  Report all 1's.  If floppy controller
    // is handling this address, it will call this function
    // set/clear D7 (the only bit it handles), then return
    // the combined value
    value8 = 0xff;
    goto return_value8;
    break;
    
  default:
    RD_PANIC("hard drive: io read to address %x unsupported\n",
	      (unsigned) address);
    
    
    ////////////////////////////////////////////
  }
  
  Ramdisk_Print("\t\tError: hard drive: shouldnt get here!\n");
  return 0;
  
return_value32:
  Ramdisk_Print("\t\t32-bit read from %04x = %08x {%s}\n",
	    (unsigned) address, value32, SELECTED_TYPE_STRING(channel));
  return value32;
  
 return_value16:
  Ramdisk_Print("\t\t16-bit read from %04x = %04x {%s}\n",
	    (unsigned) address, value16, SELECTED_TYPE_STRING(channel));
  return value16;
  
 return_value8:
  Ramdisk_Print("\t\t8-bit read from %x = %02x {%s}\n",
	    (unsigned) address, value8, SELECTED_TYPE_STRING(channel));
  return value8;
}


static
void rd_write_handler(struct channel_t *channels, Bit32u address, 
		      Bit32u value, unsigned io_len)
{

  //  off_t logical_sector;
  //  off_t ret;
  rd_bool prev_control_reset;

  Bit32u id;
  int toc_length;

  Bit8u  channel = MAX_ATA_CHANNEL;
  Bit32u port = 0xff; // undefined

  //Ramdisk_Print("[rd_write_handler]\n");
  //  Bit8u atapi_command;
  //int alloc_length;

  for (channel=0; channel<MAX_ATA_CHANNEL; channel++) {
    if ((address & 0xfff8) == channels[channel].ioaddr1) {
      port = address - channels[channel].ioaddr1;
      break;
    }
    else if ((address & 0xfff8) == channels[channel].ioaddr2) {
      port = address - channels[channel].ioaddr2 + 0x10;
      break;
      }
    }

  if (channel == MAX_ATA_CHANNEL) {
    if (address != 0x03f6) {
      RD_PANIC("Panic: write: unable to find ATA channel, ioport=0x%04x\n", address);
    } else {
      channel = 0;
      port = address - 0x03e0;
    }
  }

  Ramdisk_Print("[W_handler] IO write to %x = %02x, channel = %d\n", (unsigned) address, (unsigned) value, channel);

  struct cdrom_interface *cdif = SELECTED_DRIVE(channel).cdrom.cd;

  switch (port) {

  case 0x00: // 0x1f0
    Ramdisk_Print("\t\twrite port 170\n");

    //////////////////////////////////////////////////////////
    switch (SELECTED_CONTROLLER(channel).current_command) {
    case 0x30: // WRITE SECTORS
      RD_PANIC("\t\tneed to implement 0x30(write sector) to port 0x170\n");
      break;
      
    case 0xa0: // PACKET
      if (SELECTED_CONTROLLER(channel).buffer_index >= PACKET_SIZE)
	RD_PANIC("IO write(0x%04x): buffer_index >= PACKET_SIZE", address);
      SELECTED_CONTROLLER(channel).buffer[SELECTED_CONTROLLER(channel).buffer_index] = value;
      SELECTED_CONTROLLER(channel).buffer[SELECTED_CONTROLLER(channel).buffer_index+1] = (value >> 8);
      SELECTED_CONTROLLER(channel).buffer_index += 2;
      
      /* if packet completely writtten */
      if (SELECTED_CONTROLLER(channel).buffer_index >= PACKET_SIZE) {
	// complete command received
	Bit8u atapi_command = SELECTED_CONTROLLER(channel).buffer[0];
	
	Ramdisk_Print("\t\tcdrom: ATAPI command 0x%x started\n", atapi_command);

	switch (atapi_command) {
	case 0x00: // test unit ready
	  if (SELECTED_DRIVE(channel).cdrom.ready) {
	    rd_atapi_cmd_nop(channels, channel);
	  } else {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	  }
	  rd_raise_interrupt(channels, channel);
	  break;
	  
	case 0x03: { // request sense
	  int alloc_length = SELECTED_CONTROLLER(channel).buffer[4];
	  rd_init_send_atapi_command(channels, channel, atapi_command, 18, alloc_length, false);
				    
	  // sense data
	  SELECTED_CONTROLLER(channel).buffer[0] = 0x70 | (1 << 7);
	  SELECTED_CONTROLLER(channel).buffer[1] = 0;
	  SELECTED_CONTROLLER(channel).buffer[2] = SELECTED_DRIVE(channel).sense.sense_key;
	  SELECTED_CONTROLLER(channel).buffer[3] = SELECTED_DRIVE(channel).sense.information.arr[0];
	  SELECTED_CONTROLLER(channel).buffer[4] = SELECTED_DRIVE(channel).sense.information.arr[1];
	  SELECTED_CONTROLLER(channel).buffer[5] = SELECTED_DRIVE(channel).sense.information.arr[2];
	  SELECTED_CONTROLLER(channel).buffer[6] = SELECTED_DRIVE(channel).sense.information.arr[3];
	  SELECTED_CONTROLLER(channel).buffer[7] = 17-7;
	  SELECTED_CONTROLLER(channel).buffer[8] = SELECTED_DRIVE(channel).sense.specific_inf.arr[0];
	  SELECTED_CONTROLLER(channel).buffer[9] = SELECTED_DRIVE(channel).sense.specific_inf.arr[1];
	  SELECTED_CONTROLLER(channel).buffer[10] = SELECTED_DRIVE(channel).sense.specific_inf.arr[2];
	  SELECTED_CONTROLLER(channel).buffer[11] = SELECTED_DRIVE(channel).sense.specific_inf.arr[3];
	  SELECTED_CONTROLLER(channel).buffer[12] = SELECTED_DRIVE(channel).sense.asc;
	  SELECTED_CONTROLLER(channel).buffer[13] = SELECTED_DRIVE(channel).sense.ascq;
	  SELECTED_CONTROLLER(channel).buffer[14] = SELECTED_DRIVE(channel).sense.fruc;
	  SELECTED_CONTROLLER(channel).buffer[15] = SELECTED_DRIVE(channel).sense.key_spec.arr[0];
	  SELECTED_CONTROLLER(channel).buffer[16] = SELECTED_DRIVE(channel).sense.key_spec.arr[1];
	  SELECTED_CONTROLLER(channel).buffer[17] = SELECTED_DRIVE(channel).sense.key_spec.arr[2];
	  
	  rd_ready_to_send_atapi(channels, channel);
	}
	  break;
	  
	case 0x1b: { // start stop unit
	  //bx_bool Immed = (SELECTED_CONTROLLER(channel).buffer[1] >> 0) & 1;
	  rd_bool LoEj = (SELECTED_CONTROLLER(channel).buffer[4] >> 1) & 1;
	  rd_bool Start = (SELECTED_CONTROLLER(channel).buffer[4] >> 0) & 1;
	  
	  if (!LoEj && !Start) { // stop the disc
	    RD_ERROR("FIXME: Stop disc not implemented\n");
	    rd_atapi_cmd_nop(channels, channel);
	    rd_raise_interrupt(channels, channel);
	  } else if (!LoEj && Start) { // start (spin up) the disc

	    SELECTED_DRIVE(channel).cdrom.cd->ops.start_cdrom(cdif);

	    RD_ERROR("FIXME: ATAPI start disc not reading TOC\n");
	    rd_atapi_cmd_nop(channels, channel);
	    rd_raise_interrupt(channels, channel);
	  } else if (LoEj && !Start) { // Eject the disc
	    rd_atapi_cmd_nop(channels, channel);
	    
	    if (SELECTED_DRIVE(channel).cdrom.ready) {

	      SELECTED_DRIVE(channel).cdrom.cd->ops.eject_cdrom(cdif);

	      SELECTED_DRIVE(channel).cdrom.ready = 0;
	      //bx_options.atadevice[channel][SLAVE_SELECTED(channel)].Ostatus->set(EJECTED);
	      //bx_gui->update_drive_status_buttons();
	    }
	    rd_raise_interrupt(channels, channel);
	  } else { // Load the disc
	    // My guess is that this command only closes the tray, that's a no-op for us
	    rd_atapi_cmd_nop(channels, channel);
	    rd_raise_interrupt(channels, channel);
	  }
	}
	  break;
	  
	case 0xbd: { // mechanism status
	  uint16 alloc_length = rd_read_16bit(SELECTED_CONTROLLER(channel).buffer + 8);
	  
	  if (alloc_length == 0)
	    RD_PANIC("Zero allocation length to MECHANISM STATUS not impl.\n");
	  
	  rd_init_send_atapi_command(channels, channel, atapi_command, 8, alloc_length, false);
	  
	  SELECTED_CONTROLLER(channel).buffer[0] = 0; // reserved for non changers
	  SELECTED_CONTROLLER(channel).buffer[1] = 0; // reserved for non changers
	  
	  SELECTED_CONTROLLER(channel).buffer[2] = 0; // Current LBA (TODO!)
	  SELECTED_CONTROLLER(channel).buffer[3] = 0; // Current LBA (TODO!)
	  SELECTED_CONTROLLER(channel).buffer[4] = 0; // Current LBA (TODO!)
	  
	  SELECTED_CONTROLLER(channel).buffer[5] = 1; // one slot
	  
	  SELECTED_CONTROLLER(channel).buffer[6] = 0; // slot table length
	  SELECTED_CONTROLLER(channel).buffer[7] = 0; // slot table length
	  
	  rd_ready_to_send_atapi(channels, channel);
	}
	  break;
	  
	case 0x5a: { // mode sense
	  uint16 alloc_length = rd_read_16bit(SELECTED_CONTROLLER(channel).buffer + 7);
	  
	  Bit8u PC = SELECTED_CONTROLLER(channel).buffer[2] >> 6;
	  Bit8u PageCode = SELECTED_CONTROLLER(channel).buffer[2] & 0x3f;
	  
	  switch (PC) {
	  case 0x0: // current values
	    switch (PageCode) {
	    case 0x01: // error recovery
	      rd_init_send_atapi_command(channels, channel, atapi_command, sizeof(struct error_recovery_t) + 8, alloc_length, false);
	      
	      rd_init_mode_sense_single(channels, channel, &SELECTED_DRIVE(channel).cdrom.current.error_recovery,
				     sizeof(struct error_recovery_t));
	      rd_ready_to_send_atapi(channels, channel);
	      break;
	      
	    case 0x2a: // CD-ROM capabilities & mech. status
	      rd_init_send_atapi_command(channels, channel, atapi_command, 28, alloc_length, false);
	      rd_init_mode_sense_single(channels, channel, &SELECTED_CONTROLLER(channel).buffer[8], 28);
	      SELECTED_CONTROLLER(channel).buffer[8] = 0x2a;
	      SELECTED_CONTROLLER(channel).buffer[9] = 0x12;
	      SELECTED_CONTROLLER(channel).buffer[10] = 0x00;
	      SELECTED_CONTROLLER(channel).buffer[11] = 0x00;
	      // Multisession, Mode 2 Form 2, Mode 2 Form 1
	      SELECTED_CONTROLLER(channel).buffer[12] = 0x70; 
	      SELECTED_CONTROLLER(channel).buffer[13] = (3 << 5);
	      SELECTED_CONTROLLER(channel).buffer[14] = (unsigned char)
		(1 |
		 (SELECTED_DRIVE(channel).cdrom.locked ? (1 << 1) : 0) |
		 (1 << 3) |
		 (1 << 5));
	      SELECTED_CONTROLLER(channel).buffer[15] = 0x00;
	      SELECTED_CONTROLLER(channel).buffer[16] = (706 >> 8) & 0xff;
	      SELECTED_CONTROLLER(channel).buffer[17] = 706 & 0xff;
	      SELECTED_CONTROLLER(channel).buffer[18] = 0;
	      SELECTED_CONTROLLER(channel).buffer[19] = 2;
	      SELECTED_CONTROLLER(channel).buffer[20] = (512 >> 8) & 0xff;
	      SELECTED_CONTROLLER(channel).buffer[21] = 512 & 0xff;
	      SELECTED_CONTROLLER(channel).buffer[22] = (706 >> 8) & 0xff;
	      SELECTED_CONTROLLER(channel).buffer[23] = 706 & 0xff;
	      SELECTED_CONTROLLER(channel).buffer[24] = 0;
	      SELECTED_CONTROLLER(channel).buffer[25] = 0;
	      SELECTED_CONTROLLER(channel).buffer[26] = 0;
	      SELECTED_CONTROLLER(channel).buffer[27] = 0;
	      rd_ready_to_send_atapi(channels, channel);
	      break;
	      
	    case 0x0d: // CD-ROM
	    case 0x0e: // CD-ROM audio control
	    case 0x3f: // all
	      RD_ERROR("cdrom: MODE SENSE (curr), code=%x not implemented yet\n",
			PageCode);
	    rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
			    ASC_INV_FIELD_IN_CMD_PACKET);
	    rd_raise_interrupt(channels, channel);
	    break;
	      
	  default:
	    // not implemeted by this device
	    Ramdisk_Print("\t\tcdrom: MODE SENSE PC=%x, PageCode=%x, not implemented by device\n",
		     PC, PageCode);
	    rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
			    ASC_INV_FIELD_IN_CMD_PACKET);
	    rd_raise_interrupt(channels, channel);
	    break;
	  }
	  break;
	    
	  case 0x1: // changeable values
	    switch (PageCode) {
	    case 0x01: // error recovery
	    case 0x0d: // CD-ROM
	    case 0x0e: // CD-ROM audio control
	    case 0x2a: // CD-ROM capabilities & mech. status
	    case 0x3f: // all
	      RD_ERROR("cdrom: MODE SENSE (chg), code=%x not implemented yet\n",
		       PageCode);
	    rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
			      ASC_INV_FIELD_IN_CMD_PACKET);
	    rd_raise_interrupt(channels, channel);
	    break;
	    
	  default:
	      // not implemeted by this device
	    Ramdisk_Print("\t\tcdrom: MODE SENSE PC=%x, PageCode=%x, not implemented by device\n",
			  PC, PageCode);
	  rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
			     ASC_INV_FIELD_IN_CMD_PACKET);
	  rd_raise_interrupt(channels, channel);
	  break;
	}
	  break;
	  
	case 0x2: // default values
	  switch (PageCode) {
	  case 0x01: // error recovery
	  case 0x0d: // CD-ROM
	  case 0x0e: // CD-ROM audio control
	  case 0x2a: // CD-ROM capabilities & mech. status
	  case 0x3f: // all
	    RD_PANIC("cdrom: MODE SENSE (dflt), code=%x\n",
		     PageCode);
	    break;
	    
	    default:
	      // not implemeted by this device
	      Ramdisk_Print("\t\tcdrom: MODE SENSE PC=%x, PageCode=%x, not implemented by device\n",
		      PC, PageCode);
	      rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
				 ASC_INV_FIELD_IN_CMD_PACKET);
	      rd_raise_interrupt(channels, channel);
	      break;
	  }
	  break;
	  
	case 0x3: // saved values not implemented
	  rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_SAVING_PARAMETERS_NOT_SUPPORTED);
	  rd_raise_interrupt(channels, channel);
	  break;
	  
	default:
	  RD_PANIC("Should not get here!\n");
	  break;
	}
      }
      break;
      
    case 0x12: { // inquiry
      uint8 alloc_length = SELECTED_CONTROLLER(channel).buffer[4];
      
      rd_init_send_atapi_command(channels, channel, atapi_command, 36, alloc_length, false);
      
      SELECTED_CONTROLLER(channel).buffer[0] = 0x05; // CD-ROM
      SELECTED_CONTROLLER(channel).buffer[1] = 0x80; // Removable
      SELECTED_CONTROLLER(channel).buffer[2] = 0x00; // ISO, ECMA, ANSI version
      SELECTED_CONTROLLER(channel).buffer[3] = 0x21; // ATAPI-2, as specified
      SELECTED_CONTROLLER(channel).buffer[4] = 31; // additional length (total 36)
      SELECTED_CONTROLLER(channel).buffer[5] = 0x00; // reserved
      SELECTED_CONTROLLER(channel).buffer[6] = 0x00; // reserved
      SELECTED_CONTROLLER(channel).buffer[7] = 0x00; // reserved
      
      // Vendor ID
      const char* vendor_id = "VTAB    ";
      int i;
      for (i = 0; i < 8; i++)
	SELECTED_CONTROLLER(channel).buffer[8+i] = vendor_id[i];
      
      // Product ID
      const char* product_id = "Turbo CD-ROM    ";
      for (i = 0; i < 16; i++)
	SELECTED_CONTROLLER(channel).buffer[16+i] = product_id[i];
      
      // Product Revision level
      const char* rev_level = "1.0 ";
      for (i = 0; i < 4; i++)
	SELECTED_CONTROLLER(channel).buffer[32+i] = rev_level[i];
      
      rd_ready_to_send_atapi(channels, channel);
    }
      break;
      
	case 0x25: { // read cd-rom capacity
	  // no allocation length???
	  rd_init_send_atapi_command(channels, channel, atapi_command, 8, 8, false);
      
	  if (SELECTED_DRIVE(channel).cdrom.ready) {
	    uint32 capacity = SELECTED_DRIVE(channel).cdrom.capacity;
	    Ramdisk_Print("\t\tCapacity is %d sectors (%d bytes)\n", capacity, capacity * 2048);
	    SELECTED_CONTROLLER(channel).buffer[0] = (capacity >> 24) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[1] = (capacity >> 16) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[2] = (capacity >> 8) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[3] = (capacity >> 0) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[4] = (2048 >> 24) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[5] = (2048 >> 16) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[6] = (2048 >> 8) & 0xff;
	    SELECTED_CONTROLLER(channel).buffer[7] = (2048 >> 0) & 0xff;
	    rd_ready_to_send_atapi(channels, channel);
	  } else {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	    rd_raise_interrupt(channels, channel);
	  }
	}
	  break;
      
	case 0xbe: { // read cd
	  if (SELECTED_DRIVE(channel).cdrom.ready) {
	    RD_ERROR("Read CD with CD present not implemented\n");
	    rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
	    rd_raise_interrupt(channels, channel);
	  } else {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	    rd_raise_interrupt(channels, channel);
	  }
	}
	  break;
      
	case 0x43: { // read toc
	  if (SELECTED_DRIVE(channel).cdrom.ready) {
	    
	    bool msf = (SELECTED_CONTROLLER(channel).buffer[1] >> 1) & 1;
	    uint8 starting_track = SELECTED_CONTROLLER(channel).buffer[6];
	    
	    uint16 alloc_length = rd_read_16bit(SELECTED_CONTROLLER(channel).buffer + 7);
	    
	    uint8 format = (SELECTED_CONTROLLER(channel).buffer[9] >> 6);
	    int i;
	    switch (format) {
	    case 0:
	      
	      if (!(SELECTED_DRIVE(channel).cdrom.cd->ops.read_toc(cdif, SELECTED_CONTROLLER(channel).buffer,
							       &toc_length, msf, starting_track))) {
		rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
				   ASC_INV_FIELD_IN_CMD_PACKET);
		rd_raise_interrupt(channels, channel);
	      } else {
		rd_init_send_atapi_command(channels, channel, atapi_command, toc_length, alloc_length, false);
		rd_ready_to_send_atapi(channels, channel);
	      }
	      break;
	      
	    case 1:
	      // multi session stuff. we ignore this and emulate a single session only
	      rd_init_send_atapi_command(channels, channel, atapi_command, 12, alloc_length, false);
	      
	      SELECTED_CONTROLLER(channel).buffer[0] = 0;
	      SELECTED_CONTROLLER(channel).buffer[1] = 0x0a;
	      SELECTED_CONTROLLER(channel).buffer[2] = 1;
	      SELECTED_CONTROLLER(channel).buffer[3] = 1;
	      for (i = 0; i < 8; i++)
		SELECTED_CONTROLLER(channel).buffer[4+i] = 0;
	      
	      rd_ready_to_send_atapi(channels, channel);
	      break;
	      
	    case 2:
	    default:
	      RD_PANIC("(READ TOC) Format %d not supported\n", format);
	      break;
	    }
	  } else {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	    rd_raise_interrupt(channels, channel);
	  }
	}
	  break;
      
	case 0x28: // read (10)
	case 0xa8: // read (12)
	  { 
	
	    uint32 transfer_length;
	    if (atapi_command == 0x28)
	      transfer_length = rd_read_16bit(SELECTED_CONTROLLER(channel).buffer + 7);
	    else
	      transfer_length = rd_read_32bit(SELECTED_CONTROLLER(channel).buffer + 6);
	    
	    uint32 lba = rd_read_32bit(SELECTED_CONTROLLER(channel).buffer + 2);
	    
	    if (!SELECTED_DRIVE(channel).cdrom.ready) {
	      rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	      rd_raise_interrupt(channels, channel);
	      break;
	    }
	    
	    if (transfer_length == 0) {
	      rd_atapi_cmd_nop(channels, channel);
	      rd_raise_interrupt(channels, channel);
	      Ramdisk_Print("\t\tREAD(%d) with transfer length 0, ok\n", atapi_command==0x28?10:12);
	      break;
	    }
	    
	    if (lba + transfer_length > SELECTED_DRIVE(channel).cdrom.capacity) {
	      rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_LOGICAL_BLOCK_OOR);
	      rd_raise_interrupt(channels, channel);
	      break;
	    }
	    
	    Ramdisk_Print("\t\tcdrom: READ (%d) LBA=%d LEN=%d\n", atapi_command==0x28?10:12, lba, transfer_length);
	    
	    // handle command
	    rd_init_send_atapi_command(channels, channel, atapi_command, transfer_length * 2048,
				       transfer_length * 2048, true);
	    SELECTED_DRIVE(channel).cdrom.remaining_blocks = transfer_length;
	    SELECTED_DRIVE(channel).cdrom.next_lba = lba;
	    rd_ready_to_send_atapi(channels, channel);
	  }
	  break;
	  
	case 0x2b: { // seek
	  uint32 lba = rd_read_32bit(SELECTED_CONTROLLER(channel).buffer + 2);
	  if (!SELECTED_DRIVE(channel).cdrom.ready) {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	    rd_raise_interrupt(channels, channel);
	    break;
	  }
	  
	  if (lba > SELECTED_DRIVE(channel).cdrom.capacity) {
	    rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_LOGICAL_BLOCK_OOR);
	    rd_raise_interrupt(channels, channel);
	    break;
	  }
	  Ramdisk_Print("\t\tcdrom: SEEK (ignored)\n");
	  rd_atapi_cmd_nop(channels, channel);
	  rd_raise_interrupt(channels, channel);
	}
	  break;
      
	case 0x1e: { // prevent/allow medium removal
	  if (SELECTED_DRIVE(channel).cdrom.ready) {
	    SELECTED_DRIVE(channel).cdrom.locked = SELECTED_CONTROLLER(channel).buffer[4] & 1;
	    rd_atapi_cmd_nop(channels, channel);
	  } else {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	  }
	  rd_raise_interrupt(channels, channel);
	}
	  break;
      
	case 0x42: { // read sub-channel
	  bool msf = get_packet_field(channel,1, 1, 1);
	  bool sub_q = get_packet_field(channel,2, 6, 1);
	  uint8 data_format = get_packet_byte(channel,3);
	  uint8 track_number = get_packet_byte(channel,6);
	  uint16 alloc_length = get_packet_word(channel,7);
	  UNUSED(msf);
	  UNUSED(data_format);
	  UNUSED(track_number);
	  
	  if (!SELECTED_DRIVE(channel).cdrom.ready) {
	    rd_atapi_cmd_error(channels, channel, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT);
	    rd_raise_interrupt(channels, channel);
	  } else {
	    SELECTED_CONTROLLER(channel).buffer[0] = 0;
	    SELECTED_CONTROLLER(channel).buffer[1] = 0; // audio not supported
	    SELECTED_CONTROLLER(channel).buffer[2] = 0;
	    SELECTED_CONTROLLER(channel).buffer[3] = 0;
	
	    int ret_len = 4; // header size
	
	    if (sub_q) { // !sub_q == header only
	      RD_ERROR("Read sub-channel with SubQ not implemented\n");
	      rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST,
				 ASC_INV_FIELD_IN_CMD_PACKET);
	      rd_raise_interrupt(channels, channel);
	    }
	
	    rd_init_send_atapi_command(channels, channel, atapi_command, ret_len, alloc_length, false);
	    rd_ready_to_send_atapi(channels, channel);
	  }
	}
	  break;
      
	case 0x51: { // read disc info
	  // no-op to keep the Linux CD-ROM driver happy
	  rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
	  rd_raise_interrupt(channels, channel);
	}
	  break;
      
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
	  RD_ERROR("ATAPI command 0x%x not implemented yet\n",
		   atapi_command);
	  rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
	  rd_raise_interrupt(channels, channel);
	  break;
	default:
	  RD_PANIC("Unknown ATAPI command 0x%x (%d)\n",
		   atapi_command, atapi_command);
	  // We'd better signal the error if the user chose to continue
	  rd_atapi_cmd_error(channels, channel, SENSE_ILLEGAL_REQUEST, ASC_INV_FIELD_IN_CMD_PACKET);
	  rd_raise_interrupt(channels, channel);
	  break;
	}
      }
      
      break;
      
      
    default:
      RD_PANIC("\t\tIO write(0x%x): current command is %02xh\n", address,
	       (unsigned) SELECTED_CONTROLLER(channel).current_command);
    }

/////////////////////////////////////////////////////////
    break;
  case 0x01: // hard disk write precompensation 0x1f1
    WRITE_FEATURES(channel,value);
    break;

  case 0x02: // hard disk sector count 0x1f2
    WRITE_SECTOR_COUNT(channel,value);
    break;
    
  case 0x03: // hard disk sector number 0x1f3
    WRITE_SECTOR_NUMBER(channel,value);
    break;
    
  case 0x04: // hard disk cylinder low 0x1f4
    WRITE_CYLINDER_LOW(channel,value);
    break;
    
  case 0x05: // hard disk cylinder high 0x1f5
    WRITE_CYLINDER_HIGH(channel,value);
    break;
    
  case 0x06: // hard disk drive and head register 0x1f6
    // b7 Extended data field for ECC
    // b6/b5: Used to be sector size.  00=256,01=512,10=1024,11=128
    //   Since 512 was always used, bit 6 was taken to mean LBA mode:
    //     b6 1=LBA mode, 0=CHS mode
    //     b5 1
    // b4: DRV
    // b3..0 HD3..HD0
    
    if ( (value & 0xa0) != 0xa0 ) // 1x1xxxxx
      Ramdisk_Print("\t\tIO write 0x%x (%02x): not 1x1xxxxxb\n", address, (unsigned) value);
    
    Bit32u drvsel = channels[channel].drive_select = (value >> 4) & 0x01;
    WRITE_HEAD_NO(channel,value & 0xf);
    if (SELECTED_CONTROLLER(channel).lba_mode == 0 && ((value >> 6) & 1) == 1){
      Ramdisk_Print("\t\tenabling LBA mode\n");
	}
    WRITE_LBA_MODE(channel,(value >> 6) & 1);
    SELECTED_DRIVE(channel).cdrom.cd->lba = (value >> 6) & 1;
    
    
    if (!SELECTED_IS_PRESENT(channel)) {
      Ramdisk_Print ("\t\tError: device set to %d which does not exist! channel = %d\n",drvsel, channel);
      SELECTED_CONTROLLER(channel).error_register = 0x04; // aborted
      SELECTED_CONTROLLER(channel).status.err = 1;
    }
    
    break;
    
    
  case 0x07: // hard disk command 0x1f7

      switch (value) {
        // ATAPI commands
        case 0xa1: // IDENTIFY PACKET DEVICE
	      if (SELECTED_IS_CD(channel)) {
		    SELECTED_CONTROLLER(channel).current_command = value;
		    SELECTED_CONTROLLER(channel).error_register = 0;

		    SELECTED_CONTROLLER(channel).status.busy = 0;
		    SELECTED_CONTROLLER(channel).status.drive_ready = 1;
		    SELECTED_CONTROLLER(channel).status.write_fault = 0;
		    SELECTED_CONTROLLER(channel).status.drq   = 1;
		    SELECTED_CONTROLLER(channel).status.err   = 0;

		    SELECTED_CONTROLLER(channel).status.seek_complete = 1;
		    SELECTED_CONTROLLER(channel).status.corrected_data = 0;

		    SELECTED_CONTROLLER(channel).buffer_index = 0;
		    rd_raise_interrupt(channels, channel);
		    rd_identify_ATAPI_drive(channels, channel);
	      } else {
		rd_command_aborted(channels, channel, 0xa1);
	      }
	      break;

        case 0xa0: // SEND PACKET (atapi)
	      if (SELECTED_IS_CD(channel)) {
		    // PACKET
		    if (SELECTED_CONTROLLER(channel).features & (1 << 0))
			  RD_PANIC("\t\tPACKET-DMA not supported");
		    if (SELECTED_CONTROLLER(channel).features & (1 << 1))
			  RD_PANIC("\t\tPACKET-overlapped not supported");

		    // We're already ready!
		    SELECTED_CONTROLLER(channel).sector_count = 1;
		    SELECTED_CONTROLLER(channel).status.busy = 0;
		    SELECTED_CONTROLLER(channel).status.write_fault = 0;
		    // serv bit??
		    SELECTED_CONTROLLER(channel).status.drq = 1;
		    SELECTED_CONTROLLER(channel).status.err = 0;

		    // NOTE: no interrupt here
		    SELECTED_CONTROLLER(channel).current_command = value;
		    SELECTED_CONTROLLER(channel).buffer_index = 0;
	      } else {
		rd_command_aborted (channels, channel, 0xa0);
	      }
	      break;

      default:
	Ramdisk_Print("\t\tneed translate command %2x\n", value);
	break;
      }//switch(value)

    
  case 0x16: // hard disk adapter control 0x3f6 
    // (mch) Even if device 1 was selected, a write to this register
    // goes to device 0 (if device 1 is absent)

    prev_control_reset = SELECTED_CONTROLLER(channel).control.reset;
    channels[channel].drives[0].controller.control.reset         = value & 0x04;
    channels[channel].drives[1].controller.control.reset         = value & 0x04;
    // CGS: was: SELECTED_CONTROLLER(channel).control.disable_irq    = value & 0x02;
    channels[channel].drives[0].controller.control.disable_irq = value & 0x02;
    channels[channel].drives[1].controller.control.disable_irq = value & 0x02;
    
    Ramdisk_Print("\t\tadpater control reg: reset controller = %d\n",
		  (unsigned) (SELECTED_CONTROLLER(channel).control.reset) ? 1 : 0);
    Ramdisk_Print("\t\tadpater control reg: disable_irq(X) = %d\n",
		  (unsigned) (SELECTED_CONTROLLER(channel).control.disable_irq) ? 1 : 0);
    
    if (!prev_control_reset && SELECTED_CONTROLLER(channel).control.reset) {
      // transition from 0 to 1 causes all drives to reset
      Ramdisk_Print("\t\thard drive: RESET\n");
      
      // (mch) Set BSY, drive not ready
      for (id = 0; id < 2; id++) {
	CONTROLLER(channel,id).status.busy           = 1;
	CONTROLLER(channel,id).status.drive_ready    = 0;
	CONTROLLER(channel,id).reset_in_progress     = 1;
	
	CONTROLLER(channel,id).status.write_fault    = 0;
	CONTROLLER(channel,id).status.seek_complete  = 1;
	CONTROLLER(channel,id).status.drq            = 0;
	CONTROLLER(channel,id).status.corrected_data = 0;
	CONTROLLER(channel,id).status.err            = 0;
	
	CONTROLLER(channel,id).error_register = 0x01; // diagnostic code: no error
	
	CONTROLLER(channel,id).current_command = 0x00;
	CONTROLLER(channel,id).buffer_index = 0;
	
	CONTROLLER(channel,id).sectors_per_block = 0x80;
	CONTROLLER(channel,id).lba_mode          = 0;
	
	CONTROLLER(channel,id).control.disable_irq = 0;
	rd_lower_irq((struct vm_device *)(ramdisk_state->private_data), channels[channel].irq);
      }
    } else if (SELECTED_CONTROLLER(channel).reset_in_progress &&
	       !SELECTED_CONTROLLER(channel).control.reset) {
      // Clear BSY and DRDY
      Ramdisk_Print("\t\tReset complete {%s}\n", SELECTED_TYPE_STRING(channel));
      for (id = 0; id < 2; id++) {
	CONTROLLER(channel,id).status.busy           = 0;
	CONTROLLER(channel,id).status.drive_ready    = 1;
	CONTROLLER(channel,id).reset_in_progress     = 0;
	
	// Device signature
	if (DRIVE_IS_HD(channel,id)) {
	  Ramdisk_Print("\t\tdrive %d/%d is harddrive\n", channel, id);
	  CONTROLLER(channel,id).head_no        = 0;
	  CONTROLLER(channel,id).sector_count   = 1;
	  CONTROLLER(channel,id).sector_no      = 1;
	  CONTROLLER(channel,id).cylinder_no    = 0;
	} else {
	  CONTROLLER(channel,id).head_no        = 0;
	  CONTROLLER(channel,id).sector_count   = 1;
	  CONTROLLER(channel,id).sector_no      = 1;
	  CONTROLLER(channel,id).cylinder_no    = 0xeb14;
	}
      }
    }
    Ramdisk_Print("\t\ts[0].controller.control.disable_irq = %02x\n", (channels[channel].drives[0]).controller.control.disable_irq);
    Ramdisk_Print("\t\ts[1].controller.control.disable_irq = %02x\n", (channels[channel].drives[1]).controller.control.disable_irq);
    break;
    
  default:
    RD_PANIC("\t\thard drive: io write to address %x = %02x\n",
		  (unsigned) address, (unsigned) value);
  }  
  
  return;
}


static 
void rd_identify_ATAPI_drive(struct channel_t *channels, Bit8u channel)
{
  unsigned i;
  const char* serial_number = " VT00001\0\0\0\0\0\0\0\0\0\0\0\0";
  const char* firmware = "ALPHA1  ";

  SELECTED_DRIVE(channel).id_drive[0] = (2 << 14) | (5 << 8) | (1 << 7) | (2 << 5) | (0 << 0); // Removable CDROM, 50us response, 12 byte packets

  for (i = 1; i <= 9; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;


  for (i = 0; i < 10; i++) {
	SELECTED_DRIVE(channel).id_drive[10+i] = (serial_number[i*2] << 8) |
	      serial_number[i*2 + 1];
  }

  for (i = 20; i <= 22; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;


  for (i = 0; i < strlen(firmware)/2; i++) {
	SELECTED_DRIVE(channel).id_drive[23+i] = (firmware[i*2] << 8) |
	      firmware[i*2 + 1];
  }
  V3_ASSERT((23+i) == 27);
  
  for (i = 0; i < strlen((char *) SELECTED_MODEL(channel))/2; i++) {
	SELECTED_DRIVE(channel).id_drive[27+i] = (SELECTED_MODEL(channel)[i*2] << 8) |
	      SELECTED_MODEL(channel)[i*2 + 1];
  }
  V3_ASSERT((27+i) == 47);

  SELECTED_DRIVE(channel).id_drive[47] = 0;
  SELECTED_DRIVE(channel).id_drive[48] = 1; // 32 bits access

  SELECTED_DRIVE(channel).id_drive[49] = (1 << 9); // LBA supported

  SELECTED_DRIVE(channel).id_drive[50] = 0;
  SELECTED_DRIVE(channel).id_drive[51] = 0;
  SELECTED_DRIVE(channel).id_drive[52] = 0;

  SELECTED_DRIVE(channel).id_drive[53] = 3; // words 64-70, 54-58 valid

  for (i = 54; i <= 62; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;

  // copied from CFA540A
  SELECTED_DRIVE(channel).id_drive[63] = 0x0103; // variable (DMA stuff)
  SELECTED_DRIVE(channel).id_drive[64] = 0x0001; // PIO
  SELECTED_DRIVE(channel).id_drive[65] = 0x00b4;
  SELECTED_DRIVE(channel).id_drive[66] = 0x00b4;
  SELECTED_DRIVE(channel).id_drive[67] = 0x012c;
  SELECTED_DRIVE(channel).id_drive[68] = 0x00b4;

  SELECTED_DRIVE(channel).id_drive[69] = 0;
  SELECTED_DRIVE(channel).id_drive[70] = 0;
  SELECTED_DRIVE(channel).id_drive[71] = 30; // faked
  SELECTED_DRIVE(channel).id_drive[72] = 30; // faked
  SELECTED_DRIVE(channel).id_drive[73] = 0;
  SELECTED_DRIVE(channel).id_drive[74] = 0;

  SELECTED_DRIVE(channel).id_drive[75] = 0;

  for (i = 76; i <= 79; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;

  SELECTED_DRIVE(channel).id_drive[80] = 0x1e; // supports up to ATA/ATAPI-4
  SELECTED_DRIVE(channel).id_drive[81] = 0;
  SELECTED_DRIVE(channel).id_drive[82] = 0;
  SELECTED_DRIVE(channel).id_drive[83] = 0;
  SELECTED_DRIVE(channel).id_drive[84] = 0;
  SELECTED_DRIVE(channel).id_drive[85] = 0;
  SELECTED_DRIVE(channel).id_drive[86] = 0;
  SELECTED_DRIVE(channel).id_drive[87] = 0;
  SELECTED_DRIVE(channel).id_drive[88] = 0;

  for (i = 89; i <= 126; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;

  SELECTED_DRIVE(channel).id_drive[127] = 0;
  SELECTED_DRIVE(channel).id_drive[128] = 0;

  for (i = 129; i <= 159; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;

  for (i = 160; i <= 255; i++)
	SELECTED_DRIVE(channel).id_drive[i] = 0;

  // now convert the id_drive array (native 256 word format) to
  // the controller buffer (512 bytes)
  Bit16u temp16;
  for (i = 0; i <= 255; i++) {
	temp16 = SELECTED_DRIVE(channel).id_drive[i];
	SELECTED_CONTROLLER(channel).buffer[i*2] = temp16 & 0x00ff;
	SELECTED_CONTROLLER(channel).buffer[i*2+1] = temp16 >> 8;
  }

  return;
}



static 
void rd_raise_interrupt(struct channel_t *channels, Bit8u channel)
{
  Bit32u irq;
  struct vm_device *dev;

  Ramdisk_Print("[raise_interrupt] disable_irq = %02x\n", SELECTED_CONTROLLER(channel).control.disable_irq);

  if (!SELECTED_CONTROLLER(channel).control.disable_irq) { 
    Ramdisk_Print("\t\traising interrupt\n"); 
  } else { 
    Ramdisk_Print("\t\tNot raising interrupt\n"); 
  }
  if (!SELECTED_CONTROLLER(channel).control.disable_irq) {
    irq = channels[channel].irq; 
    Ramdisk_Print("\t\tRaising interrupt %d {%s}\n\n", irq, SELECTED_TYPE_STRING(channel));
    //        DEV_pic_raise_irq(irq);
    dev = (struct vm_device*) ramdisk_state->private_data;
    Ramdisk_Print("\t\tdev = %x\n", dev);
    dev->vm->vm_ops.raise_irq(dev->vm, irq);
  } else {
    Ramdisk_Print("\t\tirq is disabled\n");
  }
  
  return;
}

static
void rd_lower_irq(struct vm_device *dev, Bit32u irq)// __attribute__(regparm(1))
{
  Ramdisk_Print("[lower_irq] irq = %d\n", irq);
  dev->vm->vm_ops.lower_irq(dev->vm, irq);
}


/*
 * Public Routines
 */
static
int ramdisk_read_port(ushort_t port,
			 void *src,
			 uint_t length,
			 struct vm_device *dev)
{
  uint_t i;
  //Ramdisk_Print("[ramdisk_read_port] port = %x, length = %d\n", port, length);
    switch (length) {
    case 1:
      ((uchar_t*)src)[0] = rd_read_handler(ramdisk_state->channels, port, length);
      break;
    case 2:
      ((ushort_t*)src)[0] = rd_read_handler(ramdisk_state->channels, port, length);
      break;
    case 4:
      ((uint_t*)src)[0] = rd_read_handler(ramdisk_state->channels, port, length);
      break;
    default:
      for (i = 0; i < length; i++) { 
	((uchar_t*)src)[i] = rd_read_handler(ramdisk_state->channels, port, 1);
      }
    }//switch length

  return length;
}


static
int ramdisk_write_port(ushort_t port,
			 void *src,
			 uint_t length,
			 struct vm_device *dev)
{
  //Ramdisk_Print("[ramdisk_write_port] port = %x, length = %d\n", port, length);
  /*
  uint_t i;

  for (i = 0; i < length; i++)
    Ramdisk_Print("\t\tsrc[%d] = 0x%02x\n", i, ((uchar_t*)src)[i]);
  */

  switch(length) {
  case 1:
    rd_write_handler(ramdisk_state->channels, port, *((uchar_t *)src), length);
    break;
  case 2:
    rd_write_handler(ramdisk_state->channels, port, *((ushort_t *)src), length);
    break;
  case 4:
    rd_write_handler(ramdisk_state->channels, port, *((uint_t *)src), length);
    break;
  default:
    rd_write_handler(ramdisk_state->channels, port, *((uchar_t *)src), length);
    break;
  }

  return length;
}


static void trace_info(ushort_t port, void *src, uint_t length)
{
  switch(port){

  case 0x3e8:
    if (length == 1 && *((uchar_t*) src) == ATA_DETECT)
      Ramdisk_Print("ata_dectect()\n");
    break;

  case 0x3e9:
    if (length == 1 && *((uchar_t*) src) == ATA_RESET)
      Ramdisk_Print("ata_reset()\n");
    break;

  case 0x3ea:
    if (length == 1 && *((uchar_t*) src) == ATA_CMD_DATA_IN)
      Ramdisk_Print("ata_cmd_data_in()\n");
    break;

  case 0x3eb:
    if (length == 1 && *((uchar_t*) src) == ATA_CMD_DATA_OUT)
      Ramdisk_Print("ata_cmd_data_out()\n");
    break;

  case 0x3ec:
    if (length == 1 && *((uchar_t*) src) == ATA_CMD_PACKET)
      Ramdisk_Print("ata_cmd_packet()\n");
    break;

  case 0x3ed:
    if (length == 1 && *((uchar_t*) src) == ATAPI_GET_SENSE)
      Ramdisk_Print("atapi_get_sense()\n");
    break;

  case 0x3ee:
    if (length == 1 && *((uchar_t*) src) == ATAPI_IS_READY)
      Ramdisk_Print("atapi_is_ready()\n");
    break;

  case 0x3ef:
    if (length == 1 && *((uchar_t*) src) == ATAPI_IS_CDROM)
      Ramdisk_Print("atapi_is_cdrom()\n");
    break;


  case 0x2e8:
    if (length == 1 && *((uchar_t*) src) == CDEMU_INIT)
      Ramdisk_Print("cdemu_init()\n");
    break;

  case 0x2e9:
    if (length == 1 && *((uchar_t*) src) == CDEMU_ISACTIVE)
      Ramdisk_Print("cdemu_isactive()\n");
    break;

  case 0x2ea:
    if (length == 1 && *((uchar_t*) src) == CDEMU_EMULATED_DRIVE)
      Ramdisk_Print("cdemu_emulated_drive()\n");
    break;

  case 0x2eb:
    if (length == 1 && *((uchar_t*) src) == CDROM_BOOT)
      Ramdisk_Print("cdrom_boot()\n");
    break;

  case 0x2ec:
    if (length == 1 && *((uchar_t*) src) == HARD_DRIVE_POST)
      Ramdisk_Print("ata_hard_drive_post()\n");
    break;

  case 0x2ed:
    if (length == 1)
      Ramdisk_Print("ata_device_no(%d)\n", *((uchar_t*) src));
    break;

  case 0x2ee:
    if (length == 1)
      Ramdisk_Print("ata_device_type(%d)\n", *((uchar_t*) src));
    break;

  case 0x2ef:
    if (length == 1 && *((uchar_t*) src) == INT13_HARDDISK)
      Ramdisk_Print("int13_harddrive()\n");
    break;

  case 0x2f8:
    if (length == 1 && *((uchar_t*) src) == INT13_CDROM)
      Ramdisk_Print("int13_cdrom()\n");
    break;

  case 0x2f9:
    if (length == 1 && *((uchar_t*) src) == INT13_CDEMU)
      Ramdisk_Print("int13_cdemu()\n");
    break;

  case 0x2fa:
    if (length == 1 && *((uchar_t*) src) == INT13_ELTORITO)
      Ramdisk_Print("int13_eltorito()\n");
    break;

  case 0x2fb:
    if (length == 1 && *((uchar_t*) src) == INT13_DISKETTE_FUNCTION)
      Ramdisk_Print("int13_diskette_function()\n");
    break;


  default:
    break;
  }
}


int ramdisk_read_port_ignore(ushort_t port,
			 void *src,
			 uint_t length,
			 struct vm_device *dev)
{
  //  Ramdisk_Print("[ramdisk_read_port_ignore] port = %x, length = %d\n", port, length);
  return length;
}

int ramdisk_write_port_ignore(ushort_t port,
			 void *src,
			 uint_t length,
			 struct vm_device *dev)
{

  //  Ramdisk_Print("[ramdisk_write_port_ignore] port = %x, length = %d\n", port, length);

  trace_info(port, src, length);
  return length;
}

//////////////////////////////////////////////////////////////////////////

/*
 * ATAPI subroutines
 */

static 
void rd_init_send_atapi_command(struct channel_t *channels, Bit8u channel, Bit8u command, int req_length, int alloc_length, bool lazy)
{
  // SELECTED_CONTROLLER(channel).byte_count is a union of SELECTED_CONTROLLER(channel).cylinder_no;
  // lazy is used to force a data read in the buffer at the next read.
  
  Ramdisk_Print("[rd_init_send_atapi_cmd]\n");
  if (SELECTED_CONTROLLER(channel).byte_count == 0xffff)
    SELECTED_CONTROLLER(channel).byte_count = 0xfffe;
  
  if ((SELECTED_CONTROLLER(channel).byte_count & 1)
      && !(alloc_length <= SELECTED_CONTROLLER(channel).byte_count)) {
    Ramdisk_Print("\t\tOdd byte count (0x%04x) to ATAPI command 0x%02x, using 0x%x\n", 
		  SELECTED_CONTROLLER(channel).byte_count, command, SELECTED_CONTROLLER(channel).byte_count - 1);
    SELECTED_CONTROLLER(channel).byte_count -= 1;
  }
  
  if (SELECTED_CONTROLLER(channel).byte_count == 0)
    RD_PANIC("\t\tATAPI command with zero byte count\n");
  
  if (alloc_length < 0)
    RD_PANIC("\t\tAllocation length < 0\n");
  if (alloc_length == 0)
    alloc_length = SELECTED_CONTROLLER(channel).byte_count;
  
  SELECTED_CONTROLLER(channel).interrupt_reason.i_o = 1;
  SELECTED_CONTROLLER(channel).interrupt_reason.c_d = 0;
  SELECTED_CONTROLLER(channel).status.busy = 0;
  SELECTED_CONTROLLER(channel).status.drq = 1;
  SELECTED_CONTROLLER(channel).status.err = 0;
  
  // no bytes transfered yet
  if (lazy)
    SELECTED_CONTROLLER(channel).buffer_index = 2048;
  else
    SELECTED_CONTROLLER(channel).buffer_index = 0;
  SELECTED_CONTROLLER(channel).drq_index = 0;
  
  if (SELECTED_CONTROLLER(channel).byte_count > req_length)
    SELECTED_CONTROLLER(channel).byte_count = req_length;
  
  if (SELECTED_CONTROLLER(channel).byte_count > alloc_length)
    SELECTED_CONTROLLER(channel).byte_count = alloc_length;
  
  SELECTED_DRIVE(channel).atapi.command = command;
  SELECTED_DRIVE(channel).atapi.drq_bytes = SELECTED_CONTROLLER(channel).byte_count;
  SELECTED_DRIVE(channel).atapi.total_bytes_remaining = (req_length < alloc_length) ? req_length : alloc_length;
  
  // if (lazy) {
  // // bias drq_bytes and total_bytes_remaining
  // SELECTED_DRIVE(channel).atapi.drq_bytes += 2048;
  // SELECTED_DRIVE(channel).atapi.total_bytes_remaining += 2048;
  // }
}


static 
void rd_atapi_cmd_error(struct channel_t *channels, Bit8u channel, sense_t sense_key, asc_t asc)
{
  Ramdisk_Print("[rd_atapi_cmd_error]\n");
  Ramdisk_Print("Error: atapi_cmd_error channel=%02x key=%02x asc=%02x\n", channel, sense_key, asc);

  SELECTED_CONTROLLER(channel).error_register = sense_key << 4;
  SELECTED_CONTROLLER(channel).interrupt_reason.i_o = 1;
  SELECTED_CONTROLLER(channel).interrupt_reason.c_d = 1;
  SELECTED_CONTROLLER(channel).interrupt_reason.rel = 0;
  SELECTED_CONTROLLER(channel).status.busy = 0;
  SELECTED_CONTROLLER(channel).status.drive_ready = 1;
  SELECTED_CONTROLLER(channel).status.write_fault = 0;
  SELECTED_CONTROLLER(channel).status.drq = 0;
  SELECTED_CONTROLLER(channel).status.err = 1;
  
  SELECTED_DRIVE(channel).sense.sense_key = sense_key;
  SELECTED_DRIVE(channel).sense.asc = asc;
  SELECTED_DRIVE(channel).sense.ascq = 0;
}


static 
void rd_atapi_cmd_nop(struct channel_t *channels, Bit8u channel)
{
  Ramdisk_Print("[rd_atapi_cmd_nop]\n");
  SELECTED_CONTROLLER(channel).interrupt_reason.i_o = 1;
  SELECTED_CONTROLLER(channel).interrupt_reason.c_d = 1;
  SELECTED_CONTROLLER(channel).interrupt_reason.rel = 0;
  SELECTED_CONTROLLER(channel).status.busy = 0;
  SELECTED_CONTROLLER(channel).status.drive_ready = 1;
  SELECTED_CONTROLLER(channel).status.drq = 0;
  SELECTED_CONTROLLER(channel).status.err = 0;
}


static 
void rd_init_mode_sense_single(struct channel_t *channels, 
			       Bit8u channel, const void* src, int size)
{
  Ramdisk_Print("[rd_init_mode_sense_single]\n");
  // Header
  SELECTED_CONTROLLER(channel).buffer[0] = (size+6) >> 8;
  SELECTED_CONTROLLER(channel).buffer[1] = (size+6) & 0xff;
  SELECTED_CONTROLLER(channel).buffer[2] = 0x70; // no media present
  SELECTED_CONTROLLER(channel).buffer[3] = 0; // reserved
  SELECTED_CONTROLLER(channel).buffer[4] = 0; // reserved
  SELECTED_CONTROLLER(channel).buffer[5] = 0; // reserved
  SELECTED_CONTROLLER(channel).buffer[6] = 0; // reserved
  SELECTED_CONTROLLER(channel).buffer[7] = 0; // reserved
  
  // Data
  memcpy(SELECTED_CONTROLLER(channel).buffer + 8, src, size);
}


static 
void rd_ready_to_send_atapi(struct channel_t *channels, Bit8u channel)
{
  Ramdisk_Print("[rd_ready_to_send_atapi]\n");
  rd_raise_interrupt(ramdisk_state->channels, channel);
}


static 
void rd_command_aborted(struct channel_t *channels, 
			Bit8u channel, unsigned value)
{
  Ramdisk_Print("[rd_command_aborted]\n");
  Ramdisk_Print("\t\taborting on command 0x%02x {%s}\n", value, SELECTED_TYPE_STRING(channel));
  SELECTED_CONTROLLER(channel).current_command = 0;
  SELECTED_CONTROLLER(channel).status.busy = 0;
  SELECTED_CONTROLLER(channel).status.drive_ready = 1;
  SELECTED_CONTROLLER(channel).status.err = 1;
  SELECTED_CONTROLLER(channel).error_register = 0x04; // command ABORTED
  SELECTED_CONTROLLER(channel).status.drq = 0;
  SELECTED_CONTROLLER(channel).status.seek_complete = 0;
  SELECTED_CONTROLLER(channel).status.corrected_data = 0;
  SELECTED_CONTROLLER(channel).buffer_index = 0;
  rd_raise_interrupt(ramdisk_state->channels, channel);
}


static int ramdisk_init_device(struct vm_device *dev)
{
  struct ramdisk_t *ramdisk_state = (struct ramdisk_t *)dev->private_data;

  ramdisk_state->cops.init(ramdisk_state, dev);

  //hook ports IDE 0x170-0x177, 0x376 & 0x377
  dev_hook_io(dev, 0x170, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x171, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x172, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x173, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x174, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x175, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x176, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x177, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x376, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  dev_hook_io(dev, 0x377, 
	      (ramdisk_state->eops.read_port), 
	      (ramdisk_state->eops.write_port));

  //Debug ports: 0x3e8-0x3ef & 0x2e8-0x2ef

#ifdef DEBUG_RAMDISK

  dev_hook_io(dev, 0x3e8, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3e9, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3ea, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3eb, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3ec, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3ed, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3ee, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x3ef, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2e8, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2e9, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2ea, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2eb, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2ec, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2ed, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2ee, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

  dev_hook_io(dev, 0x2ef, 
	      (ramdisk_state->eops.read_port_ignore), 
	      (ramdisk_state->eops.write_port_ignore));

#endif

}


static int ramdisk_deinit_device(struct vm_device *dev)
{
  struct ramdisk_t *ramdisk_state = (struct ramdisk_t *)(dev->private_data);
  ramdisk_state->cops.close(ramdisk_state);

  return 0;
}

static struct vm_device_ops dev_ops = {
  .init = ramdisk_init_device,
  .deinit = ramdisk_deinit_device,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};



/*
 * Success: return 0; 
 * Failure: return integer greater than 0
 */

/*
struct ramdisk_t * create_ramdisk()
{
  struct ramdisk_t *ramdisk;
  ramdisk = (struct ramdisk_t *)V3_Malloc(sizeof(struct ramdisk_t));  
  V3_ASSERT(ramdisk != NULL);
  
  ramdisk->cops.init = &rd_init_harddrive;
  ramdisk->cops.close = &rd_close_harddrive;
  ramdisk->cops.reset = &rd_reset_harddrive;

  ramdisk->eops.read_port = &ramdisk_read_port;
  ramdisk->eops.write_port = &ramdisk_write_port;
  ramdisk->eops.read_port_ignore = &ramdisk_read_port_ignore;
  ramdisk->eops.write_port_ignore = &ramdisk_write_port_ignore;
 
  return ramdisk;
}

*/


struct vm_device *create_ramdisk()
{

  struct ramdisk_t *ramdisk;
  ramdisk = (struct ramdisk_t *)V3_Malloc(sizeof(struct ramdisk_t));  
  V3_ASSERT(ramdisk != NULL);  

  Ramdisk_Print("[create_ramdisk]\n");
  ramdisk->cops.init = &rd_init_harddrive;
  ramdisk->cops.close = &rd_close_harddrive;
  ramdisk->cops.reset = &rd_reset_harddrive;

  ramdisk->eops.read_port = &ramdisk_read_port;
  ramdisk->eops.write_port = &ramdisk_write_port;
  ramdisk->eops.read_port_ignore = &ramdisk_read_port_ignore;
  ramdisk->eops.write_port_ignore = &ramdisk_write_port_ignore;

  struct vm_device *device = create_device("RAMDISK", &dev_ops, ramdisk);

  return device;
}
