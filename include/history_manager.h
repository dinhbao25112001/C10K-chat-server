#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include "common.h"
#include "message_router.h"
#include <stdio.h>
#include <pthread.h>

typedef struct history_manager {
    char base_path[PATH_MAX];
    pthread_mutex_t file_lock;
    FILE *current_file;
    char current_date[32];
} history_manager_t;

/* Initialize history manager with base directory */
int history_manager_init(history_manager_t *manager, const char *base_path);

/* Write message to history (async via thread pool) */
int history_manager_write(history_manager_t *manager, const message_t *msg);

/* Flush pending writes to disk */
int history_manager_flush(history_manager_t *manager);

/* Rotate log file if date changed */
int history_manager_rotate(history_manager_t *manager);

/* Destroy and cleanup */
void history_manager_destroy(history_manager_t *manager);

#endif /* HISTORY_MANAGER_H */
