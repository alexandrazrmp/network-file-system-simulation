#include "fss_manager.h"

int *worker_array; //will be initialized later when worker limit is set
int worker_count = 0; //number of workers currently running
int worker_limit = 0; //maximum number of workers allowed

sync_info_mem_store* sync_list = NULL;

WorkerQueue* worker_queue = NULL;


int inotify_fd;

void setup_inotify() {
    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd < 0) {
        perror("inotify_init1 failed");
        exit(1);
    }
}


//add directory watch and return the watch descriptor
int add_directory_watch(const char *path) {
    int wd = inotify_add_watch(inotify_fd, path, IN_CREATE | IN_MODIFY | IN_DELETE );
    if (wd == -1) {
        perror("inotify_add_watch");
    }
    return wd;
}

//signal handler for SIGCHLD
volatile sig_atomic_t child_exited = 0; //flag to start a queued worker

void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        //remove pid from worker_array
        for (int i = 0; i < worker_count; ++i) {
            if (worker_array[i] == pid) {
                //shift the remaining child processes to the left
                for (int j = i; j < worker_count - 1; ++j) {
                    worker_array[j] = worker_array[j + 1];
                }
                worker_count--;
                break;
            }
        }


        // find sync entry and mark as done/error
        sync_info_mem_store *cur = sync_list;
        while (cur) {
            if (cur->worker_pid == pid) {
                if (WIFEXITED(status)) {
                    if (WEXITSTATUS(status) != 0) cur->error_count ++;
                } else if (WIFSIGNALED(status)) {
                    cur->error_count ++;
                }
                cur->worker_pid = -1;   //initialize worker pid
                break;
            }
            cur = cur->next;
        }
    }

    child_exited = 1;
}



void parse_config_file(FILE* file, FILE* log_file) {
    if (!file) {
        perror("fopen config_file");
        exit(1);
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
        //time to be used in the log file
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format

        char src[PATH_MAX] = {0}, tgt[PATH_MAX] = {0};
    
        if (sscanf(line, " ( %[^,] , %[^)] ) ", src, tgt) == 2) { //successfully parsed
            
            sync_list = add_sync_entry(&sync_list, src, tgt);   //add at the end
            sync_info_mem_store* current = exists_sync_entry(sync_list, src, tgt);  //get the ptr to the new entry
            current->wd = add_directory_watch(current->source_dir); //monitor source directory

            //print to log file
            fprintf(log_file, "%s Added directory: %s -> %s\n", timebuf, src, tgt);
            fprintf(log_file, "%s Monitoring started for %s\n", timebuf, src);
            fflush(log_file);
            
            //print to standard output
            printf("%s Added directory: %s -> %s\n", timebuf, src, tgt);
            printf("%s Monitoring started for %s\n", timebuf, src);
            fflush(stdout);

        }
    }
    
    fclose(file);
}


void start_worker(const char* src, const char* tgt, const char* filename, const char* operation) {
    //create read-write named pipe for each worker
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe creation failed");
        return;
    }

    sync_info_mem_store* cur = exists_sync_entry(sync_list, src, tgt);

    pid_t pid = fork();
    if (pid == 0) { // child process

        close(pipefd[0]); // close read end of the pipe
        dup2(pipefd[1], STDOUT_FILENO); // redirect stdout to the write end of the pipe
        close(pipefd[1]); // close write end of the pipe

        char* const args[] = {"./worker", (char *)src, (char *)tgt, (char *)filename, (char *)operation, NULL};
        execv(args[0], args);

        perror("execv failed");
        exit(1);

    } else if (pid < 0) {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
    } else { // parent process
        worker_array[worker_count++] = pid; //add to worker array
        if (cur) cur->worker_pid = pid; //latest worker pid
        close(pipefd[1]);

        //read from the pipe
        char buffer[MAX_LINE];
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // null-terminate the string
            printf("%s", buffer); // print the report as is
        } else if (bytes_read == 0) {
            printf("No bytes in the pipe\n");
        } else {
            perror("read failed");
        }
        close(pipefd[0]); // close read

    }

    return;
}


int main(int argc, char* argv[]) {

    setup_inotify();
    char* log_file_ = NULL;
    char* config_file_ = NULL;
    worker_limit = DEFAULT_WORKER_LIMIT;

    // Parse arguments
    int opt;
    while ((opt = getopt(argc, argv, "l:c:n:")) != -1) {
        switch (opt) {
            case 'l': log_file_ = optarg; break;
            case 'c': config_file_ = optarg; break;
            case 'n': if (atoi(optarg)>0) worker_limit = atoi(optarg); break;
            default:
                fprintf(stderr, "Please give input in the form ./fss_manager -l <logfile> -c <config_file> -n <worker_limit>\n");
                exit(1);
        }
    }

    worker_array = malloc(sizeof(int) * worker_limit); //initialize worker array

    if (!log_file_ || !config_file_) {
        fprintf(stderr, "Input error\n");
        exit(1);
    }

    //cast file pointer to FILE* (previously char* for parsing)
    FILE* log_file = fopen(log_file_, "a");
    FILE* config_file = fopen(config_file_, "r");

    //named pipes
    if ( mkfifo ( "fss_in" , 0666) == -1 ) {
        if ( errno != EEXIST ) {
            perror ( "failed to create named pipe" ) ;
            exit (6) ;
        }
    }

    if ( mkfifo ( "fss_out" , 0666) == -1 ) {
        if ( errno != EEXIST ) {
            perror ( "failed to create named pipe" ) ;
            exit (6) ;
        }
    }

    printf("FSS Manager started with a limit of %d workers.\n", worker_limit);

    //read config file and add entries to sync_list
    parse_config_file(config_file, log_file);

    //do the initial sync
    sync_info_mem_store* current = sync_list;
    for (int i = 0; i < worker_limit; i++) {
        if (current == NULL) break;         //no more entries
        current->active = 1; //mark as active
        current->last_sync_time = time(NULL); //update last sync time
        current->error_count = 0; //reset error count
        start_worker(current->source_dir, current->target_dir, "ALL", "FULL"); //start worker process
        current = current->next; // move to the next entry
    }

    worker_queue = queue_create();

    //if there are more entries, add them to the queue
    while (current != NULL) {
        worker_queue = queue_push(worker_queue, current->source_dir, current->target_dir, "ALL", "FULL"); //add to queue
        current = current->next; // move to the next entry  
    }


//open pipes
    int fd_in = open("fss_in", O_RDONLY);
    if (fd_in < 0) {
        perror("failed to open fss_in");
        fclose(log_file);
        exit(1);
    }


    int fd_out = open("fss_out", O_WRONLY);
    if (fd_out < 0) {
        perror("failed to open fss_out");
        fclose(log_file);
        close(fd_in);
        exit(1);
    }

    char input[MAX_LINE];
    char response[MAX_LINE];

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);


    while (1) {

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd_in, &read_fds);
        FD_SET(inotify_fd, &read_fds);
    
        int max_fd = (fd_in > inotify_fd) ? fd_in : inotify_fd; //select action (console input or inotify event)
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) {
                continue; //interrupted by signal
            }
            perror("select error");
            break;
        }
        
    
        if (FD_ISSET(inotify_fd, &read_fds)) {  //inotify event
            //read from inotify file descriptor
            char buffer[MAX_LINE];
            ssize_t length = read(inotify_fd, buffer, sizeof(buffer));
            if (length <= 0) {//no event found
                perror("inotify read failed");
                continue;
            }
            for (char *ptr = buffer; ptr < buffer + length; ) {
                struct inotify_event *event = (struct inotify_event*)ptr;
                ptr += sizeof(struct inotify_event) + event->len;
        
                //find which sync entry this watch descriptor belongs to
                sync_info_mem_store *entry = sync_list;
                while (entry && entry->wd != event->wd) entry = entry->next;
                if (!entry) continue;
        
                //find operation based on event mask
                const char *operation = NULL;
                if (event->mask & IN_CREATE ) operation = "ADDED";
                else if (event->mask & IN_MODIFY) operation = "MODIFIED";
                else if (event->mask & IN_DELETE) operation = "DELETED";
                else continue; //ignore other events
        
                if (worker_count < worker_limit) {
                    start_worker(entry->source_dir, entry->target_dir, event->name, operation);
                } else {
                    worker_queue = queue_push(worker_queue, entry->source_dir, entry->target_dir, event->name, operation);
                }
            }
        }
    
        if (FD_ISSET(fd_in, &read_fds)) { //console input
            //read from the pipe

            ssize_t bytes = read(fd_in, input, sizeof(input) - 1);
            if (bytes > 0) {
                input[bytes] = '\0';
            }
            else if (bytes == 0) {
                printf("MANAGER got no instruction\n");
                break;
            } else {
                perror("MANAGER failed to read from fss_in");
                break;
            }
    
            //time to be used in the log file
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
        
    
            //handle input assuming it is always valid as it is from the console
            char *instruction, *arg1, *arg2;
            instruction = strtok(input, " ");
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");

            //remove newline character if present
            instruction[strcspn(instruction, "\n")] = '\0';
            if (arg1 != NULL) arg1[strcspn(arg1, "\n")] = '\0';
            if (arg2 != NULL) arg2[strcspn(arg2, "\n")] = '\0';

            if (strcmp(instruction, "shutdown") == 0) {
                //print messages
                printf("%s Shutting down manager...\n", timebuf);
                printf("%s Waiting for all active workers to finish.\n", timebuf);
                fflush(stdout);
                //printing to be continued after "break" to actually wait for all workers to finish
    
                break;
            } else if (strcmp(instruction, "sync") == 0) { //use a worker to sync source directory arg1 to target directory
                
                sync_info_mem_store *cur = exists_sync_entry(sync_list, arg1, NULL);
                if (cur != NULL) {
                    //print to log file
                    fprintf(log_file, "%s Syncing directory: %s -> %s\n", timebuf, arg1, arg2);
                    fflush(log_file);
                    
                    //print to standard output
                    printf("%s Syncing directory: %s -> %s\n", timebuf, arg1, arg2);
                    fflush(stdout);

                    if (worker_limit > worker_count) {
                        cur->active = 1; //mark as active
                        cur->last_sync_time = time(NULL); //update last sync time
                        cur->error_count = 0; //reset error count
                        //start worker process
                        start_worker(arg1, cur->target_dir, "ALL", "FULL");
                    } else {
                        worker_queue = queue_push(worker_queue, arg1, cur->target_dir, "ALL", "FULL"); //add to queue
                    }
                }else continue; //no such entry in the sync_list, ignore 
            } else if (strcmp(instruction, "cancel") == 0) {
                //mark the entry from the sync_list as not active but maintain it in the queue
                sync_info_mem_store *cur = exists_sync_entry(sync_list, arg1, NULL);
                if (cur && cur->active) {
                    cur->active = 0;    //mark as not active
                    printf("%s Monitoring stopped for %s\n", timebuf, arg1);
                    fflush(stdout);
                    fprintf(log_file, "%s Monitoring stopped for %s\n", timebuf, arg1);
                    fflush(log_file);
                } else if (cur == NULL){
                    printf("%s Directory not monitored: %s\n", timebuf, arg1);
                }
                fflush(log_file);
            
            } else if (strcmp(instruction, "status") == 0) {
    
                //check the status of the sync_list and print it to stdout 
                sync_info_mem_store *cur = exists_sync_entry(sync_list, arg1, NULL);
                if (cur && cur->active) {
                    printf("%s Status requested for %s\n", timebuf, arg1);
                    printf("Directory: %s\n", cur->source_dir);
                    printf("Target: %s\n", cur->target_dir);
                    printf("Last Sync: %ld\n", cur->last_sync_time);
                    printf("Errors: %d\n", cur->error_count);
                    printf("Status: Active\n");
                } else {
                    printf("%s Directory not monitored: %s\n", timebuf, arg1);
                }
    
                //write to log file
    
                fprintf(log_file, "%s Command status %s\n", timebuf, arg1);   //write to log file
                fflush(log_file); // flush to ensure it's written immediately
            } else if (strcmp(instruction, "add") == 0) {
                //add the entry to the sync_list (source and target directories)
                if (exists_sync_entry(sync_list, arg1, arg2) == NULL) {
                    sync_list = add_sync_entry(&sync_list, arg1, arg2);
                    sync_info_mem_store* current = exists_sync_entry(sync_list, arg1, arg2);
                    current->wd = add_directory_watch(current->source_dir); //monitor source directory

                    if (worker_limit > worker_count) {
                        current->active = 1; //mark as active
                        current->last_sync_time = time(NULL); //update last sync time
                        current->error_count = 0; //reset error count
                        //start worker process
                        start_worker(arg1, current->target_dir, "ALL", "FULL");
                    } else {
                        worker_queue = queue_push(worker_queue, arg1, current->target_dir, "ALL", "FULL"); //add to queue
                    }

                    //print to log file
                    fprintf(log_file, "%s Added directory: %s -> %s\n", timebuf, arg1, arg2);
                    fprintf(log_file, "%s Monitoring started for %s\n", timebuf, arg1);
                    fflush(log_file);
                    
                    //print to standard output
                    printf("%s Added directory: %s -> %s\n", timebuf, arg1, arg2);
                    printf("%s Monitoring started for %s\n", timebuf, arg1);
                    fflush(stdout);
                } else {
                    //print to log file
                    fprintf(log_file, "%s Already in queue: %s\n", timebuf, arg1);
                    fflush(log_file);
                    
                    //print to standard output
                    printf("%s Already in queue: %s\n", timebuf, arg1);
                    fflush(stdout);
                }
    
            }
    
    
            snprintf(response, sizeof(response), "all good");   //just to check
    
            if (write(fd_out, response, strlen(response)) < 0) {
                perror("MANAGER failed to write to fss_out");
                break;
            }

        }
        //SIGCHLD handler
        if (child_exited && worker_queue != NULL) {
            // Start the next worker from the queue
            WorkerQueue* next_job = queue_pop(&worker_queue);
            if (next_job) {
                start_worker(next_job->source_dir, next_job->target_dir, next_job->filename, next_job->operation);
                free(next_job);
            }
            child_exited = 0; 
        }

    }


    close(fd_in);
    close(fd_out);
    fclose(log_file);

    //handle shutdown wait for all workers to finish

    //clean up
    while (worker_queue != NULL) {
        WorkerQueue * cur = queue_pop(&worker_queue);
        if (cur != NULL) {
            free(cur);
        }
    }

    sync_info_mem_store *entry = sync_list;
    while (entry) {
        if (entry->wd != -1) {
            inotify_rm_watch(inotify_fd, entry->wd);
        }
        entry = entry->next;
    }
    close(inotify_fd);

    //wait for all child processes to finish
    for (int i = 0; i < worker_count; i++) {
        waitpid(worker_array[i], NULL, 0);
    }


    while (sync_list != NULL) {
        delete_sync_entry(&sync_list, sync_list->source_dir);
    }
    free(worker_array); //free the worker array


    unlink("fss_in");
    unlink("fss_out");

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
        

    printf("%s Manager shutdown complete.\n", timebuf);
    fflush(stdout);
    return 0;
}