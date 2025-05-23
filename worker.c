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
#include <dirent.h>



#define MAX_LINE 1024
#define REPORT_SIZE 4000
char report[REPORT_SIZE]; //buffer for report
int files_copied; //number of files copied
int files_deleted; //number of files deleted
int files_skipped; //number of files that had no operation done to them
int errors; //number of errors
char error_messages[40000]; //error message
//errors are stored in a buffer and are separated by newlines

void delete_file(const char* src, const char* tgt, const char* filename) {
    char full_src[PATH_MAX];
    char full_target[PATH_MAX];

    //create full path for source and target files 
    snprintf(full_src, sizeof(full_src), "%s/%s", src, filename);
    snprintf(full_target, sizeof(full_target), "%s/%s", tgt, filename); //name same as src

    if (unlink(full_target) == -1) {
        errors++;
        //write error message to error buffer (append it)
        snprintf(error_messages + strlen(error_messages), sizeof(error_messages) - strlen(error_messages), "-file %s: %s\n", full_target, strerror(errno));
        return;
    } else {
        files_deleted++;
        //write to report
        snprintf(report + strlen(report), sizeof(report) - strlen(report), "deleted %s\n", full_target);
    }
}

void sync_file(const char* src, const char* tgt, const char* filename) {
    char full_src[PATH_MAX];
    char full_target[PATH_MAX];

    //create full path for source and target files 
    snprintf(full_src, sizeof(full_src), "%s/%s", src, filename);
    snprintf(full_target, sizeof(full_target), "%s/%s", tgt, filename); //name same as src

    int fd_src = open(full_src, O_RDONLY);
    if (fd_src == -1) {
        errors++;
        //write error message to error buffer (append it)
        snprintf(error_messages + strlen(error_messages), sizeof(error_messages) - strlen(error_messages), "-file %s: %s\n", full_src, strerror(errno));
        return;
    }
    int fd_tgt = open(full_target, O_WRONLY | O_CREAT | O_TRUNC, 0644); //open target file for overwriting or create for writing
    if (fd_tgt == -1) {
        //mention error
        errors++;
        //write error message to error buffer (append it)
        snprintf(error_messages + strlen(error_messages), sizeof(error_messages) - strlen(error_messages), "-file %s: %s\n", full_target, strerror(errno));
        close(fd_src);
        return;
    }

    //do the actual copying

    ssize_t bytes_written;
    ssize_t bytes_read;
    char buffer[4096];
    while (1) {     //use a buffer of 4096 bytes to read from source file nd write to target file
        bytes_read = read(fd_src, buffer, sizeof(buffer));
        if (bytes_read == 0) break; //EOF, done reading

        if (bytes_read == -1) {
            //mention error
            errors++;
            //write error message to error buffer (append it)
            snprintf(error_messages + strlen(error_messages), sizeof(error_messages) - strlen(error_messages), "-file %s: %s\n", full_src, strerror(errno));
            break;
        }

        bytes_written = write(fd_tgt, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            //mention error
            errors++;
            //write error message to error buffer (append it)
            snprintf(error_messages + strlen(error_messages), sizeof(error_messages) - strlen(error_messages), "-file %s: %s\n", full_target, strerror(errno));
            break;
        }
        //all good, increment the number of files copied
        files_copied++;
        //write to report
        snprintf(report + strlen(report), sizeof(report) - strlen(report), "copied %s\n", full_target);

    }
    close(fd_src);
    close(fd_tgt);
    return;
}



void op_all(const char* src, const char* tgt,  const char* op) {
    DIR * src_dir = opendir(src);
    DIR * tgt_dir = opendir(tgt);
    if (src_dir == NULL) return;    //source directory must exist
    struct dirent *file;

    while ((file = readdir(src_dir)) != NULL) {   //while there are files in the source directory
        if (file->d_ino == 0) continue; //skip empty entries
        //copy the file from src to tgt

        char* filename = file->d_name;
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) continue;

        if (strcmp(op, "DELETED") == 0) 
            delete_file(src, tgt, filename); //delete the file from the target directory
        else if (strcmp(op, "ADDED") == 0 || strcmp(op, "MODIFIED") == 0 || strcmp(op, "FULL") == 0)
            sync_file(src, tgt, filename); //sync the file from the source directory to the target directory
        else {}//mention error
        //move on to the next file
    }
    closedir(src_dir);
    closedir(tgt_dir);
    return;
}



int main(int argc, char* argv[]) {  //assuming the worker is called by fss_manager (correctly)
    char* src = argv[1];
    char* tgt = argv[2];
    char* filename = argv[3];
    char* operation = argv[4];

    errors = 0;
    files_copied = 0;
    files_deleted = 0;
    files_skipped = 0;
    memset(report, 0, sizeof(report)); //clear the report buffer
    memset(error_messages, 0, sizeof(error_messages)); //clear the error message buffer

    if (strcmp(filename, "ALL") == 0) {
        op_all(src, tgt, operation); //sync all files in the source directory
    }
    else {
        if (strcmp(operation, "ADDED") == 0) {
            sync_file(src, tgt, filename); //sync the file from the source directory to the target directory
        } else if (strcmp(operation, "FULL") == 0){ 
            errors++;
            snprintf(error_messages + strlen(error_messages), sizeof(error_messages) - strlen(error_messages), "-file %s: %s\n", filename, "FULL operation not supported on not ALL files");
        } else if (strcmp(operation, "DELETED") == 0) {
            delete_file(src, tgt, filename); //delete the file from the target directory
        } else if (strcmp(operation, "MODIFIED") == 0) {
            sync_file(src, tgt, filename); //sync the file from the source directory to the target directory
        }
    }

    //print to stdout that will go to the fss_manager
    printf("EXEC_REPORT_START\n");

    if (errors == 0) printf("STATUS: SUCCESS\n");
    else printf("STATUS: PARTIAL\n");

    printf("DETAILS: %d files copied, %d skipped\n", files_copied, files_skipped);

    if (errors > 0) printf("ERRORS:\n%s", error_messages);
    
    printf("EXEC_REPORT_END\n");
    fflush(stdout);

    return 0;
}