#pragma once
#include <stdint.h>

void syscall_init(void);

// Syscall numbers (Obsidia base set for GUI/apps)
#define SYS_EXIT     0
#define SYS_WRITE    1   // write to console (char* buf, len in rdx or simple)
#define SYS_GETTICKS 2
#define SYS_YIELD    3
#define SYS_FBINFO   4   // returns fb width/height/pitch/addr in a struct via user pointer
#define SYS_FD_READ  5
#define SYS_FD_WRITE 6
#define SYS_OPEN     7
#define SYS_CLOSE    8
#define SYS_SLEEP    9
#define SYS_SPAWN    10
#define SYS_WAIT     11
#define SYS_GETPID   12
#define SYS_VM_MAP   13
#define SYS_VM_UNMAP 14
#define SYS_HANDLE_CLOSE 15
#define SYS_HANDLE_FIND  16
#define SYS_IPC_CREATE   17
#define SYS_IPC_SEND     18
#define SYS_IPC_RECV     19
#define SYS_SHM_CREATE   20
#define SYS_SHM_MAP      21
#define SYS_SHM_UNMAP    22
#define SYS_SURFACE_CREATE 23
#define SYS_SURFACE_PRESENT 24
#define SYS_INPUT_READ      25
#define SYS_EXEC_DETECT     26
#define SYS_SERVICE_PORT_OPEN 27
#define SYS_IPC_SEND_HANDLE   28
#define SYS_IPC_RECV_HANDLE   29
#define SYS_HANDLE_SET_INHERIT 30
#define SYS_IPC_RECV_EX          31
#define SYS_PROCESS_ALIVE        32
#define SYS_IPC_TRY_SEND         33
#define SYS_HANDLE_HAS_REMOTE    34
#define SYS_IPC_TRY_SEND_HANDLE  35
#define SYS_INPUT_TRY_READ       36

// Simple fb info struct for user
typedef struct {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint32_t* fb;   // always null; physical display memory is never mapped to userspace
} fb_info_t;
