#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Buffer sizes */
#define READ_BUFFER_SIZE 8192
#define WRITE_BUFFER_SIZE 16384  /* Increased to 16KB for static file serving */
#define MESSAGE_MAX 4096
#define USERNAME_MAX 64
#define ROOMNAME_MAX 64
#define SESSION_ID_LEN 65  /* 64 hex chars + null terminator */
#define PATH_MAX 4096

/* Connection limits */
#define DEFAULT_MAX_CONNECTIONS 10000
#define DEFAULT_THREAD_POOL_SIZE 8
#define DEFAULT_TASK_QUEUE_CAPACITY 1000
#define DEFAULT_CONNECTION_TIMEOUT 300
#define DEFAULT_EPOLL_TIMEOUT 100
#define DEFAULT_PORT 8080

/* Error codes */
#define ERR_SUCCESS 0
#define ERR_INVALID_PARAM -1
#define ERR_MEMORY_ALLOC -2
#define ERR_SOCKET_ERROR -3
#define ERR_EPOLL_ERROR -4
#define ERR_THREAD_ERROR -5
#define ERR_QUEUE_FULL -6
#define ERR_AUTH_FAILED -7
#define ERR_FILE_IO -8
#define ERR_TIMEOUT -9
#define ERR_FILE_NOT_FOUND -10
#define ERR_BUFFER_FULL -11
#define ERR_FILE_OPEN_FAILED -12

#endif /* COMMON_H */
