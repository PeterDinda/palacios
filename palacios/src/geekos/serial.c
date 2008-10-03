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
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <geekos/serial.h>
#include <geekos/reboot.h>
#include <geekos/gdt.h>
#include <geekos/idt.h>
#include <geekos/fmtout.h>




unsigned short serial_io_addr = 0;
uint_t serial_print_level;

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




void SerialPutChar(unsigned char c) {
 
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


static struct Output_Sink serial_output_sink;
static void Serial_Emit(struct Output_Sink * o, int ch) { 
  SerialPutChar((unsigned char)ch); 
}
static void Serial_Finish(struct Output_Sink * o) { return; }


static void __inline__ SerialPrintInternal(const char * format, va_list ap) {
  Format_Output(&serial_output_sink, format, ap);
}


void SerialPrint(const char * format, ...) {
  va_list args;
  bool iflag = Begin_Int_Atomic();

  va_start(args, format);
  SerialPrintInternal(format, args);
  va_end(args);

  End_Int_Atomic(iflag);
}

void SerialPrintList(const char * format, va_list ap) {
  bool iflag = Begin_Int_Atomic();
  SerialPrintInternal(format, ap);
  End_Int_Atomic(iflag);

}




void SerialPrintLevel(int level, const char * format, ...) {
  if (level > serial_print_level) {
    va_list args;
    bool iflag = Begin_Int_Atomic();
    
    va_start(args, format);
    SerialPrintInternal(format, args);
    va_end(args);
    
    End_Int_Atomic(iflag);   
  }
}




void Init_Serial() {

  serial_print_level = SERIAL_PRINT_DEBUG_LEVEL;

  Print("Initialzing Serial\n");

  serial_output_sink.Emit = &Serial_Emit;
  serial_output_sink.Finish = &Serial_Finish;

  Install_IRQ(COM1_IRQ, Serial_Interrupt_Handler);
  Enable_IRQ(COM1_IRQ);
  InitSerialAddr(DEFAULT_SERIAL_ADDR);
}
