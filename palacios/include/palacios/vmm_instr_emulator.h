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

#define FLAGS_MASK 0x00000cff



#define MAKE_1OP_8FLAGS_INST(iname) static inline void iname##8(addr_t * dst,  addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint8_t tmp_dst = *(uint8_t *)dst;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %2; "					\
		      "popf; "						\
		      #iname"b %0; "					\
		      "pushf; "						\
		      "pop %1; "					\
		      "popf; "						\
		      : "=&q"(tmp_dst),"=q"(guest_flags)		\
		      : "1"(guest_flags), "0"(tmp_dst)			\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint8_t *)dst = tmp_dst;					\
									\
    }

#define MAKE_1OP_16FLAGS_INST(iname) static inline void iname##16(addr_t * dst,  addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint16_t tmp_dst = *(uint16_t *)dst;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %2; "					\
		      "popf; "						\
		      #iname"w %0; "					\
		      "pushf; "						\
		      "pop %1; "					\
		      "popf; "						\
		      : "=&q"(tmp_dst),"=q"(guest_flags)		\
		      : "1"(guest_flags), "0"(tmp_dst)			\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint16_t *)dst = tmp_dst;					\
    }

#define MAKE_1OP_32FLAGS_INST(iname) static inline void iname##32(addr_t * dst,  addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint32_t tmp_dst = *(uint32_t *)dst;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %2; "					\
		      "popf; "						\
		      #iname"l %0; "					\
		      "pushf; "						\
		      "pop %1; "					\
		      "popf; "						\
		      : "=&q"(tmp_dst),"=q"(guest_flags)		\
		      : "1"(guest_flags), "0"(tmp_dst)			\
		      : "memory"					\
		      );						\
									\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint32_t *)dst = tmp_dst;					\
    }

#define MAKE_1OP_64FLAGS_INST(iname) static inline void iname##64(addr_t * dst,  addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint64_t tmp_dst = *(uint64_t *)dst;				\
									\
	asm volatile (							\
		      "pushfq; "					\
		      "push %2; "					\
		      "popfq; "						\
		      #iname"q %0; "					\
		      "pushfq; "					\
		      "pop %1; "					\
		      "popfq; "						\
		      : "=q"(tmp_dst),"=q"(guest_flags)			\
		      : "q"(guest_flags), "0"(tmp_dst)			\
		      : "memory"					\
		      );						\
									\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint64_t *)dst = tmp_dst;					\
    }



#define MAKE_1OP_8_INST(iname) static inline void iname##8(addr_t * dst) { \
	uint8_t tmp_dst = *(uint8_t *)dst;				\
	asm volatile (							\
		      #iname"b %0; "					\
		      : "=q"(tmp_dst)					\
		      : "0"(tmp_dst)					\
		      : "memory"					\
		      );						\
	*(uint8_t *)dst = tmp_dst;					\
    }

#define MAKE_1OP_16_INST(iname) static inline void iname##16(addr_t * dst) { \
	uint16_t tmp_dst = *(uint16_t *)dst;				\
	asm volatile (							\
		      #iname"w %0; "					\
		      : "=q"(tmp_dst)					\
		      :  "0"(tmp_dst)					\
		      : "memory"					\
		      );						\
	*(uint16_t *)dst = tmp_dst;					\
    }

#define MAKE_1OP_32_INST(iname) static inline void iname##32(addr_t * dst) { \
	uint32_t tmp_dst = *(uint32_t *)dst;				\
	asm volatile (							\
		      #iname"l %0; "					\
		      : "=q"(tmp_dst)					\
		      : "0"(tmp_dst)					\
		      : "memory"					\
		      );						\
	*(uint32_t *)dst = tmp_dst;					\
    }

#define MAKE_1OP_64_INST(iname) static inline void iname##64(addr_t * dst) { \
	uint64_t tmp_dst = *(uint64_t *)dst;				\
	asm volatile (							\
		      #iname"q %0; "					\
		      : "=q"(tmp_dst)					\
		      : "0"(tmp_dst)					\
		      : "memory"					\
		      );						\
	*(uint64_t *)dst = tmp_dst;					\
    }


#define MAKE_2OP_64FLAGS_INST(iname) static inline void iname##64(addr_t * dst, addr_t * src, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint64_t tmp_dst = *(uint64_t *)dst;				\
									\
	asm volatile (							\
		      "pushfq\r\n"					\
		      "push %3\r\n"					\
		      "popfq\r\n"					\
		      #iname"q %2, %0\r\n"				\
		      "pushfq\r\n"					\
		      "pop %1\r\n"					\
		      "popfq\r\n"					\
		      : "=&q"(tmp_dst),"=q"(guest_flags)			\
		      : "q"(*(uint64_t *)src),"1"(guest_flags), "0"(tmp_dst) \
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint64_t *)dst = tmp_dst;					\
    }




#define MAKE_2OP_32FLAGS_INST(iname) static inline void iname##32(addr_t * dst, addr_t * src, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint32_t tmp_dst = *(uint32_t *)dst;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %3; "					\
		      "popf; "						\
		      #iname"l %2, %0; "				\
		      "pushf; "						\
		      "pop %1; "					\
		      "popf; "						\
		      : "=&q"(tmp_dst),"=q"(guest_flags)		\
		      : "q"(*(uint32_t *)src),"1"(guest_flags), "0"(tmp_dst) \
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint32_t *)dst = tmp_dst;					\
    }


#define MAKE_2OP_16FLAGS_INST(iname) static inline void iname##16(addr_t * dst, addr_t * src, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint16_t tmp_dst = *(uint16_t *)dst;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %3; "					\
		      "popf; "						\
		      #iname"w %2, %0; "				\
		      "pushf; "						\
		      "pop %1; "					\
		      "popf; "						\
		      : "=&q"(tmp_dst),"=q"(guest_flags)		\
		      : "q"(*(uint16_t *)src),"1"(guest_flags), "0"(tmp_dst) \
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint16_t *)dst = tmp_dst;					\
    }

#define MAKE_2OP_8FLAGS_INST(iname) static inline void iname##8(addr_t * dst, addr_t * src, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint8_t tmp_dst = *(uint8_t *)dst;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %3; "					\
		      "popf; "						\
		      #iname"b %2, %0; "				\
		      "pushf; "						\
		      "pop %1; "					\
		      "popf; "						\
		      : "=q"(tmp_dst),"=q"(guest_flags)			\
		      : "q"(*(uint8_t *)src),"1"(guest_flags), "0"(tmp_dst) \
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
	*(uint8_t *)dst = tmp_dst;					\
    }





#define MAKE_2OP_64STR_INST(iname) static inline void iname##64(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint64_t tmp_dst = 0, tmp_ecx = 0, tmp_src = 0;			\
									\
	asm volatile (							\
		      "pushfq; "					\
		      "pushq %7; "					\
		      "popfq; "						\
		      "rep; "						\
		      #iname"q; "					\
		      "pushfq; "					\
		      "popq %0; "					\
		      "popfq; "						\
		      : "=q"(guest_flags), "=&D"(tmp_dst), "=&S"(tmp_src), "=&c"(tmp_ecx) \
		      : "1"(*dst),"2"(*src),"3"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }


#define MAKE_2OP_32STR_INST(iname) static inline void iname##32(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint32_t tmp_dst = 0, tmp_ecx = 0, tmp_src = 0;			\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %7; "					\
		      "popf; "						\
		      "rep; "						\
		      #iname"l; "					\
		      "pushf; "						\
		      "pop %0; "					\
		      "popf; "						\
		      : "=q"(guest_flags), "=&D"(tmp_dst), "=&S"(tmp_src), "=&c"(tmp_ecx) \
		      : "1"(*dst),"2"(*src),"3"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }

#define MAKE_2OP_16STR_INST(iname) static inline void iname##16(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint16_t tmp_dst = 0, tmp_ecx = 0, tmp_src = 0;			\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %7; "					\
		      "popf; "						\
		      "rep; "						\
		      #iname"w; "					\
		      "pushf; "						\
		      "pop %0; "					\
		      "popf; "						\
		      : "=q"(guest_flags), "=&D"(tmp_dst), "=&S"(tmp_src), "=&c"(tmp_ecx) \
		      : "1"(*dst),"2"(*src),"3"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }



#define MAKE_2OP_8STR_INST(iname) static inline void iname##8(addr_t * dst, \
							      addr_t * src, \
							      addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint8_t tmp_dst = 0, tmp_ecx = 0, tmp_src = 0;			\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %7; "					\
		      "popf; "						\
		      "rep; "						\
		      #iname"b; "					\
		      "pushf; "						\
		      "pop %0; "					\
		      "popf; "						\
		      : "=q"(guest_flags), "=&D"(tmp_dst), "=&S"(tmp_src), "=&c"(tmp_ecx) \
		      : "1"(*dst),"2"(*src),"3"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }




#define MAKE_1OP_64STR_INST(iname) static inline void iname##64(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint64_t tmp_dst = 0, tmp_ecx = 0;				\
									\
	asm volatile (							\
		      "pushfq; "					\
		      "pushq %6; "					\
		      "popfq; "						\
		      "rep; "						\
		      #iname"q; "					\
		      "pushfq; "					\
		      "popq %0; "					\
		      "popfq; "						\
		      : "=&q"(guest_flags), "=&D"(tmp_dst), "=&c"(tmp_ecx) \
		      : "1"(*dst),"a"(*src),"2"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
									\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }


#define MAKE_1OP_32STR_INST(iname) static inline void iname##32(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint32_t tmp_dst = 0, tmp_ecx = 0;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %6; "					\
		      "popf; "						\
		      "rep; "						\
		      #iname"l; "					\
		      "pushf; "						\
		      "pop %0; "					\
		      "popf; "						\
		      : "=&q"(guest_flags), "=&D"(tmp_dst), "=&c"(tmp_ecx) \
		      : "1"(*dst),"a"(*src),"2"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
									\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }

#define MAKE_1OP_16STR_INST(iname) static inline void iname##16(addr_t * dst, \
								addr_t * src, \
								addr_t * ecx, addr_t * flags) { \
	addr_t guest_flags = *flags & FLAGS_MASK;			\
	uint16_t tmp_dst = 0, tmp_ecx = 0;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %6; "					\
		      "popf; "						\
		      "rep; "						\
		      #iname"w; "					\
		      "pushf; "						\
		      "pop %0; "					\
		      "popf; "						\
		      : "=q"(guest_flags), "=&D"(tmp_dst), "=&c"(tmp_ecx) \
		      : "1"(*dst),"a"(*src),"2"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }



#define MAKE_1OP_8STR_INST(iname) static inline void iname##8(addr_t * dst, \
							      addr_t * src, \
							      addr_t * ecx, addr_t * flags) { \
	/* Some of the flags values are not copied out in a pushf, we save them here */ \
	addr_t guest_flags = *flags & 0x00000cff;			\
	uint8_t tmp_dst = 0, tmp_ecx = 0;				\
									\
	asm volatile (							\
		      "pushf; "						\
		      "push %6; "					\
		      "popf; "						\
		      "rep; "						\
		      #iname"b; "					\
		      "pushf; "						\
		      "pop %0; "					\
		      "popf; "						\
		      : "=&q"(guest_flags), "=&D"(tmp_dst), "=&c"(tmp_ecx) \
		      : "1"(*dst),"a"(*src),"2"(*ecx),"0"(guest_flags)	\
		      : "memory"					\
		      );						\
	*flags &= ~FLAGS_MASK;						\
	*flags |= (guest_flags & FLAGS_MASK);				\
    }




#define MAKE_2OP_64_INST(iname) static inline void iname##64(addr_t * dst, addr_t * src) { \
	uint64_t tmp_dst = *(uint64_t *)dst;				\
	asm volatile (							\
		      #iname"q %1, %0; "				\
		      : "=&q"(tmp_dst)					\
		      : "q"(*(uint64_t *)src), "0"(tmp_dst)		\
		      : "memory"					\
		      );						\
	*(uint64_t *)dst = tmp_dst;					\
    }

#define MAKE_2OP_32_INST(iname) static inline void iname##32(addr_t * dst, addr_t * src) { \
	uint32_t tmp_dst = *(uint32_t *)dst;				\
	asm volatile (							\
		      #iname"l %1, %0; "				\
		      : "=&q"(tmp_dst)					\
		      : "q"(*(uint32_t *)src), "0"(tmp_dst)		\
		      : "memory"					\
		      );						\
 	*(uint32_t *)dst = tmp_dst;					\
   }

#define MAKE_2OP_16_INST(iname) static inline void iname##16(addr_t * dst, addr_t * src) { \
	uint16_t tmp_dst = *(uint16_t *)dst;				\
	asm volatile (							\
		      #iname"w %1, %0; "				\
		      : "=&q"(tmp_dst)					\
		      : "q"(*(uint16_t *)src), "0"(tmp_dst)		\
		      : "memory"					\
		      );						\
	*(uint16_t *)dst = tmp_dst;					\
    }

#define MAKE_2OP_8_INST(iname) static inline void iname##8(addr_t * dst, addr_t * src) { \
	uint8_t tmp_dst = *(uint8_t *)dst;				\
	asm volatile (							\
		      #iname"b %1, %0; "				\
		      : "=&q"(tmp_dst)					\
		      : "q"(*(uint8_t *)src), "0"(tmp_dst)		\
		      : "memory"					\
		      );						\
	*(uint8_t *)dst = tmp_dst;					\
    }





#define MAKE_2OP_8EXT_INST(iname) static inline void iname##8(addr_t * dst, addr_t * src, uint_t dst_len) { \
	if (dst_len == 2) {						\
	    uint16_t tmp_dst = *(uint16_t *)dst;			\
	    asm volatile (						\
			  #iname" %1, %0; "				\
			  : "=&q"(tmp_dst)				\
			  : "q"(*(uint8_t *)src), "0"(tmp_dst)		\
			  : "memory"					\
			  );						\
	    *(uint16_t *)dst = tmp_dst;					\
	} else if (dst_len == 4) {					\
	    uint32_t tmp_dst = *(uint32_t *)dst;			\
	    asm volatile (						\
			  #iname" %1, %0; "				\
			  : "=&q"(tmp_dst)				\
			  : "q"(*(uint8_t *)src), "0"(tmp_dst)		\
			  : "memory"					\
			  );						\
	    *(uint32_t *)dst = tmp_dst;					\
	} else if (dst_len == 8) {					\
	    uint64_t tmp_dst = *(uint64_t *)dst;			\
	    asm volatile (						\
			  #iname" %1, %0; "				\
			  : "=&q"(tmp_dst)				\
			  : "q"(*(uint8_t *)src), "0"(tmp_dst)		\
			  : "memory"					\
			  );						\
	    *(uint64_t *)dst = tmp_dst;					\
	}								\
    }

#define MAKE_2OP_16EXT_INST(iname) static inline void iname##16(addr_t * dst, addr_t * src, uint_t dst_len) { \
	if (dst_len == 4) {						\
	    uint32_t tmp_dst = *(uint32_t *)dst;			\
	    asm volatile (						\
			  #iname" %1, %0; "				\
			  : "=&q"(tmp_dst)				\
			  : "q"(*(uint16_t *)src), "0"(tmp_dst)		\
			  : "memory"					\
			  );						\
	    *(uint32_t *)dst = tmp_dst;					\
	} else if (dst_len == 8) {					\
	    uint64_t tmp_dst = *(uint64_t *)dst;			\
	    asm volatile (						\
			  #iname" %1, %0; "				\
			  : "=&q"(tmp_dst)				\
			  : "q"(*(uint16_t *)src), "0"(tmp_dst)		\
			  : "memory"					\
			  );						\
	    *(uint64_t *)dst = tmp_dst;					\
	}								\
    }




#define MAKE_2OUT_64_INST(iname) static inline void iname##64(addr_t * dst, addr_t * src) { \
	uint64_t tmp_dst = *(uint64_t *)dst;				\
	uint64_t tmp_src = *(uint64_t *)src;				\
									\
	asm volatile (							\
		      #iname"q %1, %0; "				\
		      : "=&q"(tmp_dst), "=&q"(tmp_src)			\
		      : "0"(tmp_dst), "1"(tmp_src)			\
		      : "memory"					\
		      );						\
	*(uint64_t *)src = tmp_src;					\
	*(uint64_t *)dst = tmp_dst;					\
    }

#define MAKE_2OUT_32_INST(iname) static inline void iname##32(addr_t * dst, addr_t * src) { \
	uint32_t tmp_dst = *(uint32_t *)dst;				\
	uint32_t tmp_src = *(uint32_t *)src;				\
									\
	asm volatile (							\
		      #iname"l %1, %0; "				\
		      : "=&q"(tmp_dst), "=&q"(tmp_src)			\
		      :  "0"(tmp_dst), "1"(tmp_src)			\
		      : "memory"					\
		      );						\
	*(uint32_t *)src = tmp_src;					\
	*(uint32_t *)dst = tmp_dst;					\
    }

#define MAKE_2OUT_16_INST(iname) static inline void iname##16(addr_t * dst, addr_t * src) { \
	uint16_t tmp_dst = *(uint16_t *)dst;				\
	uint16_t tmp_src = *(uint16_t *)src;				\
									\
	asm volatile (							\
		      #iname"w %1, %0; "				\
		      : "=&q"(tmp_dst), "=&q"(tmp_src)			\
		      : "0"(tmp_dst), "1"(tmp_src)			\
		      : "memory"					\
		      );						\
	*(uint16_t *)src = tmp_src;					\
	*(uint16_t *)dst = tmp_dst;					\
    }

#define MAKE_2OUT_8_INST(iname) static inline void iname##8(addr_t * dst, addr_t * src) { \
	uint8_t tmp_dst = *(uint8_t *)dst;				\
	uint8_t tmp_src = *(uint8_t *)src;				\
									\
	asm volatile (							\
		      #iname"b %1, %0; "				\
		      : "=&q"(tmp_dst), "=&q"(tmp_src)			\
		      : "0"(tmp_dst), "1"(tmp_src)			\
		      : "memory"					\
		      );						\
	*(uint8_t *)src = tmp_src;					\
	*(uint8_t *)dst = tmp_dst;					\
    }






/****************************/
/* 8 Bit instruction forms  */
/****************************/

MAKE_2OP_8FLAGS_INST(adc);
MAKE_2OP_8FLAGS_INST(add);
MAKE_2OP_8FLAGS_INST(and);
MAKE_2OP_8FLAGS_INST(or);
MAKE_2OP_8FLAGS_INST(xor);
MAKE_2OP_8FLAGS_INST(sub);


MAKE_1OP_8FLAGS_INST(inc);
MAKE_1OP_8FLAGS_INST(dec);
MAKE_1OP_8FLAGS_INST(neg);
MAKE_1OP_8FLAGS_INST(setb);
MAKE_1OP_8FLAGS_INST(setbe);
MAKE_1OP_8FLAGS_INST(setl);
MAKE_1OP_8FLAGS_INST(setle);
MAKE_1OP_8FLAGS_INST(setnb);
MAKE_1OP_8FLAGS_INST(setnbe);
MAKE_1OP_8FLAGS_INST(setnl);
MAKE_1OP_8FLAGS_INST(setnle);
MAKE_1OP_8FLAGS_INST(setno);
MAKE_1OP_8FLAGS_INST(setnp);
MAKE_1OP_8FLAGS_INST(setns);
MAKE_1OP_8FLAGS_INST(setnz);
MAKE_1OP_8FLAGS_INST(seto);
MAKE_1OP_8FLAGS_INST(setp);
MAKE_1OP_8FLAGS_INST(sets);
MAKE_1OP_8FLAGS_INST(setz);


MAKE_1OP_8_INST(not);

MAKE_2OP_8_INST(mov);
MAKE_2OP_8EXT_INST(movzx);
MAKE_2OP_8EXT_INST(movsx);

MAKE_2OUT_8_INST(xchg);

MAKE_2OP_8STR_INST(movs);
MAKE_1OP_8STR_INST(stos);
MAKE_1OP_8STR_INST(scas);


/****************************/
/* 16 Bit instruction forms */
/****************************/
MAKE_2OP_16FLAGS_INST(adc);
MAKE_2OP_16FLAGS_INST(add);
MAKE_2OP_16FLAGS_INST(and);
MAKE_2OP_16FLAGS_INST(or);
MAKE_2OP_16FLAGS_INST(xor);
MAKE_2OP_16FLAGS_INST(sub);


MAKE_1OP_16FLAGS_INST(inc);
MAKE_1OP_16FLAGS_INST(dec);
MAKE_1OP_16FLAGS_INST(neg);

MAKE_1OP_16_INST(not);

MAKE_2OP_16_INST(mov);
MAKE_2OP_16EXT_INST(movzx);
MAKE_2OP_16EXT_INST(movsx);
MAKE_2OUT_16_INST(xchg);

MAKE_2OP_16STR_INST(movs);
MAKE_1OP_16STR_INST(stos);
MAKE_1OP_16STR_INST(scas);


/****************************/
/* 32 Bit instruction forms */
/****************************/
MAKE_2OP_32FLAGS_INST(adc);
MAKE_2OP_32FLAGS_INST(add);
MAKE_2OP_32FLAGS_INST(and);
MAKE_2OP_32FLAGS_INST(or);
MAKE_2OP_32FLAGS_INST(xor);
MAKE_2OP_32FLAGS_INST(sub);


MAKE_1OP_32FLAGS_INST(inc);
MAKE_1OP_32FLAGS_INST(dec);
MAKE_1OP_32FLAGS_INST(neg);

MAKE_1OP_32_INST(not);

MAKE_2OP_32_INST(mov);

MAKE_2OUT_32_INST(xchg);



MAKE_2OP_32STR_INST(movs);
MAKE_1OP_32STR_INST(stos);
MAKE_1OP_32STR_INST(scas);



#ifdef __V3_64BIT__

/****************************/
/* 64 Bit instruction forms */
/****************************/
MAKE_2OP_64FLAGS_INST(adc);
MAKE_2OP_64FLAGS_INST(add);
MAKE_2OP_64FLAGS_INST(and);
MAKE_2OP_64FLAGS_INST(or);
MAKE_2OP_64FLAGS_INST(xor);
MAKE_2OP_64FLAGS_INST(sub);

MAKE_1OP_64FLAGS_INST(inc);
MAKE_1OP_64FLAGS_INST(dec);
MAKE_1OP_64FLAGS_INST(neg);

MAKE_1OP_64_INST(not);


MAKE_2OP_64_INST(mov);
MAKE_2OP_64STR_INST(movs);
MAKE_1OP_64STR_INST(stos);
MAKE_1OP_64STR_INST(scas);

MAKE_2OUT_64_INST(xchg);


#endif
