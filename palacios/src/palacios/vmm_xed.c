#include <palacios/vmm_decoder.h>
#include <palacios/vmm_xed.h>
#include <xed/xed-interface.h>
#include <palacios/vm_guest.h>

static xed_state_t decoder_state;


static int set_decoder_mode(struct guest_info * info, xed_state_t * state) {
  switch (info->cpu_mode) {
  case REAL:
    if (state->mmode != XED_MACHINE_MODE_LEGACY_16) {
      xed_state_init(state,
		     XED_MACHINE_MODE_LEGACY_16, 
		     XED_ADDRESS_WIDTH_16b, 
		     XED_ADDRESS_WIDTH_16b); 
    }
   break;
  case PROTECTED:
  case PROTECTED_PAE:
    if (state->mmode != XED_MACHINE_MODE_LEGACY_32) {
      xed_state_init(state,
		     XED_MACHINE_MODE_LEGACY_32, 
		     XED_ADDRESS_WIDTH_32b, 
		     XED_ADDRESS_WIDTH_32b);
    }
    break;
  case LONG:
    if (state->mmode != XED_MACHINE_MODE_LONG_64) {    
      state->mmode = XED_MACHINE_MODE_LONG_64;
    }
    break;
  default:
    return -1;
  }
  return 0;
}


int init_decoder() {
  xed_tables_init();
  xed_state_zero(&decoder_state);
  return 0;
}


int v3_decode(struct guest_info * info, addr_t instr_ptr, struct x86_instr * instr) {
  xed_decoded_inst_t xed_instr;
  xed_error_enum_t xed_error;

  if (set_decoder_mode(info, &decoder_state) == -1) {
    PrintError("Could not set decoder mode\n");
    return -1;
  }
  
  xed_decoded_inst_zero_set_mode(&xed_instr, &decoder_state);

  xed_error = xed_decode(&xed_instr, 
			 REINTERPRET_CAST(const xed_uint8_t *, instr_ptr), 
			 XED_MAX_INSTRUCTION_BYTES);
  

  if (xed_error != XED_ERROR_NONE) {
    PrintError("Xed error: %s\n", xed_error_enum_t2str(xed_error));
    return -1;
  }
  
  instr->instr_length = xed_decoded_inst_get_length (&xed_instr);
  
  
  PrintDebug("category: %s\n", xed_category_enum_t2str(xed_decoded_inst_get_category(&xed_instr)));;
  PrintDebug("ISA-extension:%s\n ",xed_extension_enum_t2str(xed_decoded_inst_get_extension(&xed_instr)));
  PrintDebug(" instruction-length: %d\n ", xed_decoded_inst_get_length(&xed_instr));
  PrintDebug(" operand-size:%d\n ", xed_operand_values_get_effective_operand_width(xed_decoded_inst_operands_const(&xed_instr)));   
  PrintDebug("address-size:%d\n ", xed_operand_values_get_effective_address_width(xed_decoded_inst_operands_const(&xed_instr))); 
  PrintDebug("iform-enum-name:%s\n ",xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xed_instr)));
  PrintDebug("iform-enum-name-dispatch (zero based):%d\n ", xed_decoded_inst_get_iform_enum_dispatch(&xed_instr));
  PrintDebug("iclass-max-iform-dispatch: %d\n ", xed_iform_max_per_iclass(xed_decoded_inst_get_iclass(&xed_instr)));
  
  // operands
  // print_operands(&xed_instr);
  
  // memops
  // print_memops(&xed_instr);
  
  // flags
  //print_flags(&xed_instr);
  
  // attributes
  //print_attributes(&xed_instr);*/



    return -1;
}


int v3_encode(struct guest_info * info, struct x86_instr * instr, char * instr_buf) {

  return -1;
}




/*

    xed_state_t dstate;
    xed_decoded_inst_t xedd;
    xed_uint_t i, length;
    xed_uint8_t itext[100] = {0x01,0x00,0x00,0x00,0x12,0x00,0x55,0x48,0x89,0xe5,0x48,0x89,0x7d,0xf8,0x89,0x75,0xf4,0x89,0x55,0xf0,0x89,0x4d,0xec,0x48,0x8b,0x55,0xf8,0x8b,0x45,0xf4,0x89,0x02,0x48,0x8b,0x55,0xf8,0x8b,0x45,0xf0,0x89,0x42,0x04,0x48,0x8b,0x55,0xf8,0x8b,0x45,0xec,0x89,0x42,0x08,0xc9,0xc3,0x55,0x48,0x89,0xe5,0x48,0x89,0x7d,0xf8,0x48,0x8b,0x45,0xf8,0x8b,0x40,0x08,0xc9,0xc3,0x90,0x0};
    xed_bool_t long_mode = true;
    unsigned int first_argv;
    int num;
    	

    for (i=0, num=1; i<100; i += length, num++){
	    xed_tables_init();
	    xed_state_zero(&dstate);
	    //if (argc > 2 && strcmp(argv[1], "-64") == 0) 
	    long_mode = true;

	    if (long_mode)  {
	        first_argv = 2;
	        dstate.mmode=XED_MACHINE_MODE_LONG_64;
	    }
	    else {
	        first_argv=1;
	        xed_state_init(&dstate,
	                       XED_MACHINE_MODE_LEGACY_32, 
	                       XED_ADDRESS_WIDTH_32b, 
	                       XED_ADDRESS_WIDTH_32b);
	    }

	    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
	    xed_error_enum_t xed_error = xed_decode(&xedd, 
	                                            REINTERPRET_CAST(const xed_uint8_t*,&itext[i]), 
	                                            XED_MAX_INSTRUCTION_BYTES);
	    switch(xed_error)    {
	      case XED_ERROR_NONE:
	        break;
	      case XED_ERROR_BUFFER_TOO_SHORT:
	        PrintDebug("Not enough bytes provided\n");
	        return 1;
	      case XED_ERROR_GENERAL_ERROR:
	        PrintDebug("Could not decode given input.\n");
	        return 1;
	      default:
	        PrintDebug("Unhandled error code \n");
	        return 1;;
	    }

	    length = xed_decoded_inst_get_length (&xedd);

	    PrintDebug("\nThe %dth instruction:", num);

	    PrintDebug("\ncategory: ");
	    PrintDebug(" %s\n", xed_category_enum_t2str(xed_decoded_inst_get_category(&xedd)));;
	    PrintDebug("ISA-extension:%s\n ",xed_extension_enum_t2str(xed_decoded_inst_get_extension(&xedd)));
	    PrintDebug(" instruction-length: %d\n ", xed_decoded_inst_get_length(&xedd));
	    PrintDebug(" operand-size:%d\n ", xed_operand_values_get_effective_operand_width(xed_decoded_inst_operands_const(&xedd)));   
	    PrintDebug("address-size:%d\n ", xed_operand_values_get_effective_address_width(xed_decoded_inst_operands_const(&xedd))); 
	    PrintDebug("iform-enum-name:%s\n ",xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)));
	    PrintDebug("iform-enum-name-dispatch (zero based):%d\n ", xed_decoded_inst_get_iform_enum_dispatch(&xedd));
	    PrintDebug("iclass-max-iform-dispatch: %d\n ", xed_iform_max_per_iclass(xed_decoded_inst_get_iclass(&xedd)));

	    // operands
	    // print_operands(&xedd);
	    
	    // memops
	    // print_memops(&xedd);
	    
	    // flags
	    //print_flags(&xedd);

	    // attributes
	    //print_attributes(&xedd);
    }



*/
