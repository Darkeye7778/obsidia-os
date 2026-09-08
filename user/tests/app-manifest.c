#include "syscall.h"
#include <obsidia/app_metadata.h>

static int parse(const char*s){uint32_t n=0;while(s[n])n++;obs_app_metadata_t r;return obs_app_manifest_parse(s,n,&r);}
int obsidia_main(void){
    static const char valid[]="manifest-version=1\nid=org.obsidia.valid-app\nname=Valid App\nexecutable=valid.obsx\nversion=1.2\nicon=application\ncapabilities=none\nfuture-field=ignored\n";
    obs_app_metadata_t r;if(obs_app_manifest_parse(valid,sizeof(valid)-1,&r)<0||r.id[0]!='o'||r.path[0]!='v'||r.capabilities)return 1;
    if(parse("manifest-version=2\nid=org.x.a\nname=A\nexecutable=a.obsx\nversion=1\nicon=application\ncapabilities=none\n")==0)return 2;
    if(parse("manifest-version=1\nname=A\nexecutable=a.obsx\nversion=1\nicon=application\ncapabilities=none\n")==0)return 3;
    if(parse("manifest-version=1\nid=org.x.a\nid=org.x.b\nname=A\nexecutable=a.obsx\nversion=1\nicon=application\ncapabilities=none\n")==0)return 4;
    if(parse("manifest-version=1\nid=org.x.a\nname=This application display name is deliberately much too long for the record\nexecutable=a.obsx\nversion=1\nicon=application\ncapabilities=none\n")==0)return 5;
    if(parse("manifest-version=1\nid=org.x.a\nname=A\nexecutable=a.obsx\nversion=1\nicon=invalid\ncapabilities=none\n")==0)return 6;
    if(parse("manifest-version=1\nid=org.x.a\nname=A\nexecutable=a.obsx\nversion=1\nicon=application\ncapabilities=settings-write\n")==0)return 7;
    if(parse("manifest-version=1\nid=org.x.a\nname=A\nexecutable=../a.obsx\nversion=1\nicon=application\ncapabilities=none\n")==0)return 8;
    static const char passed[]="app-manifest: versioned bounded parsing and malformed/capability rejection passed\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
