/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2014, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *         
 * Emulated HPET device
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_queue.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_debug.h>


#ifndef V3_CONFIG_DEBUG_HPET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define HPET_REGION_SIZE       1024
#define HPET_DEFAULT_BASE_ADDR 0xFED00000ULL
#define HPET_NUM_TIMERS        3
#define V3_VENDOR              0xA333
#define NANOSECS               1000000000ULL          
#define FEMTOSECS              1000000000000000ULL     

#define HPET_TIMER_INT_ROUTE_CAP_SHIFT 32
/* HPET irqs can be routed to IOAPIC [23..20] */
#define HPET_TIMER_INT_ROUTE_CAP      (0x00f00000ULL \ << HPET_TIMER_INT_ROUTE_CAP_SHIFT)

/* Memory-Mapped Register Offsets */
#define GEN_CAP_REG_OFFSET       0x000 // r
#define RSVD_OFFSET              0x008
#define GEN_CFG_REG_OFFSET       0x010 // rw
#define RSVD_OFFSET1             0x018
#define GEN_ISR_OFFSET           0x020 // rw clear
#define RSVD_OFFSET2             0x028
#define MAIN_CNTR_VAL_REG_OFFSET 0x0F0 // rw
#define RSVD_OFFSET3             0x0F8
#define RSVD_OFFSET4             0x118
#define RSVD_OFFSET5             0x138
#define RSVD_OFFSET6             0x158
#define RSVD_3_31_OFFSET         0x160
#define TIMER_N_OFFSET_CFG(n)     (0x100 + (n) * 0x20) // rw
#define TIMER_N_OFFSET_CMP(n)     (0x108 + (n) * 0x20) // rw
#define TIMER_N_OFFSET_FSB_IRR(n) (0x110 + (n) * 0x20) // rw

/* extract an architectural field for timer N */
#define TIMER_N(f, addr) (((addr) - TIMER_N_OFFSET_##f(0)) / (TIMER_N_OFFSET_##f(1) - TIMER_N_OFFSET_##f(0)))

/* utility macros */
#define is_hpet_enabled(hpet)         (hpet->regs.cfg.enable_cnf)

/* timer-specific utility macros */
#define in_periodic_mode(hpet, n)     (hpet->regs.timers[n].caps.tn_type_cnf)
#define timer_32bit(hpet, n)          (hpet->regs.timers[n].caps.tn_32mode_cnf)
#define is_timer_enabled(hpet, n)     (hpet->regs.timers[n].caps.tn_int_enb_cnf)
#define is_timer_ltrig(hpet, n)       (hpet->regs.timers[n].caps.tn_int_type_cnf)
#define set_timer_bit(x, n)           ((x) |= (1UL << (n)))
#define unset_timer_bit(x, n)         ((x) &= (~(1UL << (n))))

/* flag macros */
#define HPET_CFG_ENABLE 0x001
#define HPET_CFG_LEGACY 0x002
#define HPET_LEGACY_8254    2
#define HPET_LEGACY_RTC     8

#define HPET_TIMER_LEVEL       0x002
#define HPET_TIMER_ENABLE      0x004
#define HPET_TIMER_PERIODIC    0x008
#define HPET_TIMER_PERIODIC_CAP    0x010
#define HPET_TIMER_64BIT_CAP   0x020
#define HPET_TIMER_SETVAL      0x040
#define HPET_TIMER_32BIT       0x100
#define HPET_TIMER_ROUTE       0x3e00
#define HPET_TIMER_FSB     0x4000
#define HPET_TIMER_FSB_CAP     0x8000
#define HPET_TIMER_RESERVED    0xffff0081
#define HPET_TIMER_ROUTE_SHIFT 9

/* we're going to set the HPET timer freq. to be 1/16th rate of Palacios system time */
#define HPET_PERIOD 16   

#define SYS_TICKS_PER_NS(hpet)       ((hpet)->system_freq/NANOSECS)
#define hpet_guest_time(hpet)        (v3_get_guest_time(&(hpet)->core->time_state) / (HPET_PERIOD*SYS_TICKS_PER_NS(hpet)))
#define HPET_SMALL_WINDOW(hpet)      (((hpet)->system_freq >> 10) / HPET_PERIOD)

#define tick_to_ns(hpet, tick)                  \
         (((((tick) > (hpet)->max_ns_res) ?     \
         ~0ULL : (tick) * (hpet)->ns_per_tick) >> 10))


/* General Capabilities and ID Register */
struct hpet_cap_reg {
    union {
        uint64_t value;
        struct {
            uint8_t rev_id;
            uint8_t num_tim_cap    : 5;
            uint8_t count_size_cap : 1;
            uint8_t rsvd           : 1;
            uint8_t leg_route_cap  : 1;
            uint16_t vendor_id;
            uint32_t counter_clk_period; 
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* General Configuration Register */
struct hpet_gen_cfg_reg {
    union {
        uint64_t value;
        struct {
            uint8_t enable_cnf : 1;
            uint8_t leg_rt_cnf : 1;
            uint8_t rsvd       : 6;
            uint8_t rsvd2;
            uint64_t rsvd3     : 48;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* General Interrupt Status Register */
struct hpet_gen_irq_status_reg {
    union {
        uint64_t value;
        struct {
            uint8_t t0_int_sts : 1;
            uint8_t t1_int_sts : 1;
            uint8_t t2_int_sts : 1;
            uint32_t rsvd      : 29;
            uint32_t rsvd2;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Timer N Configuration and Capabilities Register */
struct timer_cfg_cap_reg {
    union {
        uint64_t value;
        struct {
            uint8_t rsvd : 1;
            uint8_t tn_int_type_cnf    : 1;
            uint8_t tn_int_enb_cnf     : 1;
            uint8_t tn_type_cnf        : 1;
            uint8_t tn_per_int_cap     : 1;
            uint8_t tn_size_cap        : 1;
            uint8_t tn_val_set_cnf     : 1;
            uint8_t rsvd2              : 1;
            uint8_t tn_32mode_cnf      : 1;
            uint8_t tn_int_route_cnf   : 5;
            uint8_t tn_fsb_en_cnf      : 1;
            uint8_t tn_fsb_int_del_cap : 1;
            uint16_t rsvd3;
            uint32_t tn_int_route_cap;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* Timer N Comparator Register */
struct timer_comp_reg {
    union {
        uint64_t value;
        struct {
            uint32_t lo;
            uint32_t hi;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* 
 * Timer N FSB Interrupt Route Reigster
 *
 * not supported 
 */
struct timer_fsb_int_route_reg {
    union {
        uint64_t value;
        struct {
            uint32_t tn_fsb_int_val;
            uint32_t tn_fsb_int_addr;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* architectural state for timer N */
struct timer_regs {
    struct timer_cfg_cap_reg       caps;
    struct timer_comp_reg          comp;
    struct timer_fsb_int_route_reg fsb_int_route; // not supported
    uint64_t                       rsvd;
} __attribute__((packed));


/* hidden state for timer N */
struct hpet_timer_state {
    uint_t              timer_num; 
    uint_t              oneshot;
    uint_t              irq;
    struct hpet_state * hpet;
    struct v3_timer *   timer;
};


struct hpet_state {

    /* archietected registers */
    union {
       uint32_t raw_regs[HPET_REGION_SIZE/4]; // 1K total is mapped in
       struct {

          /* memory mapped registers */
          struct hpet_cap_reg            caps;                    // 0x000
          uint64_t                       rsvd1;                   // 0x008
          struct hpet_gen_cfg_reg        cfg;                     // 0x010
          uint64_t                       rsvd2;                   // 0x018
          struct hpet_gen_irq_status_reg irq_status;              // 0x020
          uint64_t                       rsvd3[50];               // 0x028
          uint64_t                       main_counter;            // 0x0f0
          uint64_t                       rsvd4;                   // 0x0f8
          struct timer_regs              timers[HPET_NUM_TIMERS]; // 0x100, 0x120, etc.

       } __attribute__((packed)) regs;
    } __attribute__((packed));

    /* hidden state */
    addr_t                  base_addr;
    uint64_t                counter_offset;
    struct guest_info *     core;
    struct hpet_timer_state timer_states[HPET_NUM_TIMERS];
    v3_lock_t               lock;

    /* time-keeping */
    uint64_t                system_freq; 
    uint64_t                ns_per_tick; // how many nanosecs per HPET tick
    uint64_t                max_ns_res;  // max number of ticks we can rep in nanosec */

    /* this is the initial value written to the comparator 
     * for periodic timers, we increment the comparator by this 
     * many ticks after raising each irq 
     */
    uint64_t period[HPET_NUM_TIMERS]; 

    /*
     * shadow comparator
     * for each timer, we compare the main counter
     * against this. when it matches, raise irq 
     */
    uint64_t comparator[HPET_NUM_TIMERS];

};


static int hpet_read (struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data);
static int hpet_write (struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data);


/* return first set bit */
static inline unsigned int 
first_bit (unsigned long x)
{
    __asm__ __volatile__ ("bsf %1,%0" : "=r" (x) : "r" (x));
    return (unsigned int)x;
}


static void
remove_hpet_timers (struct hpet_state * hpet) 
{
    int i;

    for (i = 0; i < HPET_NUM_TIMERS; i++) {
        if (hpet->timer_states[i].timer) {
            v3_remove_timer(hpet->core, hpet->timer_states[i].timer);
            hpet->timer_states[i].timer = NULL;
        }
    }
}


/* make sure accesses don't go across register boundaries */
static inline int 
check_hpet_access (struct guest_info * core, addr_t guest_addr, uint_t length) 
{
    /* is access aligned to the access length? */
    if (guest_addr & (--length) || length > 8) {
        PrintError(core->vm_info, core, "HPET: access across register boundary\n");
        return -1;
    }
    return 0;
}


/* get the value that we should return when main counter is read */
static inline uint64_t 
read_hpet_counter (struct hpet_state * hpet) 
{ 
    if (is_hpet_enabled(hpet)) {
        return hpet_guest_time(hpet) + hpet->counter_offset;
    } else {
        return hpet->regs.main_counter;
    }

}


/* return the *most recent* comparator value 
 * as a side-effect, the comparator for timer N is
 * updated to reflect any missed timer interrupts */
static uint64_t 
read_hpet_comparator (struct hpet_state * hpet, unsigned int timer_n) 
{
    uint64_t comp;
    uint64_t elapsed;

    comp = hpet->comparator[timer_n];
    
    if (in_periodic_mode(hpet, timer_n)) {
        // advance comp by # of periods since last update
        uint64_t period = hpet->period[timer_n];
        if (period) {
            elapsed = read_hpet_counter(hpet) + period - 1 - comp;
            comp += (elapsed / period) * period;
            hpet->comparator[timer_n] = comp;
        }
    }

    // if we're in 32-bit mode, truncate
    if (timer_32bit(hpet, timer_n)) {
        comp = (uint32_t)comp;
    }

    hpet->regs.timers[timer_n].comp.value = comp;
    return comp;
}


static void 
init_hpet_state (struct hpet_state * hpet) 
{
    int i;

    hpet->base_addr = HPET_DEFAULT_BASE_ADDR;

    memset(hpet->raw_regs, 0, HPET_REGION_SIZE);

    hpet->regs.caps.rev_id             = 1; // must be non-zero
    hpet->regs.caps.num_tim_cap        = HPET_NUM_TIMERS - 1;
    hpet->regs.caps.count_size_cap     = 1; // 64-bit mode
    hpet->regs.caps.leg_route_cap      = 1; // we support legacy interrupt routing
    hpet->regs.caps.vendor_id          = V3_VENDOR;

    // # of femtosecs per HPET tick, HPET frequency is 1/16th of palacios time
    hpet->regs.caps.counter_clk_period = FEMTOSECS*HPET_PERIOD/hpet->system_freq;

    // timer-specific archictectural state 
    for (i = 0; i < HPET_NUM_TIMERS; i++) {
            hpet->regs.timers[i].caps.tn_int_route_cap = HPET_TIMER_INT_ROUTE_CAP_SHIFT; // we support routing through the IOAPIC
            hpet->regs.timers[i].caps.tn_size_cap      = 1; // 64-bit
            hpet->regs.timers[i].caps.tn_per_int_cap   = 1; // this timer supports periodic mode
            hpet->regs.timers[i].comp.value            = ~0ULL; // initial value for comparator: default value should be all 1's
    }
}


static inline uint64_t
hpet_get_reg (struct hpet_state * hpet, addr_t guest_addr) 
{
    /* we don't care about the lower 3 bits at this point */
    guest_addr &= ~7;

    switch (guest_addr) {
        case GEN_CAP_REG_OFFSET: 
            return hpet->regs.caps.value;
        case GEN_CFG_REG_OFFSET:
            return hpet->regs.cfg.value;
        case GEN_ISR_OFFSET:
            return hpet->regs.irq_status.value;
        case MAIN_CNTR_VAL_REG_OFFSET:
            return read_hpet_counter(hpet);
        case TIMER_N_OFFSET_CFG(0): 
        case TIMER_N_OFFSET_CFG(1): 
        case TIMER_N_OFFSET_CFG(2): 
            return hpet->regs.timers[TIMER_N(CFG, guest_addr)].caps.value;
        case TIMER_N_OFFSET_CMP(0):
        case TIMER_N_OFFSET_CMP(1):
        case TIMER_N_OFFSET_CMP(2):
             return read_hpet_comparator(hpet, TIMER_N(CMP, guest_addr));
        case TIMER_N_OFFSET_FSB_IRR(0):
        case TIMER_N_OFFSET_FSB_IRR(1):
        case TIMER_N_OFFSET_FSB_IRR(2):
             return hpet->regs.timers[TIMER_N(FSB_IRR, guest_addr)].fsb_int_route.value;
    }

    return 0;
}


static int 
hpet_read (struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data) 
{
    struct hpet_state * hpet = (struct hpet_state *)priv_data;
    uint_t flags = 0;
    uint64_t reg;
    unsigned long res;

    /* we only care about the lower 9 bits */
    guest_addr &= HPET_REGION_SIZE - 1;

    if (check_hpet_access(core, guest_addr, length) != 0) {
        res = ~0UL;
        goto out;
    }

    flags = v3_lock_irqsave(hpet->lock);

    reg = hpet_get_reg(hpet, guest_addr);

    res = reg;

    /* shift and mask out high-order bits if this is smaller than a 64-bit read */
    if (length != 8) {
        res = (reg >> ((guest_addr & 7) * 8)) & ((1ULL << (length * 8)) - 1);
    }

    v3_unlock_irqrestore(hpet->lock, flags);
out:
    PrintDebug(core->vm_info, core, "HPET: core %u: at %p: Read HPET address space (%p), length=%u, val=0x%lx\n",
	       core->vcpu_id, hpet, (void *)guest_addr, length, res);
    *(unsigned long*)dst = res;
    return 0;
}


/* timer functions */

static void 
hpet_update_time (struct guest_info * core, 
			      uint64_t cpu_cycles, uint64_t cpu_freq, 
			      void * priv_data) 
{
    struct hpet_timer_state * htimer = (struct hpet_timer_state *)(priv_data);
    struct hpet_state * hpet = htimer->hpet;
    unsigned int nr = htimer->timer_num;
    uint64_t hpet_ticks = read_hpet_counter(hpet);

    /* KCH TODO: handle missed timer interrupts?? */
    if (hpet_ticks >= hpet->comparator[nr]) {
        v3_raise_irq(core->vm_info, htimer->timer_num);

        /* we do this to update the comparator value,
         * e.g. in case we missed an interrupt */
        read_hpet_comparator(hpet, nr);
    } 
}


static struct v3_timer_ops timer_ops = {
    .update_timer = hpet_update_time,
};


static void 
hpet_stop_timer (struct hpet_state * hpet, unsigned int n)
{

    V3_ASSERT(hpet->core->vm_info, hpet->core, n < HPET_NUM_TIMERS);
    if (hpet->timer_states[n].timer) {
        v3_remove_timer(hpet->core, hpet->timer_states[n].timer);
        hpet->timer_states[n].timer = NULL;
    }
    // synch the comparator for reads that may happen while we're stopped
    read_hpet_comparator(hpet, n);
}


static void 
hpet_start_timer (struct hpet_state * hpet, unsigned int n) 
{
    uint64_t timer_comp, tick;

    V3_ASSERT(hpet->core->vm_info, hpet->core, n < HPET_NUM_TIMERS);
    if (n == 0 && hpet->regs.cfg.leg_rt_cnf) {
        /* KCH TODO: the PIT shouldn't be generating irqs on chan 0 if this bit is set 
         * AFAIK we don't have an interface to do this just yet
         */
    }

    if (!is_timer_enabled(hpet, n)) {
        return;
    }

    timer_comp = read_hpet_comparator(hpet, n);
    tick = read_hpet_counter(hpet);

    if (timer_32bit(hpet, n)) {
        timer_comp = (uint32_t)read_hpet_comparator(hpet, n);
        tick = (uint32_t)read_hpet_counter(hpet);
    }
        
    /* if LegacyReplacementRoute is set, 
     * "Timer 0 will be routed to IRQ0 in Non-APIC or IRQ2 in the I/O APIC
     *  Timer 1 will be routed to IRQ8 in Non-APIC or IRQ8 in the I/O APIC
     *  Timer 2-n will be routed as per the routing in the timer n config registers"
     * see IA-PC HPET Spec pp.12-13
     */
    if ((n <= 1) && (hpet->regs.cfg.leg_rt_cnf)) {
        hpet->timer_states[n].irq = (n == 0) ? 0 : 8;
    } else {
        hpet->timer_states[n].irq = hpet->regs.timers[n].caps.tn_int_route_cnf;
    }

    hpet->timer_states[n].oneshot = !in_periodic_mode(hpet, n);
    hpet->timer_states[n].timer   = v3_add_timer(hpet->core, &timer_ops, &hpet->timer_states[n]);

    if (hpet->timer_states[n].timer == NULL) {
        PrintError(hpet->core->vm_info, hpet->core, "HPET: Failed to attach HPET timer %d to core %d\n", n, hpet->core->vcpu_id);
        return;
    }
}


/* this ensures that only allowed bits in a given register are 
 * changed */
static inline uint64_t 
hpet_mask_write (uint64_t new, uint64_t old, uint64_t mask) 
{
    new &= mask;
    new |= old & ~mask;
    return new;
}


static int 
hpet_write (struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data) 
{
    struct hpet_state * hpet = (struct hpet_state *)(priv_data);
    uint_t flags = 0;
    uint64_t old, new;
    unsigned long to_start = 0;
    unsigned long to_stop  = 0;
    int timer_n, i;

#define mark_timer_to_start(n)     (set_timer_bit(to_start, n))
#define mark_timer_to_stop(n)      (set_timer_bit(to_stop, n))
#define mark_timer_to_reset(n)     (mark_timer_to_stop(n), mark_timer_to_start(n))

    guest_addr &= HPET_REGION_SIZE - 1;

    PrintDebug(core->vm_info, core, "HPET: core %u: write to address space (%p) (val=%x)\n", 
	       core->vcpu_id, (void *)guest_addr, *(uint32_t *)src);

    if (check_hpet_access(core, guest_addr, length) != 0) {
        goto out_prelock;
    }

    flags = v3_lock_irqsave(hpet->lock);

    old = hpet_get_reg(hpet, guest_addr);
    new = *(unsigned long *)src;

    /* this is a trick to convert a non-8byte write into one,
     * making sure that we only change the bits of the word
     * within the length of the write */
    if (length != 8) {
        // say it's a 2-byte write to offset 6
        new = hpet_mask_write(new << (guest_addr & 7) * 8,  // left shift the value by 48
                              old,
                             ((1ULL << (length*8)) - 1) << ((guest_addr & 7) * 8)); // then mask out all but the last 2 bytes
    }

    switch (guest_addr & ~7) {

        case GEN_CFG_REG_OFFSET:
            hpet->regs.cfg.value = hpet_mask_write(new, old, 0x3);
 
            /* we're starting the timer */
            if ( !(old & HPET_CFG_ENABLE) && (new & HPET_CFG_ENABLE) ) {

                hpet->counter_offset = hpet->regs.main_counter - hpet_guest_time(hpet);
                //hpet->counter_offset = hpet_guest_time(hpet) - hpet->regs.main_counter;
                PrintDebug(core->vm_info, core, "HPET: starting the hpet, setting offset to 0x%llx\n", hpet->counter_offset);

                for (i = 0; i < HPET_NUM_TIMERS; i++) {
                    hpet->comparator[i] = timer_32bit(hpet, i) ?
                                            (uint32_t)hpet->regs.timers[i].comp.value :
                                                      hpet->regs.timers[i].comp.value;
                    if (is_timer_enabled(hpet, i)) {
                        mark_timer_to_start(i);
                    }
                }

            /* we're stopping the timer */
            } else if ( (old & HPET_CFG_ENABLE) && !(new & HPET_CFG_ENABLE) ) {
                PrintDebug(core->vm_info, core, "HPET: stopping the hpet\n");

                hpet->regs.main_counter = hpet->counter_offset + hpet_guest_time(hpet);

                for (i = 0; i < HPET_NUM_TIMERS; i++) {
                    if (is_timer_enabled(hpet, i)) {
                        mark_timer_to_stop(i);
                    }
                }

            }
            break;

        case MAIN_CNTR_VAL_REG_OFFSET:
            hpet->regs.main_counter = new;
            PrintDebug(core->vm_info, core, "HPET: writing the main counter (0x%llx)\n", new);
            if (is_hpet_enabled(hpet)) {
                PrintError(core->vm_info, core, "HPET: writing main counter in unhalted state\n");
                for (i = 0; i < HPET_NUM_TIMERS; i++) {
                    if (is_timer_enabled(hpet, i)) {
                        mark_timer_to_reset(i);
                    }
                }
            }

            break;
        case TIMER_N_OFFSET_CFG(0): 
        case TIMER_N_OFFSET_CFG(1): 
        case TIMER_N_OFFSET_CFG(2): 
            /* KCH: software should set the main counter to 0 before setting a comparator */
            timer_n = TIMER_N(CFG, guest_addr);
            hpet->regs.timers[timer_n].caps.value = hpet_mask_write(new, old, 0x3F4E);

            if (is_timer_ltrig(hpet, timer_n)) {
                PrintError(core->vm_info, core, "HPET: level-triggered interrupts not supported\n");
                goto out_err;
            }

            /* are we switching the timer to 32-bit mode? */
            if (new & HPET_TIMER_32BIT) {
                hpet->regs.timers[timer_n].comp.value = (uint32_t) hpet->regs.timers[timer_n].comp.value;
                hpet->period[timer_n] = (uint32_t)hpet->period[timer_n];
            }

            if (is_hpet_enabled(hpet)) {

                if (new & HPET_TIMER_ENABLE) {
                        /* we're switching to or from periodic mode, reset */
                        if ((new ^ old) & HPET_TIMER_PERIODIC) {
                            PrintDebug(core->vm_info, core, "HPET: changing periodic mode on timer %d\n", timer_n);
                            mark_timer_to_reset(timer_n);
                        /* we're switching from 64 to 32, reset */
                        } else if ((new & HPET_TIMER_32BIT) && !(old & HPET_TIMER_32BIT)) {
                            PrintDebug(core->vm_info, core, "HPET: changing timer %d from 32-bit to 64-bit mode\n", timer_n);
                            mark_timer_to_reset(timer_n);
                        } else if (!(old & HPET_TIMER_ENABLE)) {
                            PrintDebug(core->vm_info, core, "HPET: activating timer %d\n", timer_n);
                            mark_timer_to_start(timer_n);
                        }

                } else if (old & HPET_TIMER_ENABLE) {
                    PrintDebug(core->vm_info, core, "HPET: deactivating timer %d\n", timer_n);
                    mark_timer_to_stop(timer_n);
                }
            }

            break;
        case TIMER_N_OFFSET_CMP(0):
        case TIMER_N_OFFSET_CMP(1):
        case TIMER_N_OFFSET_CMP(2):
            timer_n = TIMER_N(CMP, guest_addr);

            if (timer_32bit(hpet, timer_n)) {
                new = (uint32_t)new;
            }
            
            PrintDebug(core->vm_info, core, "HPET: writing comparator reg on timer %d (val=0x%llx)\n", timer_n, new);

            hpet->regs.timers[timer_n].comp.value = new;
            
            /* this bit means sw can set the comp directly
             * The bit gets cleared on a write to the comp
             */
            if (hpet->regs.timers[timer_n].caps.tn_val_set_cnf) {
                hpet->regs.timers[timer_n].caps.tn_val_set_cnf = 0;
            } else if (in_periodic_mode(hpet, timer_n)) {
                    
                /* set bounds on the period */
                if (tick_to_ns(hpet, new) < 100000) {
                    new = (100000 << 10) / hpet->ns_per_tick;
                }

                new &= ((timer_32bit(hpet, timer_n)) ? ~0UL : ~0ULL) >> 1;
                hpet->period[timer_n] = new;
                PrintDebug(core->vm_info, core, "HPET: period for timer %d to (0x%llx)\n", timer_n, hpet->period[timer_n]);
            }

            hpet->comparator[timer_n] = new;

            if (is_hpet_enabled(hpet) && is_timer_enabled(hpet, timer_n)) {
                mark_timer_to_reset(timer_n);
            }
            
            break;
        case TIMER_N_OFFSET_FSB_IRR(0):
        case TIMER_N_OFFSET_FSB_IRR(1):
        case TIMER_N_OFFSET_FSB_IRR(2):
            timer_n = TIMER_N(FSB_IRR, guest_addr);
            PrintDebug(core->vm_info, core, "HPET: writing to fsb_irr reg for timer %d (val=0x%llx\n", timer_n, new);
            hpet->regs.timers[timer_n].fsb_int_route.value = new;
            break;

        default:
            /* just ignore writes to unsupported regs */
           break;
    }

    /* now we update the timers that we marked */
    while (to_stop)
    {
        i = first_bit(to_stop);
        unset_timer_bit(to_stop, i);
        hpet_stop_timer(hpet, i);
    }

    while (to_start)
    {
        i = first_bit(to_start);
        unset_timer_bit(to_start, i);
        hpet_start_timer(hpet, i);
    }

#undef mark_timer_to_start
#undef mark_timer_to_stop
#undef mark_timer_to_reset

    v3_unlock_irqrestore(hpet->lock, flags);
out_prelock:
    return 0;
out_err:
    v3_unlock_irqrestore(hpet->lock, flags);
    return -1;
}


static int 
hpet_free (struct hpet_state * hpet) 
{
    struct v3_vm_info * vm   = NULL;
	struct guest_info * core = NULL;

    if (!hpet) {
        return -1;
    }

    core = hpet->core;
	vm   = core->vm_info;
    
    remove_hpet_timers(hpet);

    v3_lock_deinit(&(hpet->lock));

    if (v3_unhook_mem(vm, core->vcpu_id, hpet->base_addr)) {
        PrintError(vm, VCORE_NONE, "HPET: could not unhook memory region\n");
    }

    V3_Free(hpet);
    return 0;
}


#ifdef V3_CONFIG_CHECKPOINT

static int 
hpet_save (struct v3_chkpt_ctx * ctx, void * private_data) 
{
    PrintError(VM_NONE, VCORE_NONE, "Unimplemented\n");
    return -1;
}


static int 
hpet_load (struct v3_chkpt_ctx * ctx, void * private_data) 
{
    PrintError(VM_NONE, VCORE_NONE, "Unimplemented\n");
    return -1;
}

#endif

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))hpet_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = hpet_save,
    .load = hpet_load
#endif
};


static int 
hpet_init (struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
    char * dev_id = v3_cfg_val(cfg, "ID");
    struct hpet_state * hpet;
    struct guest_info * core = NULL;
    struct vm_device * dev = NULL;
    int i;

    core = &vm->cores[0];

    PrintDebug(vm, VCORE_NONE, "HPET: Creating HPET\n");

    hpet = (struct hpet_state *)V3_Malloc(sizeof(struct hpet_state));

    if (!hpet) {
        PrintError(vm, VCORE_NONE, "HPET: Failed to allocate space for HPET\n");
        return -1;
    }

    hpet->core = core;

    dev = v3_add_device(vm, dev_id, &dev_ops, hpet);

    if (dev == NULL) {
        PrintError(vm, VCORE_NONE, "HPET: Could not attach device %s\n", dev_id);
        goto out_err;
    }

    hpet->system_freq = core->time_state.host_cpu_freq * 1000; // convert from kHz
    hpet->ns_per_tick = ((NANOSECS * HPET_PERIOD) << 10) / hpet->system_freq;
    hpet->max_ns_res  = ~0ULL / hpet->ns_per_tick;

    PrintDebug(core->vm_info, core, "HPET: System frequency detected as %lluHz\n", hpet->system_freq);

    v3_lock_init(&(hpet->lock)); 

    PrintDebug(core->vm_info, core, "HPET: Initializing %d timers\n", HPET_NUM_TIMERS);

    for (i = 0; i < HPET_NUM_TIMERS; i++) {
        hpet->timer_states[i].timer_num = i;
        hpet->timer_states[i].hpet      = hpet;
        hpet->timer_states[i].timer     = NULL;
    }

    /* init architecturally visible state */
	init_hpet_state(hpet);

    PrintDebug(core->vm_info, core "HPET: Hooking HPET mem region at %p\n", (void*)hpet->base_addr);
	if (v3_hook_full_mem(vm, 
                         V3_MEM_CORE_ANY, 
                         hpet->base_addr, 
                         hpet->base_addr + HPET_REGION_SIZE, 
                         hpet_read, 
                         hpet_write, 
                         hpet) < 0) {
        PrintError(vm, VCORE_NONE, "HPET: Failed to map HPET memory region\n");
        goto out_err1;
    }

    PrintDebug(vm, VCORE_NONE, "HPET: Initialization complete\n");

    return 0;

out_err1: 
    v3_lock_deinit(&(hpet->lock));
    v3_remove_device(dev);
out_err:
    V3_Free(hpet);
    return -1;
}

device_register("HPET", hpet_init)
