#pragma once
#include <stdint.h>

#define OBS_FS_NAME_MAX 128U

typedef enum {
    OBS_FS_FILE = 1,
    OBS_FS_DIRECTORY = 2
} obs_fs_type_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t size;
    uint64_t created_ticks;
    uint64_t modified_ticks;
} obs_fs_stat_t;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t size;
    char name[OBS_FS_NAME_MAX];
} obs_directory_entry_t;

int os_file_sync(int fd);
int os_path_rename(const char* old_path,const char* new_path,int replace);
int os_directory_create(const char* path);
int os_file_remove(const char* path);
int os_directory_remove(const char* path);
int os_fs_stat(const char* path,obs_fs_stat_t* result);

/* Returns 1 for an entry, 0 at end of directory, and -1 on failure. */
int os_directory_read(const char* path,uint32_t index,obs_directory_entry_t* result);
