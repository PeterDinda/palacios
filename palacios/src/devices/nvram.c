/* Northwestern University */
/* (c) 2008, Peter Dinda <pdinda@cs.northwestern.edu> */

#include <devices/nvram.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>


#ifndef DEBUG_NVRAM
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




struct vm_device * thedev = NULL;

static struct vm_device * demultiplex_timer_interrupt(uint_t period_us)
{
  // hack
  return thedev;
}

struct bcd_num {
  uchar_t bot : 4;
  uchar_t top : 4;
} ;

static uchar_t add_to(uchar_t * left, uchar_t * right, uchar_t bcd)
{
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


static uchar_t days_in_month(struct vm_device *dev, uchar_t month, uchar_t bcd)
{
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


static void update_time(struct vm_device *dev, uint_t period_us)
{
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

	  if ((*monthday) != days_in_month(dev, *month, bcd)) {
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
    dev->vm->vm_ops.raise_irq(dev->vm, NVRAM_RTC_IRQ);
  }
}




void deliver_timer_interrupt_to_vmm(uint_t period_us)
{
  struct vm_device * dev = demultiplex_timer_interrupt(period_us);

  if (dev) {
    update_time(dev, period_us);
  }
  
}


static int set_nvram_defaults(struct vm_device * dev)
{
  struct nvram_internal * nvram_state = (struct nvram_internal *)dev->private_data;


  //
  // 2 1.44 MB floppy drives
  //
#if 1
  nvram_state->mem_state[NVRAM_REG_FLOPPY_TYPE] = 0x44;
#else
  nvram_state->mem_state[NVRAM_REG_FLOPPY_TYPE] = 0x00;
#endif

  //
  // For old boot sequence style, do floppy first
  //
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_OLD] = 0x10;

#if 0
  // For new boot sequence style, do floppy, cd, then hd
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_FIRST] = 0x31;
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_SECOND] = 0x20;
#endif

  // For new boot sequence style, do cd, hd, floppy
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_FIRST] = 0x23;
  nvram_state->mem_state[NVRAM_REG_BOOTSEQ_NEW_SECOND] = 0x10;
 

  // Set equipment byte to note 2 floppies, vga display, keyboard,math,floppy
  nvram_state->mem_state[NVRAM_REG_EQUIPMENT_BYTE] = 0x4f;
  //nvram_state->mem_state[NVRAM_REG_EQUIPMENT_BYTE] = 0xf;

  // Set conventional memory to 640K
  nvram_state->mem_state[NVRAM_REG_BASE_MEMORY_HIGH] = 0x02;
  nvram_state->mem_state[NVRAM_REG_BASE_MEMORY_LOW] = 0x80;

  // Set extended memory to 15 MB
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_HIGH] = 0x3C;
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_LOW] = 0x00;
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_2ND_HIGH]= 0x3C;
  nvram_state->mem_state[NVRAM_REG_EXT_MEMORY_2ND_LOW]= 0x00;

  // Set the extended memory beyond 16 MB to 128-16 MB
  nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_HIGH] = 0x7;
  nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_LOW] = 0x00;

  //nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_HIGH]= 0x00;
  //nvram_state->mem_state[NVRAM_REG_AMI_BIG_MEMORY_LOW]= 0x00;

  
  // This is the harddisk type.... Set accordingly...
  nvram_state->mem_state[NVRAM_IBM_HD_DATA] = 0x20;

  // Set the shutdown status gently
  // soft reset
  nvram_state->mem_state[NVRAM_REG_SHUTDOWN_STATUS] = 0x0;


  // RTC status A
  // 00100110 = no update in progress, base=32768 Hz, rate = 1024 Hz
  nvram_state->mem_state[NVRAM_REG_STAT_A] = 0x26; 

  // RTC status B
  // 00000100 = not setting, no interrupts, blocked rect signal, bcd mode, 24 hour, normal time
  nvram_state->mem_state[NVRAM_REG_STAT_B] = 0x06; 


  // RTC status C
  // No IRQ requested, result not do to any source
  nvram_state->mem_state[NVRAM_REG_STAT_C] = 0x00;

  // RTC status D
  // Battery is OK
  nvram_state->mem_state[NVRAM_REG_STAT_D] = 0x80;


  // january 1, 2008, 00:00:00
  nvram_state->mem_state[NVRAM_REG_MONTH] = 0x1;
  nvram_state->mem_state[NVRAM_REG_MONTH_DAY] = 0x1;
  nvram_state->mem_state[NVRAM_REG_WEEK_DAY] = 0x1;
  nvram_state->mem_state[NVRAM_REG_YEAR] = 0x08;

  nvram_state->us = 0;
  nvram_state->pus = 0;

  return 0;

}


int nvram_reset_device(struct vm_device * dev)
{
  struct nvram_internal * data = (struct nvram_internal *) dev->private_data;
  
  PrintDebug("nvram: reset device\n");

 
  data->dev_state = NVRAM_READY;
  data->thereg = 0;
  
  return 0;
}





int nvram_start_device(struct vm_device *dev)
{
  PrintDebug("nvram: start device\n");
  return 0;
}


int nvram_stop_device(struct vm_device *dev)
{
  PrintDebug("nvram: stop device\n");
  return 0;
}




int nvram_write_reg_port(ushort_t port,
			 void * src, 
			 uint_t length,
			 struct vm_device * dev)
{
  struct nvram_internal * data = (struct nvram_internal *)dev->private_data;

  memcpy(&(data->thereg), src, 1);
  PrintDebug("Writing To NVRAM reg: 0x%x\n", data->thereg);


  return 1;
}

int nvram_read_data_port(ushort_t port,
			 void * dst, 
			 uint_t length,
			 struct vm_device * dev)
{
  struct nvram_internal * data = (struct nvram_internal *) dev->private_data;



  memcpy(dst, &(data->mem_state[data->thereg]), 1);

  PrintDebug("nvram_read_data_port(0x%x)=0x%x\n", data->thereg, data->mem_state[data->thereg]);

  // hack
  if (data->thereg == NVRAM_REG_STAT_A) { 
    data->mem_state[data->thereg] ^= 0x80;  // toggle Update in progess
  }


  return 1;
}

int nvram_write_data_port(ushort_t port,
			  void * src, 
			  uint_t length,
			  struct vm_device * dev)
{
  struct nvram_internal * data = (struct nvram_internal *)dev->private_data;

  memcpy(&(data->mem_state[data->thereg]), src, 1);

  PrintDebug("nvram_write_data_port(0x%x)=0x%x\n", data->thereg, data->mem_state[data->thereg]);

  return 1;
}



int nvram_init_device(struct vm_device * dev) {
 
  struct nvram_internal * data = (struct nvram_internal *) dev->private_data;

  PrintDebug("nvram: init_device\n");

  memset(data->mem_state, 0, NVRAM_REG_MAX);

  // Would read state here
  set_nvram_defaults(dev);

  nvram_reset_device(dev);

  // hook ports
  dev_hook_io(dev, NVRAM_REG_PORT, NULL, &nvram_write_reg_port);
  dev_hook_io(dev, NVRAM_DATA_PORT, &nvram_read_data_port, &nvram_write_data_port);
  
  return 0;
}

int nvram_deinit_device(struct vm_device *dev)
{


  dev_unhook_io(dev, NVRAM_REG_PORT);
  dev_unhook_io(dev, NVRAM_DATA_PORT);

  nvram_reset_device(dev);
  return 0;
}





static struct vm_device_ops dev_ops = { 
  .init = nvram_init_device, 
  .deinit = nvram_deinit_device,
  .reset = nvram_reset_device,
  .start = nvram_start_device,
  .stop = nvram_stop_device,
};




struct vm_device * create_nvram() {
  struct nvram_internal * nvram_state = (struct nvram_internal *)V3_Malloc(sizeof(struct nvram_internal) + 1000);

  PrintDebug("nvram: internal at %x\n",nvram_state);

  struct vm_device * device = create_device("NVRAM", &dev_ops, nvram_state);

  if (thedev != NULL) {
    PrintDebug("nvram: warning! overwriting thedev\n");
  }

  thedev = device;

  return device;
}
