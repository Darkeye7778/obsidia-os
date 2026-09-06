#include "syscall.h"
int obsidia_main(void){sys_fd_write(1,"fault: deliberate user exception\n",33);__asm__ volatile("ud2");return 1;}
