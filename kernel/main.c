#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "drivers/framebuffer.h"
#include "drivers/keyboard.h"
#include "console/console.h"
#include "memory/memory.h"
#include "memory/heap.h"
#include "initrd/initrd.h"
#include "vfs/vfs.h"
#include "idt.h"
#include "timer.h"
#include "paging.h"
#include "syscall.h"
#include "gdt.h"
#include "task.h"
#include "display.h"
#include "device.h"
#include "block.h"
#include "partition.h"
#include "resource.h"
#include "acpi.h"
#include "apic.h"

__attribute__((used,section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request={
    .id=LIMINE_FRAMEBUFFER_REQUEST,.revision=0
};
__attribute__((used,section(".limine_requests")))
volatile struct limine_module_request module_request={
    .id=LIMINE_MODULE_REQUEST,.revision=0
};
__attribute__((used,section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request={
    .id=LIMINE_MEMMAP_REQUEST,.revision=0
};
__attribute__((used,section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request={.id=LIMINE_RSDP_REQUEST,.revision=0};
__attribute__((used,section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request={.id=LIMINE_HHDM_REQUEST,.revision=0};

static void serial_init(void){outb(0x3f9,0);outb(0x3fb,0x80);outb(0x3f8,3);outb(0x3f9,0);outb(0x3fb,3);outb(0x3fa,0xc7);outb(0x3fc,0x0b);}
void serial_write(const char* text){while(*text)outb(0x3f8,*text++);}
static void serial_u64(uint64_t value){char b[21];int i=20;b[i]=0;if(!value){serial_write("0");return;}while(value&&i){b[--i]=(char)('0'+value%10);value/=10;}serial_write(&b[i]);}

static void idle_task(void){for(;;)task_yield();}

void kmain(void){
    serial_init();serial_write("Starting kernel...\n");
    if(!rsdp_request.response||!hhdm_request.response||
       acpi_init(rsdp_request.response->address,hhdm_request.response->offset,memmap_request.response))serial_write("ACPI: discovery unavailable\n");
    if(!framebuffer_request.response||framebuffer_request.response->framebuffer_count<1){serial_write("NO FRAMEBUFFER\n");for(;;)__asm__ volatile("hlt");}
    struct limine_framebuffer* fb=framebuffer_request.response->framebuffers[0];
    fb_init((uint32_t*)fb->address,fb->width,fb->height,fb->pitch);fb_clear(0x00202020);display_init();console_init(fb->width,fb->height);console_print("Obsidia kernel starting\n");
    memory_init(memmap_request.response);gdt_init();
    if(module_request.response&&module_request.response->module_count){struct limine_file* initrd=module_request.response->modules[0];initrd_set((uint64_t)initrd->address,initrd->size);serial_write("Initrd module recorded\n");}
    paging_init();heap_init();vfs_init();
    if(module_request.response&&module_request.response->module_count){struct limine_file* initrd=module_request.response->modules[0];if(!vfs_mount_initrd_from((uint64_t)initrd->address,initrd->size))serial_write("VFS: initrd mount failed\n");}
    if(vfs_mount_ramfs("/tmp"))serial_write("VFS: ramfs mount failed\n");
    idt_init();apic_init();keyboard_init();timer_init();syscall_init();tasking_init();enable_interrupts();
    serial_write(resource_input_self_test()?"INPUT: coalescing/order self-test passed\n":"INPUT: coalescing/order self-test FAILED\n");
    platform_devices_init();
    block_device_t*state_disk=0,*raw_fallback=0;int saw_invalid_table=0,saw_partitioned_disk=0;
    for(block_device_t*physical=block_first_device();physical;physical=physical->next){
        if(physical->parent||(physical->type!=BLOCK_TYPE_ATA&&physical->type!=BLOCK_TYPE_AHCI))continue;int partitions=block_scan_partitions(physical);
        if(partitions>0){saw_partitioned_disk=1;block_device_t*candidate=block_find_partition_by_type(physical,OBSIDIA_STATE_PARTITION_TYPE_GUID);if(candidate&&!state_disk)state_disk=candidate;}
        else if(partitions==0&&!raw_fallback)raw_fallback=physical;else if(partitions<0)saw_invalid_table=1;
    }
    if(state_disk)serial_write("PARTITION: Obsidia state volume selected\n");
    else if(!saw_partitioned_disk&&raw_fallback){state_disk=raw_fallback;serial_write("PARTITION: legacy whole-disk state volume\n");}
    else if(saw_invalid_table)serial_write("PARTITION: invalid disk table; refusing raw fallback\n");
    else serial_write("PARTITION: no Obsidia state volume\n");
    if(!state_disk||vfs_mount_statefs("/state",state_disk))serial_write("STATEFS: persistent state unavailable\n");
    task_create_kernel_thread(idle_task,"idle");
    serial_write("MAIN: launching userspace init ELF; free pages=");serial_u64(memory_get_free_pages());serial_write("\n");
    if(task_run_user_program("init.elf"))serial_write("MAIN: userspace init launch failed\n");
    /* Boot policy ends here. Init owns service and shell startup; this context is
       only a guaranteed scheduler participant if init terminates. */
    for(;;)task_yield();
}
