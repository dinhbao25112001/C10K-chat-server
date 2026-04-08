#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "common.h"
#include "connection_handler.h"
#include <pthread.h>

/* Forward declaration */
typedef struct task_queue task_queue_t;

typedef struct message {
    char sender[USERNAME_MAX];
    char room[ROOMNAME_MAX];
    char content[MESSAGE_MAX];
    time_t timestamp;
} message_t;

typedef struct message_router {
    connection_t **connections;
    int max_connections;
    pthread_rwlock_t lock;
    task_queue_t *task_queue;  /* Task queue for enqueuing send tasks */
} message_router_t;

/* Initialize message router */
int message_router_init(message_router_t *router, int max_connections, task_queue_t *task_queue);

/* Parse raw message data */
int message_router_parse(const char *raw_data, size_t len, message_t *msg);

/* Route message to all clients in a room */
int message_router_route(message_router_t *router, const message_t *msg);

/* Register a connection for message routing */
int message_router_register(message_router_t *router, connection_t *conn);

/* Unregister a connection */
void message_router_unregister(message_router_t *router, int socket_fd);

/* Destroy router */
void message_router_destroy(message_router_t *router);

#endif /* MESSAGE_ROUTER_H */
