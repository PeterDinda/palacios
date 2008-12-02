#include <geekos/serial.h>
#include <geekos/reboot.h>
#include <geekos/gdt.h>
#include <geekos/idt.h>




unsigned short serial_io_addr = 0;


static void Serial_Interrupt_Handler(struct Interrupt_State * state) {
  char rcv_byte;
  char irq_id;

  Begin_IRQ(state);

  irq_id = In_Byte(serial_io_addr + 2);


  if ((irq_id & 0x04) != 0) {
    rcv_byte = In_Byte(serial_io_addr + 0);

    if (rcv_byte == 'k') {
      SerialPrint("Restarting Machine\r\n");
      machine_real_restart();
    } else if (rcv_byte=='d') { 
      SerialPrint("Dumping Machine State\n");
      Dump_Interrupt_State(state);
      DumpIDT();
      DumpGDT();
    }
      
#if 0
      SerialPrint("Unreserved serial byte: %d (%c)\r\n", rcv_byte, rcv_byte);
#endif
  }
  End_IRQ(state);
}

void InitSerial() {
  Print("Initialzing Serial\n");
  Install_IRQ(COM1_IRQ, Serial_Interrupt_Handler);
  Enable_IRQ(COM1_IRQ);
  InitSerialAddr(DEFAULT_SERIAL_ADDR);
}

void InitSerialAddr(unsigned short io_addr) {
  serial_io_addr = io_addr;

  Print("Initializing Polled Serial Output on COM1 - 115200 N81 noflow\n");
  //  io_adr = 0x3F8;	/* 3F8=COM1, 2F8=COM2, 3E8=COM3, 2E8=COM4 */
  Out_Byte(io_addr + 3, 0x80);
  // 115200 /* 115200 / 12 = 9600 baud */
  Out_Byte(io_addr + 0, 1);
  Out_Byte(io_addr + 1, 0);
  /* 8N1 */
  Out_Byte(io_addr + 3, 0x03);
  /* all interrupts disabled */
  //  Out_Byte(io_addr + 1, 0);
  Out_Byte(io_addr + 1, 0x01);
  /* turn off FIFO, if any */
  Out_Byte(io_addr + 2, 0);
  /* loopback off, interrupts (Out2) off, Out1/RTS/DTR off */
  //  Out_Byte(io_addr + 4, 0);
  // enable interrupts (bit 3)
  Out_Byte(io_addr + 4, 0x08);
}


inline static void SerialPutChar(unsigned char c) {
 
 //  static unsigned short io_adr;
  if (serial_io_addr==0) { 
    return;
  }


  if (c=='\n') { 
    /* wait for transmitter ready */
    while((In_Byte(serial_io_addr + 5) & 0x40) == 0) {
    }
    /* send char */
    Out_Byte(serial_io_addr + 0, '\r');
    /* wait for transmitter ready */
  }
  while((In_Byte(serial_io_addr + 5) & 0x40) == 0) {
  }
  /* send char */
  Out_Byte(serial_io_addr + 0, c);
}



void SerialPutLineN(char * line, int len) {
  int i;
  for (i = 0; i < len && line[i] != 0; i++) { 
    SerialPutChar(line[i]); 
  }
}


void SerialPutLine(char * line) {
  int i;
  for (i = 0; line[i]!= 0; i++) { 
    SerialPutChar(line[i]); 
  }
}


void SerialPrintHex(unsigned char x)
{
  unsigned char z;
  
  z = (x>>4) & 0xf ;
  SerialPrint("%x", z);
  z = x & 0xf;
  SerialPrint("%x", z);
}

void SerialMemDump(unsigned char *start, int n)
{
  int i, j;

  for (i=0;i<n;i+=16) {
    SerialPrint("%8x", (unsigned)(start+i));
    for (j=i; j<i+16 && j<n; j+=2) {
      SerialPrint(" ");
      SerialPrintHex(*((unsigned char *)(start+j)));
      if ((j+1)<n) { 
	SerialPrintHex(*((unsigned char *)(start+j+1)));
      }
    }
    SerialPrint(" ");
    for (j=i; j<i+16 && j<n;j++) {
      SerialPrint("%c", ((start[j]>=32) && (start[j]<=126)) ? start[j] : '.');
    }
    SerialPrint("\n");
  }
}
