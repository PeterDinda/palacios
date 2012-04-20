#ifndef __IFACE_HOST_HYPERCALL_H__
#define __IFACE_HOST_HYPERCALL_H__

#define V3_VM_HYPERCALL_ADD 12124
#define V3_VM_HYPERCALL_REMOVE 12125


#define HCALL_NAME_MAX 256

struct hcall_data {
  int   hcall_nr;
  char  fn[HCALL_NAME_MAX];
};

#endif
