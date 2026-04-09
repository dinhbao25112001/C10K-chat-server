#include "common.h"
#include "epoll_manager.h"
#include "connection_handler.h"
#include "thread_pool.h"
#include "task_queue.h"
#include "message_router.h"
#include "history_manager.h"
#include "auth_manager.h"
#include "signal_handler.h"
#include "websocket_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

/* Forward declarations of task handlers */
static void handle_auth_task(void *data);
static void handle_message_process_task(void *data);
static void handle_message_send_task(void *data);
static void handle_history_write_task(void *data);
static void handle_cleanup_task(void *data);

/* Forward declaration of static file serving */
static int serve_static_file(connection_t *conn, const char *static_files_path, const char *request_path);

/* Server configuration structure */
typedef struct server_config {
    int port;
    int max_connections;
    int thread_pool_size;
    int task_queue_capacity;
    int connection_timeout_seconds;
    int epoll_timeout_ms;
    char history_base_path[PATH_MAX];
    char credentials_file[PATH_MAX];
    char static_files_path[PATH_MAX];
} server_config_t;

/* Global context for task handlers */
typedef struct server_context {
    epoll_manager_t *epoll_manager;
    auth_manager_t *auth_manager;
    message_router_t *message_router;
    history_manager_t *history_manager;
    task_queue_t *task_queue;
    connection_t *connections;
    int max_connections;
    char static_files_path[PATH_MAX];
} server_context_t;

/* Task data structures */
typedef struct auth_task_data {
    connection_t *conn;
    server_context_t *ctx;
} auth_task_data_t;

typedef struct message_process_task_data {
    connection_t *conn;
    server_context_t *ctx;
} message_process_task_data_t;

typedef struct message_send_task_data {
    connection_t *conn;
    message_t *msg;
    server_context_t *ctx;
} message_send_task_data_t;

typedef struct history_write_task_data {
    message_t *msg;
    server_context_t *ctx;
} history_write_task_data_t;

typedef struct cleanup_task_data {
    connection_t *conn;
    server_context_t *ctx;
} cleanup_task_data_t;

/* Task handler functions */

/* Handle authentication task */
static void handle_auth_task(void *data) {
    auth_task_data_t *task_data = (auth_task_data_t *)data;
    if (!task_data || !task_data->conn || !task_data->ctx) {
        free(task_data);
        return;
    }
    
    connection_t *conn = task_data->conn;
    server_context_t *ctx = task_data->ctx;
    
    /* Check if this is an HTTP request (starts with GET, POST, etc.) */
    if (conn->state == CONN_CONNECTING && conn->read_offset > 0 && 
        (strncmp(conn->read_buffer, "GET ", 4) == 0 ||
         strncmp(conn->read_buffer, "POST ", 5) == 0)) {
        
        /* Parse HTTP request to extract path */
        char method[16], path[256], version[16];
        if (sscanf(conn->read_buffer, "%15s %255s %15s", method, path, version) == 3) {
            /* Check if this is a WebSocket upgrade request */
            if (strstr(conn->read_buffer, "Upgrade: websocket") != NULL ||
                strstr(conn->read_buffer, "Upgrade: WebSocket") != NULL) {
                
                /* Handle WebSocket handshake */
                if (websocket_handshake(conn, conn->read_buffer) == ERR_SUCCESS) {
                    conn->state = CONN_AUTHENTICATING;
                    conn->is_websocket = true;
                    printf("WebSocket connection established on fd %d\n", conn->socket_fd);
                    
                    /* Register for EPOLLOUT to send buffered handshake response */
                    epoll_manager_modify(ctx->epoll_manager, conn->socket_fd, 
                                       EPOLLIN | EPOLLOUT, conn);
                } else {
                    conn->state = CONN_CLOSING;
                }
            } else {
                /* Serve static file */
                serve_static_file(conn, ctx->static_files_path, path);
                
                /* Mark connection for closing after sending response */
                conn->state = CONN_CLOSING;
                
                /* Register for EPOLLOUT to send buffered data */
                epoll_manager_modify(ctx->epoll_manager, conn->socket_fd, 
                                   EPOLLIN | EPOLLOUT, conn);
            }
        }
    } else if (conn->state == CONN_AUTHENTICATING && conn->is_websocket) {
        websocket_frame_t frame;
        if (websocket_parse_frame(conn->read_buffer, conn->read_offset, &frame) == ERR_SUCCESS) {
            if (frame.opcode == WS_OPCODE_TEXT) {
                char username[USERNAME_MAX] = "user";
                const char *username_key = "\"username\":\"";
                char *username_start = strstr(frame.payload, username_key);
                if (username_start) {
                    username_start += strlen(username_key);
                    char *username_end = strchr(username_start, '"');
                    if (username_end) {
                        size_t len = username_end - username_start;
                        if (len >= USERNAME_MAX) len = USERNAME_MAX - 1;
                        strncpy(username, username_start, len);
                        username[len] = '\0';
                    }
                } else {
                    snprintf(username, sizeof(username), "user_%d", conn->socket_fd);
                }
                
                client_session_t *session = NULL;
                int result = auth_manager_create_session(ctx->auth_manager, username, &session);
                if (result == ERR_SUCCESS && session) {
                    auth_manager_associate_session(ctx->auth_manager, session, conn->socket_fd);
                    conn->session = session;
                    conn->state = CONN_AUTHENTICATED;
                    
                    strncpy(session->room, "general", ROOMNAME_MAX - 1);
                    message_router_register(ctx->message_router, conn);
                    
                    printf("Client %d authenticated as %s\n", conn->socket_fd, username);
                    
                    /* Send auth success */
                    char response[256];
                    snprintf(response, sizeof(response), "{\"type\":\"auth_success\",\"username\":\"%s\"}", username);
                    char ws_frame[1024];
                    size_t ws_frame_len = sizeof(ws_frame);
                    if (websocket_encode_frame(response, strlen(response), WS_OPCODE_TEXT, ws_frame, &ws_frame_len) == ERR_SUCCESS) {
                        connection_buffer_write(conn, ws_frame, ws_frame_len);
                        epoll_manager_modify(ctx->epoll_manager, conn->socket_fd, EPOLLIN | EPOLLOUT, conn);
                    }
                } else {
                    conn->state = CONN_CLOSING;
                }
            } else if (frame.opcode == WS_OPCODE_CLOSE) {
                conn->state = CONN_CLOSING;
            }
            if (frame.payload) free(frame.payload);
            conn->read_offset = 0;
        }
    } else {
        /* Authentication failed or invalid state */
        conn->state = CONN_CLOSING;
    }
    
    free(task_data);
}

/* Handle message processing task */
static void handle_message_process_task(void *data) {
    message_process_task_data_t *task_data = (message_process_task_data_t *)data;
    if (!task_data || !task_data->conn || !task_data->ctx) {
        free(task_data);
        return;
    }
    
    connection_t *conn = task_data->conn;
    server_context_t *ctx = task_data->ctx;
    
    /* Parse message from read buffer */
    message_t msg;
    int result = ERR_INVALID_PARAM;
    
    if (conn->is_websocket) {
        websocket_frame_t frame;
        if (websocket_parse_frame(conn->read_buffer, conn->read_offset, &frame) == ERR_SUCCESS) {
            if (frame.opcode == WS_OPCODE_TEXT) {
                result = message_router_parse(frame.payload, strlen(frame.payload), &msg);
            } else if (frame.opcode == WS_OPCODE_CLOSE) {
                conn->state = CONN_CLOSING;
            }
            if (frame.payload) free(frame.payload);
        }
    } else {
        result = message_router_parse(conn->read_buffer, conn->read_offset, &msg);
    }
    
    if (result == ERR_SUCCESS) {
        /* Find all recipients in the same room and enqueue send tasks */
        pthread_rwlock_rdlock(&ctx->message_router->lock);
        
        for (int i = 0; i < ctx->message_router->max_connections; i++) {
            connection_t *recipient = ctx->message_router->connections[i];
            
            if (!recipient || recipient->state != CONN_AUTHENTICATED || !recipient->session) {
                continue;
            }
            
            /* Check if recipient is in the same room */
            if (strcmp(recipient->session->room, msg.room) == 0) {
                /* Create message copy for this recipient */
                message_t *msg_copy = malloc(sizeof(message_t));
                if (msg_copy) {
                    memcpy(msg_copy, &msg, sizeof(message_t));
                    
                    message_send_task_data_t *send_data = malloc(sizeof(message_send_task_data_t));
                    if (send_data) {
                        send_data->conn = recipient;
                        send_data->msg = msg_copy;
                        send_data->ctx = ctx;
                        
                        task_t *send_task = malloc(sizeof(task_t));
                        if (send_task) {
                            send_task->type = TASK_MESSAGE_SEND;
                            send_task->data = send_data;
                            send_task->handler = handle_message_send_task;
                            send_task->next = NULL;
                            
                            task_queue_enqueue(ctx->task_queue, send_task);
                        } else {
                            free(send_data);
                            free(msg_copy);
                        }
                    } else {
                        free(msg_copy);
                    }
                }
            }
        }
        
        pthread_rwlock_unlock(&ctx->message_router->lock);
        
        /* Enqueue history write task */
        message_t *msg_copy = malloc(sizeof(message_t));
        if (msg_copy) {
            memcpy(msg_copy, &msg, sizeof(message_t));
            
            history_write_task_data_t *history_data = malloc(sizeof(history_write_task_data_t));
            if (history_data) {
                history_data->msg = msg_copy;
                history_data->ctx = ctx;
                
                task_t *history_task = malloc(sizeof(task_t));
                if (history_task) {
                    history_task->type = TASK_HISTORY_WRITE;
                    history_task->data = history_data;
                    history_task->handler = handle_history_write_task;
                    history_task->next = NULL;
                    
                    task_queue_enqueue(ctx->task_queue, history_task);
                } else {
                    free(history_data);
                    free(msg_copy);
                }
            } else {
                free(msg_copy);
            }
        }
    }
    
    /* Clear read buffer after processing */
    conn->read_offset = 0;
    
    free(task_data);
}

/* Handle message send task */
static void handle_message_send_task(void *data) {
    message_send_task_data_t *task_data = (message_send_task_data_t *)data;
    if (!task_data || !task_data->conn || !task_data->msg || !task_data->ctx) {
        if (task_data) {
            free(task_data->msg);
            free(task_data);
        }
        return;
    }
    
    connection_t *conn = task_data->conn;
    message_t *msg = task_data->msg;
    server_context_t *ctx = task_data->ctx;
    
    /* Format message as JSON */
    char buffer[MESSAGE_MAX + 256];
    int len = snprintf(buffer, sizeof(buffer),
                      "{\"type\":\"message\",\"sender\":\"%s\",\"room\":\"%s\",\"content\":\"%s\",\"timestamp\":%ld}\n",
                      msg->sender, msg->room, msg->content, (long)msg->timestamp);
    
    if (len > 0 && (size_t)len < sizeof(buffer)) {
        if (conn->is_websocket) {
            char ws_frame[2048];
            size_t ws_frame_len = sizeof(ws_frame);
            if (websocket_encode_frame(buffer, len, WS_OPCODE_TEXT, ws_frame, &ws_frame_len) == ERR_SUCCESS) {
                if (connection_buffer_write(conn, ws_frame, ws_frame_len) == ERR_SUCCESS) {
                    epoll_manager_modify(ctx->epoll_manager, conn->socket_fd, 
                                       EPOLLIN | EPOLLOUT, conn);
                }
            }
        } else {
            /* Buffer message for sending */
            if (connection_buffer_write(conn, buffer, len) == ERR_SUCCESS) {
                /* Register for EPOLLOUT to send buffered data */
                epoll_manager_modify(ctx->epoll_manager, conn->socket_fd, 
                                   EPOLLIN | EPOLLOUT, conn);
            }
        }
    }
    
    free(msg);
    free(task_data);
}

/* Handle history write task */
static void handle_history_write_task(void *data) {
    history_write_task_data_t *task_data = (history_write_task_data_t *)data;
    if (!task_data || !task_data->msg || !task_data->ctx) {
        if (task_data) {
            free(task_data->msg);
            free(task_data);
        }
        return;
    }
    
    message_t *msg = task_data->msg;
    server_context_t *ctx = task_data->ctx;
    
    /* Write message to history */
    history_manager_write(ctx->history_manager, msg);
    
    free(msg);
    free(task_data);
}

/* Handle cleanup task */
static void handle_cleanup_task(void *data) {
    cleanup_task_data_t *task_data = (cleanup_task_data_t *)data;
    if (!task_data || !task_data->conn || !task_data->ctx) {
        free(task_data);
        return;
    }
    
    connection_t *conn = task_data->conn;
    server_context_t *ctx = task_data->ctx;
    
    /* Unregister from message router */
    message_router_unregister(ctx->message_router, conn->socket_fd);
    
    /* Invalidate session */
    if (conn->session) {
        auth_manager_invalidate_session(ctx->auth_manager, conn->socket_fd);
    }
    
    /* Remove from epoll */
    epoll_manager_remove(ctx->epoll_manager, conn->socket_fd);
    
    /* Close connection */
    connection_close(conn);
    
    printf("Connection %d cleaned up\n", conn->socket_fd);
    
    free(task_data);
}

/* Determine content type based on file extension */
static const char* get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "text/html";
    } else if (strcmp(ext, ".css") == 0) {
        return "text/css";
    } else if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext, ".json") == 0) {
        return "application/json";
    } else if (strcmp(ext, ".png") == 0) {
        return "image/png";
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext, ".gif") == 0) {
        return "image/gif";
    } else if (strcmp(ext, ".svg") == 0) {
        return "image/svg+xml";
    } else if (strcmp(ext, ".ico") == 0) {
        return "image/x-icon";
    } else if (strcmp(ext, ".txt") == 0) {
        return "text/plain";
    }
    
    return "application/octet-stream";
}

/* Serve static file from the web directory */
static int serve_static_file(connection_t *conn, const char *static_files_path, const char *request_path) {
    if (!conn || !static_files_path || !request_path) {
        return ERR_INVALID_PARAM;
    }
    
    /* Build full file path */
    char file_path[PATH_MAX];
    
    /* Strip query string from request path if present */
    char clean_path[256];
    strncpy(clean_path, request_path, sizeof(clean_path) - 1);
    clean_path[sizeof(clean_path) - 1] = '\0';
    
    char *query = strchr(clean_path, '?');
    if (query) {
        *query = '\0';
    }
    
    /* Default to index.html if root path requested */
    const char *file_name = clean_path;
    if (strcmp(clean_path, "/") == 0 || strlen(clean_path) == 0) {
        file_name = "/index.html";
    }
    
    /* Construct full path, ensuring no directory traversal */
    snprintf(file_path, sizeof(file_path), "%s%s", static_files_path, file_name);
    
    /* Security check: ensure path doesn't contain ".." */
    if (strstr(file_path, "..") != NULL) {
        /* Send 403 Forbidden */
        const char *response = "HTTP/1.1 403 Forbidden\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 9\r\n"
                              "Connection: close\r\n"
                              "\r\n"
                              "Forbidden";
        connection_buffer_write(conn, response, strlen(response));
        return ERR_INVALID_PARAM;
    }
    
    /* Try to open file */
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        /* Send 404 Not Found */
        const char *response = "HTTP/1.1 404 Not Found\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 9\r\n"
                              "Connection: close\r\n"
                              "\r\n"
                              "Not Found";
        connection_buffer_write(conn, response, strlen(response));
        return ERR_FILE_NOT_FOUND;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Determine content type */
    const char *content_type = get_content_type(file_path);
    
    /* Send HTTP response header */
    char header[512];
    int header_len = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %ld\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             content_type, file_size);
    
    if (header_len > 0 && (size_t)header_len < sizeof(header)) {
        connection_buffer_write(conn, header, header_len);
    }
    
    /* Read and buffer file content */
    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (connection_buffer_write(conn, buffer, bytes_read) != ERR_SUCCESS) {
            fclose(fp);
            return ERR_BUFFER_FULL;
        }
    }
    
    fclose(fp);
    return ERR_SUCCESS;
}

/* Load configuration from file or use defaults */
static int load_config(server_config_t *config, const char *config_file) {
    if (!config) {
        return ERR_INVALID_PARAM;
    }
    
    /* Set default values and determine starting directory context */
    config->port = DEFAULT_PORT;
    config->max_connections = DEFAULT_MAX_CONNECTIONS;
    config->thread_pool_size = DEFAULT_THREAD_POOL_SIZE;
    config->task_queue_capacity = DEFAULT_TASK_QUEUE_CAPACITY;
    config->connection_timeout_seconds = DEFAULT_CONNECTION_TIMEOUT;
    config->epoll_timeout_ms = DEFAULT_EPOLL_TIMEOUT;
    
    FILE *test_fp = fopen("web/index.html", "r");
    if (test_fp) {
        fclose(test_fp);
        strncpy(config->history_base_path, "chat_history", PATH_MAX - 1);
        strncpy(config->credentials_file, "config/credentials.txt", PATH_MAX - 1);
        strncpy(config->static_files_path, "web", PATH_MAX - 1);
    } else {
        strncpy(config->history_base_path, "../chat_history", PATH_MAX - 1);
        strncpy(config->credentials_file, "../config/credentials.txt", PATH_MAX - 1);
        strncpy(config->static_files_path, "../web", PATH_MAX - 1);
    }
    
    /* If config file provided, parse it */
    if (config_file) {
        FILE *fp = fopen(config_file, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                /* Skip comments and empty lines */
                if (line[0] == '#' || line[0] == '\n') {
                    continue;
                }
                
                /* Parse key=value pairs */
                char key[128], value[128];
                if (sscanf(line, "%127[^=]=%127s", key, value) == 2) {
                    if (strcmp(key, "port") == 0) {
                        config->port = atoi(value);
                    } else if (strcmp(key, "max_connections") == 0) {
                        config->max_connections = atoi(value);
                    } else if (strcmp(key, "thread_pool_size") == 0) {
                        config->thread_pool_size = atoi(value);
                    } else if (strcmp(key, "task_queue_capacity") == 0) {
                        config->task_queue_capacity = atoi(value);
                    } else if (strcmp(key, "connection_timeout") == 0) {
                        config->connection_timeout_seconds = atoi(value);
                    } else if (strcmp(key, "epoll_timeout") == 0) {
                        config->epoll_timeout_ms = atoi(value);
                    } else if (strcmp(key, "history_path") == 0) {
                        strncpy(config->history_base_path, value, PATH_MAX - 1);
                    } else if (strcmp(key, "credentials_file") == 0) {
                        strncpy(config->credentials_file, value, PATH_MAX - 1);
                    } else if (strcmp(key, "static_files_path") == 0) {
                        strncpy(config->static_files_path, value, PATH_MAX - 1);
                    }
                }
            }
            fclose(fp);
        }
    }
    
    return ERR_SUCCESS;
}

/* Create and configure listening socket */
static int create_listening_socket(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return ERR_SOCKET_ERROR;
    }
    
    /* Set SO_REUSEADDR to allow quick restart */
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(listen_fd);
        return ERR_SOCKET_ERROR;
    }
    
    /* Bind to port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return ERR_SOCKET_ERROR;
    }
    
    /* Set to non-blocking mode */
    int flags = fcntl(listen_fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        close(listen_fd);
        return ERR_SOCKET_ERROR;
    }
    
    if (fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL O_NONBLOCK");
        close(listen_fd);
        return ERR_SOCKET_ERROR;
    }
    
    /* Listen with backlog */
    if (listen(listen_fd, 1024) < 0) {
        perror("listen");
        close(listen_fd);
        return ERR_SOCKET_ERROR;
    }
    
    return listen_fd;
}

int main(int argc, char *argv[]) {
    int ret = ERR_SUCCESS;
    
    printf("C10K Chat Server - Starting...\n");
    
    /* Load configuration */
    server_config_t config;
    const char *config_file = (argc > 1) ? argv[1] : NULL;
    if (load_config(&config, config_file) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to load configuration\n");
        return EXIT_FAILURE;
    }
    
    printf("Configuration loaded:\n");
    printf("  Port: %d\n", config.port);
    printf("  Max connections: %d\n", config.max_connections);
    printf("  Thread pool size: %d\n", config.thread_pool_size);
    printf("  Task queue capacity: %d\n", config.task_queue_capacity);
    
    /* Initialize signal handler */
    signal_handler_t signal_handler;
    if (signal_handler_init(&signal_handler) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize signal handler\n");
        return EXIT_FAILURE;
    }
    
    if (signal_handler_start(&signal_handler) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to start signal handler\n");
        return EXIT_FAILURE;
    }
    
    /* Initialize task queue */
    task_queue_t task_queue;
    if (task_queue_init(&task_queue, config.task_queue_capacity) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize task queue\n");
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Initialize thread pool */
    thread_pool_t thread_pool;
    if (thread_pool_init(&thread_pool, config.thread_pool_size, &task_queue) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Initialize epoll manager */
    epoll_manager_t epoll_manager;
    if (epoll_manager_init(&epoll_manager, config.max_connections) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize epoll manager\n");
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Initialize auth manager */
    auth_manager_t auth_manager;
    if (auth_manager_init(&auth_manager, config.credentials_file) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize auth manager\n");
        epoll_manager_destroy(&epoll_manager);
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Initialize message router */
    message_router_t message_router;
    if (message_router_init(&message_router, config.max_connections, &task_queue) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize message router\n");
        auth_manager_destroy(&auth_manager);
        epoll_manager_destroy(&epoll_manager);
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Initialize history manager */
    history_manager_t history_manager;
    if (history_manager_init(&history_manager, config.history_base_path) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to initialize history manager\n");
        message_router_destroy(&message_router);
        auth_manager_destroy(&auth_manager);
        epoll_manager_destroy(&epoll_manager);
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Create listening socket */
    int listen_fd = create_listening_socket(config.port);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create listening socket\n");
        history_manager_destroy(&history_manager);
        message_router_destroy(&message_router);
        auth_manager_destroy(&auth_manager);
        epoll_manager_destroy(&epoll_manager);
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    printf("Listening on port %d\n", config.port);
    
    /* Add listening socket to epoll */
    if (epoll_manager_add(&epoll_manager, listen_fd, EPOLLIN | EPOLLET, NULL) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to add listening socket to epoll\n");
        close(listen_fd);
        history_manager_destroy(&history_manager);
        message_router_destroy(&message_router);
        auth_manager_destroy(&auth_manager);
        epoll_manager_destroy(&epoll_manager);
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    printf("Server initialized successfully\n");
    
    /* Allocate connection pool */
    connection_t *connections = calloc(config.max_connections, sizeof(connection_t));
    if (!connections) {
        fprintf(stderr, "Failed to allocate connection pool\n");
        close(listen_fd);
        history_manager_destroy(&history_manager);
        message_router_destroy(&message_router);
        auth_manager_destroy(&auth_manager);
        epoll_manager_destroy(&epoll_manager);
        thread_pool_destroy(&thread_pool);
        task_queue_destroy(&task_queue);
        signal_handler_stop(&signal_handler);
        return EXIT_FAILURE;
    }
    
    /* Initialize all connections as invalid */
    for (int i = 0; i < config.max_connections; i++) {
        connections[i].socket_fd = -1;
    }
    
    /* Setup server context for task handlers */
    server_context_t server_ctx = {
        .epoll_manager = &epoll_manager,
        .auth_manager = &auth_manager,
        .message_router = &message_router,
        .history_manager = &history_manager,
        .task_queue = &task_queue,
        .connections = connections,
        .max_connections = config.max_connections
    };
    strncpy(server_ctx.static_files_path, config.static_files_path, PATH_MAX - 1);
    server_ctx.static_files_path[PATH_MAX - 1] = '\0';
    
    /* Track last timeout check time */
    time_t last_timeout_check = time(NULL);
    
    /* Main event loop */
    while (!signal_handler_should_shutdown(&signal_handler)) {
        int num_events = epoll_manager_wait(&epoll_manager, config.epoll_timeout_ms);
        
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal, check shutdown flag */
            }
            perror("epoll_wait");
            ret = ERR_EPOLL_ERROR;
            break;
        }
        
        /* Process events */
        for (int i = 0; i < num_events; i++) {
            struct epoll_event *event = epoll_manager_get_event(&epoll_manager, i);
            if (!event) {
                continue;
            }
            
            /* Check if this is the listening socket */
            if (event->data.ptr == NULL) {
                /* Listening socket - accept new connections */
                while (1) {
                    /* Find free connection slot */
                    connection_t *conn = NULL;
                    for (int j = 0; j < config.max_connections; j++) {
                        if (connections[j].socket_fd == -1) {
                            conn = &connections[j];
                            break;
                        }
                    }
                    
                    if (!conn) {
                        fprintf(stderr, "Connection pool exhausted\n");
                        break;
                    }
                    
                    /* Accept connection */
                    int result = connection_accept(listen_fd, conn);
                    if (result != ERR_SUCCESS) {
                        break;  /* No more connections or error */
                    }
                    
                    if (conn->socket_fd < 0) {
                        break;  /* No connection accepted */
                    }
                    
                    printf("Accepted connection from %s:%d (fd=%d)\n",
                           inet_ntoa(conn->addr.sin_addr),
                           ntohs(conn->addr.sin_port),
                           conn->socket_fd);
                    
                    /* Add to epoll */
                    if (epoll_manager_add(&epoll_manager, conn->socket_fd, EPOLLIN, conn) != ERR_SUCCESS) {
                        fprintf(stderr, "Failed to add connection to epoll\n");
                        connection_close(conn);
                        conn->socket_fd = -1;
                        continue;
                    }
                    
                    /* Don't enqueue auth task yet - wait for first data to arrive */
                }
            } else {
                /* Client socket event */
                connection_t *conn = (connection_t *)event->data.ptr;
                
                /* Handle errors */
                if (event->events & (EPOLLERR | EPOLLHUP)) {
                    fprintf(stderr, "Socket error on fd %d\n", conn->socket_fd);
                    
                    /* Enqueue cleanup task */
                    cleanup_task_data_t *cleanup_data = malloc(sizeof(cleanup_task_data_t));
                    if (cleanup_data) {
                        cleanup_data->conn = conn;
                        cleanup_data->ctx = &server_ctx;
                        
                        task_t *cleanup_task = malloc(sizeof(task_t));
                        if (cleanup_task) {
                            cleanup_task->type = TASK_CLEANUP;
                            cleanup_task->data = cleanup_data;
                            cleanup_task->handler = handle_cleanup_task;
                            cleanup_task->next = NULL;
                            
                            task_queue_enqueue(&task_queue, cleanup_task);
                        } else {
                            free(cleanup_data);
                        }
                    }
                    continue;
                }
                
                /* Handle readable event */
                if (event->events & EPOLLIN) {
                    ssize_t n = connection_read(conn);
                    
                    if (n == 0) {
                        /* Connection closed by peer */
                        printf("Connection %d closed by peer\n", conn->socket_fd);
                        
                        /* Enqueue cleanup task */
                        cleanup_task_data_t *cleanup_data = malloc(sizeof(cleanup_task_data_t));
                        if (cleanup_data) {
                            cleanup_data->conn = conn;
                            cleanup_data->ctx = &server_ctx;
                            
                            task_t *cleanup_task = malloc(sizeof(task_t));
                            if (cleanup_task) {
                                cleanup_task->type = TASK_CLEANUP;
                                cleanup_task->data = cleanup_data;
                                cleanup_task->handler = handle_cleanup_task;
                                cleanup_task->next = NULL;
                                
                                task_queue_enqueue(&task_queue, cleanup_task);
                            } else {
                                free(cleanup_data);
                            }
                        }
                    } else if (n < 0) {
                        /* Read error */
                        fprintf(stderr, "Read error on fd %d\n", conn->socket_fd);
                        
                        /* Enqueue cleanup task */
                        cleanup_task_data_t *cleanup_data = malloc(sizeof(cleanup_task_data_t));
                        if (cleanup_data) {
                            cleanup_data->conn = conn;
                            cleanup_data->ctx = &server_ctx;
                            
                            task_t *cleanup_task = malloc(sizeof(task_t));
                            if (cleanup_task) {
                                cleanup_task->type = TASK_CLEANUP;
                                cleanup_task->data = cleanup_data;
                                cleanup_task->handler = handle_cleanup_task;
                                cleanup_task->next = NULL;
                                
                                task_queue_enqueue(&task_queue, cleanup_task);
                            } else {
                                free(cleanup_data);
                            }
                        }
                    } else if (n > 0) {
                        /* Data received */
                        if (conn->state == CONN_CONNECTING || conn->state == CONN_AUTHENTICATING) {
                            /* First data received or auth frame, enqueue auth task to process */
                            auth_task_data_t *auth_data = malloc(sizeof(auth_task_data_t));
                            if (auth_data) {
                                auth_data->conn = conn;
                                auth_data->ctx = &server_ctx;
                                
                                task_t *auth_task = malloc(sizeof(task_t));
                                if (auth_task) {
                                    auth_task->type = TASK_AUTH;
                                    auth_task->data = auth_data;
                                    auth_task->handler = handle_auth_task;
                                    auth_task->next = NULL;
                                    
                                    if (task_queue_enqueue(&task_queue, auth_task) != ERR_SUCCESS) {
                                        free(auth_task);
                                        free(auth_data);
                                    }
                                } else {
                                    free(auth_data);
                                }
                            }
                        } else if (conn->state == CONN_AUTHENTICATED) {
                            /* Authenticated connection, process message */
                            message_process_task_data_t *msg_data = malloc(sizeof(message_process_task_data_t));
                            if (msg_data) {
                                msg_data->conn = conn;
                                msg_data->ctx = &server_ctx;
                                
                                task_t *msg_task = malloc(sizeof(task_t));
                                if (msg_task) {
                                    msg_task->type = TASK_MESSAGE_PROCESS;
                                    msg_task->data = msg_data;
                                    msg_task->handler = handle_message_process_task;
                                    msg_task->next = NULL;
                                    
                                    if (task_queue_enqueue(&task_queue, msg_task) != ERR_SUCCESS) {
                                        free(msg_task);
                                        free(msg_data);
                                    }
                                } else {
                                    free(msg_data);
                                }
                            }
                        }
                    }
                }
                
                /* Handle writable event */
                if (event->events & EPOLLOUT) {
                    ssize_t n = connection_write(conn);
                    
                    if (n < 0) {
                        /* Write error */
                        fprintf(stderr, "Write error on fd %d\n", conn->socket_fd);
                        
                        /* Enqueue cleanup task */
                        cleanup_task_data_t *cleanup_data = malloc(sizeof(cleanup_task_data_t));
                        if (cleanup_data) {
                            cleanup_data->conn = conn;
                            cleanup_data->ctx = &server_ctx;
                            
                            task_t *cleanup_task = malloc(sizeof(task_t));
                            if (cleanup_task) {
                                cleanup_task->type = TASK_CLEANUP;
                                cleanup_task->data = cleanup_data;
                                cleanup_task->handler = handle_cleanup_task;
                                cleanup_task->next = NULL;
                                
                                task_queue_enqueue(&task_queue, cleanup_task);
                            } else {
                                free(cleanup_data);
                            }
                        }
                    } else if (conn->write_length == 0) {
                        /* All data written, remove EPOLLOUT */
                        epoll_manager_modify(&epoll_manager, conn->socket_fd, EPOLLIN, conn);
                    }
                }
            }
        }
        
        /* Periodically check for connection timeouts (every 60 seconds) */
        time_t current_time = time(NULL);
        if (current_time - last_timeout_check >= 60) {
            last_timeout_check = current_time;
            
            for (int i = 0; i < config.max_connections; i++) {
                connection_t *conn = &connections[i];
                
                if (conn->socket_fd >= 0 && 
                    connection_is_timeout(conn, current_time, config.connection_timeout_seconds)) {
                    printf("Connection %d timed out\n", conn->socket_fd);
                    
                    /* Enqueue cleanup task */
                    cleanup_task_data_t *cleanup_data = malloc(sizeof(cleanup_task_data_t));
                    if (cleanup_data) {
                        cleanup_data->conn = conn;
                        cleanup_data->ctx = &server_ctx;
                        
                        task_t *cleanup_task = malloc(sizeof(task_t));
                        if (cleanup_task) {
                            cleanup_task->type = TASK_CLEANUP;
                            cleanup_task->data = cleanup_data;
                            cleanup_task->handler = handle_cleanup_task;
                            cleanup_task->next = NULL;
                            
                            task_queue_enqueue(&task_queue, cleanup_task);
                        } else {
                            free(cleanup_data);
                        }
                    }
                }
            }
        }
    }
    
    printf("C10K Chat Server - Shutting down...\n");
    
    /* Stop accepting new connections - remove listening socket from epoll */
    epoll_manager_remove(&epoll_manager, listen_fd);
    close(listen_fd);
    
    /* Close all active client sockets */
    printf("Closing all active connections...\n");
    for (int i = 0; i < config.max_connections; i++) {
        connection_t *conn = &connections[i];
        if (conn->socket_fd >= 0) {
            /* Unregister from message router */
            message_router_unregister(&message_router, conn->socket_fd);
            
            /* Invalidate session */
            if (conn->session) {
                auth_manager_invalidate_session(&auth_manager, conn->socket_fd);
            }
            
            /* Remove from epoll and close */
            epoll_manager_remove(&epoll_manager, conn->socket_fd);
            connection_close(conn);
        }
    }
    
    /* Shutdown thread pool to drain task queue */
    printf("Draining task queue...\n");
    thread_pool_shutdown(&thread_pool);
    
    /* Flush all pending history writes */
    printf("Flushing chat history...\n");
    history_manager_flush(&history_manager);
    
    /* Cleanup resources */
    printf("Cleaning up resources...\n");
    free(connections);
    history_manager_destroy(&history_manager);
    message_router_destroy(&message_router);
    auth_manager_destroy(&auth_manager);
    epoll_manager_destroy(&epoll_manager);
    thread_pool_destroy(&thread_pool);
    task_queue_destroy(&task_queue);
    signal_handler_stop(&signal_handler);
    
    printf("Server shutdown complete\n");
    
    return (ret == ERR_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
