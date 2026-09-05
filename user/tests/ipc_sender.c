#include "syscall.h"
int obsidia_main(void){int64_t h=os_handle_find(1);if(h<0)__asm__ volatile("ud2");for(char i=0;i<9;i++)if(os_ipc_send((uint64_t)h,&i,1)!=1)__asm__ volatile("ud2");return 0;}
