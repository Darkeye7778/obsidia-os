#include "syscall.h"
int obsidia_main(void){static const char message[]="native-smoke: OBSX v1 application ran\n";sys_fd_write(1,message,sizeof(message)-1);return 23;}
