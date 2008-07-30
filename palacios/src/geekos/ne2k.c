#include <geekos/ne2k.h>
#include <geekos/debug.h>
#include <geekos/io.h>
#include <geekos/irq.h>
#include <geekos/malloc.h>

#define DEBUG 0
#define RX_START_BUFF 0x4c

static uint_t received = 0;
static uint_t send_done = 1;
uint_t next = (RX_START_BUFF<<8);

static void Dump_Registers()
{
  uint_t data;
  PrintBoth("Dumping NIC registers for page %x...\n", (In_Byte(NE2K_CR) & 0xc0) >> 6);
  uint_t i = 0;
  for(i = 0; i <= 0x0f; i += 0x01) {
    data = In_Byte(NE2K_BASE_ADDR+i);
    PrintBoth("\t%x: %x\n", NE2K_BASE_ADDR + i, data);
  }
}

static void NE2K_Interrupt_Handler(struct Interrupt_State * state)
{
  Begin_IRQ(state);
  PrintBoth("NIC Interrupt Occured!\n");
  uchar_t isr_content = In_Byte(NE2K_ISR);
  Out_Byte(NE2K_ISR, 0xff); /* Clear all interrupts */
/*   if( !isr_content )
	return;
*/  

  PrintBoth("Contents of ISR: %x\n", isr_content);
  if(isr_content & 0x01)
    NE2K_Receive();

  End_IRQ(state);
  if(isr_content & 0x02)
    send_done = 1;
}

int Init_Ne2k()
{
  PrintBoth("Initializing network card...\n");
  Out_Byte(NE2K_CR+0x1f, In_Byte(NE2K_CR+0x1f));  /* Reset? */

  struct NE2K_REGS* regs = Malloc(sizeof(struct NE2K_REGS));
  struct _CR * cr = (struct _CR *)&(regs->cr);
  struct _RCR * rcr = (struct _RCR*)&(regs->rcr);
  struct _IMR * imr = (struct _IMR *)&(regs->imr);

  regs->cr = 0x21;
  regs->dcr = 0x49;
  regs->isr = 0xff;
  regs->rcr = 0x20;
  regs->tcr = 0x02;

  Out_Byte(NE2K_CR, regs->cr);
  Out_Byte(NE2K_DCR, regs->dcr);
  Out_Byte(NE2K_ISR, regs->isr);
  Out_Byte(NE2K_RCR, regs->rcr);
  Out_Byte(NE2K_TCR, regs->tcr);
  Out_Byte(NE2K_IMR, regs->imr);

  Out_Byte(NE2K_RBCR0, 0x00);
  Out_Byte(NE2K_RBCR1, 0x00);
  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, 0x00);

  Out_Byte(NE2K_TPSR, 0x40);      // Set TPSR
  Out_Byte(NE2K_PSTART, RX_START_BUFF);    // Set PSTART
  Out_Byte(NE2K_BNRY, 0x59);      // Set BNRY
  Out_Byte(NE2K_PSTOP, 0x80);     // Set PSTOP

  cr->ps = 0x01;  //switch to reg page 1
  Out_Byte(NE2K_CR, regs->cr);
  Out_Byte(NE2K_CURR, RX_START_BUFF);
  cr->ps = 0x00;

  Out_Byte(NE2K_CR, regs->cr);
  Out_Byte(NE2K_BNRY, RX_START_BUFF);

  Out_Byte(NE2K_ISR, regs->isr);

  imr->prxe = 0x1;
  imr->ptxe = 0x1;
  imr->rxee = 0x1;
  imr->txee = 0x1;
  imr->ovwe = 0x1;
  imr->cnte = 0x1;
  Out_Byte(NE2K_IMR, regs->imr);

  cr->ps = 0x01;  //switch to reg page 1
  Out_Byte(NE2K_CR, regs->cr);

  /* Set the physical address of the card to 52:54:00:12:34:58 */
  Out_Byte(NE2K_CR+0x01, 0x52);
  Out_Byte(NE2K_CR+0x02, 0x54);
  Out_Byte(NE2K_CR+0x03, 0x00);
  Out_Byte(NE2K_CR+0x04, 0x12);
  Out_Byte(NE2K_CR+0x05, 0x34);
  Out_Byte(NE2K_CR+0x06, 0x58);

  /* Accept all multicast packets */
  uint_t i;
  for(i = 0x08; i <= 0x0f; i++) {
    Out_Byte(NE2K_CR+i, 0xff);
  }

  regs->cr = 0x21;  //set CR to start value
  Out_Byte(NE2K_CR, regs->cr);

  regs->tcr = 0x00;
  Out_Byte(NE2K_TCR, regs->tcr);

  rcr->sep = 0x1;
  rcr->ar = 0x1;
  rcr->ab = 0x1;
  rcr->am = 0x1;
  rcr->pro = 0x1; // promiscuous mode, accept all packets
  rcr->mon = 0x0;
  Out_Byte(NE2K_RCR, regs->rcr);

  cr->sta = 0x1;  // toggle start bit
  cr->stp = 0x0;
  Out_Byte(NE2K_CR, regs->cr);
  
  Dump_Registers();

  cr->ps = NE2K_PAGE1;
  Out_Byte(NE2K_CR, regs->cr);
  Dump_Registers();

  cr->ps = NE2K_PAGE2;  
  Out_Byte(NE2K_CR, regs->cr);
  Dump_Registers();

  cr->ps = NE2K_PAGE0;
  Out_Byte(NE2K_CR, regs->cr);

  // Reset?
//  Out_Byte(NE2K_CR+0x1f, In_Byte(NE2K_CR+0x1f));

  Install_IRQ(NE2K_IRQ, NE2K_Interrupt_Handler);
  Enable_IRQ(NE2K_IRQ);
/*
  for(i = 0; i < 1; i++)
  {
    NE2K_Transmit(regs);
    PrintBoth("Transmitting a packet\n");
  }
*/  
  return 0;
}

int NE2K_Transmit(struct NE2K_REGS *regs)
{
  while(!send_done);
  send_done = 0;

  struct _CR * cr = (struct _CR*)&(regs->cr);
  uint_t packet_size = 80;
  regs->cr = 0x21;
  cr->stp = 0x0;  //toggle start on
  cr->sta = 0x1;
  Out_Byte(NE2K_CR, regs->cr);
  
  // Read-before-write bug fix?
  Out_Byte(NE2K_RBCR0, 0x42);
  Out_Byte(NE2K_RBCR1, 0x00);
  Out_Byte(NE2K_RSAR0, 0x42);
  Out_Byte(NE2K_RSAR1, 0x00);

  cr->rd = 0x01;  // set remote DMA to 'remote read'
  Out_Byte(NE2K_CR, regs->cr);

  regs->isr = 0x40;  // clear and set Remote DMA high
  Out_Byte(NE2K_ISR, regs->isr);
  
  Out_Byte(NE2K_RBCR0, packet_size);
  Out_Byte(NE2K_RBCR1, 0x00);

  Out_Byte(NE2K_TBCR0, packet_size);
  Out_Byte(NE2K_TBCR1, 0x00);

  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, 0x40);

  regs->cr = 0x16;
  Out_Byte(NE2K_CR, regs->cr);

  /* Begin pushing the packet into the dataport (located at 0x10 from the base address) */
  /* Destination address = 52:54:00:12:34:56 */
  Out_Word(NE2K_CR+0x10, 0x5452);
  Out_Word(NE2K_CR+0x10, 0x1200);
  Out_Word(NE2K_CR+0x10, 0x5634);

  /* Source address = 52:54:00:12:34:58 */
  Out_Word(NE2K_CR+0x10, 0x5452);
  Out_Word(NE2K_CR+0x10, 0x1200);
  Out_Word(NE2K_CR+0x10, 0x5834);

  /* Type length and data; currently random data */
  uint_t i;
  uint_t n = 0;
  for(i = 1; i <= packet_size/2-12; i++, n+=2) {
    //Out_Word(NE2K_CR+0x10, 0x0f0b);
    Out_Word(NE2K_CR+0x10, (n<<8) | (n+1));
  }

  //regs->isr = 0x40;
  //Out_Byte(NE2K_ISR, regs->isr); /* Do we need this here? */

  return 0;
}

int NE2K_Receive()
{
  PrintBoth("Packet Received\n");

  uint_t packet_size = 80;

  Out_Byte(NE2K_CR, 0x22);
  Out_Byte(NE2K_RBCR0, packet_size);
  Out_Byte(NE2K_RBCR1, 0x00);
//  Out_Byte(NE2K_RSAR0, 0x42);
  Out_Byte(NE2K_RSAR1, next >> 8);
  Out_Byte(NE2K_RSAR0, next & 0xff);
  Out_Byte(NE2K_CR, 0x0a);

  uint_t i;
  uint_t data;
  PrintBoth("\nPacket data:\n\t");

  data = In_Word(NE2K_CR + 0x10);
  PrintBoth("%x ", data);

  /* The first byte is the page number where the next packet in the ring buffer is stored */
  next = data & 0xff00;

  for(i = 2; i < packet_size; i+=2) {
    data = In_Word(NE2K_CR + 0x10);
    PrintBoth("%x ", data);
    
#if DEBUG
    PrintBoth("BNRY = %x\n", In_Byte(NE2K_BNRY));
    Out_Byte(NE2K_CR, 0x4a);
    PrintBoth("CURR = %x\n", In_Byte(NE2K_CURR));  
    Out_Byte(NE2K_CR, 0x0a);
#endif

    if(!(i%10))
      PrintBoth("\n\t");
  }

//Out_Byte(NE2K_RBCR0, (In_Byte(NE2K_RBCR0))-2);
//Out_Byte(NE2K_RSAR0, (In_Byte(NE2K_RSAR0))+2);

  PrintBoth("\n%d packets have been received", ++received);
  PrintBoth("\n\n");

  Out_Byte(NE2K_ISR, 0x40);

  /* The BNRY register stores the location of the first packet that hasn't been read yet */
  Out_Byte(NE2K_BNRY, next >> 8);

  return 0;
}

