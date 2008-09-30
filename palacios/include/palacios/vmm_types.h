/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#ifndef __VMM_TYPES_H
#define __VMM_TYPES_H

#ifdef __V3VEE__


typedef signed char schar_t;
typedef unsigned char uchar_t;

typedef signed short sshort_t;
typedef unsigned short ushort_t;

typedef signed int sint_t;
typedef unsigned int uint_t;

typedef signed long long sllong_t;
typedef unsigned long long ullong_t;

typedef signed long slong_t;
typedef unsigned long ulong_t;

typedef unsigned long size_t;
       

#define false 0
#define true 1
typedef uchar_t bool;



typedef unsigned long long uint64_t;
typedef long long sint64_t;

typedef unsigned int uint32_t;
typedef int sint32_t;


typedef unsigned short uint16_t;
typedef short sint16_t;

typedef unsigned char uint8_t;
typedef char sint8_t;

typedef ulong_t addr_t;

#endif // ! __V3VEE__

#endif
