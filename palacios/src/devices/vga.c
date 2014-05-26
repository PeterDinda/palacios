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
#include <interfaces/vmm_graphics_console.h>

#include "vga_regs.h"

#ifndef V3_CONFIG_DEBUG_VGA
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define DEBUG_MEM_DATA    0
#define DEBUG_DEEP_MEM    0
#define DEBUG_DEEP_RENDER 0


#define MEM_REGION_START 0xa0000
#define MEM_REGION_END   0xc0000
#define MEM_REGION_NUM_PAGES (((MEM_REGION_END)-(MEM_REGION_START))/4096)

#define MAP_SIZE 65536
#define MAP_NUM  4


typedef uint8_t *vga_map; // points to MAP_SIZE data

#define VGA_MAXX 1024
#define VGA_MAXY 768


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
#define VGA_DAC_DATA 0x3c9
#define VGA_DAC_PIXEL_MASK 0x3c6

#define VGA_DAC_NUM_ENTRIES 256


#define VGA_FONT_WIDTH      8
#define VGA_MAX_FONT_HEIGHT 32

struct vga_render_model {
  uint32_t   model;  // composite the following

#define VGA_DRIVEN_PERIODIC_RENDERING 0x1  // render after n GPU updates
#define CONSOLE_ADVISORY_RENDERING    0x2  // ask console if we should render following an update
#define CONSOLE_DRIVEN_RENDERING      0x4  // have console tell us when to render

  uint32_t   updates_before_render;   // defaults to the following

#define DEFAULT_UPDATES_BEFORE_RENDER   1000
};

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
    struct vga_max_row_scan_reg vga_max_row_scan;
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
    vga_vertical_display_enable_end_reg vga_vertical_display_enable_end;
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
    struct vga_color_dont_care_reg vga_color_dont_care;
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

    bool passthrough;
    bool skip_next_passthrough_out; // for word access 

    struct v3_frame_buffer_spec  target_spec;
    v3_graphics_console_t host_cons;
    struct vga_render_model render_model;

    uint32_t updates_since_render;

    struct frame_buf *framebuf; // we render to this
    
    //    void *mem_store;     // This is the region where the memory hooks will go

    vga_map  map[MAP_NUM];  // the maps that the host writes to

    uint8_t  latch[MAP_NUM];  // written to in any read, used during writes

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


typedef enum {PLANAR_SHIFT, PACKED_SHIFT, C256_SHIFT} shift_mode_t;


static void find_text_char_dim(struct vga_internal *vga, uint32_t *w, uint32_t *h)
{
    *w = (vga->vga_sequencer.vga_clocking_mode.dot8 ? 8 : 9);

    *h = vga->vga_crt_controller.vga_max_row_scan.max_scan_line+1;

}

static void find_text_res(struct vga_internal *vga, uint32_t *width, uint32_t *height)
{
    uint32_t vert_lsb, vert_msb;
    uint32_t ph;
    uint32_t ch, cw;

    *width = (vga->vga_crt_controller.vga_horizontal_display_enable_end + 1) 
	- vga->vga_crt_controller.vga_end_horizontal_blanking.display_enable_skew;

    vert_lsb = vga->vga_crt_controller.vga_vertical_display_enable_end; // 8 bits here
    vert_msb = (vga->vga_crt_controller.vga_overflow.vertical_disp_enable_end9 << 1)  // 2 bits here
	+ (vga->vga_crt_controller.vga_overflow.vertical_disp_enable_end8);
	       
    ph  = ( (vert_msb << 8) + vert_lsb + 1) ; // pixels high (scanlines)

    find_text_char_dim(vga,&cw, &ch);

    *height = ph / ch; 

}


static void find_text_data_start(struct vga_internal *vga, void **data)
{
    uint32_t offset;

    offset = vga->vga_crt_controller.vga_start_address_high;
    offset <<= 8;
    offset += vga->vga_crt_controller.vga_start_address_low;

    *data = vga->map[0]+offset;

}


static void find_text_attr_start(struct vga_internal *vga, void **data)
{
    uint32_t offset;

    offset = vga->vga_crt_controller.vga_start_address_high;
    offset <<= 8;
    offset += vga->vga_crt_controller.vga_start_address_low;

    *data = vga->map[1]+offset;

}

static void find_text_cursor_pos(struct vga_internal *vga, uint32_t *x, uint32_t *y, void **data)
{
    uint32_t w,h;
    uint32_t offset;
    uint32_t charsin;
    void *buf;

    find_text_res(vga,&w,&h);

    find_text_data_start(vga,&buf);

    offset = vga->vga_crt_controller.vga_cursor_location_high;
    offset <<= 8;
    offset += vga->vga_crt_controller.vga_cursor_location_low;

    *data = vga->map[0]+offset;

    charsin = (uint32_t)(*data - buf);
    
    *x = charsin % w;
    *y = charsin / w;
        
}


static void find_text_font_start(struct vga_internal *vga, void **data, uint8_t char_map)
{
    uint32_t mapa_offset, mapb_offset;


    switch (char_map) { 
	case 0:
	    mapa_offset = (vga->vga_sequencer.vga_char_map_select.char_map_a_sel_lsb << 1)
			   + vga->vga_sequencer.vga_char_map_select.char_map_a_sel_msb ;
	    *data = vga->map[2] + mapa_offset;
	    break;
	    
	case 1:
	    mapb_offset = (vga->vga_sequencer.vga_char_map_select.char_map_b_sel_lsb << 1)
			   + vga->vga_sequencer.vga_char_map_select.char_map_b_sel_msb ;
	    *data = vga->map[2] + mapb_offset;
	    break;
	default:
	    PrintError(VM_NONE, VCORE_NONE, "vga: unknown char_map given to find_text_font_start\n");
	    break;
	    
    }
}

static int extended_fontset(struct vga_internal *vga)
{
    if (vga->vga_sequencer.vga_mem_mode.extended_memory &&
	! ( (vga->vga_sequencer.vga_char_map_select.char_map_a_sel_lsb 
	     == vga->vga_sequencer.vga_char_map_select.char_map_b_sel_lsb) &&
	    (vga->vga_sequencer.vga_char_map_select.char_map_a_sel_msb 
	     == vga->vga_sequencer.vga_char_map_select.char_map_b_sel_msb))) { 
	return 1;
    } else {
	return 0;
    }

}

static int blinking(struct vga_internal *vga)
{
    return vga->vga_attribute_controller.vga_attribute_mode_control.enable_blink;
}


static void find_graphics_data_starting_offset(struct vga_internal *vga, uint32_t *offset)
{

    *offset = vga->vga_crt_controller.vga_start_address_high;
    *offset <<= 8;
    *offset += vga->vga_crt_controller.vga_start_address_low;
}


static void find_shift_mode(struct vga_internal *vga, shift_mode_t *mode)
{
    if (vga->vga_graphics_controller.vga_graphics_mode.c256) { 
	*mode=C256_SHIFT;
    } else {
	if (vga->vga_graphics_controller.vga_graphics_mode.shift_reg_mode) {
	    *mode=PACKED_SHIFT;
	} else {
	    *mode=PLANAR_SHIFT;
	}
    }
}


static void find_graphics_res(struct vga_internal *vga, uint32_t *width, uint32_t *height)
{
    uint32_t vert_lsb, vert_msb;

    *width = ((vga->vga_crt_controller.vga_horizontal_display_enable_end + 1) 
	      - vga->vga_crt_controller.vga_end_horizontal_blanking.display_enable_skew);

    *width *= (vga->vga_sequencer.vga_clocking_mode.dot8 ? 8 : 9);

    vert_lsb = vga->vga_crt_controller.vga_vertical_display_enable_end; // 8 bits here
    vert_msb = (vga->vga_crt_controller.vga_overflow.vertical_disp_enable_end9 << 1)  // 2 bits here
	+ (vga->vga_crt_controller.vga_overflow.vertical_disp_enable_end8);
	       
    *height  = ( (vert_msb << 8) + vert_lsb + 1) ; // pixels high (scanlines)

    // At this point we have the resolution in dot clocks across and scanlines top-to-bottom
    // This is usually the resolution in pixels, but it can be monkeyed with
    // at least in the following ways

    // vga sequencer dot clock divide by two 
    if (vga->vga_sequencer.vga_clocking_mode.dot_clock) { 
	*width/=2;
	*height/=2;
    }

    // crt_controller.max_row_scan.double_scan => each row twice for 200=>400
    if (vga->vga_crt_controller.vga_max_row_scan.double_scan) { 
	*height/=2;
    }
    
    // crt_controller.crt_mode_control.count_by_two => pixels twice as wide as normal
    if (vga->vga_crt_controller.vga_crt_mode_control.count_by_two) { 
	*width /= 2;
    }

    // crt_controller.crt_mode_control.horizontal_retrace_select => pixels twice as tall as normal
    if (vga->vga_crt_controller.vga_crt_mode_control.horizontal_retrace_select) { 
	*height /= 2;
    }
    
}


static void find_graphics_cursor_pos(struct vga_internal *vga, uint32_t *x, uint32_t *y)
{
    // todo
    *x=*y=0;
}


static void dac_lookup_24bit_color(struct vga_internal *vga,
				   uint8_t entry,
				   uint8_t *red,
				   uint8_t *green,
				   uint8_t *blue)
{
    // use internal or external palette?

    vga_palette_reg *r = &(vga->vga_dac.vga_dac_palette[entry]);

    // converting from 6 bits to 8 bits so << 2
    *red = (*r & 0x3f) << 2;
    *green = ((*r >> 8) & 0x3f) << 2;
    *blue = ((*r >> 16) & 0x3f) << 2;

}


/*
  Colors work like this:

  4 bit modes:   index is to the internal palette on the attribute controller
                 that supplies 6 bits, but we need 8 to index the dac
		 2 more (the msbs) are supplied from the color select register
                 we can optionally overwrite bits 5 and 4 from the color
		 select register as well, depending on a selection bit
		 in the mode control register.   The result of all this is
		 8 bit index for the dac

  8 bit modes:   the attribute controller passes the index straight through
                 to the DAC.


  The DAC translates from the 8 bit index into 6 bits per color channel
  (18 bit color).   We mulitply by 4 to get 24 bit color.
*/

static void find_24bit_color(struct vga_internal *vga, 
			     uint8_t val,
			     uint8_t *red,
			     uint8_t *green,
			     uint8_t *blue)
{
    uint8_t di;  // ultimate dac index

    if (vga->vga_attribute_controller.vga_attribute_mode_control.pixel_width) { 
	// 8 bit mode does right to the DAC
	di=val;
    } else {
	struct vga_internal_palette_reg pr = vga->vga_attribute_controller.vga_internal_palette[val%16];
	di = pr.palette_data;
	
	// Fix bits 5-4 if needed
	if (vga->vga_attribute_controller.vga_attribute_mode_control.p54_select) { 
	    di &= ~0x30;  // clear 5-4
 	    di |= vga->vga_attribute_controller.vga_color_select.sc4 << 4;
 	    di |= vga->vga_attribute_controller.vga_color_select.sc5 << 5;
	}

	// We must always produce bits 6 and 7
	di &= ~0xc0; // clear 7-6
	di |= vga->vga_attribute_controller.vga_color_select.sc6 << 6;
	di |= vga->vga_attribute_controller.vga_color_select.sc7 << 7;
    }
	
    dac_lookup_24bit_color(vga,di,red,green,blue);
}
	
static void render_graphics(struct vga_internal *vga, void *fb)
{

    struct v3_frame_buffer_spec *spec = &(vga->target_spec);

    uint32_t gw, gh; // graphics w/h
    uint32_t fw, fh; // fb w/h
    uint32_t rgw, rgh;  // region we can actually show on the frame buffer
    

    uint32_t fx, fy;     // pixel position within the frame buffer
    
    uint32_t offset;     // offset into the maps
    uint8_t  m;        // map
    uint8_t  p;          // pixel in the current map byte  (0..7)

    uint8_t r,g,b;  // looked up colors for entry

    void    *pixel;   // current pixel in the fb
    uint8_t *red;     // and the channels in the pixel
    uint8_t *green;   //
    uint8_t *blue;    //

    uint8_t db[4]; // 4 bytes read at a time
    uint8_t pb[8]; // 8 pixels assembled at a time

    shift_mode_t sm;   // shift mode

    uint32_t cur_x, cur_y;
    

    find_graphics_res(vga,&gw,&gh);

    find_shift_mode(vga,&sm);

    find_graphics_cursor_pos(vga,&cur_x,&cur_y);

    find_graphics_data_starting_offset(vga,&offset);

    fw = spec->width;
    fh = spec->height;


    PrintDebug(VM_NONE, VCORE_NONE, "vga: attempting graphics render (%s): graphics_res=(%u,%u), fb_res=(%u,%u), "
               "fb=0x%p offset=0x%x\n",
	       sm == PLANAR_SHIFT ? "planar shift" : 
	       sm == PACKED_SHIFT ? "packed shift" : 
	       sm == C256_SHIFT ? "color256 shift" : "UNKNOWN",
	       gw,gh,fw,fh,fb,offset);

    // First we need to clip to what we can actually show
    rgw = gw < fw ? gw : fw;
    rgh = gh < fh ? gh : fh;

    if (gw%8) { 
	PrintError(VM_NONE, VCORE_NONE, "vga: warning: graphics width is not a multiple of 8\n");
    }



    // Now we scan across by row
    for (fy=0;fy<gh;fy++) { 
	// by column
	for (fx=0;fx<gw;
	     fx += (sm==C256_SHIFT ? 4 : 8) , offset++ ) { 

	    // if any of these pixels are in the rendger region
	    if (fy < rgh && fx < rgw) {
		// assemble all 4 or 8 pixels
		
		// fetch the data bytes
		for (m=0;m<4;m++) { 
		    db[m]=*((uint8_t*)(vga->map[m]+offset));
		}
		 
		// assemble
		switch (sm) { 
		    case PLANAR_SHIFT:
			for (p=0;p<8;p++) { 
			    pb[p]= 
				(( db[0] >> 7) & 0x1) |
				(( db[1] >> 6) & 0x2) |
				(( db[2] >> 5) & 0x4) |
				(( db[3] >> 4) & 0x8) ;
			    
			    for (m=0;m<4;m++) { 
				db[m] <<= 1;
			    }
			}
			break;
			
		    case PACKED_SHIFT:
			// first 4 pixels use planes 0 and 2
			for (p=0;p<4;p++) { 
			    pb[p] = 
				((db[2] >> 4) & 0xc) |
				((db[0] >> 6) & 0x3) ;
			    db[2] <<= 2;
			    db[0] <<= 2;
			}
			break;
			
			// next 4 pixels use planes 1 and 3
			for (p=4;p<8;p++) { 
			    pb[p] = 
				((db[3] >> 4) & 0xc) |
				((db[1] >> 6) & 0x3) ;
			    db[3] <<= 2;
			    db[1] <<= 2;
			}
			break;

		    case C256_SHIFT:
			// this one is either very bizarre or as simple as this
			for (p=0;p<4;p++) { 
			    pb[p] = db[p];
			}
			break;
		}

		// draw each pixel
		for (p=0;p< (sm==C256_SHIFT ? 4 : 8);p++) { 
		    
		    // find its color
		    find_24bit_color(vga,pb[p],&r,&g,&b);
		
		    // find its position in the framebuffer;
		    pixel =  fb + (((fx + p) + (fy*spec->width)) * spec->bytes_per_pixel);
		    red = pixel + spec->red_offset;
		    green = pixel + spec->green_offset;
		    blue = pixel + spec->blue_offset;

		    // draw it
		    *red=r;
		    *green=g;
		    *blue=b;
		}
	    }
	}
    }
    
    PrintDebug(VM_NONE, VCORE_NONE, "vga: render done\n");
}


static void render_text_cursor(struct vga_internal *vga, void *fb)
{
}




//
// A variant of this function could render to
// a text console interface as well
//
static void render_text(struct vga_internal *vga, void *fb)
{
    // line graphics enable bit means to  dupe column 8 to 9 when
    // in 9 dot wide mode
    // otherwise 9th dot is background

    struct v3_frame_buffer_spec *spec = &(vga->target_spec);

    uint32_t gw, gh; // graphics w/h
    uint32_t tw, th; // text w/h
    uint32_t rtw, rth; // rendered text w/h
    uint32_t cw, ch; // char font w/h including 8/9
    uint32_t fw, fh; // fb w/h

    uint32_t px, py; // cursor position
    
    uint32_t x, y, l, p; // text location, line and pixel within the char
    uint32_t fx, fy;     // pixel position within the frame buffer
    
    uint8_t *text_start; 
    uint8_t *text;    // points to current char
    uint8_t *attr;    // and its matching attribute
    uint8_t *curs;    // to where the cursor is
    uint8_t *font;    // to where the current font is

    uint8_t fg_entry; // foreground color entry
    uint8_t bg_entry; // background color entry
    uint8_t fgr,fgg,fgb;    // looked up  foreground colors
    uint8_t bgr,bgg,bgb;    // looked up  bg colors

    uint8_t ct, ca;   // the current char and attribute
    struct vga_attribute_byte a; // decoded attribute

    void *pixel;      // current pixel in the fb
    uint8_t *red;     // and the channels in the pixel
    uint8_t *green;   //
    uint8_t *blue;    //

    

    find_graphics_res(vga,&gw,&gh);
    find_text_res(vga,&tw,&th);
    find_text_char_dim(vga,&cw,&ch);
    fw = spec->width;
    fh = spec->height;

    find_text_cursor_pos(vga,&px,&py,(void**)&curs);
    find_text_data_start(vga,(void**)&text);
    find_text_attr_start(vga,(void**)&attr);

    find_text_font_start(vga,(void**)&font,0); // will need to switch this as we go since it is part of attr

    PrintDebug(VM_NONE, VCORE_NONE, "vga: attempting text render: graphics_res=(%u,%u), fb_res=(%u,%u), text_res=(%u,%u), "
	       "char_res=(%u,%u), cursor=(%u,%u) font=0x%p, text=0x%p, attr=0x%p, curs=0x%p, fb=0x%p"
	       "graphics extension=%u, extended_fontset=%d, blinking=%d\n",
	       gw,gh,fw,fh,tw,th,cw,ch,px,py,font,text,attr,curs,fb,
	       vga->vga_attribute_controller.vga_attribute_mode_control.enable_line_graphics_char_code,
	       extended_fontset(vga), blinking(vga));

    text_start=text;

    // First we need to clip to what we can actually show
    rtw = tw < fw/cw ? tw : fw/cw;
    rth = th < fh/ch ? th : fh/ch;

    PrintDebug(VM_NONE, VCORE_NONE, "\n");

    // Now let's scan by char across the whole thing
    for (y=0;y<th;y++) { 
	for (x=0;x<tw;x++, text++, attr++) { 
	    if (x < rtw && y < rth) { 
		// grab the character and attribute for the position
		ct = *text; 
		ca = *attr;  
		a.val = ca;

		PrintDebug(VM_NONE, VCORE_NONE, "%c",ct);

		// find the character's font bitmap (one byte per row)
		find_text_font_start(vga,(void**)&font,
				     extended_fontset(vga) ? a.foreground_intensity_or_font_select : 0 ); 

		font += ct * ((VGA_MAX_FONT_HEIGHT * VGA_FONT_WIDTH)/8);

		// Now let's find out what colors we will be using
		// foreground
		
		if (!extended_fontset(vga)) { 
		    fg_entry = a.foreground_intensity_or_font_select << 3;
		} else {
		    fg_entry = 0;
		}
		fg_entry |= a.fore;

		find_24bit_color(vga,fg_entry,&fgr,&fgg,&fgb);

		if (!blinking(vga)) { 
		    bg_entry = a.blinking_or_bg_intensity << 3;
		} else {
		    bg_entry = 0;
		}
		bg_entry |= a.back;
		
		find_24bit_color(vga,bg_entry,&bgr,&bgg,&bgb);

		// Draw the character
		for (l=0; l<ch; l++, font++) {
		    uint8_t frow = *font;  // this is the row of of the font map
		    for (p=0;p<cw;p++) {
			uint8_t fbit;
			
			// a char can be 9 bits wide, but the font map
			// is only 8 bits wide, which means we need to know where to
			// get the 9th bit
			if (p >= 8) { 
			    // We get it from the font map if
			    // its line line graphics mode and its a graphics char
			    // otherwise it's the background color
			    if (vga->vga_attribute_controller.vga_attribute_mode_control.enable_line_graphics_char_code
				&& ct>=0xc0 && ct<=0xdf ) { 
				fbit = frow & 0x1;
			    } else {
				fbit = 0;
			    }
			} else {
			    fbit= (frow >> (7-p) ) & 0x1;
			}
			
			// We are now at the pixel level, with fbit being the pixel we draw (color+attr or bg+attr)
			// For now, we will draw it as black/white

			// find its position in the framebuffer;
			fx = x*cw + p;
			fy = y*ch + l;
			pixel =  fb + ((fx + (fy*spec->width)) * spec->bytes_per_pixel);
			red = pixel + spec->red_offset;
			green = pixel + spec->green_offset;
			blue = pixel + spec->blue_offset;

			// Are we on the cursor?
			// if so, let's negate this pixel to invert the cell
			if (curs==text) { 
			    fbit^=0x1;
			}
			// update the framebuffer
			if (fbit) { 
			    *red=fgr; 
			    *green=fgg;
			    *blue=fgb;
			} else {
			    *red=bgr;
			    *green=bgg;
			    *blue=bgb;
			}
		    }
		}
	    }
	}
	PrintDebug(VM_NONE, VCORE_NONE, "\n");
    }

}

			
			


static void render_test(struct vga_internal *vga, void *fb)
{
    struct v3_frame_buffer_spec *s;

    s=&(vga->target_spec);

    if (fb && s->height>=480 && s->width>=640 ) { 
	uint8_t color = (uint8_t)(vga->updates_since_render);
	
	uint32_t x, y;
	
	for (y=0;y<480;y++) {
	    for (x=0;x<640;x++) { 
		void *pixel = fb + ((x + (y*s->width)) * s->bytes_per_pixel);
		uint8_t *red = pixel + s->red_offset;
		uint8_t *green = pixel + s->green_offset;
		uint8_t *blue = pixel + s->blue_offset;
		
		if (y<(480/4)) { 
		    *red=color+x;
		    *green=0;
		    *blue=0;
		} else if (y<(480/2)) { 
		    *red=0;
		    *green=color+x;
		    *blue=0;
		} else if (y<(3*(480/4))) { 
		    *red=0;
		    *green=0;
		    *blue=color+x;
		} else {
		    *red=*green=*blue=color+x;
		}
	    }
	}
    }
}

static void render_black(struct vga_internal *vga, void *fb)
{
    struct v3_frame_buffer_spec *s;

    s=&(vga->target_spec);

    memset(fb,0,s->height*s->width*s->bytes_per_pixel);
}

static void render_maps(struct vga_internal *vga, void *fb)
{

    struct v3_frame_buffer_spec *s;
    

    s=&(vga->target_spec);

    if (fb && s->height>=768 && s->width>=1024 ) { 
	// we draw the maps next, each being a 256x256 block appearing 32 pixels below the display block
	uint8_t m;
	uint32_t x,y;
	uint8_t *b;
	
	for (m=0;m<4;m++) { 
	    b=(vga->map[m]);
	    for (y=480+32;y<768;y++) { 
		for (x=m*256;x<(m+1)*256;x++,b++) { 
		    void *pixel = fb + ((x + (y*s->width)) * s->bytes_per_pixel);
		    uint8_t *red = pixel + s->red_offset;
		    uint8_t *green = pixel + s->green_offset;
		    uint8_t *blue = pixel + s->blue_offset;
		    
		    *red=*green=*blue=*b;
		}
	    }
	}
    }
}

static int render_core(struct vga_internal *vga)
{
  void *fb;

  PrintDebug(VM_NONE, VCORE_NONE, "vga: render on update %u\n",vga->updates_since_render);
  
  fb = v3_graphics_console_get_frame_buffer_data_rw(vga->host_cons,&(vga->target_spec));
  
  if (!(vga->vga_sequencer.vga_clocking_mode.screen_off)) {
    if (vga->vga_attribute_controller.vga_attribute_mode_control.graphics) { 
      render_graphics(vga,fb);
    } else {
	  render_text(vga,fb);
	  render_text_cursor(vga,fb);
	}
  } else {
    render_black(vga,fb);
  }
  
  if (0) { render_test(vga,fb); }
  
  // always render maps for now 
  render_maps(vga,fb);
  
  v3_graphics_console_release_frame_buffer_data_rw(vga->host_cons);
  
  vga->updates_since_render=0;
  
  return 0;

}
    

static int render(struct vga_internal *vga)
{

    vga->updates_since_render++;


    if (vga->host_cons) { 
      int do_render=0;
      
      if ((vga->render_model.model & VGA_DRIVEN_PERIODIC_RENDERING)
	  &&
	  (vga->updates_since_render > vga->render_model.updates_before_render)) {
	PrintDebug(VM_NONE, VCORE_NONE, "vga: render due to periodic\n");
	
	do_render = 1;
      }
      
      if ((vga->render_model.model & CONSOLE_ADVISORY_RENDERING) 
	  &&
	  (v3_graphics_console_inform_update(vga->host_cons) > 0) ) { 

        PrintDebug(VM_NONE, VCORE_NONE, "vga: render due to advisory\n");

	do_render = 1;
      }

      // note that CONSOLE_DRIVEN_RENDERING is handled via the render_callback() function
      
      if (do_render) { 
	return render_core(vga);
      } else {
	return 0;
      }
    }
    return 0;
    
}


static int render_callback(v3_graphics_console_t cons,
			   void *priv)
{
  struct vga_internal *vga = (struct vga_internal *) priv;
  
  PrintDebug(VM_NONE, VCORE_NONE, "vga: render due to callback\n");

  return render_core(vga);
}

static int update_callback(v3_graphics_console_t cons,
			   void *priv)
{
  struct vga_internal *vga = (struct vga_internal *) priv;
  
  return vga->updates_since_render>0;
}



static void get_mem_region(struct vga_internal *vga, uint64_t *mem_start, uint64_t *mem_end) 
{
    switch (vga->vga_graphics_controller.vga_misc.memory_map) { 
	case 0: 
	    *mem_start=0xa0000;
	    *mem_end=0xc0000;
	    break;
	case 1:
	    *mem_start=0xa0000;
	    *mem_end=0xb0000;
	    break;
	case 2:
	    *mem_start=0xb0000;
	    *mem_end=0xb8000;
	    break;
	case 3:
	    *mem_start=0xb8000;
	    *mem_end=0xc0000;
	    break;
    }
}

static uint64_t find_offset_write(struct vga_internal *vga, addr_t guest_addr)
{
    uint64_t mem_start, mem_end;
    uint64_t size;
    
    mem_start=mem_end=0;
    
    get_mem_region(vga, &mem_start, &mem_end);
    
    size=(mem_end-mem_start > 65536 ? 65536 : (mem_end-mem_start)); 

    if (vga->vga_sequencer.vga_mem_mode.odd_even) { 
      // NOT odd/even mode
      return (guest_addr-mem_start) % size;
    } else {
	// odd/even mode
      return ((guest_addr-mem_start) >> 1 ) % size;
    }
}



    
// Determines which maps should be enabled for this single byte write
// and what the increment (actually 1/increment for the copy loop
//
//  memory_mode.odd_even == 0 => even address = maps 0 and 2 enabled; 1,3 otherwise
//  
static uint8_t find_map_write(struct vga_internal *vga, addr_t guest_addr)
{
    uint8_t mm = vga->vga_sequencer.vga_map_mask.val;

    if (vga->vga_sequencer.vga_mem_mode.odd_even) { 
      // NOT odd/even mode
      return mm;
    } else {
      // odd/even mode
      if (guest_addr & 0x1) { 
	return mm & 0xa;  // 0x1010
      } else {
	return mm & 0x5;  // 0x0101
      }
    }
}

static uint8_t find_increment_write(struct vga_internal *vga, addr_t new_guest_addr)
{
  if (vga->vga_sequencer.vga_mem_mode.odd_even) { 
    // NOT odd/even mode
    return 1;
  } else {
    return !(new_guest_addr & 0x1);
  }
}



static int vga_write(struct guest_info * core, 
		     addr_t guest_addr, 
		     void * src, 
		     uint_t length, 
		     void * priv_data)
{
    struct vm_device *dev = (struct vm_device *)priv_data;
    struct vga_internal *vga = (struct vga_internal *) dev->private_data;

    PrintDebug(core->vm_info, core, "vga: memory write: guest_addr=0x%p len=%u write_mode=%d first_byte=0x%x\n",
	       (void*)guest_addr, length, vga->vga_graphics_controller.vga_graphics_mode.write_mode, *(uint8_t*)src);

    if (vga->passthrough) { 
	PrintDebug(core->vm_info, core, "vga: passthrough write to 0x%p\n", V3_VAddr((void*)guest_addr));
	memcpy(V3_VAddr((void*)guest_addr),src,length);
    }
    
#if DEBUG_MEM_DATA
    int i;
    PrintDebug(core->vm_info, core, "vga: data written was 0x");
    for (i=0;i<length;i++) {
	uint8_t c= ((char*)src)[i];
	PrintDebug(core->vm_info, core, "%.2x", c);
    }
    PrintDebug(core->vm_info, core, " \"");
    for (i=0;i<length;i++) {
	char c= ((char*)src)[i];
	PrintDebug(core->vm_info, core, "%c", (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || (c==' ') ? c : '.');
    }
    PrintDebug(core->vm_info, core, "\"\n");
#endif

    /* Write mode determine by Graphics Mode Register (Index 05h).writemode */


    // probably could just reduce this to the datapath itself instead
    // of following the programmer's perspective... 

    switch (vga->vga_graphics_controller.vga_graphics_mode.write_mode) {
	case 0: {
	    
	    /* 
	       00b -- Write Mode 0: In this mode, the host data is first rotated 
	       as per the Rotate Count field, then the Enable Set/Reset mechanism 
	       selects data from this or the Set/Reset field. Then the selected 
	       Logical Operation is performed on the resulting data and the data 
	       in the latch register. Then the Bit Mask field is used to select 
	       which bits come from the resulting data and which come 
	       from the latch register. Finally, only the bit planes enabled by 
	       the Memory Plane Write Enable field are written to memory.
	    */

	    int i;

	    uint8_t  mapnum;
	    uint64_t offset;

	    uint8_t ror = vga->vga_graphics_controller.vga_data_rotate.rotate_count;
	    uint8_t func = vga->vga_graphics_controller.vga_data_rotate.function;
	    
	    offset = find_offset_write(vga, guest_addr);

#if DEBUG_DEEP_MEM
	    PrintDebug(core->vm_info, core, "vga: mode 0 write, offset=0x%llx, ror=%u, func=%u\n", offset,ror,func);
#endif

	    for (i=0;i<length;i++,offset+=find_increment_write(vga,guest_addr+i)) { 
		// now for each map
		uint8_t sr = vga->vga_graphics_controller.vga_set_reset.val & 0xf;
		uint8_t esr = vga->vga_graphics_controller.vga_enable_set_reset.val &0xf;
		uint8_t bm = vga->vga_graphics_controller.vga_bit_mask;
		uint8_t mm = find_map_write(vga,guest_addr+i);

#if DEBUG_DEEP_MEM
		PrintDebug(core->vm_info, core, "vga: write i=%u, mm=0x%x, bm=0x%x sr=0x%x esr=0x%x offset=0x%x\n",i,(unsigned int)mm,(unsigned int)bm, (unsigned int)sr, (unsigned int)esr,(unsigned int)offset);
#endif

		for (mapnum=0;mapnum<4;mapnum++, sr>>=1, esr>>=1, mm>>=1) { 
		    vga_map map = vga->map[mapnum];
		    uint8_t data = ((uint8_t *)src)[i];
		    uint8_t latchval = vga->latch[mapnum];
		    
#if DEBUG_DEEP_MEM
		    PrintDebug(core->vm_info, core, "vga: raw data=0x%x\n",data);
#endif
		    // rotate data right
		    if (ror) { 
			data = (data>>ror) | (data<<(8-ror));
		    }
		    
#if DEBUG_DEEP_MEM
		    PrintDebug(core->vm_info, core, "vga: data after ror=0x%x\n",data);
#endif
		    // use SR bit if ESR is on for this map
		    if (esr & 0x1) {
			data = (sr&0x1) * -1;
			
		    }
		    
#if DEBUG_DEEP_MEM
		    PrintDebug(core->vm_info, core, "vga: data after esrr=0x%x\n",data);
#endif
		    
		    // Apply function
		    switch (func) { 
			case 0: // NOP
			    break;
			case 1: // AND
			    data &= latchval;
			    break;
			case 2: // OR
			    data |= latchval;
			    break;
			case 3: // XOR
			    data ^= latchval;
			    break;
		    }
		    
#if DEBUG_DEEP_MEM
		    PrintDebug(core->vm_info, core, "vga: data after func=0x%x\n",data);
#endif
		    
		    // mux between the data byte and the latch byte on
		    // a per-bit basis
		    data = (bm & data) | ((~bm) & latchval);
		    

#if DEBUG_DEEP_MEM
		    PrintDebug(core->vm_info, core, "vga: data after bm mux=0x%x\n",data);
#endif
		    
		    // selective write
		    if (mm & 0x1) { 
			// write to this map
#if DEBUG_DEEP_MEM
			PrintDebug(core->vm_info, core, "vga: write map %u offset 0x%p map=0x%p pointer=0x%p\n",mapnum,(void*)offset,map,&(map[offset]));
#endif
			map[offset] = data;
		    } else {
			// skip this map
		    }
		}
	    }
	}
	    break;


	    
	case 1: {
	    /* 
	       01b -- Write Mode 1: In this mode, data is transferred directly 
	       from the 32 bit latch register to display memory, affected only by 
	       the Memory Plane Write Enable field. The host data is not used in this mode.
	    */

	    int i;

	    uint64_t offset = find_offset_write(vga,guest_addr);

#if DEBUG_DEEP_MEM
	    PrintDebug(core->vm_info, core, "vga: mode 1 write, offset=0x%llx\n", offset);
#endif

	    for (i=0;i<length;i++,offset+=find_increment_write(vga,guest_addr+i)) { 

		uint8_t mapnum;
		uint8_t mm = find_map_write(vga,guest_addr+i);

		for (mapnum=0;mapnum<4;mapnum++,  mm>>=1) { 
		    vga_map map = vga->map[mapnum];
		    uint8_t latchval = vga->latch[mapnum];
			
		    // selective write
		    if (mm & 0x1) { 
			// write to this map
			map[offset] = latchval;
		    } else {
			// skip this map
		    }
		}
	    }
	}
	    break;

	case 2: {
	    /*
	      10b -- Write Mode 2: In this mode, the bits 3-0 of the host data 
	      are replicated across all 8 bits of their respective planes. 
	      Then the selected Logical Operation is performed on the resulting 
	      data and the data in the latch register. Then the Bit Mask field is used to 
	      select which bits come from the resulting data and which come from 
	      the latch register. Finally, only the bit planes enabled by the 
	      Memory Plane Write Enable field are written to memory.
	    */
	    int i;
	    uint8_t  mapnum;
	    uint64_t offset;

	    uint8_t func = vga->vga_graphics_controller.vga_data_rotate.function;
	    
	    offset = find_offset_write(vga, guest_addr);

#if DEBUG_DEEP_MEM
	    PrintDebug(core->vm_info, core, "vga: mode 2 write, offset=0x%llx, func=%u\n", offset,func);
#endif

	    for (i=0;i<length;i++,offset+=find_increment_write(vga,guest_addr+i)) { 
		// now for each map
		uint8_t bm = vga->vga_graphics_controller.vga_bit_mask;
		uint8_t mm = find_map_write(vga,guest_addr+i);

		for (mapnum=0;mapnum<4;mapnum++,  mm>>=1) { 
		    vga_map map = vga->map[mapnum];
		    uint8_t data = ((uint8_t *)src)[i];
		    uint8_t latchval = vga->latch[mapnum];
			
		    // expand relevant bit to 8 bit
		    // it's basically esr=1, sr=bit from mode 0 write
		    data = ((data>>mapnum)&0x1) * -1;
			
		    // Apply function
		    switch (func) { 
			case 0: // NOP
			    break;
			case 1: // AND
			    data &= latchval;
			    break;
			case 2: // OR
			    data |= latchval;
			    break;
			case 3: // XOR
			    data ^= latchval;
			    break;
		    }

		    // mux between latch and alu output
		    data = (bm & data) | ((~bm) & latchval);
		    
		    // selective write
		    if (mm & 0x1) { 
			// write to this map
			map[offset] = data;
		    } else {
			// skip this map
		    }
		}
	    }
	}
	    break;

	case 3: {
	    /* 11b -- Write Mode 3: In this mode, the data in the Set/Reset field is used 
	       as if the Enable Set/Reset field were set to 1111b. Then the host data is 
	       first rotated as per the Rotate Count field, then logical ANDed with the 
	       value of the Bit Mask field. The resulting value is used on the data 
	       obtained from the Set/Reset field in the same way that the Bit Mask field 
	       would ordinarily be used. to select which bits come from the expansion 
	       of the Set/Reset field and which come from the latch register. Finally, 
	       only the bit planes enabled by the Memory Plane Write Enable field 
	       are written to memory.
	    */
	    int i;

	    uint8_t  mapnum;
	    uint64_t offset;

	    uint8_t ror = vga->vga_graphics_controller.vga_data_rotate.rotate_count;
	    
	    offset = find_offset_write(vga, guest_addr);

	    PrintDebug(core->vm_info, core, "vga: mode 3 write, offset=0x%llx, ror=%u\n", offset,ror);

	    for (i=0;i<length;i++,offset+=find_increment_write(vga,guest_addr+i)) { 
		// now for each map
		uint8_t data = ((uint8_t *)src)[i];

		if (ror) {
		    data = (data>>ror) | (data<<(8-ror));
		}

		// Note here that the bitmask is the register AND the data
		// the data written by the system is used for no other purpose
		uint8_t bm = vga->vga_graphics_controller.vga_bit_mask & data;

		uint8_t sr = vga->vga_graphics_controller.vga_set_reset.val & 0xf;

		uint8_t mm = find_map_write(vga,guest_addr+i);

		for (mapnum=0;mapnum<4;mapnum++, sr>>=1, mm>>=1) { 
		    vga_map map = vga->map[mapnum];
		    uint8_t latchval = vga->latch[mapnum];
			
		    // expand SR bit - that's the data we're going to use
		    data = (sr&0x1) * -1;

		    // mux between latch and alu output
		    data = (bm & data) | ((~bm) & latchval);
		    
		    // selective write
		    if (mm & 0x1) { 
			// write to this map
			map[offset] = data;
		    } else {
			// skip this map
		    }
		}
	    }

	}
	    break;

	    // There is no default
    }

    render(vga);
    
    return length;
}



static uint64_t find_offset_read(struct vga_internal *vga, addr_t guest_addr)
{
    uint64_t mem_start, mem_end;
    uint64_t size;
    
    mem_start=mem_end=0;
    
    get_mem_region(vga, &mem_start, &mem_end);
    
    size=(mem_end-mem_start > 65536 ? 65536 : (mem_end-mem_start)); 

    if (!vga->vga_sequencer.vga_mem_mode.odd_even) { 
      // odd/even mode takes priority
      return ((guest_addr-mem_start) >> 1 ) % size;
    } 

    if (vga->vga_sequencer.vga_mem_mode.chain4) {
      // otherwise chain4 if it's on
      return ((guest_addr - mem_start) >> 2) % size;
    }

    // and what you would expect if neither are on
    return (guest_addr-mem_start) % size;
}

// Given this address
// which specific map should we write into?
// Note that unlike with find_map_write, here we are looking
// for a single map number, not a bit vector of maps to be selected
static uint8_t find_map_read(struct vga_internal *vga, addr_t guest_addr)
{
    uint64_t mem_start, mem_end;
    
    mem_start=mem_end=0;
    
    get_mem_region(vga, &mem_start, &mem_end);
    
    if (!vga->vga_sequencer.vga_mem_mode.odd_even) { 
      // odd-even mode
      // last bit tells us map 0 or 1
      return (guest_addr-mem_start) & 0x1; 
    } 

    if (vga->vga_sequencer.vga_mem_mode.chain4) {
      // otherwise chain4 if it's on
      // last two bits
      return (guest_addr - mem_start) & 0x3;
    }

    // and what you would expect if neither are on
    // note that it's not the same as a write!
    return vga->vga_graphics_controller.vga_read_map_select.map_select;
}

static uint8_t find_increment_read(struct vga_internal *vga, addr_t new_guest_addr)
{

  if (!vga->vga_sequencer.vga_mem_mode.odd_even) { 
    // odd-even mode
    return !(new_guest_addr & 0x1); 
  } 

  if (vga->vga_sequencer.vga_mem_mode.chain4) { 
    return !(new_guest_addr & 0x3);
  } 
  return 1;
}


static int vga_read(struct guest_info * core, 
		    addr_t guest_addr, 
		    void * dst, 
		    uint_t length, 
		    void * priv_data)
{
    struct vm_device *dev = (struct vm_device *)priv_data;
    struct vga_internal *vga = (struct vga_internal *) dev->private_data;
    

    PrintDebug(core->vm_info, core, "vga: memory read: guest_addr=0x%p len=%u read_mode=%d\n",(void*)guest_addr, length,
	       vga->vga_graphics_controller.vga_graphics_mode.read_mode);

        
   
    /*
      Reading, 2 modes, set via Graphics Mode Register (index 05h).Read Mode:
    */
    switch (vga->vga_graphics_controller.vga_graphics_mode.read_mode) { 
	case 0: {
	    /*      0 - a byte from ONE of the 4 planes is returned; 
		    which plane is determined by Read Map Select (Read Map Select Register (Index 04h))
	            OR by odd/even chaining OR by chain4 chaining
	    */
	    uint8_t  mapnum;
	    uint64_t offset;
	    uint32_t i;

	    offset = find_offset_read(vga,guest_addr);
	    
#if DEBUG_DEEP_MEM
	    PrintDebug(core->vm_info, core, "vga: mode 0 read, offset=0x%llx\n",offset);
#endif
	    for (i=0;i<length;i++,offset+=find_increment_read(vga,guest_addr+i)) { 

	      mapnum = find_map_read(vga,guest_addr+i);

	      // the data returned
	      ((uint8_t*)dst)[i] = *(vga->map[mapnum]+offset);
	      
	      // need to load all latches even though we are 
	      // returning data from only the selected map
	      for (mapnum=0;mapnum<4;mapnum++) { 
		vga->latch[mapnum] = *(vga->map[mapnum]+offset);
	      }

	    }
	
	}
	    break;
	case 1: {
	    /*      1 - Compare video memory and reference color 
		    (in Color Compare, except those not set in Color Don't Care), 
		    each bit in returned result is one comparison between the reference color 

		    Ref color is *4* bits, and thus a one byte read returns a comparison 
		    with 8 pixels

	    */
	    int i;
	    uint64_t offset;
 	    
	    offset = find_offset_read(vga,guest_addr);

#if DEBUG_DEEP_MEM
	    PrintDebug(core->vm_info, core, "vga: mode 1 read, offset=0x%llx\n",offset);
#endif
		
	    for (i=0;i<length;i++,offset+=find_increment_read(vga,guest_addr+i)) { 
	      uint8_t mapnum;
	      uint8_t mapcalc=0xff;
	      
	      uint8_t cc=vga->vga_graphics_controller.vga_color_compare.val & 0xf ;
	      uint8_t dc=vga->vga_graphics_controller.vga_color_dont_care.val & 0xf;
	      
	      for (mapnum=0;mapnum<4;mapnum++, cc>>=1, dc>>=1) { 
		if (dc&0x1) {  // dc is active low; 1=we do care
		  mapcalc &= (cc * -1) & *(vga->map[mapnum]+offset);
		}
		// do latch load
		vga->latch[mapnum] = *(vga->map[mapnum]+offset);
	      }
	      // write back the comparison result (for 8 pixels)
	      ((uint8_t *)dst)[i]=mapcalc;
	    }

	}

	    break;
	    // there is no default
    }

    if (vga->passthrough) { 
	PrintDebug(core->vm_info, core, "vga: passthrough read from 0x%p\n",V3_VAddr((void*)guest_addr));
	memcpy(dst,V3_VAddr((void*)guest_addr),length);
    }


#if DEBUG_MEM_DATA
    int i;
    PrintDebug(core->vm_info, core, "vga: data read is 0x");
    for (i=0;i<length;i++) {
	uint8_t c= ((char*)dst)[i];
	PrintDebug(core->vm_info, core, "%.2x", c);
    }
    PrintDebug(core->vm_info, core, " \"");
    for (i=0;i<length;i++) {
	char c= ((char*)dst)[i];
	PrintDebug(core->vm_info, core, "%c", (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || (c==' ') ? c : '.');
    }
    PrintDebug(core->vm_info, core, "\"\n");
#endif

    return length;

}





#define ERR_WRONG_SIZE(op,reg,len,min,max)	\
    if (((len)<(min)) || ((len)>(max))) {       \
	PrintError(core->vm_info, core, "vga: %s of %s wrong size (%d bytes, but only %d to %d allowed)\n",(op),(reg),(len),(min),(max)); \
	return -1; \
}
	
static inline void passthrough_io_in(uint16_t port, void * dest, uint_t length) {
    switch (length) {
	case 1:
	    *(uint8_t *)dest = v3_inb(port);
	    break;
	case 2:
	    *(uint16_t *)dest = v3_inw(port);
	    break;
	case 4:
	    *(uint32_t *)dest = v3_indw(port);
	    break;
	default:
	    PrintError(VM_NONE, VCORE_NONE, "vga: unsupported passthrough io in size %u\n",length);
	    break;
    }
}


static inline void passthrough_io_out(uint16_t port, const void * src, uint_t length) {
    switch (length) {
	case 1:
	    v3_outb(port, *(uint8_t *)src);
	    break;
	case 2:
	    v3_outw(port, *(uint16_t *)src);
	    break;
	case 4:
	    v3_outdw(port, *(uint32_t *)src);
	    break;
	default:
	    PrintError(VM_NONE, VCORE_NONE, "vga: unsupported passthrough io out size %u\n",length);
	    break;
    }
}

#define PASSTHROUGH_IO_IN(vga,port,dest,len)				\
    do { if ((vga)->passthrough) { passthrough_io_in(port,dest,len); } } while (0)

#define PASSTHROUGH_IO_OUT(vga,port,src,len)				\
    do { if ((vga)->passthrough && (!(vga)->skip_next_passthrough_out)) { passthrough_io_out(port,src,len); } (vga)->skip_next_passthrough_out=false; } while (0)

#define PASSTHROUGH_IO_SKIP_NEXT_OUT(vga)					\
    do { if ((vga)->passthrough) { (vga)->skip_next_passthrough_out=true; } } while (0)
		
#define PASSTHROUGH_READ_CHECK(vga,inter,pass) \
    do { if ((vga)->passthrough) { if ((inter)!=(pass)) { PrintError(core->vm_info, core, "vga: passthrough error: passthrough value read is 0x%x, but internal value read is 0x%x\n",(pass),(inter)); } } } while (0)

static int misc_out_read(struct guest_info *core, 
			 uint16_t port, 
			 void *dest,
			 uint_t len,
			 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug(core->vm_info, core, "vga: misc out read data=0x%x\n", vga->vga_misc.vga_misc_out.val);

    ERR_WRONG_SIZE("read","misc out",len,1,1);
   
    *((uint8_t*)dest) = vga->vga_misc.vga_misc_out.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_misc.vga_misc_out.val,*((uint8_t*)dest));

    return len;
}

static int misc_out_write(struct guest_info *core, 
			  uint16_t port, 
			  void *src,
			  uint_t len,
			  void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug(core->vm_info, core, "vga: misc out write data=0x%x\n", *((uint8_t*)src));
	
    ERR_WRONG_SIZE("write","misc out",len,1,1);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

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

    PrintDebug(core->vm_info, core, "vga: input stat0  read data=0x%x\n", vga->vga_misc.vga_input_stat0.val);

    ERR_WRONG_SIZE("read","input stat0",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_input_stat0.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_misc.vga_input_stat0.val,*(uint8_t*)dest);

    return len;
}


static int input_stat1_read(struct guest_info *core, 
			    uint16_t port, 
			    void *dest,
			    uint_t len,
			    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug(core->vm_info, core, "vga: input stat0 (%s) read data=0x%x\n", 
	       port==0x3ba ? "mono" : "color",
	       vga->vga_misc.vga_input_stat1.val);

    ERR_WRONG_SIZE("read","input stat1",len,1,1);


    *((uint8_t*)dest) = vga->vga_misc.vga_input_stat1.val;

    // Pretend that horizontal and vertical blanking
    // is occurring
    if (!vga->vga_misc.vga_input_stat1.disp_en) {

        // if not blanking, start horizontal blanking
        vga->vga_misc.vga_input_stat1.disp_en = 1;
        vga->vga_misc.vga_input_stat1.vert_retrace = 0;

    } else {

        if (!vga->vga_misc.vga_input_stat1.vert_retrace) {
            // if not vertical blanking, then now vertical blanking
            vga->vga_misc.vga_input_stat1.disp_en = 1;
            vga->vga_misc.vga_input_stat1.vert_retrace = 1;
        } else { 
            // if vertical blanking, now not blanking
            vga->vga_misc.vga_input_stat1.disp_en = 0;
            vga->vga_misc.vga_input_stat1.vert_retrace = 0;
        }

    }

    // Stunningly, reading stat1 is also a way to reset
    // the state of attribute controller address/data flipflop
    // That is some mighty fine crack the designers were smoking.
    
    vga->vga_attribute_controller.state=ATTR_ADDR;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_misc.vga_input_stat1.val,*(uint8_t*)dest);

    return len;
}
			 

static int feature_control_read(struct guest_info *core, 
				uint16_t port, 
				void *dest,
				uint_t len,
				void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug(core->vm_info, core, "vga: feature control  read data=0x%x\n", 
	       vga->vga_misc.vga_feature_control.val);

    ERR_WRONG_SIZE("read","feature control",len,1,1);


    *((uint8_t*)dest) = vga->vga_misc.vga_feature_control.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_misc.vga_feature_control.val,*(uint8_t*)dest);

    return len;
}

static int feature_control_write(struct guest_info *core, 
				 uint16_t port, 
				 void *src,
				 uint_t len,
				 void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug(core->vm_info, core, "vga: feature control (%s) write data=0x%x\n", 
	       port==0x3ba ? "mono" : "color",
	       *((uint8_t*)src));
	
    ERR_WRONG_SIZE("write","feature control",len,1,1);
    
    PASSTHROUGH_IO_OUT(vga,port,src,len);

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

    PrintDebug(core->vm_info, core, "vga: video subsys enable read data=0x%x\n", 
	       vga->vga_misc.vga_video_subsys_enable.val);

    ERR_WRONG_SIZE("read","video subsys enable",len,1,1);

    *((uint8_t*)dest) = vga->vga_misc.vga_video_subsys_enable.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_misc.vga_video_subsys_enable.val,*(uint8_t*)dest);

    return len;
}

static int video_subsys_enable_write(struct guest_info *core, 
				     uint16_t port, 
				     void *src,
				     uint_t len,
				     void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug(core->vm_info, core, "vga: video subsys enable write data=0x%x\n", *((uint8_t*)src));
	
    ERR_WRONG_SIZE("write","video subsys enable",len,1,1);
    
    PASSTHROUGH_IO_OUT(vga,port,src,len);

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

    PrintDebug(core->vm_info, core, "vga: sequencer address read data=0x%x\n", 
	       vga->vga_sequencer.vga_sequencer_addr.val);

    ERR_WRONG_SIZE("read","vga sequencer addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_sequencer.vga_sequencer_addr.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_sequencer.vga_sequencer_addr.val,*(uint8_t*)dest);

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
    
    PrintDebug(core->vm_info, core, "vga: sequencer write data (index=%d) with 0x%x\n", 
	       index, data);
    
    ERR_WRONG_SIZE("write","vga sequencer data",len,1,1);
    
    PASSTHROUGH_IO_OUT(vga,port,src,len);


    if (index>=VGA_SEQUENCER_NUM) { 
	PrintError(core->vm_info, core, "vga: sequencer data write is for invalid index %d, ignoring\n",index);
    } else {
	vga->vga_sequencer.vga_sequencer_regs[index] = data;
    }

    render(vga);
    
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

    PrintDebug(core->vm_info, core, "vga: sequencer address write data=0x%x len=%u\n", len==1 ? *((uint8_t*)src) : len==2 ? *((uint16_t*)src) : *((uint32_t*)src), len);

    ERR_WRONG_SIZE("write","vga sequencer addr",len,1,2);

    PASSTHROUGH_IO_OUT(vga,port,src,len);
    
    vga->vga_sequencer.vga_sequencer_addr.val =  *((uint8_t*)src) ;
    
    if (len==2) { 
	PASSTHROUGH_IO_SKIP_NEXT_OUT(vga);
	// second byte is the data
	if (sequencer_data_write(core,port,src+1,1,vga)!=1) { 
	    PrintError(core->vm_info, core, "vga: write of data failed\n");
	    return -1;
	}
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

    if (index>=VGA_SEQUENCER_NUM) { 
	data=0;
	PrintError(core->vm_info, core, "vga: sequencer data read at invalid index %d, returning zero\n",index);
    } else {
	data=vga->vga_sequencer.vga_sequencer_regs[index];
    }

    PrintDebug(core->vm_info, core, "vga: sequencer data read data (index=%d) = 0x%x\n", 
		   index, data);
    
    ERR_WRONG_SIZE("read","vga sequencer data",len,1,1);

    *((uint8_t*)dest) = data;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,data,*(uint8_t*)dest);

    return len;
}

 
 


static int crt_controller_address_read(struct guest_info *core, 
					uint16_t port, 
					void *dest,
					uint_t len,
					void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;

    PrintDebug(core->vm_info, core, "vga: crt controller (%s) address read data=0x%x\n", 
	       port==0x3b4 ? "mono" : "color",
	       vga->vga_crt_controller.vga_crt_addr.val);

    ERR_WRONG_SIZE("read","vga crt controller addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_crt_controller.vga_crt_addr.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_crt_controller.vga_crt_addr.val,*(uint8_t*)dest);

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
    
    PrintDebug(core->vm_info, core, "vga: crt controller (%s) write data (index=%d) with 0x%x\n", 
	       port==0x3b5 ? "mono" : "color",
	       index, data);

    ERR_WRONG_SIZE("write","vga crt controller data",len,1,1);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

    if (index>=VGA_CRT_CONTROLLER_NUM) { 
        PrintError(core->vm_info, core, "vga; crt controller write is for illegal index %d, ignoring\n",index);
    } else {
        vga->vga_crt_controller.vga_crt_controller_regs[index] = data;
        if (index == 17) {
            if (vga->vga_crt_controller.vga_vertical_retrace_end.enable_vertical_interrupt) {
                PrintError(core->vm_info, core, "vga: vertical_retrace_interrupt_enabled is unsupported -- no interrupts will occur!\n");
            }
        }
    }

    render(vga);

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

    PrintDebug(core->vm_info, core, "vga: crt controller (%s) address write data=0x%x len=%u\n", 
	       port==0x3b4 ? "mono" : "color",
	       len==1 ? *((uint8_t*)src) : len==2 ? *((uint16_t*)src) : *((uint32_t*)src), len);

    ERR_WRONG_SIZE("write","vga crt controller addr",len,1,2);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

    vga->vga_crt_controller.vga_crt_addr.val =  *((uint8_t*)src) ;
	
    if (len==2) { 
	PASSTHROUGH_IO_SKIP_NEXT_OUT(vga);
	// second byte is the data
	if (crt_controller_data_write(core,port,src+1,1,vga)!=1) { 
	    PrintError(core->vm_info, core, "vga: write of data failed\n");
	    return -1;
	}
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
    
    if (index>=VGA_CRT_CONTROLLER_NUM) { 
	data=0;
	PrintError(core->vm_info, core, "vga: crt controller data read for illegal index %d, returning zero\n",index);
    } else {
	data=vga->vga_crt_controller.vga_crt_controller_regs[index];
    }

    PrintDebug(core->vm_info, core, "vga: crt controller data (index=%d) = 0x%x\n",index,data);
    
    ERR_WRONG_SIZE("read","vga crt controller data",len,1,1);

    *((uint8_t*)dest) = data;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,data,*(uint8_t *)dest);

    return len;
}



static int graphics_controller_address_read(struct guest_info *core, 
					    uint16_t port, 
					    void *dest,
					    uint_t len,
					    void *priv_data)
{
    struct vga_internal *vga = (struct vga_internal *) priv_data;
    
    PrintDebug(core->vm_info, core, "vga: graphics controller address read data=0x%x\n", 
	       vga->vga_graphics_controller.vga_graphics_ctrl_addr.val);

    ERR_WRONG_SIZE("read","vga graphics controller addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_graphics_controller.vga_graphics_ctrl_addr.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_graphics_controller.vga_graphics_ctrl_addr.val,*(uint8_t*)dest);

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
    

    PrintDebug(core->vm_info, core, "vga: graphics_controller write data (index=%d) with 0x%x\n", 
	       index, data);
    
    ERR_WRONG_SIZE("write","vga graphics controller data",len,1,1);
    
    PASSTHROUGH_IO_OUT(vga,port,src,len);

    if (index>=VGA_GRAPHICS_CONTROLLER_NUM) { 
	PrintError(core->vm_info, core, "vga: graphics controller write for illegal index %d ignored\n",index);
    } else {
	vga->vga_graphics_controller.vga_graphics_controller_regs[index] = data;
    }

    render(vga);
    
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

    PrintDebug(core->vm_info, core, "vga: graphics controller address write data=0x%x len=%u\n", 
	       len==1 ? *((uint8_t*)src) : len==2 ? *((uint16_t*)src) : *((uint32_t*)src), len);

    ERR_WRONG_SIZE("write","vga graphics controller addr",len,1,2);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

    vga->vga_graphics_controller.vga_graphics_ctrl_addr.val =  *((uint8_t*)src) ;

    if (len==2) { 
	PASSTHROUGH_IO_SKIP_NEXT_OUT(vga);
	// second byte is the data
	if (graphics_controller_data_write(core,port,src+1,1,vga)!=1) { 
	    PrintError(core->vm_info, core, "vga: write of data failed\n");
	    return -1;
	}
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


    if (index>=VGA_GRAPHICS_CONTROLLER_NUM) { 
	data=0;
	PrintError(core->vm_info, core, "vga: graphics controller data read from illegal index %d, returning zero\n",index);
    } else {
	data=vga->vga_graphics_controller.vga_graphics_controller_regs[index];
    }
    
    PrintDebug(core->vm_info, core, "vga: graphics controller data read data (index=%d) = 0x%x\n", 
	       index, data);

    ERR_WRONG_SIZE("read","vga graphics controller data",len,1,1);

    *((uint8_t*)dest) = data;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,data,*(uint8_t*)dest);
    
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
    
    PrintDebug(core->vm_info, core, "vga: attribute controller address read data=0x%x\n", 
	       vga->vga_attribute_controller.vga_attribute_controller_addr.val);

    ERR_WRONG_SIZE("read","vga attribute  controller addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_attribute_controller.vga_attribute_controller_addr.val;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_attribute_controller.vga_attribute_controller_addr.val,*(uint8_t*)dest);

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
	PrintDebug(core->vm_info, core, "vga: attribute controller address write data=0x%x\n", new_addr);
	
	ERR_WRONG_SIZE("write","vga attribute controller addr",len,1,1);

	PASSTHROUGH_IO_OUT(vga,port,src,len);

	vga->vga_attribute_controller.vga_attribute_controller_addr.val =  new_addr;

	vga->vga_attribute_controller.state=ATTR_DATA;
	return len;

    } else if (vga->vga_attribute_controller.state==ATTR_DATA) { 

	uint8_t data = *((uint8_t*)src);
	uint8_t index=vga->vga_attribute_controller.vga_attribute_controller_addr.val;  // should mask probably

	PrintDebug(core->vm_info, core, "vga: attribute controller data write index %d with data=0x%x\n", index,data);
	
	ERR_WRONG_SIZE("write","vga attribute controller data",len,1,1);

	PASSTHROUGH_IO_OUT(vga,port,src,len);
	
	if (index>=VGA_ATTRIBUTE_CONTROLLER_NUM) { 
	    PrintError(core->vm_info, core, "vga: attribute controller write to illegal index %d ignored\n",index);
	} else {
	    vga->vga_attribute_controller.vga_attribute_controller_regs[index] = data;
	}
	
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

    if (index>=VGA_ATTRIBUTE_CONTROLLER_NUM) { 
	data=0;
	PrintError(core->vm_info, core, "vga: attribute controller read of illegal index %d, returning zero\n",index);
    } else {
	data=vga->vga_attribute_controller.vga_attribute_controller_regs[index];
    }
    
    PrintDebug(core->vm_info, core, "vga: attribute controller data read data (index=%d) = 0x%x\n", 
	       index, data);

    ERR_WRONG_SIZE("read","vga attribute controller data",len,1,1);

    *((uint8_t*)dest) = data;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,data,*(uint8_t*)dest);

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
    
    PrintDebug(core->vm_info, core, "vga: dac write address read data=0x%x\n", 
	       vga->vga_dac.vga_dac_write_addr);

    ERR_WRONG_SIZE("read","vga dac write addr",len,1,1);


    *((uint8_t*)dest) = vga->vga_dac.vga_dac_write_addr;

    PASSTHROUGH_IO_IN(vga,port,dest,len);
    
    PASSTHROUGH_READ_CHECK(vga,vga->vga_dac.vga_dac_write_addr,*(uint8_t*)dest);

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

    PrintDebug(core->vm_info, core, "vga: dac write address write data=0x%x\n", new_addr);

    ERR_WRONG_SIZE("write","vga dac write addr",len,1,1);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

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
    
    PrintDebug(core->vm_info, core, "vga: dac read address read data=0x%x\n", 
	       vga->vga_dac.vga_dac_read_addr);

    ERR_WRONG_SIZE("read","vga dac read addr",len,1,1);

    *((uint8_t*)dest) = vga->vga_dac.vga_dac_read_addr;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_dac.vga_dac_read_addr,*(uint8_t*)dest);
    
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

    PrintDebug(core->vm_info, core, "vga: dac read address write data=0x%x\n", new_addr);

    ERR_WRONG_SIZE("write","vga dac read addr",len,1,1);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

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
	PrintError(core->vm_info, core, "vga: dac data read while in other state\n");
	// results undefined, so we continue
    }

    ERR_WRONG_SIZE("read","vga dac read data",len,1,1);

    curreg = vga->vga_dac.vga_dac_read_addr;
    curchannel = vga->vga_dac.channel;
    data = (vga->vga_dac.vga_dac_palette[curreg] >> curchannel*8) & 0x3f;

    PrintDebug(core->vm_info, core, "vga: dac reg %u [%s] = 0x%x\n",
	       curreg, 
	       curchannel == 0 ? "RED" : curchannel == 1 ? "GREEN" 
                          : curchannel==2 ? "BLUE" : "BAD CHANNEL", 
	       data);

    *((uint8_t*)dest) = data;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,data,*(uint8_t*)dest);
    
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
	PrintError(core->vm_info, core, "vga: dac data write while in other state\n");
	// results undefined, so we continue
    }

    ERR_WRONG_SIZE("read","vga dac write data",len,1,1);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

    curreg = vga->vga_dac.vga_dac_write_addr;
    curchannel = vga->vga_dac.channel;
    data = *((uint8_t *)src);

    PrintDebug(core->vm_info, core, "vga: dac reg %u [%s] write with 0x%x\n",
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
    
    PrintDebug(core->vm_info, core, "vga: dac pixel mask read data=0x%x\n", 
	       vga->vga_dac.vga_pixel_mask);

    ERR_WRONG_SIZE("read","vga pixel mask",len,1,1);

    *((uint8_t*)dest) = vga->vga_dac.vga_pixel_mask;

    PASSTHROUGH_IO_IN(vga,port,dest,len);

    PASSTHROUGH_READ_CHECK(vga,vga->vga_dac.vga_pixel_mask,*(uint8_t*)dest);

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

    PrintDebug(core->vm_info, core, "vga: dac pixel mask write data=0x%x\n", new_data);

    ERR_WRONG_SIZE("write","pixel mask",len,1,1);

    PASSTHROUGH_IO_OUT(vga,port,src,len);

    vga->vga_dac.vga_pixel_mask =  new_data;

    return len;
}

static int init_vga(struct vga_internal *vga)
{
  // TODO: startup spec of register contents, if any
  vga->vga_misc.vga_input_stat1.val = 0x1;  // display enable, not in retrace

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


    if (vga->host_cons) { 
	v3_graphics_console_close(vga->host_cons);
    }

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
    char * passthrough = v3_cfg_val(cfg, "passthrough");
    char * hostframebuf = v3_cfg_val(cfg, "hostframebuf");

    PrintDebug(vm, VCORE_NONE, "vga: init_device\n");

    vga = (struct vga_internal *)V3_Malloc(sizeof(struct vga_internal));

    if (!vga) { 
	PrintError(vm, VCORE_NONE, "vga: cannot allocate\n");
	return -1;
    }

    memset(vga, 0, sizeof(struct vga_internal));

    vga->render_model.model = CONSOLE_DRIVEN_RENDERING | VGA_DRIVEN_PERIODIC_RENDERING;
    vga->render_model.updates_before_render = DEFAULT_UPDATES_BEFORE_RENDER;

    if (passthrough && strcasecmp(passthrough,"enable")==0) {
	PrintDebug(vm, VCORE_NONE, "vga: enabling passthrough\n");
	vga->passthrough=true;
	vga->skip_next_passthrough_out=false;
    }


    if (hostframebuf && strcasecmp(hostframebuf,"enable")==0) { 
	struct v3_frame_buffer_spec req;

	PrintDebug(vm, VCORE_NONE, "vga: enabling host frame buffer console (GRAPHICS_CONSOLE)\n");

	memset(&req,0,sizeof(struct v3_frame_buffer_spec));
	memset(&(vga->target_spec),0,sizeof(struct v3_frame_buffer_spec));

	req.height=VGA_MAXY;
	req.width=VGA_MAXX;
	req.bytes_per_pixel=4;
	req.bits_per_channel=8;
	req.red_offset=0;
	req.green_offset=1;
	req.blue_offset=2;
	

	vga->host_cons = v3_graphics_console_open(vm,&req,&(vga->target_spec));

	if (!vga->host_cons) { 
	    PrintError(vm, VCORE_NONE, "vga: unable to open host OS's graphics console\n");
	    free_vga(vga);
	    return -1;
	}

	if (memcmp(&req,&(vga->target_spec),sizeof(req))) {
	    PrintDebug(vm, VCORE_NONE, "vga: warning: target spec differs from requested spec\n");
	    PrintDebug(vm, VCORE_NONE, "vga: request: %u by %u by %u with %u bpc and r,g,b at %u, %u, %u\n", req.width, req.height, req.bytes_per_pixel, req.bits_per_channel, req.red_offset, req.green_offset, req.blue_offset);
	    PrintDebug(vm, VCORE_NONE, "vga: response: %u by %u by %u with %u bpc and r,g,b at %u, %u, %u\n", vga->target_spec.width, vga->target_spec.height, vga->target_spec.bytes_per_pixel, vga->target_spec.bits_per_channel, vga->target_spec.red_offset, vga->target_spec.green_offset, vga->target_spec.blue_offset);

	}

	if (vga->render_model.model & CONSOLE_DRIVEN_RENDERING) { 
	  V3_Print(vm, VCORE_NONE, "vga: enabling console-driven rendering\n");
	  if (v3_graphics_console_register_render_request(vga->host_cons, render_callback, vga)!=0) { 
	    PrintError(vm, VCORE_NONE, "vga: cannot enable console-driven rendering\n");
	    free_vga(vga);
	    return -1;
	  }
	}
	
	V3_Print(vm, VCORE_NONE, "vga: enabling console inquiry for updates\n");
	if (v3_graphics_console_register_update_inquire(vga->host_cons, update_callback, vga)!=0) { 
	  PrintError(vm, VCORE_NONE, "vga: cannot enable console inquiry for updates\n");
	  free_vga(vga);
	  return -1;
	}
   }

    if (!vga->passthrough && !vga->host_cons) { 
	V3_Print(vm, VCORE_NONE, "vga: neither passthrough nor host console are enabled - no way to display anything!\n");
    }


    // No memory store is allocated since we will use a full memory hook
    // The VGA maps can be read as well as written
    // Reads also affect writes, since they are how you fill the latches

    // Now allocate the maps
    for (i=0;i<MAP_NUM;i++) { 
	void *temp;

	temp = (void*)V3_AllocPages(MAP_SIZE/4096);
	if (!temp) { 
	    PrintError(vm, VCORE_NONE, "vga: cannot allocate maps\n");
	    free_vga(vga);
	    return -1;
	}

	vga->map[i] = (vga_map) V3_VAddr(temp);

	memset(vga->map[i],0,MAP_SIZE);
    }
    
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, vga);
    
    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not attach device %s\n", dev_id);
	free_vga(vga);
	return -1;
    }
    
    vga->dev = dev;
    
    if (v3_hook_full_mem(vm, V3_MEM_CORE_ANY, 
			 MEM_REGION_START, MEM_REGION_END,
			 &vga_read, 
			 &vga_write,
			 dev) == -1) {
	PrintError(vm, VCORE_NONE, "vga: memory book failed\n");
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
	PrintError(vm, VCORE_NONE, "vga: Error allocating VGA I/O ports\n");
	v3_remove_device(dev);
	return -1;
    }

    init_vga(vga);

    PrintDebug(vm, VCORE_NONE, "vga: successfully added and initialized.\n");

    return 0;

}

device_register("VGA", vga_init);
