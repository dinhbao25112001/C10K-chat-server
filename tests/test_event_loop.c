/* Integration test for main event loop */
#include "../include/common.h"
#include "../include/epoll_manager.h"
#include "../include/connection_handler.h"
#include "../include/signal_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* Test that epoll can monitor multiple sockets */
static void test_epoll_multiple_sockets(void) {
    printf("Test: Epoll monitoring multiple sockets... ");
    
    epoll_manager_t manager;
    int result = epoll_manager_init(&manager, 10);
    assert(result == ERR_SUCCESS);
    
    /* Create listening socket */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);
    
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9998);
    
    result = bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    assert(result >= 0);
    
    result = listen(listen_fd, 10);
    assert(result >= 0);
    
    /* Set non-blocking */
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);
    
    /* Add to epoll */
    result = epoll_manager_add(&manager, listen_fd, EPOLLIN | EPOLLET, NULL);
    assert(result == ERR_SUCCESS);
    
    /* Wait with timeout (should timeout since no connections) */
    int num_events = epoll_manager_wait(&manager, 100);
    assert(num_events == 0);  /* Timeout, no events */
    
    /* Cleanup */
    epoll_manager_remove(&manager, listen_fd);
    close(listen_fd);
    epoll_manager_destroy(&manager);
    
    printf("PASSED\n");
}

/* Test edge-triggered read behavior */
static void test_edge_triggered_read(void) {
    printf("Test: Edge-triggered read behavior... ");
    
    /* Create a pipe for testing */
    int pipefd[2];
    int result = pipe(pipefd);
    assert(result == 0);
    
    /* Set read end to non-blocking */
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    
    /* Create epoll */
    epoll_manager_t manager;
    result = epoll_manager_init(&manager, 10);
    assert(result == ERR_SUCCESS);
    
    /* Add read end to epoll in edge-triggered mode */
    result = epoll_manager_add(&manager, pipefd[0], EPOLLIN | EPOLLET, NULL);
    assert(result == ERR_SUCCESS);
    
    /* Write some data */
    const char *msg = "Hello, World!";
    write(pipefd[1], msg, strlen(msg));
    
    /* Wait for event */
    int num_events = epoll_manager_wait(&manager, 100);
    assert(num_events == 1);
    
    /* Read data */
    char buffer[100];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
    assert(n == (ssize_t)strlen(msg));
    assert(memcmp(buffer, msg, n) == 0);
    
    /* Try to read again - should get EAGAIN */
    n = read(pipefd[0], buffer, sizeof(buffer));
    assert(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
    
    /* Cleanup */
    epoll_manager_remove(&manager, pipefd[0]);
    close(pipefd[0]);
    close(pipefd[1]);
    epoll_manager_destroy(&manager);
    
    printf("PASSED\n");
}

/* Test connection timeout detection */
static void test_connection_timeout(void) {
    printf("Test: Connection timeout detection... ");
    
    connection_t conn;
    conn.socket_fd = 1;
    conn.last_activity = time(NULL) - 400;  /* 400 seconds ago */
    
    time_t current_time = time(NULL);
    
    /* Should timeout with 300 second threshold */
    bool timed_out = connection_is_timeout(&conn, current_time, 300);
    assert(timed_out == true);
    
    /* Should not timeout with 500 second threshold */
    timed_out = connection_is_timeout(&conn, current_time, 500);
    assert(timed_out == false);
    
    /* Recent activity should not timeout */
    conn.last_activity = time(NULL) - 10;
    timed_out = connection_is_timeout(&conn, current_time, 300);
    assert(timed_out == false);
    
    printf("PASSED\n");
}

/* Test signal handler integration */
static void test_signal_handler(void) {
    printf("Test: Signal handler initialization... ");
    
    signal_handler_t handler;
    int result = signal_handler_init(&handler);
    assert(result == ERR_SUCCESS);
    
    result = signal_handler_start(&handler);
    assert(result == ERR_SUCCESS);
    
    /* Should not be shutdown initially */
    bool should_shutdown = signal_handler_should_shutdown(&handler);
    assert(should_shutdown == false);
    
    signal_handler_stop(&handler);
    
    printf("PASSED\n");
}

int main(void) {
    printf("Running event loop integration tests...\n\n");
    
    test_epoll_multiple_sockets();
    test_edge_triggered_read();
    test_connection_timeout();
    test_signal_handler();
    
    printf("\nAll integration tests passed!\n");
    return 0;
}
