/* 
 * V3 Top Half Injection Utility
 * This code allows a user to inject a "top half" of code into a running guest. 
 * The "bottom half" (a hypercall handler) is inserted using another utility.
 *
 * (c) Kyle C. Hale, 2011
 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "iface-code-inject.h"
#include "v3_ctrl.h"

#define __ELF_INJECT_CLASS 32

#define ElfW(type)	_ElfW (Elf, __ELF_INJECT_CLASS, type)
#define _ElfW(e,w,t)	_ElfW_1 (e, w, _##t)
#define _ElfW_1(e,w,t)	e##w##t


/* look for PT_DYNAMIC to see if it's dynamically linked */
static int is_dynamic (ElfW(Ehdr) * ehdr) {
    int i;
    ElfW(Phdr) * phdr = (ElfW(Phdr)*)((char*)ehdr + ehdr->e_phoff);
    ElfW(Phdr) * phdr_cursor;

    phdr_cursor = phdr;

    for (i = 0; i < ehdr->e_phnum; i++, phdr_cursor++) {
        if (phdr_cursor->p_type == PT_DYNAMIC)
            return 1;
    }

    return 0;
}


int main (int argc, char **argv) {
	char *vm_dev = NULL;
	char *top_half = NULL;
    char *bin_file = NULL;
	int t_fd, err, bytes_read, entry;
    struct stat t_stat;
    struct top_half_data elf;
    ElfW(Ehdr) * elf_hdr;

	if (argc < 4 || argc > 5) {
                // TODO: add a better explanation here
		v3_usage("/dev/v3-vm<N> <inject-code> <code-entry-offset> [inject-point-exe]\n\n"
                          "\tinject-code       : the binary file to be injected\n\n"
                          "\tcode-entry-offset : offset in the binary to .text\n\n"
                          "\tinject-point-exe  : if exec-hooked injection is used, use this exec name\n");
		return -1;
	}

	vm_dev = argv[1];
    top_half = argv[2];
    entry = strtol(argv[3], NULL, 0);
    if (argv[4]) 
        bin_file = argv[4];
    

	t_fd = open(top_half, O_RDONLY);
	if (t_fd == -1) {
		fprintf(stderr, "Error opening top half .o file: %s\n", top_half);
		return -1;
	}

    if (fstat(t_fd, &t_stat) < 0) {
        fprintf(stderr, "Error: could not stat ELF binary file %s\n", top_half);
        return -1;
    }

    memset(&elf, 0, sizeof(struct top_half_data));

    elf.elf_size = t_stat.st_size;

    if (bin_file) {
        strcpy(elf.bin_file, bin_file);
        elf.is_exec_hooked = 1;
    } else {
        elf.is_exec_hooked = 0;
    }

        
    /* read in the ELF */
    elf.elf_data = malloc(elf.elf_size);
    if (!elf.elf_data) {
        fprintf(stderr, "Error allocating memory for ELF data\n");
        return -1;
    }

    printf("Loading ELF binary...\n");
    if ((bytes_read = read(t_fd, elf.elf_data, elf.elf_size)) < 0) {
        fprintf(stderr, "Error loading ELF binary %s\n", top_half);
        return -1;
    }

    printf("Loaded. %d Bytes of data read\n", bytes_read);
    elf_hdr = (ElfW(Ehdr)*)elf.elf_data;

    /* set the entry point */
    elf.func_offset = entry;

    /* check the ELF magic nr to make sure this is a valid ELF */
    if (elf_hdr->e_ident[EI_MAG0] != 0x7f ||
        elf_hdr->e_ident[EI_MAG1] != 'E'  ||
        elf_hdr->e_ident[EI_MAG2] != 'L'  ||
        elf_hdr->e_ident[EI_MAG3] != 'F') {
            
        fprintf(stderr, "Error: Invalid ELF binary %s\n", top_half);
        return -1;
    }

    /* make sure the ELF is an actual executable file */
    if (elf_hdr->e_type != ET_EXEC) {
        fprintf(stderr, "Error: ELF must be an executable file %s\n", top_half);
        return -1;
    }

    /* is it a dynamically linked executable? */
    elf.is_dyn = is_dynamic(elf_hdr);


    printf("Transferring control to Palacios\n");
	err = v3_vm_ioctl(vm_dev, V3_VM_TOPHALF_INJECT, &elf);
	if (err < 0) {
		fprintf(stderr, "Error providing top half to palacios: %s\n", top_half);
		return -1;
	}
    
    free(elf.elf_data);
	close(t_fd);

	return 0;
}
