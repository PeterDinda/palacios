/* * String library
 * Copyright (c) 2001,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Modifications:
 * 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * 2008, Lei Xia <xiaxlei@gmail.com> 
 */

#ifndef __VMM_STRING_H__
#define __VMM_STRING_H__

#ifdef __V3VEE__

#include <palacios/vmm_stddef.h>


void* memset(void* s, int c, size_t n);
void* memcpy(void *dst, const void* src, size_t n);
//void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char* s);
size_t strnlen(const char *s, size_t maxlen);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t limit);
char *strcat(char *s1, const char *s2);
char *strncat(char *s1, const char *s2, size_t limit);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t limit);
char *strdup(const char *s1);
int atoi(const char *buf);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strpbrk(const char *s, const char *accept);



#define in_range(c, lo, up)  ((uint8_t)c >= lo && (uint8_t)c <= up)
#define isprint(c)           in_range(c, 0x20, 0x7f)
#define isdigit(c)           in_range(c, '0', '9')
#define isxdigit(c)          (isdigit(c) || in_range(c, 'a', 'f') || in_range(c, 'A', 'F'))
#define islower(c)           in_range(c, 'a', 'z')
#define isspace(c)           (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')


#endif // !__V3VEE__

#endif  /* STRING_H */
