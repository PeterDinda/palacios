#ifndef VM_CONS_H
#define VM_CONS_H

#include <geekos/string.h>
#include <geekos/io.h>
#include <geekos/screen.h>


void Init_VMCons();


void VMConsPutChar(unsigned char c);

void VMConsPrint(const char * format, ...);
void VMConsPrintLevel(int level, const char * format, ...);
void VMConsPrintList(const char * format, va_list ap);

void VMConsPutLine(char * line); 
void VMConsPutLineN(char * line, int len);


void VMConsPrintHex(unsigned char x);
void VMConsMemDump(unsigned char *start, int n);

#endif
