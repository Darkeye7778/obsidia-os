#include "syscall.h"
int obsidia_main(void){uint64_t h=(uint64_t)os_handle_find(1);char b[16];int64_t n=os_ipc_recv(h,b,sizeof(b));if(n<=0)__asm__ volatile("ud2");b[0]='R';if(os_ipc_send(h,b,(uint64_t)n)!=n)__asm__ volatile("ud2");return 0;}
