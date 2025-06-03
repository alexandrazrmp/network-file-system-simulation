#include "nfs_manager.h"

int *worker_array; //will be initialized later when worker limit is set
int worker_count = 0; //number of workers currently running
int worker_limit = 0; //maximum number of workers allowed
pthread_t* worker_thread_pool; //thread pool for worker threads

sync_info_mem_store* sync_list = NULL;

WorkerQueue* worker_queue = NULL;


//signal handler for SIGCHLD
volatile sig_atomic_t child_exited = 0; //flag to start a queued worker

//wait for all dead child processes
void sigchld_handler ( int sig ) {
    while ( waitpid ( -1 , NULL , WNOHANG ) > 0) ;
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

        line[strcspn(line, "\n")] = 0; //remove newline if present

        char src[PATH_MAX] = {0}, tgt[PATH_MAX] = {0};  //full paths for source and target directories
        if (sscanf(line, "%s %s", src, tgt) != 2) {
            fprintf(stderr, "invalid entry in config file\n");
            continue; //ignore
        }


        char source_dir[PATH_MAX] = {0};
        char target_dir[PATH_MAX] = {0};
        char source_host[64] = {0};
        int source_port = 0;
        char target_host[64] = {0};
        int target_port = 0;
    
        if (sscanf(line, "/%[^@]@%[^:]:%d /%[^@]@%[^:]:%d",source_dir, source_host, &source_port,
        target_dir, target_host, &target_port) == 6) { //successfully parsed
            
            sync_list = add_sync_entry(&sync_list, source_dir, target_dir, source_host, source_port, target_host, target_port);   //add at the end
            sync_info_mem_store* current = exists_sync_entry(sync_list, source_dir, target_dir);  //get the ptr to the new entry

            //print to log file
            fprintf(log_file, "%s Added directory: %s -> %s\n", timebuf, src, tgt);
            fprintf(log_file, "%s Monitoring started for %s\n", timebuf, src);
            fflush(log_file);
            
            //print to standard output
            printf("%s Added directory: %s -> %s\n", timebuf, src, tgt);
            printf("%s Monitoring started for %s\n", timebuf, src);
            fflush(stdout);

        }
        else {
            fprintf(stderr, "invalid entry in config file\n");
            continue; //ignore 
        }
    }
    
    fclose(file);
}

//worker function to be run in a separate thread
void* worker_function(void* arg) {
    sync_info_mem_store* entry = (sync_info_mem_store*)arg;
    if (!entry) {
        fprintf(stderr, "Invalid entry\n");
        return NULL;
    }

    //example
    printf("Worker for %s -> %s started.\n", entry->source_dir, entry->target_dir);
    sleep(2);
    printf("Worker for %s -> %s finished.\n", entry->source_dir, entry->target_dir);

    return NULL;
}


//start worker achieves the following:
//creates a worker thread for the given sync_info_mem_store entry and stores the thread in the worker thread pool
//achieves connections to the source and target directories
//calls worker_function to run the worker thread

void start_worker(sync_info_mem_store* entry, FILE* log_file) {
    if (!entry || !log_file) {
        fprintf(stderr, "Invalid entry or log file\n");
        return;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);

    //create worker thread

    pthread_t worker_thread;
    if (pthread_create(&worker_thread, NULL, worker_function, entry) != 0) {
        perror("pthread_create failed");
        return;
    }
    //store the thread in the worker thread pool
    for (int i = 0; i < worker_limit; i++) {
        if (worker_thread_pool[i] == 0) { //find an empty slot
            worker_thread_pool[i] = worker_thread;
            break;
        }
    }
    entry->worker_pid = worker_thread; //store the thread ID in the entry
    worker_array[worker_count++] = entry->worker_pid; //store the PID in the worker array

    printf("%s Worker started for %s -> %s\n", timebuf, entry->source_dir, entry->target_dir);
    fflush(stdout);

    //write to log file
    
//to fix for every file in dir
    fprintf(log_file, "%s Added file:  \n", timebuf);
    fflush(log_file);

    return;
}


int main(int argc, char* argv[]) {


    char* log_file_ = NULL;
    char* config_file_ = NULL;
    int port = 0;
    int bufferSize = 0; //number of slots (must be >0)
    worker_limit = DEFAULT_WORKER_LIMIT;

    // Parse arguments
    int option;
    while ((option = getopt(argc, argv, "l:c:n:p:b:")) != -1) {
        switch (option) {
            case 'l': log_file_ = optarg; break;
            case 'c': config_file_ = optarg; break;
            case 'n': if (atoi(optarg)>0) worker_limit = atoi(optarg); break;
            case 'p':   port = atoi(optarg); break;
            case 'b':   bufferSize = atoi(optarg);
                        if (bufferSize <=0) {
                            fprintf(stderr, "Buffer size must be > 0\n");
                            exit(1);
                        }
                        break;
            default:
                fprintf(stderr, "Please give input in the form ./fss_manager -l <logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>\n");
                exit(1);
        }
    }

    if (!log_file_ || !config_file_ || !port) {
        fprintf(stderr, "Missing arguments.\n");
        return 1;
    }

    worker_array = malloc(sizeof(int) * worker_limit); //initialize worker array

    if (!log_file_ || !config_file_) {
        fprintf(stderr, "Input error\n");
        exit(1);
    }

    //cast file pointer to FILE* (previously char* for parsing)
    FILE* log_file = fopen(log_file_, "a");
    FILE* config_file = fopen(config_file_, "r");

    printf("FSS Manager started with a limit of %d workers.\n", worker_limit);

    //socket creation on port number

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if (port <= 1024 || port > 65535) {
        fprintf(stderr, "invalid port number");
        exit(1);
    }

    //socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(1);
    }

    //to reuse the address and port immiediately
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(1);
    }

    //listen
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Socket created and listening on port %d\n", port);
    fflush(stdout);

    //read config file and add entries to sync_list
    parse_config_file(config_file, log_file);

    //initialize thread pool
    worker_thread_pool = malloc(sizeof(pthread_t) * worker_limit);


// //test
// ///////////////////////////////////////////////////////////////////////////////////////////////////////////
//     int host_port = 50000;
//     char *host_ip_ = "127.0.0.1";
//     if (host_port <= 0 || host_port > 65535) {
//         fprintf(stderr, "invalid port number: %d\n", host_port);
//         fclose(log_file);
//         return 1;
//     }

//     //create and connect socket
//     int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0) {
//         perror("socket creation failed");
//         fclose(log_file);
//         return 1;
//     }

//     struct sockaddr_in serv_addr = {0};
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(host_port);
//     if (inet_pton(AF_INET, host_ip_, &serv_addr.sin_addr) <= 0) {
//         fprintf(stderr, "invalid host IP: %s\n", host_ip_);
//         close(sockfd);
//         fclose(log_file);
//         return 1;
//     }

//     if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
//         perror("connection failed");
//         close(sockfd);
//         fclose(log_file);
//         return 1;
//     }

//     char input[MAX_LINE];
//     char response[MAX_LINE];
//     FILE *sockf = fdopen(sockfd, "r+"); //read-write
//     if (!sockf) {
//         perror("fdopen failed");
//         close(sockfd);
//         fclose(log_file);
//         return 1;
//     }

//     //send initial command to the client
//     strcpy(input, "LIST");
//     write(sockfd, input, strlen(input));
//     fflush(sockf);
//     read(sockfd, input, sizeof(input)-1);
//     printf("%s\n", input);

//     //close the socket
//     close(sockfd);
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    //do the initial sync
    sync_info_mem_store* current = sync_list;
    int active_workers = 0;

    while (current != NULL && active_workers < worker_limit) {
        current->active = 1;
        current->last_sync_time = time(NULL);
        current->error_count = 0;

        //start worker thread for each entry in sync_list and also write to log file
        start_worker(current, log_file);

        active_workers++;
        current = current->next;
    }

    worker_queue = queue_create();

    //if there are more entries, add them to the queue

    while (current != NULL) {
        worker_queue = queue_push(worker_queue, current->source_dir, current->target_dir, "ALL", "FULL");
        current = current->next;
    }


    //accept nfs_console connection
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        exit(1);
    }
    printf("Console-Manager connection achieved\n");


    char input[MAX_LINE];
    char response[MAX_LINE];


    while (1) {              //get console input and handle it

        fflush(stdout);
        ssize_t n = read(client_fd, input, sizeof(input)-1);
        if (n <= 0) {
            if (n < 0) perror("read failed");
            break;
        }
        input[n] = '\0';    //nullterminate
        input[strcspn(input, "\n")] = '\0'; //turn newline to null terminator
    
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

        char source_dir[PATH_MAX] = {0}, source_host[64] = {0};
        int source_port = 0;
        char target_dir[PATH_MAX] = {0}, target_host[64] = {0};
        int target_port = 0;

        if (arg1 != NULL) {
            int a = sscanf(arg1, "/%[^@]@%[^:]:%d", source_dir, source_host, &source_port) ; 
            //input is corerct from console, so we can assume it is valid
            if (a != 3) {   //but still, if it is not, we can ignore it
                fprintf(stderr, "invalid input format\n");
                break;
            }
        }
        
        if (arg2 != NULL) {
            int a = sscanf(arg2, "/%[^@]@%[^:]:%d", target_dir, target_host, &target_port) ; 
            //input is corerct from console, so we can assume it is valid
            if (a != 3) {   //but still, if it is not, we can ignore it
                fprintf(stderr, "invalid input format\n");
                break;
            }
        }
        
        //remove newline character if present and null terminate the strings
        source_dir[strcspn(source_dir, "\n")] = '\0';
        target_dir[strcspn(target_dir, "\n")] = '\0';
        source_host[strcspn(source_host, "\n")] = '\0';
        target_host[strcspn(target_host, "\n")] = '\0';

        if (strcmp(instruction, "shutdown") == 0) {
            //print messages
            printf("%s Shutting down manager...\n", timebuf);
            printf("%s Waiting for all active workers to finish.\n", timebuf);
            fflush(stdout);
            //printing to be continued after "break" to actually wait for all workers to finish
            break;
    
        } else if (strcmp(instruction, "cancel") == 0) {


            printf("%s Canceling operation for %s\n", timebuf, arg1);

            //find the entry in sync_list and set active to 0
            sync_info_mem_store* entry = exists_sync_entry(sync_list, source_dir, NULL);
            if (entry != NULL) {
                entry->active = 0; //set active to 0
                entry->last_sync_time = time(NULL); //update last sync time
                entry->error_count = 0; //reset error count
                printf("%s Operation for %s canceled.\n", timebuf, arg1);
            } else {
                printf("%s No active operation found for %s.\n", timebuf, arg1);
            }


        } else if (strcmp(instruction, "add") == 0) {
            if (worker_count < worker_limit) {

                if (exists_in_queue(worker_queue, arg1)) {
                    printf("%s Already in queue: %s\n", timebuf, arg1);
                    write(client_fd, "Already in queue\n", strlen("Already in queue\n"));

                }
                else {
                    //add to sync_list and start worker
                    sync_list = add_sync_entry(&sync_list, source_dir, target_dir, source_host, source_port, target_host, target_port); //add at the end
                    sync_info_mem_store* current = exists_sync_entry(sync_list, source_dir, target_dir);  //get the ptr to the new entry

                    if (current == NULL) {
                        fprintf(stderr, "Entry already exists\n");
                        break;
                    }
                    current->active = 1;
                    current->last_sync_time = time(NULL);
                    current->error_count = 0;

                    //write to log file in worker initialization
                    start_worker(current, log_file);
                    worker_count++;
                    printf("%s Added file: %s -> %s\n", timebuf, arg1, arg2);
                    char write_buf[1024];
                    int len = snprintf(write_buf, sizeof(write_buf), "%s Added file: %s -> %s\n", timebuf, arg1, arg2);
                    write(client_fd, write_buf, len); // len is string length
                }
            }

        }
    
//TO DELETE WHEN I HAVE MESSAGEs EVERYWHERE
        strcpy(response, "ok\n");
        //write response to the socket
        snprintf(response, sizeof(response), "MANAGER: %s\n", input);
        if (write(client_fd, response, strlen(response)) < 0) {
            perror("write failed");
            break;
        }


    //     //SIGCHLD handler

    }


    //close sockets
    close(server_fd);
    close(new_socket);

    //close log file
    fclose(log_file);

    //handle shutdown wait for all workers to finish

    //clean up
    while (worker_queue != NULL) {
        WorkerQueue * cur = queue_pop(&worker_queue);
        if (cur != NULL) {
            free(cur);
        }
    }


    //wait for all child processes to finish
    for (int i = 0; i < worker_count; i++) {
        waitpid(worker_array[i], NULL, 0);
    }


    while (sync_list != NULL) {
        delete_sync_entry(&sync_list, sync_list->source_dir);
    }
    free(worker_array); //free the worker array



    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
        

    printf("%s Manager shutdown complete.\n", timebuf);
    fflush(stdout);
    return 0;
}