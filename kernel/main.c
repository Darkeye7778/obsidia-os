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

static void serial_init(void){outb(0x3f9,0);outb(0x3fb,0x80);outb(0x3f8,3);outb(0x3f9,0);outb(0x3fb,3);outb(0x3fa,0xc7);outb(0x3fc,0x0b);}
void serial_write(const char* text){while(*text)outb(0x3f8,*text++);}
static void serial_u64(uint64_t value){char b[21];int i=20;b[i]=0;if(!value){serial_write("0");return;}while(value&&i){b[--i]=(char)('0'+value%10);value/=10;}serial_write(&b[i]);}

static void idle_task(void){for(;;)task_yield();}

void kmain(void){
    serial_init();serial_write("Starting kernel...\n");
    if(!framebuffer_request.response||framebuffer_request.response->framebuffer_count<1){serial_write("NO FRAMEBUFFER\n");for(;;)__asm__ volatile("hlt");}
    struct limine_framebuffer* fb=framebuffer_request.response->framebuffers[0];
    fb_init((uint32_t*)fb->address,fb->width,fb->height,fb->pitch);fb_clear(0x00202020);display_init();console_init(fb->width,fb->height);console_print("Obsidia kernel starting\n");
    memory_init(memmap_request.response);gdt_init();
    if(module_request.response&&module_request.response->module_count){struct limine_file* initrd=module_request.response->modules[0];initrd_set((uint64_t)initrd->address,initrd->size);serial_write("Initrd module recorded\n");}
    paging_init();heap_init();vfs_init();
    if(module_request.response&&module_request.response->module_count){struct limine_file* initrd=module_request.response->modules[0];if(!vfs_mount_initrd_from((uint64_t)initrd->address,initrd->size))serial_write("VFS: initrd mount failed\n");}
    if(vfs_mount_ramfs("/tmp"))serial_write("VFS: ramfs mount failed\n");
    idt_init();keyboard_init();timer_init();syscall_init();tasking_init();platform_devices_init();
    task_create_kernel_thread(idle_task,"idle");
    enable_interrupts();
    serial_write("MAIN: launching userspace init ELF; free pages=");serial_u64(memory_get_free_pages());serial_write("\n");
    if(task_run_user_program("init.elf"))serial_write("MAIN: userspace init launch failed\n");
    /* Boot policy ends here. Init owns service and shell startup; this context is
       only a guaranteed scheduler participant if init terminates. */
    for(;;)task_yield();
}
