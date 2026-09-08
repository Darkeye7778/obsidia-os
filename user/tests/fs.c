#include "syscall.h"
#include <obsidia/fs.h>

static int bytes_equal(const char* a,const char* b,uint32_t count){for(uint32_t i=0;i<count;i++)if(a[i]!=b[i])return 0;return 1;}
static int string_equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return!*a&&!*b;}
static int directory_has(const char*path,const char*name,uint32_t type){
    obs_directory_entry_t entry;
    for(uint32_t index=0;index<128;index++){
        int status=os_directory_read(path,index,&entry);
        if(status<=0)return 0;
        if(entry.type==type&&string_equal(entry.name,name))return 1;
    }
    return 0;
}
static void require(int condition){if(!condition)__asm__ volatile("ud2");}

int obsidia_main(void){
    static const char volatile_data[]="hierarchical-open-file";
    static const char persistent[]="statefs-persistent-v2";
    char output[32];
    obs_fs_stat_t info;
    obs_directory_entry_t unused_entry;

    int64_t fd=sys_open_flags("/tmp/platform.txt",OS_OPEN_CREATE|OS_OPEN_TRUNC);
    require(fd>=3);
    require(sys_fd_write((int)fd,volatile_data,sizeof(volatile_data)-1)==(int64_t)sizeof(volatile_data)-1);
    require(os_file_remove("/tmp/platform.txt")<0); /* Open-node lifetime is protected. */
    require(sys_close((int)fd)==0);
    require(os_fs_stat("/tmp/platform.txt",&info)==0&&info.type==OBS_FS_FILE&&info.size==sizeof(volatile_data)-1);
    require(os_directory_create("/tmp/fs-one")==0);
    require(os_directory_create("/tmp/fs-two")==0);
    require(os_directory_create("/tmp/fs-one/nested")==0);
    require(directory_has("/tmp","fs-one",OBS_FS_DIRECTORY));
    require(os_path_rename("/tmp/platform.txt","/tmp/fs-one/nested/moved.txt",0)==0);
    fd=sys_open("/tmp/fs-one/nested/moved.txt");
    require(fd>=3&&sys_read((int)fd,output,sizeof(volatile_data)-1)==(int64_t)sizeof(volatile_data)-1&&bytes_equal(output,volatile_data,sizeof(volatile_data)-1));
    require(sys_close((int)fd)==0);
    require(os_directory_remove("/tmp/fs-one")<0); /* Nonempty directories are protected. */
    require(os_file_remove("/tmp/fs-one/nested/moved.txt")==0);
    require(os_directory_remove("/tmp/fs-one/nested")==0);
    require(os_directory_remove("/tmp/fs-one")==0);
    require(os_directory_remove("/tmp/fs-two")==0);
    require(os_fs_stat("/tmp/platform.txt",&info)<0);
    require(sys_open_flags("/cannot-create-here",OS_OPEN_CREATE)<0);

    require(os_fs_stat("/system/apps",&info)==0&&info.type==OBS_FS_DIRECTORY);
    require(directory_has("/system/apps","window-demo.oam",OBS_FS_FILE));
    require(os_file_remove("/system/apps/window-demo.oam")<0); /* Initrd remains read-only. */
    require(sys_open_flags("/system/apps/window-demo.oam",OS_OPEN_TRUNC)<0);
    require(sys_stat("/tmp",(void*)1)<0&&sys_readdir("/tmp",0,(void*)1)<0);

    int prior=os_fs_stat("/state/regression-tree/nested/persist.dat",&info)==0;
    if(!prior){
        require(os_directory_create("/state/regression-tree")==0);
        require(os_directory_create("/state/regression-tree/nested")==0);
        fd=sys_open_flags("/state/regression-tree/source.dat",OS_OPEN_CREATE|OS_OPEN_TRUNC);
        require(fd>=3&&sys_fd_write((int)fd,persistent,sizeof(persistent)-1)==(int64_t)sizeof(persistent)-1);
        require(os_file_sync((int)fd)==0&&sys_close((int)fd)==0);
        require(os_path_rename("/state/regression-tree/source.dat","/state/regression-tree/nested/persist.dat",0)==0);
        static const char committed[]="statefs: v2 hierarchy committed\n";sys_fd_write(1,committed,sizeof(committed)-1);
    }else{
        static const char survived[]="statefs: v2 hierarchy survived reboot\n";sys_fd_write(1,survived,sizeof(survived)-1);
    }
    require(os_fs_stat("/state/regression-tree/nested/persist.dat",&info)==0&&info.type==OBS_FS_FILE&&info.size==sizeof(persistent)-1);
    require(info.modified_ticks>=info.created_ticks);
    require(directory_has("/state/regression-tree","nested",OBS_FS_DIRECTORY));
    require(directory_has("/state/regression-tree/nested","persist.dat",OBS_FS_FILE));
    fd=sys_open("/state/regression-tree/nested/persist.dat");
    require(fd>=3&&sys_read((int)fd,output,sizeof(persistent)-1)==(int64_t)sizeof(persistent)-1&&bytes_equal(output,persistent,sizeof(persistent)-1));
    require(sys_close((int)fd)==0);

    if(os_fs_stat("/state/regression-tree/scratch",&info)==0){(void)os_file_remove("/state/regression-tree/scratch/delete.dat");require(os_directory_remove("/state/regression-tree/scratch")==0);}
    if(os_fs_stat("/state/regression-tree/renamed",&info)==0){(void)os_file_remove("/state/regression-tree/renamed/delete.dat");require(os_directory_remove("/state/regression-tree/renamed")==0);}
    require(os_directory_create("/state/regression-tree/scratch")==0);
    require(os_directory_create("/state/regression-tree/scratch")<0);
    fd=sys_open_flags("/state/regression-tree/scratch/delete.dat",OS_OPEN_CREATE|OS_OPEN_TRUNC);
    require(fd>=3&&sys_fd_write((int)fd,"x",1)==1&&os_file_sync((int)fd)==0&&sys_close((int)fd)==0);
    require(os_directory_remove("/state/regression-tree/scratch")<0);
    require(os_path_rename("/state/regression-tree/scratch","/state/regression-tree/renamed",0)==0);
    require(os_fs_stat("/state/regression-tree/renamed/delete.dat",&info)==0&&info.size==1);
    require(os_file_remove("/state/regression-tree/renamed/delete.dat")==0);
    require(os_directory_remove("/state/regression-tree/renamed")==0);
    require(os_directory_read("/state/regression-tree/scratch",0,&unused_entry)<0);

    static const char passed[]="vfs: hierarchy, enumeration, stat, rename, deletion passed\n";
    sys_fd_write(1,passed,sizeof(passed)-1);
    return 0;
}
