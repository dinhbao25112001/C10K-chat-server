#include "logger.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

/* Log level names */
static const char* log_level_names[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "CRITICAL"
};

/* Initialize logger */
int logger_init(logger_t *logger, const char *log_path, log_level_t min_level, size_t max_size) {
    if (!logger || !log_path) {
        return ERR_INVALID_PARAM;
    }
    
    memset(logger, 0, sizeof(logger_t));
    
    /* Store configuration */
    strncpy(logger->log_path, log_path, sizeof(logger->log_path) - 1);
    logger->min_level = min_level;
    logger->max_size = max_size;
    
    /* Create log directory if it doesn't exist */
    char dir_path[256];
    strncpy(dir_path, log_path, sizeof(dir_path) - 1);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir_path, 0755);
    }
    
    /* Open log file in append mode */
    logger->log_file = fopen(log_path, "a");
    if (!logger->log_file) {
        return ERR_FILE_OPEN_FAILED;
    }
    
    /* Get current file size */
    fseek(logger->log_file, 0, SEEK_END);
    logger->current_size = ftell(logger->log_file);
    
    return ERR_SUCCESS;
}

/* Log a message */
void logger_log(logger_t *logger, log_level_t level, const char *component, const char *format, ...) {
    if (!logger || !logger->log_file || level < logger->min_level) {
        return;
    }
    
    /* Get current timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    /* Format log message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Write log entry */
    int written = fprintf(logger->log_file, "[%s] [%s] [%s] %s\n",
                         timestamp,
                         log_level_names[level],
                         component ? component : "UNKNOWN",
                         message);
    
    if (written > 0) {
        logger->current_size += written;
    }
    
    /* Flush to ensure log is written */
    fflush(logger->log_file);
    
    /* Check if rotation is needed */
    if (logger->max_size > 0 && logger->current_size >= logger->max_size) {
        logger_rotate(logger);
    }
}

/* Rotate log file */
int logger_rotate(logger_t *logger) {
    if (!logger || !logger->log_file) {
        return ERR_INVALID_PARAM;
    }
    
    /* Close current log file */
    fclose(logger->log_file);
    
    /* Rename current log file with timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char rotated_path[512];
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
    snprintf(rotated_path, sizeof(rotated_path), "%s.%s", logger->log_path, timestamp);
    
    rename(logger->log_path, rotated_path);
    
    /* Open new log file */
    logger->log_file = fopen(logger->log_path, "a");
    if (!logger->log_file) {
        return ERR_FILE_OPEN_FAILED;
    }
    
    logger->current_size = 0;
    
    return ERR_SUCCESS;
}

/* Close and cleanup logger */
void logger_destroy(logger_t *logger) {
    if (!logger) {
        return;
    }
    
    if (logger->log_file) {
        fclose(logger->log_file);
        logger->log_file = NULL;
    }
}
