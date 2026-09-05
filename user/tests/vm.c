#include "syscall.h"
static const char ok[]="vm: low/high mappings isolated and reclaimed\n";
static const char bad[]="vm: mapping test failed\n";
int obsidia_main(void){
    uint64_t* a=sys_vm_map((void*)0x6000000000ULL,2,1);
    uint64_t* b=sys_vm_map((void*)0x700000000000ULL,1,1);
    void* k=sys_vm_map((void*)0xffffffff80000000ULL,1,1);
    int good=a!=(void*)-1&&b!=(void*)-1&&k==(void*)-1;
    if(good){a[0]=0x1234;a[1023]=0x5678;b[0]=0x9abc;good=a[0]==0x1234&&a[1023]==0x5678&&b[0]==0x9abc;}
    if(a!=(void*)-1)good&=sys_vm_unmap(a,2)==0;
    if(b!=(void*)-1)good&=sys_vm_unmap(b,1)==0;
    sys_fd_write(1,good?ok:bad,good?sizeof(ok)-1:sizeof(bad)-1);
    if(!good)__asm__ volatile("ud2");
    return good?0:1;
}
