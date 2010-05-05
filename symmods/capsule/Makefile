mod_name := test
mod_dir := /home/jarusl
mod_obj := bar.o
mod_type := 1

DEFS := -DMOD_NAME=$(mod_name) -DMOD_DIR=$(mod_dir) -DMOD_OBJ=$(mod_obj) -DMOD_TYPE=$(mod_type)




module: FORCE
	gcc $(DEFS) -c binary.S -o module.vo

FORCE: