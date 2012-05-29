#ifndef __IFACE_SYSCALL_H__
#define __IFACE_SYSCALL_H__

#define SYSCALL_OFF   0
#define SYSCALL_ON    1
#define SYSCALL_STAT  2

#define V3_VM_SYSCALL_CTRL 0x5CA11

struct v3_syscall_cmd {
    int cmd;
    int syscall_nr;
};


#endif
