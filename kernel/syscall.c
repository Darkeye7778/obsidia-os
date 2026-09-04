#include "syscall.h"
#include "idt.h"
#include "console/console.h"
#include "drivers/framebuffer.h"
#include "timer.h"
#include "task.h"
#include "paging.h"
#include "usercopy.h"
#include "drivers/keyboard.h"
#include "vfs/vfs.h"
#include "memory/memory.h"
#include "memory/heap.h"
#include "object.h"
#include "resource.h"
#include <stdint.h>

extern void serial_write(const char* str);
static uint8_t logged_vfs_read, logged_input_block, logged_sleep, logged_wait;

static int64_t console_write_user(uint64_t src, uint64_t len) {
    char buf[256]; uint64_t done=0;
    while(done<len) { uint64_t n=len-done; if(n>sizeof(buf)) n=sizeof(buf);
        if(!copy_from_user(buf,src+done,n)) return -1;
        for(uint64_t i=0;i<n;i++) console_putc(buf[i]); done+=n;
    }
    return (int64_t)done;
}

static int64_t vfs_write_user(open_file_t* f,uint64_t src,uint64_t len) {
    char buf[256]; uint64_t done=0;
    while(done<len) { uint64_t want=len-done; if(want>sizeof(buf)) want=sizeof(buf);
        if(!copy_from_user(buf,src+done,want)) return done?(int64_t)done:-1;
        int64_t got=vfs_file_write(f,buf,want); if(got<=0) return done?(int64_t)done:got;
        done+=(uint64_t)got; if((uint64_t)got<want) break;
    } return (int64_t)done;
}

static int64_t vfs_read_user(open_file_t* f,uint64_t dst,uint64_t len) {
    char buf[256]; uint64_t done=0;
    while(done<len) { uint64_t want=len-done; if(want>sizeof(buf)) want=sizeof(buf);
        int64_t got=vfs_file_read(f,buf,want); if(got<=0) return done?(int64_t)done:got;
        if(!copy_to_user(dst+done,buf,(uint64_t)got)) return done?(int64_t)done:-1;
        done+=(uint64_t)got; if((uint64_t)got<want) break;
    } return (int64_t)done;
}

static void syscall_handler(registers_t* regs) {
    uint64_t num = regs->rax;

    switch (num) {
        case SYS_EXIT:
            serial_write("USER: SYS_EXIT\n");
            console_print("\n[syscall] task exit\n");
            task_terminate_current((int64_t)regs->rdi, 0);
            break;

        case SYS_WRITE: {
            const char* buf = (const char*)regs->rdi;
            uint64_t len = regs->rsi;
            if (!current_task || current_task->ring != 3 || len > 65536 ||
                (len && !paging_user_range_valid(current_task->process->cr3, (uint64_t)buf, len, 0))) {
                regs->rax = (uint64_t)-1;
                break;
            }
            regs->rax = (uint64_t)console_write_user((uint64_t)buf,len);
            break;
        }

        case SYS_GETTICKS:
            regs->rax = timer_get_ticks();
            break;

        case SYS_YIELD:
            regs->rax = 0;
            task_request_reschedule();
            break;

        case SYS_FBINFO: {
            fb_info_t* out = (fb_info_t*)regs->rdi;
            if (current_task && paging_user_range_valid(current_task->process->cr3,
                    (uint64_t)out, sizeof(*out), 1)) {
                uint64_t w=0, h=0, p=0;
                uint32_t* addr = 0;
                fb_get_info(&w, &h, &p, &addr);
                fb_info_t info={w,h,p,0}; (void)addr;
                regs->rax = copy_to_user((uint64_t)out,&info,sizeof(info)) ? 0 : (uint64_t)-1;
            } else {
                regs->rax = (uint64_t)-1;
            }
            break;
        }

        case SYS_FD_WRITE: {
            int fd=(int)regs->rdi; uint64_t u=regs->rsi, len=regs->rdx;
            process_t* p=current_task?current_task->process:0;
            if (!p || fd<0 || fd>=MAX_FDS || !p->fds[fd].used || len>65536 ||
                (len && !paging_user_range_valid(p->cr3,u,len,0))) { regs->rax=(uint64_t)-1; break; }
            if (fd==1 || fd==2) { regs->rax=(uint64_t)console_write_user(u,len); break; }
            open_file_t* f=(open_file_t*)p->fds[fd].object;
            regs->rax=(uint64_t)vfs_write_user(f,u,len); break;
        }
        case SYS_FD_READ: {
            int fd=(int)regs->rdi; uint64_t u=regs->rsi, len=regs->rdx;
            process_t* p=current_task?current_task->process:0;
            if (!p || fd<0 || fd>=MAX_FDS || !p->fds[fd].used || len>65536 ||
                (len && !paging_user_range_valid(p->cr3,u,len,1))) { regs->rax=(uint64_t)-1; break; }
            if (fd==0) {
                int key=keyboard_try_getkey();
                if (!key) { if(!logged_input_block){serial_write("FD: stdin reader blocked\n");logged_input_block=1;} regs->rip-=2; task_block_current(1,0); break; }
                char c=(char)key; copy_to_user(u,&c,1); regs->rax=1; break;
            }
            open_file_t* f=(open_file_t*)p->fds[fd].object;
            int64_t r=vfs_read_user(f,u,len);
            if(r>=0 && !logged_vfs_read){serial_write("FD: userspace VFS read completed\n");logged_vfs_read=1;}
            regs->rax=(uint64_t)r; break;
        }
        case SYS_OPEN: {
            char path[128]; if(!copy_string_from_user(path,regs->rdi,sizeof(path))){regs->rax=(uint64_t)-1;break;}
            uint32_t flags=(uint32_t)regs->rsi;if(flags&~(VFS_OPEN_CREATE|VFS_OPEN_TRUNC)){regs->rax=(uint64_t)-1;break;}
            open_file_t* n=vfs_open_file(path,flags); if(!n){regs->rax=(uint64_t)-1;break;}
            process_t* p=current_task->process;
            int fd; for(fd=3;fd<MAX_FDS && p->fds[fd].used;fd++); if(fd==MAX_FDS){regs->rax=(uint64_t)-1;break;}
            p->fds[fd].used=1; p->fds[fd].object=n; p->fds[fd].offset=0; regs->rax=fd; break;
        }
        case SYS_CLOSE: {
            process_t* p=current_task->process; int fd=(int)regs->rdi; if(fd<3||fd>=MAX_FDS||!p->fds[fd].used){regs->rax=(uint64_t)-1;break;}
            vfs_file_release((open_file_t*)p->fds[fd].object); p->fds[fd].used=0; p->fds[fd].object=0; regs->rax=0; break;
        }
        case SYS_SLEEP:
            if(!logged_sleep){serial_write("SCHED: userspace timer sleep blocked\n");logged_sleep=1;}
            task_block_current(2,timer_get_ticks()+regs->rdi); regs->rax=0; break;
        case SYS_SPAWN: {
            char path[128];
            if(!copy_string_from_user(path,regs->rdi,sizeof(path))) { regs->rax=(uint64_t)-1; break; }
            regs->rax=(uint64_t)process_spawn(path,current_task->process->pid); break;
        }
        case SYS_WAIT: {
            int64_t status=0; int r=task_wait_child(regs->rdi,&status);
            if(r==0) { if(!logged_wait){serial_write("PROC: parent blocked waiting for child\n");logged_wait=1;} regs->rip-=2; task_block_current(3,regs->rdi); break; }
            if(r<0 || (regs->rsi && !copy_to_user(regs->rsi,&status,sizeof(status)))) regs->rax=(uint64_t)-1;
            else regs->rax=regs->rdi;
            break;
        }
        case SYS_GETPID: regs->rax=task_get_current_pid(); break;
        case SYS_VM_MAP: {
            process_t* p=current_task->process; uint64_t va=regs->rdi,pages=regs->rsi;
            if(!va) va=0x0000001000000000ULL;
            if(!pages||pages>4096||(va&4095)||va<0x10000||va>=0x0000800000000000ULL||
               pages>(0x0000800000000000ULL-va)/4096){regs->rax=(uint64_t)-1;break;}
            uint64_t len=pages*4096; int overlap=0;
            for(vm_region_t* v=p->vm_regions;v;v=v->next)
                if(va<v->start+v->length&&v->start<va+len){overlap=1;break;}
            if(overlap){regs->rax=(uint64_t)-1;break;}
            vm_region_t* vr=kmalloc(sizeof(*vr)); if(!vr){regs->rax=(uint64_t)-1;break;}
            uint64_t done=0;
            for(;done<pages;done++){void* frame=pmm_alloc_page(); if(!frame)break;
                for(int i=0;i<4096;i++)((uint8_t*)frame)[i]=0;
                uint64_t fl=1|4|(regs->rdx?2:0)|(1ULL<<63);
                if(!paging_map_page_in(p->cr3,va+done*4096,(uint64_t)frame,fl)){pmm_free_page(frame);break;}}
            if(done!=pages){while(done){done--;uint64_t ph=paging_unmap_page_in(p->cr3,va+done*4096,0);if(ph)pmm_free_page((void*)ph);}kfree(vr);regs->rax=(uint64_t)-1;break;}
            vr->start=va;vr->length=len;vr->flags=regs->rdx?2:0;vr->kind=1;vr->object=0;vr->next=p->vm_regions;p->vm_regions=vr;regs->rax=va;break;
        }
        case SYS_VM_UNMAP: {
            process_t* p=current_task->process;uint64_t va=regs->rdi,pages=regs->rsi;
            vm_region_t* prev=0,*v=p->vm_regions;
            while(v&&(v->start!=va||v->length!=pages*4096)){prev=v;v=v->next;}
            if(!v){regs->rax=(uint64_t)-1;break;}
            for(uint64_t i=0;i<pages;i++){uint64_t ph=paging_unmap_page_in(p->cr3,va+i*4096,0);if(ph)pmm_free_page((void*)ph);}
            if(prev)prev->next=v->next;else p->vm_regions=v->next;kfree(v);regs->rax=0;break;
        }
        case SYS_HANDLE_CLOSE: regs->rax=(uint64_t)handle_close(current_task->process,regs->rdi);break;
        case SYS_HANDLE_FIND: regs->rax=(uint64_t)handle_find(current_task->process,(uint32_t)regs->rdi);break;
        case SYS_IPC_CREATE: regs->rax=(uint64_t)ipc_create(current_task->process);break;
        case SYS_IPC_SEND:{char b[64];if(!regs->rdx||regs->rdx>64||!copy_from_user(b,regs->rsi,regs->rdx)){regs->rax=(uint64_t)-1;break;}int64_t n=ipc_send(current_task->process,regs->rdi,b,regs->rdx);if(n==-2){void*c=ipc_send_wait_channel(current_task->process,regs->rdi);if(!c){regs->rax=(uint64_t)-1;break;}regs->rip-=2;task_block_on(c);break;}regs->rax=(uint64_t)n;break;}
        case SYS_IPC_RECV:{char b[64];uint64_t cap=regs->rdx;if(!cap||cap>64){regs->rax=(uint64_t)-1;break;}int64_t n=ipc_receive(current_task->process,regs->rdi,b,cap);if(n==-2){void*c=ipc_receive_wait_channel(current_task->process,regs->rdi);if(!c){regs->rax=(uint64_t)-1;break;}regs->rip-=2;task_block_on(c);break;}if(n>0&&!copy_to_user(regs->rsi,b,(uint64_t)n))n=-1;regs->rax=(uint64_t)n;break;}
        case SYS_SHM_CREATE:regs->rax=(uint64_t)shm_create(current_task->process,regs->rdi);break;
        case SYS_SHM_MAP:regs->rax=(uint64_t)shm_map(current_task->process,regs->rdi,regs->rsi,(int)regs->rdx);break;
        case SYS_SHM_UNMAP:regs->rax=(uint64_t)shm_unmap(current_task->process,regs->rdi);break;
        case SYS_SURFACE_CREATE:regs->rax=(uint64_t)surface_create(current_task->process,(uint32_t)regs->rdi,(uint32_t)regs->rsi);break;
        case SYS_SURFACE_PRESENT:regs->rax=(uint64_t)surface_present(current_task->process,regs->rdi,(uint32_t)regs->rsi,(uint32_t)regs->rdx);break;
        case SYS_INPUT_READ:{input_event_t event;int64_t n=resource_input_read(current_task->process,regs->rdi,&event);if(n==-2){kobject_t*o=handle_get(current_task->process,regs->rdi,KOBJ_INPUT,RIGHT_READ);if(!o){regs->rax=(uint64_t)-1;break;}regs->rip-=2;task_block_on(o);break;}if(n>0&&!copy_to_user(regs->rsi,&event,sizeof(event)))n=-1;regs->rax=(uint64_t)n;break;}

        default:
            regs->rax = (uint64_t)-1;
            break;
    }
}

extern void* isr_stub_table[256];

void syscall_init(void) {
    idt_set_handler(128, syscall_handler);

    // Set the IDT gate for 0x80 to point to the asm stub, but with DPL=3 so ring3 can use int $0x80
    idt_set_user_interrupt_gate(128, (uint64_t)isr_stub_table[128]);

    console_print("Syscall interface registered (int 0x80, DPL=3)\n");
}

// Demo: run a tiny user-mode snippet that exercises syscalls (draws via future or writes).
// This is the "proof" that a GUI can be a user program using the base syscalls.
void run_user_demo(void) {
    // Tiny x86_64 code blob that does a few writes and then exits via syscall.
    // The blob lives in kernel memory but we will iret to it with user CS/SS (DPL3 selectors).
    // If GDT doesn't have perfect user desc, it may GP; the demo still shows the intent.

    static const uint8_t user_blob[] __attribute__((aligned(16))) = {
        // mov $1, %rax          ; SYS_WRITE
        0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00,
        // mov $msg, %rdi
        0x48, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00,   // placeholder, will patch or use rsp relative
        // mov $len, %rsi
        0x48, 0xc7, 0xc6, 0x0e, 0x00, 0x00, 0x00,
        // int $0x80
        0xcd, 0x80,
        // mov $0, %rax ; exit
        0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00,
        // int $0x80
        0xcd, 0x80,
        // hang
        0xeb, 0xfe,
        // data: "Hello from user\n"
        'H','e','l','l','o',' ','f','r','o','m',' ','u','s','e','r','\n', 0
    };

    // For a minimal working demo without full GDT/ring3 pain in this pass, we execute the blob in kernel
    // context but pretend it's the "user entry point" the GUI developer will use the same syscall numbers.
    // Real ring3 + proper GDT + TSS + per task user stack is the immediate follow-up (see ROADMAP).
    console_print("[base] user demo path ready (syscalls defined). Full ring3 transition requires GDT/TSS work.\n");
    console_print("Demo blob prepared for future loader + iretq.\n");
}
