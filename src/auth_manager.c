#include "auth_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

/* Hash table entry for credentials */
typedef struct credential_entry {
    char username[USERNAME_MAX];
    char password_hash[65]; /* SHA-256 hex string + null terminator */
    struct credential_entry *next;
} credential_entry_t;

/* Hash table entry for sessions */
typedef struct session_entry {
    int socket_fd;
    client_session_t *session;
    struct session_entry *next;
} session_entry_t;

/* Simple hash table implementation */
#define HASH_TABLE_SIZE 1024

typedef struct hash_table {
    void **buckets;
    size_t size;
} hash_table_t;

/* Hash function for strings */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

/* Hash function for integers */
static unsigned int hash_int(int value) {
    return ((unsigned int)value) % HASH_TABLE_SIZE;
}

/* Create hash table */
static hash_table_t* hash_table_create(void) {
    hash_table_t *table = malloc(sizeof(hash_table_t));
    if (!table) return NULL;
    
    table->size = HASH_TABLE_SIZE;
    table->buckets = calloc(HASH_TABLE_SIZE, sizeof(void*));
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    
    return table;
}

/* Hash password using SHA-256 */
static void hash_password(const char *password, char *output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password, strlen(password), hash);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}

/* Generate random session ID */
static void generate_session_id(char *session_id) {
    unsigned char random_bytes[32];
    RAND_bytes(random_bytes, 32);
    
    for (int i = 0; i < 32; i++) {
        sprintf(session_id + (i * 2), "%02x", random_bytes[i]);
    }
    session_id[64] = '\0';
}

/* Add credential to hash table */
static int add_credential(hash_table_t *table, const char *username, const char *password_hash) {
    unsigned int index = hash_string(username);
    
    credential_entry_t *entry = malloc(sizeof(credential_entry_t));
    if (!entry) return ERR_MEMORY_ALLOC;
    
    strncpy(entry->username, username, USERNAME_MAX - 1);
    entry->username[USERNAME_MAX - 1] = '\0';
    strncpy(entry->password_hash, password_hash, 64);
    entry->password_hash[64] = '\0';
    
    entry->next = (credential_entry_t*)table->buckets[index];
    table->buckets[index] = entry;
    
    return ERR_SUCCESS;
}

/* Find credential in hash table */
static credential_entry_t* find_credential(hash_table_t *table, const char *username) {
    unsigned int index = hash_string(username);
    credential_entry_t *entry = (credential_entry_t*)table->buckets[index];
    
    while (entry) {
        if (strcmp(entry->username, username) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* Add session to hash table */
static int add_session(hash_table_t *table, int socket_fd, client_session_t *session) {
    unsigned int index = hash_int(socket_fd);
    
    session_entry_t *entry = malloc(sizeof(session_entry_t));
    if (!entry) return ERR_MEMORY_ALLOC;
    
    entry->socket_fd = socket_fd;
    entry->session = session;
    
    entry->next = (session_entry_t*)table->buckets[index];
    table->buckets[index] = entry;
    
    return ERR_SUCCESS;
}

/* Find session in hash table */
static client_session_t* find_session(hash_table_t *table, int socket_fd) {
    unsigned int index = hash_int(socket_fd);
    session_entry_t *entry = (session_entry_t*)table->buckets[index];
    
    while (entry) {
        if (entry->socket_fd == socket_fd) {
            return entry->session;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/* Remove session from hash table */
static void remove_session(hash_table_t *table, int socket_fd) {
    unsigned int index = hash_int(socket_fd);
    session_entry_t *entry = (session_entry_t*)table->buckets[index];
    session_entry_t *prev = NULL;
    
    while (entry) {
        if (entry->socket_fd == socket_fd) {
            if (prev) {
                prev->next = entry->next;
            } else {
                table->buckets[index] = entry->next;
            }
            
            if (entry->session) {
                free(entry->session);
            }
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

/* Destroy hash table */
static void hash_table_destroy(hash_table_t *table, bool free_sessions) {
    if (!table) return;
    
    for (size_t i = 0; i < table->size; i++) {
        if (free_sessions) {
            session_entry_t *entry = (session_entry_t*)table->buckets[i];
            while (entry) {
                session_entry_t *next = entry->next;
                if (entry->session) {
                    free(entry->session);
                }
                free(entry);
                entry = next;
            }
        } else {
            credential_entry_t *entry = (credential_entry_t*)table->buckets[i];
            while (entry) {
                credential_entry_t *next = entry->next;
                free(entry);
                entry = next;
            }
        }
    }
    
    free(table->buckets);
    free(table);
}

int auth_manager_init(auth_manager_t *manager, const char *credentials_file) {
    if (!manager || !credentials_file) {
        return ERR_INVALID_PARAM;
    }
    
    /* Initialize read-write lock */
    if (pthread_rwlock_init(&manager->lock, NULL) != 0) {
        return ERR_THREAD_ERROR;
    }
    
    /* Create credentials hash table */
    manager->credentials_db = hash_table_create();
    if (!manager->credentials_db) {
        pthread_rwlock_destroy(&manager->lock);
        return ERR_MEMORY_ALLOC;
    }
    
    /* Create sessions hash table */
    manager->sessions = hash_table_create();
    if (!manager->sessions) {
        hash_table_destroy((hash_table_t*)manager->credentials_db, false);
        pthread_rwlock_destroy(&manager->lock);
        return ERR_MEMORY_ALLOC;
    }
    
    /* Load credentials from file */
    FILE *file = fopen(credentials_file, "r");
    if (!file) {
        /* File doesn't exist, that's okay - empty credentials */
        return ERR_SUCCESS;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        /* Remove newline */
        line[strcspn(line, "\n")] = '\0';
        
        /* Parse username:password format */
        char *colon = strchr(line, ':');
        if (!colon) continue;
        
        *colon = '\0';
        char *username = line;
        char *password = colon + 1;
        
        /* Hash the password */
        char password_hash[65];
        hash_password(password, password_hash);
        
        /* Add to credentials table */
        add_credential((hash_table_t*)manager->credentials_db, username, password_hash);
    }
    
    fclose(file);
    return ERR_SUCCESS;
}

int auth_manager_authenticate(auth_manager_t *manager, const char *username, 
                               const char *password, client_session_t **session) {
    if (!manager || !username || !password || !session) {
        return ERR_INVALID_PARAM;
    }
    
    pthread_rwlock_rdlock(&manager->lock);
    
    /* Find credential */
    credential_entry_t *cred = find_credential((hash_table_t*)manager->credentials_db, username);
    if (!cred) {
        pthread_rwlock_unlock(&manager->lock);
        return ERR_AUTH_FAILED;
    }
    
    /* Hash provided password and compare */
    char password_hash[65];
    hash_password(password, password_hash);
    
    if (strcmp(cred->password_hash, password_hash) != 0) {
        pthread_rwlock_unlock(&manager->lock);
        return ERR_AUTH_FAILED;
    }
    
    pthread_rwlock_unlock(&manager->lock);
    
    /* Create session for authenticated user */
    return auth_manager_create_session(manager, username, session);
}

int auth_manager_create_session(auth_manager_t *manager, const char *username,
                                 client_session_t **session) {
    if (!manager || !username || !session) {
        return ERR_INVALID_PARAM;
    }
    
    /* Allocate session */
    client_session_t *new_session = malloc(sizeof(client_session_t));
    if (!new_session) {
        return ERR_MEMORY_ALLOC;
    }
    
    /* Initialize session */
    strncpy(new_session->username, username, USERNAME_MAX - 1);
    new_session->username[USERNAME_MAX - 1] = '\0';
    
    generate_session_id(new_session->session_id);
    
    new_session->created_at = time(NULL);
    new_session->last_seen = new_session->created_at;
    new_session->authenticated = true;
    new_session->socket_fd = -1; /* Will be set later */
    strncpy(new_session->room, "general", ROOMNAME_MAX - 1);
    new_session->room[ROOMNAME_MAX - 1] = '\0';
    
    *session = new_session;
    return ERR_SUCCESS;
}

int auth_manager_associate_session(auth_manager_t *manager, client_session_t *session, int socket_fd) {
    if (!manager || !session || socket_fd < 0) {
        return ERR_INVALID_PARAM;
    }
    
    pthread_rwlock_wrlock(&manager->lock);
    
    /* Update session socket_fd */
    session->socket_fd = socket_fd;
    
    /* Add to sessions hash table */
    int result = add_session((hash_table_t*)manager->sessions, socket_fd, session);
    
    pthread_rwlock_unlock(&manager->lock);
    
    return result;
}

client_session_t* auth_manager_get_session(auth_manager_t *manager, int socket_fd) {
    if (!manager || socket_fd < 0) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&manager->lock);
    client_session_t *session = find_session((hash_table_t*)manager->sessions, socket_fd);
    pthread_rwlock_unlock(&manager->lock);
    
    return session;
}

void auth_manager_invalidate_session(auth_manager_t *manager, int socket_fd) {
    if (!manager || socket_fd < 0) {
        return;
    }
    
    pthread_rwlock_wrlock(&manager->lock);
    remove_session((hash_table_t*)manager->sessions, socket_fd);
    pthread_rwlock_unlock(&manager->lock);
}

void auth_manager_destroy(auth_manager_t *manager) {
    if (!manager) {
        return;
    }
    
    pthread_rwlock_wrlock(&manager->lock);
    
    /* Destroy hash tables */
    if (manager->credentials_db) {
        hash_table_destroy((hash_table_t*)manager->credentials_db, false);
        manager->credentials_db = NULL;
    }
    
    if (manager->sessions) {
        hash_table_destroy((hash_table_t*)manager->sessions, true);
        manager->sessions = NULL;
    }
    
    pthread_rwlock_unlock(&manager->lock);
    pthread_rwlock_destroy(&manager->lock);
}
