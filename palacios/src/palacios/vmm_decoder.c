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


#include <palacios/vmm_decoder.h>





uint8_t v3_get_prefixes(uint8_t * instr, struct x86_prefixes * prefixes) {
    uint8_t * instr_cursor = instr;

    while (1) {
	switch (*instr_cursor) {
	    case 0xF0:      // lock
		prefixes->lock = 1;
		break;

	    case 0xF2:      // REPNE/REPNZ
		prefixes->repnz = 1;
		prefixes->repne = 1;
		break;

	    case 0xF3:      // REP or REPE/REPZ
		prefixes->rep = 1;
		prefixes->repe = 1;
		prefixes->repz = 1; 
		break;

	    case 0x2E:      // CS override or Branch hint not taken (with Jcc instr_cursors)
		prefixes->cs_override = 1;
		prefixes->br_not_taken = 1;
		break;

	    case 0x36:      // SS override
		prefixes->ss_override = 1;
		break;

	    case 0x3E:      // DS override or Branch hint taken (with Jcc instr_cursors)
		prefixes->ds_override = 1;
		prefixes->br_taken = 1;
		break;

	    case 0x26:      // ES override
		prefixes->es_override = 1;
		break;

	    case 0x64:      // FS override
		prefixes->fs_override = 1;
		break;
      
	    case 0x65:      // GS override
		prefixes->gs_override = 1;
		break;

	    case 0x66:      // operand size override
		prefixes->op_size = 1;
		break;

	    case 0x67:    // address size override
		prefixes->addr_size = 1;
		break;

	    default:
		return (instr_cursor - instr);
	}

	instr_cursor++;
    }
}

void v3_strip_rep_prefix(uchar_t * instr, int length) {
    int read_ctr = 0;
    int write_ctr = 0;
    int found = 0;

    while (read_ctr < length) {
	if ((!found) && 
	    ( (instr[read_ctr] == 0xF2) ||
	      (instr[read_ctr] == 0xF3))) {
	    read_ctr++;
	    found = 1;
	} else {
	    instr[write_ctr] = instr[read_ctr];
	    write_ctr++;
	    read_ctr++;
	}
    }
}
