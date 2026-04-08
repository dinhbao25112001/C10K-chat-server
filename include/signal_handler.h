#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "common.h"
#include <signal.h>
#include <pthread.h>

typedef struct signal_handler {
    sigset_t signal_set;
    pthread_t signal_thread;
    volatile sig_atomic_t shutdown_requested;
} signal_handler_t;

/* Initialize signal handler */
int signal_handler_init(signal_handler_t *handler);

/* Start signal handling thread */
int signal_handler_start(signal_handler_t *handler);

/* Check if shutdown was requested */
bool signal_handler_should_shutdown(signal_handler_t *handler);

/* Stop signal handler */
void signal_handler_stop(signal_handler_t *handler);

#endif /* SIGNAL_HANDLER_H */
