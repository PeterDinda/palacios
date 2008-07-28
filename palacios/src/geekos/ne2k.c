#include <geekos/ne2k.h>
#include <geekos/debug.h>
#include <geekos/io.h>
#include <geekos/irq.h>
#include <geekos/malloc.h>

static uint_t received = 0;

struct _CR {  //COMMAND REG
        uint_t stp: 1;  //STOP- software reset
        uint_t sta: 1;  //START- activates NIC
        uint_t txp: 1;  //TRANSMIT- set to send
        uint_t rd:  3;  //REMOTE DMA
        uint_t ps:  2;  //PAGE SELECT
};

struct _ISR{  //INTERRUPT STATUS REG
        uint_t prx: 1;  //PACKET RECIEVED
        uint_t ptx: 1;  //PACKET TRANSMITTED
        uint_t rxe: 1;  //TRANSMIT ERROR
        uint_t txe: 1;  //RECEIVE ERROR
        uint_t ovw: 1;  //OVERWRITE WARNING
        uint_t cnt: 1;  //COUNTER OVERFLOW
        uint_t rdc: 1;  //REMOTE DMA COMPLETE
        uint_t rst: 1;  //RESET STATUS
};

struct _IMR {  //INTERRUPT MASK REG
        uint_t prxe: 1;  //PACKET RX INTRPT
        uint_t ptxe: 1;  //PACKET TX INTRPT
        uint_t rxee: 1;  //RX ERROR INTRPT
        uint_t txee: 1;  //TX ERROR INTRPt
        uint_t ovwe: 1;  //OVERWRITE WARNING INTRPT
        uint_t cnte: 1;  //COUNTER OVERFLOW INTRPT
        uint_t rdce: 1;  //DMA COMLETE INTRPT
	uint_t rsvd: 1;
};

struct _DCR {  //DATA CONFIG REGISTER
        uint_t wts: 1;  //WORD TRANSFER SELECT
        uint_t bos: 1;  //BYTE ORDER SELECT
        uint_t las: 1;  //LONG ADDR SELECT
        uint_t ls:  1;  //LOOPBACK SELECT
        uint_t arm: 1;  //AUTO-INITIALIZE REMOTE
        uint_t ft:  2;  //FIFO THRESH SELECT
};

struct _TCR {  //TX CONFIG REGISTER
        uint_t crc:  1;  //INHIBIT CRC
        uint_t lb:   2;  //ENCODED LOOPBACK
        uint_t atd:  1;  //AUTO TRANSMIT
        uint_t ofst: 1;  //COLLISION OFFSET ENABLE
	uint_t rsvd: 3;
};

struct _TSR {
        uint_t ptx:  1;  //PACKET TX
	uint_t rsvd: 1;
        uint_t col:  1;  //TX COLLIDED
        uint_t abt:  1;  //TX ABORTED
        uint_t crs:  1;  //CARRIER SENSE LOST
        uint_t fu:   1;  //FIFO UNDERRUN
        uint_t cdh:  1;  //CD HEARTBEAT
        uint_t owc:  1;  //OUT OF WINDOW COLLISION
};

struct _RCR {  //RECEIVE CONFIGURATION REGISTER
        uint_t sep:  1;  //SAVE ERRORED PACKETS
        uint_t ar:   1;  //ACCEPT RUNT PACKETS
        uint_t ab:   1;  //ACCEPT BROADCAST
        uint_t am:   1;  //ACCEPT MULTICAST
        uint_t pro:  1;  //PROMISCUOUS PHYSICAL
        uint_t mon:  1;  //MONITOR MODE
	uint_t rsvd: 2;
};  

struct _RSR {  //RECEIVE STATUS REG
        uint_t prx: 1;  //PACKET RX INTACT
        uint_t crc: 1;  //CRC ERROR
        uint_t fae: 1;  //FRAME ALIGNMENT ERROR
        uint_t fo:  1;  //FIFO OVERRUN
        uint_t mpa: 1;  //MISSED PACKET
        uint_t phy: 1;  //PHYSICAL/MULTICAST ADDR
        uint_t dis: 1;  //RX DISABLED
        uint_t dfr: 1;  //DEFERRING
};


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
  uint_t isr_content = In_Byte(NE2K_ISR);
  PrintBoth("Contents of ISR: %x\n", isr_content);
  if(In_Byte(NE2K_ISR) & 0x01)
    NE2K_Receive();

  Out_Byte(NE2K_ISR, 0xff);
  End_IRQ(state);
}


int Init_Ne2k()
{

uint_t CR_START_VAL = 0x21;
static const uint_t DCR_START_VAL = 0x49;
static const uint_t ISR_START_VAL = 0xff;
static const uint_t RCR_START_VAL = 0x20;
static const uint_t TCR_START_VAL = 0x02;
//static const uint_t IMR_START_VAL = 0x00;
PrintBoth("location Startval first = %x\n", &CR_START_VAL);

  PrintBoth("Initializing network card...\n");

  struct NE2K_REGS* regs = Malloc(sizeof(struct NE2K_REGS));

  regs->cr = (struct _CR*)&CR_START_VAL;
  regs->dcr = (struct _DCR*)&DCR_START_VAL;
  regs->isr = (struct _ISR*)&ISR_START_VAL;
  regs->rcr = (struct _RCR*)&RCR_START_VAL;
  regs->tcr = (struct _TCR*)&TCR_START_VAL;

  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);
  Out_Byte(NE2K_DCR, *(uint_t *)regs->dcr);
  Out_Byte(NE2K_ISR, *(uint_t *)regs->isr);
  Out_Byte(NE2K_RCR, *(uint_t *)regs->rcr);
  Out_Byte(NE2K_TCR, *(uint_t *)regs->tcr);
  Out_Byte(NE2K_IMR, *(uint_t *)regs->imr);
  Out_Byte(NE2K_RBCR0, 0x00);
  Out_Byte(NE2K_RBCR1, 0x00);
  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, 0x00);

  Out_Byte(NE2K_TPSR, 0x40);      // Set TPSR
  Out_Byte(NE2K_PSTART, 0x42);    // Set PSTART
  Out_Byte(NE2K_BNRY, 0x59);      // Set BNRY
  Out_Byte(NE2K_PSTOP, 0x60);     // Set PSTOP

  Out_Byte(NE2K_ISR, *(uint_t *)regs->isr);

  regs->imr->prxe = 0x1;
  regs->imr->ptxe = 0x1;
  regs->imr->rxee = 0x1;
  regs->imr->txee = 0x1;
  regs->imr->ovwe = 0x1;
  regs->imr->cnte = 0x1;
  Out_Byte(NE2K_IMR, *(uint_t *)regs->imr);

  regs->cr->ps = 0x01;  //switch to reg page 1
  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);

PrintBoth("Startval 2= %x\n", CR_START_VAL);

PrintBoth("regs locat = %x\n", (uint_t *)regs->cr);
PrintBoth("regs = %x\n", *(uint_t *)regs->cr);

  /* Set the physical address of the card to 52:54:00:12:34:58 */
  Out_Byte(NE2K_CR+0x01, 0x52);
  Out_Byte(NE2K_CR+0x02, 0x54);
  Out_Byte(NE2K_CR+0x03, 0x00);
  Out_Byte(NE2K_CR+0x04, 0x12);
  Out_Byte(NE2K_CR+0x05, 0x34);
  Out_Byte(NE2K_CR+0x06, 0x58);

  uint_t i;
  for(i = 0x08; i <= 0x0f; i++) {
    Out_Byte(NE2K_CR+i, 0xff);
  }

  *(uint_t *)regs->cr = 0x21;  //set CR to start value
  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);

PrintBoth("startval 3 = %x\n", CR_START_VAL); 
PrintBoth("location Startval 2 = %x\n", &CR_START_VAL);
PrintBoth("regs = %x\n", (uint_t *)regs->cr);

  regs->tcr = 0x00;
  Out_Byte(NE2K_TCR, *(uint_t *)regs->tcr);

  regs->rcr->sep = 0x1;
  regs->rcr->ar = 0x1;
  regs->rcr->ab = 0x1;
  regs->rcr->am = 0x1;
  regs->rcr->pro = 0x1; //promiscuous mode, accept all packets
  regs->rcr->mon = 0x0;
  Out_Byte(NE2K_RCR, *(uint_t *)regs->rcr);

  regs->cr->sta = 0x1;  //toggle start bit
  regs->cr->stp = 0x0;
  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);
  
  Dump_Registers();

  Out_Byte(NE2K_CR, (*(uint_t *)regs->cr & 0x3f) | NE2K_PAGE1);
  Dump_Registers();

  Out_Byte(NE2K_CR, (*(uint_t *)regs->cr & 0x3f) | NE2K_PAGE2);
  Dump_Registers();

  // Reset?
  Out_Byte(NE2K_CR+0x1f, In_Byte(NE2K_CR+0x1f));

  Install_IRQ(NE2K_IRQ, NE2K_Interrupt_Handler);
  Enable_IRQ(NE2K_IRQ);

  for(i = 0; i < 4; i++)
  {
    NE2K_Transmit(regs);
    PrintBoth("Transmitting a packet\n");
  }

  return 0;
}
int NE2K_Transmit(struct NE2K_REGS *regs)
{
  uint_t packet_size = 80;
  *(uint_t *)regs->cr = 0x21;
  regs->cr->stp = 0x0;  //toggle start on
  regs->cr->sta = 0x1;
  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);
  
  // Read-before-write bug fix?
  Out_Byte(NE2K_RBCR0, 0x42);
  Out_Byte(NE2K_RBCR1, 0x00);
  Out_Byte(NE2K_RSAR0, 0x42);
  Out_Byte(NE2K_RSAR1, 0x00);

  regs->cr->rd = 0x01;  //set remote DMA to 'remote read'
  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);

  *(uint_t*)regs->isr = 0x40;  //clear and set Remote DMA high
  Out_Byte(NE2K_ISR, *(uint_t *)regs->isr);
  
  Out_Byte(NE2K_RBCR0, packet_size);
  Out_Byte(NE2K_RBCR1, 0x00);

  Out_Byte(NE2K_TBCR0, packet_size);
  Out_Byte(NE2K_TBCR1, 0x00);

  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, 0x40);

  *(uint_t*)regs->cr = 0x16;
  Out_Byte(NE2K_CR, *(uint_t *)regs->cr);
  
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

  Out_Byte(NE2K_ISR, 0x40);

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
    Out_Byte(NE2K_RSAR0, 0x4c);
  Out_Byte(NE2K_RSAR1, 0x00);
  Out_Byte(NE2K_CR, 0x0a);

  uint_t i;
  uint_t data;
  PrintBoth("\nPacket data:\n\t");

  for(i = 0; i < packet_size; i+=2) {
    data = In_Word(NE2K_CR + 0x10);
    PrintBoth("%x ", data);
    if(!(i%10))
      PrintBoth("\n\t");
  }

  PrintBoth("\n%d packets have been received", ++received);
  PrintBoth("\n\n");

  Out_Byte(NE2K_ISR, 0x40);

  return 0;
}


//int NE2K_Ringbuff_Overflow(){
	

