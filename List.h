#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h> 
#include <limits.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <bits/getopt_core.h>
#include <time.h>

#define SYNCING 1
#define NOT_SYNCING 0

#define MAX_LINE 1024

//linked list to store sync info
typedef struct sync_info_mem_store_struct {
    char source_dir[PATH_MAX];
    char target_dir[PATH_MAX];
    time_t last_sync_time;
    pid_t worker_pid;  //PID of last worker
    int active;
    int error_count;
    int wd; //watch descriptor for inotify
    struct sync_info_mem_store_struct* next;
} sync_info_mem_store;

//add entry to linked list
sync_info_mem_store* add_sync_entry(sync_info_mem_store** sync_list, const char* src, const char* tgt) ;

//check if entry exists in linked list
sync_info_mem_store* exists_sync_entry(sync_info_mem_store* sync_list, const char* src, const char* tgt) ;

//delete entry from linked list using source directory
void delete_sync_entry(sync_info_mem_store** sync_list, const char* src);