
#include <palacios/vm_guest.h>

int v3_init_vmx_io_map(struct guest_info * info);

int v3_handle_vmx_io_in(struct guest_info * info);
int v3_handle_vmx_io_ins(struct guest_info * info);
int v3_handle_vmx_io_out(struct guest_info * info);
int v3_handle_vmx_io_outs(struct guest_info * info);

