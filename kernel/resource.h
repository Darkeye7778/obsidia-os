#pragma once
#include <stdint.h>
#include "task.h"
typedef struct { uint32_t type, code; int32_t value; uint32_t reserved; uint64_t ticks; } input_event_t;
int64_t ipc_create(process_t* p);
int64_t ipc_send(process_t* p,uint64_t h,const void* data,uint64_t len);
int64_t ipc_receive(process_t* p,uint64_t h,void* data,uint64_t cap);
void* ipc_send_wait_channel(process_t* p,uint64_t h);
void* ipc_receive_wait_channel(process_t* p,uint64_t h);
int64_t shm_create(process_t* p,uint64_t pages);
int64_t shm_map(process_t* p,uint64_t h,uint64_t va,int writable);
int64_t shm_unmap(process_t* p,uint64_t va);
int64_t surface_create(process_t* p,uint32_t width,uint32_t height);
int64_t surface_present(process_t* p,uint64_t h,uint32_t x,uint32_t y);
int resource_grant_input(process_t* p);
void resource_input_push(uint32_t code,int32_t value);
int64_t resource_input_read(process_t* p,uint64_t h,input_event_t* event);
