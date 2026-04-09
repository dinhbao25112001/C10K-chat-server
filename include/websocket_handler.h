#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include "common.h"
#include "connection_handler.h"

#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT 0x1
#define WS_OPCODE_BINARY 0x2
#define WS_OPCODE_CLOSE 0x8
#define WS_OPCODE_PING 0x9
#define WS_OPCODE_PONG 0xA

typedef struct websocket_frame {
    uint8_t opcode;
    bool fin;
    bool masked;
    uint64_t payload_length;
    uint8_t mask_key[4];
    char *payload;
} websocket_frame_t;

/* Perform WebSocket handshake */
int websocket_handshake(connection_t *conn, const char *http_request);

/* Parse WebSocket frame */
int websocket_parse_frame(const char *data, size_t len, websocket_frame_t *frame);

/* Encode data as WebSocket frame */
int websocket_encode_frame(const char *data, size_t len, uint8_t opcode, 
                            char *output, size_t *output_len);

/* Handle WebSocket close frame */
void websocket_close(connection_t *conn, uint16_t status_code);

#endif /* WEBSOCKET_HANDLER_H */
