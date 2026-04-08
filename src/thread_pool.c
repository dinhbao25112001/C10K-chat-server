#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

/* Forward declaration of worker thread function */
static void* worker_thread(void *arg);

int thread_pool_init(thread_pool_t *pool, int num_threads, task_queue_t *queue) {
    if (!pool || num_threads <= 0 || !queue) {
        return ERR_INVALID_PARAM;
    }

    /* Use external task queue */
    pool->queue = queue;

    /* Allocate thread array */
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        return ERR_MEMORY_ALLOC;
    }

    pool->num_threads = num_threads;
    pool->shutdown = false;

    /* Create worker threads */
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            /* Cleanup already created threads */
            pool->shutdown = true;
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            return ERR_THREAD_ERROR;
        }
    }

    return ERR_SUCCESS;
}

static void* worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1) {
        /* Dequeue task with timeout to check shutdown flag periodically */
        task_t *task = task_queue_dequeue_timeout(pool->queue, 100);

        /* Process task if available */
        if (task) {
            if (task->handler) {
                task->handler(task->data);
            }
            free(task);
        }

        /* Check shutdown flag after processing task */
        if (pool->shutdown && task_queue_size(pool->queue) == 0) {
            break;
        }
    }

    return NULL;
}

int thread_pool_submit(thread_pool_t *pool, task_t *task) {
    if (!pool || !task) {
        return ERR_INVALID_PARAM;
    }

    if (pool->shutdown) {
        return ERR_THREAD_ERROR;
    }

    /* Enqueue task to the queue */
    return task_queue_enqueue(pool->queue, task);
}

void thread_pool_shutdown(thread_pool_t *pool) {
    if (!pool) {
        return;
    }

    /* Set shutdown flag */
    pool->shutdown = true;

    /* Wait for all worker threads to complete */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool) {
        return;
    }

    /* Ensure shutdown was called */
    if (!pool->shutdown) {
        thread_pool_shutdown(pool);
    }

    /* Free thread array */
    if (pool->threads) {
        free(pool->threads);
        pool->threads = NULL;
    }

    /* Note: task queue is external, not freed here */
    pool->queue = NULL;
}
