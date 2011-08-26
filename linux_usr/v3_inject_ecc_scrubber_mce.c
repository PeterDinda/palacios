/*
 * V3 ECC DRAM Scrubber MCE
 * (c) Philip Soltero, 2010
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>

#define V3_VM_INJECT_SCRUBBER_MCE (10224+20)

int main(int argc, char * argv[]) {
    char * end_ptr;
    char * vm_device;
    unsigned int cpu;
    uint64_t address;
    int v3_fd = 0;

    if (argc <= 3) {
        fprintf(stderr, "Usage: v3_inject_ecc_scrubber_mce <vm_device> <cpu> <hex address>\n");
        return -1;
    }

    vm_device = argv[1];

    cpu = strtol(argv[2], &end_ptr, 10);
    if (strcmp(end_ptr, "\0") != 0) {
        fprintf(stderr, "The specified cpu is not a valid integer '%s', in particular '%s'.\n", argv[2], end_ptr);
        return -1;
    }

    address = strtoll(argv[3], &end_ptr, 16);
    if (strcmp(end_ptr, "\0") != 0) {
        fprintf(stderr, "The specified address is not a valid integer '%s', in particular '%s'.\n", argv[3], end_ptr);
        return -1;
    }

    v3_fd = open(vm_device, O_RDONLY);

    if (v3_fd == -1) {
        fprintf(stderr, "Error opening V3Vee control device.\n");
        return -1;
    }

    ioctl(v3_fd, V3_VM_INJECT_SCRUBBER_MCE, address);

    /* Close the file descriptor.  */
    close(v3_fd);

    return 0;
}
