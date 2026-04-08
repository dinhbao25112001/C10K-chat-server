#include "../include/message_router.h"
#include "../include/task_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test 1: Initialize message router */
static void test_message_router_init(void) {
    printf("Test 1: Initialize message router... ");
    
    message_router_t router;
    task_queue_t queue;
    
    task_queue_init(&queue, 100);
    
    int result = message_router_init(&router, 100, &queue);
    assert(result == ERR_SUCCESS);
    assert(router.connections != NULL);
    assert(router.max_connections == 100);
    assert(router.task_queue == &queue);
    
    message_router_destroy(&router);
    task_queue_destroy(&queue);
    
    printf("PASSED\n");
}

/* Test 2: Parse valid JSON message */
static void test_message_router_parse_valid(void) {
    printf("Test 2: Parse valid JSON message... ");
    
    const char *json = "{\"type\":\"message\",\"sender\":\"alice\",\"room\":\"general\",\"content\":\"Hello, world!\",\"timestamp\":1234567890}";
    message_t msg;
    
    int result = message_router_parse(json, strlen(json), &msg);
    
    assert(result == ERR_SUCCESS);
    assert(strcmp(msg.sender, "alice") == 0);
    assert(strcmp(msg.room, "general") == 0);
    assert(strcmp(msg.content, "Hello, world!") == 0);
    assert(msg.timestamp == 1234567890);
    
    printf("PASSED\n");
}

/* Test 3: Parse message without timestamp */
static void test_message_router_parse_no_timestamp(void) {
    printf("Test 3: Parse message without timestamp... ");
    
    const char *json = "{\"type\":\"message\",\"sender\":\"bob\",\"room\":\"tech\",\"content\":\"Test message\"}";
    message_t msg;
    
    int result = message_router_parse(json, strlen(json), &msg);
    
    assert(result == ERR_SUCCESS);
    assert(strcmp(msg.sender, "bob") == 0);
    assert(strcmp(msg.room, "tech") == 0);
    assert(strcmp(msg.content, "Test message") == 0);
    assert(msg.timestamp > 0);  /* Should use current time */
    
    printf("PASSED\n");
}

/* Test 4: Parse invalid message (missing required fields) */
static void test_message_router_parse_invalid(void) {
    printf("Test 4: Parse invalid message... ");
    
    const char *json = "{\"type\":\"message\",\"sender\":\"alice\"}";
    message_t msg;
    
    int result = message_router_parse(json, strlen(json), &msg);
    
    assert(result == ERR_INVALID_PARAM);
    
    printf("PASSED\n");
}

/* Test 5: Register and unregister connection */
static void test_message_router_register_unregister(void) {
    printf("Test 5: Register and unregister connection... ");
    
    message_router_t router;
    task_queue_t queue;
    connection_t conn;
    
    task_queue_init(&queue, 100);
    message_router_init(&router, 100, &queue);
    
    /* Initialize connection */
    memset(&conn, 0, sizeof(connection_t));
    conn.socket_fd = 42;
    conn.state = CONN_AUTHENTICATED;
    
    /* Register connection */
    int result = message_router_register(&router, &conn);
    assert(result == ERR_SUCCESS);
    
    /* Verify connection is registered */
    assert(router.connections[0] == &conn);
    
    /* Unregister connection */
    message_router_unregister(&router, 42);
    
    /* Verify connection is unregistered */
    assert(router.connections[0] == NULL);
    
    message_router_destroy(&router);
    task_queue_destroy(&queue);
    
    printf("PASSED\n");
}

/* Test 6: Parse message with long content (truncation) */
static void test_message_router_parse_long_content(void) {
    printf("Test 6: Parse message with long content... ");
    
    /* Create a message with content longer than MESSAGE_MAX */
    char long_content[MESSAGE_MAX + 100];
    memset(long_content, 'A', sizeof(long_content) - 1);
    long_content[sizeof(long_content) - 1] = '\0';
    
    char json[MESSAGE_MAX + 500];
    snprintf(json, sizeof(json), 
             "{\"type\":\"message\",\"sender\":\"alice\",\"room\":\"general\",\"content\":\"%s\",\"timestamp\":1234567890}",
             long_content);
    
    message_t msg;
    int result = message_router_parse(json, strlen(json), &msg);
    
    assert(result == ERR_SUCCESS);
    assert(strlen(msg.content) < MESSAGE_MAX);
    
    printf("PASSED\n");
}

/* Test 7: Initialize with invalid parameters */
static void test_message_router_init_invalid(void) {
    printf("Test 7: Initialize with invalid parameters... ");
    
    message_router_t router;
    task_queue_t queue;
    
    task_queue_init(&queue, 100);
    
    /* NULL router */
    int result = message_router_init(NULL, 100, &queue);
    assert(result == ERR_INVALID_PARAM);
    
    /* Invalid max_connections */
    result = message_router_init(&router, 0, &queue);
    assert(result == ERR_INVALID_PARAM);
    
    result = message_router_init(&router, -1, &queue);
    assert(result == ERR_INVALID_PARAM);
    
    task_queue_destroy(&queue);
    
    printf("PASSED\n");
}

int main(void) {
    printf("Running message_router tests...\n\n");
    
    test_message_router_init();
    test_message_router_parse_valid();
    test_message_router_parse_no_timestamp();
    test_message_router_parse_invalid();
    test_message_router_register_unregister();
    test_message_router_parse_long_content();
    test_message_router_init_invalid();
    
    printf("\nAll tests passed!\n");
    return 0;
}
