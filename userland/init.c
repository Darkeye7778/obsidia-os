#include "syscall.h"

static const char start[]="init: userspace service bootstrap\n";
static const char child[]="hello.elf";
static const char invalid[]="invalid.elf";
static const char fault[]="fault_user.bin";
static const char done[]="init: child exited; services ready\n";

void _start(void) {
    sys_fd_write(1,start,sizeof(start)-1);
    int64_t pid=sys_spawn(child);
    int64_t pid2=sys_spawn(invalid);
    if(pid>0) { int64_t status; sys_wait((uint64_t)pid,&status); }
    if(pid2>0) { int64_t status; sys_wait((uint64_t)pid2,&status); }
    pid=sys_spawn(fault);
    if(pid>0) { int64_t status; sys_wait((uint64_t)pid,&status); }
    sys_fd_write(1,done,sizeof(done)-1);
    sys_exit(0);
    for(;;) { }
}
