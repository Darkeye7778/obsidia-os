#include "syscall.h"
static const char ok[]="fpu: SSE state survived preemption\n";
static const char bad[]="fpu: SSE STATE CORRUPTED\n";
int obsidia_main(void) {
    uint64_t pid=sys_getpid(), out[2];
    uint64_t in[2]={0x1122334455667788ULL^pid,0x8877665544332211ULL^pid};
    __asm__ volatile("movdqu %0, %%xmm0"::"m"(in):"xmm0");
    sys_sleep(8);
    __asm__ volatile("movdqu %%xmm0, %0":"=m"(out));
    int good=out[0]==in[0]&&out[1]==in[1];
    sys_fd_write(1,good?ok:bad,good?sizeof(ok)-1:sizeof(bad)-1);
    if(!good) __asm__ volatile("ud2");
    return good?0:1;
}
