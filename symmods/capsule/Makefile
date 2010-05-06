ifeq ($(MOD_TYPE),V3_LINUX_MOD)
	MOD_IDX := 1
else ifeq ($(MOD_TYPE),V3_SYM_MOD)
	MOD_IDX := 2
else ifeq($(MOD_TYPE),V3_SEC_MOD)
	MOD_IDX := 3
endif



DEFS := -DMOD_NAME=$(MOD_NAME) \
	-DMOD_DIR=$(MOD_DIR) \
	-DMOD_OBJ=$(MOD_OBJ) \
	-DMOD_TYPE=$(MOD_IDX)


#	cpp -undef $(DEFS) binary.S -o $(MOD_DIR)/.tmp.binary.S
#	cpp $(DEFS) $(MOD_DIR)/.tmp.binary.S


module: FORCE
	cpp -undef $(DEFS) binary.S -o $(MOD_DIR)/.tmp.binary.S
	gcc $(DEFS) -c $(MOD_DIR)/.tmp.binary.S -o $(MOD_DIR)/$(MOD_NAME).vo
	rm -f $(MOD_DIR)/.tmp.binary.S

FORCE:
