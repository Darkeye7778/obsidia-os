#include "syscall.h"
#include <obsidia/app_catalog.h>
#include <obsidia/service.h>
#include <obsidia/internal/app_catalog_protocol.h>

static int raw_bad_version(void){int64_t service=os_service_connect(OBS_APP_CATALOG_SERVICE_NAME),reply=os_ipc_create();if(service<0||reply<0)return-1;obs_app_catalog_request_t q={99,OBS_APP_CATALOG_OP_SNAPSHOT,0,0};obs_app_catalog_response_t r={0};int ok=os_ipc_send_handle((uint64_t)service,&q,sizeof(q),(uint64_t)reply,OS_RIGHT_WRITE)==(int64_t)sizeof(q)&&os_ipc_recv((uint64_t)reply,&r,sizeof(r))==(int64_t)sizeof(r);os_handle_close((uint64_t)service);os_handle_close((uint64_t)reply);return ok?r.status:-1;}
int obsidia_main(void){obs_app_catalog_t c;if(obs_app_catalog_connect(&c)<0||obs_app_catalog_count(&c)!=2)return 1;const obs_app_metadata_t*w=obs_app_catalog_find_id(&c,"org.obsidia.window-demo"),*s=obs_app_catalog_find_path(&c,"settings-demo.obsx");if(!w||!s||w->capabilities||(s->capabilities&OBS_APP_CAP_SETTINGS_WRITE)==0||obs_app_catalog_at(&c,2))return 2;if(raw_bad_version()!=OBS_APP_CATALOG_BAD_VERSION)return 3;obs_app_catalog_close(&c);if(!obs_app_catalog_find_id(&c,"org.obsidia.window-demo"))return 4;static const char passed[]="app-catalog: service snapshot, enumeration, lookup, policy and cache survival passed\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;}
