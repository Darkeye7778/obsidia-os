#include <obsidia/service.h>
#include <obsidia/internal/service_protocol.h>
#include "syscall.h"

int os_service_register(const char* name,uint64_t endpoint) {
    if(!name||!name[0])return-1;
    obs_service_request_t request={OBS_SERVICE_REGISTER,{0}};
    uint32_t i=0;for(;i<sizeof(request.name)-1&&name[i];i++)request.name[i]=name[i];
    if(name[i])return-1;
    int64_t port=os_service_port_open();if(port<0)return-1;
    int64_t sent=os_ipc_send_handle((uint64_t)port,&request,sizeof(request),endpoint,
        OS_RIGHT_READ|OS_RIGHT_WRITE|OS_RIGHT_DUP);
    os_handle_close((uint64_t)port);return sent==(int64_t)sizeof(request)?0:-1;
}

int64_t os_service_connect(const char* name) {
    if(!name||!name[0])return-1;
    obs_service_request_t request={OBS_SERVICE_CONNECT,{0}};
    uint32_t i=0;for(;i<sizeof(request.name)-1&&name[i];i++)request.name[i]=name[i];
    if(name[i])return-1;
    int64_t port=os_service_port_open(),reply=os_ipc_create();
    if(port<0||reply<0){if(port>=0)os_handle_close((uint64_t)port);if(reply>=0)os_handle_close((uint64_t)reply);return-1;}
    if(os_ipc_send_handle((uint64_t)port,&request,sizeof(request),(uint64_t)reply,
       OS_RIGHT_READ|OS_RIGHT_WRITE|OS_RIGHT_DUP)!=(int64_t)sizeof(request)){
        os_handle_close((uint64_t)port);os_handle_close((uint64_t)reply);return-1;
    }
    int64_t service=-1;uint8_t response=0;
    int64_t received=os_ipc_recv_handle((uint64_t)reply,&response,1,&service);
    os_handle_close((uint64_t)port);os_handle_close((uint64_t)reply);
    return received==1&&response==0?service:-1;
}
