#include "../include/websocket_handler.h"
#include "../include/connection_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test 1: Parse simple text frame */
static void test_parse_simple_text_frame(void) {
    printf("Test 1: Parse simple text frame... ");
    
    /* Create a simple masked text frame: "Hello" */
    /* FIN=1, opcode=0x1 (text), MASK=1, payload_len=5 */
    /* To mask "Hello" with key [0x12, 0x34, 0x56, 0x78]:
     * H (0x48) ^ 0x12 = 0x5A
     * e (0x65) ^ 0x34 = 0x51
     * l (0x6C) ^ 0x56 = 0x3A
     * l (0x6C) ^ 0x78 = 0x14
     * o (0x6F) ^ 0x12 = 0x7D
     */
    unsigned char frame_data[] = {
        0x81,       /* FIN=1, opcode=1 */
        0x85,       /* MASK=1, len=5 */
        0x12, 0x34, 0x56, 0x78,  /* mask key */
        0x5A, 0x51, 0x3A, 0x14, 0x7D  /* masked "Hello" */
    };
    
    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    
    int result = websocket_parse_frame((char*)frame_data, sizeof(frame_data), &frame);
    
    assert(result == ERR_SUCCESS);
    assert(frame.fin == true);
    assert(frame.opcode == 0x1);
    assert(frame.masked == true);
    assert(frame.payload_length == 5);
    assert(frame.payload != NULL);
    assert(strcmp(frame.payload, "Hello") == 0);
    
    free(frame.payload);
    
    printf("PASSED\n");
}

/* Test 2: Encode simple text frame */
static void test_encode_simple_text_frame(void) {
    printf("Test 2: Encode simple text frame... ");
    
    const char *message = "Hello";
    char output[256];
    size_t output_len = sizeof(output);
    
    int result = websocket_encode_frame(message, strlen(message), 0x1, output, &output_len);
    
    assert(result == ERR_SUCCESS);
    assert(output_len == 7);  /* 2 byte header + 5 byte payload */
    assert((output[0] & 0x80) != 0);  /* FIN=1 */
    assert((output[0] & 0x0F) == 0x1);  /* opcode=1 */
    assert((output[1] & 0x80) == 0);  /* MASK=0 (server doesn't mask) */
    assert((output[1] & 0x7F) == 5);  /* payload length=5 */
    assert(memcmp(output + 2, "Hello", 5) == 0);
    
    printf("PASSED\n");
}

/* Test 3: Encode frame with extended length (126) */
static void test_encode_extended_length_frame(void) {
    printf("Test 3: Encode frame with extended length... ");
    
    char message[200];
    memset(message, 'A', sizeof(message));
    message[sizeof(message) - 1] = '\0';
    
    char output[512];
    size_t output_len = sizeof(output);
    
    int result = websocket_encode_frame(message, 199, 0x1, output, &output_len);
    
    assert(result == ERR_SUCCESS);
    assert(output_len == 203);  /* 4 byte header + 199 byte payload */
    assert((output[1] & 0x7F) == 126);  /* extended length indicator */
    assert((unsigned char)output[2] == 0);  /* high byte of length */
    assert((unsigned char)output[3] == 199);  /* low byte of length */
    
    printf("PASSED\n");
}

/* Test 4: Parse close frame */
static void test_parse_close_frame(void) {
    printf("Test 4: Parse close frame... ");
    
    /* Create a close frame with status code 1000 (0x03E8) */
    /* To mask [0x03, 0xE8] with key [0x12, 0x34, 0x56, 0x78]:
     * 0x03 ^ 0x12 = 0x11
     * 0xE8 ^ 0x34 = 0xDC
     */
    unsigned char frame_data[] = {
        0x88,       /* FIN=1, opcode=8 (close) */
        0x82,       /* MASK=1, len=2 */
        0x12, 0x34, 0x56, 0x78,  /* mask key */
        0x11, 0xDC  /* masked status code 1000 */
    };
    
    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    
    int result = websocket_parse_frame((char*)frame_data, sizeof(frame_data), &frame);
    
    assert(result == ERR_SUCCESS);
    assert(frame.fin == true);
    assert(frame.opcode == 0x8);
    assert(frame.masked == true);
    assert(frame.payload_length == 2);
    
    free(frame.payload);
    
    printf("PASSED\n");
}

/* Test 5: Invalid frame (too short) */
static void test_parse_invalid_frame(void) {
    printf("Test 5: Parse invalid frame (too short)... ");
    
    unsigned char frame_data[] = { 0x81 };  /* Only 1 byte */
    
    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    
    int result = websocket_parse_frame((char*)frame_data, sizeof(frame_data), &frame);
    
    assert(result == ERR_INVALID_PARAM);
    
    printf("PASSED\n");
}

/* Test 6: Encode close frame */
static void test_encode_close_frame(void) {
    printf("Test 6: Encode close frame... ");
    
    char close_payload[2];
    uint16_t status_code = 1000;
    close_payload[0] = (status_code >> 8) & 0xFF;
    close_payload[1] = status_code & 0xFF;
    
    char output[256];
    size_t output_len = sizeof(output);
    
    int result = websocket_encode_frame(close_payload, 2, 0x8, output, &output_len);
    
    assert(result == ERR_SUCCESS);
    assert(output_len == 4);  /* 2 byte header + 2 byte payload */
    assert((output[0] & 0x0F) == 0x8);  /* opcode=8 (close) */
    assert((output[1] & 0x7F) == 2);  /* payload length=2 */
    
    printf("PASSED\n");
}

/* Test 7: WebSocket handshake with valid request */
static void test_websocket_handshake_valid(void) {
    printf("Test 7: WebSocket handshake with valid request... ");
    
    const char *http_request = 
        "GET /chat HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    
    connection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.socket_fd = -1;  /* Mock socket */
    
    int result = websocket_handshake(&conn, http_request);
    
    assert(result == ERR_SUCCESS);
    assert(conn.is_websocket == true);
    
    /* Verify response is buffered */
    assert(conn.write_length > 0);
    assert(strstr(conn.write_buffer, "101 Switching Protocols") != NULL);
    assert(strstr(conn.write_buffer, "Upgrade: websocket") != NULL);
    assert(strstr(conn.write_buffer, "Sec-WebSocket-Accept:") != NULL);
    
    printf("PASSED\n");
}

/* Test 8: WebSocket handshake with invalid request */
static void test_websocket_handshake_invalid(void) {
    printf("Test 8: WebSocket handshake with invalid request... ");
    
    const char *http_request = 
        "GET /chat HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n";
    
    connection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.socket_fd = -1;
    
    int result = websocket_handshake(&conn, http_request);
    
    assert(result == ERR_INVALID_PARAM);
    
    printf("PASSED\n");
}

int main(void) {
    printf("Running websocket_handler tests...\n\n");
    
    test_parse_simple_text_frame();
    test_encode_simple_text_frame();
    test_encode_extended_length_frame();
    test_parse_close_frame();
    test_parse_invalid_frame();
    test_encode_close_frame();
    test_websocket_handshake_valid();
    test_websocket_handshake_invalid();
    
    printf("\nAll tests passed!\n");
    return 0;
}
