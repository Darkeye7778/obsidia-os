#include "syscall.h"

static const char start[]="init: userspace service bootstrap\n";
static const char child[]="hello.elf";
static const char invalid[]="invalid.elf";
static const char fault[]="fault_user.bin";
static const char fpu[]="fpu.elf";
static const char vm[]="vm.elf";
static const char ipcclient[]="ipc_client.elf";
static const char ipcsender[]="ipc_sender.elf";
static const char shmclient[]="shm_client.elf";
static const char shell[]="shell.elf";
static const char surface[]="surface.elf";
static const char fs[]="fs.elf";
static const char done[]="init: child exited; services ready\n";

int obsidia_main(void) {
    sys_fd_write(1,start,sizeof(start)-1);
    int64_t pid=sys_spawn(child);
    int64_t pid2=sys_spawn(invalid);
    if(pid>0) { int64_t status; sys_wait((uint64_t)pid,&status); }
    if(pid2>0) { int64_t status; sys_wait((uint64_t)pid2,&status); }
    pid=sys_spawn(vm); if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}
    int64_t ep=os_ipc_create();
    pid=sys_spawn(ipcclient);pid2=sys_spawn(ipcclient);sys_sleep(3);
    char q1[]="one",q2[]="two",reply[8];os_ipc_send(ep,q1,3);os_ipc_send(ep,q2,3);
    if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}if(pid2>0){int64_t status;sys_wait((uint64_t)pid2,&status);}
    if(os_ipc_recv(ep,reply,sizeof(reply))<=0||os_ipc_recv(ep,reply,sizeof(reply))<=0)__asm__ volatile("ud2");
    pid=sys_spawn(ipcsender);sys_sleep(3);for(int i=0;i<9;i++)if(os_ipc_recv(ep,reply,sizeof(reply))!=1)__asm__ volatile("ud2");if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}
    if(os_handle_close((uint64_t)ep)||os_handle_close((uint64_t)ep)==0)__asm__ volatile("ud2");
    int64_t sh=os_shm_create(1);uint64_t*sp=os_shm_map((uint64_t)sh,(void*)0x5100000000ULL,1);
    if(sp==(void*)-1)__asm__ volatile("ud2");
    sp[0]=0x11223344ULL;pid=sys_spawn(shmclient);
    if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}if(sp[0]!=0x55667788ULL)__asm__ volatile("ud2");
    if(os_shm_unmap(sp)||os_handle_close((uint64_t)sh))__asm__ volatile("ud2");
    pid=sys_spawn(fpu); pid2=sys_spawn(fpu);
    if(pid>0) { int64_t status; sys_wait((uint64_t)pid,&status); }
    if(pid2>0) { int64_t status; sys_wait((uint64_t)pid2,&status); }
    pid=sys_spawn(fault);
    if(pid>0) { int64_t status; sys_wait((uint64_t)pid,&status); }
    pid=sys_spawn(surface);if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}
    pid=sys_spawn(fs);if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}
    sys_fd_write(1,done,sizeof(done)-1);
    pid=sys_spawn(shell);if(pid>0){int64_t status;sys_wait((uint64_t)pid,&status);}
    return 0;
}
