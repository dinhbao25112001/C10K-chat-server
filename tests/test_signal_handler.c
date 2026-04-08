#include "../include/signal_handler.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

void test_signal_handler_init() {
    signal_handler_t handler;
    int result = signal_handler_init(&handler);
    assert(result == ERR_SUCCESS);
    assert(handler.shutdown_requested == 0);
    printf("✓ test_signal_handler_init passed\n");
}

void test_signal_handler_should_shutdown() {
    signal_handler_t handler;
    signal_handler_init(&handler);
    
    /* Initially should not be shutdown */
    assert(signal_handler_should_shutdown(&handler) == false);
    
    /* Manually set shutdown flag */
    handler.shutdown_requested = 1;
    assert(signal_handler_should_shutdown(&handler) == true);
    
    printf("✓ test_signal_handler_should_shutdown passed\n");
}

void test_signal_handler_start_stop() {
    signal_handler_t handler;
    int result;
    
    result = signal_handler_init(&handler);
    assert(result == ERR_SUCCESS);
    
    result = signal_handler_start(&handler);
    assert(result == ERR_SUCCESS);
    assert(handler.signal_thread != 0);
    
    /* Give thread time to start */
    usleep(10000);
    
    /* Stop the handler */
    signal_handler_stop(&handler);
    
    printf("✓ test_signal_handler_start_stop passed\n");
}

void test_signal_handler_sigint() {
    signal_handler_t handler;
    int result;
    
    result = signal_handler_init(&handler);
    assert(result == ERR_SUCCESS);
    
    result = signal_handler_start(&handler);
    assert(result == ERR_SUCCESS);
    
    /* Give thread time to start */
    usleep(10000);
    
    /* Send SIGINT to the process */
    kill(getpid(), SIGINT);
    
    /* Give thread time to process signal */
    usleep(50000);
    
    /* Check shutdown was requested */
    assert(signal_handler_should_shutdown(&handler) == true);
    
    signal_handler_stop(&handler);
    
    printf("✓ test_signal_handler_sigint passed\n");
}

void test_signal_handler_sigterm() {
    signal_handler_t handler;
    int result;
    
    result = signal_handler_init(&handler);
    assert(result == ERR_SUCCESS);
    
    result = signal_handler_start(&handler);
    assert(result == ERR_SUCCESS);
    
    /* Give thread time to start */
    usleep(10000);
    
    /* Send SIGTERM to the process */
    kill(getpid(), SIGTERM);
    
    /* Give thread time to process signal */
    usleep(50000);
    
    /* Check shutdown was requested */
    assert(signal_handler_should_shutdown(&handler) == true);
    
    signal_handler_stop(&handler);
    
    printf("✓ test_signal_handler_sigterm passed\n");
}

int main() {
    printf("Running signal_handler tests...\n\n");
    
    test_signal_handler_init();
    test_signal_handler_should_shutdown();
    test_signal_handler_start_stop();
    test_signal_handler_sigint();
    test_signal_handler_sigterm();
    
    printf("\n✓ All tests passed!\n");
    return 0;
}
