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

#ifndef SERIAL_H
#define SERIAL_H

#include <geekos/irq.h>
#include <geekos/string.h>
#include <geekos/io.h>
#include <geekos/screen.h>

#define COM1_IRQ 4
#define DEFAULT_SERIAL_ADDR 0x3F8


#ifndef SERIAL_PRINT_DEBUG_LEVEL
#define SERIAL_PRINT_DEBUG_LEVEL  10
#endif

void SerialPutChar(unsigned char c);

void SerialPrint(const char * format, ...);
void SerialPrintLevel(int level, const char * format, ...);
void SerialPrintList(const char * format, va_list ap);

void SerialPutLine(char * line); 
void SerialPutLineN(char * line, int len);


void SerialPrintHex(unsigned char x);
void SerialMemDump(unsigned char *start, int n);

void Init_Serial();
void InitSerialAddr(unsigned short io_addr);

#endif
