#ifndef GEEKOS_NE2K_H
#define GEEKOS_NE2K_H

//#include <geekos/ktypes.h>
#include <geekos/malloc.h>

#define NE2K_PAGE0	0x00
#define NE2K_PAGE1	0x40
#define NE2K_PAGE2	0x80
#define NE2K_PAGE3	0xc0

#define NE2K_BASE_ADDR 	0xc100		/* Starting address of the card */
#define NE2K_CR		NE2K_BASE_ADDR	/* Command register */

/* Page 0 register offsets */
#define NE2K CLDA0	NE2K_CR + 0x01
#define NE2K_PSTART	NE2K_CR + 0x01
#define NE2K_CLDA1	NE2K_CR + 0x02
#define NE2K_PSTOP	NE2K_CR + 0x02
#define NE2K_BNRY	NE2K_CR + 0x03
#define NE2K_TSR	NE2K_CR + 0x04
#define NE2K_TPSR	NE2K_CR + 0x04
#define NE2K_NCR	NE2K_CR + 0x05
#define NE2K_TBCR0	NE2K_CR + 0x05
#define NE2K_FIFO	NE2K_CR + 0x06
#define NE2K_TBCR1	NE2K_CR + 0x06
#define NE2K_ISR	NE2K_CR + 0x07	/* Interrupt status register */
#define NE2K_CRDA0	NE2K_CR + 0x08
#define NE2K_RSAR0	NE2K_CR + 0x08	/* Remote start address registers */
#define NE2K_CRDA1	NE2K_CR + 0x09
#define NE2K_RSAR1	NE2K_CR + 0x09
#define NE2K_RBCR0	NE2K_CR + 0x0a	/* Remote byte count registers */
#define NE2K_RBCR1	NE2K_CR + 0x0b
#define NE2K_RSR	NE2K_CR + 0x0c
#define NE2K_RCR	NE2K_CR + 0x0c	/* Receive configuration register */
#define NE2K_CNTR0	NE2K_CR + 0x0d
#define NE2K_TCR	NE2K_CR + 0x0d	/* Transmit configuration register */
#define NE2K_CNTR1	NE2K_CR + 0x0e
#define NE2K_DCR	NE2K_CR + 0x0e	/* Data configuration register */
#define NE2K_CNTR2	NE2K_CR + 0x0f
#define NE2K_IMR	NE2K_CR + 0x0f	/* Interrupt mask register */

/* Page 1 register offsets */
#define NE2K_PAR0	NE2K_CR + 0x01
#define NE2K_PAR1	NE2K_CR + 0x02
#define NE2K_PAR2	NE2K_CR + 0x03
#define NE2K_PAR3	NE2K_CR + 0x04
#define NE2K_PAR4	NE2K_CR + 0x05
#define NE2K_PAR5	NE2K_CR + 0x06
#define NE2K_CURR	NE2K_CR + 0x07
#define NE2K_MAR0	NE2K_CR + 0x08
#define NE2K_MAR1	NE2K_CR + 0x09
#define NE2K_MAR2	NE2K_CR + 0x0a
#define NE2K_MAR3	NE2K_CR + 0x0b
#define NE2K_MAR4	NE2K_CR + 0x0c
#define NE2K_MAR5	NE2K_CR + 0x0d
#define NE2K_MAR6	NE2K_CR + 0x0e
#define NE2K_MAR7	NE2K_CR + 0x0f

#define NE2K_IRQ	11		/* Interrupt channel */

struct NE2K_REGS {
	struct _CR * cr;
	struct _ISR * isr;
	struct _IMR * imr;
	struct _DCR * dcr;
	struct _TCR *tcr;
	struct _TSR *tsr;
	struct _RCR *rcr;
	struct _RSR *rsr;
};

int Init_Ne2k();
int NE2K_Transmit(struct NE2K_REGS *);
int NE2K_Receive();

#endif  /* GEEKOS_NE2K_H */
