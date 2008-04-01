#ifndef __SVM_CTRL_REGS_H
#define __SVM_CTRL_REGS_H

#include <geekos/vm_guest.h>
#include <geekos/vmm_util.h>


// First opcode byte
static const uchar_t cr_access_byte = 0x0f;

// Second opcode byte
static const uchar_t lmsw_byte = 0x01;
static const uchar_t lmsw_reg_byte = 0x6;
static const uchar_t smsw_byte = 0x01;
static const uchar_t smsw_reg_byte = 0x4;
static const uchar_t clts_byte = 0x06;
static const uchar_t mov_to_cr_byte = 0x22;
static const uchar_t mov_from_cr_byte = 0x20;



int handle_cr0_write(struct guest_info * info, ullong_t * new_cr0);




#endif
