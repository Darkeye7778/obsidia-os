#include <obsidia/app_catalog.h>
#include <obsidia/service.h>
#include <obsidia/internal/app_catalog_protocol.h>
#include "syscall.h"

#define CATALOG_MAP 0x0000005a00000000ULL
static void clear_records(obs_app_catalog_t*c){c->generation=c->count=0;for(uint32_t i=0;i<sizeof(c->records);i++)((uint8_t*)c->records)[i]=0;}
static int same(const char*a,const char*b){if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b;}
int obs_app_catalog_refresh(obs_app_catalog_t*c){
    if(!c||c->service_handle==(uint64_t)-1)return OBS_APP_CATALOG_ERROR;
    int64_t reply=os_ipc_create();if(reply<0)return OBS_APP_CATALOG_ERROR;
    obs_app_catalog_request_t q={OBS_APP_CATALOG_PROTOCOL_VERSION,OBS_APP_CATALOG_OP_SNAPSHOT,0,0};obs_app_catalog_response_t r={0};int64_t snapshot=-1;
    int64_t sent=os_ipc_send_handle(c->service_handle,&q,sizeof(q),(uint64_t)reply,OS_RIGHT_WRITE);
    int64_t received=sent==(int64_t)sizeof(q)?os_ipc_recv_handle((uint64_t)reply,&r,sizeof(r),&snapshot):-1;os_handle_close((uint64_t)reply);
    if(received!=(int64_t)sizeof(r)||r.status<0||r.version!=OBS_APP_CATALOG_PROTOCOL_VERSION||r.record_size!=sizeof(obs_app_metadata_t)||r.count>OBS_APP_CATALOG_MAX||snapshot<0){if(snapshot>=0)os_handle_close((uint64_t)snapshot);return OBS_APP_CATALOG_ERROR;}
    const obs_app_catalog_snapshot_t*s=os_shm_map((uint64_t)snapshot,(void*)CATALOG_MAP,0);
    if(s==(void*)-1){os_handle_close((uint64_t)snapshot);return OBS_APP_CATALOG_ERROR;}
    int valid=s->magic==OBS_APP_CATALOG_SNAPSHOT_MAGIC&&s->version==OBS_APP_CATALOG_PROTOCOL_VERSION&&s->generation==r.generation&&s->count==r.count;
    if(valid){clear_records(c);c->generation=s->generation;c->count=s->count;for(uint32_t i=0;i<c->count;i++)c->records[i]=s->records[i];}
    os_shm_unmap((void*)CATALOG_MAP);os_handle_close((uint64_t)snapshot);return valid?0:OBS_APP_CATALOG_ERROR;
}
int obs_app_catalog_connect(obs_app_catalog_t*c){if(!c)return-1;c->service_handle=(uint64_t)-1;clear_records(c);int64_t h=os_service_connect(OBS_APP_CATALOG_SERVICE_NAME);if(h<0)return-1;c->service_handle=(uint64_t)h;if(obs_app_catalog_refresh(c)<0){obs_app_catalog_close(c);return-1;}return 0;}
void obs_app_catalog_close(obs_app_catalog_t*c){if(!c)return;if(c->service_handle!=(uint64_t)-1)os_handle_close(c->service_handle);c->service_handle=(uint64_t)-1;}
uint32_t obs_app_catalog_count(const obs_app_catalog_t*c){return c?c->count:0;}
const obs_app_metadata_t*obs_app_catalog_at(const obs_app_catalog_t*c,uint32_t i){return c&&i<c->count?&c->records[i]:0;}
const obs_app_metadata_t*obs_app_catalog_find_id(const obs_app_catalog_t*c,const char*id){if(c)for(uint32_t i=0;i<c->count;i++)if(same(c->records[i].id,id))return&c->records[i];return 0;}
const obs_app_metadata_t*obs_app_catalog_find_path(const obs_app_catalog_t*c,const char*path){if(c)for(uint32_t i=0;i<c->count;i++)if(same(c->records[i].path,path))return&c->records[i];return 0;}
