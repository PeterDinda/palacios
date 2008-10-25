/* 
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

#ifndef __DEVICES_IDE_H__
#define __DEVICES_IDE_H__

#ifdef __V3VEE__


#include <palacios/vmm_types.h>


typedef long off_t;
typedef sint32_t ssize_t;
typedef unsigned int rd_bool;
typedef uchar_t Bit8u;
typedef ushort_t Bit16u;
typedef uint32_t Bit32u;
typedef uint64_t Bit64u;



#define MAX_ATA_CHANNEL 4

typedef enum _sense {
      SENSE_NONE = 0, 
      SENSE_NOT_READY = 2, 
      SENSE_ILLEGAL_REQUEST = 5,
      SENSE_UNIT_ATTENTION = 6
} sense_t ;

typedef enum _asc {
      ASC_INV_FIELD_IN_CMD_PACKET = 0x24,
      ASC_MEDIUM_NOT_PRESENT = 0x3a,
      ASC_SAVING_PARAMETERS_NOT_SUPPORTED = 0x39,
      ASC_LOGICAL_BLOCK_OOR = 0x21
} asc_t ;



typedef struct  {
  unsigned cylinders;
  unsigned heads;
  unsigned sectors;
} device_image_t;




struct interrupt_reason_t {
  unsigned  c_d : 1; 
  unsigned  i_o : 1; 
  unsigned  rel : 1; 
  unsigned  tag : 5; 
};


struct controller_status {
  rd_bool busy;
  rd_bool drive_ready;
  rd_bool write_fault;
  rd_bool seek_complete;
  rd_bool drq;
  rd_bool corrected_data;
  rd_bool index_pulse;
  unsigned int index_pulse_count;
  rd_bool err;
};





struct  sense_info_t {
  sense_t sense_key;

  struct  {
    Bit8u arr[4];
  } information;

  struct  {
    Bit8u arr[4];
  } specific_inf;

  struct  {
    Bit8u arr[3];
  } key_spec;

  Bit8u fruc;
  Bit8u asc;
  Bit8u ascq;
};


struct  error_recovery_t {
  unsigned char data[8];
};

struct  cdrom_t {
  rd_bool ready;
  rd_bool locked;

  struct cdrom_ops * cd;

  uint32_t capacity;
  int next_lba;
  int remaining_blocks;

  struct  currentStruct {
    struct error_recovery_t error_recovery;
  } current;

};

struct  atapi_t {
  uint8_t command;
  int drq_bytes;
  int total_bytes_remaining;
};


typedef enum { IDE_NONE, IDE_DISK, IDE_CDROM } device_type_t;

struct controller_t  {
  struct controller_status status;
  Bit8u    error_register;
  Bit8u    head_no;

  union {
    Bit8u    sector_count;
    struct interrupt_reason_t interrupt_reason;
  };


  Bit8u    sector_no;

  union  {
    Bit16u   cylinder_no;
    Bit16u   byte_count;
  };

  Bit8u    buffer[2048]; 
  Bit32u   buffer_index;
  Bit32u   drq_index;
  Bit8u    current_command;
  Bit8u    sectors_per_block;
  Bit8u    lba_mode;

  struct  {
    rd_bool reset;       // 0=normal, 1=reset controller
    rd_bool disable_irq;     // 0=allow irq, 1=disable irq
  } control;

  Bit8u    reset_in_progress;
  Bit8u    features;
};




struct  drive_t {
  device_image_t  hard_drive;
  device_type_t device_type;
  // 512 byte buffer for ID drive command
  // These words are stored in native word endian format, as
  // they are fetched and returned via a return(), so
  // there's no need to keep them in x86 endian format.
  Bit16u id_drive[256];
  
  struct controller_t controller;
  struct cdrom_t cdrom;
  struct sense_info_t sense;
  struct atapi_t atapi;
  

  /* JRL */
  void * private_data;

  Bit8u model_no[41];
};


// FIXME:
// For each ATA channel we should have one controller struct
// and an array of two drive structs
struct  channel_t {
  struct drive_t drives[2];
  unsigned drive_select;
  
  Bit16u ioaddr1;
  Bit16u ioaddr2;
  Bit8u  irq;
};



struct  ramdisk_t {
  struct channel_t channels[MAX_ATA_CHANNEL];
};









#endif // ! __V3VEE__


#endif


#if 0

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


#endif
