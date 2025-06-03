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
#include <sys/select.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>

#include "List.h"
#include "Queue.h"

#define DEFAULT_WORKER_LIMIT 5

#define MAX_LINE 1024


void sigchld_handler(int sig) ;

void parse_config_file(FILE* file, FILE* log_file) ;

void start_worker(sync_info_mem_store* entry, FILE* log_file) ;

int main(int argc, char* argv[]);