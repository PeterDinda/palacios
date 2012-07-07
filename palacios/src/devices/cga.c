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
 * Initial VGA support added by Erik van der Kouwe <vdkouwe@cs.vu.nl>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_sprintf.h>

#include <devices/console.h>


#if V3_CONFIG_DEBUG_CGA >= 2
#define PrintVerbose PrintDebug
#else
#define PrintVerbose(fmt, args...)
#endif
#if V3_CONFIG_DEBUG_CGA == 0
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define START_ADDR 0xA0000
#define END_ADDR 0xC0000

#define FRAMEBUF_SIZE (END_ADDR - START_ADDR)

#define BYTES_PER_COL 2

struct misc_outp_reg {
    uint8_t ios		: 1;
    uint8_t eram	: 1;
    uint8_t cs		: 1;
    uint8_t reserved	: 2;
    uint8_t vsp		: 1;
    uint8_t hsp		: 1;
};

struct seq_data_reg_clocking_mode {
    uint8_t d89		: 1;
    uint8_t reserved1	: 1;
    uint8_t sl		: 1;
    uint8_t dc		: 1;
    uint8_t sh4		: 1;
    uint8_t so		: 1;
    uint8_t reserved2	: 2;
};

struct crtc_data_reg_overflow {
    uint8_t vt8		: 1;
    uint8_t vde8	: 1;
    uint8_t vrs8	: 1;
    uint8_t vbs8	: 1;
    uint8_t lc8		: 1;
    uint8_t vt9		: 1;
    uint8_t vde9	: 1;
    uint8_t vrs9	: 1;
};

struct crtc_data_reg_max_scan_line {
    uint8_t msl		: 5;
    uint8_t vbs9	: 1;
    uint8_t lc9		: 1;
    uint8_t dsc		: 1;
};

struct graphc_data_reg_graphics_mode {
    uint8_t wm		: 2;
    uint8_t reserved1	: 1;
    uint8_t rm		: 1;
    uint8_t or		: 1;
    uint8_t sr		: 1;
    uint8_t c256	: 1;
    uint8_t reserved2	: 1;
};

struct graphc_data_reg_misc {
    uint8_t gm		: 1;
    uint8_t oe		: 1;
    uint8_t mm		: 2;
    uint8_t reserved	: 4;
};

struct attrc_data_reg_attr_mode_control {
    uint8_t g		: 1;
    uint8_t me		: 1;
    uint8_t elg		: 1;
    uint8_t eb		: 1;
    uint8_t reserved	: 1;
    uint8_t pp		: 1;
    uint8_t pw		: 1;
    uint8_t ps		: 1;
};

#define SEQ_REG_COUNT				5
#define SEQ_REGIDX_RESET			0
#define SEQ_REGIDX_CLOCKING_MODE		1
#define SEQ_REGIDX_MAP_MASK			2
#define SEQ_REGIDX_CHARACTER_MAP_SELECT		3
#define SEQ_REGIDX_MEMORY_MODE			4

#define CRTC_REG_COUNT				25
#define CRTC_REGIDX_HORI_TOTAL			0
#define CRTC_REGIDX_HORI_DISPLAY_ENABLE		1
#define CRTC_REGIDX_START_HORI_BLANKING		2
#define CRTC_REGIDX_END_HORI_BLANKING		3
#define CRTC_REGIDX_START_HORI_RETRACE		4
#define CRTC_REGIDX_END_HORI_RETRACE		5
#define CRTC_REGIDX_VERT_TOTAL			6
#define CRTC_REGIDX_OVERFLOW			7
#define CRTC_REGIDX_PRESET_ROW_SCAN		8
#define CRTC_REGIDX_MAX_SCAN_LINE		9
#define CRTC_REGIDX_CURSOR_START		10
#define CRTC_REGIDX_CURSOR_END			11
#define CRTC_REGIDX_START_ADDR_HIGH		12
#define CRTC_REGIDX_START_ADDR_LOW		13
#define CRTC_REGIDX_CURSOR_LOC_HIGH		14
#define CRTC_REGIDX_CURSOR_LOC_LOW		15
#define CRTC_REGIDX_VERT_RETRACE_START		16
#define CRTC_REGIDX_VERT_RETRACE_END		17
#define CRTC_REGIDX_VERT_DISPLAY_ENABLE_END	18
#define CRTC_REGIDX_OFFSET			19
#define CRTC_REGIDX_UNDERLINE_LOCATION		20
#define CRTC_REGIDX_START_VERT_BLANKING		21
#define CRTC_REGIDX_END_VERT_BLANKING		22
#define CRTC_REGIDX_CRT_MODE_CONTROL		23
#define CRTC_REGIDX_LINE_COMPATE		24

#define GRAPHC_REG_COUNT		9
#define GRAPHC_REGIDX_SET_RESET		0
#define GRAPHC_REGIDX_ENABLE_SET_RESET	1
#define GRAPHC_REGIDX_COLOR_COMPARE	2
#define GRAPHC_REGIDX_DATA_ROTATE	3
#define GRAPHC_REGIDX_READ_MAP_SELECT	4
#define GRAPHC_REGIDX_GRAPHICS_MODE	5
#define GRAPHC_REGIDX_MISC		6
#define GRAPHC_REGIDX_COLOR_DONT_CARE	7
#define GRAPHC_REGIDX_BIT_MASK		8

#define ATTRC_REG_COUNT				21
#define ATTRC_REGIDX_PALETTE_0			0
#define ATTRC_REGIDX_ATTR_MODE_CONTROL		16
#define ATTRC_REGIDX_OVERSCAN_COLOR		17
#define ATTRC_REGIDX_COLOR_PLANE_ENABLE		18
#define ATTRC_REGIDX_HORI_PEL_PANNING		19
#define ATTRC_REGIDX_COLOR_SELECT		20

#define DAC_ENTRY_COUNT			256
#define DAC_COLOR_COUNT			3
#define DAC_REG_COUNT			(DAC_ENTRY_COUNT * DAC_COLOR_COUNT)

struct video_internal {
    uint8_t * framebuf;
    addr_t framebuf_pa;

    /* registers */
    struct misc_outp_reg misc_outp_reg;		// io port 3CC (R) / 3C2 (W)
    uint8_t seq_index_reg;			// io port 3C4
    uint8_t seq_data_regs[SEQ_REG_COUNT];	// io port 3C5
    uint8_t crtc_index_reg;			// io port 3D4
    uint8_t crtc_data_regs[CRTC_REG_COUNT];	// io port 3D5
    uint8_t graphc_index_reg;			// io port 3CE
    uint8_t graphc_data_regs[GRAPHC_REG_COUNT];	// io port 3CF
    uint8_t attrc_index_flipflop;
    uint8_t attrc_index_reg;			// io port 3C0
    uint8_t attrc_data_regs[ATTRC_REG_COUNT];	// io port 3C1 (R) / 3C0 (W)
    uint8_t dac_indexr_reg;			// io port 3C8
    uint8_t dac_indexr_color;
    uint8_t dac_indexw_reg;			// io port 3C7
    uint8_t dac_indexw_color;
    uint8_t dac_data_regs[DAC_REG_COUNT];	// io port 3C9

    /* auxilary fields derived from register values */
    addr_t activefb_addr;
    uint_t activefb_len;
    uint16_t iorange;
    uint_t vres;
    uint_t hres;
    uint_t vchars;
    uint_t hchars;
    int graphmode;
    
    /* status */
    int dirty;
    int reschanged;

    /* IMPORTANT: These are column offsets _NOT_ byte offsets */
    uint16_t screen_offset; // relative to the framebuffer
    uint16_t cursor_offset; // relative to the framebuffer
    /* ** */

    struct vm_device * dev;


    uint8_t passthrough;


    struct v3_console_ops * ops;
    void * private_data;



};

static void refresh_screen(struct video_internal * state) {
    uint_t screen_size;
    
    PrintDebug("Screen config: framebuf=0x%x-0x%x, gres=%dx%d, tres=%dx%d, %s mode\n", 
    	(unsigned) state->activefb_addr, 
    	(unsigned) state->activefb_addr + state->activefb_len, 
	state->hres,
	state->vres,
	state->hchars,
	state->vchars,
	state->graphmode ? "graphics" : "text");
    
    /* tell the frontend to refresh the screen entirely */
    state->dirty = 0;

    if (state->reschanged) {
        /* resolution change message will trigger update */
	state->reschanged = 0;
	PrintDebug("Video: set_text_resolution(%d, %d)\n", 
    	    state->hchars, state->vchars);
	if (state->ops && state->ops->set_text_resolution) {
    	    state->ops->set_text_resolution(state->hchars, state->vchars, state->private_data);
    	}
    } else {
        /* send update for full buffer */
	PrintDebug("Video: update_screen(0, 0, %d * %d * %d)\n", state->vchars, state->hchars, BYTES_PER_COL);
	screen_size = state->vchars * state->hchars * BYTES_PER_COL;
	if (state->ops) {
    	    state->ops->update_screen(0, 0, screen_size, state->private_data);
    	}
    }
}

static void registers_updated(struct video_internal * state) {
    struct seq_data_reg_clocking_mode *cm;
    struct graphc_data_reg_misc *misc;
    struct crtc_data_reg_max_scan_line *msl;
    struct crtc_data_reg_overflow *ovf;
    int lines_per_char;
    uint_t activefb_addr, activefb_len, hchars, vchars, vde, hres, vres;
    
    /* framebuffer mapping address */
    misc = (struct graphc_data_reg_misc *)(state->graphc_data_regs + GRAPHC_REGIDX_MISC);

    if (misc->mm >= 2) {
    	activefb_addr = (misc->mm == 3) ? 0xb8000 : 0xb0000;
    	activefb_len = 0x8000;
    } else {
    	activefb_addr = 0xa0000;
    	activefb_len = (misc->mm == 1) ? 0x10000 : 0x20000;
    }

    if ((state->activefb_addr != activefb_addr) || (state->activefb_len != activefb_len)) {
	state->activefb_addr = activefb_addr;
	state->activefb_len = activefb_len;
	state->dirty = 1;
	PrintVerbose("Video: need refresh (activefb=0x%x-0x%x)\n", 
	    activefb_addr, activefb_addr + activefb_len);
    } 
    
    /* mode selection; may be inconclusive, keep old value in that case */
    if (state->graphmode != misc->gm) {
	state->graphmode = misc->gm;
	state->dirty = 1;
	PrintVerbose("Video: need refresh (graphmode=%d)\n", state->graphmode);
    }

    /* graphics resolution */
    if (state->misc_outp_reg.hsp) {
	vres = (state->misc_outp_reg.vsp) ? 480 : 400;
    } else {
	if (!state->misc_outp_reg.vsp) {
    	    PrintError("Video: reserved value in misc_outp_reg (0x%x)\n", 
		*(uint8_t *) &state->misc_outp_reg);
	}
    	vres = 350;
    }
    msl = (struct crtc_data_reg_max_scan_line *) (
	state->crtc_data_regs + CRTC_REGIDX_MAX_SCAN_LINE);
    if (msl->dsc) vres /= 2;
    if (state->vres != vres) {
	state->vres = vres;
	state->reschanged = 1;
	PrintVerbose("Video: need refresh (vres=%d)\n", vres);
    }
    
    switch (state->misc_outp_reg.cs) {
        case 0: hres = 640; break;
        case 1: hres = 720; break;
	default:
		PrintError("Video: reserved value in misc_outp_reg (0x%x)\n", 
			*(uint8_t *) &state->misc_outp_reg);
		hres = 640;
		break;
    }
    cm = (struct seq_data_reg_clocking_mode *) (
	state->seq_data_regs + SEQ_REGIDX_CLOCKING_MODE);
    if (cm->dc) hres /= 2;
    if (state->hres != hres) {
	state->hres = hres;
	state->reschanged = 1;
	PrintVerbose("Video: need refresh (hres=%d)\n", hres);
    }

    /* text resolution */
    ovf = (struct crtc_data_reg_overflow *) (state->crtc_data_regs + CRTC_REGIDX_OVERFLOW);
    
    hchars = state->crtc_data_regs[CRTC_REGIDX_HORI_DISPLAY_ENABLE] + 1;
    lines_per_char = msl->msl + 1;
    vde = state->crtc_data_regs[CRTC_REGIDX_VERT_DISPLAY_ENABLE_END] |
	(((unsigned) ovf->vde8) << 8) | 
	(((unsigned) ovf->vde9) << 9);
    vchars = (vde + 1) / lines_per_char;
    if (state->hchars != hchars || state->vchars != vchars) {
	state->hchars = hchars;
	state->vchars = vchars;
	state->reschanged = 1;
	PrintVerbose("Video: need refresh (hchars=%d, vchars=%d)\n", hchars, vchars);
    }
    
    /* resolution change implies refresh needed */
    if (state->reschanged) {
	state->dirty = 1;
    }

    /* IO port range selection */
    state->iorange = state->misc_outp_reg.ios ? 0x3d0 : 0x3b0;
}

static void registers_initialize(struct video_internal * state) {

    /* initialize the registers; defaults taken from vgatables.h in the VGA 
     * BIOS, mode 3 (which is specified by IBM as the default mode)
     */
    static const uint8_t seq_defaults[] = {
    	0x03, 0x00, 0x03, 0x00, 0x02,
    };
    static const uint8_t crtc_defaults[] = {
	0x5f, 0x4f, 0x50, 0x82,	/* 0 - 3 */
	0x55, 0x81, 0xbf, 0x1f,	/* 4 - 7 */
	0x00, 0x4f, 0x0d, 0x0e,	/* 8 - 11 */
	0x00, 0x00, 0x00, 0x00,	/* 12 - 15 */
	0x9c, 0x8e, 0x8f, 0x28,	/* 16 - 19 */
	0x1f, 0x96, 0xb9, 0xa3,	/* 20 - 23 */
	0xff 			/* 24 */
    };
    static const uint8_t graphc_defaults[] = {
	0x00, 0x00, 0x00, 0x00,	/* 0 - 3 */
	0x00, 0x10, 0x0e, 0x0f,	/* 4 - 7 */
	0xff 			/* 8 */
    };
    static const uint8_t attrc_defaults[] = {
	0x00, 0x01, 0x02, 0x03,	/* 0 - 3 */
	0x04, 0x05, 0x14, 0x07,	/* 4 - 7 */ 
	0x38, 0x39, 0x3a, 0x3b,	/* 8 - 11 */ 
	0x3c, 0x3d, 0x3e, 0x3f,	/* 12 - 15 */ 
	0x0c, 0x00, 0x0f, 0x08,	/* 16 - 19 */ 
	0x00, 			/* 20 */
    };
 
    /* general registers */
    state->misc_outp_reg.ios = 1;
    state->misc_outp_reg.eram = 1;
    state->misc_outp_reg.cs = 1;
    state->misc_outp_reg.hsp = 1;    
    
    /* sequencer registers */
    V3_ASSERT(sizeof(seq_defaults) == sizeof(state->seq_data_regs));
    memcpy(state->seq_data_regs, seq_defaults, sizeof(state->seq_data_regs));    
    
    /* CRT controller registers */
    V3_ASSERT(sizeof(crtc_defaults) == sizeof(state->crtc_data_regs));
    memcpy(state->crtc_data_regs, crtc_defaults, sizeof(state->crtc_data_regs));    
    
    /* graphics controller registers */
    V3_ASSERT(sizeof(graphc_defaults) == sizeof(state->graphc_data_regs));
    memcpy(state->graphc_data_regs, graphc_defaults, sizeof(state->graphc_data_regs));    
    
    /* attribute controller registers */
    V3_ASSERT(sizeof(attrc_defaults) == sizeof(state->attrc_data_regs));
    memcpy(state->attrc_data_regs, attrc_defaults, sizeof(state->attrc_data_regs));    
    
    /* initialize auxilary fields */
    registers_updated(state);
}

static void passthrough_in(uint16_t port, void * src, uint_t length) {
    switch (length) {
	case 1:
	    *(uint8_t *)src = v3_inb(port);
	    break;
	case 2:
	    *(uint16_t *)src = v3_inw(port);
	    break;
	case 4:
	    *(uint32_t *)src = v3_indw(port);
	    break;
	default:
	    break;
    }
}


static void passthrough_out(uint16_t port, const void * src, uint_t length) {
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
	    break;
    }
}

#if V3_CONFIG_DEBUG_CGA >= 2
static unsigned long get_value(const void *ptr, int len) {
  unsigned long value = 0;

  if (len > sizeof(value)) len = sizeof(value);
  memcpy(&value, ptr, len);

  return value;
}

static char opsize_char(uint_t length) {
    switch (length) {
    case 1: return 'b'; 
    case 2: return 'w'; 
    case 4: return 'l'; 
    case 8: return 'q';     
    default: return '?'; 
    }
}
#endif

static int video_write_mem(struct guest_info * core, addr_t guest_addr, void * dest, uint_t length, void * priv_data) {
    struct vm_device * dev = (struct vm_device *)priv_data;
    struct video_internal * state = (struct video_internal *)dev->private_data;
    uint_t length_adjusted, screen_pos, x, y;
    addr_t framebuf_offset, framebuf_offset_screen, screen_offset;
    
    V3_ASSERT(guest_addr >= START_ADDR);
    V3_ASSERT(guest_addr < END_ADDR);
    
    PrintVerbose("Video: write(%p, 0x%lx, %d)\n", 
       (void *)guest_addr, 
	get_value(state->framebuf + (guest_addr - START_ADDR), length),
	length);

    /* get data written by passthrough into frame buffer if needed */
    if (state->passthrough) {
	memcpy(state->framebuf + (guest_addr - START_ADDR), V3_VAddr((void *)guest_addr), length);
    }
    
    /* refresh the entire screen after the registers have been changed */
    if (state->dirty) {
	refresh_screen(state);
	return length;
    }
    
    /* the remainder is only needed if there is a front-end */
    if (!state->ops) {
	return length;
    }

    /* write may point into a framebuffer not currently active, for example 
     * preparing a VGA buffer at 0xA0000 while the CGA text mode at 0xB8000 
     * is still on the display; in this case we have to ignore (part of) the 
     * write to avoid buffer overflows
     */
    length_adjusted = length;
    if (state->activefb_addr > guest_addr) {
    	uint_t diff = state->activefb_addr - guest_addr;
    	if (diff >= length_adjusted) return length;
    	guest_addr += diff;
    	length_adjusted -= diff;
    }

    framebuf_offset = guest_addr - state->activefb_addr;
    if (state->activefb_len <= framebuf_offset) return length;
    if (length_adjusted > state->activefb_len - framebuf_offset) {
    	length_adjusted = state->activefb_len - framebuf_offset;
    }

    /* determine position on screen, considering wrapping */
    framebuf_offset_screen = state->screen_offset * BYTES_PER_COL;
    if (framebuf_offset > framebuf_offset_screen) {
    	screen_offset = framebuf_offset - framebuf_offset_screen;
    } else {
    	screen_offset = framebuf_offset + state->activefb_len - framebuf_offset_screen;
    }
    
    /* translate to coordinates and pass to the frontend */
    screen_pos = screen_offset / BYTES_PER_COL;
    x = screen_pos % state->hchars;
    y = screen_pos / state->hchars;
    if (y >= state->vchars) return length;
    PrintVerbose("Video: update_screen(%d, %d, %d)\n", x, y, length_adjusted);
    state->ops->update_screen(x, y, length_adjusted, state->private_data);

    return length;
}

static void debug_port(struct video_internal * video_state, const char *function, uint16_t port, uint_t length, uint_t maxlength)
{
    uint16_t portrange = port & 0xfff0;

    /* report any unexpected guest behaviour, it may explain failures */
    if (portrange != 0x3c0 && portrange != video_state->iorange) {
	PrintError("Video %s: got bad port 0x%x\n", function, port);
    }

    if (!video_state->passthrough && length > maxlength) {
    	PrintError("Video %s: got bad length %d\n", function, length);
    }
    V3_ASSERT(length >= 1);
}

static void handle_port_read(struct video_internal * video_state, const char *function, uint16_t port, void *dest, uint_t length, uint_t maxlength) {
    PrintVerbose("Video %s: in%c(0x%x): 0x%lx\n", function, opsize_char(length), port, get_value(dest, length));
    debug_port(video_state, function, port, length, maxlength);

    if (video_state->passthrough) {
	passthrough_in(port, dest, length);
    }
}

static void handle_port_write(struct video_internal * video_state, const char *function, uint16_t port, const void *src, uint_t length, uint_t maxlength) {
    PrintVerbose("Video %s: out%c(0x%x, 0x%lx)\n", function, opsize_char(length), port, get_value(src, length));
    debug_port(video_state, function, port, length, maxlength);

    if (video_state->passthrough) {
	passthrough_out(port, src, length);
    }
}

static int notimpl_port_read(struct video_internal * video_state, const char *function, uint16_t port, void *dest, uint_t length) {
    memset(dest, 0xff, length);
    handle_port_read(video_state, function, port, dest, length, 1);
    if (!video_state->passthrough) {
    	PrintError("Video %s: not implemented\n", function);
    }
    return length;
}

static int notimpl_port_write(struct video_internal * video_state, const char *function, uint16_t port, const void *src, uint_t length) {
    handle_port_write(video_state, function, port, src, length, 1);
    if (!video_state->passthrough) {
    	PrintError("Video %s: not implemented\n", function);
    }
    return length;
}

/* general registers */
static int misc_outp_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(struct misc_outp_reg *) dest = video_state->misc_outp_reg;

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int misc_outp_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    PrintDebug("Video: misc_outp=0x%x\n", *(uint8_t *) src);
    video_state->misc_outp_reg = *(struct misc_outp_reg *) src;
    registers_updated(video_state);

    return length;
}

static int inp_status0_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    return notimpl_port_read(priv_data, __FUNCTION__, port, dest, length);
}

static int inp_status1_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    /* next write to attrc selects the index rather than data */
    video_state->attrc_index_flipflop = 0;
    memset(dest, 0x0, length);

    handle_port_read(priv_data, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int feat_ctrl_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    return notimpl_port_read(priv_data, __FUNCTION__, port, dest, length);
}

static int feat_ctrl_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    return notimpl_port_write(priv_data, __FUNCTION__, port, src, length);
}

static int video_enable_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    return notimpl_port_read(priv_data, __FUNCTION__, port, dest, length);
}

static int video_enable_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    return notimpl_port_write(priv_data, __FUNCTION__, port, src, length);
}

/* sequencer registers */
static int seq_data_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    int index = video_state->seq_index_reg;

    if (index < SEQ_REG_COUNT) {
    	*(uint8_t *) dest = video_state->seq_data_regs[index];
    } else {
    	PrintError("Video %s: index %d out of range\n", __FUNCTION__, video_state->seq_index_reg);
    	*(uint8_t *) dest = 0;
    }

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;    
}

static int seq_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    int index = video_state->seq_index_reg;
    uint8_t val = *(uint8_t *) src;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    if (index < SEQ_REG_COUNT) {
    	PrintDebug("Video: seq[%d]=0x%x\n", index, val);
    	video_state->seq_data_regs[index] = val;
    	registers_updated(video_state);
    } else {
    	PrintError("Video %s: index %d out of range\n", __FUNCTION__, video_state->seq_index_reg);
    }

    return length;    
}

static int seq_index_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(uint8_t *) dest = video_state->seq_index_reg;

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int seq_index_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 2);

    video_state->seq_index_reg = *(uint8_t *) src;

    if (length > 1) {
	if (seq_data_write(core, port + 1, (uint8_t *) src + 1, length - 1, priv_data) != length - 1) {
	    return -1;
	}
    }

    return length;    
}

/* CRT controller registers */
static int crtc_data_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(uint8_t *) dest = video_state->crtc_index_reg;

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int crtc_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    uint8_t val = *(uint8_t *)src;
    uint_t index = video_state->crtc_index_reg;

    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);
    if (length != 1) {
	PrintError("Invalid write length for port 0x%x\n", port);
	return -1;
    }

    video_state->crtc_data_regs[index] = val;
    registers_updated(video_state);

    switch (index) {

	case CRTC_REGIDX_START_ADDR_HIGH: break; // Dealt by low-order byte write
	case CRTC_REGIDX_START_ADDR_LOW: {  // Scroll low byte
	    int diff, refresh;
	    uint_t screen_offset;

	    screen_offset =
		((uint16_t) video_state->crtc_data_regs[CRTC_REGIDX_START_ADDR_HIGH] << 8) |
		((uint16_t) video_state->crtc_data_regs[CRTC_REGIDX_START_ADDR_LOW]);
	    if ((screen_offset - video_state->screen_offset) % video_state->hchars) {
	        /* diff is not a multiple of column count, need full refresh */
		diff = 0;
		refresh = 1;
	    } else {
	        /* normal scroll (the common case) */
	        diff = (screen_offset - video_state->screen_offset) / video_state->hchars;
		refresh = 0;
	    }
	    PrintVerbose("Video: screen_offset=%d, video_state->screen_offset=%d, video_state->hchars=%d, diff=%d, refresh=%d\n",
		screen_offset, video_state->screen_offset, video_state->hchars, diff, refresh);

	    // Update the true offset value
	    video_state->screen_offset = screen_offset;

	    if (refresh || video_state->dirty) {
		refresh_screen(video_state);
	    } else if (diff && video_state->ops) {
	        PrintVerbose("Video: scroll(%d)\n", diff);
		if (video_state->ops->scroll(diff, video_state->private_data) == -1) {
		    PrintError("Error sending scroll event\n");
		    return -1;
		}
	    }
	    break;
	}
	case CRTC_REGIDX_CURSOR_LOC_HIGH: break; // Dealt by low-order byte write
	case CRTC_REGIDX_CURSOR_LOC_LOW: { // Cursor adjustment low byte
 	    uint_t x;
	    uint_t y;
	    
	    video_state->cursor_offset =
		((uint16_t) video_state->crtc_data_regs[CRTC_REGIDX_CURSOR_LOC_HIGH] << 8) |
		((uint16_t) video_state->crtc_data_regs[CRTC_REGIDX_CURSOR_LOC_LOW]);
	    x = video_state->cursor_offset % video_state->hchars;
	    y = (video_state->cursor_offset - video_state->screen_offset) / video_state->hchars;
	    PrintVerbose("Video: video_state->cursor_offset=%d, x=%d, y=%d\n",
		video_state->cursor_offset, x, y);

	    if (video_state->dirty) {
		refresh_screen(video_state);
	    }
	    
	    PrintVerbose("Video: set cursor(%d, %d)\n", x, y);
	    if (video_state->ops) {
		if (video_state->ops->update_cursor(x, y, video_state->private_data) == -1) {
		    PrintError("Error updating cursor\n");
		    return -1;
		}
	    } 

	    break;
	}
	default:
	    PrintDebug("Video: crtc[%d]=0x%x\n", index, val);
	    break;
    }

    if (video_state->passthrough) {
	passthrough_out(port, src, length);
    }

    return length;
}

static int crtc_index_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(uint8_t *) dest = video_state->crtc_index_reg;

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int crtc_index_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    handle_port_write(video_state, __FUNCTION__, port, src, length, 2);
    if (length > 2) {
	PrintError("Invalid write length for crtc index register port: %d (0x%x)\n",
		   port, port);
	return -1;
    }

    video_state->crtc_index_reg = *(uint8_t *)src;

    // Only do the passthrough IO for the first byte
    // the second byte will be done in the data register handler
    if (video_state->passthrough) {
	passthrough_out(port, src, 1);
    }

    if (length > 1) {
	if (crtc_data_write(core, port + 1, (uint8_t *) src + 1, length - 1, priv_data) != length - 1) {
	    return -1;
	}
    }

    return length;
}

/* graphics controller registers */
static int graphc_data_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    int index = video_state->graphc_index_reg;

    if (index < GRAPHC_REG_COUNT) {
    	*(uint8_t *) dest = video_state->graphc_data_regs[index];
    } else {
    	PrintError("Video %s: index %d out of range\n", __FUNCTION__, video_state->graphc_index_reg);
    	*(uint8_t *) dest = 0;
    }

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;    
}

static int graphc_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    int index = video_state->graphc_index_reg;
    uint8_t val = *(uint8_t *) src;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    if (index < GRAPHC_REG_COUNT) {
    	PrintDebug("Video: graphc[%d]=0x%x\n", index, val);
    	video_state->graphc_data_regs[index] = val;
    	registers_updated(video_state);
    } else {
    	PrintError("Video %s: index %d out of range\n", __FUNCTION__, video_state->graphc_index_reg);
    }

    return length;    
}

static int graphc_index_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(uint8_t *) dest = video_state->graphc_index_reg;

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int graphc_index_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 2);

    video_state->graphc_index_reg = *(uint8_t *) src;

    if (length > 1) {
	if (graphc_data_write(core, port + 1, (uint8_t *) src + 1, length - 1, priv_data) != length - 1) {
	    return -1;
	}
    }
    
    return length;    
}

/* attribute controller registers */
static int attrc_data_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    int index = video_state->attrc_index_reg;

    if (index < ATTRC_REG_COUNT) {
    	*(uint8_t *) dest = video_state->attrc_data_regs[index];
    } else {
    	PrintError("Video %s: index %d out of range\n", __FUNCTION__, video_state->attrc_index_reg);
    	*(uint8_t *) dest = 0;
    }

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;    
}

static int attrc_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    int index = video_state->attrc_index_reg;
    uint8_t val = *(uint8_t *) src;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    if (index < ATTRC_REG_COUNT) {
    	PrintDebug("Video: attrc[%d]=0x%x\n", index, val);
    	video_state->attrc_data_regs[index] = val;
    } else {
    	PrintError("Video %s: index %d out of range\n", __FUNCTION__, video_state->attrc_index_reg);
    }

    return length;    
}

static int attrc_index_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(uint8_t *) dest = video_state->attrc_index_reg;

    if (length > 1) {
	if (attrc_data_read(core, port + 1, (uint8_t *) dest + 1, length - 1, priv_data) != length - 1) {
	    return -1;
	}
    }
    
    handle_port_read(video_state, __FUNCTION__, port, dest, length, 2);
    return length;
}

static int attrc_index_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    video_state->attrc_index_reg = *(uint8_t *) src;

    return length;    
}

static int attrc_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    /* two registers in one, written in an alternating fashion */
    if (video_state->attrc_index_flipflop) {
    	video_state->attrc_index_flipflop = 0;
    	return attrc_data_write(core, port, src, length, priv_data);
    } else {
    	video_state->attrc_index_flipflop = 1;
    	return attrc_index_write(core, port, src, length, priv_data);
    }
}

/* video DAC palette registers */
static int dac_indexw_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;

    *(uint8_t *) dest = video_state->dac_indexw_reg;

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int dac_indexw_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    video_state->dac_indexw_reg = *(uint8_t *) src;
    video_state->dac_indexw_color = 0;

    return length;  
}

static int dac_indexr_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    video_state->dac_indexr_reg = *(uint8_t *) src;
    video_state->dac_indexr_color = 0;

    return length;  
}

static int dac_data_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    unsigned index;

    /* update palette */
    index = (unsigned) video_state->dac_indexr_reg * DAC_COLOR_COUNT + 
    	video_state->dac_indexr_color;
    V3_ASSERT(index < DAC_REG_COUNT);
    *(uint8_t *) dest = video_state->dac_data_regs[index];
    
    /* move on to next entry/color */
    if (++video_state->dac_indexr_color > DAC_COLOR_COUNT) {
    	video_state->dac_indexr_reg++;
    	video_state->dac_indexr_color -= DAC_COLOR_COUNT;
    }

    handle_port_read(video_state, __FUNCTION__, port, dest, length, 1);
    return length;
}

static int dac_data_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    struct video_internal * video_state = priv_data;
    unsigned index;
    handle_port_write(video_state, __FUNCTION__, port, src, length, 1);

    /* update palette */
    index = (unsigned) video_state->dac_indexw_reg * DAC_COLOR_COUNT + 
    	video_state->dac_indexw_color;
    V3_ASSERT(index < DAC_REG_COUNT);
    video_state->dac_data_regs[index] = *(uint8_t *) src;
    
    /* move on to next entry/color */
    if (++video_state->dac_indexw_color > DAC_COLOR_COUNT) {
    	video_state->dac_indexw_reg++;
    	video_state->dac_indexw_color -= DAC_COLOR_COUNT;
    }

    return length;
}

static int dac_pelmask_read(struct guest_info * core, uint16_t port, void * dest, uint_t length, void * priv_data) {
    return notimpl_port_read(priv_data, __FUNCTION__, port, dest, length);
}

static int dac_pelmask_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {
    return notimpl_port_write(priv_data, __FUNCTION__, port, src, length);
}


static int v3_cons_get_fb_graph(struct video_internal * state, uint8_t * dst, uint_t offset, uint_t length) {
    char c, text[80];
    uint_t textlength, textoffset, textsize;

    /* this call is not intended for graphics mode, tell the user this */

    /* center informative text on 10th line */
    snprintf(text, sizeof(text), "* * * GRAPHICS MODE %dx%d * * *",
        state->hres, state->vres);
    textlength = strlen(text);
    textoffset = (state->hchars * 9 + (state->hchars - textlength) / 2) * BYTES_PER_COL;
    textsize = textlength * BYTES_PER_COL;

    /* fill the buffer */
    while (length-- > 0) {
    	if (offset % BYTES_PER_COL) {
	    c = 0; /* attribute byte */
    	} else if (offset < textoffset || offset - textoffset >= textsize) {
	    c = ' '; /* unused character byte */
    	} else {
    	    c = text[(offset - textoffset) / BYTES_PER_COL];
    	}

   	*(dst++) = c;
    	offset++;
    }

    return 0;
}

static uint_t min_uint(uint_t x, uint_t y) {
    return (x < y) ? x : y;
}

int v3_cons_get_fb_text(struct video_internal * state, uint8_t * dst, uint_t offset, uint_t length) {
    uint8_t *framebuf;
    uint_t framebuf_offset, len1, len2;
    uint_t screen_byte_offset = state->screen_offset * BYTES_PER_COL;

    PrintVerbose("Video: getfb o=%d l=%d so=%d aa=0x%x al=0x%x hc=%d vc=%d\n",
    	offset, length, state->screen_offset, 
    	(unsigned) state->activefb_addr, (unsigned) state->activefb_len,
    	state->hchars, state->vchars);
    V3_ASSERT(state->activefb_addr >= START_ADDR);
    V3_ASSERT(state->activefb_addr + state->activefb_len <= END_ADDR);

    /* Copy memory with wrapping (should be configurable, but where else to get the data?) */
    framebuf = state->framebuf + (state->activefb_addr - START_ADDR);
    framebuf_offset = (screen_byte_offset + offset) % state->activefb_len;
    len1 = min_uint(length, state->activefb_len - framebuf_offset);
    len2 = length - len1;
    if (len1 > 0) memcpy(dst, framebuf + framebuf_offset, len1);
    if (len2 > 0) memcpy(dst + len1, framebuf, len2);

    return 0;
}

int v3_cons_get_fb(struct vm_device * frontend_dev, uint8_t * dst, uint_t offset, uint_t length) {
    struct video_internal * state = (struct video_internal *)frontend_dev->private_data;

    /* Deal with call depending on mode */
    if (state->graphmode) {
	return v3_cons_get_fb_graph(state, dst, offset, length);
    } else {
	return v3_cons_get_fb_text(state, dst, offset, length);
    }
}

static int cga_free(struct video_internal * video_state) {

    if (video_state->framebuf_pa) {
	PrintError("Freeing framebuffer PA %p\n", (void *)(video_state->framebuf_pa));
	V3_FreePages((void *)(video_state->framebuf_pa), (FRAMEBUF_SIZE / 4096));
    }

    v3_unhook_mem(video_state->dev->vm, V3_MEM_CORE_ANY, START_ADDR);

    V3_Free(video_state);

    return 0;
}


#ifdef V3_CONFIG_CHECKPOINT
static int cga_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct video_internal * cga = (struct video_internal *)private_data;

    v3_chkpt_save(ctx, "FRAMEBUFFER", FRAMEBUF_SIZE, cga->framebuf);

    V3_CHKPT_STD_SAVE(ctx, cga->misc_outp_reg);
    V3_CHKPT_STD_SAVE(ctx, cga->seq_index_reg);		
    V3_CHKPT_STD_SAVE(ctx, cga->seq_data_regs[SEQ_REG_COUNT]);	
    V3_CHKPT_STD_SAVE(ctx, cga->crtc_index_reg);		
    V3_CHKPT_STD_SAVE(ctx, cga->crtc_data_regs[CRTC_REG_COUNT]);
    V3_CHKPT_STD_SAVE(ctx, cga->graphc_index_reg);		
    V3_CHKPT_STD_SAVE(ctx, cga->graphc_data_regs[GRAPHC_REG_COUNT]);
    V3_CHKPT_STD_SAVE(ctx, cga->attrc_index_flipflop);
    V3_CHKPT_STD_SAVE(ctx, cga->attrc_index_reg);	
    V3_CHKPT_STD_SAVE(ctx, cga->attrc_data_regs[ATTRC_REG_COUNT]);	
    V3_CHKPT_STD_SAVE(ctx, cga->dac_indexr_reg);	
    V3_CHKPT_STD_SAVE(ctx, cga->dac_indexr_color);
    V3_CHKPT_STD_SAVE(ctx, cga->dac_indexw_reg);		
    V3_CHKPT_STD_SAVE(ctx, cga->dac_indexw_color);
    V3_CHKPT_STD_SAVE(ctx, cga->dac_data_regs[DAC_REG_COUNT]);

    V3_CHKPT_STD_SAVE(ctx, cga->activefb_addr);
    V3_CHKPT_STD_SAVE(ctx, cga->activefb_len);
    V3_CHKPT_STD_SAVE(ctx, cga->iorange);
    V3_CHKPT_STD_SAVE(ctx, cga->vres);
    V3_CHKPT_STD_SAVE(ctx, cga->hres);
    V3_CHKPT_STD_SAVE(ctx, cga->vchars);
    V3_CHKPT_STD_SAVE(ctx, cga->hchars);
    V3_CHKPT_STD_SAVE(ctx, cga->graphmode);

    V3_CHKPT_STD_SAVE(ctx, cga->dirty);
    V3_CHKPT_STD_SAVE(ctx, cga->reschanged);

    V3_CHKPT_STD_SAVE(ctx, cga->passthrough);

    v3_chkpt_save_16(ctx, "SCREEN_OFFSET", &(cga->screen_offset));
    v3_chkpt_save_16(ctx, "CURSOR_OFFSET", &(cga->cursor_offset));

    return 0;
}

static int cga_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct video_internal * cga = (struct video_internal *)private_data;

    v3_chkpt_load(ctx, "FRAMEBUFFER", FRAMEBUF_SIZE, cga->framebuf);


    V3_CHKPT_STD_LOAD(ctx, cga->misc_outp_reg);
    V3_CHKPT_STD_LOAD(ctx, cga->seq_index_reg);		
    V3_CHKPT_STD_LOAD(ctx, cga->seq_data_regs[SEQ_REG_COUNT]);	
    V3_CHKPT_STD_LOAD(ctx, cga->crtc_index_reg);		
    V3_CHKPT_STD_LOAD(ctx, cga->crtc_data_regs[CRTC_REG_COUNT]);
    V3_CHKPT_STD_LOAD(ctx, cga->graphc_index_reg);		
    V3_CHKPT_STD_LOAD(ctx, cga->graphc_data_regs[GRAPHC_REG_COUNT]);
    V3_CHKPT_STD_LOAD(ctx, cga->attrc_index_flipflop);
    V3_CHKPT_STD_LOAD(ctx, cga->attrc_index_reg);	
    V3_CHKPT_STD_LOAD(ctx, cga->attrc_data_regs[ATTRC_REG_COUNT]);	
    V3_CHKPT_STD_LOAD(ctx, cga->dac_indexr_reg);	
    V3_CHKPT_STD_LOAD(ctx, cga->dac_indexr_color);
    V3_CHKPT_STD_LOAD(ctx, cga->dac_indexw_reg);		
    V3_CHKPT_STD_LOAD(ctx, cga->dac_indexw_color);
    V3_CHKPT_STD_LOAD(ctx, cga->dac_data_regs[DAC_REG_COUNT]);

    V3_CHKPT_STD_LOAD(ctx, cga->activefb_addr);
    V3_CHKPT_STD_LOAD(ctx, cga->activefb_len);
    V3_CHKPT_STD_LOAD(ctx, cga->iorange);
    V3_CHKPT_STD_LOAD(ctx, cga->vres);
    V3_CHKPT_STD_LOAD(ctx, cga->hres);
    V3_CHKPT_STD_LOAD(ctx, cga->vchars);
    V3_CHKPT_STD_LOAD(ctx, cga->hchars);
    V3_CHKPT_STD_LOAD(ctx, cga->graphmode);

    V3_CHKPT_STD_LOAD(ctx, cga->dirty);
    V3_CHKPT_STD_LOAD(ctx, cga->reschanged);

    V3_CHKPT_STD_LOAD(ctx, cga->passthrough);

    v3_chkpt_load_16(ctx, "SCREEN_OFFSET", &(cga->screen_offset));
    v3_chkpt_load_16(ctx, "CURSOR_OFFSET", &(cga->cursor_offset));


    return 0;
}

#endif


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))cga_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = cga_save, 
    .load = cga_load
#endif

};

static int cga_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct video_internal * video_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * passthrough_str = v3_cfg_val(cfg, "passthrough");
    int ret = 0;
    
    PrintDebug("video: init_device\n");

    video_state = (struct video_internal *)V3_Malloc(sizeof(struct video_internal));

    if (!video_state) {
	PrintError("Cannot allocate space for CGA state\n");
	return -1;
    }

    memset(video_state, 0, sizeof(struct video_internal));

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, video_state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(video_state);
	return -1;
    }
  
    video_state->dev = dev;

    video_state->framebuf_pa = (addr_t)V3_AllocPages(FRAMEBUF_SIZE / 4096);

    if (!video_state->framebuf_pa) { 
	PrintError("Cannot allocate frame buffer\n");
	V3_Free(video_state);
	return -1;
    }

    video_state->framebuf = V3_VAddr((void *)(video_state->framebuf_pa));

    memset(video_state->framebuf, 0, FRAMEBUF_SIZE);

    PrintDebug("PA of array: %p\n", (void *)(video_state->framebuf_pa));

    if ((passthrough_str != NULL) &&
	(strcasecmp(passthrough_str, "enable") == 0)) {;
	video_state->passthrough = 1;
    }


    if (video_state->passthrough) {
	PrintDebug("Enabling CGA Passthrough\n");
	if (v3_hook_write_mem(vm, V3_MEM_CORE_ANY, START_ADDR, END_ADDR, 
			      START_ADDR, &video_write_mem, dev) == -1) {
	    PrintError("\n\nVideo Hook failed.\n\n");
	    return -1;
	}
    } else {
	if (v3_hook_write_mem(vm, V3_MEM_CORE_ANY, START_ADDR, END_ADDR, 
			      video_state->framebuf_pa, &video_write_mem, dev) == -1) {
	    PrintError("\n\nVideo Hook failed.\n\n");
	    return -1;
	}
    }

    /* registers according to http://www.mcamafia.de/pdf/ibm_vgaxga_trm2.pdf */

    /* general registers */
    ret |= v3_dev_hook_io(dev, 0x3cc, &misc_outp_read, NULL);
    ret |= v3_dev_hook_io(dev, 0x3c2, &inp_status0_read, &misc_outp_write);
    ret |= v3_dev_hook_io(dev, 0x3ba, &inp_status1_read, &feat_ctrl_write);
    ret |= v3_dev_hook_io(dev, 0x3da, &inp_status1_read, &feat_ctrl_write);
    ret |= v3_dev_hook_io(dev, 0x3ca, &feat_ctrl_read, NULL);
    ret |= v3_dev_hook_io(dev, 0x3c3, &video_enable_read, &video_enable_write);

    /* sequencer registers */
    ret |= v3_dev_hook_io(dev, 0x3c4, &seq_index_read, &seq_index_write);
    ret |= v3_dev_hook_io(dev, 0x3c5, &seq_data_read, &seq_data_write);

    /* CRT controller registers, both CGA and VGA ranges */
    ret |= v3_dev_hook_io(dev, 0x3b4, &crtc_index_read, &crtc_index_write);
    ret |= v3_dev_hook_io(dev, 0x3b5, &crtc_data_read, &crtc_data_write);
    ret |= v3_dev_hook_io(dev, 0x3d4, &crtc_index_read, &crtc_index_write);
    ret |= v3_dev_hook_io(dev, 0x3d5, &crtc_data_read, &crtc_data_write);

    /* graphics controller registers */
    ret |= v3_dev_hook_io(dev, 0x3ce, &graphc_index_read, &graphc_index_write);
    ret |= v3_dev_hook_io(dev, 0x3cf, &graphc_data_read, &graphc_data_write);

    /* attribute controller registers */
    ret |= v3_dev_hook_io(dev, 0x3c0, &attrc_index_read, &attrc_write);
    ret |= v3_dev_hook_io(dev, 0x3c1, &attrc_data_read, NULL);

    /* video DAC palette registers */
    ret |= v3_dev_hook_io(dev, 0x3c8, &dac_indexw_read, &dac_indexw_write);
    ret |= v3_dev_hook_io(dev, 0x3c7, NULL, &dac_indexr_write);
    ret |= v3_dev_hook_io(dev, 0x3c9, &dac_data_read, &dac_data_write);
    ret |= v3_dev_hook_io(dev, 0x3c6, &dac_pelmask_read, &dac_pelmask_write);

    if (ret != 0) {
	PrintError("Error allocating VGA IO ports\n");
	v3_remove_device(dev);
	return -1;
    }

    /* initialize the state as it is at boot time */
    registers_initialize(video_state);

    return 0;
}

device_register("CGA_VIDEO", cga_init);


int v3_console_register_cga(struct vm_device * cga_dev, struct v3_console_ops * ops, void * private_data) {
    struct video_internal * video_state = (struct video_internal *)cga_dev->private_data;
    
    video_state->ops = ops;
    video_state->private_data = private_data;

    return 0;
}
