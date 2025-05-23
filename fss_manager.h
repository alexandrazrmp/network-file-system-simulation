#define _GNU_SOURCE

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
#include <time.h>
#include <sys/inotify.h>
#include <sys/select.h>

#include "List.h"
#include "Queue.h"

#define DEFAULT_WORKER_LIMIT 5

#define MAX_LINE 1024


void setup_inotify();

int add_directory_watch(const char *path) ;

void sigchld_handler(int sig) ;

void parse_config_file(FILE* file, FILE* log_file) ;

void start_worker(const char* src, const char* tgt, const char* filename, const char* operation) ;

int main(int argc, char* argv[]);