#include "task_queue.h"
#include <stdlib.h>
#include <errno.h>

int task_queue_init(task_queue_t *queue, int capacity) {
    if (!queue || capacity <= 0) {
        return ERR_INVALID_PARAM;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->capacity = capacity;

    /* Initialize mutex */
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return ERR_THREAD_ERROR;
    }

    /* Initialize condition variables */
    if (pthread_cond_init(&queue->cond_not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return ERR_THREAD_ERROR;
    }

    if (pthread_cond_init(&queue->cond_not_full, NULL) != 0) {
        pthread_cond_destroy(&queue->cond_not_empty);
        pthread_mutex_destroy(&queue->mutex);
        return ERR_THREAD_ERROR;
    }

    return ERR_SUCCESS;
}

int task_queue_enqueue(task_queue_t *queue, task_t *task) {
    if (!queue || !task) {
        return ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&queue->mutex);

    /* Block while queue is full */
    while (queue->size >= queue->capacity) {
        pthread_cond_wait(&queue->cond_not_full, &queue->mutex);
    }

    /* Add task to tail of queue */
    task->next = NULL;
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    queue->size++;

    /* Signal that queue is not empty */
    pthread_cond_signal(&queue->cond_not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return ERR_SUCCESS;
}

task_t* task_queue_dequeue(task_queue_t *queue) {
    if (!queue) {
        return NULL;
    }

    pthread_mutex_lock(&queue->mutex);

    /* Block while queue is empty */
    while (queue->size == 0) {
        pthread_cond_wait(&queue->cond_not_empty, &queue->mutex);
    }

    /* Remove task from head of queue */
    task_t *task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;

    /* Signal that queue is not full */
    pthread_cond_signal(&queue->cond_not_full);
    pthread_mutex_unlock(&queue->mutex);

    return task;
}

task_t* task_queue_dequeue_timeout(task_queue_t *queue, int timeout_ms) {
    if (!queue || timeout_ms < 0) {
        return NULL;
    }

    pthread_mutex_lock(&queue->mutex);

    /* Calculate absolute timeout */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    /* Wait with timeout while queue is empty */
    while (queue->size == 0) {
        int result = pthread_cond_timedwait(&queue->cond_not_empty, &queue->mutex, &ts);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }
        if (result != 0) {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }
    }

    /* Remove task from head of queue */
    task_t *task = queue->head;
    queue->head = task->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;

    /* Signal that queue is not full */
    pthread_cond_signal(&queue->cond_not_full);
    pthread_mutex_unlock(&queue->mutex);

    return task;
}

int task_queue_size(task_queue_t *queue) {
    if (!queue) {
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);
    int size = queue->size;
    pthread_mutex_unlock(&queue->mutex);

    return size;
}

void task_queue_destroy(task_queue_t *queue) {
    if (!queue) {
        return;
    }

    pthread_mutex_lock(&queue->mutex);

    /* Free all remaining tasks */
    task_t *current = queue->head;
    while (current) {
        task_t *next = current->next;
        free(current);
        current = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    pthread_mutex_unlock(&queue->mutex);

    /* Destroy synchronization primitives */
    pthread_cond_destroy(&queue->cond_not_full);
    pthread_cond_destroy(&queue->cond_not_empty);
    pthread_mutex_destroy(&queue->mutex);
}
