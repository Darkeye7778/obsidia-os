#include "resource.h"
#include "object.h"
#include "memory/heap.h"
#include "memory/memory.h"
#include "paging.h"
#include "drivers/framebuffer.h"
#include "timer.h"

#define IPC_MESSAGES 8
#define IPC_MESSAGE_BYTES 64

typedef struct { kobject_t base; uint8_t data[IPC_MESSAGES][IPC_MESSAGE_BYTES]; uint8_t len[IPC_MESSAGES]; uint8_t head, tail, count; } ipc_t;
typedef struct { kobject_t base; uint64_t pages; uint64_t* frames; uint32_t width, height; } shm_t;
typedef struct { kobject_t base; input_event_t events[64]; uint8_t head,tail,count; } input_t;
static input_t system_input = { .base = { KOBJ_INPUT, 1, 0 } };

static void ipc_destroy(kobject_t* object) { kfree(object); }
static void shm_destroy(kobject_t* object) {
    shm_t* shm = (shm_t*)object;
    for (uint64_t i = 0; i < shm->pages; i++) pmm_free_page((void*)shm->frames[i]);
    kfree(shm->frames); kfree(shm);
}

int64_t ipc_create(process_t* process) {
    ipc_t* endpoint = kmalloc(sizeof(*endpoint)); if (!endpoint) return -1;
    for (uint64_t i = 0; i < sizeof(*endpoint); i++) ((uint8_t*)endpoint)[i] = 0;
    endpoint->base = (kobject_t){ KOBJ_IPC, 0, ipc_destroy };
    int64_t handle = handle_install(process, &endpoint->base, RIGHT_READ|RIGHT_WRITE|RIGHT_DUP, 1);
    if (handle < 0) ipc_destroy(&endpoint->base); return handle;
}
int64_t ipc_send(process_t* process, uint64_t handle, const void* data, uint64_t len) {
    ipc_t* endpoint = (ipc_t*)handle_get(process, handle, KOBJ_IPC, RIGHT_WRITE);
    if (!endpoint || !len || len > IPC_MESSAGE_BYTES) return -1;
    if (endpoint->count == IPC_MESSAGES) return -2;
    for (uint64_t i=0;i<len;i++) endpoint->data[endpoint->head][i]=((const uint8_t*)data)[i];
    endpoint->len[endpoint->head]=(uint8_t)len; endpoint->head=(endpoint->head+1)%IPC_MESSAGES; endpoint->count++;
    task_wake_channel(&endpoint->count,0); return (int64_t)len;
}
int64_t ipc_receive(process_t* process, uint64_t handle, void* data, uint64_t capacity) {
    ipc_t* endpoint=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_READ); if(!endpoint)return-1; if(!endpoint->count)return-2;
    uint64_t len=endpoint->len[endpoint->tail]; if(len>capacity)len=capacity;
    for(uint64_t i=0;i<len;i++)((uint8_t*)data)[i]=endpoint->data[endpoint->tail][i];
    endpoint->tail=(endpoint->tail+1)%IPC_MESSAGES; endpoint->count--;task_wake_channel(&endpoint->head,0);return (int64_t)len;
}
void* ipc_send_wait_channel(process_t* process,uint64_t handle){ipc_t*e=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_WRITE);return e?&e->head:0;}
void* ipc_receive_wait_channel(process_t* process,uint64_t handle){ipc_t*e=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_READ);return e?&e->count:0;}

static int64_t memory_object_create(process_t* process,uint64_t pages,uint32_t type,uint32_t width,uint32_t height) {
    if(!pages||pages>16384)return-1;
    shm_t* shm=kmalloc(sizeof(*shm)); uint64_t* frames=kmalloc(pages*sizeof(uint64_t));
    if(!shm||!frames){if(shm)kfree(shm);if(frames)kfree(frames);return-1;}
    shm->base=(kobject_t){type,0,shm_destroy};shm->pages=pages;shm->frames=frames;shm->width=width;shm->height=height;
    uint64_t allocated=0;for(;allocated<pages;allocated++){void* frame=pmm_alloc_page();if(!frame)break;frames[allocated]=(uint64_t)frame;for(uint64_t j=0;j<4096;j++)((uint8_t*)frame)[j]=0;}
    if(allocated!=pages){while(allocated)pmm_free_page((void*)frames[--allocated]);kfree(frames);kfree(shm);return-1;}
    uint32_t rights=RIGHT_READ|RIGHT_WRITE|RIGHT_MAP|RIGHT_DUP;if(type==KOBJ_SURFACE)rights|=RIGHT_PRESENT;
    int64_t handle=handle_install(process,&shm->base,rights,1);if(handle<0)shm_destroy(&shm->base);return handle;
}
int64_t shm_create(process_t* process,uint64_t pages){if(pages>256)return-1;return memory_object_create(process,pages,KOBJ_SHM,0,0);}
int64_t shm_map(process_t* process,uint64_t handle,uint64_t va,int writable) {
    shm_t* shm=(shm_t*)handle_get(process,handle,0,RIGHT_MAP);
    if(!shm||(shm->base.type!=KOBJ_SHM&&shm->base.type!=KOBJ_SURFACE)||(va&4095)||va<0x10000||va>=0x0000800000000000ULL||shm->pages>(0x0000800000000000ULL-va)/4096)return-1;
    for(uint64_t i=0;i<shm->pages;i++)if(paging_translate_in(process->cr3,va+i*4096,0))return-1;
    vm_region_t* region=kmalloc(sizeof(*region));if(!region)return-1;uint64_t mapped=0;
    uint64_t flags=1|4|(writable?2:0)|(1ULL<<63)|PAGING_SHARED;
    for(;mapped<shm->pages;mapped++)if(!paging_map_page_in(process->cr3,va+mapped*4096,shm->frames[mapped],flags))break;
    if(mapped!=shm->pages){while(mapped)paging_unmap_page_in(process->cr3,va+(--mapped)*4096,0);kfree(region);return-1;}
    region->start=va;region->length=shm->pages*4096;region->flags=writable?2:0;region->kind=2;region->object=&shm->base;region->next=process->vm_regions;process->vm_regions=region;object_retain(&shm->base);return(int64_t)va;
}
int64_t shm_unmap(process_t* process,uint64_t va) {
    vm_region_t* previous=0,*region=process->vm_regions;while(region&&(region->start!=va||region->kind!=2)){previous=region;region=region->next;}if(!region)return-1;
    for(uint64_t i=0;i<region->length/4096;i++)paging_unmap_page_in(process->cr3,va+i*4096,0);
    if(previous)previous->next=region->next;else process->vm_regions=region->next;kobject_t*object=region->object;kfree(region);object_release(object);return 0;
}
int64_t surface_create(process_t* process,uint32_t width,uint32_t height) {
    if(!width||!height||width>4096||height>4096)return-1;uint64_t bytes=(uint64_t)width*height*sizeof(uint32_t);
    return memory_object_create(process,(bytes+4095)/4096,KOBJ_SURFACE,width,height);
}
int64_t surface_present(process_t* process,uint64_t handle,uint32_t x,uint32_t y) {
    shm_t* surface=(shm_t*)handle_get(process,handle,KOBJ_SURFACE,RIGHT_PRESENT);if(!surface)return-1;
    uint64_t fb_width,fb_height,pitch;uint32_t* framebuffer;fb_get_info(&fb_width,&fb_height,&pitch,&framebuffer);(void)pitch;(void)framebuffer;
    if(x>=fb_width||y>=fb_height)return-1;uint64_t width=surface->width,height=surface->height;if(width>fb_width-x)width=fb_width-x;if(height>fb_height-y)height=fb_height-y;
    for(uint64_t row=0;row<height;row++)for(uint64_t column=0;column<width;column++){uint64_t offset=(row*surface->width+column)*4;uint32_t color=*(uint32_t*)(surface->frames[offset/4096]+(offset&4095));fb_put_pixel(x+column,y+row,color);}
    return 0;
}

int resource_grant_input(process_t* process) {
    return handle_install(process,&system_input.base,RIGHT_READ|RIGHT_DUP,1)<0 ? -1 : 0;
}
void resource_input_push(uint32_t code,int32_t value) {
    if(system_input.count==64){system_input.tail=(system_input.tail+1)%64;system_input.count--;}
    input_event_t* event=&system_input.events[system_input.head];
    event->type=1;event->code=code;event->value=value;event->reserved=0;event->ticks=timer_get_ticks();
    system_input.head=(system_input.head+1)%64;system_input.count++;
    task_wake_channel(&system_input,0);
}
int64_t resource_input_read(process_t* process,uint64_t handle,input_event_t* event) {
    input_t* input=(input_t*)handle_get(process,handle,KOBJ_INPUT,RIGHT_READ);
    if(!input)return-1;if(!input->count)return-2;*event=input->events[input->tail];input->tail=(input->tail+1)%64;input->count--;return sizeof(*event);
}
