#include <obsidia/app_metadata.h>

static void zero(void*p,uint32_t n){for(uint32_t i=0;i<n;i++)((uint8_t*)p)[i]=0;}
static int equal_n(const char*a,uint32_t n,const char*b){uint32_t i=0;while(b[i]&&i<n&&a[i]==b[i])i++;return !b[i]&&i==n;}
static int copy_value(char*out,uint32_t capacity,const char*value,uint32_t length){
    if(!length||length>=capacity)return-1;
    for(uint32_t i=0;i<length;i++){unsigned char c=(unsigned char)value[i];if(c<32||c>126)return-1;out[i]=(char)c;}
    out[length]=0;return 0;
}
static int valid_id(const char*s){
    if(!s||!s[0])return 0;
    uint32_t dots=0;
    for(uint32_t i=0;s[i];i++){char c=s[i];if(c=='.'){if(!i||!s[i+1]||s[i+1]=='.')return 0;dots++;continue;}if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return 0;}
    return dots>0;
}
static int valid_path(const char*s){
    if(!s||!s[0]||s[0]=='/')return 0;
    for(uint32_t i=0;s[i];i++){unsigned char c=(unsigned char)s[i];if(c<33||c>126||c=='\\')return 0;if(c=='.'&&s[i+1]=='.'&&(i==0||s[i-1]=='/')&&(s[i+2]==0||s[i+2]=='/'))return 0;}
    return 1;
}
int obs_app_manifest_parse(const char*data,uint32_t size,obs_app_metadata_t*r){
    if(!data||!size||size>1024||!r)return-1;
    zero(r,sizeof(*r));
    uint32_t seen=0,at=0,manifest_version=0;
    while(at<size){uint32_t start=at;while(at<size&&data[at]!='\n'&&data[at]!='\r')at++;uint32_t end=at;while(at<size&&(data[at]=='\n'||data[at]=='\r'))at++;if(end==start||data[start]=='#')continue;
        uint32_t equals=start;while(equals<end&&data[equals]!='=')equals++;if(equals==start||equals==end)return-1;const char*v=data+equals+1;uint32_t vn=end-equals-1,bit=0;
        if(equal_n(data+start,equals-start,"manifest-version")){bit=1;if(vn!=1||v[0]!='1')return-1;manifest_version=1;}
        else if(equal_n(data+start,equals-start,"id")){bit=2;if(copy_value(r->id,sizeof(r->id),v,vn)<0)return-1;}
        else if(equal_n(data+start,equals-start,"name")){bit=4;if(copy_value(r->display_name,sizeof(r->display_name),v,vn)<0)return-1;}
        else if(equal_n(data+start,equals-start,"executable")){bit=8;if(copy_value(r->path,sizeof(r->path),v,vn)<0)return-1;}
        else if(equal_n(data+start,equals-start,"version")){bit=16;if(copy_value(r->version,sizeof(r->version),v,vn)<0)return-1;}
        else if(equal_n(data+start,equals-start,"icon")){bit=32;if(equal_n(v,vn,"window-demo"))r->icon=OBS_ICON_WINDOW_DEMO;else if(equal_n(v,vn,"settings"))r->icon=OBS_ICON_SETTINGS;else if(equal_n(v,vn,"application"))r->icon=OBS_ICON_APPLICATION;else return-1;}
        else if(equal_n(data+start,equals-start,"capabilities")){bit=64;if(!equal_n(v,vn,"none"))return-1;}
        else continue;
        if(seen&bit)return-1;
        seen|=bit;
    }
    if(manifest_version!=OBS_APP_MANIFEST_VERSION||seen!=127||!valid_id(r->id)||!valid_path(r->path))return-1;
    return 0;
}
