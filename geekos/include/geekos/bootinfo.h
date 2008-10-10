/*
 * Boot information structure, passed to kernel Main() routine
 * Copyright (c) 2001, David H. Hovemeyer <daveho@cs.umd.edu>
 * (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * $Revision: 1.7 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef GEEKOS_BOOTINFO_H
#define GEEKOS_BOOTINFO_H

struct Boot_Info {
  int bootInfoSize;	 /* size of this struct; for versioning */
  int vmm_size;
  int memSizeKB;	 /* number of KB, as reported by int 15h */

  /*Zheng 08/02/2008*/
  void * ramdisk_image; /*ramdisk load addr*/ 
  unsigned long ramdisk_size;
};

#endif  /* GEEKOS_BOOTINFO_H */
