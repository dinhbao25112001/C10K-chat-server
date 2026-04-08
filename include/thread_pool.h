#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"
#include "task_queue.h"
#include <pthread.h>

typedef struct thread_pool {
    pthread_t *threads;
    int num_threads;
    task_queue_t *queue;
    volatile bool shutdown;
} thread_pool_t;

/* Initialize thread pool with specified number of workers and external task queue */
int thread_pool_init(thread_pool_t *pool, int num_threads, task_queue_t *queue);

/* Submit a task to the pool */
int thread_pool_submit(thread_pool_t *pool, task_t *task);

/* Shutdown pool gracefully (complete queued tasks) */
void thread_pool_shutdown(thread_pool_t *pool);

/* Destroy pool and free resources */
void thread_pool_destroy(thread_pool_t *pool);

#endif /* THREAD_POOL_H */
