#include "../include/history_manager.h"
#include "../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* Test helper to check if file exists */
static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Test helper to read file content */
static char* read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    return content;
}

/* Test initialization */
void test_init() {
    printf("Test: history_manager_init\n");
    
    history_manager_t manager;
    int ret = history_manager_init(&manager, "/tmp/test_history");
    
    assert(ret == ERR_SUCCESS);
    assert(strcmp(manager.base_path, "/tmp/test_history") == 0);
    assert(file_exists("/tmp/test_history"));
    
    history_manager_destroy(&manager);
    printf("  PASSED\n");
}

/* Test writing messages */
void test_write() {
    printf("Test: history_manager_write\n");
    
    history_manager_t manager;
    history_manager_init(&manager, "/tmp/test_history");
    
    /* Create a test message */
    message_t msg;
    strncpy(msg.sender, "alice", USERNAME_MAX);
    strncpy(msg.room, "general", ROOMNAME_MAX);
    strncpy(msg.content, "Hello, world!", MESSAGE_MAX);
    msg.timestamp = 1234567890;
    
    /* Write message */
    int ret = history_manager_write(&manager, &msg);
    assert(ret == ERR_SUCCESS);
    
    /* Check that the log file was created */
    char date_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_info);
    
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "/tmp/test_history/%s/general.log", date_str);
    
    assert(file_exists(log_path));
    
    /* Read and verify content */
    char *content = read_file(log_path);
    assert(content != NULL);
    assert(strstr(content, "alice") != NULL);
    assert(strstr(content, "general") != NULL);
    assert(strstr(content, "Hello, world!") != NULL);
    assert(strstr(content, "1234567890") != NULL);
    
    free(content);
    history_manager_destroy(&manager);
    printf("  PASSED\n");
}

/* Test multiple messages to different rooms */
void test_multiple_rooms() {
    printf("Test: history_manager_write (multiple rooms)\n");
    
    history_manager_t manager;
    history_manager_init(&manager, "/tmp/test_history");
    
    /* Write to general room */
    message_t msg1;
    strncpy(msg1.sender, "alice", USERNAME_MAX);
    strncpy(msg1.room, "general", ROOMNAME_MAX);
    strncpy(msg1.content, "Message 1", MESSAGE_MAX);
    msg1.timestamp = time(NULL);
    history_manager_write(&manager, &msg1);
    
    /* Write to tech room */
    message_t msg2;
    strncpy(msg2.sender, "bob", USERNAME_MAX);
    strncpy(msg2.room, "tech", ROOMNAME_MAX);
    strncpy(msg2.content, "Message 2", MESSAGE_MAX);
    msg2.timestamp = time(NULL);
    history_manager_write(&manager, &msg2);
    
    /* Check both log files exist */
    char date_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_info);
    
    char log_path1[PATH_MAX];
    snprintf(log_path1, sizeof(log_path1), "/tmp/test_history/%s/general.log", date_str);
    assert(file_exists(log_path1));
    
    char log_path2[PATH_MAX];
    snprintf(log_path2, sizeof(log_path2), "/tmp/test_history/%s/tech.log", date_str);
    assert(file_exists(log_path2));
    
    history_manager_destroy(&manager);
    printf("  PASSED\n");
}

/* Test flush */
void test_flush() {
    printf("Test: history_manager_flush\n");
    
    history_manager_t manager;
    history_manager_init(&manager, "/tmp/test_history");
    
    /* Write a message */
    message_t msg;
    strncpy(msg.sender, "alice", USERNAME_MAX);
    strncpy(msg.room, "general", ROOMNAME_MAX);
    strncpy(msg.content, "Test flush", MESSAGE_MAX);
    msg.timestamp = time(NULL);
    history_manager_write(&manager, &msg);
    
    /* Flush */
    int ret = history_manager_flush(&manager);
    assert(ret == ERR_SUCCESS);
    
    history_manager_destroy(&manager);
    printf("  PASSED\n");
}

/* Test rotate */
void test_rotate() {
    printf("Test: history_manager_rotate\n");
    
    history_manager_t manager;
    history_manager_init(&manager, "/tmp/test_history");
    
    /* Set a fake current date */
    strncpy(manager.current_date, "2024-01-01", sizeof(manager.current_date));
    
    /* Rotate should update to current date */
    int ret = history_manager_rotate(&manager);
    assert(ret == ERR_SUCCESS);
    
    /* Current date should be updated */
    char date_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_info);
    assert(strcmp(manager.current_date, date_str) == 0);
    
    history_manager_destroy(&manager);
    printf("  PASSED\n");
}

int main() {
    printf("Running history_manager tests...\n\n");
    
    /* Clean up test directory */
    system("rm -rf /tmp/test_history");
    
    test_init();
    test_write();
    test_multiple_rooms();
    test_flush();
    test_rotate();
    
    printf("\nAll tests passed!\n");
    return 0;
}
