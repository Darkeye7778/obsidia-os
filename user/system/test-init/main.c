#include "syscall.h"
#include <obsidia/app.h>
static int wait_for(const char*path){int64_t pid=obs_app_launch(path),status=0;if(pid<0)return-999;return sys_wait((uint64_t)pid,&status)<0?-998:(int)status;}
int obsidia_main(void){
    static const char start[]="TEST: regression boot started\n";sys_fd_write(1,start,sizeof(start)-1);
    if(obs_exec_detect("malformed.exe")!=OBS_EXEC_UNKNOWN||obs_exec_detect("win-smoke.exe")!=OBS_EXEC_PE32_PLUS||obs_exec_detect("win32.exe")!=OBS_EXEC_PE32||
       wait_for("native-smoke" OBS_NATIVE_EXEC_EXTENSION)!=23||wait_for("win-smoke.exe")!=0||obs_app_launch("imports.exe")>=0)__asm__ volatile("ud2");
    if(obs_app_launch("win32.exe")>=0)__asm__ volatile("ud2");
    const char*tests[]={"hello.elf","invalid.elf","vm.elf","fpu.elf","surface.elf","fs.elf"};
    for(uint32_t i=0;i<sizeof(tests)/sizeof(tests[0]);i++)if(wait_for(tests[i])<0)__asm__ volatile("ud2");
    if(wait_for("fault.elf")>=0)__asm__ volatile("ud2");
    int64_t endpoint=os_ipc_create(),p1=obs_app_launch("ipc_client.elf"),p2=obs_app_launch("ipc_client.elf"),status=0;
    sys_sleep(3);char one[]="one",two[]="two",reply[8];os_ipc_send(endpoint,one,3);os_ipc_send(endpoint,two,3);
    if(p1>0)sys_wait((uint64_t)p1,&status);
    if(p2>0)sys_wait((uint64_t)p2,&status);
    if(os_ipc_recv(endpoint,reply,sizeof(reply))<=0||os_ipc_recv(endpoint,reply,sizeof(reply))<=0)__asm__ volatile("ud2");
    int64_t sender=obs_app_launch("ipc_sender.elf");sys_sleep(3);for(int i=0;i<9;i++)if(os_ipc_recv(endpoint,reply,sizeof(reply))!=1)__asm__ volatile("ud2");
    if(sender>0)sys_wait((uint64_t)sender,&status);
    os_handle_close((uint64_t)endpoint);
    int64_t shared=os_shm_create(1);uint64_t*data=os_shm_map((uint64_t)shared,(void*)0x5100000000ULL,1);if(data==(void*)-1)__asm__ volatile("ud2");
    data[0]=0x11223344ULL;int64_t client=obs_app_launch("shm_client.elf");if(client>0)sys_wait((uint64_t)client,&status);
    if(data[0]!=0x55667788ULL)__asm__ volatile("ud2");
    os_shm_unmap(data);os_handle_close((uint64_t)shared);
    int64_t transfer_ep=os_ipc_create(),transfer_shm=os_shm_create(1),received_handle=-1;uint8_t marker=0x5a,received_marker=0;
    if(transfer_ep<0||transfer_shm<0||os_ipc_send_handle((uint64_t)transfer_ep,&marker,1,(uint64_t)transfer_shm,OS_RIGHT_READ)!=1)__asm__ volatile("ud2");
    os_handle_close((uint64_t)transfer_shm);
    if(os_ipc_recv_handle((uint64_t)transfer_ep,&received_marker,1,&received_handle)!=1||received_marker!=marker||received_handle<0)__asm__ volatile("ud2");
    if(os_shm_map((uint64_t)received_handle,(void*)0x5200000000ULL,0)!=(void*)-1)__asm__ volatile("ud2");
    os_handle_close((uint64_t)received_handle);os_handle_close((uint64_t)transfer_ep);
    static const char passed[]="TEST: all regression checks passed\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
