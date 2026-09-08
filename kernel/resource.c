#include "resource.h"
#include "object.h"
#include "memory/heap.h"
#include "memory/memory.h"
#include "paging.h"
#include "drivers/framebuffer.h"
#include "timer.h"
#include "console/console.h"
#include "block.h"
#include "acpi.h"
#include "idt.h"

#define IPC_MESSAGES 8
#define IPC_MESSAGE_BYTES 64
extern void serial_write(const char*);

typedef struct {
    kobject_t base;
    uint8_t data[IPC_MESSAGES][IPC_MESSAGE_BYTES];
    uint8_t len[IPC_MESSAGES];
    kobject_t* attachment[IPC_MESSAGES];
    uint32_t attachment_rights[IPC_MESSAGES];
    uint64_t sender_pid[IPC_MESSAGES];
    uint8_t head, tail, count;
} ipc_t;
typedef struct { kobject_t base; uint64_t pages; uint64_t* frames; uint32_t width, height; } shm_t;
#define INPUT_EVENTS 128
typedef struct { kobject_t base; input_event_t events[INPUT_EVENTS]; uint16_t head,tail,count; uint64_t dropped_motion,coalesced_motion; } input_t;
static input_t system_input = { .base = { KOBJ_INPUT, 1, 0 } };
static kobject_t system_display_output = { KOBJ_DISPLAY_OUTPUT, 1, 0 };
static kobject_t system_control = { KOBJ_SYSTEM_CONTROL, 1, 0 };
static ipc_t* system_service_port;

static void ipc_destroy(kobject_t* object) {
    ipc_t* endpoint=(ipc_t*)object;
    for(uint32_t i=0;i<IPC_MESSAGES;i++)if(endpoint->attachment[i])object_release(endpoint->attachment[i]);
    kfree(object);
}
static void shm_destroy(kobject_t* object) {
    shm_t* shm = (shm_t*)object;
    for (uint64_t i = 0; i < shm->pages; i++) pmm_free_page((void*)shm->frames[i]);
    kfree(shm->frames); kfree(shm);
}

int64_t ipc_create(process_t* process) {
    ipc_t* endpoint = kmalloc(sizeof(*endpoint)); if (!endpoint) return -1;
    for (uint64_t i = 0; i < sizeof(*endpoint); i++) ((uint8_t*)endpoint)[i] = 0;
    endpoint->base = (kobject_t){ KOBJ_IPC, 0, ipc_destroy };
    int64_t handle = handle_install(process, &endpoint->base, RIGHT_READ|RIGHT_WRITE|RIGHT_DUP, 0);
    if (handle < 0) ipc_destroy(&endpoint->base); return handle;
}
int64_t ipc_send(process_t* process, uint64_t handle, const void* data, uint64_t len) {
    ipc_t* endpoint = (ipc_t*)handle_get(process, handle, KOBJ_IPC, RIGHT_WRITE);
    if (!endpoint || !len || len > IPC_MESSAGE_BYTES) return -1;
    if (endpoint->count == IPC_MESSAGES) return -2;
    for (uint64_t i=0;i<len;i++) endpoint->data[endpoint->head][i]=((const uint8_t*)data)[i];
    endpoint->len[endpoint->head]=(uint8_t)len;endpoint->sender_pid[endpoint->head]=process->pid;
    endpoint->head=(endpoint->head+1)%IPC_MESSAGES; endpoint->count++;
    task_wake_channel(&endpoint->count,0); return (int64_t)len;
}
int64_t ipc_send_handle(process_t* process,uint64_t handle,const void* data,uint64_t len,
                        uint64_t attached,uint32_t rights) {
    ipc_t* endpoint=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_WRITE);
    kobject_t* object=0;uint32_t granted=0;
    if(!endpoint||!len||len>IPC_MESSAGE_BYTES||endpoint->count==IPC_MESSAGES||
       handle_export(process,attached,rights,&object,&granted)<0)return endpoint&&endpoint->count==IPC_MESSAGES?-2:-1;
    uint8_t slot=endpoint->head;
    for(uint64_t i=0;i<len;i++)endpoint->data[slot][i]=((const uint8_t*)data)[i];
    endpoint->len[slot]=(uint8_t)len;endpoint->attachment[slot]=object;
    endpoint->attachment_rights[slot]=granted;endpoint->sender_pid[slot]=process->pid;object_retain(object);
    endpoint->head=(endpoint->head+1)%IPC_MESSAGES;endpoint->count++;
    task_wake_channel(&endpoint->count,0);return(int64_t)len;
}
int64_t ipc_receive(process_t* process, uint64_t handle, void* data, uint64_t capacity) {
    ipc_t* endpoint=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_READ); if(!endpoint)return-1; if(!endpoint->count)return-2;
    if(endpoint->attachment[endpoint->tail])return-3;
    uint64_t len=endpoint->len[endpoint->tail]; if(len>capacity)len=capacity;
    for(uint64_t i=0;i<len;i++)((uint8_t*)data)[i]=endpoint->data[endpoint->tail][i];
    endpoint->sender_pid[endpoint->tail]=0;endpoint->tail=(endpoint->tail+1)%IPC_MESSAGES;
    endpoint->count--;task_wake_channel(&endpoint->head,0);return (int64_t)len;
}
int64_t ipc_receive_handle(process_t* process,uint64_t handle,void* data,uint64_t capacity,
                           int64_t* attached_out) {
    ipc_t* endpoint=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_READ);if(!endpoint)return-1;if(!endpoint->count)return-2;
    uint8_t slot=endpoint->tail;kobject_t* object=endpoint->attachment[slot];if(!object)return-3;
    int64_t installed=handle_install(process,object,endpoint->attachment_rights[slot],0);if(installed<0)return-4;
    uint64_t len=endpoint->len[slot];if(len>capacity)len=capacity;
    for(uint64_t i=0;i<len;i++)((uint8_t*)data)[i]=endpoint->data[slot][i];
    endpoint->attachment[slot]=0;endpoint->attachment_rights[slot]=0;endpoint->sender_pid[slot]=0;object_release(object);
    endpoint->tail=(endpoint->tail+1)%IPC_MESSAGES;endpoint->count--;task_wake_channel(&endpoint->head,0);
    *attached_out=installed;return(int64_t)len;
}
int64_t ipc_receive_ex(process_t* process,uint64_t handle,void* data,uint64_t capacity,
                       int64_t* attached_out,uint64_t* sender_pid_out) {
    ipc_t* endpoint=(ipc_t*)handle_get(process,handle,KOBJ_IPC,RIGHT_READ);
    if(!endpoint||!attached_out||!sender_pid_out)return-1;
    if(!endpoint->count)return-2;
    uint8_t slot=endpoint->tail;
    kobject_t* object=endpoint->attachment[slot];
    int64_t installed=-1;
    if(object){
        installed=handle_install(process,object,endpoint->attachment_rights[slot],0);
        if(installed<0)return-4;
    }
    uint64_t len=endpoint->len[slot];if(len>capacity)len=capacity;
    for(uint64_t i=0;i<len;i++)((uint8_t*)data)[i]=endpoint->data[slot][i];
    *attached_out=installed;*sender_pid_out=endpoint->sender_pid[slot];
    if(object){endpoint->attachment[slot]=0;endpoint->attachment_rights[slot]=0;object_release(object);}
    endpoint->sender_pid[slot]=0;endpoint->tail=(endpoint->tail+1)%IPC_MESSAGES;endpoint->count--;
    task_wake_channel(&endpoint->head,0);return(int64_t)len;
}
int64_t service_port_open(process_t* process) {
    if(!system_service_port){
        system_service_port=kmalloc(sizeof(*system_service_port));if(!system_service_port)return-1;
        for(uint64_t i=0;i<sizeof(*system_service_port);i++)((uint8_t*)system_service_port)[i]=0;
        system_service_port->base=(kobject_t){KOBJ_IPC,1,ipc_destroy}; /* kernel bootstrap pin */
    }
    return handle_install(process,&system_service_port->base,RIGHT_READ|RIGHT_WRITE|RIGHT_DUP,0);
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
    uint32_t rights=RIGHT_READ|RIGHT_WRITE|RIGHT_MAP|RIGHT_DUP;
    int64_t handle=handle_install(process,&shm->base,rights,0);if(handle<0)shm_destroy(&shm->base);return handle;
}
int64_t shm_create(process_t* process,uint64_t pages){if(pages>256)return-1;return memory_object_create(process,pages,KOBJ_SHM,0,0);}
int64_t shm_map(process_t* process,uint64_t handle,uint64_t va,int writable) {
    uint32_t required=RIGHT_MAP|(writable?RIGHT_WRITE:RIGHT_READ);
    shm_t* shm=(shm_t*)handle_get(process,handle,0,required);
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
int64_t surface_present(process_t* process,uint64_t output_handle,uint64_t surface_handle,uint32_t x,uint32_t y) {
    if(!handle_get(process,output_handle,KOBJ_DISPLAY_OUTPUT,RIGHT_PRESENT))return-1;
    shm_t* surface=(shm_t*)handle_get(process,surface_handle,KOBJ_SURFACE,RIGHT_READ);if(!surface)return-1;
    uint64_t fb_width,fb_height,pitch;uint32_t* framebuffer;fb_get_info(&fb_width,&fb_height,&pitch,&framebuffer);(void)pitch;(void)framebuffer;
    if(x>=fb_width||y>=fb_height)return-1;console_set_framebuffer_enabled(0);uint64_t width=surface->width,height=surface->height;if(width>fb_width-x)width=fb_width-x;if(height>fb_height-y)height=fb_height-y;
    for(uint64_t row=0;row<height;row++)for(uint64_t column=0;column<width;column++){uint64_t offset=(row*surface->width+column)*4;uint32_t color=*(uint32_t*)(surface->frames[offset/4096]+(offset&4095));fb_put_pixel(x+column,y+row,color);}
    return 0;
}

int resource_grant_input(process_t* process) {
    return handle_install(process,&system_input.base,RIGHT_READ|RIGHT_DUP,0)<0 ? -1 : 0;
}
int resource_grant_display_output(process_t* process) {
    return handle_install(process,&system_display_output,RIGHT_PRESENT|RIGHT_DUP,0)<0?-1:0;
}
int resource_grant_system_control(process_t*process){return handle_install(process,&system_control,RIGHT_WRITE|RIGHT_DUP,0)<0?-1:0;}
int resource_system_control(process_t*process,uint64_t handle,uint32_t action){
    if(!handle_get(process,handle,KOBJ_SYSTEM_CONTROL,RIGHT_WRITE)||(action!=1&&action!=2))return-1;
    serial_write(action==1?"POWER: synchronized shutdown requested\n":"POWER: synchronized reboot requested\n");
    if(block_flush_all())serial_write("POWER: block flush reported an error\n");
    disable_interrupts();int result=action==1?acpi_power_off():acpi_reboot();
    if(result==0)for(;;)__asm__ volatile("hlt");enable_interrupts();return result;
}
static int32_t motion_sum(int32_t a,int32_t b){int64_t sum=(int64_t)a+b;if(sum>32767)return 32767;if(sum<-32768)return-32768;return(int32_t)sum;}
static int input_motion(const input_event_t*event){return event->type==INPUT_EVENT_MOUSE_MOVE||event->type==INPUT_EVENT_RELATIVE;}
static void input_remove(uint16_t offset){
    for(uint16_t i=offset;i+1<system_input.count;i++){uint16_t here=(system_input.tail+i)%INPUT_EVENTS,next=(system_input.tail+i+1)%INPUT_EVENTS;system_input.events[here]=system_input.events[next];}
    system_input.head=(system_input.head+INPUT_EVENTS-1)%INPUT_EVENTS;system_input.count--;
}
static int evict_motion(void){for(uint16_t i=0;i<system_input.count;i++)if(input_motion(&system_input.events[(system_input.tail+i)%INPUT_EVENTS])){input_remove(i);system_input.dropped_motion++;return 1;}return 0;}
void resource_input_push(uint32_t type,uint32_t code,int32_t value) {
    if(type==INPUT_EVENT_RELATIVE&&system_input.count){uint16_t last=(system_input.head+INPUT_EVENTS-1)%INPUT_EVENTS;input_event_t*previous=&system_input.events[last];if(previous->type==type&&previous->code==code){previous->value=motion_sum(previous->value,value);previous->ticks=timer_get_ticks();system_input.coalesced_motion++;task_wake_channel(&system_input,0);return;}}
    if(system_input.count==INPUT_EVENTS){if(type==INPUT_EVENT_RELATIVE){system_input.dropped_motion++;return;}if(!evict_motion()){system_input.tail=(system_input.tail+1)%INPUT_EVENTS;system_input.count--;}}
    input_event_t* event=&system_input.events[system_input.head];
    event->type=type;event->code=code;event->value=value;event->reserved=0;event->ticks=timer_get_ticks();
    system_input.head=(system_input.head+1)%INPUT_EVENTS;system_input.count++;
    task_wake_channel(&system_input,0);
}
void resource_input_push_motion(int32_t dx,int32_t dy){
    if(!dx&&!dy)return;
    if(system_input.count){uint16_t last=(system_input.head+INPUT_EVENTS-1)%INPUT_EVENTS;input_event_t*previous=&system_input.events[last];if(previous->type==INPUT_EVENT_MOUSE_MOVE){previous->value=motion_sum(previous->value,dx);previous->reserved=(uint32_t)motion_sum((int32_t)previous->reserved,dy);previous->ticks=timer_get_ticks();system_input.coalesced_motion++;task_wake_channel(&system_input,0);return;}}
    if(system_input.count==INPUT_EVENTS){system_input.dropped_motion++;return;}
    input_event_t*event=&system_input.events[system_input.head];event->type=INPUT_EVENT_MOUSE_MOVE;event->code=0;event->value=dx;event->reserved=(uint32_t)dy;event->ticks=timer_get_ticks();system_input.head=(system_input.head+1)%INPUT_EVENTS;system_input.count++;task_wake_channel(&system_input,0);
}
int64_t resource_input_read(process_t* process,uint64_t handle,input_event_t* event) {
    input_t* input=(input_t*)handle_get(process,handle,KOBJ_INPUT,RIGHT_READ);
    if(!input)return-1;if(!input->count)return-2;*event=input->events[input->tail];input->tail=(input->tail+1)%INPUT_EVENTS;input->count--;return sizeof(*event);
}
uint64_t resource_input_dropped_motion(void){return system_input.dropped_motion;}
uint64_t resource_input_coalesced_motion(void){return system_input.coalesced_motion;}
int resource_input_self_test(void){
    system_input.head=system_input.tail=system_input.count=0;system_input.dropped_motion=system_input.coalesced_motion=0;
    for(int i=0;i<200;i++)resource_input_push_motion(1,-1);resource_input_push(INPUT_EVENT_BUTTON,INPUT_BUTTON_LEFT,1);for(int i=0;i<200;i++)resource_input_push_motion(-1,1);resource_input_push(INPUT_EVENT_BUTTON,INPUT_BUTTON_LEFT,0);
    if(system_input.count!=4||system_input.events[0].type!=INPUT_EVENT_MOUSE_MOVE||system_input.events[0].value!=200||(int32_t)system_input.events[0].reserved!=-200||system_input.events[1].type!=INPUT_EVENT_BUTTON||system_input.events[2].value!=-200||system_input.events[3].value!=0)return 0;
    system_input.head=system_input.tail=system_input.count=0;
    for(uint32_t i=0;i<INPUT_EVENTS;i++){if(i&1)resource_input_push(INPUT_EVENT_KEY,'a',1);else resource_input_push_motion(1,0);}resource_input_push(INPUT_EVENT_BUTTON,INPUT_BUTTON_RIGHT,1);
    int found=0;for(uint16_t i=0;i<system_input.count;i++){input_event_t*event=&system_input.events[(system_input.tail+i)%INPUT_EVENTS];if(event->type==INPUT_EVENT_BUTTON&&event->code==INPUT_BUTTON_RIGHT)found=1;}
    system_input.head=system_input.tail=system_input.count=0;system_input.dropped_motion=system_input.coalesced_motion=0;return found;
}
