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
    char source_dir[PATH_MAX];  //full source (with port and host)
    char target_dir[PATH_MAX];  //full target (with port and host)
    char filename[NAME_MAX];

    struct WorkerQueue* next;
} WorkerQueue;


WorkerQueue* queue_create() ;
WorkerQueue* queue_push(WorkerQueue *worker_queue, const char* src, const char* tgt, const char* filename) ;
WorkerQueue* queue_pop(WorkerQueue **worker_queue) ;
int exists_in_queue(WorkerQueue *worker_queue, const char* src, const char* tgt, const char* filename) ;