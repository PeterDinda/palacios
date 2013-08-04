/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
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

#include <palacios/vmm_lock.h>

#include <devices/ide.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vm_guest.h>


#ifndef V3_CONFIG_DEBUG_NVRAM
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define NVRAM_REG_PORT  0x70
#define NVRAM_DATA_PORT 0x71

#define NVRAM_RTC_IRQ   0x8


typedef enum {NVRAM_READY, NVRAM_REG_POSTED} nvram_state_t;


#define NVRAM_REG_MAX   256


// These are borrowed from Bochs, which borrowed from
// Ralf Brown's interupt list, and extended
#define NVRAM_REG_SEC                     0x00
#define NVRAM_REG_SEC_ALARM               0x01
#define NVRAM_REG_MIN                     0x02
#define NVRAM_REG_MIN_ALARM               0x03
#define NVRAM_REG_HOUR                    0x04
#define NVRAM_REG_HOUR_ALARM              0x05
#define NVRAM_REG_WEEK_DAY                0x06
#define NVRAM_REG_MONTH_DAY               0x07
#define NVRAM_REG_MONTH                   0x08
#define NVRAM_REG_YEAR                    0x09
#define NVRAM_REG_STAT_A                  0x0a
#define NVRAM_REG_STAT_B                  0x0b
#define NVRAM_REG_STAT_C                  0x0c
#define NVRAM_REG_STAT_D                  0x0d
#define NVRAM_REG_DIAGNOSTIC_STATUS       0x0e  
#define NVRAM_REG_SHUTDOWN_STATUS         0x0f

#define NVRAM_IBM_HD_DATA                 0x12
#define NVRAM_IDE_TRANSLATION             0x39

#define NVRAM_REG_FLOPPY_TYPE             0x10
#define NVRAM_REG_EQUIPMENT_BYTE          0x14

#define NVRAM_REG_BASE_MEMORY_HIGH        0x16
#define NVRAM_REG_BASE_MEMORY_LOW         0x15

#define NVRAM_REG_EXT_MEMORY_HIGH         0x18
#define NVRAM_REG_EXT_MEMORY_LOW          0x17

#define NVRAM_REG_EXT_MEMORY_2ND_HIGH     0x31
#define NVRAM_REG_EXT_MEMORY_2ND_LOW      0x30

#define NVRAM_REG_BOOTSEQ_OLD             0x2d

#define NVRAM_REG_AMI_BIG_MEMORY_HIGH     0x35
#define NVRAM_REG_AMI_BIG_MEMORY_LOW      0x34

#define NVRAM_REG_CSUM_HIGH               0x2e
#define NVRAM_REG_CSUM_LOW                0x2f
#define NVRAM_REG_IBM_CENTURY_BYTE        0x32  
#define NVRAM_REG_IBM_PS2_CENTURY_BYTE    0x37  

#define NVRAM_REG_BOOTSEQ_NEW_FIRST       0x3D
#define NVRAM_REG_BOOTSEQ_NEW_SECOND      0x38

#define CHECKSUM_REGION_FIRST_BYTE        0x10
#define CHECKSUM_REGION_LAST_BYTE         0x2d

// Following fields are used by SEABIOS
#define NVRAM_REG_HIGHMEM_LOW             0x5b
#define NVRAM_REG_HIGHMEM_MID             0x5c
#define NVRAM_REG_HIGHMEM_HIGH            0x5d
#define NVRAM_REG_SMPCPUS                 0x5f

struct nvram_internal {
    nvram_state_t dev_state;
    uint8_t       thereg;
    uint8_t       mem_state[NVRAM_REG_MAX];
    uint8_t       reg_map[NVRAM_REG_MAX / 8];

    struct vm_device * ide;

    struct v3_vm_info * vm;
    
    struct v3_timer   *timer;

    v3_lock_t nvram_lock;

    uint64_t        us;   //microseconds - for clock update - zeroed every second
    uint64_t        pus;  //microseconds - for periodic interrupt - cleared every period
};


struct rtc_stata {
    uint8_t        rate   : 4;  // clock rate = 65536Hz / 2 rate (0110=1024 Hz)
    uint8_t        basis  : 3;  // time base, 010 = 32,768 Hz
    uint8_t        uip    : 1;  // 1=update in progress
} __attribute__((__packed__)) __attribute__((__aligned__ (1)))  ;

struct rtc_statb {
    uint8_t        sum    : 1;  // 1=summer (daylight savings)
    uint8_t        h24    : 1;  // 1=24h clock
    uint8_t        dm     : 1;  // 0=date/time is in bcd, 1=binary
    uint8_t        rec    : 1;  // 1=rectangular signal
    uint8_t        ui     : 1;  // 1=update interrupt
    uint8_t        ai     : 1;  // 1=alarm interrupt
    uint8_t        pi     : 1;  // 1=periodic interrupt
    uint8_t        set    : 1;  // 1=blocked update
} __attribute__((__packed__))  __attribute__((__aligned__ (1))) ;

struct rtc_statc {
    uint8_t        res    : 4;  // reserved
    uint8_t        uf     : 1;  // 1=source of interrupt is update
    uint8_t        af     : 1;  // 1=source of interrupt is alarm interrupt
    uint8_t        pf     : 1;  // 1=source of interrupt is periodic interrupt
    uint8_t        irq    : 1;  // 1=interrupt requested
}  __attribute__((__packed__))  __attribute__((__aligned__ (1))) ;

struct rtc_statd {
    uint8_t        res    : 7;  // reserved
    uint8_t        val    : 1;  // 1=cmos ram data is OK
}  __attribute__((__packed__))  __attribute__((__aligned__ (1))) ;




struct bcd_num {
    uint8_t bot : 4;
    uint8_t top : 4;
} __attribute__((packed));;



static void set_reg_num(struct nvram_internal * nvram, uint8_t reg_num) {
    int major = (reg_num / 8);
    int minor = reg_num % 8;

    nvram->reg_map[major] |= (0x1 << minor);
}

static int is_reg_set(struct nvram_internal * nvram, uint8_t reg_num) {
    int major = (reg_num / 8);
    int minor = reg_num % 8;
    
    return (nvram->reg_map[major] & (0x1 << minor)) ? 1 : 0;
}


static void set_memory(struct nvram_internal * nvram, uint8_t reg, uint8_t val) {
    set_reg_num(nvram, reg);
    nvram->mem_state[reg] = val;
}

static int get_memory(struct nvram_internal * nvram, uint8_t reg, uint8_t * val) {

    if (!is_reg_set(nvram, reg)) {
	*val = 0;
	return -1;
    }

    *val = nvram->mem_state[reg];
    return 0;
}


static uint8_t add_to(uint8_t * left, uint8_t * right, uint8_t bcd) {
    uint8_t temp;

    if (bcd) { 
	struct bcd_num * bl = (struct bcd_num *)left;
	struct bcd_num * br = (struct bcd_num *)right;
	uint8_t carry = 0;

	bl->bot += br->bot;
	carry = bl->bot / 0xa;
	bl->bot %= 0xa;

	bl->top += carry + br->top;
	carry = bl->top / 0xa;
	bl->top %= 0xa;

	return carry;
    } else {
	temp = *left;
	*left += *right;

	if (*left < temp) { 
	    return 1;
	} else {
	    return 0;
	}
    }
}


static uint8_t days_in_month(uint8_t month, uint8_t bcd) {
    // This completely ignores Julian / Gregorian stuff right now

    if (bcd) { 

	switch (month) 
	    {
		case 0x1: //jan
		case 0x3: //march
		case 0x5: //may
		case 0x7: //july
		case 0x8: //aug
		case 0x10: //oct
		case 0x12: //dec
		    return 0x31;
		    break;
		case 0x4: //april
		case 0x6: //june
		case 0x9: //sept
		case 0x11: //nov
		    return 0x30;
		    break;
		case 0x2: //feb
		    return 0x28;
		    break;
		default:
		    return 0x30;
	    }
    
    } else {

	switch (month) 
	    {
		case 1: //jan
		case 3: //march
		case 5: //may
		case 7: //july
		case 8: //aug
		case 10: //oct
		case 12: //dec
		    return 31;
		    break;
		case 4: //april
		case 6: //june
		case 9: //sept
		case 11: //nov
		    return 30;
		    break;
		case 2: //feb
		    return 28;
		    break;
		default:
		    return 30;
	    }
    }
}


static void update_time(struct nvram_internal * data, uint64_t period_us) {
    struct rtc_stata * stata = (struct rtc_stata *)&((data->mem_state[NVRAM_REG_STAT_A]));
    struct rtc_statb * statb = (struct rtc_statb *)&((data->mem_state[NVRAM_REG_STAT_B]));
    struct rtc_statc * statc = (struct rtc_statc *)&((data->mem_state[NVRAM_REG_STAT_C]));
    //struct rtc_statd *statd = (struct rtc_statd *) &((data->mem_state[NVRAM_REG_STAT_D]));
    uint8_t * sec      = (uint8_t *)&(data->mem_state[NVRAM_REG_SEC]);
    uint8_t * min      = (uint8_t *)&(data->mem_state[NVRAM_REG_MIN]);
    uint8_t * hour     = (uint8_t *)&(data->mem_state[NVRAM_REG_HOUR]);
    uint8_t * weekday  = (uint8_t *)&(data->mem_state[NVRAM_REG_WEEK_DAY]);
    uint8_t * monthday = (uint8_t *)&(data->mem_state[NVRAM_REG_MONTH_DAY]);
    uint8_t * month    = (uint8_t *)&(data->mem_state[NVRAM_REG_MONTH]);
    uint8_t * year     = (uint8_t *)&(data->mem_state[NVRAM_REG_YEAR]);
    uint8_t * cent     = (uint8_t *)&(data->mem_state[NVRAM_REG_IBM_CENTURY_BYTE]);
    uint8_t * cent_ps2 = (uint8_t *)&(data->mem_state[NVRAM_REG_IBM_PS2_CENTURY_BYTE]);
    uint8_t * seca     = (uint8_t *)&(data->mem_state[NVRAM_REG_SEC_ALARM]);
    uint8_t * mina     = (uint8_t *)&(data->mem_state[NVRAM_REG_MIN_ALARM]);
    uint8_t * houra    = (uint8_t *)&(data->mem_state[NVRAM_REG_HOUR_ALARM]);
    uint8_t hour24;

    uint8_t bcd = (statb->dm == 0);
    uint8_t carry = 0;
    uint8_t nextday = 0;
    uint32_t  periodic_period;

    PrintDebug(VM_NONE, VCORE_NONE, "nvram: update_time by %llu microseocnds\n",period_us);
  
    // We will set these flags on exit
    statc->irq = 0;
    statc->pf = 0;
    statc->af = 0;
    statc->uf = 0;

    // We will reset us after one second
    data->us += period_us;
    // We will reset pus after one periodic_period
    data->pus += period_us;

    if (data->us > 1000000) { 
	carry = 1;
	carry = add_to(sec, &carry, bcd);

	if (carry) { 
	    PrintError(VM_NONE, VCORE_NONE, "nvram: somehow managed to get a carry in second update\n"); 
	}

	if ( (bcd && (*sec == 0x60)) || 
	     ((!bcd) && (*sec == 60))) { 
  
	    *sec = 0;

	    carry = 1;
	    carry = add_to(min, &carry, bcd);
	    if (carry) { 
		PrintError(VM_NONE, VCORE_NONE, "nvram: somehow managed to get a carry in minute update\n"); 
	    }

	    if ( (bcd && (*min == 0x60)) || 
		 ((!bcd) && (*min == 60))) { 

		*min = 0;
		hour24 = *hour;

		if (!(statb->h24)) { 

		    if (hour24 & 0x80) { 
			hour24 &= 0x8f;
			uint8_t temp = ((bcd) ? 0x12 : 12);
			add_to(&hour24, &temp, bcd);
		    }
		}

		carry = 1;
		carry = add_to(&hour24, &carry, bcd);
		if (carry) { 
		    PrintError(VM_NONE, VCORE_NONE, "nvram: somehow managed to get a carry in hour update\n"); 
		}

		if ( (bcd && (hour24 == 0x24)) || 
		     ((!bcd) && (hour24 == 24))) { 
		    carry = 1;
		    nextday = 1;
		    hour24 = 0;
		} else {
		    carry = 0;
		}


		if (statb->h24) { 
		    *hour = hour24;
		} else {
		    if ( (bcd && (hour24 < 0x12)) || 
			 ((!bcd) && (hour24 < 12))) { 
			*hour = hour24;

		    } else {

			if (!bcd) { 
			    *hour = (hour24 - 12) | 0x80;
			} else {
			    *hour = hour24;
			    struct bcd_num * n = (struct bcd_num *)hour;

			    if (n->bot < 0x2) { 
				n->top--;
				n->bot += 0xa;
			    }

			    n->bot -= 0x2;
			    n->top -= 0x1;
			}
		    }
		}

		// now see if we need to carry into the days and further
		if (nextday) { 
		    carry = 1;
		    add_to(weekday, &carry, bcd);

		    *weekday %= 0x7;  // same regardless of bcd

		    if ((*monthday) != days_in_month(*month, bcd)) {
			add_to(monthday, &carry, bcd);
		    } else {
			*monthday = 0x1;

			carry = 1;
			add_to(month, &carry, bcd);

			if ( (bcd && (*month == 0x13)) || 
			     ((!bcd) && (*month == 13))) { 
			    *month = 1; // same for both 

			    carry = 1;
			    carry = add_to(year, &carry, bcd);

			    if ( (bcd && carry) || 
				 ((!bcd) && (*year == 100))) { 
				*year = 0;
				carry = 1;
				add_to(cent, &carry, bcd);
				*cent_ps2 = *cent;
			    }
			}
		    }
		}
	    }
	}


	data->us -= 1000000;
	// OK, now check for the alarm, if it is set to interrupt
	if (statb->ai) { 
	    if ((*sec == *seca) && (*min == *mina) && (*hour == *houra)) { 
		statc->af = 1;
		PrintDebug(VM_NONE, VCORE_NONE, "nvram: interrupt on alarm\n");
	    }
	}
    }

    if (statb->pi) { 
	periodic_period = 1000000 / (65536 / (0x1 << stata->rate));
	if (data->pus >= periodic_period) { 
	    statc->pf = 1;
	    data->pus -= periodic_period;
	    PrintDebug(VM_NONE, VCORE_NONE, "nvram: interrupt on periodic\n");
	}
    }

    if (statb->ui) { 
	statc->uf = 1;
	PrintDebug(VM_NONE, VCORE_NONE, "nvram: interrupt on update\n");
    }

    statc->irq = (statc->pf || statc->af || statc->uf);
  
    PrintDebug(VM_NONE, VCORE_NONE, "nvram: time is now: YMDHMS: 0x%x:0x%x:0x%x:0x%x:0x%x,0x%x bcd=%d\n", *year, *month, *monthday, *hour, *min, *sec,bcd);
  
    // Interrupt associated VM, if needed
    if (statc->irq) { 
	PrintDebug(VM_NONE, VCORE_NONE, "nvram: injecting interrupt\n");
	v3_raise_irq(data->vm, NVRAM_RTC_IRQ);
    }
}


static void nvram_update_timer(struct guest_info *vm,
			       ullong_t           cpu_cycles,
			       ullong_t           cpu_freq,
			       void              *priv_data)
{
    struct nvram_internal *nvram_state = (struct nvram_internal *)priv_data;
    uint64_t period_us;

    
    // cpu freq in khz
    period_us = (1000*cpu_cycles/cpu_freq);

    update_time(nvram_state,period_us);

}


static void set_memory_size(struct nvram_internal * nvram, addr_t bytes) {
    // 1. Conventional Mem: 0-640k in K
    // 2. Extended Mem: 0-16MB in K
    // 3. Big Mem: 0-4G in 64K
    // 4. High Mem: 4G-... in 64K

    // at most 640K of conventional memory
    {
	uint16_t memk = 0;

	if (bytes > (640 * 1024)) {
	    memk = 640;
	} else {
	    memk = bytes / 1024;
	}

	set_memory(nvram, NVRAM_REG_BASE_MEMORY_HIGH, (memk >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_BASE_MEMORY_LOW, memk & 0x00ff);
    }

    // set extended memory - first 1 MB is lost to 640K chunk
    // extended memory is min(0MB, bytes - 1MB)
    {
	uint16_t memk = 0;

	if (bytes >= (1024 * 1024)) {
	    memk = (bytes - (1024 * 1024)) / 1024;
	}
	
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_HIGH, (memk >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_LOW, memk & 0x00ff);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_2ND_HIGH, (memk >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_2ND_LOW, memk & 0x00ff);
    }

    // Set the extended memory beyond 16 MB in 64k chunks
    // this is min(0, bytes - 16MB)
    {
	uint16_t mem_chunks = 0;

	if (bytes >= (1024 * 1024 * 16)) {
	    mem_chunks = (bytes - (1024 * 1024 * 16)) / (1024 * 64);
	}
	
	set_memory(nvram, NVRAM_REG_AMI_BIG_MEMORY_HIGH, (mem_chunks >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_AMI_BIG_MEMORY_LOW, mem_chunks & 0x00ff);
    }

    // Set high (>4GB) memory size
    {

	uint32_t high_mem_chunks = 0;

	if (bytes >= (1024LL * 1024LL * 1024LL * 4LL)) {
	    high_mem_chunks = (bytes - (1024LL * 1024LL * 1024LL * 4LL))  / (1024 * 64);
	}

	set_memory(nvram, NVRAM_REG_HIGHMEM_LOW, high_mem_chunks & 0xff);
	set_memory(nvram, NVRAM_REG_HIGHMEM_MID, (high_mem_chunks >> 8) & 0xff);
	set_memory(nvram, NVRAM_REG_HIGHMEM_HIGH, (high_mem_chunks >> 16) & 0xff);
    }

    return;
}



static void init_harddrives(struct nvram_internal * nvram) {
    uint8_t hd_data = 0;
    uint32_t cyls;
    uint32_t sects;
    uint32_t heads;
    int i = 0;
    int info_base_reg = 0x1b;
    int type_reg = 0x19;

    // 0x19 == first drive type
    // 0x1a == second drive type

    // 0x1b == first drive geometry base
    // 0x24 == second drive geometry base

    // It looks like the BIOS only tracks the disks on the first channel at 0x12?
    for (i = 0; i < 2; i++) {
	if (v3_ide_get_geometry(nvram->ide->private_data, 0, i, &cyls, &heads, &sects) == 0) {

	    int info_reg = info_base_reg + (i * 9);

	    set_memory(nvram, type_reg + i, 0x2f);

	    set_memory(nvram, info_reg, cyls & 0xff);
	    set_memory(nvram, info_reg + 1, (cyls >> 8) & 0xff);
	    set_memory(nvram, info_reg + 2, heads & 0xff);

	    // Write precomp cylinder (1 and 2)
	    set_memory(nvram, info_reg + 3, 0xff);
	    set_memory(nvram, info_reg + 4, 0xff);

	    // harddrive control byte 
	    set_memory(nvram, info_reg + 5, 0xc0 | ((heads > 8) << 3));

	    set_memory(nvram, info_reg + 6, cyls & 0xff);
	    set_memory(nvram, info_reg + 7, (cyls >> 8) & 0xff);

	    set_memory(nvram, info_reg + 8, sects & 0xff);
	    
	    hd_data |= (0xf0 >> (i * 4));
	}
    }

    set_memory(nvram, NVRAM_IBM_HD_DATA, hd_data);
    
    {
#define TRANSLATE_NONE  0x0
#define TRANSLATE_LBA   0x1
#define TRANSLATE_LARGE 0x2
#define TRANSLATE_RECHS 0x3
	// We're going to do LBA translation for everything...
	uint8_t trans = 0;

	for (i = 0; i < 4; i++) {
	    int chan_num = i / 2;
	    int drive_num = i % 2;
	    uint32_t tmp[3];

	    if (v3_ide_get_geometry(nvram->ide->private_data, chan_num, drive_num, &tmp[0], &tmp[1], &tmp[2]) == 0) {
		trans |= TRANSLATE_LBA << (i * 2);
	    }
	}

	set_memory(nvram, NVRAM_IDE_TRANSLATION, trans);
    }
}

static uint16_t compute_checksum(struct nvram_internal * nvram) {
    uint16_t checksum = 0;
    uint8_t reg = 0;
    uint8_t val = 0;
    
    /* add all fields between the RTC and the checksum fields */
    for (reg = CHECKSUM_REGION_FIRST_BYTE; reg < CHECKSUM_REGION_LAST_BYTE; reg++) {
        /* unset fields are considered zero so get_memory can be ignored */
        get_memory(nvram, reg, &val);
	checksum += val;
    }
		
    return checksum;
}

static int init_nvram_state(struct v3_vm_info * vm, struct nvram_internal * nvram) {
    uint16_t checksum = 0;

    memset(nvram->mem_state, 0, NVRAM_REG_MAX);
    memset(nvram->reg_map, 0, NVRAM_REG_MAX / 8);

    v3_lock_init(&(nvram->nvram_lock));

    //
    // 2 1.44 MB floppy drives
    //
#if 1
    set_memory(nvram, NVRAM_REG_FLOPPY_TYPE, 0x44);
#else
    set_memory(nvram, NVRAM_REG_FLOPPY_TYPE, 0x00);
#endif

    //
    // For old boot sequence style, do floppy first
    //
    set_memory(nvram, NVRAM_REG_BOOTSEQ_OLD, 0x10);

#if 0
    // For new boot sequence style, do floppy, cd, then hd
    set_memory(nvram, NVRAM_REG_BOOTSEQ_NEW_FIRST, 0x31);
    set_memory(nvram, NVRAM_REG_BOOTSEQ_NEW_SECOND, 0x20);
#endif

    // For new boot sequence style, do cd, hd, floppy
    set_memory(nvram, NVRAM_REG_BOOTSEQ_NEW_FIRST, 0x23);
    set_memory(nvram, NVRAM_REG_BOOTSEQ_NEW_SECOND, 0x10);
  
  
    // Set equipment byte to note 2 floppies, vga display, keyboard,math,floppy
    set_memory(nvram, NVRAM_REG_EQUIPMENT_BYTE, 0x4f);
    // set_memory(nvram, NVRAM_REG_EQUIPMENT_BYTE, 0xf);
  

    // Set the shutdown status gently
    // soft reset
    set_memory(nvram, NVRAM_REG_SHUTDOWN_STATUS, 0x0);


    // RTC status A
    // 00100110 = no update in progress, base=32768 Hz, rate = 1024 Hz
    set_memory(nvram, NVRAM_REG_STAT_A, 0x26); 

    // RTC status B
    // 00000010 = not setting, no interrupts, blocked rect signal, bcd mode (bit 3 = 0), 24 hour, normal time
    set_memory(nvram, NVRAM_REG_STAT_B, 0x02); 


    // RTC status C
    // No IRQ requested, result not do to any source
    set_memory(nvram, NVRAM_REG_STAT_C, 0x00);

    // RTC status D
    // Battery is OK
    set_memory(nvram, NVRAM_REG_STAT_D, 0x80);


    // january 1, 2008, 00:00:00
    set_memory(nvram, NVRAM_REG_SEC, 0x00);
    set_memory(nvram, NVRAM_REG_SEC_ALARM, 0x00);
    set_memory(nvram, NVRAM_REG_MIN, 0x00);
    set_memory(nvram, NVRAM_REG_MIN_ALARM, 0x00);
    set_memory(nvram, NVRAM_REG_HOUR, 0x00);
    set_memory(nvram, NVRAM_REG_HOUR_ALARM, 0x00);

    set_memory(nvram, NVRAM_REG_MONTH, 0x01);
    set_memory(nvram, NVRAM_REG_MONTH_DAY, 0x1);
    set_memory(nvram, NVRAM_REG_WEEK_DAY, 0x1);
    set_memory(nvram, NVRAM_REG_YEAR, 0x08);
    set_memory(nvram, NVRAM_REG_IBM_CENTURY_BYTE, 0x20);
    set_memory(nvram, NVRAM_REG_IBM_PS2_CENTURY_BYTE, 0x20);

    set_memory(nvram, NVRAM_REG_DIAGNOSTIC_STATUS, 0x00);
    
    nvram->us = 0;
    nvram->pus = 0;

    set_memory_size(nvram, vm->mem_size);
    init_harddrives(nvram);

    set_memory(nvram, NVRAM_REG_SMPCPUS, vm->num_cores - 1);
    
    /* compute checksum (must follow all assignments here) */
    checksum = compute_checksum(nvram);
    set_memory(nvram, NVRAM_REG_CSUM_HIGH, (checksum >> 8) & 0xff);
    set_memory(nvram, NVRAM_REG_CSUM_LOW, checksum & 0xff);

    
    
    nvram->dev_state = NVRAM_READY;
    nvram->thereg = 0;

    return 0;
}






static int nvram_write_reg_port(struct guest_info * core, uint16_t port,
				void * src, uint_t length, void * priv_data) {
    uint8_t reg;
    struct nvram_internal * data = priv_data;

    memcpy(&reg,src,1);

    data->thereg = reg & 0x7f;  //discard NMI bit if it's there
    
    PrintDebug(core->vm_info, core, "nvram: Writing To NVRAM reg: 0x%x (NMI_disable=%d)\n", data->thereg,reg>>7);

    return 1;
}

static int nvram_read_data_port(struct guest_info * core, uint16_t port,
				void * dst, uint_t length, void * priv_data) {

    struct nvram_internal * data = priv_data;

    addr_t irq_state = v3_lock_irqsave(data->nvram_lock);

    if (get_memory(data, data->thereg, (uint8_t *)dst) == -1) {
	PrintError(core->vm_info, core, "nvram: Register %d (0x%x) Not set - POSSIBLE BUG IN MACHINE INIT - CONTINUING\n", data->thereg, data->thereg);

    } 

    PrintDebug(core->vm_info, core, "nvram: nvram_read_data_port(0x%x)  =  0x%x\n", data->thereg, *(uint8_t *)dst);

    // hack
    if (data->thereg == NVRAM_REG_STAT_A) { 
	data->mem_state[data->thereg] ^= 0x80;  // toggle Update in progess
    }

    v3_unlock_irqrestore(data->nvram_lock, irq_state);

    return 1;
}


static int nvram_write_data_port(struct guest_info * core, uint16_t port,
				 void * src, uint_t length, void * priv_data) {

    struct nvram_internal * data = priv_data;

    addr_t irq_state = v3_lock_irqsave(data->nvram_lock);

    set_memory(data, data->thereg, *(uint8_t *)src);

    v3_unlock_irqrestore(data->nvram_lock, irq_state);

    PrintDebug(core->vm_info, core, "nvram: nvram_write_data_port(0x%x) = 0x%x\n", 
	       data->thereg, data->mem_state[data->thereg]);

    return 1;
}




static int nvram_free(struct nvram_internal * nvram_state) {
    
    // unregister host events
    struct guest_info *info = &(nvram_state->vm->cores[0]);

    if (nvram_state->timer) { 
	v3_remove_timer(info,nvram_state->timer);
    }

    v3_lock_deinit(&(nvram_state->nvram_lock));

    V3_Free(nvram_state);
    return 0;
}



static struct v3_timer_ops timer_ops = {
    .update_timer = nvram_update_timer,
};


static struct v3_device_ops dev_ops = {  
    .free = (int (*)(void *))nvram_free,
};





static int nvram_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct nvram_internal * nvram_state = NULL;
    struct vm_device * ide = v3_find_dev(vm, v3_cfg_val(cfg, "storage"));
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    if (!ide) {
	PrintError(vm, VCORE_NONE, "nvram: Could not find IDE device\n");
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "nvram: init_device\n");
    nvram_state = (struct nvram_internal *)V3_Malloc(sizeof(struct nvram_internal) + 1000);

    if (!nvram_state) {
	PrintError(vm, VCORE_NONE, "Cannot allocate in init\n");
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "nvram: internal at %p\n", (void *)nvram_state);

    nvram_state->ide = ide;
    nvram_state->vm = vm;

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, nvram_state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "nvram: Could not attach device %s\n", dev_id);
	V3_Free(nvram_state);
	return -1;
    }

    init_nvram_state(vm, nvram_state);

    // hook ports
    ret |= v3_dev_hook_io(dev, NVRAM_REG_PORT, NULL, &nvram_write_reg_port);
    ret |= v3_dev_hook_io(dev, NVRAM_DATA_PORT, &nvram_read_data_port, &nvram_write_data_port);
  
    if (ret != 0) {
	PrintError(vm, VCORE_NONE, "nvram: Error hooking NVRAM IO ports\n");
	v3_remove_device(dev);
	return -1;
    }

    nvram_state->timer = v3_add_timer(&(vm->cores[0]),&timer_ops,nvram_state);

    if (nvram_state->timer == NULL ) { 
	v3_remove_device(dev);
	return -1;
    }

    return 0;
}

device_register("NVRAM", nvram_init)
