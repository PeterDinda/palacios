/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_io.h>

#include "vga_regs.h"

#define MEM_REGION_START 0xa0000
#define MEM_REGION_END   0xc0000
#define MEM_REGION_NUM_PAGES (((MEM_REGION_END)-(MEM_REGION_START))/4096)

#define MAP_SIZE 65536
#define MAP_NUM  4

typedef uint8_t *vga_map; // points to MAP_SIZE data


#define VGA_MISC_OUT_READ 0x3cc
#define VGA_MISC_OUT_WRITE 0x3c2

#define VGA_INPUT_STAT0_READ 0x3c2

#define VGA_INPUT_STAT1_READ_MONO 0x3ba
#define VGA_INPUT_STAT1_READ_COLOR 0x3da

#define VGA_FEATURE_CONTROL_READ 0x3ca
#define VGA_FEATURE_CONTROL_WRITE_MONO 0x3ba
#define VGA_FEATURE_CONTROL_WRITE_COLOR 0x3da

#define VGA_VIDEO_SUBSYS_ENABLE 0x3c3

#define VGA_SEQUENCER_ADDRESS 0x3c4
#define VGA_SEQUENCER_DATA 0x3c5
#define VGA_SEQUENCER_NUM 5


#define VGA_CRT_CONTROLLER_ADDRESS_MONO 0x3b4
#define VGA_CRT_CONTROLLER_ADDRESS_COLOR 0x3d4
#define VGA_CRT_CONTROLLER_DATA_MONO 0x3b5
#define VGA_CRT_CONTROLLER_DATA_COLOR 0x3d5
#define VGA_CRT_CONTROLLER_NUM 25


#define VGA_GRAPHICS_CONTROLLER_ADDRESS 0x3ce
#define VGA_GRAPHICS_CONTROLLER_DATA 0x3cf
#define VGA_GRAPHICS_CONTROLLER_NUM 9

#define VGA_ATTRIBUTE_CONTROLLER_ADDRESS_AND_WRITE 0x3c0
#define VGA_ATTRIBUTE_CONTROLLER_READ 0x3c1
#define VGA_ATTRIBUTE_CONTROLLER_NUM 21

#define VGA_DAC_WRITE_ADDR 0x3c8
#define VGA_DAC_READ_ADDR 0x3c7
#define VGA_DAC_DATA 0x3c8
#define VGA_DAC_PIXEL_MASK 0x3c6

#define VGA_DAC_NUM_ENTRIES 256

struct vga_misc_regs {
    /* Read: 0x3cc; Write: 0x3c2 */
    struct vga_misc_out_reg        vga_misc_out;
    /* Read: 0x3c2 */
    struct vga_input_stat0_reg     vga_input_stat0;
    /* Read: 0x3?a  3ba for mono; 3da for cga set by misc.io_addr_sel */
    struct vga_input_stat1_reg     vga_input_stat1; 
    /* Read: 0x3ca; Write: 0x3?a 3ba for mono 3da for color - set by misc.io_addr_sel*/
    struct vga_feature_control_reg vga_feature_control;
    /* Read: 0x3c3; Write: 0x3c3 */
    struct vga_video_subsys_enable_reg vga_video_subsys_enable;
} __attribute__((packed));

struct vga_sequencer_regs {
    /*   Address register is 0x3c4, data register is 0x3c5 */
    /* 0x3c4 */
    struct vga_sequencer_addr_reg vga_sequencer_addr;

    /* these can be accessed via the index, offset on start 
       or via the specific regs.   For this reason, it is essential
       that this is all packed and that the order does not change */
    
    uint8_t  vga_sequencer_regs[0];

    /* Index 0 */
    struct vga_reset_reg   vga_reset;
    /* Index 1 */
    struct vga_clocking_mode_reg vga_clocking_mode;
    /* Index 2 */
    struct vga_map_mask_reg vga_map_mask;
    /* Index 3 */
    struct vga_char_map_select_reg vga_char_map_select;
    /* Index 4 */
    struct vga_mem_mode_reg  vga_mem_mode;
} __attribute__((packed));

struct vga_crt_controller_regs {
    /* Address Register is 0x3b4 or 0x3d4 */
    /* Data register is 0x3b5 or 0x3d5 based on mono versus color */
    struct vga_crt_addr_reg vga_crt_addr;

    /* these can be accessed via the index, offset on start 
       or via the specific regs.   For this reason, it is essential
       that this is all packed and that the order does not change */
    
    uint8_t  vga_crt_controller_regs[0];

    /* index 0 */
    vga_horizontal_total_reg vga_horizontal_total;
    /* index 1 */
    vga_horizontal_display_enable_end_reg vga_horizontal_display_enable_end;
    /* index 2 */
    vga_start_horizontal_blanking_reg vga_start_horizontal_blanking;
    /* index 3 */
    struct vga_end_horizontal_blanking_reg vga_end_horizontal_blanking;
    /* index 4 */
    vga_start_horizontal_retrace_pulse_reg vga_start_horizontal_retrace_pulse;
    /* index 5 */
    struct vga_end_horizontal_retrace_reg vga_end_horizontal_retrace;
    /* index 6 */
    vga_vertical_total_reg vga_vertical_total;
    /* index 7 */
    struct vga_overflow_reg vga_overflow;
    /* index 8 */
    struct vga_preset_row_scan_reg vga_preset_row_scan;
    /* index 9 */
    struct vga_max_row_scan_reg vga_row_scan;
    /* index 10 */
    struct vga_cursor_start_reg vga_cursor_start;
    /* index 11 */
    struct vga_cursor_end_reg vga_cursor_end;
    /* index 12 */
    vga_start_address_high_reg vga_start_address_high;
    /* index 13 */
    vga_start_address_low_reg vga_start_address_low;
    /* index 14 */
    vga_cursor_location_high_reg vga_cursor_location_high;
    /* index 15 */
    vga_cursor_location_low_reg vga_cursor_location_low;
    /* index 16 */
    vga_vertical_retrace_start_reg vga_vertical_retrace_start;
    /* index 17 */
    struct vga_vertical_retrace_end_reg vga_vertical_retrace_end;
    /* index 18 */
    vga_vertical_display_enable_end_reg vga_vertical_display_enable;
    /* index 19 */
    vga_offset_reg vga_offset;
    /* index 20 */
    struct vga_underline_location_reg vga_underline_location;
    /* index 21 */
    vga_start_vertical_blanking_reg vga_start_vertical_blanking;
    /* index 22 */
    vga_end_vertical_blanking_reg vga_end_vertical_blanking;
    /* index 23 */
    struct vga_crt_mode_control_reg vga_crt_mode_control;
    /* index 24 */
    vga_line_compare_reg vga_line_compare;
} __attribute__((packed));

struct vga_graphics_controller_regs {
    /*   Address: 0x3ce    Data: 0x3cf */

    /* 0x3ce */
    struct vga_graphics_ctrl_addr_reg vga_graphics_ctrl_addr;

    /* these can be accessed via the index, offset on start 
       or via the specific regs.   For this reason, it is essential
       that this is all packed and that the order does not change */
    
    uint8_t  vga_graphics_controller_regs[0];

    /* Index 0 */
    struct vga_set_reset_reg vga_set_reset;
    /* Index 1 */
    struct vga_enable_set_reset_reg vga_enable_set_reset;
    /* Index 2 */
    struct vga_color_compare_reg vga_color_compare;
    /* Index 3 */
    struct vga_data_rotate_reg vga_data_rotate;
    /* Index 4 */
    struct vga_read_map_select_reg vga_read_map_select;
    /* Index 5 */
    struct vga_graphics_mode_reg vga_graphics_mode;
    /* Index 6 */
    struct vga_misc_reg vga_misc;
    /* Index 7 */
    struct vga_color_dont_care__reg vga_color_dont_care;
    /* Index 8 */
    vga_bit_mask_reg vga_bit_mask;
} __attribute__((packed));


struct vga_attribute_contoller_regs {
    /*
      Address AND WRITE: 0x3c0
      Read: 0x3c1

      The write protocol is to write the index to 0x3c0 followed by 
      the data.  The read protocol is to write the index to 0x3c0
      and then read from 0x3c1
  
      IMPORTANT: write address, write data flips state back to write address
      write address, read data DOES NOT

      To reset to write address state, read input status register 1
*/
    enum { ATTR_ADDR, ATTR_DATA }  state;  //state of the flip flop

    /* 0x3c0 */
    struct vga_attribute_controller_address_reg vga_attribute_controller_addr;


    
    /* these can be accessed via the index, offset on start 
       or via the specific regs.   For this reason, it is essential
       that this is all packed and that the order does not change */
    
    uint8_t  vga_attribute_controller_regs[0];

    /* Indices 0..15 */
    vga_internal_palette_regs   vga_internal_palette;
    /* Index 16 */
    struct vga_attribute_mode_control_reg vga_attribute_mode_control;
    /* Index 17 */
    vga_overscan_color_reg vga_overscan_color;
    /* Index 18 */
    struct vga_color_plane_enable_reg vga_color_plane_enable;
    /* Index 19 */
    struct vga_horizontal_pixel_pan_reg vga_horizontal_pixel_pan;
    /* Index 20 */
    struct vga_color_select_reg vga_color_select;
} __attribute__((packed));

struct vga_dac_regs {
    enum {DAC_READ=0, DAC_WRITE} state;
    enum {RED=0,GREEN,BLUE} channel;
    vga_dac_pixel_mask_reg vga_pixel_mask;
    vga_dac_write_addr_reg vga_dac_write_addr;
    vga_dac_read_addr_reg vga_dac_read_addr;
    // the dac_data register is used only to access the registers
    // and thus has no representation here
    vga_palette_reg vga_dac_palette[VGA_DAC_NUM_ENTRIES];
} __attribute__((packed));
    

struct vga_internal {
    struct vm_device *dev;  

    struct frame_buf *framebuf; // we render to this
    
    //    void *mem_store;     // This is the region where the memory hooks will go

    vga_map  map[MAP_NUM];  // the maps that the host writes to

    /* Range of I/O ports here for backward compat with MDA and CGA */
    struct vga_misc_regs  vga_misc;

    /* Address Register is 0x3b4 or 0x3d4 */
    /* Data register is 0x3b5 or 0x3d5 based on MDA/CGA/VGA (backward compat) */
    struct vga_crt_controller_regs vga_crt_controller;

    /*   Address register is 0x3c4, data register is 0x3c5 */
    struct vga_sequencer_regs vga_sequencer;

    /*   Address: 0x3ce    Data: 0x3cf */
    struct vga_graphics_controller_regs vga_graphics_controller;

    /*
      Address AND WRITE: 0x3c0
      Read: 0x3c1
      Flip-Flop
    */
    struct vga_attribute_contoller_regs vga_attribute_controller;

    /*
      address for reads: 0x3c7 (also resets state machine for access to 18 bit regs 
      address for writes: 0x3c8 ("")
      data: 0x3c9
      pixel mask: 0x3c6 - do not write (init to 0xff)
    */
    struct vga_dac_regs vga_dac;
};



static int vga_write(struct guest_info * core, 
		     addr_t guest_addr, 
		     void * src, 
		     uint_t length, 
		     void * priv_data)
{


    PrintError("vga: vga_write UNIMPLEMENTED\n");

/*
up to 256K mapped through a window of 128K

most cards support linear mode as well

Need to implement readability too

Write extended memory bit to enable all 256K: 

   Sequencer Memory Mode Register (Index 04h) . extended memory

Must enable writes before effects happen:
  
  Miscellaneous Output Register (Read at 3CCh, Write at 3C2h).ram enable

Choose which addresses are supported for CPU writes:

Miscellaneous Graphics Register (Index 06h).memory map select
 
00b -- A0000h-BFFFFh (128K region)
01b -- A0000h-AFFFFh (64K region)
10b -- B0000h-B7FFFh (32K region)
11b -- B8000h-BFFFFh (32K region)

There are three addressing modes: Chain 4, Odd/Even mode, and normal mode:

Chain 4: This mode is used for MCGA emulation in the 320x200 256-color mode. The address is mapped to memory MOD 4 (shifted right 2 places.)

Memory model: 64K 32 bit locations; divided into 4 64K bit planes

Write Modes - set via Graphics Mode Register (Index 05h).writemode

00b -- Write Mode 0: In this mode, the host data is first rotated as per the Rotate Count field, then the Enable Set/Reset mechanism selects data from this or the Set/Reset field. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory.

01b -- Write Mode 1: In this mode, data is transferred directly from the 32 bit latch register to display memory, affected only by the Memory Plane Write Enable field. The host data is not used in this mode.

10b -- Write Mode 2: In this mode, the bits 3-0 of the host data are replicated across all 8 bits of their respective planes. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory.

11b -- Write Mode 3: In this mode, the data in the Set/Reset field is used as if the Enable Set/Reset field were set to 1111b. Then the host data is first rotated as per the Rotate Count field, then logical ANDed with the value of the Bit Mask field. The resulting value is used on the data obtained from the Set/Reset field in the same way that the Bit Mask field would ordinarily be used. to select which bits come from the expansion of the Set/Reset field and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory.

five stages of a write:

write(void *adx, uint8_t x)  // or 4 byte?!
  
uint8_t rx[4];

switch (Write Mode) { 
case 0:
// 1. Rotate
   x = ROR(x,Rotate Count)

// 2. Clone from Host Data or Set/Reset Reg
   for (i=0;i<4;i++) { 
      if (Enable Set/Reset[i]) {
          rx[i]=Set/Reset (expanded to 8 bits)
       } else { 
          rx[i]=x;
       }
   }    

// 3. Logical Operator 
   for (i=0;i<4;i++) {
      rx[i] = rx[i] LOP LATCH_REG[i]
//    LOP = NOP, AND, OR, XOR
   }

// 4. Select
   for (i=0;i<4;i++) { 
      rx[i] = BITWISE_MUX(rx[i], LATCH_REG[i], Bit Mask Reg);
   }

// 5. Selective Write
   for (i=0;i<4;i++) { 
     if (Map Mask Reg.Memory Plane Write Enable[i])
      BUF[TRANSLATE(adx,i)] = rx[i];
   }
break;

case 1:
// 4. Select latch register directly
   for (i=0;i<4;i++) { 
      rx[i] = LATCH_REG[i];
   }
// 5. Selective Write
   for (i=0;i<4;i++) { 
     if (Map Mask Reg.Memory Plane Write Enable[i])
      BUF[TRANSLATE(adx,i)] = rx[i];
   }



   


Assume linear framebuffer, starting at address buf:

*/
    return -1;
}


static int vga_read(struct guest_info * core, 
		    addr_t guest_addr, 
		    void * dst, 
		    uint_t length, 
		    void * priv_data)
{

    PrintError("vga: vga_read UNIMPLEMENTED\n");

    /*
      Reading, 2 modes, set via Graphics Mode Register (index 05h).Read Mode:
      0 - a byte from ONE of the 4 planes is returned; which plane is determined by Read Map Select (Read Map Select Register (Index 04h))
      1 - Compare video memory and reference color (in Color Compare, except those not set in Color Don't Care), each bit in returned result is one comparison between the reference color, and is set to one if true (plane by plane, I assume)
    */
    
    return -1;
}



static int render(struct vga_internal *vga)
{
    PrintError("vga: render UNIMPLEMENTED\n");
    return 0;
}


#define ERR_WRONG_SIZE(op,reg,len,min,max)	\
    if (((len)<(min)) || ((len)>(max))) {       \
	PrintError("vga: %s of %s wrong size (%d bytes, but only %d to %d allowed)\n",(op),(reg),(len),(min),(max)); \
	return -1; \
}
	


static int misc_out_read(struct guest_info *core, 
			 uint16_t port, 
			 void *dest,
			 uint_t len,
			 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: misc out read data=0x%x\n", vga->vga_misc.vga_misc_out.val);

    ERR_WRONG_SIZE("read","misc out",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_misc_out.val;

    return len;
}

static int misc_out_write(struct guest_info *core, 
			  uint16_t port, 
			  void *src,
			  uint_t len,
			  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: misc out write data=0x%x\n", *((uint8_t*)src));
	
    ERR_WRONG_SIZE("write","misc out",len,1,1);
    
    vga->vga_misc.vga_misc_out.val =  *((uint8_t*)src) ;
    
    render(vga);
    
    return len;
}



static int input_stat0_read(struct guest_info *core, 
			    uint16_t port, 
			    void *dest,
			    uint_t len,
			    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: input stat0  read data=0x%x\n", vga->vga_misc.vga_input_stat0.val);

    ERR_WRONG_SIZE("read","inpust stat0",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_input_stat0.val;

    return len;
}


static int input_stat1_read(struct guest_info *core, 
			    uint16_t port, 
			    void *dest,
			    uint_t len,
			    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: input stat0 (%s) read data=0x%x\n", 
	       port==0x3ba ? "mono" : "color",
	       vga->vga_misc.vga_input_stat1.val);

    ERR_WRONG_SIZE("read","inpust stat1",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_input_stat1.val;

    // Stunningly, reading stat1 is also a way to reset
    // the state of attribute controller address/data flipflop
    // That is some mighty fine crack the designers were smoking.
    
    vga->vga_attribute_controller.state=ATTR_ADDR;

    return len;
}
			 

static int feature_control_read(struct guest_info *core, 
				uint16_t port, 
				void *dest,
				uint_t len,
				void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: feature control  read data=0x%x\n", 
	       vga->vga_misc.vga_feature_control.val);

    ERR_WRONG_SIZE("read","feature control",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_feature_control.val;

    return len;
}

static int feature_control_write(struct guest_info *core, 
				 uint16_t port, 
				 void *src,
				 uint_t len,
				 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: feature control (%s) write data=0x%x\n", 
	       port==0x3ba ? "mono" : "color",
	       *((uint8_t*)src));
	
    ERR_WRONG_SIZE("write","feature control",len,1,1);
    
    vga->vga_misc.vga_feature_control.val =  *((uint8_t*)src) ;
    
    render(vga);
    
    return len;
}


static int video_subsys_enable_read(struct guest_info *core, 
				    uint16_t port, 
				    void *dest,
				    uint_t len,
				    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: video subsys enable read data=0x%x\n", 
	       vga->vga_misc.vga_video_subsys_enable.val);

    ERR_WRONG_SIZE("read","video subsys enable",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_video_subsys_enable.val;

    return len;
}

static int video_subsys_enable_write(struct guest_info *core, 
				     uint16_t port, 
				     void *src,
				     uint_t len,
				     void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: video subsys enable write data=0x%x\n", *((uint8_t*)src));
	
    ERR_WRONG_SIZE("write","video subsys enable",len,1,1);
    
    vga->vga_misc.vga_video_subsys_enable.val =  *((uint8_t*)src) ;
    
    render(vga);
    
    return len;
}

static int sequencer_address_read(struct guest_info *core, 
				  uint16_t port, 
				  void *dest,
				  uint_t len,
				  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: sequencer address read data=0x%x\n", 
	       vga->vga_sequencer.vga_sequencer_addr.val);

    ERR_WRONG_SIZE("read","vga sequenver addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_sequencer.vga_sequencer_addr.val;

    return len;
}

static int sequencer_address_write(struct guest_info *core, 
				  uint16_t port, 
				  void *src,
				  uint_t len,
				  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t new_addr;

    new_addr=*((uint8_t*)src);

    PrintDebug("vga: sequencer address write data=0x%x\n", new_addr);

    ERR_WRONG_SIZE("write","vga sequenver addr",len,1,1);

    if (new_addr>VGA_SEQUENCER_NUM) {
	PrintError("vga: ignoring change of sequencer address to %u (>%u)\n",
		   new_addr, VGA_SEQUENCER_NUM);
	//return -1;
    } else {
	vga->vga_sequencer.vga_sequencer_addr.val =  *((uint8_t*)src) ;
    }

    return len;
}

static int sequencer_data_read(struct guest_info *core, 
			      uint16_t port, 
			      void *dest,
			      uint_t len,
			      void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;

    index=vga->vga_sequencer.vga_sequencer_addr.val;  // should mask probably
    data=vga->vga_sequencer.vga_sequencer_regs[index];
    
    PrintDebug("vga: sequencer data read data (index=%d) = 0x%x\n", 
	       index, data);

    ERR_WRONG_SIZE("read","vga sequenver data",len,1,1);

    *((uint8_t*)dest) = data;

    return len;
}

static int sequencer_data_write(struct guest_info *core, 
			       uint16_t port, 
			       void *src,
			       uint_t len,
			       void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;
    
    data=*((uint8_t*)src);
    index=vga->vga_sequencer.vga_sequencer_addr.val;  // should mask probably
    
    PrintDebug("vga: sequencer write data (index=%d) with 0x%x\n", 
	       index, data);
    
    ERR_WRONG_SIZE("read","vga sequenver data",len,1,1);
    
    vga->vga_sequencer.vga_sequencer_regs[index] = data;

    render(vga);
    
    return len;
}
 
 


static int crt_controller_address_read(struct guest_info *core, 
					uint16_t port, 
					void *dest,
					uint_t len,
					void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug("vga: crt controller (%s) address read data=0x%x\n", 
	       port==0x3b4 ? "mono" : "color",
	       vga->vga_crt_controller.vga_crt_addr.val);

    ERR_WRONG_SIZE("read","vga crt controller addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_crt_controller.vga_crt_addr.val;

    return len;
}

static int crt_controller_address_write(struct guest_info *core, 
					uint16_t port, 
					void *src,
					uint_t len,
					void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t new_addr;

    new_addr=*((uint8_t*)src);

    PrintDebug("vga: crt controller (%s) address write data=0x%x\n", 
	       port==0x3b4 ? "mono" : "color",
	       new_addr);

    ERR_WRONG_SIZE("write","vga crt controller addr",len,1,1);

    if (new_addr>VGA_CRT_CONTROLLER_NUM) {
	PrintError("vga: ignoring change of crt controller address to %u (>%u)\n",
		   new_addr, VGA_CRT_CONTROLLER_NUM);
	//return -1;
    } else {
	vga->vga_crt_controller.vga_crt_addr.val =  *((uint8_t*)src) ;
    }

    return len;
}

static int crt_controller_data_read(struct guest_info *core, 
				    uint16_t port, 
				    void *dest,
				    uint_t len,
				    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;

    index=vga->vga_crt_controller.vga_crt_addr.val;  // should mask probably
    data=vga->vga_crt_controller.vga_crt_controller_regs[index];
    
    PrintDebug("vga: crt controller data (%s) read data (index=%d) = 0x%x\n", 
	       port==0x3b5 ? "mono" : "color",
	       index, data);

    ERR_WRONG_SIZE("read","vga crt controller data",len,1,1);

    *((uint8_t*)dest) = data;

    return len;
}

static int crt_controller_data_write(struct guest_info *core, 
				     uint16_t port, 
				     void *src,
				     uint_t len,
				     void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;

    data=*((uint8_t*)src);

    index=vga->vga_crt_controller.vga_crt_addr.val;  // should mask probably
    
    PrintDebug("vga: crt controller (%s) write data (index=%d) with 0x%x\n", 
	       port==0x3b5 ? "mono" : "color",
	       index, data);

    ERR_WRONG_SIZE("read","vga crt controlle data",len,1,1);

    vga->vga_crt_controller.vga_crt_controller_regs[index] = data;

    render(vga);

    return len;
}


static int graphics_controller_address_read(struct guest_info *core, 
					    uint16_t port, 
					    void *dest,
					    uint_t len,
					    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: graphics controller address read data=0x%x\n", 
	       vga->vga_graphics_controller.vga_graphics_ctrl_addr.val);

    ERR_WRONG_SIZE("read","vga graphics controller addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_graphics_controller.vga_graphics_ctrl_addr.val;

    return len;
}

static int graphics_controller_address_write(struct guest_info *core, 
					     uint16_t port, 
					     void *src,
					     uint_t len,
					     void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t new_addr;

    new_addr=*((uint8_t*)src);

    PrintDebug("vga: graphics controller address write data=0x%x\n", new_addr);

    ERR_WRONG_SIZE("write","vga graphics controller addr",len,1,1);

    if (new_addr>VGA_GRAPHICS_CONTROLLER_NUM) {
	PrintError("vga: ignoring change of graphics controller address to %u (>%u)\n",
		   new_addr, VGA_GRAPHICS_CONTROLLER_NUM);
	//return -1;
    } else {
	vga->vga_graphics_controller.vga_graphics_ctrl_addr.val =  *((uint8_t*)src) ;
    }

    return len;
}

static int graphics_controller_data_read(struct guest_info *core, 
					 uint16_t port, 
					 void *dest,
					 uint_t len,
					 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;

    index=vga->vga_graphics_controller.vga_graphics_ctrl_addr.val;  // should mask probably
    data=vga->vga_graphics_controller.vga_graphics_controller_regs[index];
    
    PrintDebug("vga: graphics controller data read data (index=%d) = 0x%x\n", 
	       index, data);

    ERR_WRONG_SIZE("read","vga graphics controller data",len,1,1);

    *((uint8_t*)dest) = data;

    return len;
}

static int graphics_controller_data_write(struct guest_info *core, 
					  uint16_t port, 
					  void *src,
					  uint_t len,
					  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;
    
    data=*((uint8_t*)src);
    index=vga->vga_graphics_controller.vga_graphics_ctrl_addr.val;  // should mask probably
    
    PrintDebug("vga: graphics_controller write data (index=%d) with 0x%x\n", 
	       index, data);
    
    ERR_WRONG_SIZE("read","vga sequenver data",len,1,1);
    
    vga->vga_graphics_controller.vga_graphics_controller_regs[index] = data;

    render(vga);
    
    return len;
}



/* Note that these guys have a bizarre protocol*/

static int attribute_controller_address_read(struct guest_info *core, 
					     uint16_t port, 
					     void *dest,
					     uint_t len,
					     void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: attribute controller address read data=0x%x\n", 
	       vga->vga_attribute_controller.vga_attribute_controller_addr.val);

    ERR_WRONG_SIZE("read","vga attribute  controller addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_attribute_controller.vga_attribute_controller_addr.val;

    // Reading the attribute controller does not change the state

    return len;
}

static int attribute_controller_address_and_data_write(struct guest_info *core, 
						       uint16_t port, 
						       void *src,
						       uint_t len,
						       void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;


    if (vga->vga_attribute_controller.state==ATTR_ADDR) { 
	uint8_t new_addr = *((uint8_t*)src);
	// We are to treat this as an address write, and flip state
	// to expect data ON THIS SAME PORT
	PrintDebug("vga: graphics controller address write data=0x%x\n", new_addr);
	
	ERR_WRONG_SIZE("write","vga graphics controller addr",len,1,1);

	if (new_addr>VGA_ATTRIBUTE_CONTROLLER_NUM) {
	    PrintError("vga: ignoring change of attribute controller address to %u (>%u)\n",
		       new_addr, VGA_ATTRIBUTE_CONTROLLER_NUM);
	    //return -1;
	} else {
	    vga->vga_attribute_controller.vga_attribute_controller_addr.val =  *((uint8_t*)src) ;
	}
	vga->vga_attribute_controller.state=ATTR_DATA;
	return len;

    } else if (vga->vga_attribute_controller.state==ATTR_DATA) { 

	uint8_t data = *((uint8_t*)src);
	uint8_t index=vga->vga_attribute_controller.vga_attribute_controller_addr.val;  // should mask probably

	PrintDebug("vga: graphics controller data write index %d with data=0x%x\n", index,data);
	
	ERR_WRONG_SIZE("write","vga graphics controller data",len,1,1);

	vga->vga_attribute_controller.vga_attribute_controller_regs[index] = data;
	
	vga->vga_attribute_controller.state=ATTR_ADDR;
	
	return len;
    }
    
    return -1;
	
}

static int attribute_controller_data_read(struct guest_info *core, 
					  uint16_t port, 
					  void *dest,
					  uint_t len,
					  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t index;
    uint8_t data;

    index=vga->vga_attribute_controller.vga_attribute_controller_addr.val;  // should mask probably
    data=vga->vga_attribute_controller.vga_attribute_controller_regs[index];
    
    PrintDebug("vga: attribute controller data read data (index=%d) = 0x%x\n", 
	       index, data);

    ERR_WRONG_SIZE("read","vga attribute controller data",len,1,1);

    *((uint8_t*)dest) = data;

    return len;
}


/*
   Note that these guys also have a strange protocol
   since they need to squeeze 18 bits of data through
   an 8 bit port 
*/
static int dac_write_address_read(struct guest_info *core, 
				  uint16_t port, 
				  void *dest,
				  uint_t len,
				  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: dac write address read data=0x%x\n", 
	       vga->vga_dac.vga_dac_write_addr);

    ERR_WRONG_SIZE("read","vga dac write addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_dac.vga_dac_write_addr;

    // This read does not reset the state machine

    return len;
}

static int dac_write_address_write(struct guest_info *core, 
				   uint16_t port, 
				   void *src,
				   uint_t len,
				   void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t new_addr;

    new_addr=*((uint8_t*)src);

    PrintDebug("vga: dac write address write data=0x%x\n", new_addr);

    ERR_WRONG_SIZE("write","vga dac write addr",len,1,1);

    // cannot be out of bounds since there are 256 regs

    vga->vga_dac.vga_dac_write_addr =  *((uint8_t*)src) ;

    // Now we also need to reset the state machine
    
    vga->vga_dac.state=DAC_WRITE;
    vga->vga_dac.channel=RED;

    return len;
}


static int dac_read_address_read(struct guest_info *core, 
				 uint16_t port, 
				 void *dest,
				 uint_t len,
				 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: dac read address read data=0x%x\n", 
	       vga->vga_dac.vga_dac_read_addr);

    ERR_WRONG_SIZE("read","vga dac read addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_dac.vga_dac_read_addr;

    // This read does not reset the state machine

    return len;
}

static int dac_read_address_write(struct guest_info *core, 
				  uint16_t port, 
				  void *src,
				  uint_t len,
				  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t new_addr;

    new_addr=*((uint8_t*)src);

    PrintDebug("vga: dac read address write data=0x%x\n", new_addr);

    ERR_WRONG_SIZE("write","vga dac read addr",len,1,1);

    // cannot be out of bounds since there are 256 regs

    vga->vga_dac.vga_dac_read_addr =  *((uint8_t*)src) ;

    // Now we also need to reset the state machine
    
    vga->vga_dac.state=DAC_READ;
    vga->vga_dac.channel=RED;

    return len;
}


static int dac_data_read(struct guest_info *core, 
			 uint16_t port, 
			 void *dest,
			 uint_t len,
			 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t curreg;
    uint8_t curchannel;
    uint8_t data;
    
    if (vga->vga_dac.state!=DAC_READ) { 
	PrintError("vga: dac data read while in other state\n");
	// results undefined, so we continue
    }

    ERR_WRONG_SIZE("read","vga dac read data",len,1,1);

    curreg = vga->vga_dac.vga_dac_read_addr;
    curchannel = vga->vga_dac.channel;
    data = (vga->vga_dac.vga_dac_palette[curreg] >> curchannel*8) & 0x3f;

    PrintDebug("vga: dac reg %u [%s] = 0x%x\n",
	       curreg, 
	       curchannel == 0 ? "RED" : curchannel == 1 ? "GREEN" 
                          : curchannel==2 ? "BLUE" : "BAD CHANNEL", 
	       data);

    *((uint8_t*)dest) = data;
    
    curchannel = (curchannel+1)%3;
    vga->vga_dac.channel=curchannel;
    if (curchannel==0) { 
	curreg = (curreg + 1) % VGA_DAC_NUM_ENTRIES;
    } 
    vga->vga_dac.vga_dac_read_addr = curreg;
    vga->vga_dac.state=DAC_READ;

    return len;
}



static int dac_data_write(struct guest_info *core, 
			  uint16_t port, 
			  void *src,
			  uint_t len,
			  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t curreg;
    uint8_t curchannel;
    uint8_t data;
    vga_palette_reg data32;
    vga_palette_reg mask32;
    
    if (vga->vga_dac.state!=DAC_WRITE) { 
	PrintError("vga: dac data write while in other state\n");
	// results undefined, so we continue
    }

    ERR_WRONG_SIZE("read","vga dac write data",len,1,1);

    curreg = vga->vga_dac.vga_dac_write_addr;
    curchannel = vga->vga_dac.channel;
    data = *((uint8_t *)src);

    PrintDebug("vga: dac reg %u [%s] write with 0x%x\n",
	       curreg, 
	       curchannel == 0 ? "RED" : curchannel == 1 ? "GREEN" 
                          : curchannel==2 ? "BLUE" : "BAD CHANNEL", 
	       data);

    data32 = data & 0x3f ;
    data32 <<= curchannel*8;
    mask32 = ~(0xff << (curchannel * 8));

    vga->vga_dac.vga_dac_palette[curreg] &= mask32;
    vga->vga_dac.vga_dac_palette[curreg] |= data32;

    curchannel = (curchannel+1)%3;
    vga->vga_dac.channel=curchannel;
    if (curchannel==0) { 
	curreg = (curreg + 1) % VGA_DAC_NUM_ENTRIES;
    } 
    vga->vga_dac.vga_dac_write_addr = curreg;
    vga->vga_dac.state=DAC_WRITE;

    render(vga);

    return len;
}

 

static int dac_pixel_mask_read(struct guest_info *core, 
			       uint16_t port, 
			       void *dest,
			       uint_t len,
			       void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug("vga: dac pixel mask read data=0x%x\n", 
	       vga->vga_dac.vga_pixel_mask);

    ERR_WRONG_SIZE("read","vga pixel mask",len,1,1);

    *((uint8_t*)dest) = vga->vga_dac.vga_pixel_mask;

    return len;
}

static int dac_pixel_mask_write(struct guest_info *core, 
				uint16_t port, 
				void *src,
				uint_t len,
				void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    uint8_t new_data;

    new_data=*((uint8_t*)src);

    PrintDebug("vga: dac pixel mask write data=0x%x\n", new_data);

    ERR_WRONG_SIZE("write","pixel mask",len,1,1);

    vga->vga_dac.vga_pixel_mask =  new_data;

    return len;
}

static int init_vga(struct vga_internal *vga)
{
    // TODO: startup spec of register contents, if any
    PrintError("vga: init_vga is UNIMPLEMTED\n");
    return 0;
}

static int free_vga(struct vga_internal *vga) 
{
    int i;
    int ret;
    struct vm_device *dev = vga->dev;

    // Framebuffer deletion is user's responsibility

    //    if (vga->mem_store) {
    //	V3_FreePages(v3_hva_to_hpa(vga->mem_store),MEM_REGION_NUM_PAGES);
    //  vga->mem_store=0;
    //}
    
    for (i=0;i<MAP_NUM;i++) { 
	if (vga->map[i]) { 
	    V3_FreePages(V3_PAddr(vga->map[i]),MAP_SIZE/4096);
	    vga->map[i]=0;
	}
    }

    v3_unhook_mem(vga->dev->vm, V3_MEM_CORE_ANY, MEM_REGION_START);

    ret = 0;

    ret |= v3_dev_unhook_io(dev, VGA_MISC_OUT_READ);
    // The following also covers VGA_INPUT_STAT0_READ
    ret |= v3_dev_unhook_io(dev, VGA_MISC_OUT_WRITE);
    // The following also covers VGA_FEATURE_CTRL_WRITE_MONO
    ret |= v3_dev_unhook_io(dev, VGA_INPUT_STAT1_READ_MONO);
    // The folowinn also covers VGA FEATURE_CONTROL_WRITE_COLOR
    ret |= v3_dev_unhook_io(dev, VGA_INPUT_STAT1_READ_COLOR);
    ret |= v3_dev_unhook_io(dev, VGA_FEATURE_CONTROL_READ);
    
    ret |= v3_dev_unhook_io(dev, VGA_VIDEO_SUBSYS_ENABLE);

    /* Sequencer registers */
    ret |= v3_dev_unhook_io(dev, VGA_SEQUENCER_ADDRESS);
    ret |= v3_dev_unhook_io(dev, VGA_SEQUENCER_DATA);

    /* CRT controller registers */
    ret |= v3_dev_unhook_io(dev, VGA_CRT_CONTROLLER_ADDRESS_MONO);
    ret |= v3_dev_unhook_io(dev, VGA_CRT_CONTROLLER_ADDRESS_COLOR);
    ret |= v3_dev_unhook_io(dev, VGA_CRT_CONTROLLER_DATA_MONO);
    ret |= v3_dev_unhook_io(dev, VGA_CRT_CONTROLLER_DATA_COLOR);

    /* graphics controller registers */
    ret |= v3_dev_unhook_io(dev, VGA_GRAPHICS_CONTROLLER_ADDRESS);
    ret |= v3_dev_unhook_io(dev, VGA_GRAPHICS_CONTROLLER_DATA);

    /* attribute controller registers */
    ret |= v3_dev_unhook_io(dev, VGA_ATTRIBUTE_CONTROLLER_ADDRESS_AND_WRITE);
    ret |= v3_dev_unhook_io(dev, VGA_ATTRIBUTE_CONTROLLER_READ);

    /* video DAC palette registers */
    ret |= v3_dev_unhook_io(dev, VGA_DAC_WRITE_ADDR);
    ret |= v3_dev_unhook_io(dev, VGA_DAC_READ_ADDR);
    ret |= v3_dev_unhook_io(dev, VGA_DAC_DATA);
    ret |= v3_dev_unhook_io(dev, VGA_DAC_PIXEL_MASK);


    V3_Free(vga);

    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))free_vga,
};


static int vga_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vga_internal *vga;
    int i;
    int ret;

    char * dev_id = v3_cfg_val(cfg, "ID");

    // DETERMINE THE FRAMEBUFFER AND SET IT EARLY
    // FRAMEBUFFER IS SUPPLIED BY THE BACKEND

    PrintDebug("vga: init_device\n");

    vga = (struct vga_internal *)V3_Malloc(sizeof(struct vga_internal));

    if (!vga) { 
	PrintError("vga: cannot allocate\n");
	return -1;
    }

    memset(vga, 0, sizeof(struct vga_internal));

    // No memory store is allocated since we will use a full memory hook
    // The VGA maps can be read as well as written

    // Now allocate the maps
    for (i=0;i<MAP_NUM;i++) { 
	vga->map[i] = (vga_map) V3_VAddr((void*)V3_AllocPages(MAP_SIZE/4096));
	if (!(vga->map[i])) {
	    free_vga(vga);
	}
	memset(vga->map[i],0,MAP_SIZE);
    }
    
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, vga);
    
    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	free_vga(vga);
	return -1;
    }
    
    vga->dev = dev;
    
    if (v3_hook_full_mem(vm, V3_MEM_CORE_ANY, 
			 MEM_REGION_START, MEM_REGION_END,
			 &vga_read, 
			 &vga_write,
			 dev) == -1) {
	PrintError("vga: memory book failed\n");
	v3_remove_device(dev);
	return -1;
    }

    ret = 0;
    
    /* Miscelaneous registers */
    ret |= v3_dev_hook_io(dev, VGA_MISC_OUT_READ, &misc_out_read, NULL);
    // The following also covers VGA_INPUT_STAT0_READ
    ret |= v3_dev_hook_io(dev, VGA_MISC_OUT_WRITE, &input_stat0_read, &misc_out_write);
    // The following also covers VGA_FEATURE_CTRL_WRITE_MONO
    ret |= v3_dev_hook_io(dev, VGA_INPUT_STAT1_READ_MONO, &input_stat1_read, &feature_control_write);
    // The folowinn also covers VGA FEATURE_CONTROL_WRITE_COLOR
    ret |= v3_dev_hook_io(dev, VGA_INPUT_STAT1_READ_COLOR, &input_stat1_read, &feature_control_write);
    ret |= v3_dev_hook_io(dev, VGA_FEATURE_CONTROL_READ, &feature_control_read, NULL);
    
    ret |= v3_dev_hook_io(dev, VGA_VIDEO_SUBSYS_ENABLE, &video_subsys_enable_read, &video_subsys_enable_write);

    /* Sequencer registers */
    ret |= v3_dev_hook_io(dev, VGA_SEQUENCER_ADDRESS, &sequencer_address_read, &sequencer_address_write);
    ret |= v3_dev_hook_io(dev, VGA_SEQUENCER_DATA, &sequencer_data_read, &sequencer_data_write);

    /* CRT controller registers */
    ret |= v3_dev_hook_io(dev, VGA_CRT_CONTROLLER_ADDRESS_MONO, &crt_controller_address_read,&crt_controller_address_write);
    ret |= v3_dev_hook_io(dev, VGA_CRT_CONTROLLER_ADDRESS_COLOR, &crt_controller_address_read,&crt_controller_address_write);
    ret |= v3_dev_hook_io(dev, VGA_CRT_CONTROLLER_DATA_MONO, &crt_controller_data_read,&crt_controller_data_write);
    ret |= v3_dev_hook_io(dev, VGA_CRT_CONTROLLER_DATA_COLOR, &crt_controller_data_read,&crt_controller_data_write);

    /* graphics controller registers */
    ret |= v3_dev_hook_io(dev, VGA_GRAPHICS_CONTROLLER_ADDRESS, &graphics_controller_address_read,&graphics_controller_address_write);
    ret |= v3_dev_hook_io(dev, VGA_GRAPHICS_CONTROLLER_DATA, &graphics_controller_data_read,&graphics_controller_data_write);

    /* attribute controller registers */
    ret |= v3_dev_hook_io(dev, VGA_ATTRIBUTE_CONTROLLER_ADDRESS_AND_WRITE, &attribute_controller_address_read,&attribute_controller_address_and_data_write);
    ret |= v3_dev_hook_io(dev, VGA_ATTRIBUTE_CONTROLLER_READ, &attribute_controller_data_read,NULL);

    /* video DAC palette registers */
    ret |= v3_dev_hook_io(dev, VGA_DAC_WRITE_ADDR, &dac_write_address_read,&dac_write_address_write);
    ret |= v3_dev_hook_io(dev, VGA_DAC_READ_ADDR, &dac_read_address_read,&dac_read_address_write);
    ret |= v3_dev_hook_io(dev, VGA_DAC_DATA, &dac_data_read, &dac_data_write);
    ret |= v3_dev_hook_io(dev, VGA_DAC_PIXEL_MASK, &dac_pixel_mask_read, &dac_pixel_mask_write);

    if (ret != 0) {
	PrintError("vga: Error allocating VGA I/O ports\n");
	v3_remove_device(dev);
	return -1;
    }

    init_vga(vga);

    PrintDebug("vga: successfully added and initialized, waiting for framebuffer attach\n");

    return 0;

}

device_register("VGA", vga_init);