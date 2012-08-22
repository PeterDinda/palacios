/* 
 * V3 Environment Variable Injection Utility
 * This code allows a user to inject environment variables into a process
 * marked by a specific binary name in a running guest.
 *
 * (c) Kyle C. Hale, 2012
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "iface-env-inject.h"
#include "v3_ctrl.h"


int main (int argc, char **argv) {
	char *vm_dev, *env_file, *bin_name;
	int err, bytes_read, num_strings;
    struct stat t_stat;
    struct env_data env;
    char * strings[MAX_NUM_STRINGS];
    char tmp_str[MAX_STRING_LEN];
    int i = 0;
    FILE * t_fd;

	if (argc < 4) {
		v3_usage("<vm device> <env-file> <inject-point-exe>\n\n"
                         "\tenv-file : file containing a list of new-line separated env vars\n\n"
                         "\tinject-point-exe : if this is an exec-hooked inject, use this executable name\n");
                         
		return -1;
	}

	vm_dev = argv[1];
    env_file = argv[2];
    bin_name = argv[3];

	t_fd = fopen(env_file, "r");
	if (!t_fd) {
		fprintf(stderr, "Error opening environment variable file: %s\n", env_file);
		return -1;
	}

    /* copy in the vars line by line */
    while (fgets(tmp_str, MAX_STRING_LEN, t_fd) != NULL) {
        int len = strlen(tmp_str) - 1;
        if (tmp_str[len] == '\n')
            tmp_str[len] = 0;
        strings[i] = (char*)malloc(MAX_STRING_LEN);
        if (!strings[i]) {
                fprintf(stderr, "Error allocating space for variable\n");
                return -1;
        }
        strcpy(strings[i], tmp_str);
        i++;
    }

    env.num_strings = i;
    printf("Found %d environment variables to inject\n", i);

    env.strings = (char**) strings;

    if (!bin_name) {
        fprintf(stderr, "Error: no binary hook provided\n");
        return -1;
    }

    strncpy(env.bin_name, bin_name, MAX_STRING_LEN);

    printf("Transferring control to Palacios\n");
	err = v3_vm_ioctl(vm_dev, V3_VM_ENV_INJECT, &env);
	if (err < 0) {
		fprintf(stderr, "Error providing env var data to palacios\n");
		return -1;
	}
    
	close(t_fd);
	return 0;
}
