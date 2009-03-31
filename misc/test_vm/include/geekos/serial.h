#ifndef SERIAL_H
#define SERIAL_H

#include <geekos/irq.h>
#include <geekos/string.h>
#include <geekos/io.h>
#include <geekos/screen.h>

#define COM1_IRQ 4
#define DEFAULT_SERIAL_ADDR 0x3F8



void SerialPrint(const char * format, ...);
void SerialPrintList(const char * format, va_list ap);

void SerialPutLine(char * line); 
void SerialPutLineN(char * line, int len);


void SerialPrintHex(unsigned char x);
void SerialMemDump(unsigned char *start, int n);

void InitSerial();
void InitSerialAddr(unsigned short io_addr);

#endif
