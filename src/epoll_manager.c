#include "epoll_manager.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int epoll_manager_init(epoll_manager_t *manager, int max_events) {
    if (!manager || max_events <= 0) {
        return ERR_INVALID_PARAM;
    }

    /* Create epoll file descriptor */
    manager->epoll_fd = epoll_create1(0);
    if (manager->epoll_fd == -1) {
        return ERR_EPOLL_ERROR;
    }

    /* Allocate events array */
    manager->events = (struct epoll_event *)calloc(max_events, sizeof(struct epoll_event));
    if (!manager->events) {
        close(manager->epoll_fd);
        manager->epoll_fd = -1;
        return ERR_MEMORY_ALLOC;
    }

    manager->max_events = max_events;
    return ERR_SUCCESS;
}

int epoll_manager_add(epoll_manager_t *manager, int fd, uint32_t events, void *data) {
    if (!manager || manager->epoll_fd == -1 || fd < 0) {
        return ERR_INVALID_PARAM;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events | EPOLLET;  /* Add EPOLLET flag for edge-triggered mode */
    ev.data.ptr = data;

    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        return ERR_EPOLL_ERROR;
    }

    return ERR_SUCCESS;
}

int epoll_manager_modify(epoll_manager_t *manager, int fd, uint32_t events, void *data) {
    if (!manager || manager->epoll_fd == -1 || fd < 0) {
        return ERR_INVALID_PARAM;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events | EPOLLET;  /* Maintain EPOLLET flag for edge-triggered mode */
    ev.data.ptr = data;

    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        return ERR_EPOLL_ERROR;
    }

    return ERR_SUCCESS;
}

int epoll_manager_remove(epoll_manager_t *manager, int fd) {
    if (!manager || manager->epoll_fd == -1 || fd < 0) {
        return ERR_INVALID_PARAM;
    }

    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        return ERR_EPOLL_ERROR;
    }

    return ERR_SUCCESS;
}

int epoll_manager_wait(epoll_manager_t *manager, int timeout_ms) {
    if (!manager || manager->epoll_fd == -1 || !manager->events) {
        return ERR_INVALID_PARAM;
    }

    int nfds = epoll_wait(manager->epoll_fd, manager->events, manager->max_events, timeout_ms);
    if (nfds == -1) {
        return ERR_EPOLL_ERROR;
    }

    return nfds;
}

struct epoll_event* epoll_manager_get_event(epoll_manager_t *manager, int index) {
    if (!manager || !manager->events || index < 0 || index >= manager->max_events) {
        return NULL;
    }

    return &manager->events[index];
}

void epoll_manager_destroy(epoll_manager_t *manager) {
    if (!manager) {
        return;
    }

    /* Close epoll file descriptor */
    if (manager->epoll_fd != -1) {
        close(manager->epoll_fd);
        manager->epoll_fd = -1;
    }

    /* Free events array */
    if (manager->events) {
        free(manager->events);
        manager->events = NULL;
    }

    manager->max_events = 0;
}
