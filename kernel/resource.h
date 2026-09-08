#pragma once
#include <stdint.h>
#include "task.h"
enum { INPUT_EVENT_KEY=1, INPUT_EVENT_RELATIVE=2, INPUT_EVENT_BUTTON=3, INPUT_EVENT_MOUSE_MOVE=4 };
enum { INPUT_AXIS_X=1, INPUT_AXIS_Y=2 };
enum { INPUT_BUTTON_LEFT=1, INPUT_BUTTON_RIGHT=2, INPUT_BUTTON_MIDDLE=3 };
typedef struct { uint32_t type, code; int32_t value; uint32_t reserved; uint64_t ticks; } input_event_t;
#define OS_SERVICE_NAME_MAX 32
int64_t ipc_create(process_t* p);
int64_t ipc_send(process_t* p,uint64_t h,const void* data,uint64_t len);
int64_t ipc_receive(process_t* p,uint64_t h,void* data,uint64_t cap);
int64_t ipc_send_handle(process_t* p,uint64_t h,const void* data,uint64_t len,uint64_t attached,uint32_t rights);
int64_t ipc_receive_handle(process_t* p,uint64_t h,void* data,uint64_t cap,int64_t* attached_out);
int64_t ipc_receive_ex(process_t* p,uint64_t h,void* data,uint64_t cap,
                       int64_t* attached_out,uint64_t* sender_pid_out);
int64_t service_port_open(process_t* p);
void* ipc_send_wait_channel(process_t* p,uint64_t h);
void* ipc_receive_wait_channel(process_t* p,uint64_t h);
int64_t shm_create(process_t* p,uint64_t pages);
int64_t shm_map(process_t* p,uint64_t h,uint64_t va,int writable);
int64_t shm_unmap(process_t* p,uint64_t va);
int64_t surface_create(process_t* p,uint32_t width,uint32_t height);
int64_t surface_present(process_t* p,uint64_t output,uint64_t surface,uint32_t x,uint32_t y);
int resource_grant_input(process_t* p);
int resource_grant_display_output(process_t* p);
int resource_grant_system_control(process_t* p);
int resource_system_control(process_t* p,uint64_t handle,uint32_t action);
void resource_input_push(uint32_t type,uint32_t code,int32_t value);
void resource_input_push_motion(int32_t dx,int32_t dy);
int64_t resource_input_read(process_t* p,uint64_t h,input_event_t* event);
int resource_input_self_test(void);
uint64_t resource_input_dropped_motion(void);
uint64_t resource_input_coalesced_motion(void);
