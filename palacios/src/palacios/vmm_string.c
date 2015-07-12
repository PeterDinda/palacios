/*
 * String library 
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
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
/* Modifications by Jack Lange <jarusl@cs.northwestern.edu> */



/*
 * NOTE:
 * These are slow and simple implementations of a subset of
 * the standard C library string functions.
 * We also have an implementation of snprintf().
 */


#include <palacios/vmm_types.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm.h>


#ifdef V3_CONFIG_BUILT_IN_MEMSET
void * memset(void * s, int c, size_t n) {
    uchar_t * p = (uchar_t *) s;

    while (n > 0) {
	*p++ = (uchar_t) c;
	--n;
    }

    return s;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_MEMCPY
void * memcpy(void * dst, const void * src, size_t n) {
    uchar_t * d = (uchar_t *) dst;
    const uchar_t * s = (const uchar_t *)src;

    while (n > 0) {
	*d++ = *s++;
	--n;
    }

    return dst;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_MEMMOVE
void * memmove(void * dst, const void * src, size_t n) {
    uint8_t * tmp = (uint8_t *)V3_Malloc(n);

    if (!tmp) {
	PrintError(info->vm_info, info, "Cannot allocate in built-in memmove\n");
	return NULL;
    }
    
    memcpy(tmp, src, n);
    memcpy(dst, tmp, n);
    
    V3_Free(tmp);
    return dst;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_MEMCMP
int memcmp(const void * s1_, const void * s2_, size_t n) {
    const char * s1 = s1_;
    const char * s2 = s2_;

    while (n > 0) {
	int cmp = (*s1 - *s2);
	
	if (cmp != 0) {
	    return cmp;
	}

	++s1;
	++s2;
    }

    return 0;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRLEN
size_t strlen(const char * s) {
    size_t len = 0;

    while (*s++ != '\0') {
	++len;
    }

    return len;
}
#endif



#ifdef V3_CONFIG_BUILT_IN_STRNLEN
/*
 * This it a GNU extension.
 * It is like strlen(), but it will check at most maxlen
 * characters for the terminating nul character,
 * returning maxlen if it doesn't find a nul.
 * This is very useful for checking the length of untrusted
 * strings (e.g., from user space).
 */
size_t strnlen(const char * s, size_t maxlen) {
    size_t len = 0;

    while ((len < maxlen) && (*s++ != '\0')) {
	++len;
    }

    return len;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRCMP
int strcmp(const char * s1, const char * s2) {
    while (1) {
	int cmp = (*s1 - *s2);
	
	if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
	    return cmp;
	}
	
	++s1;
	++s2;
    }
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STRCASECMP
int strcasecmp(const char * s1, const char * s2) {
    while (1) {
	int cmp = (tolower(*s1) - tolower(*s2));

	if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
	    return cmp;
	}

	++s1;
	++s2;
    }
}

#endif


#ifdef V3_CONFIG_BUILT_IN_STRNCMP
int strncmp(const char * s1, const char * s2, size_t limit) {
    size_t i = 0;

    while (i < limit) {
	int cmp = (*s1 - *s2);

	if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
	    return cmp;
	}

	++s1;
	++s2;
	++i;
    }

    /* limit reached and equal */
    return 0;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STRNCASECMP
int strncasecmp(const char * s1, const char * s2, size_t limit) {
    size_t i = 0;

    while (i < limit) {
	int cmp = (tolower(*s1) - tolower(*s2));

	if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
	    return cmp;
	}

	++s1;
	++s2;
	++i;
    }

    return 0;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRCAT
char * strcat(char * s1, const char * s2) {
    char * t1 = s1;

    while (*s1) { s1++; }
    while (*s2) { *s1++ = *s2++; }

    *s1 = '\0';

    return t1;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRNCAT
char * strncat(char * s1, const char * s2, size_t limit) {
    size_t i = 0;
    char * t1;

    t1 = s1;

    while (*s1) { s1++; }

    while (i < limit) {
	if (*s2 == '\0') {
	    break;
	}
	*s1++ = *s2++;		
    }
    *s1 = '\0';
    return t1;
}
#endif



#ifdef V3_CONFIG_BUILT_IN_STRCPY
char * strcpy(char * dest, const char * src)
{
    char *ret = dest;

    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';

    return ret;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRNCPY
char * strncpy(char * dest, const char * src, size_t limit) {
    char * ret = dest;

    while ((*src != '\0') && (limit > 0)) {
	*dest++ = *src++;
	--limit;
    }

    if (limit > 0)
	*dest = '\0';

    return ret;
}
#endif



#ifdef  V3_CONFIG_BUILT_IN_STRDUP
char * strdup(const char * s1) {
    char *ret;

    ret = V3_Malloc(strlen(s1) + 1);

    if (!ret) {
        PrintError(VM_NONE, VCORE_NONE, "Cannot allocate in built-in strdup\n");
	return NULL;
    }

    strcpy(ret, s1);

    return ret;
}
#endif




#ifdef V3_CONFIG_BUILT_IN_ATOI
int atoi(const char * buf) {
    int ret = 0;

    while ((*buf >= '0') && (*buf <= '9')) {
	ret *= 10;
	ret += (*buf - '0');
	buf++;
    }

    return ret;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRTOI
int strtoi(const char * nptr, char ** endptr) {
    int ret = 0;
    char * buf = (char *)nptr;

    while ((*buf >= '0') && (*buf <= '9')) {
	ret *= 10;
	ret += (*buf - '0');

	buf++;

	if (endptr) {
	    *endptr = buf;
	}
    }

    return ret;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_ATOX
uint64_t atox(const char * buf) {
    uint64_t ret = 0;

    if (*(buf + 1) == 'x') {
	buf += 2;
    }

    while (isxdigit(*buf)) {
	ret <<= 4;
	
	if (isdigit(*buf)) {
	    ret += (*buf - '0');
	} else {
	    ret += tolower(*buf) - 'a' + 10;
	}

	buf++;
    }

    return ret;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STRTOX
uint64_t strtox(const char * nptr, char ** endptr) {
    uint64_t ret = 0;
    char * buf = (char *)nptr;

    if (*(buf + 1) == 'x') {
	buf += 2;
    }

    while (isxdigit(*buf)) {
	ret <<= 4;
	
	if (isdigit(*buf)) {
	    ret += (*buf - '0');
	} else {
	    ret += tolower(*buf) - 'a' + 10;
	}

	buf++;

	if (endptr) {
	    *endptr = buf;
	}
    }

    return ret;

}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRCHR
char * strchr(const char * s, int c) {
    while (*s != '\0') {
	if (*s == c)
	    return (char *)s;
	++s;
    }
    return 0;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRRCHR
char * strrchr(const char * s, int c) {
    size_t len = strlen(s);
    const char * p = s + len;

    while (p > s) {
	--p;

	if (*p == c) {
	    return (char *)p;
	}
    }
    return 0;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STRPBRK
char * strpbrk(const char * s, const char * accept) {
    size_t setLen = strlen(accept);

    while (*s != '\0') {
	size_t i;
	for (i = 0; i < setLen; ++i) {
	    if (*s == accept[i]) {
		return (char *)s;
	    }
	}
	++s;
    }

    return 0;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STRSPN
size_t strspn(const char * s, const char * accept) {
    int match = 1;
    int cnt = 0;
    int i = 0;
    int accept_len = strlen(accept);

    while (match) {
	match = 0;

	for (i = 0; i < accept_len; i++) {
	    if (s[cnt] == accept[i]) {
		match = 1;
		cnt++;
		break;
	    }
	}
    }

    return cnt;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRCSPN
size_t strcspn(const char * s, const char * reject) {
    int match = 0;
    int cnt = 0;
    int i = 0;
    int reject_len = strlen(reject);

    while (!match) {
	for (i = 0; i < reject_len; i++) {
	    if (s[cnt] == reject[i]) {
		match = 1;
		break;
	    }
	}

	if (!match) {
	    cnt++;
	}

    }

    return cnt;
}
#endif


#ifdef V3_CONFIG_BUILT_IN_STRSTR
char *strstr(const char *haystack, const char *needle)
{
        int l1, l2;

        l2 = strlen(s2);
        if (!l2)
                return (char *)s1;
        l1 = strlen(s1);
        while (l1 >= l2) {
                l1--;
                if (!memcmp(s1, s2, l2))
                        return (char *)s1;
                s1++;
        }
        return NULL;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STR_TOLOWER
void str_tolower(char * s) {
    while (isalpha(*s)) {
	if (!islower(*s)) {
	    *s = tolower(*s);
	}
	s++;
    }
}
#endif

#ifdef V3_CONFIG_BUILT_IN_STR_TOUPPER
void str_toupper(char * s) {
    while (isalpha(*s)) {
	if (!isupper(*s)) {
	    *s = toupper(*s);
	}
	s++;
    }
}
#endif
