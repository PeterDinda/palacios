#include <palacios/vmm_decoder.h>



/* The full blown instruction parser... */
int v3_parse_instr(struct guest_info * info,
		   char * instr_ptr,
		   uint_t * instr_length, 
		   addr_t * opcode,
		   uint_t * opcode_length,
		   struct x86_prefix_list * prefixes,
		   struct x86_operand * src_operand,
		   struct x86_operand * dst_operand,
		   struct x86_operand * extra_operand) {

  V3_ASSERT(src_operand != NULL);
  V3_ASSERT(dst_operand != NULL);
  V3_ASSERT(extra_operand != NULL);
  V3_ASSERT(instr_length != NULL);
  V3_ASSERT(info != NULL);

  
  // Ignore prefixes for now
  while (is_prefix_byte(*instr_ptr)) {
    instr_ptr++;
    *instr_length++;
  }


  // Opcode table lookup, see xen/kvm





  return 0;
}
