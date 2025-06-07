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

#define MAX_LINE 1024

//one entry for each sync operation of a single file of a directory

typedef struct WorkerQueue {
    char source_dir[PATH_MAX];  //source directory
    char source_host[64];  //source host
    int source_port;       //source port

    char target_dir[PATH_MAX];  //target directory
    char target_host[64];  //target host
    int target_port;       //target port

    char filename[NAME_MAX];

    struct WorkerQueue* next;
} WorkerQueue;


WorkerQueue* queue_push(WorkerQueue *worker_queue, const char* src, const char* source_host, int source_port, const char* tgt, 
                        const char* target_host, int target_port, const char* filename) ;

WorkerQueue* queue_pop(WorkerQueue **worker_queue) ;

int exists_in_queue(WorkerQueue *worker_queue, const char* src, const char* tgt, const char* filename) ;

int queue_remove_all_source(WorkerQueue** worker_queue, const char* src); 