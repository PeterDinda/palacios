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

#ifndef CONFIG_DEBUG_NVRAM
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


struct nvram_internal {
    nvram_state_t dev_state;
    uchar_t       thereg;
    uchar_t       mem_state[NVRAM_REG_MAX];
    uchar_t       reg_map[NVRAM_REG_MAX / 8];

    struct vm_device * ide;

    v3_lock_t nvram_lock;

    uint_t        us;   //microseconds - for clock update - zeroed every second
    uint_t        pus;  //microseconds - for periodic interrupt - cleared every period
};


struct rtc_stata {
    uint_t        rate: 4;  // clock rate = 65536Hz / 2 rate (0110=1024 Hz)
    uint_t        basis: 3; // time base, 010 = 32,768 Hz
    uint_t        uip: 1;   // 1=update in progress
} __attribute__((__packed__)) __attribute__((__aligned__ (1)))  ;

struct rtc_statb {
    uint_t        sum: 1;  // 1=summer (daylight savings)
    uint_t        h24: 1;  // 1=24h clock
    uint_t        dm: 1;   // 1=date/time is in bcd, 0=binary
    uint_t        rec: 1;  // 1=rectangular signal
    uint_t        ui: 1;   // 1=update interrupt
    uint_t        ai: 1;   // 1=alarm interrupt
    uint_t        pi: 1;   // 1=periodic interrupt
    uint_t        set: 1;  // 1=blocked update
} __attribute__((__packed__))  __attribute__((__aligned__ (1))) ;

struct rtc_statc {
    uint_t        res: 4;   // reserved
    uint_t        uf: 1;    // 1=source of interrupt is update
    uint_t        af: 1;    // 1=source of interrupt is alarm interrupt
    uint_t        pf: 1;    // 1=source of interrupt is periodic interrupt
    uint_t        irq: 1;   // 1=interrupt requested
}  __attribute__((__packed__))  __attribute__((__aligned__ (1))) ;

struct rtc_statd {
    uint_t        res: 7;   // reserved
    uint_t        val: 1;   // 1=cmos ram data is OK
}  __attribute__((__packed__))  __attribute__((__aligned__ (1))) ;




struct bcd_num {
    uchar_t bot : 4;
    uchar_t top : 4;
};



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


static uchar_t add_to(uchar_t * left, uchar_t * right, uchar_t bcd) {
    uchar_t temp;

    if (bcd) { 
	struct bcd_num * bl = (struct bcd_num *)left;
	struct bcd_num * br = (struct bcd_num *)right;
	uchar_t carry = 0;

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


static uchar_t days_in_month(uchar_t month, uchar_t bcd) {
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


static void update_time( struct vm_device * dev, uint_t period_us) {
    struct nvram_internal * data = (struct nvram_internal *) (dev->private_data);
    struct rtc_stata * stata = (struct rtc_stata *) &((data->mem_state[NVRAM_REG_STAT_A]));
    struct rtc_statb * statb = (struct rtc_statb *) &((data->mem_state[NVRAM_REG_STAT_B]));
    struct rtc_statc * statc = (struct rtc_statc *) &((data->mem_state[NVRAM_REG_STAT_C]));
    //struct rtc_statd *statd = (struct rtc_statd *) &((data->mem_state[NVRAM_REG_STAT_D]));
    uchar_t * sec = (uchar_t *) &(data->mem_state[NVRAM_REG_SEC]);
    uchar_t * min = (uchar_t *) &(data->mem_state[NVRAM_REG_MIN]);
    uchar_t * hour = (uchar_t *) &(data->mem_state[NVRAM_REG_HOUR]);
    uchar_t * weekday = (uchar_t *) &(data->mem_state[NVRAM_REG_WEEK_DAY]);
    uchar_t * monthday = (uchar_t *) &(data->mem_state[NVRAM_REG_MONTH_DAY]);
    uchar_t * month = (uchar_t *) &(data->mem_state[NVRAM_REG_MONTH]);
    uchar_t * year = (uchar_t *) &(data->mem_state[NVRAM_REG_YEAR]);
    uchar_t * cent = (uchar_t *) &(data->mem_state[NVRAM_REG_IBM_CENTURY_BYTE]);
    uchar_t * seca = (uchar_t *) &(data->mem_state[NVRAM_REG_SEC_ALARM]);
    uchar_t * mina = (uchar_t *) &(data->mem_state[NVRAM_REG_MIN_ALARM]);
    uchar_t * houra = (uchar_t *) &(data->mem_state[NVRAM_REG_HOUR_ALARM]);
    uchar_t hour24;

    uchar_t bcd = (statb->dm == 1);
    uchar_t carry = 0;
    uchar_t nextday = 0;
    uint_t  periodic_period;

    //PrintDebug("nvram: sizeof(struct rtc_stata)=%d\n", sizeof(struct rtc_stata));


    //PrintDebug("nvram: update_time\n",statb->pi);
  
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
	    PrintDebug("nvram: somehow managed to get a carry in second update\n"); 
	}

	if ( (bcd && (*sec == 0x60)) || 
	     ((!bcd) && (*sec == 60))) { 
  
	    *sec = 0;

	    carry = 1;
	    carry = add_to(min, &carry, bcd);
	    if (carry) { 
		PrintDebug("nvram: somehow managed to get a carry in minute update\n"); 
	    }

	    if ( (bcd && (*min == 0x60)) || 
		 ((!bcd) && (*min == 60))) { 

		*min = 0;
		hour24 = *hour;

		if (!(statb->h24)) { 

		    if (hour24 & 0x80) { 
			hour24 &= 0x8f;
			uchar_t temp = ((bcd) ? 0x12 : 12);
			add_to(&hour24, &temp, bcd);
		    }
		}

		carry = 1;
		carry = add_to(&hour24, &carry, bcd);
		if (carry) { 
		    PrintDebug("nvram: somehow managed to get a carry in hour update\n"); 
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
		PrintDebug("nvram: interrupt on alarm\n");
	    }
	}
    }

    if (statb->pi) { 
	periodic_period = 1000000 / (65536 / (0x1 << stata->rate));
	if (data->pus >= periodic_period) { 
	    statc->pf = 1;
	    data->pus -= periodic_period;
	    PrintDebug("nvram: interrupt on periodic\n");
	}
    }

    if (statb->ui) { 
	statc->uf = 1;
	PrintDebug("nvram: interrupt on update\n");
    }

    statc->irq = (statc->pf || statc->af || statc->uf);
  
    //PrintDebug("nvram: time is now: YMDHMS: 0x%x:0x%x:0x%x:0x%x:0x%x,0x%x bcd=%d\n", *year, *month, *monthday, *hour, *min, *sec,bcd);
  
    // Interrupt associated VM, if needed
    if (statc->irq) { 
	PrintDebug("nvram: injecting interrupt\n");
	v3_raise_irq(dev->vm, NVRAM_RTC_IRQ);
    }
}


static int handle_timer_event(struct v3_vm_info * vm, 
			      struct v3_timer_event * evt, 
			      void * priv_data) {

    struct vm_device * dev = (struct vm_device *)priv_data;

    if (dev) {
	struct nvram_internal * data = (struct nvram_internal *) (dev->private_data);
	
	addr_t irq_state = v3_lock_irqsave(data->nvram_lock);
	update_time(dev, evt->period_us);
	v3_unlock_irqrestore(data->nvram_lock, irq_state);
    }
  
    return 0;
}



static void set_memory_size(struct nvram_internal * nvram, addr_t bytes) {
    // 1. Conventional Mem: 0-640k in K
    // 2. Extended Mem: 0-16MB in K
    // 3. Big Mem: 0-4G in 64K

    if (bytes > 640 * 1024) {
	set_memory(nvram, NVRAM_REG_BASE_MEMORY_HIGH, 0x02);
	set_memory(nvram, NVRAM_REG_BASE_MEMORY_LOW, 0x80);

	//	nvram->mem_state[NVRAM_REG_BASE_MEMORY_HIGH] = 0x02;
	//	nvram->mem_state[NVRAM_REG_BASE_MEMORY_LOW] = 0x80;
    } else {
	uint16_t memk = bytes * 1024;
	set_memory(nvram, NVRAM_REG_BASE_MEMORY_HIGH, (memk >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_BASE_MEMORY_LOW, memk & 0x00ff);

	return;
    }

    if (bytes > (16 * 1024 * 1024)) {
	// Set extended memory to 15 MB
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_HIGH, 0x3C);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_LOW, 0x00);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_2ND_HIGH, 0x3C);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_2ND_LOW, 0x00);
    } else {
	uint16_t memk = bytes * 1024;

	set_memory(nvram, NVRAM_REG_EXT_MEMORY_HIGH, (memk >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_LOW, memk & 0x00ff);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_2ND_HIGH, (memk >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_EXT_MEMORY_2ND_LOW, memk & 0x00ff);

	return;
    }

    {
	// Set the extended memory beyond 16 MB in 64k chunks
	uint16_t mem_chunks = (bytes - (1024 * 1024 * 16)) / (1024 * 64);

	set_memory(nvram, NVRAM_REG_AMI_BIG_MEMORY_HIGH, (mem_chunks >> 8) & 0x00ff);
	set_memory(nvram, NVRAM_REG_AMI_BIG_MEMORY_LOW, mem_chunks & 0x00ff);
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
	if (v3_ide_get_geometry(nvram->ide, 0, i, &cyls, &heads, &sects) == 0) {

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

	    if (v3_ide_get_geometry(nvram->ide, chan_num, drive_num, &tmp[0], &tmp[1], &tmp[2]) == 0) {
		trans |= TRANSLATE_LBA << (i * 2);
	    }
	}

	set_memory(nvram, NVRAM_IDE_TRANSLATION, trans);
    }
}

static int init_nvram_state(struct v3_vm_info * vm, struct vm_device * dev) {

    struct nvram_internal * nvram = (struct nvram_internal *)dev->private_data;
  
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
    // 00000100 = not setting, no interrupts, blocked rect signal, bcd mode, 24 hour, normal time
    set_memory(nvram, NVRAM_REG_STAT_B, 0x06); 


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

    set_memory(nvram, NVRAM_REG_DIAGNOSTIC_STATUS, 0x00);
    
    nvram->us = 0;
    nvram->pus = 0;

    set_memory_size(nvram, vm->mem_size);
    init_harddrives(nvram);
    
    nvram->dev_state = NVRAM_READY;
    nvram->thereg = 0;

    return 0;
}




static int nvram_reset_device(struct vm_device * dev) {

    return 0;
}





static int nvram_start_device(struct vm_device * dev) {
    PrintDebug("nvram: start device\n");
    return 0;
}


static int nvram_stop_device(struct vm_device * dev) {
    PrintDebug("nvram: stop device\n");
    return 0;
}




static int nvram_write_reg_port(struct guest_info * core, ushort_t port,
				void * src, uint_t length, struct vm_device * dev) {

    struct nvram_internal * data = (struct nvram_internal *)dev->private_data;
    
    memcpy(&(data->thereg), src, 1);
    PrintDebug("Writing To NVRAM reg: 0x%x\n", data->thereg);

    return 1;
}

static int nvram_read_data_port(struct guest_info * core, ushort_t port,
				void * dst, uint_t length, struct vm_device * dev) {

    struct nvram_internal * data = (struct nvram_internal *)dev->private_data;

    addr_t irq_state = v3_lock_irqsave(data->nvram_lock);

    if (get_memory(data, data->thereg, (uint8_t *)dst) == -1) {
	PrintError("Register %d (0x%x) Not set\n", data->thereg, data->thereg);

	v3_unlock_irqrestore(data->nvram_lock, irq_state);

	return -1;
    }

    PrintDebug("nvram_read_data_port(0x%x)  =  0x%x\n", data->thereg, *(uint8_t *)dst);

    // hack
    if (data->thereg == NVRAM_REG_STAT_A) { 
	data->mem_state[data->thereg] ^= 0x80;  // toggle Update in progess
    }

    v3_unlock_irqrestore(data->nvram_lock, irq_state);

    return 1;
}


static int nvram_write_data_port(struct guest_info * core, ushort_t port,
				 void * src, uint_t length, struct vm_device * dev) {

    struct nvram_internal * data = (struct nvram_internal *)dev->private_data;

    addr_t irq_state = v3_lock_irqsave(data->nvram_lock);

    set_memory(data, data->thereg, *(uint8_t *)src);

    v3_unlock_irqrestore(data->nvram_lock, irq_state);

    PrintDebug("nvram_write_data_port(0x%x) = 0x%x\n", 
	       data->thereg, data->mem_state[data->thereg]);

    return 1;
}




static int nvram_free(struct vm_device * dev) {
    v3_dev_unhook_io(dev, NVRAM_REG_PORT);
    v3_dev_unhook_io(dev, NVRAM_DATA_PORT);

    return 0;
}





static struct v3_device_ops dev_ops = {  
    .free = nvram_free,
    .reset = nvram_reset_device,
    .start = nvram_start_device,
    .stop = nvram_stop_device,
};





static int nvram_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct nvram_internal * nvram_state = NULL;
    struct vm_device * ide = v3_find_dev(vm, v3_cfg_val(cfg, "storage"));
    char * dev_id = v3_cfg_val(cfg, "ID");

    if (!ide) {
	PrintError("Could not find IDE device\n");
	return -1;
    }

    PrintDebug("nvram: init_device\n");
    nvram_state = (struct nvram_internal *)V3_Malloc(sizeof(struct nvram_internal) + 1000);

    PrintDebug("nvram: internal at %p\n", (void *)nvram_state);

    nvram_state->ide = ide;

    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, nvram_state);


    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	return -1;
    }

    init_nvram_state(vm, dev);

    // hook ports
    v3_dev_hook_io(dev, NVRAM_REG_PORT, NULL, &nvram_write_reg_port);
    v3_dev_hook_io(dev, NVRAM_DATA_PORT, &nvram_read_data_port, &nvram_write_data_port);
  
    v3_hook_host_event(vm, HOST_TIMER_EVT, V3_HOST_EVENT_HANDLER(handle_timer_event), dev);

    return 0;
}

device_register("NVRAM", nvram_init)
