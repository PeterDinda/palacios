#include <palacios/vmm_types.h>



#define MAKE_1OP_8FLAGS_INST(iname) static inline void iname##8(addr_t * dst,  addr_t * flags) { \
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

#define MAKE_1OP_16FLAGS_INST(iname) static inline void iname##16(addr_t * dst,  addr_t * flags) { \
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

#define MAKE_1OP_32FLAGS_INST(iname) static inline void iname##32(addr_t * dst,  addr_t * flags) { \
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

#define MAKE_1OP_64FLAGS_INST(iname) static inline void iname##64(addr_t * dst,  addr_t * flags) { \
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



#define MAKE_1OP_8_INST(iname) static inline void iname##8(addr_t * dst) { \
    uchar_t tmp_dst = *dst;						\
									\
    asm volatile (							\
		  #iname"b %0; "					\
		  : "=q"(tmp_dst)					\
		  : "0"(tmp_dst)					\
		  );							\
    *dst = tmp_dst;							\
  }

#define MAKE_1OP_16_INST(iname) static inline void iname##16(addr_t * dst) { \
    ushort_t tmp_dst = *dst;						\
    									\
    asm volatile (							\
	 #iname"w %0; "							\
	 : "=q"(tmp_dst)						\
	 :  "0"(tmp_dst)						\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_1OP_32_INST(iname) static inline void iname##32(addr_t * dst) { \
    uint_t tmp_dst = *dst;						\
									\
    asm volatile (							\
	 #iname"l %0; "							\
	 : "=q"(tmp_dst)						\
	 : "0"(tmp_dst)							\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_1OP_64_INST(iname) static inline void iname##64(addr_t * dst) { \
    ullong_t tmp_dst = *dst;						\
    									\
    asm volatile (							\
		  #iname"q %0; "					\
		  : "=q"(tmp_dst)					\
		  : "0"(tmp_dst)					\
		  );							\
    *dst = tmp_dst;							\
  }


#define MAKE_2OP_64FLAGS_INST(iname) static inline void iname##64(addr_t * dst, addr_t * src, addr_t * flags) { \
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




#define MAKE_2OP_32FLAGS_INST(iname) static inline void iname##32(addr_t * dst, addr_t * src, addr_t * flags) { \
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


#define MAKE_2OP_16FLAGS_INST(iname) static inline void iname##16(addr_t * dst, addr_t * src, addr_t * flags) { \
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

#define MAKE_2OP_8FLAGS_INST(iname) static inline void iname##8(addr_t * dst, addr_t * src, addr_t * flags) { \
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




#define MAKE_2OP_32STR_INST(iname) static inline void iname##32(addr_t * dst, \
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
	 : "=b"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"b"(*flags)			\
	 );								\
									\
    /*	 : "=D"(*dst),"=S"(*src),"=c"(*ecx),"=q"(*flags)*/		\
    *flags |= flags_rsvd;						\
  }

#define MAKE_2OP_16STR_INST(iname) static inline void iname##16(addr_t * dst, \
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
	 : "=b"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"b"(*flags)			\
	 );								\
    *flags |= flags_rsvd;						\
  }



#define MAKE_2OP_8STR_INST(iname) static inline void iname##8(addr_t * dst, \
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
	 : "=b"(*flags)							\
	 : "D"(*dst),"S"(*src),"c"(*ecx),"b"(*flags)			\
	 );								\
    *flags |= flags_rsvd;						\
  }




#define MAKE_2OP_32_INST(iname) static inline void iname##32(addr_t * dst, addr_t * src) { \
    uint32_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"l %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_2OP_16_INST(iname) static inline void iname##16(addr_t * dst, addr_t * src) { \
    ushort_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"w %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }

#define MAKE_2OP_8_INST(iname) static inline void iname##8(addr_t * dst, addr_t * src) { \
    uchar_t tmp_dst = *dst, tmp_src = *src;				\
									\
    asm volatile (							\
	 #iname"b %1, %0; "						\
	 : "=q"(tmp_dst)						\
	 : "q"(tmp_src), "0"(tmp_dst)					\
	 );								\
    *dst = tmp_dst;							\
  }







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
MAKE_2OP_8_INST(xchg);



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
MAKE_2OP_16_INST(xchg);





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
MAKE_2OP_32_INST(xchg);

MAKE_2OP_8STR_INST(movs);
MAKE_2OP_16STR_INST(movs);
MAKE_2OP_32STR_INST(movs);
