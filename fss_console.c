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

int main(int argc, char *argv[]) {

    if (argc != 3 || strcmp(argv[1], "-l") != 0) {
        fprintf(stderr, "Please give input in the form ./fss_console -l <console-logfile>");
        return 1;
    }

    char *logfile = argv[2];
    FILE *log_file = fopen(logfile, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return 1;
    }

    printf("Opening fss_in for writing...\n");
    int fd_in = open("fss_in", O_WRONLY);
    printf("fss_in opened!\n");
    
    if (fd_in < 0) {
        perror("failed to open fss_in");
        fclose(log_file);
        exit(1);
    }

    int fd_out = open("fss_out", O_RDONLY);
    if (fd_out < 0) {
        perror("failed to open fss_out");
        fclose(log_file);
        close(fd_in);
        exit(1);
    }

    char input[MAX_LINE];
    char response[MAX_LINE];

    int break_flag = 0; //flag to break the loop

    while (1) {
        printf("Insert instruction:\n");
        fflush(stdout); //print immediately
        if (!fgets(input, sizeof(input), stdin)) {
            perror("Failed to read user input");
            break;
        }

        //keep a copy of input so that it can be passed on through the pipe 
        char copy[MAX_LINE];
        strcpy(copy, input);

        //turn new line to null terminator
        input[strcspn(input, "\n")] = '\0';

        char * instruction, *arg1, *arg2;
        instruction = strtok(input, " ");
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
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
        } else if (strcmp(instruction, "sync") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                fprintf(stderr, "CONSOLE received invalid instruction\n");
                continue;
            }
            //write to log file
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
                    
            fprintf(log_file, "%s Command sync %s\n", timebuf, arg1);   //write to log file
                    
            fflush(log_file); // flush to ensure it's written immediately
            //use pipe to send sync instruction
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
        } else if (strcmp(instruction, "status") == 0) {
            if (arg1 == NULL || arg2 != NULL) {
                fprintf(stderr, "CONSOLE received invalid instruction\n");
                continue;
            }
            //write to log file
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", t);   //get the corerct time and format
                    
            fprintf(log_file, "%s Command status", timebuf);   //write to log file
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
            fprintf(log_file, " %s %s\n", arg1, arg2);
            fflush(log_file); // flush to ensure it's written immediately

        } else {
            fprintf(stderr, "CONSOLE received invalid instruction\n");
            continue;
        }

        //write to pipe
        if (write(fd_in, copy, strlen(copy)) < 0) {
            perror("CONSOLE failed to write to fss_in");
            break;
        }

        //wait and read response from manager
        ssize_t n = read(fd_out, response, sizeof(response) - 1);
        if (n<=0) break;    //response must be "all good" therefore not empty
        if (break_flag) break;
    }

    close(fd_in);
    close(fd_out);



    return(0);
}
