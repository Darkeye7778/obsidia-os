#include "usercopy.h"
#include "task.h"
#include "paging.h"
int copy_from_user(void* dst, uint64_t src, uint64_t len) {
    if (len && (!current_task || !paging_user_range_valid(current_task->cr3,src,len,0))) return 0;
    for(uint64_t i=0;i<len;i++) ((uint8_t*)dst)[i]=((uint8_t*)src)[i]; return 1;
}
int copy_to_user(uint64_t dst, const void* src, uint64_t len) {
    if (len && (!current_task || !paging_user_range_valid(current_task->cr3,dst,len,1))) return 0;
    for(uint64_t i=0;i<len;i++) ((uint8_t*)dst)[i]=((const uint8_t*)src)[i]; return 1;
}
int copy_string_from_user(char* dst, uint64_t src, uint64_t cap) {
    if (!cap) return 0;
    for(uint64_t i=0;i<cap;i++){ if(!copy_from_user(&dst[i],src+i,1)) return 0; if(!dst[i]) return 1; }
    dst[cap-1]=0; return 0;
}
