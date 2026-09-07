#include "syscall.h"
#include <obsidia/app.h>

static void write(const char* text) { uint64_t n=0;while(text[n])n++;sys_fd_write(1,text,n); }
static int launch_and_wait(const char* path) {
    int64_t pid=obs_app_launch(path),status=0;
    if(pid<0)return -1;
    return sys_wait((uint64_t)pid,&status)<0?-1:(int)status;
}

int obsidia_main(void) {
    write("init: Obsidia production boot\n");
    if(obs_exec_detect("init.elf")!=OBS_EXEC_ELF64 ||
       obs_exec_detect("native-smoke" OBS_NATIVE_EXEC_EXTENSION)!=OBS_EXEC_NATIVE ||
       obs_exec_detect("win-smoke.exe")!=OBS_EXEC_PE32_PLUS ||
       obs_exec_detect("win32.exe")!=OBS_EXEC_PE32 ||
       obs_exec_detect("malformed.exe")!=OBS_EXEC_UNKNOWN) {
        write("init: executable detection self-check FAILED\n");return 1;
    }
    write("init: ELF64/native/PE detection passed\n");
    if(launch_and_wait("native-smoke" OBS_NATIVE_EXEC_EXTENSION)!=23){write("init: Obsidia-native launch FAILED\n");return 2;}
    write("init: Obsidia-native launch passed\n");
    if(launch_and_wait("win-smoke.exe")!=0){write("init: PE32+ launch FAILED\n");return 3;}
    write("init: PE32+ import-free launch passed\n");
    if(obs_app_launch("imports.exe")>=0){write("init: unsupported PE import rejection FAILED\n");return 4;}
    write("init: unsupported PE imports rejected cleanly\n");
    if(obs_app_launch("win32.exe")>=0){write("init: unsupported PE32 rejection FAILED\n");return 5;}
    write("init: PE32 recognized and rejected cleanly\n");
    if(obs_app_launch("malformed.exe")>=0){write("init: malformed PE rejection FAILED\n");return 5;}
    write("init: launching core services and desktop\n");
    if(obs_app_launch("serviced" OBS_NATIVE_EXEC_EXTENSION)<0)return 6;
    sys_sleep(2);
    int64_t settings_authority=os_ipc_create();if(settings_authority<0||os_handle_set_inheritable((uint64_t)settings_authority,1)<0)return 6;
    if(obs_app_launch("settingsd" OBS_NATIVE_EXEC_EXTENSION)<0)return 6;
    os_handle_set_inheritable((uint64_t)settings_authority,0);sys_sleep(2);
    int64_t output=os_handle_find(OS_OBJECT_DISPLAY_OUTPUT);if(output<0||os_handle_set_inheritable((uint64_t)output,1)<0)return 6;
    if(obs_app_launch("displayd" OBS_NATIVE_EXEC_EXTENSION)<0)return 6;
    os_handle_set_inheritable((uint64_t)output,0);
    sys_sleep(2);
    int64_t input=os_handle_find(OS_OBJECT_INPUT);if(input<0||os_handle_set_inheritable((uint64_t)input,1)<0)return 8;
    if(obs_app_launch("inputd" OBS_NATIVE_EXEC_EXTENSION)<0)return 8;
    os_handle_set_inheritable((uint64_t)input,0);
    sys_sleep(2);
    os_handle_set_inheritable((uint64_t)settings_authority,1);
    int64_t desktop=obs_app_launch("desktop" OBS_NATIVE_EXEC_EXTENSION);os_handle_set_inheritable((uint64_t)settings_authority,0);os_handle_close((uint64_t)settings_authority);if(desktop<0)return 7;
    int64_t status=0;sys_wait((uint64_t)desktop,&status);return (int)status;
}
