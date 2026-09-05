#include "syscall.h"
static const char msg[]="hello: normal ELF64 child process; VFS says: ";
static const char path[]="foundation.txt";
int obsidia_main(void) {
    sys_fd_write(1,msg,sizeof(msg)-1);
    int64_t fd=sys_open(path);
    if(fd>=0) {
        char buf[64]; int64_t n=sys_read((int)fd,buf,sizeof(buf));
        if(n>0) sys_fd_write(1,buf,(uint64_t)n);
        sys_close((int)fd);
    }
    for (volatile uint64_t i=0;i<20000000ULL;i++) { }
    sys_sleep(5);
    return 7;
}
