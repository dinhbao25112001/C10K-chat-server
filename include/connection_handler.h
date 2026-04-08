#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "common.h"
#include "auth_manager.h"
#include <sys/socket.h>
#include <netinet/in.h>

typedef enum conn_state {
    CONN_CONNECTING,
    CONN_AUTHENTICATING,
    CONN_AUTHENTICATED,
    CONN_CLOSING
} conn_state_t;

typedef struct connection {
    int socket_fd;
    struct sockaddr_in addr;
    char read_buffer[READ_BUFFER_SIZE];
    size_t read_offset;
    char write_buffer[WRITE_BUFFER_SIZE];
    size_t write_offset;
    size_t write_length;
    client_session_t *session;
    time_t last_activity;
    conn_state_t state;
    bool is_websocket;
} connection_t;

/* Accept new connection and set non-blocking */
int connection_accept(int listen_fd, connection_t *conn);

/* Set socket to non-blocking mode */
int connection_set_nonblocking(int fd);

/* Read available data from socket (edge-triggered) */
ssize_t connection_read(connection_t *conn);

/* Write buffered data to socket */
ssize_t connection_write(connection_t *conn);

/* Buffer data for writing */
int connection_buffer_write(connection_t *conn, const char *data, size_t len);

/* Close connection and cleanup */
void connection_close(connection_t *conn);

/* Check if connection has timed out */
bool connection_is_timeout(connection_t *conn, time_t current_time, int timeout_seconds);

#endif /* CONNECTION_HANDLER_H */
