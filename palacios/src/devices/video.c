/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Robert Deloatch <rtdeloatch@gmail.com>
 * Copyright (c) 2009, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Robdert Deloatch <rtdeloatch@gmail.com>
 *         Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <devices/video.h>
#include <palacios/vmm.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_instr_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_socket.h>
#include <palacios/vmm_host_events.h>
#include <devices/pci.h>
#include <devices/pci_types.h>



/* #ifndef DEBUG_VIDEO */
/* #undef PrintDebug */
/* #define PrintDebug(fmt, args...) */
/* #endif */


#define START_ADDR 0xA0000    //0xB0000   //Attempting to hook entire region
#define END_ADDR 0xC0000     //0xC0000//(START_ADDR + ROWS * COLS * DEPTH) 

#define SIZE_OF_REGION (END_ADDR-START_ADDR)

#define PCI_ENABLED 1

#define SEND_UPDATE_RAW       0
#define SEND_UPDATE_ALL_VT100 0
#define SEND_UPDATE_INCREMENTAL_VT100 1
#define SCROLLING_ENABLED 1
#define PASSTHROUGH 1
#define PORT 19997

#define NO_CODE 0
#define PORT_OFFSET 0x3B0

#define ESC_CHAR          ((unsigned char)0x1b)

struct key_code {
  char scan_code;
  uint_t capital;
};

#define NO_KEY { NO_CODE, 0 }

static const struct key_code ascii_to_key_code[] = {             // ASCII Value Serves as Index
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x00 - 0x03
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x04 - 0x07
    { 0x0E, 0 },    { 0x0F, 0 },    NO_KEY,         NO_KEY,      // 0x08 - 0x0B
    NO_KEY,         { 0x1C, 0 },    NO_KEY,         NO_KEY,      // 0x0C - 0x0F
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x10 - 0x13
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x14 - 0x17
    NO_KEY,         NO_KEY,         NO_KEY,         { 0x01, 0 }, // 0x18 - 0x1B
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x1C - 0x1F
    { 0x39, 0 },    { 0x02, 1 },    { 0x28, 1 },    { 0x04, 1 }, // 0x20 - 0x23
    { 0x05, 1 },    { 0x06, 1 },    { 0x08, 1 },    { 0x28, 0 }, // 0x24 - 0x27
    { 0x0A, 1 },    { 0x0B, 1 },    { 0x09, 1 },    { 0x0D, 1 }, // 0x28 - 0x2B
    { 0x33, 0 },    { 0x0C, 0 },    { 0x34, 0 },    { 0x35, 0 }, // 0x2C - 0x2F
    { 0x0B, 0 },    { 0x02, 0 },    { 0x03, 0 },    { 0x04, 0 }, // 0x30 - 0x33
    { 0x05, 0 },    { 0x06, 0 },    { 0x07, 0 },    { 0x08, 0 }, // 0x34 - 0x37
    { 0x09, 0 },    { 0x0A, 0 },    { 0x27, 1 },    { 0x27, 0 }, // 0x38 - 0x3B
    { 0x33, 1 },    { 0x0D, 0 },    { 0x34, 1 },    { 0x35, 1 }, // 0x3C - 0x3F
    { 0x03, 1 },    { 0x1E, 1 },    { 0x30, 1 },    { 0x2E, 1 }, // 0x40 - 0x43
    { 0x20, 1 },    { 0x12, 1 },    { 0x21, 1 },    { 0x22, 1 }, // 0x44 - 0x47
    { 0x23, 1 },    { 0x17, 1 },    { 0x24, 1 },    { 0x25, 1 }, // 0x48 - 0x4B
    { 0x26, 1 },    { 0x32, 1 },    { 0x31, 1 },    { 0x18, 1 }, // 0x4C - 0x4F
    { 0x19, 1 },    { 0x10, 1 },    { 0x13, 1 },    { 0x1F, 1 }, // 0x50 - 0x53
    { 0x14, 1 },    { 0x16, 1 },    { 0x2F, 1 },    { 0x11, 1 }, // 0x54 - 0x57
    { 0x2D, 1 },    { 0x15, 1 },    { 0x2C, 1 },    { 0x1A, 0 }, // 0x58 - 0x5B
    { 0x2B, 0 },    { 0x1B, 0 },    { 0x07, 1 },    { 0x0C, 1 }, // 0x5C - 0x5F
    { 0x29, 0 },    { 0x1E, 0 },    { 0x30, 0 },    { 0x2E, 0 }, // 0x60 - 0x63
    { 0x20, 0 },    { 0x12, 0 },    { 0x21, 0 },    { 0x22, 0 }, // 0x64 - 0x67
    { 0x23, 0 },    { 0x17, 0 },    { 0x24, 0 },    { 0x25, 0 }, // 0x68 - 0x6B
    { 0x26, 0 },    { 0x32, 0 },    { 0x31, 0 },    { 0x18, 0 }, // 0x6C - 0x6F
    { 0x19, 0 },    { 0x10, 0 },    { 0x13, 0 },    { 0x1F, 0 }, // 0x70 - 0x73
    { 0x14, 0 },    { 0x16, 0 },    { 0x2F, 0 },    { 0x11, 0 }, // 0x74 - 0x77
    { 0x2D, 0 },    { 0x15, 0 },    { 0x2C, 0 },    { 0x1A, 1 }, // 0x78 - 0x7B
    { 0x2B, 1 },    { 0x1B, 1 },    { 0x29, 1 },    { 0x0E, 0 }  // 0x7C - 0x7F
};

struct get_keys_arg {
  struct guest_info * info;
  int fd;
};

struct video_internal {
  addr_t video_memory_pa;
  uchar_t * video_memory;

#if PCI_ENABLED
  struct vm_device * pci_bus;
  struct pci_device * pci_dev;
#endif

  int socket_fd;
  int client_fd;

  struct get_keys_arg key_arg;

  uint_t screen_bottom;
  uint_t ports[44];
  uchar_t reg_3C4[0x100];
  uchar_t reg_3CE[0x100];
  ushort_t start_addr_offset;
  ushort_t old_start_addr_offset;
  ushort_t cursor_addr;
  
  uchar_t reg_3D5[0x19];

  uint_t high_addr;
  uint_t low_addr;
};

void video_do_in(ushort_t port, void *src, uint_t length){
   #if PASSTHROUGH
   uint_t i;
    
    switch (length) {
	case 1:
	    ((uchar_t*)src)[0] = v3_inb(port);
	    break;
	case 2:
	    ((ushort_t*)src)[0] = v3_inw(port);
	    break;
	case 4:
	    ((uint_t*)src)[0] = v3_indw(port);
	    break;
	default:
	    for (i = 0; i < length; i++) { 
		((uchar_t*)src)[i] = v3_inb(port);
	    }
    }//switch length
    #endif
}

void video_do_out(ushort_t port, void * src, uint_t length){
  #if PASSTHROUGH
  uint_t i;
  switch (length) {
	case 1:
	    v3_outb(port,((uchar_t*)src)[0]);
	    break;
	case 2:
	    v3_outw(port,((ushort_t*)src)[0]);
	    break;
	case 4:
	    v3_outdw(port,((uint_t*)src)[0]);
	    break;
	default:
	    for (i = 0; i < length; i++) { 
		v3_outb(port, ((uchar_t*)src)[i]);
	    }
    } //switch length
  #endif
}

static int deliver_scan_code(struct get_keys_arg * args, struct key_code * k) {
   uint_t cap = k->capital;
   struct v3_keyboard_event key_event;
   key_event.status = 0;
   key_event.scan_code = (unsigned char)k->scan_code;
   PrintDebug("Scan code: 0x%x\n", key_event.scan_code);
   struct v3_keyboard_event key_shift;
   key_shift.status = 0;
   key_shift.scan_code = (unsigned char)0x2A;
   if(cap) {
       if(v3_deliver_keyboard_event(args->info, &key_shift) == -1) {
           PrintError("Video: Error delivering key event\n");
	   return -1;
       }
   }
   // Press
   if(v3_deliver_keyboard_event(args->info, &key_event) == -1) {
       PrintError("Video: Error delivering key event\n");
       return -1;
   }
   // Release
   if(key_event.scan_code != 0x1c) {
       key_event.scan_code = key_event.scan_code|0x80;
       if(v3_deliver_keyboard_event(args->info, &key_event) == -1) {
           PrintError("Video: Error delivering key event\n");
	   return -1;
       }
    }
    if(cap) {
        key_shift.scan_code = 0x2A|0x80;
	if(v3_deliver_keyboard_event(args->info, &key_shift) == -1) {
	    PrintError("Video: Error delivering key event\n");
	    return -1;
	}
    }
    PrintDebug("Finished with Key delivery\n");
    return 0;
}

static int get_keys(void * arg) {
  struct get_keys_arg * args =  &(((struct video_internal *)arg)->key_arg);
  while(1) {
    PrintDebug("Get keys loop start\n");
    uint_t cap;
    char key[1];
    memset(key, 0, 1);

    int recv = V3_Recv(args->fd, key, 1);
    if(recv == -1){
      PrintError("Video: Error getting key from network\n");
    }
    else if(recv == 0){
      PrintDebug("Video: Client Disconnected (FD=%d)\n", args->fd);
      break;
    }

    uint_t i;
    for(i = 0; i < 1; i++) {
      cap = 0;
      uchar_t ascii = (uchar_t)key[i];
      
      if(ascii == 0) {
	PrintDebug("Recieved ASCII NULL character\n");
	break;
      }

      PrintDebug("Character recieved: 0x%x\n", ascii);
      if(ascii < 0x80) {
	key[i] = ascii_to_key_code[ascii].scan_code;
	cap = ascii_to_key_code[ascii].capital;
	if(key[i]) {
	  // Special Terminal Escape Sequences (UP, DOWN, LEFT, RIGHT) 
	  if(key[i] == 0x01) {
	    int j;
	    int esc = 0;
	    for(j = 0; j < 2; j++) {
	      int recv2 = V3_Recv(args->fd, key, 1);
	      if(recv2 == -1){
		PrintDebug("Video: Error getting key from network\n");
		break;
	      }
	      else if(recv2 == 0){
		PrintDebug("Video: Client Disconnected (FD=%d)\n", args->fd);
      		break;
    	      }
	      if(key[0] == '[') {
		esc = 1;
	      }
	      // UP ARROW
	      if(esc == 1 && key[0] == 'A') {
	        struct key_code up = { 0x48, 0 };
		deliver_scan_code(args, &up);
	      }
	      // DOWN ARROW
	      if(esc == 1 && key[0] == 'B') {
                struct key_code down = { 0x50, 0 };
		deliver_scan_code(args, &down);
	      }
	      // RIGHT ARROW
	      if(esc == 1 && key[0] == 'C') {
		struct key_code right = { 0x4D, 0 };
		deliver_scan_code(args, &right);
	      }
	      // LEFT ARROW
	      if(esc == 1 && key[0] == 'D') {
		struct key_code left = { 0x4B, 0 };
		deliver_scan_code(args, &left);
	      }
	    }
	    break;
	  }
	  else if(key[i] == 0x1D){
	    int recv2 = V3_Recv(args->fd, key, 1);
	    if(recv2 == -1){
	      PrintDebug("Video: Error getting key from network\n");
	    }
	    else if(recv2 == 0){
	      PrintDebug("Video: Client Disconnected (FD=%d)\n", args->fd);
	    }
	    break;
	  }
	  else {
            struct key_code k = { ascii_to_key_code[ascii].scan_code, ascii_to_key_code[ascii].capital };
	    deliver_scan_code(args, &k);
	  }
	}
	else {
	  break;
	}
      }
    }
  }
  return 0;
}

#if SEND_UPDATE_INCREMENTAL_VT100
static int send_all(struct video_internal * data, const int sock, const char * buf, const int len){
  int bytes_left = len;
  while(bytes_left != 0) {
    int written = 0;
    if((written = V3_Send(sock, buf + (len-bytes_left), bytes_left)) == -1) {
      return -1;
    }
    bytes_left -= written;
  }
  return 0;
}
#endif

// Translate attribute color into terminal escape sequence color
static unsigned char text_mode_fg_color[] = {
  30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97
};

static unsigned char text_mode_bg_color[] = {
  40, 44, 42, 46, 41, 45, 43, 47, 100, 104, 102, 106, 101, 105, 103, 107
};

#if SEND_UPDATE_INCREMENTAL_VT100
static unsigned char * append_char_as_digits(unsigned char *s, unsigned char c)
{
  if (c/100) { 
    *s++ = '0'+ c/100;
  }
  c %= 100;
  if (c/10) { 
    *s++= '0' + c/10;
  }
  c %= 10;
  *s++ = '0'+ c;
  return s;
}


static unsigned char * append_write_attribute_at(unsigned char *s, unsigned char  x, unsigned char  y, unsigned char c) 
{
  // Update cursor
  *s++ = ESC_CHAR;
  *s++ = '[';
  s = append_char_as_digits(s,y+1);
  *s++ = ';';
  s = append_char_as_digits(s,x+1);
  *s++ = 'H';
  
  *s++ = ESC_CHAR;
  *s++ = '[';
  *s++ = '0';
  *s++ = 'm';

  // Update attribute
  unsigned char fg_color = 0;
  unsigned char bg_color = 0;

  *s++ = ESC_CHAR;
  *s++ = '[';
  unsigned char low = c & 0x0F;
  unsigned char high = (c & 0xF0) >> 4;
  low = low % 16;
  fg_color = text_mode_fg_color[low];
  s = append_char_as_digits(s,fg_color);
  *s++ = '\x3b';
  high = high % 16;
  bg_color = text_mode_bg_color[high];
  s = append_char_as_digits(s,bg_color);
  *s++ = 'm';
  return s;
}

static unsigned char * append_write_char_at(unsigned char *s, unsigned char x, unsigned char y, unsigned char c) {
  *s++ = ESC_CHAR;
  *s++ = '[';
  s = append_char_as_digits(s,y+1);
  *s++ = ';';
  s = append_char_as_digits(s,x+1);
  *s++ = 'H';
  *s++ = c;
  return s;
}
#endif
static int video_write_mem(addr_t guest_addr, void * dest, uint_t length, void * priv_data) {
  PrintDebug("\n\nInside Video Write Memory.\n\n");
  PrintDebug("Guest address: 0x%x length = %d\n", (uint_t)guest_addr, length);

  struct vm_device * dev = (struct vm_device *) priv_data;
  struct video_internal * data = (struct video_internal *) dev->private_data;
  addr_t write_offset = guest_addr - START_ADDR;

#if 1  
  PrintDebug("Write offset: 0x%p\n", (void*)write_offset);
  
  {
    int i;
    PrintDebug("Video_Memory: ");
    for(i = 0; i < length; i += 2){
      PrintDebug("%c",((char *)(V3_VAddr((void*)guest_addr)))[i]);
    }
  }
#endif
 
  memcpy(data->video_memory + write_offset, V3_VAddr((void*)guest_addr), length);

#if SEND_UPDATE_RAW
  send_all(data, data->client_fd, (char *)(data->video_memory + write_offset), length);
#endif

#if SEND_UPDATE_INCREMENTAL_VT100
 
  unsigned char buf[32];
  unsigned char *n;
  unsigned char *a;
  uint_t difference = 0x18000;


  unsigned char x,y,c;
  uint_t       offset;
  uint_t l = length;

  for (offset=0;offset<l;offset+=2){
    memset(buf, 0, 32);
    y = ((write_offset - difference - (data->start_addr_offset *2) + offset)/160) ;
    if(y < 25){
      //      PrintDebug("  Video: Y value = %d  ", y); 
      x = ((write_offset - difference + offset)%160)/2;
      c = (data->video_memory + difference + data->start_addr_offset * 2)[y*160+x*2+1];
      a = append_write_attribute_at(buf,x,y,c);
      send_all(data,data->client_fd, (char*)buf, 32);
      memset(buf, 0, 32);
      c = (data->video_memory + difference + data->start_addr_offset * 2)[y*160+x*2];
      n = append_write_char_at(buf,x,y,c);
      send_all(data,data->client_fd, (char *)buf, 32);
    }
  }

#endif

#if SEND_UPDATE_ALL_VT100
 
  unsigned char buf[16];
  unsigned char *n;

  unsigned char x,y,c;
  
  for (y=0;y<25;y++) { 
    for (x=0;x<80;x++) { 
      c = data->video_memory[y*160+x*2];
      n = append_write_char_at(buf,x,y,c);
      send_all(data,data->client_fd, (char *)buf, n-buf);
    }
  }
#endif
  PrintDebug(" Done.\n"); 
  return length;
}

static int video_read_port(ushort_t port,
				 void * dest,
				 uint_t length,
				 struct vm_device * dev) {
  PrintDebug("Video: Read port 0x%x\n",port);
  video_do_in(port, dest, length);
  return length;
}

static int video_read_port_generic(ushort_t port,
				 void * dest,
				 uint_t length,
				 struct vm_device * dev) {
  memset(dest, 0, length);
  video_do_in(port, dest, length);
  return length;
}


static int video_write_port(ushort_t port,
				 void * src,
				 uint_t length,
				 struct vm_device * dev) {

  PrintDebug("Video: write port 0x%x...Wrote: ", port);
  uint_t i;
  for(i = 0; i < length; i++){
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  PrintDebug("...Done\n");
  video_do_out(port, src, length);
  return length;
}

static int video_write_port_store(ushort_t port,
				 void * src,
				 uint_t length,
				 struct vm_device * dev) {

  PrintDebug("Entering video_write_port_store...port 0x%x\n", port);
  uint_t i;
  for(i = 0; i < length; i++){
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  PrintDebug("...Done\n"); 

  struct video_internal * video_state = (struct video_internal *)dev->private_data;
  video_state->ports[port - PORT_OFFSET] = 0;
  memcpy(video_state->ports + (port - PORT_OFFSET), src, length); 
  video_do_out(port, src, length);
  return length;
}

static int video_write_port_3D5(ushort_t port,
				 void * src,
				 uint_t length,
				 struct vm_device * dev) {
  PrintDebug("Video: write port 0x%x...Wrote: ", port);
  uint_t i;
  for(i = 0; i < length; i++){
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  PrintDebug("...Done\n");
  struct video_internal * video_state = (struct video_internal *)dev->private_data;
  video_state->ports[port - PORT_OFFSET] = 0;
  memcpy(video_state->ports + (port - PORT_OFFSET), src, length);

  uint_t index = video_state->ports[port - 1 - PORT_OFFSET];
  memcpy(&(video_state->reg_3D5[index]), src, length);
  uchar_t new_start;
  switch(index) {
     case 0x0c:
         video_state->old_start_addr_offset = video_state->start_addr_offset;
	 new_start = *((uchar_t *)src);
	 video_state->start_addr_offset = (new_start << 8);
         break;
     case 0x0d:
         new_start = *((uchar_t *)src);
	 video_state->start_addr_offset += new_start;
	 int diff =  video_state->start_addr_offset - video_state->old_start_addr_offset;
	 if(diff > 0) {
	     for(diff /= 80; diff > 0; diff--){
	         unsigned char message[2];
	         memset(message, 0, 2);
	         message[0] = ESC_CHAR;
	         message[1] = 'D';
	         send_all(video_state,video_state->client_fd, (char *)message, 2);
	     }
	 }
	 // Scroll Up?
         break;
     case 0x0E:
       new_start = *((uchar_t *)src);
       video_state->cursor_addr = new_start << 8;
       break;
     case 0x0F:
       new_start = *((uchar_t *)src);
       video_state->cursor_addr += new_start;
       // Update cursor
       unsigned char s[16];
       memset(s, 0, 16);
       unsigned char *n = s;
       *n++ = ESC_CHAR;
       *n++ = '[';
       n = append_char_as_digits(n,(((video_state->cursor_addr)/80))+1);
       *n++ = ';';
       n = append_char_as_digits(n,((video_state->cursor_addr)% 80)+1);
       *n++ = 'H';
       send_all(video_state, video_state->client_fd, (char *)s, 16);
       break;
     default:
         break;
  }
  video_do_out(port, src, length);
  return length;
}


static int video_write_port_3C5(ushort_t port,
				 void * src,
				 uint_t length,
				struct vm_device * dev) {
  PrintDebug("Entering write_port_3C5....port 0x%x\n", port);
  uint_t i;
  for(i = 0; i < length; i++){
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  PrintDebug("...Done\n");

  struct video_internal * video_state = (struct video_internal *)dev->private_data;
  video_state->ports[port - PORT_OFFSET] = 0;
  memcpy(video_state->ports + (port - PORT_OFFSET), src, length); 

  uint_t index = video_state->ports[port - 1 - PORT_OFFSET];
  memcpy(&(video_state->reg_3C4[index]), src, length);
  video_do_out(port, src, length);
  return length;
}

static int video_write_port_3CF(ushort_t port,
				 void * src,
				 uint_t length,
				 struct vm_device * dev) {
  PrintDebug("Entering write_port_3CF....port 0x%x\n", port);
  uint_t i;
  for(i = 0; i < length; i++){
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  PrintDebug("...Done\n");

  struct video_internal * video_state = (struct video_internal *)dev->private_data;
  video_state->ports[port - PORT_OFFSET] = 0;
  memcpy(video_state->ports + (port - PORT_OFFSET), src, length); 

  uint_t index = video_state->ports[port - 1 - PORT_OFFSET];
  memcpy(&(video_state->reg_3CE[index]), src, length);
  video_do_out(port, src, length);
  return length;
}

static int video_write_port_3D4(ushort_t port, void *src, uint_t length,
				struct vm_device *dev){
  struct video_internal *video_state = (struct video_internal *) dev -> private_data;

#if 1

  if(length == 1) {
    video_state->ports[port - PORT_OFFSET] = 0;
    memcpy(video_state->ports + (port - PORT_OFFSET), src, length); 
  }
  else if(length == 2){
    ushort_t new_start = 0;
    ushort_t cursor_start = 0;
    new_start = *((ushort_t *)src);
    cursor_start = *((ushort_t *)src);

    //Updating the cursor
    if((cursor_start & 0x00FF) == 0x000E){
      PrintDebug("Video:  E Case cursor start = 0x%x\n", cursor_start);
      cursor_start = (cursor_start & 0xFF00);  //0x70
      PrintDebug("Video: cursor start after and = 0x%x\n", cursor_start);
      video_state->cursor_addr = cursor_start;
    }
    else if((cursor_start & 0x00FF) == 0x000F){
      PrintDebug("Video:  F Case cursor start = 0x%x\n", cursor_start);
      PrintDebug("Cursor Address after: 0x%x\n", video_state->cursor_addr);
      video_state->cursor_addr += ((cursor_start >> 8) & 0x00FF);
      // Update cursor
      unsigned char s[16];
      memset(s, 0, 16);
      unsigned char *n = s;
      *n++ = ESC_CHAR;
      *n++ = '[';
      n = append_char_as_digits(n,(((video_state->cursor_addr)/80))+1);
      *n++ = ';';
      n = append_char_as_digits(n,((video_state->cursor_addr)% 80)+1);
      *n++ = 'H';
      PrintDebug("Cursor Address: 0x%x\n", video_state->cursor_addr);
      PrintDebug("Cursor Y location should be:  %d\n", ((video_state->cursor_addr)/80)+1);
      PrintDebug("Cursor X location should be:  %d\n",((video_state->cursor_addr % 80)/2)+1);
      send_all(video_state, video_state->client_fd, (char *)s, 16);
    }
    //Checking to see if scrolling is needed
    if((new_start & 0x00FF) == 0x000C){
      video_state->old_start_addr_offset = video_state->start_addr_offset;
      new_start = (new_start & 0xFF00);
      video_state->start_addr_offset = new_start;
    }
    else if((new_start & 0x00FF) == 0x000D){
      video_state->start_addr_offset += ((new_start >> 8) & 0x00FF);
      int diff =  video_state->start_addr_offset - video_state->old_start_addr_offset;
      if(diff > 0){
#if 1
	for(diff /= 80; diff > 0; diff--){
	  unsigned char message[2];
	  memset(message, 0, 2);
	  message[0] = ESC_CHAR;
	  message[1] = 'D';
	  send_all(video_state,video_state->client_fd, (char *)message, 2);
	}
#endif
      }
      //Still need to handle scroll up
    }
  }
#endif 
  video_do_out(port, src, length);
  return length;
}



#if PCI_ENABLED
static int video_write_mem_region(addr_t guest_addr, void * src, uint_t length, void * priv_data){
 
  struct vm_device * dev = (struct vm_device *) priv_data;
  struct video_internal * data = (struct video_internal *) dev->private_data;
 
  if(data->low_addr == 0){
    data->low_addr = guest_addr;
    //memset(V3_VAddr((void*)0xA0000),0,0x10000);
    memset(V3_VAddr((void *)0xA000000),0, 0x4affc);
    memset(V3_VAddr((void *)0xF0000000),0, 0x4affc);
  }

  if(guest_addr > data->high_addr){
    data->high_addr = guest_addr;
  }
  if(guest_addr < data->low_addr){
    data->low_addr = guest_addr;
  }
  

  PrintDebug("Video:  Within video_write_mem_region\n");

  PrintDebug("Length of write: %d\n", length);
  PrintDebug("Guest Address of write: 0x%p\n", (void *)guest_addr);
  //PrintDebug("\"Src Address\": 0x%p\n", (void *)src);


  //PrintDebug("Write Mem Value: ");
  // PrintDebug("0x%x",*((uint32_t *)(V3_VAddr((void*)guest_addr))));
  // PrintDebug("0x%x",*((uint32_t *)src));
  /* 
  unsigned char message[4];
  memset(message, 0, 4);
  message[0] = (*((char *)(V3_VAddr((void*)guest_addr))));
  message[1] = (*((char *)(V3_VAddr((void*)guest_addr) + 1)));
  message[2] = (*((char *)(V3_VAddr((void*)guest_addr) + 2)));
  message[3] = (*((char *)(V3_VAddr((void*)guest_addr) + 3)));
  PrintDebug("Character Value: %s\n", message);
  //send_all(data,data->client_fd, (char *)message, 4);
  */

  PrintDebug("\nLowest address written to...");
  PrintDebug("0x%x",data->low_addr);

  PrintDebug("\nHighest address written to...");
  PrintDebug("0x%x\n",data->high_addr);

  // memcpy(V3_VAddr((void*)0xF0000000), V3_VAddr((void*)guest_addr) , length);
  //addr_t write_offset = guest_addr - 0xA000000;// % 0x20000; // (guest_addr - 0xA000000);// % 0x20000
  // PrintDebug("0x%x",*((uint32_t *)(src + )));
  //memcpy(V3_VAddr((void *)(0xF0000000 + write_offset)), src, length);
  /*If(write_offset < 0x10000) {
    memcpy(V3_VAddr((void *)(START_ADDR + write_offset)), V3_VAddr((void*)guest_addr) , length);
  }
  else {
    memset(V3_VAddr((void*)0xA0000),0,0x10000);
    }*/
  // memset(V3_VAddr((void *)(0xF0000000)), 5, 0x4affc);
  //memset(V3_VAddr((void *)(0xF004affa)), 7, 0xF);

  //Int i;
  //PrintDebug("Write Mem Value: ");
  //for(i = 0; i < length; i++){
  //  PrintDebug("%x",((char *)(V3_VAddr((void*)guest_addr)))[i]);
  //}

  PrintDebug("\n...Done\n");

  return length;
}

static int video_read_mem_region(addr_t guest_addr, void * dest, uint_t length, void * priv_data){
  PrintDebug("Video:  Within video_read_mem_region\n");
  return length;
}

static int video_write_io_region(addr_t guest_addr, void * src, uint_t length, void * priv_data){
  PrintDebug("Video:  Within video_write_io_region\n");
  return length;
}

static int video_read_io_region(addr_t guest_addr, void * dest, uint_t length, void * priv_data){
  PrintDebug("Video:  Within video_read_io_region\n");
  return length;
}
#endif

static int video_free(struct vm_device * dev) {
  v3_unhook_mem(dev->vm, START_ADDR);
  return 0;
}

static int video_reset_device(struct vm_device * dev) {
  PrintDebug("Video: reset device\n");
  return 0;
}

static int video_start_device(struct vm_device * dev) {
  PrintDebug("Video: start device\n");
  return 0;
}

static int video_stop_device(struct vm_device * dev) {
  PrintDebug("Video: stop device\n");
  return 0;
}

static struct v3_device_ops dev_ops = {
  .free = video_free,
  .reset = video_reset_device,
  .start = video_start_device,
  .stop = video_stop_device,
};

static int video_init(struct guest_info * vm, void * cfg_data){
  PrintDebug("video: init_device\n");
struct video_internal * video_state = (struct video_internal *)V3_Malloc(sizeof(struct video_internal));

 struct vm_device * dev = v3_allocate_device("VIDEO", &dev_ops, video_state);

   if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "VIDEO");
	return -1;
    }

  PrintDebug("Num Pages=%d\n", SIZE_OF_REGION / 4096);
  video_state->video_memory_pa = (addr_t)V3_AllocPages(SIZE_OF_REGION / 4096);
  video_state->video_memory = V3_VAddr((void*)video_state->video_memory_pa);

  memset(video_state->video_memory, 0, SIZE_OF_REGION);
  PrintDebug("Video: hook io ports\n");
#if 1
  v3_dev_hook_io(dev, 0x3b0, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b1, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b2, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b3, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b4, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b5, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b6, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b7, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b8, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3b9, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3ba, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3bb, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3c0, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3c1, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3c2, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3c3, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3c4, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3c5, &video_read_port, &video_write_port_3C5);
  v3_dev_hook_io(dev, 0x3c6, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3c7, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3c8, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3c9, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3ca, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3cb, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3cc, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3cd, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3ce, &video_read_port, &video_write_port_store);
  v3_dev_hook_io(dev, 0x3cf, &video_read_port, &video_write_port_3CF);
  v3_dev_hook_io(dev, 0x3d0, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d1, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d2, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d3, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d4, &video_read_port, &video_write_port_3D4);
  v3_dev_hook_io(dev, 0x3d5, &video_read_port, &video_write_port_3D5);
  v3_dev_hook_io(dev, 0x3d6, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d7, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d8, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3d9, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3da, &video_read_port_generic, &video_write_port);
  v3_dev_hook_io(dev, 0x3db, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3dc, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3dd, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3de, &video_read_port, &video_write_port);
  v3_dev_hook_io(dev, 0x3df, &video_read_port, &video_write_port);
#endif
   PrintDebug("Video: End i/o hooks\n");
   struct guest_info * info = vm;
   PrintDebug("PA of array: %p\n", (void*)video_state->video_memory_pa);

   if(v3_hook_write_mem(info, START_ADDR, END_ADDR, START_ADDR, &video_write_mem, dev) == -1){
      PrintDebug("\n\nVideo Hook failed.\n\n");
    }
   print_shadow_map(info);


  // Testing out network stuff
  video_state->socket_fd = V3_Create_TCP_Socket();
  PrintDebug("Video: Socket File Descriptor: %d\n", video_state->socket_fd);

#if 1

  if(V3_Bind_Socket(video_state->socket_fd, PORT) == -1) {
    PrintError("Video: Failed to bind to socket %d\n", PORT);
  }
  if(V3_Listen_Socket(video_state->socket_fd, 10) == -1) {
    PrintError("Video: Failed to listen with socket %d\n", video_state->socket_fd);
  }
  const char * ip_string = "10.10.10.138";
  PrintDebug("Video: IP Address in string form: %s\n", ip_string);
  uint32_t ip_h = v3_inet_addr(ip_string);
  PrintDebug("Video: IP Address in host integer form: %x\n", ip_h);
  uint32_t ip_n = v3_htonl(ip_h);
  PrintDebug("Video: IP Address in network integer form %x\n", ip_n);
  uint32_t port = PORT;
  unsigned int ip_ptr = (ip_n);
  unsigned int port_ptr = (port);
  if((video_state->client_fd = V3_Accept_Socket(video_state->socket_fd, &ip_ptr, &port_ptr)) == -1) {
    PrintDebug("Video: Failed to accept connection on port %d", PORT);
  }
  PrintDebug("Video: Accept worked? FD= %d\n", video_state->client_fd);
#endif

  video_state->key_arg.info = info;
  video_state->key_arg.fd = video_state->client_fd;
  V3_CREATE_THREAD(get_keys,(void *)video_state, "Get Keys from Network");

  video_state->screen_bottom = 25;
  PrintDebug("video: init complete\n");



#if PCI_ENABLED
  if(video_state->pci_bus){
    struct v3_pci_bar bars[6];
    struct pci_device * pci_dev = NULL;

    int i;
    for(i = 0; i < 6; i++){
      bars[i].type = PCI_BAR_NONE;
    }

    bars[0].type = PCI_BAR_MEM32;
    bars[0].num_pages = /*0x20000/0x1000;*/ 0x2000000/0x1000;
    bars[0].default_base_addr = (addr_t)V3_VAddr((void *)0xF0000000);//(addr_t)V3_VAddr(V3_AllocPages(bars[0].num_pages));

    bars[0].mem_read = video_read_mem_region;
    bars[0].mem_write = video_write_mem_region;

    bars[1].type = PCI_BAR_MEM32;
    bars[1].num_pages = 1;
    bars[1].default_base_addr = (addr_t)V3_VAddr(V3_AllocPages(bars[1].num_pages));
    
    bars[1].mem_read = video_read_io_region;
    bars[1].mem_write = video_write_io_region;
    //-1 Means autoassign
    //                                                     Not sure if STD
    pci_dev = v3_pci_register_device(video_state->pci_bus, PCI_STD_DEVICE, 0,
				     //or0  1st null could be pci_config_update
				     -1, 0, "VIDEO", bars, NULL, NULL,
				     NULL, dev);

    if (pci_dev == NULL) {
	PrintError("Failed to register VIDEO %d with PCI\n", i);
	return -1;
    }
    else{
      PrintDebug("Registering PCI_VIDEO succeeded\n");
    }
    //Need to set some pci_dev->config_header.vendor_id type variables

    pci_dev->config_header.vendor_id = 0x1013;
    pci_dev->config_header.device_id = 0x00B8;
    pci_dev->config_header.revision = 0x00;

    //If we treat video as a VGA device than below is correct
    //If treated as a VGA compatible controller, which has mapping
    //0xA0000-0xB0000 and I/O addresses 0x3B0-0x3BB than change
    //#define from VGA to 0

    //pci_dev->config_header.class = 0x00;
    //pci_dev->config_header.subclass = 0x01;

    pci_dev->config_header.class = 0x03;
    pci_dev->config_header.subclass = 0x00;
    pci_dev->config_header.prog_if = 0x00;


    //We have a subsystem ID, but optional to provide:  1AF4:1100
    pci_dev->config_header.subsystem_vendor_id = 0x1AF4;
    pci_dev->config_header.subsystem_id = 0x1100;
    //pci_dev->config_header.header_type = 0x00;
    pci_dev->config_header.command = 0x03;
    video_state->pci_dev = pci_dev;
  }
#endif
  return 0;
}

device_register("VIDEO", video_init)


/*
#if PCI_ENABLED
struct vm_device * v3_create_video(struct vm_device * pci_bus) {

#else
struct vm_device * v3_create_video() {
#endif
  PrintDebug("Video Print Debug in create\n");

  PrintDebug("Video, before initial \n");

  struct video_internal * video_state = NULL;

  video_state = (struct video_internal *)V3_Malloc(sizeof(struct video_internal));

  PrintDebug("video: internal at %p\n", (void *)video_state);

 struct vm_device * device = v3_create_device("VIDEO", &dev_ops, video_state);

#if PCI_ENABLED
  if(pci_bus != NULL){
    video_state->pci_bus = pci_bus;
  }
#endif

 return device;
}
*/

