/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#include <geekos/debug.h>


void PrintBoth(const char * format, ...) {
  va_list args;

  va_start(args, format);
  PrintList(format, args);
  SerialPrintList(format, args);
  va_end(args);
}
