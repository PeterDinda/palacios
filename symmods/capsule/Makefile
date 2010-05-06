
DEFS := -DMOD_NAME=$(MOD_NAME) \
	-DMOD_DIR=$(MOD_DIR) \
	-DMOD_OBJ=$(MOD_OBJ) \
	-DMOD_TYPE=$(MOD_TYPE)

module: FORCE
	cpp -undef $(DEFS) binary.S -o $(MOD_DIR)/.tmp.binary.S
	gcc $(DEFS) -c $(MOD_DIR)/.tmp.binary.S -o $(MOD_DIR)/$(MOD_NAME).vo
	rm -f $(MOD_DIR)/.tmp.binary.S

FORCE: