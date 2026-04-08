#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include "common.h"
#include <sys/epoll.h>

typedef struct epoll_manager {
    int epoll_fd;
    struct epoll_event *events;
    int max_events;
} epoll_manager_t;

/* Initialize epoll with specified max events */
int epoll_manager_init(epoll_manager_t *manager, int max_events);

/* Add a file descriptor to epoll monitoring */
int epoll_manager_add(epoll_manager_t *manager, int fd, uint32_t events, void *data);

/* Modify events for an existing file descriptor */
int epoll_manager_modify(epoll_manager_t *manager, int fd, uint32_t events, void *data);

/* Remove a file descriptor from epoll */
int epoll_manager_remove(epoll_manager_t *manager, int fd);

/* Wait for events (returns number of events) */
int epoll_manager_wait(epoll_manager_t *manager, int timeout_ms);

/* Get event at index */
struct epoll_event* epoll_manager_get_event(epoll_manager_t *manager, int index);

/* Cleanup epoll resources */
void epoll_manager_destroy(epoll_manager_t *manager);

#endif /* EPOLL_MANAGER_H */
