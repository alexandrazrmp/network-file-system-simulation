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
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_LINE 1024

int main(int argc, char *argv[]) {

    if (argc != 7) {
        fprintf(stderr, "Please give input in the form ./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>");
        return 1;
    }

    char* log_file_ = NULL;
    char* host_ip_ = NULL;
    char *host_port_ = NULL;


    // Parse arguments
    int option;
    while ((option = getopt(argc, argv, "l:h:p:")) != -1) {
        switch (option) {
            case 'l': log_file_ = optarg; break;
            case 'h': host_ip_ = optarg; break;
            case 'p': host_port_ = optarg; break;

            default:
                fprintf(stderr, "Please give input in the form ./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>\n");
                exit(1);
        }
    }

    if (!log_file_ || !host_ip_ || !host_port_) {
        fprintf(stderr, "Missing arguments.\n");
        return 1;
    }


    FILE *log_file = fopen(log_file_, "a");
    if (log_file == NULL) {
        perror("failed to open log file");
        return 1;
    }

    int host_port = atoi(host_port_);
    if (host_port <= 0 || host_port > 65535) {
        fprintf(stderr, "invalid port number: %d\n", host_port);
        fclose(log_file);
        return 1;
    }

    //create and connect socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        fclose(log_file);
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(host_port);
    if (inet_pton(AF_INET, host_ip_, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "invalid host IP: %s\n", host_ip_);
        close(sockfd);
        fclose(log_file);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        fclose(log_file);
        return 1;
    }

    char input[MAX_LINE];
    char response[MAX_LINE];
    FILE *sockf = fdopen(sockfd, "r+"); //read-write
    if (!sockf) {
        perror("fdopen failed");
        close(sockfd);
        fclose(log_file);
        return 1;
    }

    int break_flag = 0; //flag to break the loop

    while (1) {
        printf("Insert instruction:\n");
        fflush(stdout); //print immediately
        if (!fgets(input, sizeof(input), stdin)) {
            perror("Failed to read user input");
            break;
        }

        //keep a copy of input so that it can be passed on through the socket
        char copy[MAX_LINE];
        strcpy(copy, input);

        //turn new line to null terminator
        input[strcspn(input, "\n")] = '\0';

        char * instruction, *arg1, *arg2;
        instruction = strtok(input, " ");
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");

        char source_dir[PATH_MAX] = {0}, source_host[64] = {0};
        int source_port = 0;
        char target_dir[PATH_MAX] = {0}, target_host[64] = {0};
        int target_port = 0;

        if (arg1 != NULL) {
            int a = sscanf(arg1, "/%[^@]@%[^:]:%d", source_dir, source_host, &source_port) ; 
            if (a != 3) {
                fprintf(stderr, "CONSOLE recieved invalid input format\n");
                continue;
            }
        }
        
        if (arg2 != NULL) {
            int a = sscanf(arg2, "/%[^@]@%[^:]:%d", target_dir, target_host, &target_port) ; 
            if (a != 3) {
                fprintf(stderr, "CONSOLE recieved invalid input format\n");
                continue;
            }
        }

        if (strcmp(instruction, "shutdown") == 0) {
            if (arg1 != NULL) {
                fprintf(stderr, "CONSOLE received invalid shutdown instruction\n");
                continue;
            }
            //write to log file
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
        
            fprintf(log_file, "%s Command shutdown\n", timebuf);   //write to log file

            break_flag = 1; //set the flag to break the loop
        } else if (strcmp(instruction, "cancel") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                fprintf(stderr, "INVALID INSTRUCTION\n");
                continue;
            }
            //write to log file
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
                    
            fprintf(log_file, "%s Command cancel", timebuf);   //write to log file
            fprintf(log_file, " %s\n", arg1);
                    
            fflush(log_file); // flush to ensure it's written immediately
        } else if (strcmp(instruction, "add") == 0) {
            if (arg1 == NULL || arg2 == NULL) {
                fprintf(stderr, "CONSOLE received invalid instruction\n");
                continue;
            }
            //write to log file
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
                    
            fprintf(log_file, "%s Command add", timebuf);   //write to log file
            fprintf(log_file, " %s -> %s\n", arg1, arg2);
            fflush(log_file); // flush to ensure it's written immediately

        } else {
            fprintf(stderr, "CONSOLE received invalid instruction\n");
            continue;
        }

        //write to the socket
        dprintf(sockfd, "%s\n", copy);

        //wait and read responses from manager
        FILE *sockf = fdopen(sockfd, "r");
        if (!sockf) {
            perror("fdopen");
            close(sockfd);
            fclose(log_file);
            return 1;
        }

        while (fgets(response, sizeof(response), sockf)) {
            if (strcmp(response, "END\n") == 0)
                break;

            printf("%s", response);
            fflush(stdout); // print each line immediately
        }


        if (break_flag) {
            close(sockfd); //close the socket
            fclose(log_file); //close the log file
            break; //break the loop
        }


    }

    //nothing to write


    return(0);
}
