#pragma once
#include <stdint.h>

void syscall_init(void);

// Syscall numbers (Obsidia base set for GUI/apps)
#define SYS_EXIT     0
#define SYS_WRITE    1   // write to console (char* buf, len in rdx or simple)
#define SYS_GETTICKS 2
#define SYS_YIELD    3
#define SYS_FBINFO   4   // returns fb width/height/pitch/addr in a struct via user pointer

// Simple fb info struct for user
typedef struct {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint32_t* fb;   // virtual address (usable from user in our single-AS model)
} fb_info_t;
