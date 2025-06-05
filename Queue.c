#include "Queue.h"


//push to the front of the queue
WorkerQueue* queue_push(WorkerQueue *worker_queue, const char* src, const char* source_host, int source_port, const char* tgt, 
                        const char* target_host, int target_port, const char* filename) {
    WorkerQueue* new_worker = malloc(sizeof(WorkerQueue));
    strcpy(new_worker->source_dir, src);
    strcpy(new_worker->source_host, source_host);
    new_worker->source_port = source_port;
    strcpy(new_worker->target_dir, tgt);
    strcpy(new_worker->target_host, target_host);
    new_worker->target_port = target_port;
    strcpy(new_worker->filename, filename); //one file is pushed at a time
    new_worker->next = worker_queue;
    return new_worker;
}

//pop from the end of the queue
WorkerQueue* queue_pop(WorkerQueue **worker_queue) {
    if (worker_queue == NULL || *worker_queue == NULL) {
        return NULL;
    }
    WorkerQueue *cur = *worker_queue;

    if (cur->next == NULL) {
        *worker_queue = NULL;
        return cur;
    }
    while (cur->next->next != NULL) {
        cur = cur->next;
    }
    WorkerQueue *last = cur->next;
    cur->next = NULL;
    return last;
}

int exists_in_queue(WorkerQueue *worker_queue, const char* src, const char* tgt, const char* filename) {
    WorkerQueue *cur = worker_queue;

    while (cur != NULL) {
        if (strcmp(cur->source_dir, src) == 0 &&
            strcmp(cur->target_dir, tgt) == 0 &&
            strcmp(cur->filename, filename) == 0) {
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}