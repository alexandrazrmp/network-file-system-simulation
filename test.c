#include "nfs_manager.h"


int main() {
//test
///////////////////////////////////////////////////////////////////////////////////////////////////////////
    int host_port = 50000;
    char *host_ip_ = "127.0.0.1";

    if (host_port <= 0 || host_port > 65535) {
        fprintf(stderr, "invalid port number: %d\n", host_port);
        return 1;
    }

    //create and connect socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(host_port);
    if (inet_pton(AF_INET, host_ip_, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "invalid host IP: %s\n", host_ip_);
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return 1;
    }

    char input[MAX_LINE];
    char response[MAX_LINE];
    FILE *sockf = fdopen(sockfd, "r+"); //read-write
    if (!sockf) {
        perror("fdopen failed");
        close(sockfd);
        return 1;
    }

    //send initial command to the client
    strcpy(input, "PUSH\n");
    write(sockfd, input, strlen(input));
    fflush(sockf);
    read(sockfd, input, sizeof(input)-1);
    printf("%s\n", input);

    //close the socket
    close(sockfd);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}