/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <geekos/string.h>
#include <geekos/debug.h>
#include <geekos/vm_cons.h>
#include <geekos/serial.h>
#include <geekos/screen.h>
#include <geekos/fmtout.h>


void PrintBoth(const char * format, ...) {
  va_list args;

  va_start(args, format);
  PrintList(format, args);
#ifdef DEBUG_SERIAL
  SerialPrintList(format, args);
#endif
  VMConsPrintList(format, args);
  va_end(args);
}
