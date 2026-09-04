#include "syscall.h"
static const char banner[]="Obsidia userspace shell (type hello or exit)\n$ ";
static const char prompt[]="$ ";
static int eq(const char*a,const char*b){int i=0;while(a[i]&&b[i]&&a[i]==b[i])i++;return a[i]==b[i];}
int obsidia_main(void){char line[64];int n=0;int64_t input=os_handle_find(OS_OBJECT_INPUT);if(input<0)__asm__ volatile("ud2");sys_fd_write(1,banner,sizeof(banner)-1);for(;;){os_input_event_t event;if(os_input_read((uint64_t)input,&event)!=(int64_t)sizeof(event)||event.type!=1)continue;char c=(char)event.code;if(c=='\n'||c=='\r'){sys_fd_write(1,"\n",1);line[n]=0;if(eq(line,"exit"))return 0;if(eq(line,"hello")){int64_t p=sys_spawn("hello.elf");if(p>0){int64_t s;sys_wait((uint64_t)p,&s);}}n=0;sys_fd_write(1,prompt,sizeof(prompt)-1);}else if(event.code<256&&n<63){line[n++]=c;sys_fd_write(1,&c,1);}}}
