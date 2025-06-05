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
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>


#define MAX_LINE 4096


//function to list files in a directory and send them to the manager
void list(const char *src_dir, FILE *client_fp) {
    DIR *dir = opendir(src_dir);
    if (!dir) {
        fprintf(client_fp, "-1\n");
        fflush(client_fp);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; //skip . and ..
        }
        fprintf(client_fp, "%s\n", entry->d_name);
        fflush(client_fp);
    }

    fprintf(client_fp, ".\n");  //end of list
    fflush(client_fp);
    closedir(dir);
}


//function to pull a file from the source directory
void pull(const char *file, int client_fd) {
    //file is in the form of /source_dir/file
    //and /source_dir is the directory where the client is running

    //get the real path of the file same as this program

    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        perror("readlink failed");
        return;
    }
    exe_path[len] = '\0';  // Null-terminate the string

    //get the directory where the program is located
    char *dir_path = dirname(exe_path);

    //construct the full path to the file
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s%s", dir_path, file);

    printf("CLIENT: Pulling file: %s\n", file_path);

    
    //open the file for reading
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        perror("fopen failed");
        return;
    }

    //send the file to the client until it stops reading
    char buffer[1024* 1024]; //1 MB buffer
    size_t bytes_read = 0;
    bytes_read = fread(buffer, 1, sizeof(buffer)-1, fp);
    //add eof at the end of the file
    buffer[bytes_read] = '\0'; //null terminate the buffer

    //write to the manager
    write(client_fd, buffer, sizeof(buffer));


    //read the file in chunks and send to the client
    //if we reach the end of the file, we will stop reading and reset bytes_read to 0
    //this is to ensure that we can read the file in chunks and not send the whole file at once



    //close the file
    fclose(fp);

}



void push(const char *file_path, long chunk_size, FILE *client_fp, int client_fd) {
    return; //not implemented yet
}



int main(int argc, char *argv[]) {
    int port = 0;
    int opt;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Please give input in the form ./nfs_client -p <port_number>\n");
                exit(1);
        }
    }
    if (port <= 0 || port > 65535) {    //valid port numbers
        fprintf(stderr, "Please give input in the form ./nfs_client -p <port_number> with <port_number> in range 0..65535\n");
        exit(1);
    }

    int server_fd, client_fd;
    struct sockaddr_in addr;

    //create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(1);
    }

    int one = 1;

    //to reuse the address and port immiediately
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &one, sizeof(one))) {
        perror("setsockopt failed");
        exit(1);
    }

    //bind the socket to the specified port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    //listen for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("CLIENT: Listening on port %d...\n", port);
    fflush(stdout);

    //client can always be running in the background, accepting connections
    while (1) {
        //accept new connection
        if ((client_fd = accept(server_fd, NULL, NULL)) < 0) {
            perror("accept failed");
            continue;
        }
        printf("CLIENT: Client-Manager Conection achieved.\n");
        fflush(stdout);

        //turn socket into FILE*
        FILE *client_fp = fdopen(client_fd, "r+");
        if (!client_fp) {
            perror("fdopen failed");
            close(client_fd);
            continue;
        }

        char line[MAX_LINE];
        while (fgets(line, sizeof(line), client_fp)) {

            line[strcspn(line, "\n")] = 0; //remove newline if present

            //get the command and arguments
            char *cmd = strtok(line, " ");
            if (!cmd) {
                fprintf(client_fp, "Invalid input\n");
                fflush(client_fp);
                continue;
            }

            if (strcmp(cmd, "LIST") == 0) {
                char *src_dir = strtok(NULL, " ");
                if (!src_dir) {
                    fprintf(client_fp, "-1");
                    fflush(client_fp);
                    continue;
                }

                list(src_dir, client_fp);

            } else if (strcmp(cmd, "PULL") == 0) {
            
                char *file_path = strtok(NULL, " ");
                if (!file_path) {
                    fprintf(client_fp, "-1");
                    fflush(client_fp);
                    continue;
                }
                pull(file_path, client_fd);

            } else if (strcmp(cmd, "PUSH") == 0) {

                char *file_path = strtok(NULL, " ");
                char *chunk_str = strtok(NULL, " ");
                if (!file_path || !chunk_str) {
                    fprintf(client_fp, "-1");
                    fflush(client_fp);
                    continue;
                }
                errno = 0;
                long chunk_size = strtol(chunk_str, NULL, 10);
                if (errno != 0) {
                    fprintf(client_fp, "-1");
                    fflush(client_fp);
                    continue;
                }
                push(file_path, chunk_size, client_fp, client_fd);

            } else {
                fprintf(client_fp, "Invalid command\n");
                fflush(client_fp);
            }
        }

        //handle end of connection
        printf("nfs_client: Connection ended.\n");

        //clean up

        fclose(client_fp);
        fflush(stdout);
    }

    close(server_fd);
    return 0;
}
