#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

/* Log levels */
typedef enum log_level {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
} log_level_t;

/* Logger structure */
typedef struct logger {
    FILE *log_file;
    log_level_t min_level;
    char log_path[256];
    size_t max_size;
    size_t current_size;
} logger_t;

/* Initialize logger */
int logger_init(logger_t *logger, const char *log_path, log_level_t min_level, size_t max_size);

/* Log a message */
void logger_log(logger_t *logger, log_level_t level, const char *component, const char *format, ...);

/* Rotate log file if needed */
int logger_rotate(logger_t *logger);

/* Close and cleanup logger */
void logger_destroy(logger_t *logger);

/* Convenience macros */
#define LOG_DEBUG(logger, component, ...) logger_log(logger, LOG_DEBUG, component, __VA_ARGS__)
#define LOG_INFO(logger, component, ...) logger_log(logger, LOG_INFO, component, __VA_ARGS__)
#define LOG_WARN(logger, component, ...) logger_log(logger, LOG_WARN, component, __VA_ARGS__)
#define LOG_ERROR(logger, component, ...) logger_log(logger, LOG_ERROR, component, __VA_ARGS__)
#define LOG_CRITICAL(logger, component, ...) logger_log(logger, LOG_CRITICAL, component, __VA_ARGS__)

#endif /* LOGGER_H */
