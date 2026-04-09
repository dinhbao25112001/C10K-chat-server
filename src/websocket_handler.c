#include "websocket_handler.h"
#include "connection_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <arpa/inet.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* Base64 encode helper */
static char* base64_encode(const unsigned char *input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);
    
    char *buff = (char *)malloc(bptr->length + 1);
    if (buff) {
        memcpy(buff, bptr->data, bptr->length);
        buff[bptr->length] = '\0';
    }
    
    BIO_free_all(b64);
    return buff;
}

/* Extract WebSocket key from HTTP request */
static int extract_websocket_key(const char *http_request, char *key, size_t key_size) {
    const char *key_header = "Sec-WebSocket-Key: ";
    const char *key_start = strstr(http_request, key_header);
    
    if (!key_start) {
        return ERR_INVALID_PARAM;
    }
    
    key_start += strlen(key_header);
    const char *key_end = strstr(key_start, "\r\n");
    
    if (!key_end) {
        return ERR_INVALID_PARAM;
    }
    
    size_t key_len = key_end - key_start;
    if (key_len >= key_size) {
        return ERR_INVALID_PARAM;
    }
    
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';
    
    return ERR_SUCCESS;
}

int websocket_handshake(connection_t *conn, const char *http_request) {
    if (!conn || !http_request) {
        return ERR_INVALID_PARAM;
    }
    
    /* Validate HTTP Upgrade request */
    if (!strstr(http_request, "Upgrade: websocket") || 
        !strstr(http_request, "Connection: Upgrade")) {
        return ERR_INVALID_PARAM;
    }
    
    /* Extract Sec-WebSocket-Key */
    char client_key[256];
    if (extract_websocket_key(http_request, client_key, sizeof(client_key)) != ERR_SUCCESS) {
        return ERR_INVALID_PARAM;
    }
    
    /* Concatenate key with GUID */
    char accept_key[512];
    snprintf(accept_key, sizeof(accept_key), "%s%s", client_key, WS_GUID);
    
    /* Compute SHA-1 hash */
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)accept_key, strlen(accept_key), hash);
    
    /* Base64 encode the hash */
    char *accept_value = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (!accept_value) {
        return ERR_MEMORY_ALLOC;
    }
    
    /* Send 101 Switching Protocols response */
    char response[1024];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_value);
    
    free(accept_value);
    
    if (response_len < 0 || (size_t)response_len >= sizeof(response)) {
        return ERR_INVALID_PARAM;
    }
    
    /* Buffer the response for writing */
    int result = connection_buffer_write(conn, response, response_len);
    if (result == ERR_SUCCESS) {
        conn->is_websocket = true;
    }
    
    return result;
}

int websocket_parse_frame(const char *data, size_t len, websocket_frame_t *frame) {
    if (!data || !frame || len < 2) {
        return ERR_INVALID_PARAM;
    }
    
    /* Parse first byte: FIN, RSV, opcode */
    frame->fin = (data[0] & 0x80) != 0;
    frame->opcode = data[0] & 0x0F;
    
    /* Parse second byte: MASK, payload length */
    frame->masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    
    size_t header_size = 2;
    
    /* Extended payload length */
    if (payload_len == 126) {
        if (len < 4) {
            return ERR_INVALID_PARAM;
        }
        payload_len = (data[2] << 8) | data[3];
        header_size += 2;
    } else if (payload_len == 127) {
        if (len < 10) {
            return ERR_INVALID_PARAM;
        }
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | (unsigned char)data[2 + i];
        }
        header_size += 8;
    }
    
    frame->payload_length = payload_len;
    
    /* Extract masking key if present */
    if (frame->masked) {
        if (len < header_size + 4) {
            return ERR_INVALID_PARAM;
        }
        memcpy(frame->mask_key, data + header_size, 4);
        header_size += 4;
    }
    
    /* Check if we have the complete payload */
    if (len < header_size + payload_len) {
        return ERR_INVALID_PARAM;
    }
    
    /* Allocate and unmask payload */
    frame->payload = (char *)malloc(payload_len + 1);
    if (!frame->payload) {
        return ERR_MEMORY_ALLOC;
    }
    
    if (frame->masked) {
        for (uint64_t i = 0; i < payload_len; i++) {
            frame->payload[i] = data[header_size + i] ^ frame->mask_key[i % 4];
        }
    } else {
        memcpy(frame->payload, data + header_size, payload_len);
    }
    
    frame->payload[payload_len] = '\0';
    
    return ERR_SUCCESS;
}

int websocket_encode_frame(const char *data, size_t len, uint8_t opcode, 
                            char *output, size_t *output_len) {
    if (!data || !output || !output_len) {
        return ERR_INVALID_PARAM;
    }
    
    size_t header_size = 2;
    size_t frame_size;
    
    /* Determine header size based on payload length */
    if (len <= 125) {
        header_size = 2;
    } else if (len <= 65535) {
        header_size = 4;
    } else {
        header_size = 10;
    }
    
    frame_size = header_size + len;
    
    if (*output_len < frame_size) {
        return ERR_INVALID_PARAM;
    }
    
    /* First byte: FIN=1, RSV=0, opcode */
    output[0] = 0x80 | (opcode & 0x0F);
    
    /* Second byte: MASK=0 (server doesn't mask), payload length */
    if (len <= 125) {
        output[1] = len;
    } else if (len <= 65535) {
        output[1] = 126;
        output[2] = (len >> 8) & 0xFF;
        output[3] = len & 0xFF;
    } else {
        output[1] = 127;
        for (int i = 0; i < 8; i++) {
            output[2 + i] = (len >> (56 - i * 8)) & 0xFF;
        }
    }
    
    /* Copy payload */
    memcpy(output + header_size, data, len);
    
    *output_len = frame_size;
    
    return ERR_SUCCESS;
}

void websocket_close(connection_t *conn, uint16_t status_code) {
    if (!conn) {
        return;
    }
    
    /* Encode close frame with status code */
    char close_payload[2];
    close_payload[0] = (status_code >> 8) & 0xFF;
    close_payload[1] = status_code & 0xFF;
    
    char frame[128];
    size_t frame_len = sizeof(frame);
    
    if (websocket_encode_frame(close_payload, 2, WS_OPCODE_CLOSE, frame, &frame_len) == ERR_SUCCESS) {
        connection_buffer_write(conn, frame, frame_len);
    }
    
    /* Close the connection */
    connection_close(conn);
}
