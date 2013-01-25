/* +------------------------------------------------------------------------+
   | quix86                                                                 |
   +------------------------------------------------------------------------+
   | This file is part of quix86, an x86-64 instruction decoder.            |
   |                                                                        |
   | Copyright (C) 2011 Institute for System Programming of Russian Academy |
   | of Sciences.                                                           |
   |                                                                        |
   | Contact e-mail: <unicluster@ispras.ru>.                                |
   |                                                                        |
   | quix86 is free software: you can redistribute it and/or modify it      |
   | under the terms of the GNU Lesser General Public License as published  |
   | by the Free Software Foundation, either version 3 of the License, or   |
   | (at your option) any later version.                                    |
   |                                                                        |
   | quix86 is distributed in the hope that it will be useful, but WITHOUT  |
   | ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or  |
   | FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public   |
   | License for more details.                                              |
   |                                                                        |
   | You should have received a copy of the GNU Lesser General Public       |
   | License along with quix86. If not, see <http://www.gnu.org/licenses/>. |
   +------------------------------------------------------------------------+ */

#ifndef QUIX86_H
#define QUIX86_H

/**
 * Major <tt>quix86</tt> version number.
 *
 * \author                              icee
 * \since                               1.1
 */
#define QX86_MAJOR_VERSION              1

/**
 * Minor <tt>quix86</tt> version number.
 *
 * \author                              icee
 * \since                               1.1
 */
#define QX86_MINOR_VERSION              1

/* Provide definitions for INT8..INT64 and UINT8..UINT64.  */
#ifdef _MSC_VER
    /* Definitions for INT8..INT64.  */
#   define QX86_INT8                    __int8
#   define QX86_INT16                   __int16
#   define QX86_INT32                   __int32
#   define QX86_INT64                   __int64

    /* Definitions for UINT8..UINT64.  */
#   define QX86_UINT8                   unsigned __int8
#   define QX86_UINT16                  unsigned __int16
#   define QX86_UINT32                  unsigned __int32
#   define QX86_UINT64                  unsigned __int64
#else
    /* No built-in types.  See if we have one of the standard headers.  */
#   if defined(HAVE_INTTYPES_H) || defined(HAVE_STDINT_H)
        /* Prefer <stdint.h> as it's somewhat smaller.  */
#       ifdef HAVE_STDINT_H
            /* Include <stdint.h>.  */
#           include <stdint.h>
#       else
            /* Include <inttypes.h> instead.  */
#           include <inttypes.h>
#       endif

        /* Definitions for INT8..INT64.  */
#       define QX86_INT8                int8_t
#       define QX86_INT16               int16_t
#       define QX86_INT32               int32_t
#       define QX86_INT64               int64_t

        /* Definitions for UINT8..UINT64.  */
#       define QX86_UINT8               uint8_t
#       define QX86_UINT16              uint16_t
#       define QX86_UINT32              uint32_t
#       define QX86_UINT64              uint64_t
#   else
        /* Likely definitions for INT8..INT64.  */
#       define QX86_INT8                signed char
#       define QX86_INT16               short
#       define QX86_INT32               int
#       define QX86_INT64               long long

        /* Likely definitions for UINT8..UINT64.  */
#       define QX86_UINT8               unsigned char
#       define QX86_UINT16              unsigned short
#       define QX86_UINT32              unsigned int
#       define QX86_UINT64              unsigned long long
#   endif
#endif

/* Provide wrappers around const and inline for compilers that don't support
   C99.  */
#ifdef _MSC_VER
    /* Microsoft Visual C is not C99-conformant.  Use alternative keywords.  */
#   define QX86_CONST                   const
#   define QX86_INLINE                  __inline
#   define QX86_RESTRICT                /* ILB */
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
    /* C99 supported.  */
#   define QX86_CONST                   const
#   define QX86_INLINE                  inline
#   define QX86_RESTRICT                restrict
#elif defined(__GNUC__) && (__GNUC__ >= 4)
    /* GNU C supported. */
#   define QX86_CONST                   const
#   define QX86_INLINE                  inline
#   define QX86_RESTRICT                restrict
#elif defined(__cplusplus)
    /* C++ mode supports const and inline.  */
#   define QX86_CONST                   const
#   define QX86_INLINE                  inline
#   define QX86_RESTRICT                /* ILB */
#else
    /* Assume none of the qualifiers is supported.  */
#   define QX86_CONST                   /* ILB */
#   define QX86_INLINE                  /* ILB */
#   define QX86_RESTRICT                /* ILB */
#endif

/* Wrap declarations in extern "C" if needed.  */
#ifdef __cplusplus
    /* Need wrapper.  */
#   define QX86_EXTERN_C                extern "C"
#else
    /* No wrapper required.  */
#   define QX86_EXTERN_C                /* ILB */
#endif

/**
 * 8-bit signed integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_INT8                       qx86_int8;

/**
 * 16-bit signed integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_INT16                      qx86_int16;

/**
 * 32-bit signed integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_INT32                      qx86_int32;

/**
 * 64-bit signed integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_INT64                      qx86_int64;

/**
 * 8-bit unsigned integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_UINT8                      qx86_uint8;

/**
 * 16-bit unsigned integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_UINT16                     qx86_uint16;

/**
 * 32-bit unsigned integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_UINT32                     qx86_uint32;

/**
 * 64-bit unsigned integer type.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef QX86_UINT64                     qx86_uint64;

/* Public API structure declarations.  */
typedef struct qx86_amode               qx86_amode;
typedef struct qx86_ctx                 qx86_ctx;
typedef struct qx86_insn                qx86_insn;
typedef struct qx86_insn_attributes     qx86_insn_attributes;
typedef struct qx86_insn_modifiers      qx86_insn_modifiers;
typedef struct qx86_print_options_intel qx86_print_options_intel;
typedef struct qx86_mtab_item           qx86_mtab_item;
typedef struct qx86_opcode_map          qx86_opcode_map;
typedef struct qx86_opcode_map_item     qx86_opcode_map_item;
typedef struct qx86_operand             qx86_operand;
typedef struct qx86_operand_far_pointer qx86_operand_far_pointer;
typedef struct qx86_operand_form        qx86_operand_form;
typedef struct qx86_operand_form_amode  qx86_operand_form_amode;
typedef struct qx86_operand_form_rtuple qx86_operand_form_rtuple;
typedef struct qx86_operand_immediate   qx86_operand_immediate;
typedef struct qx86_operand_jump_offset qx86_operand_jump_offset;
typedef struct qx86_operand_memory      qx86_operand_memory;
typedef struct qx86_operand_register    qx86_operand_register;
typedef struct qx86_print_item          qx86_print_item;
typedef struct qx86_rtab_item           qx86_rtab_item;
typedef struct qx86_rtuple              qx86_rtuple;
typedef struct qx86_stuple              qx86_stuple;

/* Public API union declarations.  */
typedef union qx86_operand_union        qx86_operand_union;
typedef union qx86_operand_form_union   qx86_operand_form_union;

/* Public API enumerations.  */

/**
 * Enumeration of <em>x86</em> condition codes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_CC_O                           = 0,
    QX86_CC_NO                          = 1,
    QX86_CC_B                           = 2,
    QX86_CC_AE                          = 3,
    QX86_CC_Z                           = 4,
    QX86_CC_NZ                          = 5,
    QX86_CC_BE                          = 6,
    QX86_CC_A                           = 7,
    QX86_CC_S                           = 8,
    QX86_CC_NS                          = 9,
    QX86_CC_P                           = 10,
    QX86_CC_NP                          = 11,
    QX86_CC_L                           = 12,
    QX86_CC_GE                          = 13,
    QX86_CC_LE                          = 14,
    QX86_CC_G                           = 15,
    
    QX86_CC_NONE                        = 16,

    QX86_CC_CXZ                         = 17,
    QX86_CC_ECXZ                        = 18,
    QX86_CC_RCXZ                        = 19,
    QX86_CC_CXO                         = 20,
    QX86_CC_ECXO                        = 21,
    QX86_CC_RCXO                        = 22
};

/**
 * Enumeration of <em>x86</em> instruction defects.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_DEFECT_NONE                    = 0,

    QX86_DEFECT_MODRM_MOD_NOT_3         = 1 << 0,
    QX86_DEFECT_MODRM_MOD_3             = 1 << 1
};

/**
 * Enumeration of <em>x86</em> displacement sizes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_DISP_NONE                      = 0,
    QX86_DISP_8                         = 1,
    QX86_DISP_16                        = 2,
    QX86_DISP_32                        = 4,
    QX86_DISP_64                        = 8,

    QX86_DISP_INVALID                   = 3
};

/**
 * Enumeration of <tt>quix86</tt> error codes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_SUCCESS                        = 0,

    QX86_E_INTERNAL                     = 1,
    QX86_E_API                          = 2,

    QX86_E_INSN_INCOMPLETE              = 3,
    QX86_E_INSN_UNDEFINED               = 4,

    QX86_E_INSUFFICIENT_BUFFER          = 5,
    
    QX86_E_CALLBACK                     = 6,

    QX86_E_COUNT                        = 7
};

/**
 * Enumeration of instruction classes.
 *
 * An instruction can belong to multiple instruction classes at the same time.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_ICLASS_NONE                    = 0,

    QX86_ICLASS_CONDITIONAL_EXECUTION   = 1 << 0,

    QX86_ICLASS_TRANSFER                = 1 << 1,
    QX86_ICLASS_TRANSFER_LINKED         = 1 << 2,
    QX86_ICLASS_TRANSFER_LINKED_BACK    = 1 << 3,
    QX86_ICLASS_TRANSFER_SERVICE        = 1 << 4
};

/**
 * Architectural limits of the <em>x86</em>.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_IMMEDIATE_SIZE_MAX             = 8,
    QX86_INSN_SIZE_MAX                  = 15,
    QX86_OPERAND_NMAX                   = 4,
    QX86_IMPLICIT_OPERAND_NMAX          = 8
};

/**
 * Enumeration of <em>x86</em> instruction mnemonics.
 *
 * \author                              icee
 * \since                               1.0
 */
enum qx86_mnemonic
{
    QX86_MNEMONIC_NONE                  = 0,

    /* Enumerators are sorted based on their names.  */
    QX86_MNEMONIC_AAA                   = 1,
    QX86_MNEMONIC_AAD                   = 2,
    QX86_MNEMONIC_AAM                   = 3,
    QX86_MNEMONIC_AAS                   = 4,
    QX86_MNEMONIC_ADC                   = 5,
    QX86_MNEMONIC_ADD                   = 6,
    QX86_MNEMONIC_ADDPD                 = 7,
    QX86_MNEMONIC_ADDPS                 = 8,
    QX86_MNEMONIC_ADDSD                 = 9,
    QX86_MNEMONIC_ADDSS                 = 10,
    QX86_MNEMONIC_ADDSUBPD              = 11,
    QX86_MNEMONIC_ADDSUBPS              = 12,
    QX86_MNEMONIC_AESDEC                = 13,
    QX86_MNEMONIC_AESDECLAST            = 14,
    QX86_MNEMONIC_AESENC                = 15,
    QX86_MNEMONIC_AESENCLAST            = 16,
    QX86_MNEMONIC_AESIMC                = 17,
    QX86_MNEMONIC_AESKEYGENASSIST       = 18,
    QX86_MNEMONIC_AND                   = 19,
    QX86_MNEMONIC_ANDNPD                = 20,
    QX86_MNEMONIC_ANDNPS                = 21,
    QX86_MNEMONIC_ANDPD                 = 22,
    QX86_MNEMONIC_ANDPS                 = 23,
    QX86_MNEMONIC_ARPL                  = 24,
    QX86_MNEMONIC_BLENDPD               = 25,
    QX86_MNEMONIC_BLENDPS               = 26,
    QX86_MNEMONIC_BLENDVPD              = 27,
    QX86_MNEMONIC_BLENDVPS              = 28,
    QX86_MNEMONIC_BOUND                 = 29,
    QX86_MNEMONIC_BSF                   = 30,
    QX86_MNEMONIC_BSR                   = 31,
    QX86_MNEMONIC_BSWAP                 = 32,
    QX86_MNEMONIC_BT                    = 33,
    QX86_MNEMONIC_BTC                   = 34,
    QX86_MNEMONIC_BTR                   = 35,
    QX86_MNEMONIC_BTS                   = 36,
    QX86_MNEMONIC_CALL                  = 37,
    QX86_MNEMONIC_CALLF                 = 38,
    QX86_MNEMONIC_CBW                   = 39,
    QX86_MNEMONIC_CDQ                   = 40,
    QX86_MNEMONIC_CDQE                  = 41,
    QX86_MNEMONIC_CLC                   = 42,
    QX86_MNEMONIC_CLD                   = 43,
    QX86_MNEMONIC_CLFLUSH               = 44,
    QX86_MNEMONIC_CLGI                  = 45,
    QX86_MNEMONIC_CLI                   = 46,
    QX86_MNEMONIC_CLTS                  = 47,
    QX86_MNEMONIC_CMC                   = 48,
    QX86_MNEMONIC_CMOVA                 = 49,
    QX86_MNEMONIC_CMOVAE                = 50,
    QX86_MNEMONIC_CMOVB                 = 51,
    QX86_MNEMONIC_CMOVBE                = 52,
    QX86_MNEMONIC_CMOVG                 = 53,
    QX86_MNEMONIC_CMOVGE                = 54,
    QX86_MNEMONIC_CMOVL                 = 55,
    QX86_MNEMONIC_CMOVLE                = 56,
    QX86_MNEMONIC_CMOVNO                = 57,
    QX86_MNEMONIC_CMOVNP                = 58,
    QX86_MNEMONIC_CMOVNS                = 59,
    QX86_MNEMONIC_CMOVNZ                = 60,
    QX86_MNEMONIC_CMOVO                 = 61,
    QX86_MNEMONIC_CMOVP                 = 62,
    QX86_MNEMONIC_CMOVS                 = 63,
    QX86_MNEMONIC_CMOVZ                 = 64,
    QX86_MNEMONIC_CMP                   = 65,
    QX86_MNEMONIC_CMPPD                 = 66,
    QX86_MNEMONIC_CMPPS                 = 67,
    QX86_MNEMONIC_CMPSB                 = 68,
    QX86_MNEMONIC_CMPSD                 = 69,
    QX86_MNEMONIC_CMPSD_SSE             = 70,
    QX86_MNEMONIC_CMPSQ                 = 71,
    QX86_MNEMONIC_CMPSS                 = 72,
    QX86_MNEMONIC_CMPSW                 = 73,
    QX86_MNEMONIC_CMPXCHG               = 74,
    QX86_MNEMONIC_CMPXCHG16B            = 75,
    QX86_MNEMONIC_CMPXCHG8B             = 76,
    QX86_MNEMONIC_COMISD                = 77,
    QX86_MNEMONIC_COMISS                = 78,
    QX86_MNEMONIC_CPUID                 = 79,
    QX86_MNEMONIC_CQO                   = 80,
    QX86_MNEMONIC_CRC32                 = 81,
    QX86_MNEMONIC_CVTDQ2PD              = 82,
    QX86_MNEMONIC_CVTDQ2PS              = 83,
    QX86_MNEMONIC_CVTPD2DQ              = 84,
    QX86_MNEMONIC_CVTPD2PI              = 85,
    QX86_MNEMONIC_CVTPD2PS              = 86,
    QX86_MNEMONIC_CVTPI2PD              = 87,
    QX86_MNEMONIC_CVTPI2PS              = 88,
    QX86_MNEMONIC_CVTPS2DQ              = 89,
    QX86_MNEMONIC_CVTPS2PD              = 90,
    QX86_MNEMONIC_CVTPS2PI              = 91,
    QX86_MNEMONIC_CVTSD2SI              = 92,
    QX86_MNEMONIC_CVTSD2SS              = 93,
    QX86_MNEMONIC_CVTSI2SD              = 94,
    QX86_MNEMONIC_CVTSI2SS              = 95,
    QX86_MNEMONIC_CVTSS2SD              = 96,
    QX86_MNEMONIC_CVTSS2SI              = 97,
    QX86_MNEMONIC_CVTTPD2DQ             = 98,
    QX86_MNEMONIC_CVTTPD2PI             = 99,
    QX86_MNEMONIC_CVTTPS2DQ             = 100,
    QX86_MNEMONIC_CVTTPS2PI             = 101,
    QX86_MNEMONIC_CVTTSD2SI             = 102,
    QX86_MNEMONIC_CVTTSS2SI             = 103,
    QX86_MNEMONIC_CWD                   = 104,
    QX86_MNEMONIC_CWDE                  = 105,
    QX86_MNEMONIC_DAA                   = 106,
    QX86_MNEMONIC_DAS                   = 107,
    QX86_MNEMONIC_DEC                   = 108,
    QX86_MNEMONIC_DIV                   = 109,
    QX86_MNEMONIC_DIVPD                 = 110,
    QX86_MNEMONIC_DIVPS                 = 111,
    QX86_MNEMONIC_DIVSD                 = 112,
    QX86_MNEMONIC_DIVSS                 = 113,
    QX86_MNEMONIC_DPPD                  = 114,
    QX86_MNEMONIC_DPPS                  = 115,
    QX86_MNEMONIC_EMMS                  = 116,
    QX86_MNEMONIC_ENTER                 = 117,
    QX86_MNEMONIC_EXTRACTPS             = 118,
    QX86_MNEMONIC_EXTRQ                 = 119,
    QX86_MNEMONIC_F2XM1                 = 120,
    QX86_MNEMONIC_FABS                  = 121,
    QX86_MNEMONIC_FADD                  = 122,
    QX86_MNEMONIC_FADDP                 = 123,
    QX86_MNEMONIC_FBLD                  = 124,
    QX86_MNEMONIC_FBSTP                 = 125,
    QX86_MNEMONIC_FCHS                  = 126,
    QX86_MNEMONIC_FCMOVB                = 127,
    QX86_MNEMONIC_FCMOVBE               = 128,
    QX86_MNEMONIC_FCMOVE                = 129,
    QX86_MNEMONIC_FCMOVNB               = 130,
    QX86_MNEMONIC_FCMOVNBE              = 131,
    QX86_MNEMONIC_FCMOVNE               = 132,
    QX86_MNEMONIC_FCMOVNU               = 133,
    QX86_MNEMONIC_FCMOVU                = 134,
    QX86_MNEMONIC_FCOM                  = 135,
    QX86_MNEMONIC_FCOMI                 = 136,
    QX86_MNEMONIC_FCOMIP                = 137,
    QX86_MNEMONIC_FCOMP                 = 138,
    QX86_MNEMONIC_FCOMPP                = 139,
    QX86_MNEMONIC_FCOS                  = 140,
    QX86_MNEMONIC_FDECSTP               = 141,
    QX86_MNEMONIC_FDIV                  = 142,
    QX86_MNEMONIC_FDIVP                 = 143,
    QX86_MNEMONIC_FDIVR                 = 144,
    QX86_MNEMONIC_FDIVRP                = 145,
    QX86_MNEMONIC_FEMMS                 = 146,
    QX86_MNEMONIC_FFREE                 = 147,
    QX86_MNEMONIC_FFREEP                = 148,
    QX86_MNEMONIC_FIADD                 = 149,
    QX86_MNEMONIC_FICOM                 = 150,
    QX86_MNEMONIC_FICOMP                = 151,
    QX86_MNEMONIC_FIDIV                 = 152,
    QX86_MNEMONIC_FIDIVR                = 153,
    QX86_MNEMONIC_FILD                  = 154,
    QX86_MNEMONIC_FIMUL                 = 155,
    QX86_MNEMONIC_FINCSTP               = 156,
    QX86_MNEMONIC_FIST                  = 157,
    QX86_MNEMONIC_FISTP                 = 158,
    QX86_MNEMONIC_FISTTP                = 159,
    QX86_MNEMONIC_FISUB                 = 160,
    QX86_MNEMONIC_FISUBR                = 161,
    QX86_MNEMONIC_FLD                   = 162,
    QX86_MNEMONIC_FLD1                  = 163,
    QX86_MNEMONIC_FLDCW                 = 164,
    QX86_MNEMONIC_FLDENV                = 165,
    QX86_MNEMONIC_FLDL2E                = 166,
    QX86_MNEMONIC_FLDL2T                = 167,
    QX86_MNEMONIC_FLDLG2                = 168,
    QX86_MNEMONIC_FLDLN2                = 169,
    QX86_MNEMONIC_FLDPI                 = 170,
    QX86_MNEMONIC_FLDZ                  = 171,
    QX86_MNEMONIC_FMUL                  = 172,
    QX86_MNEMONIC_FMULP                 = 173,
    QX86_MNEMONIC_FNCLEX                = 174,
    QX86_MNEMONIC_FNINIT                = 175,
    QX86_MNEMONIC_FNOP                  = 176,
    QX86_MNEMONIC_FNSAVE                = 177,
    QX86_MNEMONIC_FNSTCW                = 178,
    QX86_MNEMONIC_FNSTENV               = 179,
    QX86_MNEMONIC_FNSTSW                = 180,
    QX86_MNEMONIC_FPATAN                = 181,
    QX86_MNEMONIC_FPREM                 = 182,
    QX86_MNEMONIC_FPREM1                = 183,
    QX86_MNEMONIC_FPTAN                 = 184,
    QX86_MNEMONIC_FRNDINT               = 185,
    QX86_MNEMONIC_FRSTOR                = 186,
    QX86_MNEMONIC_FSCALE                = 187,
    QX86_MNEMONIC_FSIN                  = 188,
    QX86_MNEMONIC_FSINCOS               = 189,
    QX86_MNEMONIC_FSQRT                 = 190,
    QX86_MNEMONIC_FST                   = 191,
    QX86_MNEMONIC_FSTP                  = 192,
    QX86_MNEMONIC_FSUB                  = 193,
    QX86_MNEMONIC_FSUBP                 = 194,
    QX86_MNEMONIC_FSUBR                 = 195,
    QX86_MNEMONIC_FSUBRP                = 196,
    QX86_MNEMONIC_FTST                  = 197,
    QX86_MNEMONIC_FUCOM                 = 198,
    QX86_MNEMONIC_FUCOMI                = 199,
    QX86_MNEMONIC_FUCOMIP               = 200,
    QX86_MNEMONIC_FUCOMP                = 201,
    QX86_MNEMONIC_FUCOMPP               = 202,
    QX86_MNEMONIC_FWAIT                 = 203,
    QX86_MNEMONIC_FXAM                  = 204,
    QX86_MNEMONIC_FXCH                  = 205,
    QX86_MNEMONIC_FXRSTOR               = 206,
    QX86_MNEMONIC_FXSAVE                = 207,
    QX86_MNEMONIC_FXTRACT               = 208,
    QX86_MNEMONIC_FYL2X                 = 209,
    QX86_MNEMONIC_FYL2XP1               = 210,
    QX86_MNEMONIC_GETSEC                = 211,
    QX86_MNEMONIC_HADDPD                = 212,
    QX86_MNEMONIC_HADDPS                = 213,
    QX86_MNEMONIC_HLT                   = 214,
    QX86_MNEMONIC_HSUBPD                = 215,
    QX86_MNEMONIC_HSUBPS                = 216,
    QX86_MNEMONIC_IDIV                  = 217,
    QX86_MNEMONIC_IMUL                  = 218,
    QX86_MNEMONIC_IN                    = 219,
    QX86_MNEMONIC_INC                   = 220,
    QX86_MNEMONIC_INSB                  = 221,
    QX86_MNEMONIC_INSD                  = 222,
    QX86_MNEMONIC_INSERTPS              = 223,
    QX86_MNEMONIC_INSERTQ               = 224,
    QX86_MNEMONIC_INSW                  = 225,
    QX86_MNEMONIC_INT                   = 226,
    QX86_MNEMONIC_INT1                  = 227,
    QX86_MNEMONIC_INT3                  = 228,
    QX86_MNEMONIC_INTO                  = 229,
    QX86_MNEMONIC_INVD                  = 230,
    QX86_MNEMONIC_INVEPT                = 231,
    QX86_MNEMONIC_INVLPG                = 232,
    QX86_MNEMONIC_INVLPGA               = 233,
    QX86_MNEMONIC_INVPCID               = 234,
    QX86_MNEMONIC_INVVPID               = 235,
    QX86_MNEMONIC_IRET                  = 236,
    QX86_MNEMONIC_IRETD                 = 237,
    QX86_MNEMONIC_IRETQ                 = 238,
    QX86_MNEMONIC_JA                    = 239,
    QX86_MNEMONIC_JAE                   = 240,
    QX86_MNEMONIC_JB                    = 241,
    QX86_MNEMONIC_JBE                   = 242,
    QX86_MNEMONIC_JCXZ                  = 243,
    QX86_MNEMONIC_JECXZ                 = 244,
    QX86_MNEMONIC_JG                    = 245,
    QX86_MNEMONIC_JGE                   = 246,
    QX86_MNEMONIC_JL                    = 247,
    QX86_MNEMONIC_JLE                   = 248,
    QX86_MNEMONIC_JMP                   = 249,
    QX86_MNEMONIC_JMPF                  = 250,
    QX86_MNEMONIC_JNO                   = 251,
    QX86_MNEMONIC_JNP                   = 252,
    QX86_MNEMONIC_JNS                   = 253,
    QX86_MNEMONIC_JNZ                   = 254,
    QX86_MNEMONIC_JO                    = 255,
    QX86_MNEMONIC_JP                    = 256,
    QX86_MNEMONIC_JRCXZ                 = 257,
    QX86_MNEMONIC_JS                    = 258,
    QX86_MNEMONIC_JZ                    = 259,
    QX86_MNEMONIC_LAHF                  = 260,
    QX86_MNEMONIC_LAR                   = 261,
    QX86_MNEMONIC_LDDQU                 = 262,
    QX86_MNEMONIC_LDMXCSR               = 263,
    QX86_MNEMONIC_LDS                   = 264,
    QX86_MNEMONIC_LEA                   = 265,
    QX86_MNEMONIC_LEAVE                 = 266,
    QX86_MNEMONIC_LES                   = 267,
    QX86_MNEMONIC_LFENCE                = 268,
    QX86_MNEMONIC_LFS                   = 269,
    QX86_MNEMONIC_LGDT                  = 270,
    QX86_MNEMONIC_LGS                   = 271,
    QX86_MNEMONIC_LIDT                  = 272,
    QX86_MNEMONIC_LLDT                  = 273,
    QX86_MNEMONIC_LMSW                  = 274,
    QX86_MNEMONIC_LODSB                 = 275,
    QX86_MNEMONIC_LODSD                 = 276,
    QX86_MNEMONIC_LODSQ                 = 277,
    QX86_MNEMONIC_LODSW                 = 278,
    QX86_MNEMONIC_LOOP                  = 279,
    QX86_MNEMONIC_LOOPNZ                = 280,
    QX86_MNEMONIC_LOOPZ                 = 281,
    QX86_MNEMONIC_LSL                   = 282,
    QX86_MNEMONIC_LSS                   = 283,
    QX86_MNEMONIC_LTR                   = 284,
    QX86_MNEMONIC_LZCNT                 = 285,
    QX86_MNEMONIC_MASKMOVDQU            = 286,
    QX86_MNEMONIC_MASKMOVQ              = 287,
    QX86_MNEMONIC_MAXPD                 = 288,
    QX86_MNEMONIC_MAXPS                 = 289,
    QX86_MNEMONIC_MAXSD                 = 290,
    QX86_MNEMONIC_MAXSS                 = 291,
    QX86_MNEMONIC_MFENCE                = 292,
    QX86_MNEMONIC_MINPD                 = 293,
    QX86_MNEMONIC_MINPS                 = 294,
    QX86_MNEMONIC_MINSD                 = 295,
    QX86_MNEMONIC_MINSS                 = 296,
    QX86_MNEMONIC_MONITOR               = 297,
    QX86_MNEMONIC_MOV                   = 298,
    QX86_MNEMONIC_MOVAPD                = 299,
    QX86_MNEMONIC_MOVAPS                = 300,
    QX86_MNEMONIC_MOVBE                 = 301,
    QX86_MNEMONIC_MOVD                  = 302,
    QX86_MNEMONIC_MOVDDUP               = 303,
    QX86_MNEMONIC_MOVDQ2Q               = 304,
    QX86_MNEMONIC_MOVDQA                = 305,
    QX86_MNEMONIC_MOVDQU                = 306,
    QX86_MNEMONIC_MOVHLPS               = 307,
    QX86_MNEMONIC_MOVHPD                = 308,
    QX86_MNEMONIC_MOVHPS                = 309,
    QX86_MNEMONIC_MOVLHPS               = 310,
    QX86_MNEMONIC_MOVLPD                = 311,
    QX86_MNEMONIC_MOVLPS                = 312,
    QX86_MNEMONIC_MOVMSKPD              = 313,
    QX86_MNEMONIC_MOVMSKPS              = 314,
    QX86_MNEMONIC_MOVNTDQ               = 315,
    QX86_MNEMONIC_MOVNTDQA              = 316,
    QX86_MNEMONIC_MOVNTI                = 317,
    QX86_MNEMONIC_MOVNTPD               = 318,
    QX86_MNEMONIC_MOVNTPS               = 319,
    QX86_MNEMONIC_MOVNTQ                = 320,
    QX86_MNEMONIC_MOVNTSD               = 321,
    QX86_MNEMONIC_MOVNTSS               = 322,
    QX86_MNEMONIC_MOVQ                  = 323,
    QX86_MNEMONIC_MOVQ2DQ               = 324,
    QX86_MNEMONIC_MOVSB                 = 325,
    QX86_MNEMONIC_MOVSD                 = 326,
    QX86_MNEMONIC_MOVSD_SSE             = 327,
    QX86_MNEMONIC_MOVSHDUP              = 328,
    QX86_MNEMONIC_MOVSLDUP              = 329,
    QX86_MNEMONIC_MOVSQ                 = 330,
    QX86_MNEMONIC_MOVSS                 = 331,
    QX86_MNEMONIC_MOVSW                 = 332,
    QX86_MNEMONIC_MOVSX                 = 333,
    QX86_MNEMONIC_MOVSXD                = 334,
    QX86_MNEMONIC_MOVUPD                = 335,
    QX86_MNEMONIC_MOVUPS                = 336,
    QX86_MNEMONIC_MOVZX                 = 337,
    QX86_MNEMONIC_MPSADBW               = 338,
    QX86_MNEMONIC_MUL                   = 339,
    QX86_MNEMONIC_MULPD                 = 340,
    QX86_MNEMONIC_MULPS                 = 341,
    QX86_MNEMONIC_MULSD                 = 342,
    QX86_MNEMONIC_MULSS                 = 343,
    QX86_MNEMONIC_MWAIT                 = 344,
    QX86_MNEMONIC_NEG                   = 345,
    QX86_MNEMONIC_NOP                   = 346,
    QX86_MNEMONIC_NOT                   = 347,
    QX86_MNEMONIC_OR                    = 348,
    QX86_MNEMONIC_ORPD                  = 349,
    QX86_MNEMONIC_ORPS                  = 350,
    QX86_MNEMONIC_OUT                   = 351,
    QX86_MNEMONIC_OUTSB                 = 352,
    QX86_MNEMONIC_OUTSD                 = 353,
    QX86_MNEMONIC_OUTSW                 = 354,
    QX86_MNEMONIC_PABSB                 = 355,
    QX86_MNEMONIC_PABSD                 = 356,
    QX86_MNEMONIC_PABSW                 = 357,
    QX86_MNEMONIC_PACKSSDW              = 358,
    QX86_MNEMONIC_PACKSSWB              = 359,
    QX86_MNEMONIC_PACKUSDW              = 360,
    QX86_MNEMONIC_PACKUSWB              = 361,
    QX86_MNEMONIC_PADDB                 = 362,
    QX86_MNEMONIC_PADDD                 = 363,
    QX86_MNEMONIC_PADDQ                 = 364,
    QX86_MNEMONIC_PADDSB                = 365,
    QX86_MNEMONIC_PADDSW                = 366,
    QX86_MNEMONIC_PADDUSB               = 367,
    QX86_MNEMONIC_PADDUSW               = 368,
    QX86_MNEMONIC_PADDW                 = 369,
    QX86_MNEMONIC_PALIGNR               = 370,
    QX86_MNEMONIC_PAND                  = 371,
    QX86_MNEMONIC_PANDN                 = 372,
    QX86_MNEMONIC_PAUSE                 = 373,
    QX86_MNEMONIC_PAVGB                 = 374,
    QX86_MNEMONIC_PAVGUSB               = 375,
    QX86_MNEMONIC_PAVGW                 = 376,
    QX86_MNEMONIC_PBLENDVB              = 377,
    QX86_MNEMONIC_PBLENDW               = 378,
    QX86_MNEMONIC_PCLMULQDQ             = 379,
    QX86_MNEMONIC_PCMPEQB               = 380,
    QX86_MNEMONIC_PCMPEQD               = 381,
    QX86_MNEMONIC_PCMPEQQ               = 382,
    QX86_MNEMONIC_PCMPEQW               = 383,
    QX86_MNEMONIC_PCMPESTRI             = 384,
    QX86_MNEMONIC_PCMPESTRM             = 385,
    QX86_MNEMONIC_PCMPGTB               = 386,
    QX86_MNEMONIC_PCMPGTD               = 387,
    QX86_MNEMONIC_PCMPGTQ               = 388,
    QX86_MNEMONIC_PCMPGTW               = 389,
    QX86_MNEMONIC_PCMPISTRI             = 390,
    QX86_MNEMONIC_PCMPISTRM             = 391,
    QX86_MNEMONIC_PEXTRB                = 392,
    QX86_MNEMONIC_PEXTRD                = 393,
    QX86_MNEMONIC_PEXTRQ                = 394,
    QX86_MNEMONIC_PEXTRW                = 395,
    QX86_MNEMONIC_PF2ID                 = 396,
    QX86_MNEMONIC_PF2IW                 = 397,
    QX86_MNEMONIC_PFACC                 = 398,
    QX86_MNEMONIC_PFADD                 = 399,
    QX86_MNEMONIC_PFCMPEQ               = 400,
    QX86_MNEMONIC_PFCMPGE               = 401,
    QX86_MNEMONIC_PFCMPGT               = 402,
    QX86_MNEMONIC_PFMAX                 = 403,
    QX86_MNEMONIC_PFMIN                 = 404,
    QX86_MNEMONIC_PFMUL                 = 405,
    QX86_MNEMONIC_PFNACC                = 406,
    QX86_MNEMONIC_PFPNACC               = 407,
    QX86_MNEMONIC_PFRCP                 = 408,
    QX86_MNEMONIC_PFRCPIT1              = 409,
    QX86_MNEMONIC_PFRCPIT2              = 410,
    QX86_MNEMONIC_PFRSQIT1              = 411,
    QX86_MNEMONIC_PFRSQRT               = 412,
    QX86_MNEMONIC_PFSUB                 = 413,
    QX86_MNEMONIC_PFSUBR                = 414,
    QX86_MNEMONIC_PHADDD                = 415,
    QX86_MNEMONIC_PHADDSW               = 416,
    QX86_MNEMONIC_PHADDW                = 417,
    QX86_MNEMONIC_PHMINPOSUW            = 418,
    QX86_MNEMONIC_PHSUBD                = 419,
    QX86_MNEMONIC_PHSUBSW               = 420,
    QX86_MNEMONIC_PHSUBW                = 421,
    QX86_MNEMONIC_PI2FD                 = 422,
    QX86_MNEMONIC_PI2FW                 = 423,
    QX86_MNEMONIC_PINSRB                = 424,
    QX86_MNEMONIC_PINSRD                = 425,
    QX86_MNEMONIC_PINSRQ                = 426,
    QX86_MNEMONIC_PINSRW                = 427,
    QX86_MNEMONIC_PMADDUBSW             = 428,
    QX86_MNEMONIC_PMADDWD               = 429,
    QX86_MNEMONIC_PMAXSB                = 430,
    QX86_MNEMONIC_PMAXSD                = 431,
    QX86_MNEMONIC_PMAXSW                = 432,
    QX86_MNEMONIC_PMAXUB                = 433,
    QX86_MNEMONIC_PMAXUD                = 434,
    QX86_MNEMONIC_PMAXUW                = 435,
    QX86_MNEMONIC_PMINSB                = 436,
    QX86_MNEMONIC_PMINSD                = 437,
    QX86_MNEMONIC_PMINSW                = 438,
    QX86_MNEMONIC_PMINUB                = 439,
    QX86_MNEMONIC_PMINUD                = 440,
    QX86_MNEMONIC_PMINUW                = 441,
    QX86_MNEMONIC_PMOVMSKB              = 442,
    QX86_MNEMONIC_PMOVSXBD              = 443,
    QX86_MNEMONIC_PMOVSXBQ              = 444,
    QX86_MNEMONIC_PMOVSXBW              = 445,
    QX86_MNEMONIC_PMOVSXDQ              = 446,
    QX86_MNEMONIC_PMOVSXWD              = 447,
    QX86_MNEMONIC_PMOVSXWQ              = 448,
    QX86_MNEMONIC_PMOVZXBD              = 449,
    QX86_MNEMONIC_PMOVZXBQ              = 450,
    QX86_MNEMONIC_PMOVZXBW              = 451,
    QX86_MNEMONIC_PMOVZXDQ              = 452,
    QX86_MNEMONIC_PMOVZXWD              = 453,
    QX86_MNEMONIC_PMOVZXWQ              = 454,
    QX86_MNEMONIC_PMULDQ                = 455,
    QX86_MNEMONIC_PMULHRSW              = 456,
    QX86_MNEMONIC_PMULHRW               = 457,
    QX86_MNEMONIC_PMULHUW               = 458,
    QX86_MNEMONIC_PMULHW                = 459,
    QX86_MNEMONIC_PMULLD                = 460,
    QX86_MNEMONIC_PMULLW                = 461,
    QX86_MNEMONIC_PMULUDQ               = 462,
    QX86_MNEMONIC_POP                   = 463,
    QX86_MNEMONIC_POPA                  = 464,
    QX86_MNEMONIC_POPAD                 = 465,
    QX86_MNEMONIC_POPCNT                = 466,
    QX86_MNEMONIC_POPF                  = 467,
    QX86_MNEMONIC_POPFD                 = 468,
    QX86_MNEMONIC_POPFQ                 = 469,
    QX86_MNEMONIC_POR                   = 470,
    QX86_MNEMONIC_PREFETCH              = 471,
    QX86_MNEMONIC_PREFETCHNTA           = 472,
    QX86_MNEMONIC_PREFETCHT0            = 473,
    QX86_MNEMONIC_PREFETCHT1            = 474,
    QX86_MNEMONIC_PREFETCHT2            = 475,
    QX86_MNEMONIC_PREFETCHW             = 476,
    QX86_MNEMONIC_PSADBW                = 477,
    QX86_MNEMONIC_PSHUFB                = 478,
    QX86_MNEMONIC_PSHUFD                = 479,
    QX86_MNEMONIC_PSHUFHW               = 480,
    QX86_MNEMONIC_PSHUFLW               = 481,
    QX86_MNEMONIC_PSHUFW                = 482,
    QX86_MNEMONIC_PSIGNB                = 483,
    QX86_MNEMONIC_PSIGND                = 484,
    QX86_MNEMONIC_PSIGNW                = 485,
    QX86_MNEMONIC_PSLLD                 = 486,
    QX86_MNEMONIC_PSLLDQ                = 487,
    QX86_MNEMONIC_PSLLQ                 = 488,
    QX86_MNEMONIC_PSLLW                 = 489,
    QX86_MNEMONIC_PSRAD                 = 490,
    QX86_MNEMONIC_PSRAW                 = 491,
    QX86_MNEMONIC_PSRLD                 = 492,
    QX86_MNEMONIC_PSRLDQ                = 493,
    QX86_MNEMONIC_PSRLQ                 = 494,
    QX86_MNEMONIC_PSRLW                 = 495,
    QX86_MNEMONIC_PSUBB                 = 496,
    QX86_MNEMONIC_PSUBD                 = 497,
    QX86_MNEMONIC_PSUBQ                 = 498,
    QX86_MNEMONIC_PSUBSB                = 499,
    QX86_MNEMONIC_PSUBSW                = 500,
    QX86_MNEMONIC_PSUBUSB               = 501,
    QX86_MNEMONIC_PSUBUSW               = 502,
    QX86_MNEMONIC_PSUBW                 = 503,
    QX86_MNEMONIC_PSWAPD                = 504,
    QX86_MNEMONIC_PTEST                 = 505,
    QX86_MNEMONIC_PUNPCKHBW             = 506,
    QX86_MNEMONIC_PUNPCKHDQ             = 507,
    QX86_MNEMONIC_PUNPCKHQDQ            = 508,
    QX86_MNEMONIC_PUNPCKHWD             = 509,
    QX86_MNEMONIC_PUNPCKLBW             = 510,
    QX86_MNEMONIC_PUNPCKLDQ             = 511,
    QX86_MNEMONIC_PUNPCKLQDQ            = 512,
    QX86_MNEMONIC_PUNPCKLWD             = 513,
    QX86_MNEMONIC_PUSH                  = 514,
    QX86_MNEMONIC_PUSHA                 = 515,
    QX86_MNEMONIC_PUSHAD                = 516,
    QX86_MNEMONIC_PUSHF                 = 517,
    QX86_MNEMONIC_PUSHFD                = 518,
    QX86_MNEMONIC_PUSHFQ                = 519,
    QX86_MNEMONIC_PXOR                  = 520,
    QX86_MNEMONIC_RCL                   = 521,
    QX86_MNEMONIC_RCPPS                 = 522,
    QX86_MNEMONIC_RCPSS                 = 523,
    QX86_MNEMONIC_RCR                   = 524,
    QX86_MNEMONIC_RDFSBASE              = 525,
    QX86_MNEMONIC_RDGSBASE              = 526,
    QX86_MNEMONIC_RDMSR                 = 527,
    QX86_MNEMONIC_RDPMC                 = 528,
    QX86_MNEMONIC_RDTSC                 = 529,
    QX86_MNEMONIC_RDTSCP                = 530,
    QX86_MNEMONIC_RET                   = 531,
    QX86_MNEMONIC_RETF                  = 532,
    QX86_MNEMONIC_ROL                   = 533,
    QX86_MNEMONIC_ROR                   = 534,
    QX86_MNEMONIC_ROUNDPD               = 535,
    QX86_MNEMONIC_ROUNDPS               = 536,
    QX86_MNEMONIC_ROUNDSD               = 537,
    QX86_MNEMONIC_ROUNDSS               = 538,
    QX86_MNEMONIC_RSM                   = 539,
    QX86_MNEMONIC_RSQRTPS               = 540,
    QX86_MNEMONIC_RSQRTSS               = 541,
    QX86_MNEMONIC_SAHF                  = 542,
    QX86_MNEMONIC_SALC                  = 543,
    QX86_MNEMONIC_SAR                   = 544,
    QX86_MNEMONIC_SBB                   = 545,
    QX86_MNEMONIC_SCASB                 = 546,
    QX86_MNEMONIC_SCASD                 = 547,
    QX86_MNEMONIC_SCASQ                 = 548,
    QX86_MNEMONIC_SCASW                 = 549,
    QX86_MNEMONIC_SETA                  = 550,
    QX86_MNEMONIC_SETAE                 = 551,
    QX86_MNEMONIC_SETB                  = 552,
    QX86_MNEMONIC_SETBE                 = 553,
    QX86_MNEMONIC_SETG                  = 554,
    QX86_MNEMONIC_SETGE                 = 555,
    QX86_MNEMONIC_SETL                  = 556,
    QX86_MNEMONIC_SETLE                 = 557,
    QX86_MNEMONIC_SETNO                 = 558,
    QX86_MNEMONIC_SETNP                 = 559,
    QX86_MNEMONIC_SETNS                 = 560,
    QX86_MNEMONIC_SETNZ                 = 561,
    QX86_MNEMONIC_SETO                  = 562,
    QX86_MNEMONIC_SETP                  = 563,
    QX86_MNEMONIC_SETS                  = 564,
    QX86_MNEMONIC_SETZ                  = 565,
    QX86_MNEMONIC_SFENCE                = 566,
    QX86_MNEMONIC_SGDT                  = 567,
    QX86_MNEMONIC_SHL                   = 568,
    QX86_MNEMONIC_SHLD                  = 569,
    QX86_MNEMONIC_SHR                   = 570,
    QX86_MNEMONIC_SHRD                  = 571,
    QX86_MNEMONIC_SHUFPD                = 572,
    QX86_MNEMONIC_SHUFPS                = 573,
    QX86_MNEMONIC_SIDT                  = 574,
    QX86_MNEMONIC_SKINIT                = 575,
    QX86_MNEMONIC_SLDT                  = 576,
    QX86_MNEMONIC_SMSW                  = 577,
    QX86_MNEMONIC_SQRTPD                = 578,
    QX86_MNEMONIC_SQRTPS                = 579,
    QX86_MNEMONIC_SQRTSD                = 580,
    QX86_MNEMONIC_SQRTSS                = 581,
    QX86_MNEMONIC_STC                   = 582,
    QX86_MNEMONIC_STD                   = 583,
    QX86_MNEMONIC_STGI                  = 584,
    QX86_MNEMONIC_STI                   = 585,
    QX86_MNEMONIC_STMXCSR               = 586,
    QX86_MNEMONIC_STOSB                 = 587,
    QX86_MNEMONIC_STOSD                 = 588,
    QX86_MNEMONIC_STOSQ                 = 589,
    QX86_MNEMONIC_STOSW                 = 590,
    QX86_MNEMONIC_STR                   = 591,
    QX86_MNEMONIC_SUB                   = 592,
    QX86_MNEMONIC_SUBPD                 = 593,
    QX86_MNEMONIC_SUBPS                 = 594,
    QX86_MNEMONIC_SUBSD                 = 595,
    QX86_MNEMONIC_SUBSS                 = 596,
    QX86_MNEMONIC_SWAPGS                = 597,
    QX86_MNEMONIC_SYSCALL               = 598,
    QX86_MNEMONIC_SYSENTER              = 599,
    QX86_MNEMONIC_SYSEXIT               = 600,
    QX86_MNEMONIC_SYSRET                = 601,
    QX86_MNEMONIC_TEST                  = 602,
    QX86_MNEMONIC_TZCNT                 = 603,
    QX86_MNEMONIC_UCOMISD               = 604,
    QX86_MNEMONIC_UCOMISS               = 605,
    QX86_MNEMONIC_UD1                   = 606,
    QX86_MNEMONIC_UD2                   = 607,
    QX86_MNEMONIC_UNPCKHPD              = 608,
    QX86_MNEMONIC_UNPCKHPS              = 609,
    QX86_MNEMONIC_UNPCKLPD              = 610,
    QX86_MNEMONIC_UNPCKLPS              = 611,
    QX86_MNEMONIC_VERR                  = 612,
    QX86_MNEMONIC_VERW                  = 613,
    QX86_MNEMONIC_VMCALL                = 614,
    QX86_MNEMONIC_VMCLEAR               = 615,
    QX86_MNEMONIC_VMFUNC                = 616,
    QX86_MNEMONIC_VMLAUNCH              = 617,
    QX86_MNEMONIC_VMLOAD                = 618,
    QX86_MNEMONIC_VMMCALL               = 619,
    QX86_MNEMONIC_VMPTRLD               = 620,
    QX86_MNEMONIC_VMPTRST               = 621,
    QX86_MNEMONIC_VMREAD                = 622,
    QX86_MNEMONIC_VMRESUME              = 623,
    QX86_MNEMONIC_VMRUN                 = 624,
    QX86_MNEMONIC_VMSAVE                = 625,
    QX86_MNEMONIC_VMWRITE               = 626,
    QX86_MNEMONIC_VMXOFF                = 627,
    QX86_MNEMONIC_VMXON                 = 628,
    QX86_MNEMONIC_WBINVD                = 629,
    QX86_MNEMONIC_WRFSBASE              = 630,
    QX86_MNEMONIC_WRGSBASE              = 631,
    QX86_MNEMONIC_WRMSR                 = 632,
    QX86_MNEMONIC_XABORT                = 633,
    QX86_MNEMONIC_XADD                  = 634,
    QX86_MNEMONIC_XBEGIN                = 635,
    QX86_MNEMONIC_XCHG                  = 636,
    QX86_MNEMONIC_XEND                  = 637,
    QX86_MNEMONIC_XGETBV                = 638,
    QX86_MNEMONIC_XLAT                  = 639,
    QX86_MNEMONIC_XOR                   = 640,
    QX86_MNEMONIC_XORPD                 = 641,
    QX86_MNEMONIC_XORPS                 = 642,
    QX86_MNEMONIC_XRSTOR                = 643,
    QX86_MNEMONIC_XSAVE                 = 644,
    QX86_MNEMONIC_XSAVEOPT              = 645,
    QX86_MNEMONIC_XSETBV                = 646,
    QX86_MNEMONIC_XTEST                 = 647,

    QX86_MNEMONIC_COUNT                 = 648
};

/**
 * Enumeration of mnemonic attributes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_MATTRIBUTE_NONE                = 0,
    QX86_MATTRIBUTE_REP                 = 1 << 0,
    QX86_MATTRIBUTE_REPZ                = 1 << 1,
    QX86_MATTRIBUTE_DEFAULT_SIZE_64     = 1 << 2,
    QX86_MATTRIBUTE_FIXED_SIZE_64       = 1 << 3,
    QX86_MATTRIBUTE_INTERLOCKABLE       = 1 << 4,
    QX86_MATTRIBUTE_IMPLICIT_LOCK       = 1 << 5
};

/**
 * Enumeration of ModRM fields.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_MODRM_FIELD_NONE               = 0,

    QX86_MODRM_FIELD_MOD                = 1 << 0,
    QX86_MODRM_FIELD_REG                = 1 << 2,
    QX86_MODRM_FIELD_RM                 = 1 << 3
};

/**
 * Enumeration of <em>x86</em> opcode escapes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_OPCODE_ESCAPE_NONE             = 0,

    QX86_OPCODE_ESCAPE_0F               = 1,
    QX86_OPCODE_ESCAPE_0F_38            = 2,
    QX86_OPCODE_ESCAPE_0F_3A            = 3,

    QX86_OPCODE_ESCAPE_COUNT            = 4
};

/**
 * Enumeration of opcode map indexes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_OPCODE_MAP_INDEX_NONE          = 0,

    QX86_OPCODE_MAP_INDEX_NB            = 1,
    QX86_OPCODE_MAP_INDEX_PB            = 2,

    QX86_OPCODE_MAP_INDEX_AS            = 3,
    QX86_OPCODE_MAP_INDEX_CS            = 4,
    QX86_OPCODE_MAP_INDEX_OS            = 5,

    QX86_OPCODE_MAP_INDEX_SP            = 6,

    QX86_OPCODE_MAP_INDEX_MOD           = 7,
    QX86_OPCODE_MAP_INDEX_REG           = 8,
    QX86_OPCODE_MAP_INDEX_RM            = 9,

    QX86_OPCODE_MAP_INDEX_COUNT         = 10
};

/**
 * Enumeration of opcode map item codes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_OPCODE_MAP_ITEM_CODE_NONE      = -0,
    QX86_OPCODE_MAP_ITEM_CODE_LINK      = -1,
    QX86_OPCODE_MAP_ITEM_CODE_PREFIX    = -2
};

/**
 * Enumeration of <em>x86</em> opcode extension prefixes.
 *
 * \author                              icee
 * \since                               1.1
 */
enum
{
    QX86_OPCODE_PREFIX_NONE             = 0,

    QX86_OPCODE_PREFIX_66               = 1,
    QX86_OPCODE_PREFIX_F2               = 2,
    QX86_OPCODE_PREFIX_F3               = 3,

    QX86_OPCODE_PREFIX_COUNT            = 4
};

/**
 * Enumeration of <em>x86</em> operand attributes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_OPERAND_ATTRIBUTE_NONE         = 0,

    QX86_OPERAND_ATTRIBUTE_READ         = 1,
    QX86_OPERAND_ATTRIBUTE_WRITTEN      = 2,
    QX86_OPERAND_ATTRIBUTE_READWRITTEN  = 3,

    QX86_OPERAND_ATTRIBUTE_RW_CERTAIN   = 4
};

/**
 * Enumeration of <em>x86</em> operand form types.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_OPERAND_FORM_TYPE_NONE         = 0,

    QX86_OPERAND_FORM_TYPE_AMODE        = 1,
    QX86_OPERAND_FORM_TYPE_IMPLICIT_1   = 2,
    QX86_OPERAND_FORM_TYPE_RTUPLE       = 3,

    QX86_OPERAND_FORM_COUNT             = 4
};

/**
 * Enumeration of <em>x86</em> operand types.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_OPERAND_TYPE_NONE              = 0,

    QX86_OPERAND_TYPE_FAR_POINTER       = 1,
    QX86_OPERAND_TYPE_IMMEDIATE         = 2,
    QX86_OPERAND_TYPE_JUMP_OFFSET       = 3,
    QX86_OPERAND_TYPE_MEMORY            = 4,
    QX86_OPERAND_TYPE_REGISTER          = 5,

    QX86_OPERAND_TYPE_COUNT             = 6
};

/**
 * Enumeration of <em>x86</em> register classes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_RCLASS_NONE                    = 0,
    QX86_RCLASS_IP                      = 1,
    QX86_RCLASS_FLAGS                   = 2,
    QX86_RCLASS_RESERVED_3              = 3,

    QX86_RCLASS_REG8                    = 4,
    QX86_RCLASS_REG16                   = 5,
    QX86_RCLASS_REG32                   = 6,
    QX86_RCLASS_REG64                   = 7,

    QX86_RCLASS_CREG                    = 8,
    QX86_RCLASS_DREG                    = 9,
    QX86_RCLASS_SREG                    = 10,
    QX86_RCLASS_TREG                    = 11,

    QX86_RCLASS_X87                     = 12,
    QX86_RCLASS_MMX                     = 13,
    QX86_RCLASS_XMM                     = 14,
    QX86_RCLASS_YMM                     = 15,

    QX86_RCLASS_COUNT                   = 16
};

/**
 * Enumeration of <em>x86</em> registers.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_REGISTER_NONE                  = 0,
    QX86_REGISTER_INVALID               = 1,
    QX86_REGISTER_SPECIAL               = 2,
    QX86_REGISTER_RESERVED_3            = 3,

    QX86_REGISTER_RESERVED_4            = 4,
    QX86_REGISTER_IP                    = 5,
    QX86_REGISTER_EIP                   = 6,
    QX86_REGISTER_RIP                   = 7,

    QX86_REGISTER_RESERVED_8            = 8,
    QX86_REGISTER_FLAGS                 = 9,
    QX86_REGISTER_EFLAGS                = 10,
    QX86_REGISTER_RFLAGS                = 11,

    QX86_REGISTER_AH                    = 12,
    QX86_REGISTER_CH                    = 13,
    QX86_REGISTER_DH                    = 14,
    QX86_REGISTER_BH                    = 15,

    QX86_REGISTER_AL                    = 16,
    QX86_REGISTER_CL                    = 17,
    QX86_REGISTER_DL                    = 18,
    QX86_REGISTER_BL                    = 19,
    QX86_REGISTER_SPL                   = 20,
    QX86_REGISTER_BPL                   = 21,
    QX86_REGISTER_SIL                   = 22,
    QX86_REGISTER_DIL                   = 23,
    QX86_REGISTER_R8B                   = 24,
    QX86_REGISTER_R9B                   = 25,
    QX86_REGISTER_R10B                  = 26,
    QX86_REGISTER_R11B                  = 27,
    QX86_REGISTER_R12B                  = 28,
    QX86_REGISTER_R13B                  = 29,
    QX86_REGISTER_R14B                  = 30,
    QX86_REGISTER_R15B                  = 31,

    QX86_REGISTER_AX                    = 32,
    QX86_REGISTER_CX                    = 33,
    QX86_REGISTER_DX                    = 34,
    QX86_REGISTER_BX                    = 35,
    QX86_REGISTER_SP                    = 36,
    QX86_REGISTER_BP                    = 37,
    QX86_REGISTER_SI                    = 38,
    QX86_REGISTER_DI                    = 39,
    QX86_REGISTER_R8W                   = 40,
    QX86_REGISTER_R9W                   = 41,
    QX86_REGISTER_R10W                  = 42,
    QX86_REGISTER_R11W                  = 43,
    QX86_REGISTER_R12W                  = 44,
    QX86_REGISTER_R13W                  = 45,
    QX86_REGISTER_R14W                  = 46,
    QX86_REGISTER_R15W                  = 47,

    QX86_REGISTER_EAX                   = 48,
    QX86_REGISTER_ECX                   = 49,
    QX86_REGISTER_EDX                   = 50,
    QX86_REGISTER_EBX                   = 51,
    QX86_REGISTER_ESP                   = 52,
    QX86_REGISTER_EBP                   = 53,
    QX86_REGISTER_ESI                   = 54,
    QX86_REGISTER_EDI                   = 55,
    QX86_REGISTER_R8D                   = 56,
    QX86_REGISTER_R9D                   = 57,
    QX86_REGISTER_R10D                  = 58,
    QX86_REGISTER_R11D                  = 59,
    QX86_REGISTER_R12D                  = 60,
    QX86_REGISTER_R13D                  = 61,
    QX86_REGISTER_R14D                  = 62,
    QX86_REGISTER_R15D                  = 63,

    QX86_REGISTER_RAX                   = 64,
    QX86_REGISTER_RCX                   = 65,
    QX86_REGISTER_RDX                   = 66,
    QX86_REGISTER_RBX                   = 67,
    QX86_REGISTER_RSP                   = 68,
    QX86_REGISTER_RBP                   = 69,
    QX86_REGISTER_RSI                   = 70,
    QX86_REGISTER_RDI                   = 71,
    QX86_REGISTER_R8                    = 72,
    QX86_REGISTER_R9                    = 73,
    QX86_REGISTER_R10                   = 74,
    QX86_REGISTER_R11                   = 75,
    QX86_REGISTER_R12                   = 76,
    QX86_REGISTER_R13                   = 77,
    QX86_REGISTER_R14                   = 78,
    QX86_REGISTER_R15                   = 79,

    QX86_REGISTER_CR0                   = 80,
    QX86_REGISTER_CR1                   = 81,
    QX86_REGISTER_CR2                   = 82,
    QX86_REGISTER_CR3                   = 83,
    QX86_REGISTER_CR4                   = 84,
    QX86_REGISTER_CR5                   = 85,
    QX86_REGISTER_CR6                   = 86,
    QX86_REGISTER_CR7                   = 87,
    QX86_REGISTER_CR8                   = 88,
    QX86_REGISTER_CR9                   = 89,
    QX86_REGISTER_CR10                  = 90,
    QX86_REGISTER_CR11                  = 91,
    QX86_REGISTER_CR12                  = 92,
    QX86_REGISTER_CR13                  = 93,
    QX86_REGISTER_CR14                  = 94,
    QX86_REGISTER_CR15                  = 95,

    QX86_REGISTER_DR0                   = 96,
    QX86_REGISTER_DR1                   = 97,
    QX86_REGISTER_DR2                   = 98,
    QX86_REGISTER_DR3                   = 99,
    QX86_REGISTER_DR4                   = 100,
    QX86_REGISTER_DR5                   = 101,
    QX86_REGISTER_DR6                   = 102,
    QX86_REGISTER_DR7                   = 103,
    QX86_REGISTER_DR8                   = 104,
    QX86_REGISTER_DR9                   = 105,
    QX86_REGISTER_DR10                  = 106,
    QX86_REGISTER_DR11                  = 107,
    QX86_REGISTER_DR12                  = 108,
    QX86_REGISTER_DR13                  = 109,
    QX86_REGISTER_DR14                  = 110,
    QX86_REGISTER_DR15                  = 111,

    QX86_REGISTER_ES                    = 112,
    QX86_REGISTER_CS                    = 113,
    QX86_REGISTER_SS                    = 114,
    QX86_REGISTER_DS                    = 115,
    QX86_REGISTER_FS                    = 116,
    QX86_REGISTER_GS                    = 117,
    QX86_REGISTER_SR6                   = 118,
    QX86_REGISTER_SR7                   = 119,

    QX86_REGISTER_ST0                   = 120,
    QX86_REGISTER_ST1                   = 121,
    QX86_REGISTER_ST2                   = 122,
    QX86_REGISTER_ST3                   = 123,
    QX86_REGISTER_ST4                   = 124,
    QX86_REGISTER_ST5                   = 125,
    QX86_REGISTER_ST6                   = 126,
    QX86_REGISTER_ST7                   = 127,

    QX86_REGISTER_FPR0                  = 128,
    QX86_REGISTER_FPR1                  = 129,
    QX86_REGISTER_FPR2                  = 130,
    QX86_REGISTER_FPR3                  = 131,
    QX86_REGISTER_FPR4                  = 132,
    QX86_REGISTER_FPR5                  = 133,
    QX86_REGISTER_FPR6                  = 134,
    QX86_REGISTER_FPR7                  = 135,

    QX86_REGISTER_MMX0                  = 136,
    QX86_REGISTER_MMX1                  = 137,
    QX86_REGISTER_MMX2                  = 138,
    QX86_REGISTER_MMX3                  = 139,
    QX86_REGISTER_MMX4                  = 140,
    QX86_REGISTER_MMX5                  = 141,
    QX86_REGISTER_MMX6                  = 142,
    QX86_REGISTER_MMX7                  = 143,

    QX86_REGISTER_XMM0                  = 144,
    QX86_REGISTER_XMM1                  = 145,
    QX86_REGISTER_XMM2                  = 146,
    QX86_REGISTER_XMM3                  = 147,
    QX86_REGISTER_XMM4                  = 148,
    QX86_REGISTER_XMM5                  = 149,
    QX86_REGISTER_XMM6                  = 150,
    QX86_REGISTER_XMM7                  = 151,
    QX86_REGISTER_XMM8                  = 152,
    QX86_REGISTER_XMM9                  = 153,
    QX86_REGISTER_XMM10                 = 154,
    QX86_REGISTER_XMM11                 = 155,
    QX86_REGISTER_XMM12                 = 156,
    QX86_REGISTER_XMM13                 = 157,
    QX86_REGISTER_XMM14                 = 158,
    QX86_REGISTER_XMM15                 = 159,

    QX86_REGISTER_YMM0                  = 160,
    QX86_REGISTER_YMM1                  = 161,
    QX86_REGISTER_YMM2                  = 162,
    QX86_REGISTER_YMM3                  = 163,
    QX86_REGISTER_YMM4                  = 164,
    QX86_REGISTER_YMM5                  = 165,
    QX86_REGISTER_YMM6                  = 166,
    QX86_REGISTER_YMM7                  = 167,
    QX86_REGISTER_YMM8                  = 168,
    QX86_REGISTER_YMM9                  = 169,
    QX86_REGISTER_YMM10                 = 170,
    QX86_REGISTER_YMM11                 = 171,
    QX86_REGISTER_YMM12                 = 172,
    QX86_REGISTER_YMM13                 = 173,
    QX86_REGISTER_YMM14                 = 174,
    QX86_REGISTER_YMM15                 = 175,

    QX86_REGISTER_GDTR                  = 176,
    QX86_REGISTER_IDTR                  = 177,
    QX86_REGISTER_LDTR                  = 178,
    QX86_REGISTER_TR                    = 179,

    QX86_REGISTER_FCW                   = 180,
    QX86_REGISTER_FSW                   = 181,
    QX86_REGISTER_FTW                   = 182,
    QX86_REGISTER_MXCSR                 = 183,

    QX86_REGISTER_COUNT                 = 184
};

/**
 * Enumeration of ModRM and SIB <em>scale</em> values.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_SCALE_NONE                     = 0,
    QX86_SCALE_X2                       = 1,
    QX86_SCALE_X4                       = 2,
    QX86_SCALE_X8                       = 3,
    QX86_SCALE_INVALID                  = 4
};

/**
 * Enumeration of <em>x86</em> code, address, operand, and stack sizes.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_SIZE_16                        = 0,
    QX86_SIZE_32                        = 1,
    QX86_SIZE_64                        = 2,
    QX86_SIZE_INVALID                   = 3,
    QX86_SIZE_MASK                      = 3
};

/**
 * Enumeration of <em>x86</em> subregisters.
 *
 * \author                              icee
 * \since                               1.0
 */
enum
{
    QX86_SUBREG_NONE                    = 0,

    QX86_SUBREG_BASE                    = 1,
    QX86_SUBREG_LIMIT                   = 2,
    QX86_SUBREG_FLAGS                   = 3,

    QX86_SUBREG_COUNT                   = 4
};

/* Public API structures.  */

/**
 * Addressing mode definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_amode
{
    QX86_CONST char *                   referenceName;
    QX86_CONST char *                   name;

    qx86_uint8                          modrmField;
    qx86_uint8                          rclass;

    int                                 (*decodeFunc)(qx86_insn *, int);
};

/**
 * Callback function definition.
 *
 * \author                              icee
 * \since                               1.0
 */
typedef int                             (*qx86_callback)(void *data, int rindex, int subreg, unsigned char *value);

/**
 * Decode context structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_ctx
{
    qx86_uint8 *                        ptr;
    int                                 ptrSize;

    int                                 pumpIndex;
};

/**
 * Instruction attributes definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_insn_attributes
{
    qx86_uint8                          addressSize;
    qx86_uint8                          addressSizeOverridden;

    qx86_uint8                          operandSize;
    qx86_uint8                          operandSizeOverridden;

    qx86_uint8                          interlocked;
};

/**
 * Instruction modifiers definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_insn_modifiers
{
    qx86_uint8                          modrm;
    qx86_int8                           modrmIndex;

    qx86_uint8                          sib;
    qx86_int8                           sibIndex;

    qx86_uint8                          rex;
    qx86_int8                           rexIndex;

    qx86_uint8                          prefixSize;

    qx86_uint8                          escape;
    qx86_uint8                          opcodePrefix;

    /* XXX: values 0x00, 0xF2, 0xF3.  */
    qx86_uint8                          repeatPrefix;

    int                                 sriOverride;

    qx86_uint8                          extendedB;
    qx86_uint8                          extendedR;
    qx86_uint8                          extendedX;
};

/**
 * Far pointer instruction operand definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_far_pointer
{
    qx86_uint8                          offset[QX86_IMMEDIATE_SIZE_MAX];
    qx86_uint8                          offsetSize;

    qx86_uint8                          selector[2];
};

/**
 * Immediate instruction operand definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_immediate
{
    qx86_uint8                          value[QX86_IMMEDIATE_SIZE_MAX];
    qx86_uint8                          valueSize;

    qx86_uint8                          extended[QX86_IMMEDIATE_SIZE_MAX];
    qx86_uint8                          extendedSize;
};

/**
 * Jump offset instruction operand definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_jump_offset
{
    qx86_uint8                          offset[QX86_IMMEDIATE_SIZE_MAX];
    qx86_uint8                          offsetSize;
};

/**
 * Memory instruction operand definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_memory
{
    int                                 sri;
    int                                 bri;
    int                                 iri;

    int                                 scale;

    qx86_uint8                          disp[QX86_IMMEDIATE_SIZE_MAX];
    qx86_uint8                          dispSize;
};

/**
 * Register instruction operand definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_register
{
    int                                 rindex;
};

/**
 * Instruction operand definition union.
 *
 * \author                              icee
 * \since                               1.0
 */
union qx86_operand_union
{
    qx86_operand_far_pointer            f;
    qx86_operand_immediate              i;
    qx86_operand_jump_offset            j;
    qx86_operand_memory                 m;
    qx86_operand_register               r;
};

/**
 * Instruction operand definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand
{
    qx86_uint8                          ot;

    int                                 attributes;
    int                                 size;

    qx86_operand_union                  u;
};

/**
 * Instruction definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_insn
{
    qx86_uint8                          rawSize;
    qx86_uint8                          raw[QX86_INSN_SIZE_MAX];

    int                                 processorMode;

    int                                 mnemonic;

    qx86_int8                           implicitOperandCount;
    qx86_int8                           operandCount;

    qx86_operand                        implicitOperands[QX86_IMPLICIT_OPERAND_NMAX];
    qx86_operand                        operands[QX86_OPERAND_NMAX];

    qx86_operand_form *                 implicitOperandForms[QX86_IMPLICIT_OPERAND_NMAX];
    qx86_operand_form *                 operandForms[QX86_OPERAND_NMAX];

    qx86_insn_attributes                attributes;
    qx86_insn_modifiers                 modifiers;

    qx86_uint8                          iclass;
    qx86_uint8                          defects;

    qx86_callback                       callback;
    void *                              data;
};

/**
 * Mnemonic table item definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_mtab_item
{
    QX86_CONST char *                   referenceName;
    QX86_CONST char *                   name;

    qx86_uint8                          attributes;

    qx86_uint8                          iclass;
    qx86_uint8                          cc;

    int                                 demoted;
    int                                 promoted;
};

/**
 * Opcode map definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_opcode_map
{
    qx86_uint8                          index;
    qx86_uint8                          limit;

    qx86_opcode_map_item *              items;
};

/**
 * Addressing mode instruction operand form definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_form_amode
{
    qx86_amode *                        amode;
    qx86_stuple *                       stuple;
};

/**
 * Register tuple instruction operand form definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_form_rtuple
{
    qx86_rtuple *                       rtuple;
};

/**
 * Instruction operand form definition union.
 *
 * \author                              icee
 * \since                               1.0
 */
union qx86_operand_form_union
{
    QX86_CONST void *                   initializer[2];

    qx86_operand_form_amode             a;
    qx86_operand_form_rtuple            r;
};

/**
 * Instruction operand form definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_operand_form
{
    int                                 ft;
    int                                 attributes;
    qx86_operand_form_union             u;
};

/**
 * Opcode map item definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_opcode_map_item
{
    int                                 code;
    qx86_opcode_map *                   link;

    int                                 operandCount;
    qx86_operand_form                   operandForms[QX86_OPERAND_NMAX];
};

/**
 * Print item definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_print_item
{
    QX86_CONST qx86_uint8 *             number;
    int                                 numberSize;

    QX86_CONST char *                   string;
};

/**
 * Intel print options structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_print_options_intel
{
    int                                 flipCase : 1;
};

/**
 * Register table item definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_rtab_item
{
    QX86_CONST char *                   referenceName;
    QX86_CONST char *                   name;

    qx86_uint8                          rclass;
    qx86_uint8                          size;
};

/**
 * Register tuple definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_rtuple
{
    QX86_CONST char *                   referenceName;
    QX86_CONST char *                   name;

    int                                 rindexes[12];
};

/**
 * Size tuple definition structure.
 *
 * \author                              icee
 * \since                               1.0
 */
struct qx86_stuple
{
    QX86_CONST char *                   referenceName;
    QX86_CONST char *                   name;

    QX86_CONST char *                   atoms[4];
    int                                 sizes[4];
};

/**
 * Extract the <em>mod</em> ModRM field value.
 *
 * \param                               modrm
 *                                      ModRM octet value.
 *
 * \return                              ModRM <em>mod</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_MODRM_MOD(modrm)           ((qx86_uint8) ((modrm) >> 6))

/**
 * Extract the <em>reg</em> ModRM field value.
 *
 * \param                               modrm
 *                                      ModRM octet value.
 *
 * \return                              ModRM <em>reg</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_MODRM_REG(modrm)           ((qx86_uint8) (((modrm) >> 3) & 7))

/**
 * Extract the <em>r/m</em> ModRM field value.
 *
 * \param                               modrm
 *                                      ModRM octet value.
 *
 * \return                              ModRM <em>rm</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_MODRM_RM(modrm)            ((qx86_uint8) ((modrm) & 7))

/**
 * Extract the <em>b</em> REX field value.
 *
 * \param                               rex
 *                                      REX octet value.
 *
 * \return                              REX <em>b</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_REX_B(rex)                 ((qx86_uint8) (0 != ((rex) & 1)))

/**
 * Extract the <em>r</em> REX field value.
 *
 * \param                               rex
 *                                      REX octet value.
 *
 * \return                              REX <em>r</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_REX_R(rex)                 ((qx86_uint8) (0 != ((rex) & 4)))

/**
 * Extract the <em>w</em> REX field value.
 *
 * \param                               rex
 *                                      REX octet value.
 *
 * \return                              REX <em>w</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_REX_W(rex)                 ((qx86_uint8) (0 != ((rex) & 8)))

/**
 * Extract the <em>x</em> REX field value.
 *
 * \param                               rex
 *                                      REX octet value.
 *
 * \return                              REX <em>x</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_REX_X(rex)                 ((qx86_uint8) (0 != ((rex) & 2)))

/**
 * Extract the <em>base</em> SIB field value.
 *
 * \param                               sib
 *                                      SIB octet value.
 *
 * \return                              SIB <em>base</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_SIB_BASE(sib)              ((qx86_uint8) ((sib) & 7))

/**
 * Extract the <em>index</em> SIB field value.
 *
 * \param                               sib
 *                                      SIB octet value.
 *
 * \return                              SIB <em>index</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_SIB_INDEX(sib)             ((qx86_uint8) (((sib) >> 3) & 7))

/**
 * Extract the <em>scale</em> SIB field value.
 *
 * \param                               sib
 *                                      SIB octet value.
 *
 * \return                              SIB <em>scale</em> field value.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_SIB_SCALE(sib)             ((qx86_uint8) ((sib) >> 6))

/**
 * Convert a #qx86_size enumerator to number of octets.  The \a size value must
 * be valid.
 *
 * \param                               size
 *                                      A #qx86_size enumerator.
 *
 * \return                              Number of octets.
 *
 * \author                              icee
 * \since                               1.0
 */
#define QX86_SIZE_OCTETS(size)          (2 << (size))

/**
 * Calculate effective address of an <em>x86</em> memory operand.
 *
 * \param                               insn
 *                                      Instruction pointer.
 * \param                               operandIndex
 *                                      Index of operand to decode: 0 and up
 *                                      for explicit operands, negative values
 *                                      for implicit operands.
 * \param[out]                          address
 *                                      Effective address if successful.
 *
 * \return                              Status code.
 *
 * \author                              icee
 * \since                               1.0
 */
QX86_EXTERN_C int
qx86_calculate_effective_address(QX86_CONST qx86_insn *insn, int operandIndex, qx86_uint64 *address);

/**
 * Calculate linear address of an <em>x86</em> memory operand.
 *
 * \param                               insn
 *                                      Instruction pointer.
 * \param                               operandIndex
 *                                      Index of operand to decode: 0 and up
 *                                      for explicit operands, negative values
 *                                      for implicit operands.
 * \param[out]                          address
 *                                      Linear address if successful.
 *
 * \return                              Status code.
 *
 * \author                              icee
 * \since                               1.0
 */
QX86_EXTERN_C int
qx86_calculate_linear_address(QX86_CONST qx86_insn *insn, int operandIndex, qx86_uint64 *address);

/**
 * Decode an <em>x86</em> instruction.
 *
 * TODO: documentation.
 *
 * \author                              icee
 * \since                               1.0
 */
QX86_EXTERN_C int
qx86_decode(qx86_insn *insn, int processorMode, QX86_CONST void *ptr, int ptrSize);

/**
 * Get <em>x86</em> mnemonic information.
 *
 * \param                               mindex
 *                                      Mnemonic index, one of #qx86_mnemonic
 *                                      enumerators.
 *
 * \return                              Pointer to mnemonic structure; \c NULL
 *                                      if \a mindex is invalid.
 *
 * \author                              icee
 * \since                               1.1
 */
QX86_EXTERN_C QX86_CONST qx86_mtab_item *
qx86_minfo(int mindex);

/**
 * Rename an <em>x86</em> mnemonic.  This function changes name used in search
 * and print functions.
 *
 * The buffer pointed to by \a name is \b not copied and must remain valid
 * for the whole \c quix86 lifetime.
 *
 * Passing \c NULL as \a name resets mnemonic name to its default, reference
 * name.
 *
 * \param                               mindex
 *                                      Mnemonic index, one of #qx86_mnemonic
 *                                      enumerators.
 * \param                               name
 *                                      Name pointer.
 *
 * \author                              icee
 * \since                               1.1
 */
QX86_EXTERN_C void
qx86_minfo_rename(int rindex, QX86_CONST char *name);

/**
 * Print a decoded <em>x86</em> instruction using the Intel format.
 *
 * TODO: documentation.
 *
 * \param                               insn
 *                                      Instruction to print.
 * \param                               options
 *                                      Printer options.
 * \param[out]                          buffer
 *                                      Pre-allocated buffer to print to.
 * \param[in,out]                       bufferSize
 *                                      TODO.
 *
 * \return                              TODO.
 *
 * \author                              icee
 * \since                               1.0
 */
QX86_EXTERN_C int
qx86_print_intel(QX86_CONST qx86_insn *insn, QX86_CONST qx86_print_options_intel *options, char *buffer, int *bufferSize);

/**
 * Get <em>x86</em> register information.
 *
 * \param                               rindex
 *                                      Register index, one of #qx86_register
 *                                      enumerators.
 *
 * \return                              Pointer to register structure; \c NULL
 *                                      if \a rindex is invalid.
 *
 * \author                              icee
 * \since                               1.1
 */
QX86_EXTERN_C QX86_CONST qx86_rtab_item *
qx86_rinfo(int rindex);

/**
 * Rename an <em>x86</em> register.  This function changes name used in search
 * and print functions.
 *
 * The buffer pointed to by \a name is \b not copied and must remain valid
 * for the whole \c quix86 lifetime.
 *
 * Passing \c NULL as \a name resets register name to its default, reference
 * name.
 *
 * \param                               rindex
 *                                      Register index, one of #qx86_register
 *                                      enumerators.
 * \param                               name
 *                                      Name pointer.
 *
 * \author                              icee
 * \since                               1.1
 */
QX86_EXTERN_C void
qx86_rinfo_rename(int rindex, QX86_CONST char *name);

#endif
