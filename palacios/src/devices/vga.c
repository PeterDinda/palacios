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

#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>

#include "vga_regs.h"

#define MAP_SIZE 65536
#define MAP_NUM  4

typedef uint8_t vga_map[MAP_SIZE];


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
    struct vga_crt_addr_reg cga_crt_addr;

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

    /* 0x3c0 */
    struct vga_attribute_controller_address_reg vga_attribute_controller_address;


    
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

struct vga_internal {
    struct frame_buf *framebuf; // we render to this
    
    vga_map  map[MAP_NUM];

    /* Range of I/O ports here for backward compat with MDA and CGA */
    struct vga_misc_regs  vga_misc;

    /* Address Register is 0x3b4 or 0x3d4 */
    /* Data register is 0x3b5 or 0x3d5 based on MDA/CGA/VGA (backward compat) */
    struct vga_crt_controller_regs vga_crt_controller;

    /*   Address register is 0x3c4, data register is 0x3c5 */
    struct vga_sequencer_regs vga_sequencer;

    /*   Address: 0x3ce    Data: 0x3cf */
    struct vga_graphics_controller_regs vga_graphics_contoller;

    /*
      Address AND WRITE: 0x3c0
      Read: 0x3c1
      Flip-Flop
    */
    struct vga_attribute_contoller_regs vga_attribute_controller;

};


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

Reading, 2 modes, set via Graphics Mode Register (index 05h).Read Mode:
   0 - a byte from ONE of the 4 planes is returned; which plane is determined by Read Map Select (Read Map Select Register (Index 04h))
   1 - Compare video memory and reference color (in Color Compare, except those not set in Color Don't Care), each bit in returned result is one comparison between the reference color, and is set to one if true (plane by plane, I assume)

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
