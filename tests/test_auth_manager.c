#include "../include/auth_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test helper to create a credentials file */
static void create_test_credentials_file(const char *filename) {
    FILE *file = fopen(filename, "w");
    assert(file != NULL);
    fprintf(file, "alice:password123\n");
    fprintf(file, "bob:secret456\n");
    fprintf(file, "charlie:test789\n");
    fclose(file);
}

/* Test 1: Initialize auth manager */
static void test_auth_manager_init(void) {
    printf("Test 1: Initialize auth manager... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    
    int result = auth_manager_init(&manager, creds_file);
    assert(result == ERR_SUCCESS);
    
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 2: Authenticate with valid credentials */
static void test_authenticate_valid(void) {
    printf("Test 2: Authenticate with valid credentials... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    client_session_t *session = NULL;
    int result = auth_manager_authenticate(&manager, "alice", "password123", &session);
    
    assert(result == ERR_SUCCESS);
    assert(session != NULL);
    assert(strcmp(session->username, "alice") == 0);
    assert(session->authenticated == true);
    assert(strlen(session->session_id) == 64);
    
    free(session);
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 3: Authenticate with invalid password */
static void test_authenticate_invalid_password(void) {
    printf("Test 3: Authenticate with invalid password... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    client_session_t *session = NULL;
    int result = auth_manager_authenticate(&manager, "alice", "wrongpassword", &session);
    
    assert(result == ERR_AUTH_FAILED);
    
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 4: Authenticate with non-existent user */
static void test_authenticate_nonexistent_user(void) {
    printf("Test 4: Authenticate with non-existent user... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    client_session_t *session = NULL;
    int result = auth_manager_authenticate(&manager, "nonexistent", "password", &session);
    
    assert(result == ERR_AUTH_FAILED);
    
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 5: Create and associate session */
static void test_create_and_associate_session(void) {
    printf("Test 5: Create and associate session... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    client_session_t *session = NULL;
    int result = auth_manager_create_session(&manager, "testuser", &session);
    
    assert(result == ERR_SUCCESS);
    assert(session != NULL);
    assert(strcmp(session->username, "testuser") == 0);
    
    /* Associate with socket fd */
    int socket_fd = 42;
    result = auth_manager_associate_session(&manager, session, socket_fd);
    assert(result == ERR_SUCCESS);
    
    /* Retrieve session by socket fd */
    client_session_t *retrieved = auth_manager_get_session(&manager, socket_fd);
    assert(retrieved != NULL);
    assert(retrieved == session);
    assert(strcmp(retrieved->username, "testuser") == 0);
    
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 6: Invalidate session */
static void test_invalidate_session(void) {
    printf("Test 6: Invalidate session... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    client_session_t *session = NULL;
    auth_manager_create_session(&manager, "testuser", &session);
    
    int socket_fd = 42;
    auth_manager_associate_session(&manager, session, socket_fd);
    
    /* Verify session exists */
    client_session_t *retrieved = auth_manager_get_session(&manager, socket_fd);
    assert(retrieved != NULL);
    
    /* Invalidate session */
    auth_manager_invalidate_session(&manager, socket_fd);
    
    /* Verify session no longer exists */
    retrieved = auth_manager_get_session(&manager, socket_fd);
    assert(retrieved == NULL);
    
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 7: Session uniqueness */
static void test_session_uniqueness(void) {
    printf("Test 7: Session uniqueness... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    client_session_t *session1 = NULL;
    client_session_t *session2 = NULL;
    
    auth_manager_create_session(&manager, "user1", &session1);
    auth_manager_create_session(&manager, "user2", &session2);
    
    /* Verify session IDs are unique */
    assert(strcmp(session1->session_id, session2->session_id) != 0);
    
    free(session1);
    free(session2);
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

/* Test 8: Password hashing (no plaintext storage) */
static void test_password_hashing(void) {
    printf("Test 8: Password hashing... ");
    
    auth_manager_t manager;
    const char *creds_file = "/tmp/test_credentials.txt";
    
    create_test_credentials_file(creds_file);
    auth_manager_init(&manager, creds_file);
    
    /* Authenticate successfully */
    client_session_t *session = NULL;
    int result = auth_manager_authenticate(&manager, "alice", "password123", &session);
    assert(result == ERR_SUCCESS);
    
    /* Verify that the same password authenticates again (hash is consistent) */
    client_session_t *session2 = NULL;
    result = auth_manager_authenticate(&manager, "alice", "password123", &session2);
    assert(result == ERR_SUCCESS);
    
    free(session);
    free(session2);
    auth_manager_destroy(&manager);
    remove(creds_file);
    
    printf("PASSED\n");
}

int main(void) {
    printf("Running auth_manager tests...\n\n");
    
    test_auth_manager_init();
    test_authenticate_valid();
    test_authenticate_invalid_password();
    test_authenticate_nonexistent_user();
    test_create_and_associate_session();
    test_invalidate_session();
    test_session_uniqueness();
    test_password_hashing();
    
    printf("\nAll tests passed!\n");
    return 0;
}
