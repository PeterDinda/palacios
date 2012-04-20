#ifndef __IFACE_CODE_INJECT_H__
#define __IFACE_CODE_INJECT_H__

#define V3_VM_TOPHALF_INJECT 12123
#define V3_VM_HYPERCALL_ADD 12124
#define V3_VM_HYPERCALL_REMOVE 12125

#define MAX_INJ 128

struct top_half_data {
    unsigned long elf_size;
    void *elf_data;
    int got_offset;
    int plt_offset;
    int func_offset;
    char bin_file[256];
    int hcall_nr;
    int inject_id;
    int is_dyn;
    int is_exec_hooked;
};

#define HCALL_NAME_MAX 256

struct hcall_data {
  int   fd;
  int   hcall_nr;
  char  fn[HCALL_NAME_MAX];
};

#endif
