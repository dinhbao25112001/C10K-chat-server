# Implementation Plan: C10K Chat Server

## Overview

This implementation plan breaks down the C10K Chat Server into discrete, incremental coding tasks. The server is built in C using Linux epoll in edge-triggered mode, non-blocking I/O, and a thread pool architecture to handle 10,000+ concurrent connections. The implementation follows a bottom-up approach: foundational components first (data structures, thread pool, task queue), then core networking (epoll manager, connection handler), then business logic (message routing, authentication, history), and finally integration with WebSocket support and web frontend.

Each task builds on previous work, with checkpoints to validate functionality. Property-based tests are included as optional sub-tasks to verify correctness properties from the design document.

## Tasks

- [x] 1. Set up project structure and build system
  - Create directory structure (include/, src/, tests/, web/, config/)
  - Write CMakeLists.txt with C11 standard, compiler flags (-Wall -Wextra -Werror), and pthread linking
  - Create common.h with shared constants (buffer sizes, limits, error codes)
  - Create placeholder header files for all 9 components
  - Verify build produces c10k-chat-server executable
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6_

- [x] 2. Implement Task Queue with thread-safe operations
  - [x] 2.1 Create task_queue.h and task_queue.c
    - Define task_queue_t structure with mutex and condition variables
    - Implement task_queue_init() with configurable capacity
    - Implement task_queue_enqueue() with blocking when full
    - Implement task_queue_dequeue() with blocking when empty
    - Implement task_queue_dequeue_timeout() for timed waits
    - Implement task_queue_size() and task_queue_destroy()
    - _Requirements: 3.2_
  
  - [ ]* 2.2 Write property test for Task Queue
    - **Property 3: Thread-Safe Queue Consistency**
    - **Validates: Requirements 3.2**
    - Generate random concurrent enqueue/dequeue operations
    - Verify queue size equals enqueues minus dequeues
    - Verify FIFO ordering is preserved
    - Verify no tasks are lost or duplicated

- [x] 3. Implement Thread Pool for asynchronous task processing
  - [x] 3.1 Create thread_pool.h and thread_pool.c
    - Define thread_pool_t structure with worker threads array
    - Define task_t structure with type, data, and handler function pointer
    - Implement thread_pool_init() to create configurable number of worker threads
    - Implement worker thread function that continuously dequeues and processes tasks
    - Implement thread_pool_submit() to enqueue tasks
    - Implement thread_pool_shutdown() to set shutdown flag and wait for workers
    - Implement thread_pool_destroy() to cleanup resources
    - _Requirements: 3.1, 3.3, 3.4_
  
  - [ ]* 3.2 Write property test for Thread Pool
    - **Property 4: Task Distribution Fairness**
    - **Validates: Requirements 3.5**
    - Generate large set of tasks (1000+)
    - Track which worker processes each task
    - Verify distribution variance is within acceptable bounds (no worker > 2x average)
  
  - [ ]* 3.3 Write unit tests for Thread Pool
    - Test initialization with various thread counts
    - Test task submission and execution
    - Test graceful shutdown with queued tasks
    - _Requirements: 3.1, 3.4_

- [x] 4. Checkpoint - Verify thread pool and task queue
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement Signal Handler for graceful shutdown
  - [x] 5.1 Create signal_handler.h and signal_handler.c
    - Define signal_handler_t structure with sigset_t and shutdown flag
    - Implement signal_handler_init() to block SIGINT, SIGTERM, SIGPIPE in all threads
    - Implement signal_handler_start() to create dedicated signal thread using sigwait()
    - Implement signal_handler_should_shutdown() to check atomic shutdown flag
    - Implement signal_handler_stop() to cleanup signal thread
    - Use sig_atomic_t for shutdown flag
    - _Requirements: 4.1, 4.2, 4.3_
  
  - [ ]* 5.2 Write unit tests for Signal Handler
    - Test signal capture and shutdown flag setting
    - Test SIGPIPE is ignored
    - _Requirements: 4.1, 4.2, 4.3_

- [x] 6. Implement Epoll Manager for event-driven I/O
  - [x] 6.1 Create epoll_manager.h and epoll_manager.c
    - Define epoll_manager_t structure with epoll_fd and events array
    - Implement epoll_manager_init() to create epoll fd and allocate events array
    - Implement epoll_manager_add() to add fd with EPOLLET flag for edge-triggered mode
    - Implement epoll_manager_modify() to change events for existing fd
    - Implement epoll_manager_remove() to remove fd from epoll
    - Implement epoll_manager_wait() to call epoll_wait() with timeout
    - Implement epoll_manager_get_event() to retrieve event at index
    - Implement epoll_manager_destroy() to close epoll fd and free events
    - _Requirements: 1.1, 1.5, 8.1_
  
  - [ ]* 6.2 Write unit tests for Epoll Manager
    - Test epoll initialization
    - Test adding/modifying/removing file descriptors
    - Test EPOLLET flag is set
    - _Requirements: 1.1, 8.1_

- [x] 7. Implement Connection Handler for non-blocking socket I/O
  - [x] 7.1 Create connection_handler.h and connection_handler.c
    - Define connection_t structure with socket_fd, buffers, session pointer, state, timestamps
    - Define conn_state_t enum (CONNECTING, AUTHENTICATING, AUTHENTICATED, CLOSING)
    - Implement connection_set_nonblocking() to set O_NONBLOCK flag using fcntl()
    - Implement connection_accept() to accept connection and set non-blocking
    - Implement connection_read() to read until EAGAIN in edge-triggered mode
    - Implement connection_write() to write buffered data until EAGAIN
    - Implement connection_buffer_write() to append data to write buffer
    - Implement connection_close() to close socket and cleanup
    - Implement connection_is_timeout() to check last_activity timestamp
    - _Requirements: 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 2.5, 9.2, 9.7_
  
  - [ ]* 7.2 Write property test for Non-blocking I/O Configuration
    - **Property 1: Non-blocking I/O Configuration**
    - **Validates: Requirements 1.3, 2.1, 2.2**
    - Generate various socket scenarios
    - Verify O_NONBLOCK flag is set on all accepted sockets
  
  - [ ]* 7.3 Write property test for Partial I/O Integrity
    - **Property 2: Partial I/O Integrity**
    - **Validates: Requirements 2.5**
    - Generate random data buffers and simulate partial read/write sequences
    - Verify complete data is transmitted without corruption or loss
  
  - [ ]* 7.4 Write property test for Edge-Triggered Read Completeness
    - **Property 14: Edge-Triggered Read Completeness**
    - **Validates: Requirements 8.2**
    - Mock sockets with varying amounts of available data
    - Verify connection_read() reads repeatedly until EAGAIN
  
  - [ ]* 7.5 Write property test for Edge-Triggered Write Completeness
    - **Property 15: Edge-Triggered Write Completeness**
    - **Validates: Requirements 8.3**
    - Mock sockets with varying buffer sizes
    - Verify connection_write() writes repeatedly until EAGAIN or buffer empty
  
  - [ ]* 7.6 Write property test for Connection Timeout Enforcement
    - **Property 17: Connection Timeout Enforcement**
    - **Validates: Requirements 9.2, 9.7**
    - Generate connections with various last_activity timestamps
    - Verify connection_is_timeout() correctly identifies timed-out connections
  
  - [ ]* 7.7 Write unit tests for Connection Handler
    - Test socket acceptance and non-blocking flag setting
    - Test partial read/write handling
    - Test EAGAIN handling
    - Test error handling for EPIPE, ECONNRESET
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 9.3_

- [x] 8. Checkpoint - Verify core networking components
  - Ensure all tests pass, ask the user if questions arise.

- [x] 9. Implement Authentication Manager for secure login
  - [x] 9.1 Create auth_manager.h and auth_manager.c
    - Define client_session_t structure with username, session_id, timestamps, authenticated flag
    - Define auth_manager_t structure with credentials hash table and sessions hash table
    - Implement auth_manager_init() to load credentials from file
    - Implement password hashing function using SHA-256 or bcrypt
    - Implement auth_manager_authenticate() to validate username/password
    - Implement auth_manager_create_session() to generate unique session_id
    - Implement auth_manager_get_session() to lookup session by socket_fd
    - Implement auth_manager_invalidate_session() to remove session
    - Implement auth_manager_destroy() to cleanup hash tables
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_
  
  - [ ]* 9.2 Write property test for Authentication Requirement
    - **Property 9: Authentication Requirement**
    - **Validates: Requirements 7.1**
    - Generate random connection attempts
    - Verify authentication is required before chat access
  
  - [ ]* 9.3 Write property test for Credential Validation
    - **Property 10: Credential Validation**
    - **Validates: Requirements 7.2**
    - Generate random valid and invalid credentials
    - Verify auth_manager_authenticate() returns correct results
  
  - [ ]* 9.4 Write property test for Session Uniqueness
    - **Property 11: Session Uniqueness and Association**
    - **Validates: Requirements 7.3, 7.4**
    - Generate multiple authentication attempts
    - Verify all session IDs are unique
    - Verify sessions correctly associated with socket fds
  
  - [ ]* 9.5 Write property test for Password Hashing
    - **Property 12: Password Hashing**
    - **Validates: Requirements 7.6**
    - Generate random passwords
    - Verify all stored passwords are hashed (not plaintext)
  
  - [ ]* 9.6 Write property test for Session Invalidation
    - **Property 13: Session Invalidation**
    - **Validates: Requirements 7.7**
    - Generate random disconnections
    - Verify sessions are invalidated and cannot be reused
  
  - [ ]* 9.7 Write unit tests for Auth Manager
    - Test credential loading from file
    - Test authentication with valid/invalid credentials
    - Test session creation and lookup
    - Test session invalidation
    - _Requirements: 7.2, 7.3, 7.4, 7.7_

- [x] 10. Implement Message Router for chat message distribution
  - [x] 10.1 Create message_router.h and message_router.c
    - Define message_t structure with sender, room, content, timestamp
    - Define message_router_t structure with connections array and read-write lock
    - Implement message_router_init() to initialize router
    - Implement message_router_parse() to parse JSON message format
    - Implement message_router_route() to identify recipients in same room
    - Implement message_router_route() to enqueue TASK_MESSAGE_SEND for each recipient
    - Implement message_router_register() to add connection to routing table
    - Implement message_router_unregister() to remove connection
    - Implement message_router_destroy() to cleanup
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6_
  
  - [ ]* 10.2 Write property test for Message Parsing Validity
    - **Property 6: Message Parsing Validity**
    - **Validates: Requirements 5.1**
    - Generate random valid messages conforming to message format
    - Verify message_router_parse() successfully extracts all fields
  
  - [ ]* 10.3 Write property test for Message Routing Completeness
    - **Property 7: Message Routing Completeness**
    - **Validates: Requirements 5.2, 5.3, 5.4**
    - Generate chat rooms with N connected clients
    - Send message to room
    - Verify exactly N send tasks are enqueued
  
  - [ ]* 10.4 Write unit tests for Message Router
    - Test message parsing with valid and invalid formats
    - Test recipient identification
    - Test task enqueueing for message delivery
    - Test connection registration/unregistration
    - _Requirements: 5.1, 5.2, 5.3_

- [x] 11. Implement History Manager for chat persistence
  - [x] 11.1 Create history_manager.h and history_manager.c
    - Define history_manager_t structure with base_path, file_lock, current_file, current_date
    - Implement history_manager_init() to create base directory structure
    - Implement history_manager_write() to append message to date/room-specific log file
    - Implement history_manager_rotate() to check date and rotate log file if needed
    - Implement history_manager_flush() to fsync() current file
    - Implement history_manager_destroy() to close files and cleanup
    - Use newline-delimited JSON format for log entries
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_
  
  - [ ]* 11.2 Write property test for History Persistence Correctness
    - **Property 8: History Persistence Correctness**
    - **Validates: Requirements 6.1, 6.3**
    - Generate random messages with various dates and rooms
    - Verify messages written to correct date-based directory and room-specific file
  
  - [ ]* 11.3 Write unit tests for History Manager
    - Test file path generation for different dates/rooms
    - Test log rotation on date change
    - Test error handling for file I/O failures
    - Test flush operation
    - _Requirements: 6.1, 6.3, 6.4, 6.5_

- [x] 12. Checkpoint - Verify business logic components
  - Ensure all tests pass, ask the user if questions arise.

- [x] 13. Implement WebSocket Handler for browser clients
  - [x] 13.1 Create websocket_handler.h and websocket_handler.c
    - Define websocket_frame_t structure with opcode, fin, masked, payload_length, mask_key, payload
    - Implement websocket_handshake() to validate HTTP Upgrade request
    - Implement websocket_handshake() to compute Sec-WebSocket-Accept using SHA-1
    - Implement websocket_handshake() to send 101 Switching Protocols response
    - Implement websocket_parse_frame() to parse WebSocket frame format
    - Implement websocket_encode_frame() to encode data as WebSocket frame
    - Implement websocket_close() to send close frame with status code
    - Support text frames (opcode 0x1) and close frames (opcode 0x8)
    - _Requirements: 11.2, 11.3_
  
  - [ ]* 13.2 Write property test for WebSocket Handshake
    - **Property 19: WebSocket Handshake**
    - **Validates: Requirements 11.2, 11.3**
    - Generate random valid WebSocket handshake requests
    - Verify websocket_handshake() successfully upgrades connection
  
  - [ ]* 13.3 Write unit tests for WebSocket Handler
    - Test handshake validation and response generation
    - Test frame parsing with various payload sizes
    - Test frame encoding
    - Test masking/unmasking
    - _Requirements: 11.2, 11.3_

- [x] 14. Implement main server loop and integration
  - [x] 14.1 Create main.c with server initialization
    - Define server_config_t structure with all configuration parameters
    - Implement load_config() to parse configuration file
    - Implement main() to initialize all components (signal handler, thread pool, epoll manager, auth manager, message router, history manager)
    - Create listening socket, bind to port, set to non-blocking, listen with backlog
    - Add listening socket to epoll with EPOLLIN
    - _Requirements: 1.2, 1.3, 4.4_
  
  - [x] 14.2 Implement main event loop
    - Loop while !signal_handler_should_shutdown()
    - Call epoll_manager_wait() with timeout
    - For each event, check if listening socket (accept new connection) or client socket (read/write)
    - For listening socket: accept connection, set non-blocking, add to epoll, enqueue TASK_AUTH
    - For client socket EPOLLIN: read until EAGAIN, enqueue TASK_MESSAGE_PROCESS
    - For client socket EPOLLOUT: write buffered data until EAGAIN, remove EPOLLOUT if buffer empty
    - Handle EPOLLERR and EPOLLHUP by closing connection
    - Periodically check for connection timeouts (every 60 seconds)
    - _Requirements: 1.1, 1.2, 1.4, 1.5, 2.1, 2.2, 2.3, 2.4, 8.2, 8.3, 9.2, 9.7_
  
  - [x] 14.3 Implement task handler functions
    - Implement handle_auth_task() to authenticate connection and create session
    - Implement handle_message_process_task() to parse message and call message_router_route()
    - Implement handle_message_send_task() to buffer message for sending and register EPOLLOUT
    - Implement handle_history_write_task() to call history_manager_write()
    - Implement handle_cleanup_task() to close connection and free resources
    - _Requirements: 5.1, 5.2, 5.3, 6.1, 7.1, 7.2_
  
  - [x] 14.4 Implement graceful shutdown sequence
    - When shutdown flag set, stop accepting new connections
    - Call thread_pool_shutdown() to drain task queue
    - Call history_manager_flush() to sync all pending writes
    - Close all active client sockets
    - Call epoll_manager_destroy(), thread_pool_destroy(), auth_manager_destroy(), message_router_destroy(), history_manager_destroy()
    - Free all allocated memory
    - _Requirements: 4.4, 4.5, 4.6, 4.7, 4.8, 4.9_
  
  - [ ]* 14.5 Write property test for Shutdown Completeness
    - **Property 5: Shutdown Completeness**
    - **Validates: Requirements 4.6, 4.7**
    - Generate random server states with active connections and queued tasks
    - Trigger shutdown
    - Verify all connections closed and all tasks completed
  
  - [ ]* 14.6 Write property test for Error Logging Completeness
    - **Property 16: Error Logging Completeness**
    - **Validates: Requirements 9.1**
    - Generate various socket errors (EPIPE, ECONNRESET, etc.)
    - Verify all errors logged with context (socket fd, error code)

- [x] 15. Checkpoint - Verify main server integration
  - Ensure all tests pass, ask the user if questions arise.

- [x] 16. Implement web frontend for browser access
  - [x] 16.1 Create static file serving in main.c
    - Implement serve_static_file() to read file from static_files_path
    - Implement HTTP response generation with appropriate Content-Type headers
    - Support .html, .css, .js file types
    - Handle 404 Not Found for missing files
    - _Requirements: 11.1_
  
  - [ ]* 16.2 Write property test for Static File Serving
    - **Property 18: Static File Serving**
    - **Validates: Requirements 11.1**
    - Generate random valid file requests
    - Verify correct files served with appropriate headers
  
  - [x] 16.3 Create web/index.html
    - Create login screen with username and password inputs
    - Create chat screen with message display area and input form
    - Include elements for current user, current room, logout button
    - _Requirements: 11.4, 11.5, 11.6_
  
  - [x] 16.4 Create web/style.css
    - Style login screen and chat screen
    - Style message display with sender, timestamp, and content
    - Make interface responsive and user-friendly
    - _Requirements: 11.5_
  
  - [x] 16.5 Create web/app.js
    - Implement ChatClient class with WebSocket connection
    - Implement connect() to establish WebSocket connection
    - Implement login() to send authentication message
    - Implement sendMessage() to send chat message
    - Implement handleMessage() to process incoming messages (auth_success, auth_failure, message, history)
    - Implement displayMessage() to append message to chat display
    - Add event listeners for login form and message form
    - _Requirements: 11.2, 11.4, 11.5, 11.6, 11.7_

- [x] 17. Add configuration file support
  - [x] 17.1 Create config/config.conf.example
    - Define configuration sections: [server], [paths], [logging]
    - Include all configurable parameters (port, max_connections, thread_pool_size, timeouts, paths)
    - _Requirements: 10.1_
  
  - [x] 17.2 Implement configuration file parser in main.c
    - Parse INI-style configuration file
    - Populate server_config_t structure
    - Provide sensible defaults for missing values
    - _Requirements: 10.1_

- [x] 18. Add logging infrastructure
  - [x] 18.1 Create logger.h and logger.c
    - Define log levels (DEBUG, INFO, WARN, ERROR, CRITICAL)
    - Implement log_init() to open log file
    - Implement log_message() with level, timestamp, and message
    - Implement log rotation by size
    - Use structured logging format (timestamp, level, component, message)
    - _Requirements: 9.1_
  
  - [x] 18.2 Add logging throughout codebase
    - Log connection accepted/closed events
    - Log authentication attempts and results
    - Log message routing events
    - Log all errors with context
    - Log shutdown sequence
    - _Requirements: 9.1, 9.5_

- [x] 19. Create example credentials database
  - [x] 19.1 Create config/credentials.db.example
    - Create simple text format: username:password_hash per line
    - Include example users with hashed passwords
    - Document password hashing algorithm used
    - _Requirements: 7.2, 7.6_

- [x] 20. Checkpoint - Verify complete system
  - Ensure all tests pass, ask the user if questions arise.

- [ ]* 21. Write integration tests
  - [ ]* 21.1 Create tests/integration/test_e2e.c
    - Test end-to-end message flow with multiple clients
    - Test authentication flow
    - Test message routing between clients in same room
    - Test history persistence and retrieval
    - Test graceful shutdown with active connections
    - _Requirements: 5.4, 6.1, 7.1, 7.2, 11.5, 11.6, 11.7_
  
  - [ ]* 21.2 Create tests/integration/test_load.c
    - Test server with 1000+ concurrent connections
    - Measure message throughput
    - Measure latency percentiles
    - Verify no connection rejections under limit
    - _Requirements: 1.4, 9.4_
  
  - [ ]* 21.3 Create tests/integration/test_websocket.c
    - Test WebSocket handshake from browser client
    - Test bidirectional communication
    - Test message framing
    - _Requirements: 11.2, 11.3_

- [ ]* 22. Add memory leak detection
  - Run all tests under Valgrind
  - Fix any memory leaks detected
  - Verify clean shutdown with no leaks
  - _Requirements: 4.8_

- [x] 23. Create documentation
  - [x] 23.1 Create README.md
    - Document project overview and features
    - Document build instructions
    - Document configuration options
    - Document running the server
    - Document testing instructions
    - _Requirements: 10.1_
  
  - [x] 23.2 Add code comments
    - Document all public functions with purpose, parameters, return values
    - Document complex algorithms and design decisions
    - Document thread safety considerations
    - _Requirements: 10.1_

- [x] 24. Final checkpoint - Complete system validation
  - Build project in release mode
  - Run all unit tests
  - Run all property tests
  - Run integration tests
  - Verify server handles 10,000+ connections
  - Verify graceful shutdown works correctly
  - Verify web frontend connects and functions
  - Ask the user if questions arise or if ready for deployment

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP delivery
- Each task references specific requirements for traceability
- Property tests validate universal correctness properties from the design document
- Integration tests verify system behavior with real components
- Checkpoints ensure incremental validation throughout implementation
- The implementation follows a bottom-up approach: foundational components first, then networking, then business logic, then integration
- All code must compile without warnings (-Wall -Wextra -Werror)
- Thread safety is critical for task queue, message router, auth manager, and history manager
- Edge-triggered epoll requires reading/writing until EAGAIN
- Graceful shutdown must complete all queued tasks and flush history before exit
