#include "Queue.h"

WorkerQueue* queue_create() {
    WorkerQueue* queue = malloc(sizeof(WorkerQueue));
    queue->next = NULL;
    return queue;
}

//push to the front of the queue
WorkerQueue* queue_push(WorkerQueue *worker_queue, const char* src, const char* tgt, const char* filename, const char* operation) {
    WorkerQueue* new_worker = malloc(sizeof(WorkerQueue));
    strcpy(new_worker->source_dir, src);
    strcpy(new_worker->target_dir, tgt);
    strcpy(new_worker->filename, filename);
    strcpy(new_worker->operation, operation);
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
