
#include <palacios/vmx_io.h>
#include <palacios/vmm_io.h>
#include <palacios/vmcs.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm.h>

/* Same as SVM */
static int update_map(struct guest_info * info, uint16_t port, int hook_read, int hook_write)
{
    uchar_t * bitmap = (uint8_t *)(info->io_map.arch_data);
    int major = port / 8;
    int minor = port % 8;

    if ((hook_read == 0) && (hook_write == 0)) {
	*(bitmap + major) &= ~(0x1 << minor);
    } else {
	*(bitmap + major) |= (0x1 << minor);
    }

    return 0;
}

int v3_init_vmx_io_map(struct guest_info * info)
{
    info->io_map.update_map = update_map;
    
    info->io_map.arch_data = V3_VAddr(V3_AllocPages(2));
    memset(info->io_map.arch_data, 0, PAGE_SIZE_4KB*2);

    return 0;
}

int v3_handle_vmx_io_in(struct guest_info * info)
{
    PrintDebug("IN not implemented\n");
    return -1;
}

int v3_handle_vmx_io_ins(struct guest_info * info)
{
    PrintDebug("INS not implemented\n");
    return -1;
}

int v3_handle_vmx_io_out(struct guest_info * info)
{
    ulong_t exit_qual;

    vmcs_read(VMCS_EXIT_QUAL, &exit_qual);

    struct vmcs_io_qual * io_qual = (struct vmcs_io_qual *)&exit_qual;

    struct v3_io_hook * hook = v3_get_io_hook(info, io_qual->port);

    if(hook == NULL) {
        PrintError("Hook not present for out on port %x\n", io_qual->port);
        return -1;
    }

    int write_size = 1<<(io_qual->accessSize);
    
    PrintDebug("OUT of %d bytes on port %d (0x%x)\n", write_size, io_qual->port, io_qual->port);

    if(hook->write(io_qual->port, &(info->vm_regs.rax), write_size, hook->priv_data) != write_size) {
        PrintError("Write failure for out on port %x\n",io_qual->port);
        return -1;
    }

    uint32_t instr_length;

    vmcs_read(VMCS_EXIT_INSTR_LEN, &instr_length);

    info->rip += instr_length;

    return 0;
}

int v3_handle_vmx_io_outs(struct guest_info * info)
{
    ulong_t exit_qual;

    vmcs_read(VMCS_EXIT_QUAL, &exit_qual);

    struct vmcs_io_qual * io_qual = (struct vmcs_io_qual *)&exit_qual;

    PrintDebug("OUTS on port %d, (0x%x)\n", io_qual->port, io_qual->port);
    return -1;
}
