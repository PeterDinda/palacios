ifeq ($(MOD_TYPE),V3_LINUX_MOD)
	TYPE_IDX := 1
else ifeq ($(MOD_TYPE),V3_SYM_MOD)
	TYPE_IDX := 2
else ifeq ($(MOD_TYPE),V3_SEC_MOD)
	TYPE_IDX := 3
else 
	TYPE_IDX := 0
endif

ifeq ($(MOD_ARCH),i386)
	ARCH_IDX := 1
else ifeq ($(MOD_ARCH),x86_64)
	ARCH_IDX := 2
else 
	ARCH_IDX := 0
endif



DEFS := -DMOD_NAME=$(MOD_NAME) \
	-DMOD_DIR=$(MOD_DIR) \
	-DMOD_OBJ=$(MOD_OBJ) \
	-DMOD_TYPE=$(TYPE_IDX) \
	-DMOD_ARCH=$(ARCH_IDX)


#	cpp -undef $(DEFS) binary.S -o $(MOD_DIR)/.tmp.binary.S
#	cpp $(DEFS) $(MOD_DIR)/.tmp.binary.S

CFLAGS := -fno-stack-protector -Wall -O -Werror -fPIC -mno-red-zone

module: FORCE
	cpp -undef $(DEFS) binary.S -o $(MOD_DIR)/.tmp.binary.S
	gcc $(CFLAGS) $(DEFS) -c $(MOD_DIR)/.tmp.binary.S -o $(MOD_DIR)/$(MOD_NAME).vo
	rm -f $(MOD_DIR)/.tmp.binary.S

FORCE:
