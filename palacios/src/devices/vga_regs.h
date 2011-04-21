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
#ifndef _VGA_REGS
#define _VGA_REGS

/*
    General Purpose Registers 
  
    These have well-known io ports for backward compatibility with
    monochrome and cga controllers.   Note that the ioports of 
    some of these vary depending on vga_misc_out_reg.io_addr_sel.  
    If this is zero, then they are mapped as expected for a mono card
    If one, then as for a cga card

*/

/* Read: 0x3cc; Write: 0x3c2 */
struct vga_misc_out_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t io_addr_sel:1; // 0=>CRT at 0x3bx, input1 at 0x3ba (mono)
                                   // 1=>CRT at 0x3dx, input1 at 0x3da (cga)
	    uint8_t en_ram:1;      // allow FB writes
	    uint8_t clock_sel:2;   // 00=>25.175/640 wide; 01=>28.322/720 wide
	    uint8_t reserved:2;
	    uint8_t horiz_sync_pol:1; // VH=01 => 400 lines; 10 => 350 lines
	    uint8_t vert_sync_pol:1;  // VH=11 => 480 lines
	    uint8_t reserved2:1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* Read: 0x3c2 */
struct vga_input_stat0_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t reserved:4;
	    uint8_t switch_sense:1; // type of display attached (mono/color)
	    uint8_t reserved2:2;
	    uint8_t crt_inter:1;   // vertical retrace interrupt pending;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* Read: 0x3?a  3ba for mono; 3da for cga set by misc.io_addr_sel */
struct vga_input_stat1_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t disp_en:1; // in horizontal or vertical retrace (drawing)
	    uint8_t reserved:2;
	    uint8_t vert_retrace:1; // in vertical retrace interval
	    uint8_t reserved2:4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* Read: 0x3ca; Write: 0x3?a 3ba for mono 3da for color - set by misc.io_addr_sel*/
struct vga_feature_control_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t reserved:8; // per IBM spec
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Read: 0x3c3; Write: 0x3c3 */
struct vga_video_subsys_enable_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t reserved:8; // per IBM spec
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* 
   Sequencer Registers 
  
   These are all accessed via address and data registers.  
  
   Address register is 0x3c4, data register is 0x3c5
*/

/* 0x3c4 */
struct vga_sequencer_addr_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t seq_address:3; 
	    uint8_t reserved:5; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 0 */
struct vga_reset_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t async_reset:1; // write zero to async clear and halt
	    uint8_t sync_reset:1;  // write zero to sync clear and halt
                                   // write 11 to run
	    uint8_t reserved:6; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Index 1 */
struct vga_clocking_mode_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t dot8:1;        // 0=>9 dot clocks per char clock; 1=>8
	    uint8_t reserved:1;    // must be one
	    uint8_t shift_load:1;  //
	    uint8_t dot_clock:1;   // 0=> dc=master clock; 1=> dc=0.5*master
                                   // e.g. 0 for 640 wide, 1 for 320 wide
	    uint8_t shift_4:1;     // shift_4 shift_load  Load video serializer
                                   //    0       0          every char clock
                                   //    0       1          every 2nd
                                   //    1       0          every 4th 
	    uint8_t screen_off:1;  // turn off display
	    uint8_t reserved2:2;   
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Index 2 */
struct vga_map_mask_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t map0_en:1;  // enable map 0 (write to bank zero of mem)
	    uint8_t map1_en:1;  // enable map 1
	    uint8_t map2_en:1;  // enable map 2  All enabled=>chain4 mode
	    uint8_t map3_en:1;  // enable map 3
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Index 3 */
struct vga_char_map_select_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t char_map_b_sel_lsb:2;  // low 2 bits of char map b sel
	    uint8_t char_map_a_sel_lsb:2;  // low 2 bits of char map a sel
            uint8_t char_map_b_sel_msb:1;  // high bit of char map b sel
	    uint8_t char_map_a_sel_msb:1;  // high bit of char map a sel
	    uint8_t reserved:2; 
	    /*
               For A: the map bits give the char sets at:
                  000  1st 8K of map (bank) 2
                  001  3rd 8K
                  010  5th 8K
                  011  7th 8K
                  100  2nd 8K
                  101  4th 8K
                  110  6th 8K
                  111  8th 8K
 
               Identical for B
	    */
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Index 4 */
struct vga_mem_mode_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t reserved:1;
	    uint8_t extended_memory:1; // 1=>256K RAM, 0=>64K
	    uint8_t odd_even:1;
                // 0 => even addresses go to BOTH banks 0 and 2, odd 1 and 3
                // 1 => address bank sequentially, map mask selects bank
	    uint8_t chain4:1;  
                // 0 => map mask register used to select bank
                // 1 => lower 2 bits of address used to select bank
	    uint8_t reserved2:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/*
  CRT Controller Registers

  These registers control the rendering of the framebuffer (the maps)
  to the monitor.   This includes text/graphics mode, and which kind of mode
  
  It's important to understand that while the vertical resolution works
  as you would expect, using scan lines, the horizontal resolution is
  in terms of characters (char clocks), EVEN IF YOU ARE IN A GRAPHICS MODE

  Another important thing to understand is that the modes defined in he
  VGA/VESA bios are just convenient names for particular settings of 
  the CRT controller registers.   Other options are also possible.

  Address register is 0x3d4 or 0x3b4 depending on misc.ioaddr_sel
  Data register is 0x3d5 or 0x3b5 depending on misc.ioaddr_sel
  b4/5 is for mono compatability
*/

/* 0x3b4 or 0x3d4 */
struct vga_crt_addr_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t crt_address:5; 
	    uint8_t reserved:3; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;


/* index 0 */
// Total number of chars in horizontal interval, including retrace
typedef uint8_t vga_horizontal_total_reg;

/* index 1 */
// text: number of displayed characters minus 1 (columns-1)
// graphic: horizontal resolution is cols*#dotclocks_per_charclock
typedef uint8_t vga_horizontal_display_enable_end_reg;

/* index 2 */
// horizontal characer count at which blanking starts
typedef uint8_t vga_start_horizontal_blanking_reg;

/* index 3 */
struct vga_end_horizontal_blanking_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t end_blanking:5; // when blanking ends, in char clocks 
                                    // top bit in horizontal retrace
            uint8_t display_enable_skew:2; // skew in character clocks
	    uint8_t reserved:1;     // must be 1 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 4 */
// screen centering:  char position where horizontal retrace is active
typedef uint8_t vga_start_horizontal_retrace_pulse_reg;

/* index 5 */
struct vga_end_horizontal_retrace_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t end_horizontal_retrace:5; 
	    // horizontal char count where retrace inactive
	    uint8_t horizontal_retrace_delay:2; 
	    // skew of retace signal in char clocks
	    uint8_t end_horizontal_blanking5:1;
	    // top bit in horizontal blanking
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 6 */
// lower 8 bits of total number of scan lines minus 2
// There are 10 bits total, 2 of which are on the overflow reg
typedef uint8_t vga_vertical_total_reg;

/* index 7 */
struct vga_overflow_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t vertical_total8:1; // vert total, bit 8
	    uint8_t vertical_disp_enable_end8:1; // bit 8
	    uint8_t vertical_retrace_start8:1; // bit 8
	    uint8_t vertical_blanking_start8:1; //bit 8
	    uint8_t line_compare8:1; // bit 8;
	    uint8_t vertical_total9:1; // vert total, bit 9
	    uint8_t vertical_disp_enable_end9:1; // bit 9
	    uint8_t vertical_retrace_start9:1; // bit 9
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 8 */
struct vga_preset_row_scan_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t start_row_scan_count:5; 
	    // what the row counter begins at?
	    uint8_t byte_panning:2; 
	    // ?
	    uint8_t reserved:1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 9 */
struct vga_max_row_scan_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t max_scan_line:5; 
	    // scan lines per character row minus 1
	    uint8_t start_vertical_blanking9:1; 
	    // bit 9 of this field
	    uint8_t line_compare9:1;
	    // bit 9 of this field
	    uint8_t double_scan:1; // 1=> 200 scan lines double to 400
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 10 */
struct vga_cursor_start_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t row_scan_cursor_begin:5; 
	    // row at which the cursor begins
	    uint8_t cursor_off:1; 
	    // bit 9 of this field
	    uint8_t reserved:2;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 11 */
struct vga_cursor_end_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t row_scan_cursor_end:5; 
	    // row at which the cursor ends
	    uint8_t cursor_skew:2; 
	    // skew in cursor clocks
	    uint8_t reserved:1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;


/* index 12 */
// starting address of regenerative buffer (?)
typedef uint8_t vga_start_address_high_reg;
/* index 13 */
// starting address of regenerative buffer (?)
typedef uint8_t vga_start_address_low_reg;

/* index 14 */
// cursor location
typedef uint8_t vga_cursor_location_high_reg;
/* index 15 */
// cursor location
typedef uint8_t vga_cursor_location_low_reg;

/* index 16 */
// vertical retrace start - low 8 bits, 9th is on overflow register
typedef uint8_t vga_vertical_retrace_start_reg;


/* index 17 */
struct vga_vertical_retrace_end_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t vertical_retrace_end:4;
	    uint8_t clear_vertical_interrupt:1; // 0 to clear interrupt
	    uint8_t enable_vertical_interrupt:1; // IRQ2 on vert retrace
	    uint8_t select_5_refresh_cycles:1; // Slower displays
	    uint8_t protect_regs:1; // 1=> indices 0..7 disabled
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 18 */
// lower 8 bits of 10 bit value (rest on overflow reg)
// this is the total number of scan lines minus one
typedef uint8_t vga_vertical_display_enable_end_reg;

/* index 19 */
// logical line width of the screen (including attrs)
// starting memory address of next character row is 2 or 4 times
// this value
typedef uint8_t vga_offset_reg;


/* index 20 */
struct vga_underline_location_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t start_underline:5; // scan line in char where UL starts
	    uint8_t count_by_four:1;   // memory addresses count by 4 (for dw)
	    uint8_t doubleword:1;      // memory addresses are 32 bits
	    uint8_t reserved:1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 21 */
// 8 bits of 10 bit quanity, rest are on overflow and maximum scan line reg
// scan line count at which vertical blanking goes on, minus one
typedef uint8_t vga_start_vertical_blanking_reg;

/* index 22 */
// scan line count at which vertical blanking goes off
typedef uint8_t vga_end_vertical_blanking_reg;


/* index 23 */
struct vga_crt_mode_control_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t cms0:1; 
	    // bit 13 of output mux come from rowscan, bit zero if zero
	    // otherwise from bit 13 of address counter
	    // ?? compatability with CGA
   
	    uint8_t select_row_scan_ctr:1;
	    // bit 14 of output mux comes from bit 1 of rowscan if zero
	    // otherwise from bit 14 of addres counter
	    // ? CGA ?

	    uint8_t horizontal_retrace_select:1;
	    // select clock of vertical timing counter
	    // 0 = horiz retrace clock, 1=same / 2

	    uint8_t count_by_two:1;
	    // address counter source
	    // 0 = character clock
	    // 1 = character clock / 2 (word)

	    uint8_t reserved:1;

	    uint8_t address_wrap:1;
	    // is MA 13 or MA 15 output in MA 0 when in word address mode

	    uint8_t word_byte_mode:1;
	    // 0 = word mode memory access , 1= byte
	    
	    uint8_t reset:1;
	    // 0 = stop horizontal and vertical retrace
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* index 24 */
// lower 8 bits of 10 bit counter; rest are on overflow and max scan line regs
// line counter is zeroed when it hits this value
typedef uint8_t vga_line_compare_reg;

/*
   Graphics Controller Registers

   These registers control how the framebuffer / banks are accessed 
   from the host computer.   How a memory write is translated is 
   determined by these settings.

   Address: 0x3ce
   Data: 0x3cf

*/

/* 0x3ce */
struct vga_graphics_ctrl_addr_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t graphics_address:4; 
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 0 */
// these are the values written to each map given memory write
// mode 0.   While they are single bits, they are extended to full 
// bytes
struct vga_set_reset_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t sr0:1; //  bank 0
	    uint8_t sr1:1; //  bank 1
	    uint8_t sr2:1; //  bank 2
	    uint8_t sr3:1; //  bank 3
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 1 */
// these flags enable the use of the set/reset register values
// in writing the map when write mode 0 is used
struct vga_enable_set_reset_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t esr0:1; //  bank 0
	    uint8_t esr1:1; //  bank 1
	    uint8_t esr2:1; //  bank 2
	    uint8_t esr3:1; //  bank 3
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 2 */
// Color comparison values for one of the read modes
struct vga_color_compare_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t cc0:1; //  bank 0
	    uint8_t cc1:1; //  bank 1
	    uint8_t cc2:1; //  bank 2
	    uint8_t cc3:1; //  bank 3
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 3 */
// rotation amount and function for write mode 0 and others
struct vga_data_rotate_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t rotate_count:3; //  amount to ROR bits being written
	    uint8_t function:2;     // function to use
	    // 00 : NOP
	    // 01 : AND
	    // 10 : OR
	    // 11 : XOR
	    uint8_t reserved:3; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 4 */
// which of the maps (banks) will be read from when a system read occurs
// Does not affect color compare reads
struct vga_read_map_select_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t map_select:2; //  which one you want to read
	    uint8_t reserved:6; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 5 */
// Graphics Mode DOES NOT MEAN graphics mode - it means
// the modes in which framebuffer reads/writes are done
struct vga_graphics_mode_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t write_mode:2; 
	    /* 
              
From the IBM docmentation:

0 0 Each memory map is written with the system data rotated by the count
in the Data Rotate register. If the set/reset function is enabled for a
specific map, that map receives the 8-bit value contained in the
Set/Reset register.

0 1 Each memory map is written with the contents of the system latches.
These latches are loaded by a system read operation.

1 0 Memory map n (0 through 3) is filled with 8 bits of the value of data
bit n.

1 1 Each memory map is written with the 8-bit value contained in the
Set/Reset register for that map (the Enable Set/Reset register has no
effect). Rotated system data is ANDed with the Bit Mask register to
form an 8-bit value that performs the same function as the Bit Mask
register in write modes 0 and 2 (see also Bit Mask register on
page 2-88).
	    */
	    uint8_t reserved:1;
	    uint8_t read_mode:1;
	    // 1 = read gets comparison of all maps and color compare
	    // 0 = read gets bits from selected map
	    uint8_t odd_even:1;
	    // 1 = odd/even addressing as in CGMA
	    uint8_t shift_reg_mode:1;
	    // 1 = shift regs get odd bits from odd maps and even/even
	    uint8_t c256:1;         	    // 1 = 256 color mode
	    // 0 = shift_reg_mode controls shift regs
	    uint8_t reserved2:1; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 6 */
// Misc
struct vga_misc_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t graphics_mode:1;
	    // if on, then we are in a graphics mode
	    // if off, we are in a text mode
	    uint8_t odd_even:1;
	    // if on, then low order bit of system address
	    // selects odd or even map
	    uint8_t memory_map:2;
	    // Controls mapping of regenerative buffer into address space
	    // 00 => A0000 for 128K
	    // 01 => A0000 for 64K
	    // 10 => B0000 for 32K
	    // 11 => B8000 for 32K
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 7 */
// Color don't care
struct vga_color_dont_care_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t dc_map0:1;
	    uint8_t dc_map1:1;
	    uint8_t dc_map2:1;
	    uint8_t dc_map3:1;
	    // if off then the corresponding map is not
	    // considered in color comparison
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 8 */
typedef uint8_t vga_bit_mask_reg;
// bit high means corresponding bit in each mask
// can be changed  (used for write modes 0 and 2)

/* 
  Attribute Controller Registers

  The attribute controller essentially handles
  attributes in text mode and palletes in graphics mode

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
struct vga_attribute_controller_address_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t index:5;    // actual address
	    uint8_t internal_palette_address_source:1; 
	    // 0 => use the internal color palette (load the regs)
	    // 1 => use the external color palette
	    uint8_t reserved:2; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* 
   Internal Palette Registers

   Index 0..15

   Register k maps attribute k to the palette entry loaded in register k
*/
struct vga_internal_palette_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t palette_data:6;    // which palette entry to use
	    uint8_t reserved:2; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

typedef struct vga_internal_palette_reg vga_internal_palette_regs[16];

/* Index 16 */
struct vga_attribute_mode_control_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t graphics:1;  // graphics versus text mode
	    uint8_t mono_emul:1; // emulate an MDA
	    uint8_t enable_line_graphics_char_code:1;
	    // 1 => enable special line graphics characters
	    //      and force 9th dot to be same as 8th dot of char
	    uint8_t enable_blink;
	    // 1 => MSB of the attribute means blink (8 colors + blink)
	    // 0 => MSB of the attribute means intensity (16 colors)
	    uint8_t reserved:1;
	    uint8_t pixel_panning:1;
	    // if 1, pixel panning reg set to 0 when line compare succeeds
	    uint8_t pixel_width:1;
	    // 1 => 8 bit color (256 colors)
	    uint8_t p54_select:1;
	    // select source of p5 and p6 inputs to DAC
	    // 0 => use internal palette regs
	    // 1 => use color select reg
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
;

/* Index 17 */
// this is the screen border color
typedef uint8_t vga_overscan_color_reg;

/* Index 18 */
// Enable the corresponding display memory color plane
struct vga_color_plane_enable_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t enable_color_plane:4;
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
    ;

/* Index 19 */
// number of pixels to shift the display to the left
struct vga_horizontal_pixel_pan_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t horizontal_pixel_pan:4;
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
    ;

/* Index 20 */
// color select
// These allow for quick switching back and forth between
// color palletes
struct vga_color_select_reg {
    union {
	uint8_t val;
	struct {
 	    uint8_t sc4:1;
 	    uint8_t sc5:1;
 	    uint8_t sc6:1;
 	    uint8_t sc7:1;
	    uint8_t reserved:4; 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed))
    ;

/*
  DAC registers

  Used to derive the ultimate pixel colors that are rendered on the display
  
  There are 256 palette registers
  We can change any one or read any one
*/

//
// The palette register that will returned on a data write
// 0x3c8
typedef uint8_t vga_dac_write_addr_reg;

//
// The palette register that will returned on a data read
// 0x3c7
typedef uint8_t vga_dac_read_addr_reg;

//
// Read or write of a palette register
// 0x3c9
// Successive references to this register increment
// the palette register index (for reads and writes separately)
// Three SUCCESSIVE WRITES ARE EXPECTED to set the 18 bit register
// Three SUCCESSIVE READS ARE EXPECTED to read out the register
// reads or writes are in order RED GREEN BLUE
// ADDRESS REG WRITE always resets
typedef uint8_t vga_dac_data_reg;

//
// Pixel Mask
// 0x3c6
typedef uint8_t vga_dac_pixel_mask_reg;

// Palette register (256 of these)
// strictly speaking, each of these is 18 bits wide, 6 bits per channel
// We will provide reg&0x3f, reg>>8 & 0x3f, etc
// This is red, green, blue
typedef uint32_t vga_palette_reg; 


//
//  What attribute bytes mean in text mode
//
struct vga_attribute_byte {
    union {
	uint8_t val;
	struct {
	    uint8_t fore:3;   //foreground color
	    uint8_t foreground_intensity_or_font_select:1; // depends on char map select reg
	    // character map selection is effected
	    // when memory_mode.extended meomory=1
	    // and the two character map enteries on character_map_select are 
	    // different
	    uint8_t back:3;   //background color
	    uint8_t blinking_or_bg_intensity:1; 
	    // attribute mode control.enableblink = 1 => blink
	    // =0 => intensity (16 colors of bg)
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

#endif
