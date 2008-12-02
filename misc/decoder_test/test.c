#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "test.h"
#include "vmm_xed.h"
#include "vmm_decoder.h"
#include "vm_guest.h"


/* Disgusting mask hack...
   I can't think right now, so we'll do it this way...
*/
static const ullong_t mask_1 = 0x00000000000000ffLL;
static const ullong_t mask_2 = 0x000000000000ffffLL;
static const ullong_t mask_4 = 0x00000000ffffffffLL;
static const ullong_t mask_8 = 0xffffffffffffffffLL;


#define MASK(val, length) ({			\
      ullong_t mask = 0x0LL;			\
      switch (length) {				\
      case 1:					\
	mask = mask_1;				\
      case 2:					\
	mask = mask_2;				\
      case 4:					\
	mask = mask_4;				\
      case 8:					\
	mask = mask_8;				\
      }						\
      val & mask;})				\

static void init_guest_info(struct guest_info * info) {
  memset(info, 0, sizeof(struct guest_info));
  info->cpu_mode = PROTECTED;

  info->vm_regs.rax = 0x01010101;
  info->vm_regs.rbx = 0x02020202;
  info->vm_regs.rcx = 0x03030303;
  info->vm_regs.rdx = 0x04040404;

  info->vm_regs.rdi = 0x05050505;
  info->vm_regs.rsi = 0x06060606;
  info->vm_regs.rsp = 0x07070707;
  info->vm_regs.rbp = 0x08080808;

  info->vm_regs.rdi = 0x05050505;
  info->vm_regs.rsi = 0x06060606;
  info->vm_regs.rsp = 0x07070707;
  info->vm_regs.rbp = 0x08080808;


  info->segments.ds.base = 0xf0f0f0f0;
  info->segments.es.base = 0xe0e0e0e0;




}
static const char * mem = "MEMORY";
static const char * reg = "REGISTER";
static const char * imm = "IMMEDIATE";
static const char * invalid = "INVALID";

static const char * get_op_type_str(operand_type_t type) {
  if (type == MEM_OPERAND) {
    return mem;
  } else if (type == REG_OPERAND) {
    return reg;
  } else if (type == IMM_OPERAND) {
    return imm;
  } else {
    return invalid;
  }
}

static int print_op(struct x86_operand *op) {
  printf("\ttype=%s\n", get_op_type_str(op->type));

  switch (op->type) {
  case REG_OPERAND:
    printf("\tsize=%d\n", op->size);
    printf("\taddr=0x%x (val=%x)\n", op->operand, MASK(*(uint_t*)(op->operand), op->size));
    return 0;
  case MEM_OPERAND:
    printf("\tsize=%d\n", op->size);
    printf("\taddr=0x%x\n", op->operand);
    return 0;

  case IMM_OPERAND:
    printf("\tsize=%d\n", op->size);
    printf("\tval=0x%x\n", op->operand);
    return 0;

  default:
    return -1;
  }
}

int main(int argc, char ** argv) {
  char * filename;
  int fd;
  struct stat file_state;
  int ret;
  char * file_buf;
  int buf_offset = 0;
  int file_size = 0;
  char * instr_ptr = 0;
  
  struct guest_info * info = (struct guest_info *)malloc(sizeof(struct guest_info ));;

  init_decoder();
  init_guest_info(info);

  if (argc == 1) {
    printf("Error: Must give a binary file\n");
    exit(-1);
  }

  filename = argv[1];
  
  ret = stat(filename, &file_state); 

  if (ret == -1) {
    printf("Could not stat file\n");
    return -1;
  }
  file_size = file_state.st_size;

  file_buf = malloc(file_size);

  fd = open(filename, NULL);

  if (fd == -1) {
    printf("Could not open file\n");
    return -1;
  } else {
    int total_read = 0;
    int num_read = 0;
    
    while (total_read < file_size) {
      num_read = read(fd, file_buf + total_read, file_size - total_read);
      
      if (num_read == 0) {
	printf("end of file\n");
	break;
      }
      if (num_read == -1) {
	printf("Read error\n");
	exit(-1);
      }

      total_read += num_read;
    }

  }

  
  instr_ptr = file_buf;
 

  PrintV3CtrlRegs(info);
  PrintV3GPRs(info);
  PrintV3Segments(info);


  while (buf_offset < file_size) {
    struct x86_instr instr;

    if (v3_decode(info, (addr_t)instr_ptr + buf_offset, &instr) == -1) {
      printf("Unhandled instruction\n");
      buf_offset += instr.instr_length;
      continue;
    }
    printf("instr_length = %d, noperands=%d\n", instr.instr_length, instr.num_operands);

    printf("Source:\n");
    print_op(&(instr.src_operand));

    printf("Dest:\n");
    print_op(&(instr.dst_operand));


    printf("\n\n");

    buf_offset += instr.instr_length;
  }

  return 0;
}
