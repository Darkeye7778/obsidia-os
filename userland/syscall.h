#pragma once
#include <stdint.h>

/* Userland syscall numbers (must match kernel/syscall.h) */
#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_GETTICKS 2
#define SYS_YIELD    3
#define SYS_FBINFO   4
#define SYS_FD_READ  5
#define SYS_FD_WRITE 6
#define SYS_OPEN     7
#define SYS_CLOSE    8
#define SYS_SLEEP    9
#define SYS_SPAWN    10
#define SYS_WAIT     11

/* Simple fb info struct for user (matches kernel) */
typedef struct {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint32_t* fb;
} fb_info_t;

/* Raw syscall stub using int 0x80.
 * Follows the convention used by the existing asm demo:
 *   rax = syscall number
 *   rdi, rsi, rdx, ... = args (System V style for the int 0x80 path)
 */
static inline uint64_t syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t ret;
    __asm__ volatile (
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(ret)
        : "r"(num), "r"(a1), "r"(a2), "r"(a3)
        : "rax", "rdi", "rsi", "rdx", "memory"
    );
    return ret;
}

/* Convenience wrappers matching the old asm demo */
static inline void sys_exit(int code) {
    syscall(SYS_EXIT, (uint64_t)code, 0, 0);
}

static inline void sys_write(const char* buf, uint64_t len) {
    syscall(SYS_WRITE, (uint64_t)buf, len, 0);
}

static inline uint64_t sys_getticks(void) {
    return syscall(SYS_GETTICKS, 0, 0, 0);
}

static inline void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0);
}

static inline void sys_fbinfo(fb_info_t* out) {
    syscall(SYS_FBINFO, (uint64_t)out, 0, 0);
}

static inline int64_t sys_read(int fd, void* buf, uint64_t len) {
    return (int64_t)syscall(SYS_FD_READ,(uint64_t)fd,(uint64_t)buf,len);
}
static inline int64_t sys_fd_write(int fd, const void* buf, uint64_t len) {
    return (int64_t)syscall(SYS_FD_WRITE,(uint64_t)fd,(uint64_t)buf,len);
}
static inline int64_t sys_spawn(const char* path) {
    return (int64_t)syscall(SYS_SPAWN,(uint64_t)path,0,0);
}
static inline int64_t sys_open(const char* path) {
    return (int64_t)syscall(SYS_OPEN,(uint64_t)path,0,0);
}
static inline int64_t sys_close(int fd) {
    return (int64_t)syscall(SYS_CLOSE,(uint64_t)fd,0,0);
}
static inline void sys_sleep(uint64_t ticks) {
    syscall(SYS_SLEEP,ticks,0,0);
}
static inline int64_t sys_wait(uint64_t pid, int64_t* status) {
    return (int64_t)syscall(SYS_WAIT,pid,(uint64_t)status,0);
}
