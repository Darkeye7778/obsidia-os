#include "syscall.h"
int obsidia_main(void){uint64_t h=(uint64_t)os_handle_find(2);uint64_t*p=os_shm_map(h,(void*)0x5000000000ULL,1);if(p==(void*)-1||p[0]!=0x11223344ULL)__asm__ volatile("ud2");p[0]=0x55667788ULL;if(os_shm_unmap(p))__asm__ volatile("ud2");return 0;}
