#include "connection_handler.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

int connection_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return ERR_SOCKET_ERROR;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        return ERR_SOCKET_ERROR;
    }
    
    return ERR_SUCCESS;
}

int connection_accept(int listen_fd, connection_t *conn) {
    if (!conn) {
        return ERR_INVALID_PARAM;
    }
    
    socklen_t addr_len = sizeof(conn->addr);
    conn->socket_fd = accept(listen_fd, (struct sockaddr *)&conn->addr, &addr_len);
    
    if (conn->socket_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_SUCCESS;  /* No more connections to accept */
        }
        perror("accept");
        return ERR_SOCKET_ERROR;
    }
    
    /* Set socket to non-blocking mode */
    if (connection_set_nonblocking(conn->socket_fd) != ERR_SUCCESS) {
        close(conn->socket_fd);
        return ERR_SOCKET_ERROR;
    }
    
    /* Initialize connection state */
    conn->read_offset = 0;
    conn->write_offset = 0;
    conn->write_length = 0;
    conn->session = NULL;
    conn->last_activity = time(NULL);
    conn->state = CONN_CONNECTING;
    conn->is_websocket = false;
    
    return ERR_SUCCESS;
}

ssize_t connection_read(connection_t *conn) {
    if (!conn || conn->socket_fd < 0) {
        return ERR_INVALID_PARAM;
    }
    
    ssize_t total_read = 0;
    
    /* Edge-triggered mode: read until EAGAIN */
    while (1) {
        size_t available_space = READ_BUFFER_SIZE - conn->read_offset;
        if (available_space == 0) {
            /* Buffer full, need to process data first */
            break;
        }
        
        ssize_t n = read(conn->socket_fd, 
                        conn->read_buffer + conn->read_offset, 
                        available_space);
        
        if (n > 0) {
            conn->read_offset += n;
            total_read += n;
            conn->last_activity = time(NULL);
        } else if (n == 0) {
            /* Connection closed by peer */
            return 0;
        } else {
            /* Error occurred */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No more data available, normal for edge-triggered */
                break;
            } else if (errno == EINTR) {
                /* Interrupted, retry */
                continue;
            } else {
                /* Real error */
                perror("read");
                return -1;
            }
        }
    }
    
    return total_read;
}

ssize_t connection_write(connection_t *conn) {
    if (!conn || conn->socket_fd < 0) {
        return ERR_INVALID_PARAM;
    }
    
    ssize_t total_written = 0;
    
    /* Edge-triggered mode: write until EAGAIN */
    while (conn->write_offset < conn->write_length) {
        size_t remaining = conn->write_length - conn->write_offset;
        
        ssize_t n = write(conn->socket_fd,
                         conn->write_buffer + conn->write_offset,
                         remaining);
        
        if (n > 0) {
            conn->write_offset += n;
            total_written += n;
            conn->last_activity = time(NULL);
        } else if (n == 0) {
            /* Should not happen with sockets */
            break;
        } else {
            /* Error occurred */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Socket buffer full, normal for edge-triggered */
                break;
            } else if (errno == EINTR) {
                /* Interrupted, retry */
                continue;
            } else if (errno == EPIPE) {
                /* Broken pipe, connection closed */
                return -1;
            } else {
                /* Real error */
                perror("write");
                return -1;
            }
        }
    }
    
    /* If all data written, reset buffer */
    if (conn->write_offset >= conn->write_length) {
        conn->write_offset = 0;
        conn->write_length = 0;
    }
    
    return total_written;
}

int connection_buffer_write(connection_t *conn, const char *data, size_t len) {
    if (!conn || !data) {
        return ERR_INVALID_PARAM;
    }
    
    /* Check if there's enough space in the write buffer */
    size_t available_space = WRITE_BUFFER_SIZE - conn->write_length;
    if (len > available_space) {
        fprintf(stderr, "Write buffer full, cannot buffer %zu bytes\n", len);
        return ERR_SOCKET_ERROR;
    }
    
    /* Append data to write buffer */
    memcpy(conn->write_buffer + conn->write_length, data, len);
    conn->write_length += len;
    
    return ERR_SUCCESS;
}

void connection_close(connection_t *conn) {
    if (!conn) {
        return;
    }
    
    if (conn->socket_fd >= 0) {
        close(conn->socket_fd);
        conn->socket_fd = -1;
    }
    
    /* Clear buffers */
    conn->read_offset = 0;
    conn->write_offset = 0;
    conn->write_length = 0;
    
    /* Clear session pointer (actual session cleanup handled by auth_manager) */
    conn->session = NULL;
    
    conn->state = CONN_CLOSING;
}

bool connection_is_timeout(connection_t *conn, time_t current_time, int timeout_seconds) {
    if (!conn) {
        return false;
    }
    
    time_t elapsed = current_time - conn->last_activity;
    return elapsed >= timeout_seconds;
}
