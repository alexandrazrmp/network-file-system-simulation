#include "nfs_manager.h"

int worker_count = 0; //number of workers currently running                     //same as active_workers
int worker_limit = 0; //maximum number of workers allowed
pthread_t* worker_thread_pool; //thread pool for worker threads

int stop_worker_handler = 0; //flag to stop the worker handler thread

sync_info_mem_store* sync_list = NULL;

WorkerQueue* worker_queue = NULL;


//mutex to handle worker count
pthread_mutex_t worker_count_mutex = PTHREAD_MUTEX_INITIALIZER;

//condition that allows a thread to start when another worker thread finishes
pthread_cond_t worker_count_cond = PTHREAD_COND_INITIALIZER; //condition variable to wait for a worker to finish



//signal handler for SIGCHLD
volatile sig_atomic_t child_exited = 0; //flag to start a queued worker

//wait for all dead child processes
void sigchld_handler ( int sig ) {
    while ( waitpid ( -1 , NULL , WNOHANG ) > 0) ;
}

//parses config file and adds entries to sync_list
void parse_config_file(FILE* file) {
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
            if (current == NULL) {
                fprintf(stderr, "Source directory already linked to a Target direectory for synchronization\n");
                continue; //ignore
            }

        }
        else {
            fprintf(stderr, "invalid entry in config file\n");
            continue; //ignore 
        }
    }
    
    fclose(file);
}

//worker_function synchronizes source and target files 
//worker function to be run in a separate thread
void* worker_function(void* arg) {
    WorkerQueue* worker = (WorkerQueue*)arg; //cast arg to WorkerQueue pointer

    //example
    printf("Worker for %s : %s started.\n", worker->source_dir, worker->filename);
















    sleep(5); //simulate work being done
    printf("Worker for %s : %s finished.\n", worker->source_dir, worker->filename);

    //signal that this worker is done

    pthread_mutex_lock(&worker_count_mutex);
    worker_count--; //decrease worker count
    pthread_cond_signal(&worker_count_cond); //signal that a worker is done
    pthread_mutex_unlock(&worker_count_mutex);

    return NULL;
}


void* worker_handler(void* arg) {
    //worker handler to handle the worker threads
    worker_queue = (WorkerQueue*)arg; //cast arg to WorkerQueue pointer
    WorkerQueue* cur = worker_queue; //current worker in the queue

    while (1) {                 //loop until program ends

        //wait for a worker to finish
        pthread_mutex_lock(&worker_count_mutex);
        if (worker_count >= worker_limit) { //if worker count is at limit, wait for a worker to finish
            pthread_cond_wait(&worker_count_cond, &worker_count_mutex);
        }
        pthread_mutex_unlock(&worker_count_mutex);

        //check if we need to stop the worker handler thread
        if (stop_worker_handler) {
            break; //exit the loop if stop_worker_handler is set
        }

//must wait on queue pop to avoid busy waiting (will fix later)


        //if there is a worker in the queue, pop it and start a thread
        if ((cur = queue_pop(&worker_queue)) != NULL) {  //if there is a worker in the queue
            pthread_mutex_lock(&worker_count_mutex);
            worker_count++;
            pthread_mutex_unlock(&worker_count_mutex);
            //create a thread for the worker
            if (pthread_create(&worker_thread_pool[worker_count - 1], NULL, worker_function, cur) != 0) {
                printf("pthread_create failed for worker thread\n");
                free(cur); //free the worker queue node
                pthread_mutex_lock(&worker_count_mutex);
                worker_count--;
                pthread_mutex_unlock(&worker_count_mutex);
            }
        }
    }

    return NULL;
}




//get list achieves the following:
//connects to the source directory and gets the list of files
//checks if the file already exists in the queue
//if not, adds the file to the queue

void get_list(sync_info_mem_store* entry, FILE* log_file, int console_fd) {
    if (!entry || !log_file) {
        fprintf(stderr, "Invalid entry or log file\n");
        return;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);

    //SOURCE DIRECTORY
    int host_port = entry->source_port;
    char *host_ip_ = entry->source_host;

    if (host_port <= 0 || host_port > 65535) {
        fprintf(stderr, "invalid port number: %d\n", host_port);
        return;
    }

    //create and connect socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(host_port);
    if (inet_pton(AF_INET, host_ip_, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "invalid host IP: %s\n", host_ip_);
        close(sockfd);
        return;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return;
    }


    FILE *sockf = fdopen(sockfd, "r+"); //read-write
    if (!sockf) {
        perror("fdopen failed");
        close(sockfd);
        return;
    }

    //LIST

    //send initial command to the client
    fprintf(sockf, "LIST %s\n", entry->source_dir);
    fflush(sockf);


    //get response line by line until "."
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), sockf)) {
        line[strcspn(line, "\n")] = '\0';   //remove newline if present)
        if (strcmp(line, ".") == 0) break;
        //if line starts with . continue;
        if (line[0] == '.') {
            continue; //skip lines starting with . that are not just a dot
        }

        //check if the file already exists in the queue
        if (exists_in_queue(worker_queue, entry->source_dir, entry->target_dir, (const char*)line)) {   //line is the filename
            printf("%s Already in queue: %s\n", timebuf, line);
            fflush(stdout);
            //write to console
            if (console_fd >= 0) { //if console_fd is valid
                char write_buf[1024];
                int len = snprintf(write_buf, sizeof(write_buf), "%s Already in queue: %s\n", timebuf, line);
                write(console_fd, write_buf, len); // len is string length
                continue; //skip if it already exists in the queue
            }
        }

        //push the filename to queue
        worker_queue = queue_push(worker_queue, entry->source_dir, entry->source_host, entry->source_port,
            entry->target_dir, entry->target_host, entry->target_port, (const char*)line); //push to the queue

        char full_source_file[PATH_MAX] = {0}, full_target_file[PATH_MAX] = {0};
        snprintf(full_source_file, sizeof(full_source_file)*3, "/%s/%s@%s:%d", entry->source_dir, (char*)line, entry->source_host, entry->source_port);
        snprintf(full_target_file, sizeof(full_target_file)*3, "/%s/%s@%s:%d", entry->target_dir, (char*)line, entry->target_host, entry->target_port);

        printf("%s Added file: %s -> %s\n", timebuf, full_source_file, full_target_file);
        fflush(stdout);
        //write to the socket to the console
        if (console_fd >= 0) { //if console_fd is valid
            char write_buf[1024];
            int len = snprintf(write_buf, sizeof(write_buf), "%s Added file: %s -> %s\n", timebuf, full_source_file, full_target_file);
            write(console_fd, write_buf, len); // len is string length
        }
        //also write to log file
        fprintf(log_file, "%s Added file: %s -> %s\n", timebuf, full_source_file, full_target_file);
        fflush(log_file); // flush to ensure it's written immediately

    }

    fclose(sockf);  //also closes sockfd

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


    if (!log_file_ || !config_file_) {
        fprintf(stderr, "Input error\n");
        exit(1);
    }

    //cast file pointer to FILE* (previously char* for parsing)
    FILE* log_file = fopen(log_file_, "a");
    FILE* config_file = fopen(config_file_, "r");

    printf("FSS Manager started with a limit of %d workers.\n", worker_limit);



    //read config file and add entries to sync_list
    parse_config_file(config_file);

    //initialize thread pool
    worker_thread_pool = malloc(sizeof(pthread_t) * worker_limit);

    //do the initial sync
    sync_info_mem_store* current = sync_list;

    while (current != NULL) {
        current->active = 1;
        current->last_sync_time = time(NULL);
        current->error_count = 0;
        //prepare to start worker for each entry in sync_list and also write to log file
        get_list(current, log_file, -1); //-1 means no console_fd, as we are not connected to the console yet
        current = current->next;
    }



    //start worker handler thread (a single thread to handle all workers)
    pthread_t worker_handler_thread;
    if (pthread_create(&worker_handler_thread, NULL, worker_handler, worker_queue) != 0) {
        printf("pthread_create failed for worker handler thread\n");
        fclose(log_file);
        fclose(config_file);
        exit(1);
    }


    //connect to the console
    //socket creation on port number

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;


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


    //accept nfs_console connection
    int console_fd = accept(server_fd, NULL, NULL);
    if (console_fd < 0) {
        perror("accept");
        exit(1);
    }
    printf("Console-Manager connection achieved\n");

    close(server_fd); //close the server socket as we don't need it anymore


    char input[MAX_LINE];
    char response[MAX_LINE];


    while (1) {              //get console input and handle it when it arrives


        ssize_t n = read(console_fd, input, sizeof(input)-1);
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
            if (a != 3) {   //but still, if it is not, break
                fprintf(stderr, "invalid input format\n");
                break;
            }
        }
        
        if (arg2 != NULL) {
            int a = sscanf(arg2, "/%[^@]@%[^:]:%d", target_dir, target_host, &target_port) ; 
            //input is corerct from console, so we can assume it is valid
            if (a != 3) {   //but still, if it is not, break
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
            fflush(stdout);
            //send message to the console
            snprintf(response, sizeof(response), "%s Shutting down manager...\n", timebuf);
            if (write(console_fd, response, strlen(response)) < 0) {
                perror("write failed");
            }
            printf("%s Waiting for all active workers to finish.\n", timebuf);
            fflush(stdout);
            //send message to the console
            snprintf(response, sizeof(response), "%s Waiting for all active workers to finish.\n", timebuf);
            if (write(console_fd, response, strlen(response)) < 0) {
                perror("write failed");
            }
            //printing to be continued after "break" to actually wait for all workers to finish
            break;
    
        } else if (strcmp(instruction, "cancel") == 0) {

            //find the entry in sync_list and set active to 0
            sync_info_mem_store* entry = exists_sync_entry(sync_list, source_dir, NULL);
            if (entry != NULL) {
                entry->active = 0; //set active to 0
                entry->last_sync_time = time(NULL); //update last sync time
                entry->error_count = 0; //reset error count

                printf("%s Synchronization stopped for %s\n", timebuf, arg1);
                fflush(stdout); //print immediately
                //write to logfile and send to console
                fprintf(log_file, "%s Synchronization stopped for %s\n", timebuf, arg1);
                fflush(log_file); // flush to ensure it's written immediately
                snprintf(response, sizeof(response), "%s Synchronization stopped for %s\n", timebuf, arg1);
                if (write(console_fd, response, strlen(response)) < 0) {
                    perror("write failed");
                }
            } else {
                printf("%s Directory not being synchronized: %s.\n", timebuf, arg1);
                fflush(stdout); //print immediately
                //send to console
                snprintf(response, sizeof(response), "%s Directory not being synchronized: %s.\n", timebuf, arg1);
                if (write(console_fd, response, strlen(response)) < 0) {
                    perror("write failed");
                }
            }

        } else if (strcmp(instruction, "add") == 0) {

            //add to sync_list and start worker
            sync_list = add_sync_entry(&sync_list, source_dir, target_dir, source_host, source_port, target_host, target_port); //add at the end
            sync_info_mem_store* current = exists_sync_entry(sync_list, source_dir, target_dir);  //get the ptr to the new entry

            if (current == NULL) {
                fprintf(stderr, "Entry already exists (this has \n");
                break;
            }
            current->active = 1;
            current->last_sync_time = time(NULL);
            current->error_count = 0;

            //write to log file in worker initialization
            get_list(current, log_file, console_fd);

        }
    

        //write to console that input is done
        snprintf(response, sizeof(response), "END\n");
        if (write(console_fd, response, strlen(response)) < 0) {
            perror("write failed");
        }


    }


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


//maybe have this in worker_handler thread

    //wait for all threads to finish using thread pool
    for (int i = 0; i < worker_count; i++) {
        if (pthread_join(worker_thread_pool[i], NULL) != 0) {   
            printf("pthread_join failed\n");
        }
    }
    free(worker_thread_pool); //free the thread pool
/////////


    //finish worker handler thread force it to exit
    stop_worker_handler = 1;    //set the flag to stop the worker handler thread
    pthread_join(worker_handler_thread, NULL); //wait for the worker handler thread to finish


    while (sync_list != NULL) {
        delete_sync_entry(&sync_list, sync_list->source_dir);
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
        

    printf("%s Manager shutdown complete.\n", timebuf);
    fflush(stdout);

    //send message to the console
    snprintf(response, sizeof(response), "%s Manager shutdown complete.\n", timebuf);
    if (write(console_fd, response, strlen(response)) < 0) {
        perror("write failed");
    }

    //write to console that input is done
    snprintf(response, sizeof(response), "END\n");
    if (write(console_fd, response, strlen(response)) < 0) {
        perror("write failed");
    }


    close(console_fd); //close the client socket

    return 0;
}