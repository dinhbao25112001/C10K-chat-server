#include "../include/connection_handler.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

void test_connection_set_nonblocking() {
    printf("Testing connection_set_nonblocking...\n");
    
    /* Create a socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    
    /* Set non-blocking */
    int result = connection_set_nonblocking(sock);
    assert(result == ERR_SUCCESS);
    
    /* Verify O_NONBLOCK flag is set */
    int flags = fcntl(sock, F_GETFL, 0);
    assert(flags != -1);
    assert(flags & O_NONBLOCK);
    
    close(sock);
    printf("  PASS: Socket set to non-blocking mode\n");
}

void test_connection_buffer_write() {
    printf("Testing connection_buffer_write...\n");
    
    connection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.socket_fd = -1;
    
    /* Test buffering data */
    const char *test_data = "Hello, World!";
    size_t test_len = strlen(test_data);
    
    int result = connection_buffer_write(&conn, test_data, test_len);
    assert(result == ERR_SUCCESS);
    assert(conn.write_length == test_len);
    assert(memcmp(conn.write_buffer, test_data, test_len) == 0);
    
    printf("  PASS: Data buffered correctly\n");
}

void test_connection_is_timeout() {
    printf("Testing connection_is_timeout...\n");
    
    connection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.last_activity = time(NULL) - 100;  /* 100 seconds ago */
    
    /* Should timeout with 50 second threshold */
    bool timed_out = connection_is_timeout(&conn, time(NULL), 50);
    assert(timed_out == true);
    
    /* Should not timeout with 200 second threshold */
    timed_out = connection_is_timeout(&conn, time(NULL), 200);
    assert(timed_out == false);
    
    printf("  PASS: Timeout detection works correctly\n");
}

void test_connection_close() {
    printf("Testing connection_close...\n");
    
    connection_t conn;
    memset(&conn, 0, sizeof(conn));
    
    /* Create a socket */
    conn.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(conn.socket_fd >= 0);
    
    conn.read_offset = 10;
    conn.write_offset = 5;
    conn.write_length = 20;
    
    /* Close connection */
    connection_close(&conn);
    
    /* Verify cleanup */
    assert(conn.socket_fd == -1);
    assert(conn.read_offset == 0);
    assert(conn.write_offset == 0);
    assert(conn.write_length == 0);
    assert(conn.state == CONN_CLOSING);
    
    printf("  PASS: Connection closed and cleaned up\n");
}

int main() {
    printf("Running connection_handler tests...\n\n");
    
    test_connection_set_nonblocking();
    test_connection_buffer_write();
    test_connection_is_timeout();
    test_connection_close();
    
    printf("\nAll tests passed!\n");
    return 0;
}
