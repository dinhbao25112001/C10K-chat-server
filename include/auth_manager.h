#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include "common.h"
#include <pthread.h>

typedef struct client_session {
    char username[USERNAME_MAX];
    char session_id[SESSION_ID_LEN];
    char room[ROOMNAME_MAX];
    time_t created_at;
    time_t last_seen;
    bool authenticated;
    int socket_fd;
} client_session_t;

typedef struct auth_manager {
    void *credentials_db;
    void *sessions;
    pthread_rwlock_t lock;
} auth_manager_t;

/* Initialize auth manager */
int auth_manager_init(auth_manager_t *manager, const char *credentials_file);

/* Authenticate user credentials */
int auth_manager_authenticate(auth_manager_t *manager, const char *username, 
                               const char *password, client_session_t **session);

/* Create session for authenticated user */
int auth_manager_create_session(auth_manager_t *manager, const char *username,
                                 client_session_t **session);

/* Associate session with socket fd */
int auth_manager_associate_session(auth_manager_t *manager, client_session_t *session, int socket_fd);

/* Get session by socket fd */
client_session_t* auth_manager_get_session(auth_manager_t *manager, int socket_fd);

/* Invalidate session */
void auth_manager_invalidate_session(auth_manager_t *manager, int socket_fd);

/* Destroy auth manager */
void auth_manager_destroy(auth_manager_t *manager);

#endif /* AUTH_MANAGER_H */
