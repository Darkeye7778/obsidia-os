#include <obsidia/fs.h>
#include "syscall.h"

int os_file_sync(int fd){return sys_fd_sync(fd)<0?-1:0;}
int os_path_rename(const char*old_path,const char*new_path,int replace){if(!old_path||!new_path)return-1;return sys_rename(old_path,new_path,replace)<0?-1:0;}
int os_directory_create(const char*path){if(!path)return-1;return sys_mkdir(path)<0?-1:0;}
int os_file_remove(const char*path){if(!path)return-1;return sys_unlink(path,0)<0?-1:0;}
int os_directory_remove(const char*path){if(!path)return-1;return sys_unlink(path,1)<0?-1:0;}
int os_fs_stat(const char*path,obs_fs_stat_t*result){if(!path||!result)return-1;return sys_stat(path,result)<0?-1:0;}
int os_directory_read(const char*path,uint32_t index,obs_directory_entry_t*result){if(!path||!result)return-1;int64_t value=sys_readdir(path,index,result);return value<0?-1:(int)value;}
