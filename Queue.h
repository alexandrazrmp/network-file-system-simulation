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


typedef struct WorkerQueue {
    char source_dir[PATH_MAX];
    char target_dir[PATH_MAX];
    char filename[NAME_MAX];
    char operation[32];
    struct WorkerQueue* next;
} WorkerQueue;


WorkerQueue* queue_create() ;
WorkerQueue* queue_push(WorkerQueue *worker_queue, const char* src, const char* tgt, const char* filename, const char* operation) ;
WorkerQueue* queue_pop(WorkerQueue **worker_queue) ;