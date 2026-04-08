#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include "common.h"
#include "connection_handler.h"

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
