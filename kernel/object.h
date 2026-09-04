#pragma once
#include <stdint.h>
struct process;
typedef struct process process_t;

enum { KOBJ_IPC=1, KOBJ_SHM=2, KOBJ_SURFACE=3, KOBJ_INPUT=4 };
enum { RIGHT_READ=1, RIGHT_WRITE=2, RIGHT_MAP=4, RIGHT_DUP=8, RIGHT_PRESENT=16 };
typedef struct kobject { uint32_t type, refs; void (*destroy)(struct kobject*); } kobject_t;
void object_retain(kobject_t* o);
void object_release(kobject_t* o);
int64_t handle_install(process_t* p,kobject_t* o,uint32_t rights,int inheritable);
kobject_t* handle_get(process_t* p,uint64_t handle,uint32_t type,uint32_t rights);
int handle_close(process_t* p,uint64_t handle);
void handles_inherit(process_t* child,process_t* parent);
void handles_close_all(process_t* p);
int64_t handle_find(process_t* p,uint32_t type);
