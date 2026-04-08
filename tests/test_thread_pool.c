#include "../include/thread_pool.h"
#include "../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* Test data structure */
typedef struct test_data {
    int value;
    int *counter;
    pthread_mutex_t *mutex;
} test_data_t;

/* Simple task handler that increments a counter */
void increment_handler(void *data) {
    test_data_t *td = (test_data_t *)data;
    pthread_mutex_lock(td->mutex);
    (*td->counter)++;
    pthread_mutex_unlock(td->mutex);
    free(td);
}

/* Test basic thread pool initialization and destruction */
void test_init_destroy() {
    printf("Test 1: Thread pool initialization and destruction...\n");
    
    /* Create task queue first */
    task_queue_t queue;
    int result = task_queue_init(&queue, 100);
    assert(result == ERR_SUCCESS);
    
    thread_pool_t pool;
    result = thread_pool_init(&pool, 4, &queue);
    assert(result == ERR_SUCCESS);
    assert(pool.num_threads == 4);
    assert(pool.threads != NULL);
    assert(pool.queue != NULL);
    assert(pool.shutdown == false);
    
    thread_pool_shutdown(&pool);
    assert(pool.shutdown == true);
    
    thread_pool_destroy(&pool);
    task_queue_destroy(&queue);
    
    printf("  ✓ Passed\n");
}

/* Test task submission and execution */
void test_task_execution() {
    printf("Test 2: Task submission and execution...\n");
    
    /* Create task queue first */
    task_queue_t queue;
    int result = task_queue_init(&queue, 200);
    assert(result == ERR_SUCCESS);
    
    thread_pool_t pool;
    result = thread_pool_init(&pool, 4, &queue);
    assert(result == ERR_SUCCESS);
    
    int counter = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    
    /* Submit 100 tasks */
    for (int i = 0; i < 100; i++) {
        task_t *task = malloc(sizeof(task_t));
        test_data_t *td = malloc(sizeof(test_data_t));
        td->value = i;
        td->counter = &counter;
        td->mutex = &mutex;
        
        task->type = TASK_MESSAGE_PROCESS;
        task->data = td;
        task->handler = increment_handler;
        task->next = NULL;
        
        result = thread_pool_submit(&pool, task);
        assert(result == ERR_SUCCESS);
    }
    
    /* Wait for tasks to complete */
    sleep(1);
    
    pthread_mutex_lock(&mutex);
    int final_count = counter;
    pthread_mutex_unlock(&mutex);
    
    assert(final_count == 100);
    
    thread_pool_shutdown(&pool);
    thread_pool_destroy(&pool);
    task_queue_destroy(&queue);
    pthread_mutex_destroy(&mutex);
    
    printf("  ✓ Passed (processed %d tasks)\n", final_count);
}

/* Test invalid parameters */
void test_invalid_params() {
    printf("Test 3: Invalid parameters...\n");
    
    thread_pool_t pool;
    task_queue_t queue;
    
    /* Initialize valid queue */
    int result = task_queue_init(&queue, 100);
    assert(result == ERR_SUCCESS);
    
    /* NULL pool */
    assert(thread_pool_init(NULL, 4, &queue) == ERR_INVALID_PARAM);
    
    /* Invalid thread count */
    assert(thread_pool_init(&pool, 0, &queue) == ERR_INVALID_PARAM);
    assert(thread_pool_init(&pool, -1, &queue) == ERR_INVALID_PARAM);
    
    /* NULL queue */
    assert(thread_pool_init(&pool, 4, NULL) == ERR_INVALID_PARAM);
    
    /* Initialize valid pool */
    result = thread_pool_init(&pool, 2, &queue);
    assert(result == ERR_SUCCESS);
    
    /* NULL task */
    assert(thread_pool_submit(&pool, NULL) == ERR_INVALID_PARAM);
    
    /* NULL pool for submit */
    task_t task;
    assert(thread_pool_submit(NULL, &task) == ERR_INVALID_PARAM);
    
    thread_pool_shutdown(&pool);
    thread_pool_destroy(&pool);
    task_queue_destroy(&queue);
    
    printf("  ✓ Passed\n");
}

/* Test shutdown behavior */
void test_shutdown() {
    printf("Test 4: Shutdown behavior...\n");
    
    /* Create task queue first */
    task_queue_t queue;
    int result = task_queue_init(&queue, 100);
    assert(result == ERR_SUCCESS);
    
    thread_pool_t pool;
    result = thread_pool_init(&pool, 4, &queue);
    assert(result == ERR_SUCCESS);
    
    int counter = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    
    /* Submit tasks */
    for (int i = 0; i < 50; i++) {
        task_t *task = malloc(sizeof(task_t));
        test_data_t *td = malloc(sizeof(test_data_t));
        td->value = i;
        td->counter = &counter;
        td->mutex = &mutex;
        
        task->type = TASK_MESSAGE_PROCESS;
        task->data = td;
        task->handler = increment_handler;
        task->next = NULL;
        
        thread_pool_submit(&pool, task);
    }
    
    /* Shutdown should wait for queued tasks */
    thread_pool_shutdown(&pool);
    
    pthread_mutex_lock(&mutex);
    int final_count = counter;
    pthread_mutex_unlock(&mutex);
    
    /* All tasks should complete before shutdown returns */
    assert(final_count == 50);
    
    /* Submitting after shutdown should fail */
    task_t *task = malloc(sizeof(task_t));
    task->handler = NULL;
    task->data = NULL;
    assert(thread_pool_submit(&pool, task) == ERR_THREAD_ERROR);
    free(task);
    
    thread_pool_destroy(&pool);
    task_queue_destroy(&queue);
    pthread_mutex_destroy(&mutex);
    
    printf("  ✓ Passed (all %d tasks completed before shutdown)\n", final_count);
}

int main() {
    printf("Running thread pool tests...\n\n");
    
    test_init_destroy();
    test_task_execution();
    test_invalid_params();
    test_shutdown();
    
    printf("\n✓ All tests passed!\n");
    return 0;
}
