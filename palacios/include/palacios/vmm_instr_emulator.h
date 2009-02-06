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

#include <palacios/vmm_types.h>



#define MAKE_1OP_8FLAGS_WINST(iname) static inline void iname##8(addr_t * dst,  addr_t * flags) { \
    uchar_t tmp_dst = *dst;						\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (								\
	 "pushf; "							\
	 "push %2; "							\
	 "popf; "							\
	 #iname"b %0; "							\
	 "pushf; "							\
	 "pop %1; "							\
	 "popf; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(*flags), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }

#define MAKE_1OP_16FLAGS_WINST(iname) static inline void iname##16(addr_t * dst,  addr_t * flags) { \
    ushort_t tmp_dst = *dst;						\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (								\
	 "pushf; "							\
	 "push %2; "							\
	 "popf; "							\
	 #iname"w %0; "							\
	 "pushf; "							\
	 "pop %1; "							\
	 "popf; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(*flags), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }

#define MAKE_1OP_32FLAGS_WINST(iname) static inline void iname##32(addr_t * dst,  addr_t * flags) { \
    uint_t tmp_dst = *dst;						\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (								\
	 "pushf; "							\
	 "push %2; "							\
	 "popf; "							\
	 #iname"l %0; "							\
	 "pushf; "							\
	 "pop %1; "							\
	 "popf; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(*flags), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }

#define MAKE_1OP_64FLAGS_WINST(iname) static inline void iname##64(addr_t * dst,  addr_t * flags) { \
    ullong_t tmp_dst = *dst;						\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (								\
	 "pushfq; "							\
	 "push %2; "							\
	 "popfq; "							\
	 #iname"q %0; "							\
	 "pushfq; "							\
	 "pop %1; "							\
	 "popfq; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(*flags), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }



#define MAKE_1OP_8_WINST(iname) static inline void iname##8(addr_t * dst) { \
    uchar_t tmp_dst = *dst;						\
									\
    asm volatile (							\
		  #iname"b %0; "					\
		  : "=q"(tmp_dst)					\
		  : "0"(tmp_dst)					\
		  );							\
    *dst = tmp_dst;							\
  }

#define MAKE_1OP_16_WINST(iname) static inline void iname##16(addr_t * dst) { \
    ushort_t tmp_dst = *dst;						\
    									\
    asm volatile (							\
	 #iname"w %0; "							\
	 : "=q"(tmp_dst)						\
	 :  "0"(tmp_dst)						\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_1OP_32_WINST(iname) static inline void iname##32(addr_t * dst) { \
    uint_t tmp_dst = *dst;						\
									\
    asm volatile (							\
	 #iname"l %0; "							\
	 : "=q"(tmp_dst)						\
	 : "0"(tmp_dst)							\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_1OP_64_WINST(iname) static inline void iname##64(addr_t * dst) { \
    ullong_t tmp_dst = *dst;						\
    									\
    asm volatile (							\
		  #iname"q %0; "					\
		  : "=q"(tmp_dst)					\
		  : "0"(tmp_dst)					\
		  );							\
    *dst = tmp_dst;							\
  }


#define MAKE_2OP_64FLAGS_WINST(iname) static inline void iname##64(addr_t * dst, addr_t * src, addr_t * flags) { \
    uint64_t tmp_dst = *dst, tmp_src = *src;					\
    addr_t tmp_flags = *flags;						\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (								\
	 "pushfq\r\n"							\
	 "push %3\r\n"							\
	 "popfq\r\n"							\
	 #iname"q %2, %0\r\n"						\
	 "pushfq\r\n"							\
	 "pop %1\r\n"							\
	 "popfq\r\n"							\
	 : "=q"(tmp_dst),"=q"(tmp_flags)				\
	 : "q"(tmp_src),"q"(tmp_flags), "0"(tmp_dst)			\
	 );								\
									\
    *dst = tmp_dst;							\
    *flags = tmp_flags;							\
    *flags |= flags_rsvd;						\
									\
  }




#define MAKE_2OP_32FLAGS_WINST(iname) static inline void iname##32(addr_t * dst, addr_t * src, addr_t * flags) { \
    uint32_t tmp_dst = *dst, tmp_src = *src;				\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushf; "							\
	 "push %3; "							\
	 "popf; "							\
	 #iname"l %2, %0; "						\
	 "pushf; "							\
	 "pop %1; "							\
	 "popf; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(tmp_src),"q"(*flags), "0"(tmp_dst)			\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }


#define MAKE_2OP_16FLAGS_WINST(iname) static inline void iname##16(addr_t * dst, addr_t * src, addr_t * flags) { \
    ushort_t tmp_dst = *dst, tmp_src = *src;				\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushf; "							\
	 "push %3; "							\
	 "popf; "							\
	 #iname"w %2, %0; "						\
	 "pushf; "							\
	 "pop %1; "							\
	 "popf; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(tmp_src),"q"(*flags), "0"(tmp_dst)			\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }

#define MAKE_2OP_8FLAGS_WINST(iname) static inline void iname##8(addr_t * dst, addr_t * src, addr_t * flags) { \
    uchar_t tmp_dst = *dst, tmp_src = *src;				\
									\
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushf; "							\
	 "push %3; "							\
	 "popf; "							\
	 #iname"b %2, %0; "						\
	 "pushf; "							\
	 "pop %1; "							\
	 "popf; "							\
	 : "=q"(tmp_dst),"=q"(*flags)					\
	 : "q"(tmp_src),"q"(*flags), "0"(tmp_dst)			\
	 );								\
    *dst = tmp_dst;							\
    *flags |= flags_rsvd;						\
									\
  }





#define MAKE_2OP_64STR_WINST(iname) static inline void iname##64(addr_t * dst, \
								 addr_t * src, \
								 addr_t * ecx, addr_t * flags) { \
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushfq; "							\
	 "pushq %4; "							\
	 "popfq; "							\
	 "rep; "							\
	 #iname"q; "							\
	 "pushfq; "							\
	 "popq %0; "							\
	 "popfq; "							\
	 : "=q"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"q"(*flags)			\
	 );								\
									\
    /*	 : "=D"(*dst),"=S"(*src),"=c"(*ecx),"=q"(*flags)*/		\
    *flags |= flags_rsvd;						\
  }


#define MAKE_2OP_32STR_WINST(iname) static inline void iname##32(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushf; "							\
	 "push %4; "							\
	 "popf; "							\
	 "rep; "							\
	 #iname"l; "							\
	 "pushf; "							\
	 "pop %0; "							\
	 "popf; "							\
	 : "=q"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"q"(*flags)			\
	 );								\
									\
    /*	 : "=D"(*dst),"=S"(*src),"=c"(*ecx),"=q"(*flags)*/		\
    *flags |= flags_rsvd;						\
  }

#define MAKE_2OP_16STR_WINST(iname) static inline void iname##16(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
     /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushf; "							\
	 "push %4; "							\
	 "popf; "							\
	 "rep; "							\
	 #iname"w; "							\
	 "pushf; "							\
	 "pop %0; "							\
	 "popf; "							\
	 : "=q"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"q"(*flags)			\
	 );								\
    *flags |= flags_rsvd;						\
  }



#define MAKE_2OP_8STR_WINST(iname) static inline void iname##8(addr_t * dst, \
							      addr_t * src, \
							      addr_t * ecx, addr_t * flags) { \
    /* Some of the flags values are not copied out in a pushf, we save them here */ \
    addr_t flags_rsvd = *flags & ~0xfffe7fff;				\
									\
    asm volatile (							\
	 "pushf; "							\
	 "push %4; "							\
	 "popf; "							\
	 "rep; "							\
	 #iname"b; "							\
	 "pushf; "							\
	 "pop %0; "							\
	 "popf; "							\
	 : "=q"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"q"(*flags)			\
	 );								\
    *flags |= flags_rsvd;						\
  }




#define MAKE_2OP_64_WINST(iname) static inline void iname##64(addr_t * dst, addr_t * src) { \
    uint32_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"q %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_2OP_32_WINST(iname) static inline void iname##32(addr_t * dst, addr_t * src) { \
    uint32_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"l %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_2OP_16_WINST(iname) static inline void iname##16(addr_t * dst, addr_t * src) { \
    ushort_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"w %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_2OP_8_WINST(iname) static inline void iname##8(addr_t * dst, addr_t * src) { \
    uchar_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"b %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }







MAKE_2OP_8FLAGS_WINST(adc);
MAKE_2OP_8FLAGS_WINST(add);
MAKE_2OP_8FLAGS_WINST(and);
MAKE_2OP_8FLAGS_WINST(or);
MAKE_2OP_8FLAGS_WINST(xor);
MAKE_2OP_8FLAGS_WINST(sub);


MAKE_1OP_8FLAGS_WINST(inc);
MAKE_1OP_8FLAGS_WINST(dec);
MAKE_1OP_8FLAGS_WINST(neg);
MAKE_1OP_8FLAGS_WINST(setb);
MAKE_1OP_8FLAGS_WINST(setbe);
MAKE_1OP_8FLAGS_WINST(setl);
MAKE_1OP_8FLAGS_WINST(setle);
MAKE_1OP_8FLAGS_WINST(setnb);
MAKE_1OP_8FLAGS_WINST(setnbe);
MAKE_1OP_8FLAGS_WINST(setnl);
MAKE_1OP_8FLAGS_WINST(setnle);
MAKE_1OP_8FLAGS_WINST(setno);
MAKE_1OP_8FLAGS_WINST(setnp);
MAKE_1OP_8FLAGS_WINST(setns);
MAKE_1OP_8FLAGS_WINST(setnz);
MAKE_1OP_8FLAGS_WINST(seto);
MAKE_1OP_8FLAGS_WINST(setp);
MAKE_1OP_8FLAGS_WINST(sets);
MAKE_1OP_8FLAGS_WINST(setz);


MAKE_1OP_8_WINST(not);

MAKE_2OP_8_WINST(mov);
MAKE_2OP_8_WINST(xchg);



MAKE_2OP_16FLAGS_WINST(adc);
MAKE_2OP_16FLAGS_WINST(add);
MAKE_2OP_16FLAGS_WINST(and);
MAKE_2OP_16FLAGS_WINST(or);
MAKE_2OP_16FLAGS_WINST(xor);
MAKE_2OP_16FLAGS_WINST(sub);


MAKE_1OP_16FLAGS_WINST(inc);
MAKE_1OP_16FLAGS_WINST(dec);
MAKE_1OP_16FLAGS_WINST(neg);

MAKE_1OP_16_WINST(not);

MAKE_2OP_16_WINST(mov);
MAKE_2OP_16_WINST(xchg);





MAKE_2OP_32FLAGS_WINST(adc);
MAKE_2OP_32FLAGS_WINST(add);
MAKE_2OP_32FLAGS_WINST(and);
MAKE_2OP_32FLAGS_WINST(or);
MAKE_2OP_32FLAGS_WINST(xor);
MAKE_2OP_32FLAGS_WINST(sub);


MAKE_1OP_32FLAGS_WINST(inc);
MAKE_1OP_32FLAGS_WINST(dec);
MAKE_1OP_32FLAGS_WINST(neg);

MAKE_1OP_32_WINST(not);

MAKE_2OP_32_WINST(mov);
MAKE_2OP_32_WINST(xchg);

MAKE_2OP_8STR_WINST(movs);
MAKE_2OP_16STR_WINST(movs);
MAKE_2OP_32STR_WINST(movs);
