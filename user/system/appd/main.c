#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/app_catalog.h>
#include <obsidia/service.h>
#include <obsidia/internal/app_catalog_protocol.h>

#define CATALOG_INDEX "system/apps/catalog.list"
#define SNAPSHOT_MAP 0x0000005b00000000ULL

static int64_t snapshot_handle=-1;
static obs_app_catalog_snapshot_t*snapshot;
static void write(const char*s){uint32_t n=0;while(s[n])n++;sys_fd_write(1,s,n);}
static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int read_file(const char*path,char*data,uint32_t capacity,uint32_t*length){
    int64_t fd=sys_open(path);if(fd<0)return-1;uint32_t used=0;
    while(used<capacity){int64_t n=sys_read((int)fd,data+used,capacity-used);if(n<0){sys_close((int)fd);return-1;}if(!n)break;used+=(uint32_t)n;}
    char extra;if(used==capacity&&sys_read((int)fd,&extra,1)>0){sys_close((int)fd);return-1;}sys_close((int)fd);*length=used;return 0;
}
static uint32_t trusted_capabilities(const obs_app_metadata_t*r){
    if(same(r->id,"org.obsidia.settings-demo")&&same(r->path,"settings-demo.obsx"))return OBS_APP_CAP_SETTINGS_WRITE;
    return 0;
}
static int duplicate(const obs_app_metadata_t*r){for(uint32_t i=0;i<snapshot->count;i++)if(same(snapshot->records[i].id,r->id)||same(snapshot->records[i].path,r->path))return 1;return 0;}
static void load_manifest(const char*path){
    char data[1024];uint32_t length=0;obs_app_metadata_t record;
    if(snapshot->count>=OBS_APP_CATALOG_MAX||read_file(path,data,sizeof(data),&length)<0||obs_app_manifest_parse(data,length,&record)<0||duplicate(&record)||obs_exec_detect(record.path)==OBS_EXEC_UNKNOWN){write("appd: rejected invalid application manifest\n");return;}
    record.capabilities=trusted_capabilities(&record);snapshot->records[snapshot->count++]=record;
}
static int load_catalog(void){
    char index[1024];uint32_t length=0;if(read_file(CATALOG_INDEX,index,sizeof(index),&length)<0)return-1;
    uint32_t at=0;while(at<length){while(at<length&&(index[at]=='\n'||index[at]=='\r'))at++;uint32_t start=at;while(at<length&&index[at]!='\n'&&index[at]!='\r')at++;uint32_t n=at-start;if(!n||index[start]=='#')continue;if(n>=128){write("appd: ignored oversized catalog path\n");continue;}char path[128];for(uint32_t i=0;i<n;i++)path[i]=index[start+i];path[n]=0;load_manifest(path);}
    return 0;
}
static void respond(int64_t reply,int status,int attach){
    if(reply<0)return;
    obs_app_catalog_response_t r={status,OBS_APP_CATALOG_PROTOCOL_VERSION,snapshot?snapshot->generation:0,snapshot?snapshot->count:0,sizeof(obs_app_metadata_t)};
    if(attach&&status==0)os_ipc_try_send_handle((uint64_t)reply,&r,sizeof(r),(uint64_t)snapshot_handle,OS_RIGHT_READ|OS_RIGHT_MAP);
    else os_ipc_try_send((uint64_t)reply,&r,sizeof(r));
    os_handle_close((uint64_t)reply);
}
int obsidia_main(void){
    uint32_t pages=(uint32_t)((sizeof(obs_app_catalog_snapshot_t)+4095)/4096);if(pages>2)return 1;
    snapshot_handle=os_shm_create(pages);if(snapshot_handle<0)return 2;snapshot=os_shm_map((uint64_t)snapshot_handle,(void*)SNAPSHOT_MAP,1);if(snapshot==(void*)-1)return 3;
    for(uint32_t i=0;i<pages*4096;i++)((uint8_t*)snapshot)[i]=0;
    snapshot->magic=OBS_APP_CATALOG_SNAPSHOT_MAGIC;snapshot->version=OBS_APP_CATALOG_PROTOCOL_VERSION;snapshot->generation=1;
    if(load_catalog()<0)return 4;
    int64_t endpoint=os_ipc_create();if(endpoint<0||os_service_register_restricted(OBS_APP_CATALOG_SERVICE_NAME,(uint64_t)endpoint,OS_RIGHT_WRITE|OS_RIGHT_DUP)<0)return 5;
    write("appd: installed application catalog ready\n");
    for(;;){uint8_t data[64];os_ipc_message_info_t info={-1,0};int64_t n=os_ipc_recv_ex((uint64_t)endpoint,data,sizeof(data),&info,1);if(n==-2){sys_sleep(1);continue;}if(n!=(int64_t)sizeof(obs_app_catalog_request_t)){respond(info.attached,OBS_APP_CATALOG_ERROR,0);continue;}const obs_app_catalog_request_t*q=(const obs_app_catalog_request_t*)data;if(q->version!=OBS_APP_CATALOG_PROTOCOL_VERSION)respond(info.attached,OBS_APP_CATALOG_BAD_VERSION,0);else if(q->operation!=OBS_APP_CATALOG_OP_SNAPSHOT||q->reserved0||q->reserved1)respond(info.attached,OBS_APP_CATALOG_ERROR,0);else respond(info.attached,OBS_APP_CATALOG_OK,1);}
}
