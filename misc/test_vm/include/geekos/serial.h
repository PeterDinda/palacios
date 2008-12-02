#ifndef SERIAL_H
#define SERIAL_H

#include <geekos/irq.h>
#include <geekos/string.h>
#include <geekos/io.h>
#include <geekos/screen.h>

#define COM1_IRQ 4
#define DEFAULT_SERIAL_ADDR 0x3F8


#ifndef SERIAL_PRINT
#define SERIAL_PRINT              1
#endif
#ifndef SERIAL_PRINT_DEBUG
#define SERIAL_PRINT_DEBUG        1 
#endif
#ifndef SERIAL_PRINT_DEBUG_LEVEL
#define SERIAL_PRINT_DEBUG_LEVEL  10
#endif

#define SERIAL_PRINT_MAXBUF       256
 

#if SERIAL_PRINT						 
#define SerialPrint(format, args...)                             \
do {                                                             \
  char buf[SERIAL_PRINT_MAXBUF];                                                 \
  snprintf( buf, SERIAL_PRINT_MAXBUF, format, ## args ) ;			 \
  SerialPutLineN(buf, SERIAL_PRINT_MAXBUF);	       				 \
} while (0)  
#else
#define SerialPrint(format, args...) do {} while (0)
#endif


#define PrintBoth(format, args...) \
do {  \
  Print(format, ## args); \
  SerialPrint(format, ##args); \
 } while (0)


#if SERIAL_PRINT_DEBUG
#define SerialPrintLevel(level, format, args...)		 \
do {                                                             \
  char buf[SERIAL_PRINT_MAXBUF];                                                 \
  if (level >= SERIAL_PRINT_DEBUG_LEVEL  ) {                                     \
    snprintf( buf, SERIAL_PRINT_MAXBUF, format, ## args ) ;		\
    SerialPutLineN(buf, SERIAL_PRINT_MAXBUF);				\
  }                                                                     \
} while (0)  
#else
#define SerialPrintLevel(level, format, args...) do {} while (0)
#endif


void SerialPutLine(char * line); 
void SerialPutLineN(char * line, int len);


void SerialPrintHex(unsigned char x);
void SerialMemDump(unsigned char *start, int n);

void InitSerial();
void InitSerialAddr(unsigned short io_addr);

#endif
