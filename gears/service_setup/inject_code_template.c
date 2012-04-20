#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


/* 32-bit syscall numbers */
#define __NR_exit          1
#define __NR_fork          2
#define __NR_write         4
#define __NR_open          5
#define __NR_close         6
#define __NR_waitpid       7
#define __NR_execve       11

/* 32-bit system call conventions 
 *
 * eax = syscall nr
 * ebx = arg 1
 * ecx = arg 2
 * edx = arg 3
 * esi = arg 4
 * edi = arg 5
 * ebp = arg 6
 */
int _start() {

    int FD, bytes_written, status, exec_ret;
    int flags = O_RDWR|O_CREAT; 
    int mode = S_IRUSR|S_IWUSR|S_IXUSR;
    pid_t pid, ret;
    char * env[1];

    env[0] = 0;

#include "generated.h"

#ifdef DO_WRITE
    /* open("FILENAME, O_RDWR | O_CREAT,  */
    asm volatile ("pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx" 
                : "=a" (FD)
                : "0" (__NR_open), "r" (FILE_NAME), "c" (flags), "d" (mode)); 

    if (!FD)
        goto die;


    /* write(FD, INJECT_FILE, FILE_LENGTH) */
    asm volatile ("pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx" 
		        : "=a" (bytes_written)
		        : "0" (__NR_write), "r" (FD), "c" (inject_file), "d" (FILE_LENGTH));

    if (!bytes_written)
        goto die;


    /* close(FD) */
    asm volatile ("pushl %%ebx; movl %1,%%ebx; int $0x80; popl %%ebx" 
		      : : "a" (__NR_close), "r" (FD));
#endif 


#ifdef DO_FORKEXEC
    /* pid = fork() */
    asm volatile ("int $0x80" : "=a" (pid) : "0" (__NR_fork));


    if (pid < 0) {
        goto die;
    } else if (pid > 0) {

        do {
            /* ret = waitpid(pid, &status, 0) */
            asm volatile ("pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                        : "=a" (ret)
                        : "0" (__NR_waitpid), "r" (pid), "c" (&status), "d" (0));

        } while (ret == -1);

    } else {

        /* execve("command", "arg0" , ..., "argN" , env) */
        asm volatile ("pushl %%ebx; movl %2,%%ebx; int $0x80; popl %%ebx"
                    : "=a" (exec_ret)
                    : "0" (__NR_execve), "r" (CMD), "c" (args), "d" (env));
        
        if (exec_ret < 0)
            /* exit(127) */
            asm volatile ("pushl %%ebx; movl %1,%%ebx; int $0x80; popl %%ebx"
                          : : "a" (__NR_exit), "r" (127));
    }
#endif

    die:
        /* hypercall(f001) <=> exit(0) */
        asm volatile ("movl $0xf001, %eax");
        asm volatile ("vmmcall");
        /* exit(1) */
        asm volatile ("pushl %%ebx; movl %1,%%ebx; int $0x80; popl %%ebx"
                      : : "a" (__NR_exit), "r" (1));
}
