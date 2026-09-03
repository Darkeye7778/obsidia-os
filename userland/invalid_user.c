#include "syscall.h"
static const char ok[]="invalid-pointer: kernel rejected bad buffer\n";
void _start(void) {
    int64_t r=sys_fd_write(1,(const void*)0x7ffffffff000ULL,32);
    if(r<0) sys_fd_write(1,ok,sizeof(ok)-1);
    sys_exit(r<0 ? 0 : 1);
    for(;;) { }
}
