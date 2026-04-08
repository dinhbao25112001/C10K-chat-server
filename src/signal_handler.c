#include "signal_handler.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/* Signal handling thread function */
static void* signal_thread_func(void *arg) {
    signal_handler_t *handler = (signal_handler_t*)arg;
    int sig;
    int result;
    
    while (1) {
        /* Wait for a signal to arrive */
        result = sigwait(&handler->signal_set, &sig);
        if (result != 0) {
            fprintf(stderr, "sigwait failed: %s\n", strerror(result));
            continue;
        }
        
        /* Handle the received signal */
        switch (sig) {
            case SIGINT:
            case SIGTERM:
                fprintf(stderr, "Received signal %d, initiating shutdown\n", sig);
                handler->shutdown_requested = 1;
                return NULL;
            
            case SIGPIPE:
                /* Ignore SIGPIPE - already blocked, but handle if received */
                break;
            
            default:
                fprintf(stderr, "Unexpected signal %d received\n", sig);
                break;
        }
    }
    
    return NULL;
}

int signal_handler_init(signal_handler_t *handler) {
    if (!handler) {
        return ERR_INVALID_PARAM;
    }
    
    /* Initialize shutdown flag */
    handler->shutdown_requested = 0;
    handler->signal_thread = 0;
    
    /* Initialize signal set */
    if (sigemptyset(&handler->signal_set) != 0) {
        fprintf(stderr, "sigemptyset failed: %s\n", strerror(errno));
        return ERR_THREAD_ERROR;
    }
    
    /* Add signals to the set */
    if (sigaddset(&handler->signal_set, SIGINT) != 0) {
        fprintf(stderr, "sigaddset(SIGINT) failed: %s\n", strerror(errno));
        return ERR_THREAD_ERROR;
    }
    
    if (sigaddset(&handler->signal_set, SIGTERM) != 0) {
        fprintf(stderr, "sigaddset(SIGTERM) failed: %s\n", strerror(errno));
        return ERR_THREAD_ERROR;
    }
    
    if (sigaddset(&handler->signal_set, SIGPIPE) != 0) {
        fprintf(stderr, "sigaddset(SIGPIPE) failed: %s\n", strerror(errno));
        return ERR_THREAD_ERROR;
    }
    
    /* Block these signals in all threads */
    if (pthread_sigmask(SIG_BLOCK, &handler->signal_set, NULL) != 0) {
        fprintf(stderr, "pthread_sigmask failed: %s\n", strerror(errno));
        return ERR_THREAD_ERROR;
    }
    
    return ERR_SUCCESS;
}

int signal_handler_start(signal_handler_t *handler) {
    if (!handler) {
        return ERR_INVALID_PARAM;
    }
    
    /* Create dedicated signal handling thread */
    int result = pthread_create(&handler->signal_thread, NULL, 
                                signal_thread_func, handler);
    if (result != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(result));
        return ERR_THREAD_ERROR;
    }
    
    return ERR_SUCCESS;
}

bool signal_handler_should_shutdown(signal_handler_t *handler) {
    if (!handler) {
        return false;
    }
    
    /* Check atomic shutdown flag */
    return handler->shutdown_requested != 0;
}

void signal_handler_stop(signal_handler_t *handler) {
    if (!handler) {
        return;
    }
    
    /* If signal thread was started, wait for it to complete */
    if (handler->signal_thread != 0) {
        /* Set shutdown flag in case thread is still waiting */
        handler->shutdown_requested = 1;
        
        /* Send SIGTERM to wake up the signal thread if it's blocked in sigwait */
        pthread_kill(handler->signal_thread, SIGTERM);
        
        /* Wait for signal thread to exit */
        pthread_join(handler->signal_thread, NULL);
        handler->signal_thread = 0;
    }
}
