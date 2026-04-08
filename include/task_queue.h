#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "common.h"
#include <pthread.h>

typedef enum task_type {
    TASK_AUTH,
    TASK_MESSAGE_PROCESS,
    TASK_MESSAGE_SEND,
    TASK_HISTORY_WRITE,
    TASK_CLEANUP
} task_type_t;

typedef struct task {
    task_type_t type;
    void *data;
    void (*handler)(void *data);
    struct task *next;
} task_t;

typedef struct task_queue {
    task_t *head;
    task_t *tail;
    int size;
    int capacity;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
} task_queue_t;

/* Initialize queue with capacity */
int task_queue_init(task_queue_t *queue, int capacity);

/* Enqueue task (blocks if full) */
int task_queue_enqueue(task_queue_t *queue, task_t *task);

/* Dequeue task (blocks if empty) */
task_t* task_queue_dequeue(task_queue_t *queue);

/* Try to dequeue with timeout */
task_t* task_queue_dequeue_timeout(task_queue_t *queue, int timeout_ms);

/* Get current queue size */
int task_queue_size(task_queue_t *queue);

/* Destroy queue */
void task_queue_destroy(task_queue_t *queue);

#endif /* TASK_QUEUE_H */
