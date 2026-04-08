cmake_minimum_required(VERSION 3.10)
project(c10k-chat-server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Source files
set(SOURCES
    src/main.c
    src/epoll_manager.c
    src/connection_handler.c
    src/thread_pool.c
    src/task_queue.c
    src/message_router.c
    src/history_manager.c
    src/auth_manager.c
    src/signal_handler.c
    src/websocket_handler.c
    src/logger.c
)

# Executable
add_executable(c10k-chat-server ${SOURCES})

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Link libraries
target_link_libraries(c10k-chat-server pthread OpenSSL::Crypto)

# Include directories
target_include_directories(c10k-chat-server PRIVATE include)

# Install
install(TARGETS c10k-chat-server DESTINATION bin)

# Tests
enable_testing()

# Test executables
add_executable(test_auth_manager tests/test_auth_manager.c src/auth_manager.c)
target_include_directories(test_auth_manager PRIVATE include)
target_link_libraries(test_auth_manager pthread OpenSSL::Crypto)
add_test(NAME test_auth_manager COMMAND test_auth_manager)

add_executable(test_connection_handler tests/test_connection_handler.c src/connection_handler.c)
target_include_directories(test_connection_handler PRIVATE include)
target_link_libraries(test_connection_handler pthread)
add_test(NAME test_connection_handler COMMAND test_connection_handler)

add_executable(test_history_manager tests/test_history_manager.c src/history_manager.c)
target_include_directories(test_history_manager PRIVATE include)
target_link_libraries(test_history_manager pthread)
add_test(NAME test_history_manager COMMAND test_history_manager)

add_executable(test_main tests/test_main.c)
target_include_directories(test_main PRIVATE include)
target_link_libraries(test_main pthread)
add_test(NAME test_main COMMAND test_main)

add_executable(test_message_router tests/test_message_router.c src/message_router.c src/task_queue.c)
target_include_directories(test_message_router PRIVATE include)
target_link_libraries(test_message_router pthread)
add_test(NAME test_message_router COMMAND test_message_router)

add_executable(test_signal_handler tests/test_signal_handler.c src/signal_handler.c)
target_include_directories(test_signal_handler PRIVATE include)
target_link_libraries(test_signal_handler pthread)
add_test(NAME test_signal_handler COMMAND test_signal_handler)

add_executable(test_thread_pool tests/test_thread_pool.c src/thread_pool.c src/task_queue.c)
target_include_directories(test_thread_pool PRIVATE include)
target_link_libraries(test_thread_pool pthread)
add_test(NAME test_thread_pool COMMAND test_thread_pool)

add_executable(test_websocket_handler tests/test_websocket_handler.c src/websocket_handler.c src/connection_handler.c)
target_include_directories(test_websocket_handler PRIVATE include)
target_link_libraries(test_websocket_handler pthread OpenSSL::Crypto)
add_test(NAME test_websocket_handler COMMAND test_websocket_handler)

# Shutdown integration test
add_executable(test_shutdown tests/test_shutdown.c 
    src/epoll_manager.c 
    src/connection_handler.c 
    src/thread_pool.c 
    src/task_queue.c 
    src/message_router.c 
    src/history_manager.c 
    src/auth_manager.c 
    src/signal_handler.c)
target_include_directories(test_shutdown PRIVATE include)
target_link_libraries(test_shutdown pthread OpenSSL::Crypto)
add_test(NAME test_shutdown COMMAND test_shutdown)
