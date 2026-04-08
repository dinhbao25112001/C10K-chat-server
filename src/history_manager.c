#include "history_manager.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

/* Helper function to create directory recursively */
static int create_directory(const char *path) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return ERR_FILE_IO;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return ERR_FILE_IO;
    }
    return ERR_SUCCESS;
}

/* Helper function to get current date string */
static void get_current_date(char *date_str, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(date_str, size, "%Y-%m-%d", tm_info);
}

/* Helper function to build log file path */
static int build_log_path(const char *base_path, const char *date, 
                          const char *room, char *path, size_t path_size) {
    int ret = snprintf(path, path_size, "%s/%s/%s.log", base_path, date, room);
    if (ret < 0 || (size_t)ret >= path_size) {
        return ERR_INVALID_PARAM;
    }
    return ERR_SUCCESS;
}

int history_manager_init(history_manager_t *manager, const char *base_path) {
    if (!manager || !base_path) {
        return ERR_INVALID_PARAM;
    }

    /* Initialize structure */
    strncpy(manager->base_path, base_path, PATH_MAX - 1);
    manager->base_path[PATH_MAX - 1] = '\0';
    manager->current_file = NULL;
    manager->current_date[0] = '\0';

    /* Initialize mutex */
    if (pthread_mutex_init(&manager->file_lock, NULL) != 0) {
        return ERR_THREAD_ERROR;
    }

    /* Create base directory */
    int ret = create_directory(base_path);
    if (ret != ERR_SUCCESS) {
        pthread_mutex_destroy(&manager->file_lock);
        return ret;
    }

    return ERR_SUCCESS;
}

int history_manager_write(history_manager_t *manager, const message_t *msg) {
    if (!manager || !msg) {
        return ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&manager->file_lock);

    /* Check if we need to rotate (date changed) */
    char current_date[32];
    get_current_date(current_date, sizeof(current_date));
    
    if (strcmp(manager->current_date, current_date) != 0 || !manager->current_file) {
        /* Close current file if open */
        if (manager->current_file) {
            fclose(manager->current_file);
            manager->current_file = NULL;
        }
        
        /* Update current date */
        strncpy(manager->current_date, current_date, sizeof(manager->current_date) - 1);
        manager->current_date[sizeof(manager->current_date) - 1] = '\0';
    }

    /* Build path for date/room directory */
    char date_dir[PATH_MAX];
    int ret = snprintf(date_dir, sizeof(date_dir), "%s/%s", manager->base_path, current_date);
    if (ret < 0 || (size_t)ret >= sizeof(date_dir)) {
        pthread_mutex_unlock(&manager->file_lock);
        return ERR_INVALID_PARAM;
    }
    
    /* Create date directory if it doesn't exist */
    if (create_directory(date_dir) != ERR_SUCCESS) {
        pthread_mutex_unlock(&manager->file_lock);
        return ERR_FILE_IO;
    }

    /* Build full log file path */
    char log_path[PATH_MAX];
    if (build_log_path(manager->base_path, current_date, msg->room, 
                       log_path, sizeof(log_path)) != ERR_SUCCESS) {
        pthread_mutex_unlock(&manager->file_lock);
        return ERR_INVALID_PARAM;
    }

    /* Open log file in append mode */
    FILE *log_file = fopen(log_path, "a");
    if (!log_file) {
        pthread_mutex_unlock(&manager->file_lock);
        return ERR_FILE_IO;
    }

    /* Write message as newline-delimited JSON */
    fprintf(log_file, "{\"timestamp\":%ld,\"sender\":\"%s\",\"room\":\"%s\",\"content\":\"%s\"}\n",
            (long)msg->timestamp, msg->sender, msg->room, msg->content);

    /* Close the file (we open/close per write for simplicity) */
    fclose(log_file);

    pthread_mutex_unlock(&manager->file_lock);
    return ERR_SUCCESS;
}

int history_manager_flush(history_manager_t *manager) {
    if (!manager) {
        return ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&manager->file_lock);

    if (manager->current_file) {
        /* Flush stdio buffer */
        if (fflush(manager->current_file) != 0) {
            pthread_mutex_unlock(&manager->file_lock);
            return ERR_FILE_IO;
        }

        /* Sync to disk */
        int fd = fileno(manager->current_file);
        if (fd != -1 && fsync(fd) != 0) {
            pthread_mutex_unlock(&manager->file_lock);
            return ERR_FILE_IO;
        }
    }

    pthread_mutex_unlock(&manager->file_lock);
    return ERR_SUCCESS;
}

int history_manager_rotate(history_manager_t *manager) {
    if (!manager) {
        return ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&manager->file_lock);

    /* Get current date */
    char current_date[32];
    get_current_date(current_date, sizeof(current_date));

    /* Check if date has changed */
    if (strcmp(manager->current_date, current_date) != 0) {
        /* Close current file */
        if (manager->current_file) {
            fclose(manager->current_file);
            manager->current_file = NULL;
        }

        /* Update current date */
        strncpy(manager->current_date, current_date, sizeof(manager->current_date) - 1);
        manager->current_date[sizeof(manager->current_date) - 1] = '\0';
    }

    pthread_mutex_unlock(&manager->file_lock);
    return ERR_SUCCESS;
}

void history_manager_destroy(history_manager_t *manager) {
    if (!manager) {
        return;
    }

    pthread_mutex_lock(&manager->file_lock);

    /* Close current file if open */
    if (manager->current_file) {
        fflush(manager->current_file);
        fsync(fileno(manager->current_file));
        fclose(manager->current_file);
        manager->current_file = NULL;
    }

    pthread_mutex_unlock(&manager->file_lock);

    /* Destroy mutex */
    pthread_mutex_destroy(&manager->file_lock);
}
