#include "message_router.h"
#include "task_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int message_router_init(message_router_t *router, int max_connections, task_queue_t *task_queue) {
    if (!router || max_connections <= 0) {
        return ERR_INVALID_PARAM;
    }

    /* Allocate connections array */
    router->connections = calloc(max_connections, sizeof(connection_t *));
    if (!router->connections) {
        return ERR_MEMORY_ALLOC;
    }

    router->max_connections = max_connections;
    router->task_queue = task_queue;

    /* Initialize read-write lock */
    if (pthread_rwlock_init(&router->lock, NULL) != 0) {
        free(router->connections);
        router->connections = NULL;
        return ERR_THREAD_ERROR;
    }

    return ERR_SUCCESS;
}

int message_router_parse(const char *raw_data, size_t len, message_t *msg) {
    if (!raw_data || !msg || len == 0) {
        return ERR_INVALID_PARAM;
    }

    /* Initialize message fields */
    memset(msg, 0, sizeof(message_t));

    /* Simple JSON parser for message format:
     * {"type":"message","sender":"username","room":"roomname","content":"text","timestamp":1234567890}
     */
    
    const char *sender_key = "\"sender\":\"";
    const char *room_key = "\"room\":\"";
    const char *content_key = "\"content\":\"";
    const char *timestamp_key = "\"timestamp\":";

    /* Parse sender */
    const char *sender_start = strstr(raw_data, sender_key);
    if (sender_start) {
        sender_start += strlen(sender_key);
        const char *sender_end = strchr(sender_start, '"');
        if (sender_end) {
            size_t sender_len = sender_end - sender_start;
            if (sender_len >= USERNAME_MAX) {
                sender_len = USERNAME_MAX - 1;
            }
            strncpy(msg->sender, sender_start, sender_len);
            msg->sender[sender_len] = '\0';
        }
    }

    /* Parse room */
    const char *room_start = strstr(raw_data, room_key);
    if (room_start) {
        room_start += strlen(room_key);
        const char *room_end = strchr(room_start, '"');
        if (room_end) {
            size_t room_len = room_end - room_start;
            if (room_len >= ROOMNAME_MAX) {
                room_len = ROOMNAME_MAX - 1;
            }
            strncpy(msg->room, room_start, room_len);
            msg->room[room_len] = '\0';
        }
    }

    /* Parse content */
    const char *content_start = strstr(raw_data, content_key);
    if (content_start) {
        content_start += strlen(content_key);
        const char *content_end = strchr(content_start, '"');
        if (content_end) {
            size_t content_len = content_end - content_start;
            if (content_len >= MESSAGE_MAX) {
                content_len = MESSAGE_MAX - 1;
            }
            strncpy(msg->content, content_start, content_len);
            msg->content[content_len] = '\0';
        }
    }

    /* Parse timestamp */
    const char *timestamp_start = strstr(raw_data, timestamp_key);
    if (timestamp_start) {
        timestamp_start += strlen(timestamp_key);
        msg->timestamp = (time_t)atoll(timestamp_start);
    } else {
        /* Use current time if not provided */
        msg->timestamp = time(NULL);
    }

    /* Validate required fields */
    if (msg->sender[0] == '\0' || msg->room[0] == '\0' || msg->content[0] == '\0') {
        return ERR_INVALID_PARAM;
    }

    return ERR_SUCCESS;
}

int message_router_route(message_router_t *router, const message_t *msg) {
    if (!router || !msg) {
        return ERR_INVALID_PARAM;
    }

    /* Acquire read lock for routing */
    if (pthread_rwlock_rdlock(&router->lock) != 0) {
        return ERR_THREAD_ERROR;
    }

    int recipients_count = 0;

    /* Identify all recipients in the same room */
    for (int i = 0; i < router->max_connections; i++) {
        connection_t *conn = router->connections[i];
        
        if (!conn) {
            continue;
        }

        /* Check if connection is authenticated and in the same room */
        if (conn->state == CONN_AUTHENTICATED && 
            conn->session && 
            strcmp(conn->session->room, msg->room) == 0) {
            
            /* Create a copy of the message for this recipient */
            message_t *msg_copy = malloc(sizeof(message_t));
            if (!msg_copy) {
                continue; /* Skip this recipient on allocation failure */
            }
            memcpy(msg_copy, msg, sizeof(message_t));

            /* Create task for sending message */
            task_t *send_task = malloc(sizeof(task_t));
            if (!send_task) {
                free(msg_copy);
                continue; /* Skip this recipient on allocation failure */
            }

            send_task->type = TASK_MESSAGE_SEND;
            send_task->data = msg_copy;
            send_task->handler = NULL; /* Handler will be set by thread pool */
            send_task->next = NULL;

            /* Enqueue send task (if task queue is available) */
            if (router->task_queue) {
                if (task_queue_enqueue(router->task_queue, send_task) == ERR_SUCCESS) {
                    recipients_count++;
                } else {
                    /* Queue full or error - cleanup */
                    free(send_task);
                    free(msg_copy);
                }
            } else {
                /* No task queue available - cleanup */
                free(send_task);
                free(msg_copy);
            }
        }
    }

    pthread_rwlock_unlock(&router->lock);

    return recipients_count > 0 ? ERR_SUCCESS : ERR_INVALID_PARAM;
}

int message_router_register(message_router_t *router, connection_t *conn) {
    if (!router || !conn) {
        return ERR_INVALID_PARAM;
    }

    /* Acquire write lock for registration */
    if (pthread_rwlock_wrlock(&router->lock) != 0) {
        return ERR_THREAD_ERROR;
    }

    int result = ERR_INVALID_PARAM;

    /* Find an empty slot and register the connection */
    for (int i = 0; i < router->max_connections; i++) {
        if (router->connections[i] == NULL) {
            router->connections[i] = conn;
            result = ERR_SUCCESS;
            break;
        }
    }

    pthread_rwlock_unlock(&router->lock);

    return result;
}

void message_router_unregister(message_router_t *router, int socket_fd) {
    if (!router) {
        return;
    }

    /* Acquire write lock for unregistration */
    if (pthread_rwlock_wrlock(&router->lock) != 0) {
        return;
    }

    /* Find and remove the connection */
    for (int i = 0; i < router->max_connections; i++) {
        if (router->connections[i] && router->connections[i]->socket_fd == socket_fd) {
            router->connections[i] = NULL;
            break;
        }
    }

    pthread_rwlock_unlock(&router->lock);
}

void message_router_destroy(message_router_t *router) {
    if (!router) {
        return;
    }

    /* Destroy read-write lock */
    pthread_rwlock_destroy(&router->lock);

    /* Free connections array */
    if (router->connections) {
        free(router->connections);
        router->connections = NULL;
    }

    router->max_connections = 0;
}
