#include "syscall.h"
#include <obsidia/internal/service_protocol.h>

#define SERVICE_CAPACITY 16
typedef struct {char name[32];int64_t endpoint;} service_entry_t;
static service_entry_t services[SERVICE_CAPACITY];

static int equal(const char*a,const char*b){for(uint32_t i=0;i<32;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}return 0;}
static int find(const char*name){for(int i=0;i<SERVICE_CAPACITY;i++)if(services[i].endpoint>0&&equal(services[i].name,name))return i;return-1;}
static int empty(void){for(int i=0;i<SERVICE_CAPACITY;i++)if(services[i].endpoint<=0)return i;return-1;}

int obsidia_main(void){
    for(int i=0;i<SERVICE_CAPACITY;i++)services[i].endpoint=-1;
    int64_t port=os_service_port_open();if(port<0)return 1;
    static const char ready[]="serviced: userspace registry ready\n";sys_fd_write(1,ready,sizeof(ready)-1);
    for(;;){
        obs_service_request_t request;int64_t attached=-1;
        if(os_ipc_recv_handle((uint64_t)port,&request,sizeof(request),&attached)!=(int64_t)sizeof(request))continue;
        request.name[sizeof(request.name)-1]=0;
        if(request.operation==OBS_SERVICE_REGISTER){
            int slot=find(request.name);if(slot<0)slot=empty();
            if(slot>=0){uint32_t i=0;for(;i<sizeof(services[slot].name)-1&&request.name[i];i++)services[slot].name[i]=request.name[i];services[slot].name[i]=0;services[slot].endpoint=attached;}
            else os_handle_close((uint64_t)attached);
        }else if(request.operation==OBS_SERVICE_CONNECT){
            int slot=find(request.name);uint8_t response=slot>=0?0:1;
            if(slot>=0)os_ipc_send_handle((uint64_t)attached,&response,1,(uint64_t)services[slot].endpoint,
                OS_RIGHT_READ|OS_RIGHT_WRITE|OS_RIGHT_DUP);
            else os_ipc_send((uint64_t)attached,&response,1);
            os_handle_close((uint64_t)attached);
        }else os_handle_close((uint64_t)attached);
    }
}
