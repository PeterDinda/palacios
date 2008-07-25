#include <palacios/vmm_decoder.h>


int opcode_cmp(const uchar_t * op1, const uchar_t * op2) {
  if (op1[0] != op2[0]) {
    return op1[0] - op2[0];;
  } else {
    return memcmp(op1 + 1, op2 + 1, op1[0]);
  }
}
