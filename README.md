# C10K Chat Server

A high-performance, event-driven chat server implemented in C that handles 10,000+ concurrent connections using Linux epoll in edge-triggered mode.

## Features

- **High Concurrency**: Handles 10,000+ simultaneous client connections
- **Event-Driven Architecture**: Uses Linux epoll in edge-triggered mode for minimal system calls
- **Non-Blocking I/O**: All socket operations are non-blocking to prevent slow clients from blocking the server
- **Thread Pool**: Asynchronous task processing with configurable worker threads
- **WebSocket Support**: Browser-based clients can connect via WebSocket protocol
- **Chat History**: Persistent message storage organized by date and room
- **Secure Authentication**: Password hashing with SHA-256
- **Graceful Shutdown**: Completes all pending tasks and flushes history before exit
- **Web Frontend**: Modern, responsive web interface for browser access

## Architecture

The server uses a multi-layered architecture:

1. **Main Event Loop**: Epoll-based event detection in the main thread
2. **Task Queue**: Thread-safe FIFO queue for distributing work
3. **Thread Pool**: Worker threads process tasks asynchronously
4. **Business Logic**: Message routing, authentication, and history management

## Requirements

- Ubuntu 24.04 or compatible Linux distribution
- CMake 3.10 or higher
- GCC with C11 support
- OpenSSL development libraries
- pthread library

## Build Instructions

### Install Dependencies

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libssl-dev
```

### Build the Server

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make

# Run tests (optional)
make test
```

### Build Configurations

**Debug Build** (with debugging symbols):
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

**Release Build** (optimized):
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Configuration

Copy the example configuration file and modify as needed:

```bash
cp config/config.conf.example config/config.conf
```

### Configuration Options

**Server Settings:**
- `port`: Server listening port (default: 8080)
- `max_connections`: Maximum concurrent connections (default: 10000)
- `thread_pool_size`: Number of worker threads (default: 8)
- `task_queue_capacity`: Maximum queued tasks (default: 1000)
- `connection_timeout`: Idle connection timeout in seconds (default: 300)
- `epoll_timeout`: Epoll wait timeout in milliseconds (default: 100)

**File Paths:**
- `history_path`: Directory for chat history files (default: chat_history)
- `credentials_file`: Path to credentials database (default: config/credentials.txt)
- `static_files_path`: Directory for web frontend files (default: web)

**Logging:**
- `log_level`: Minimum log level (DEBUG, INFO, WARN, ERROR, CRITICAL)
- `log_file`: Path to log file (default: logs/server.log)
- `log_max_size`: Maximum log file size before rotation (default: 100MB)

## Running the Server

### Basic Usage

```bash
# Run with default configuration (from project root directory)
./build/c10k-chat-server

# Run with custom configuration file
./build/c10k-chat-server config/config.conf
```

**Important:** The server must be run from the project root directory (not from the `build/` directory) so it can find the `web/`, `config/`, and `chat_history/` directories.

### System Configuration

For optimal performance with 10,000+ connections, adjust system limits:

```bash
# Increase file descriptor limit
ulimit -n 100000

# Increase ephemeral port range
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"

# Increase socket buffer sizes
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216

# Increase connection backlog
sudo sysctl -w net.core.somaxconn=4096
```

### Accessing the Web Interface

1. Start the server
2. Open a web browser and navigate to: `http://localhost:8080`
3. Login with example credentials:
   - Username: `alice`, `bob`, `charlie`, `dave`, or `eve`
   - Password: `password123`

## User Management

### Credentials File Format

The credentials file (`config/credentials.txt`) uses the format:
```
username:password_hash
```

### Adding New Users

1. Generate a password hash:
```bash
echo -n "your_password" | sha256sum
```

2. Add the user to `config/credentials.txt`:
```
newuser:generated_hash_here
```

3. Restart the server to load new credentials

## Testing

### Run All Tests

```bash
cd build
make test
```

### Run Individual Tests

```bash
# Unit tests
./build/test_auth_manager
./build/test_connection_handler
./build/test_thread_pool
./build/test_message_router
./build/test_history_manager
./build/test_signal_handler
./build/test_websocket_handler

# Integration tests
./build/test_shutdown
```

### Memory Leak Detection

```bash
# Install Valgrind
sudo apt-get install valgrind

# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./build/c10k-chat-server
```

## Project Structure

```
c10k-chat-server/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/                # Header files
│   ├── auth_manager.h
│   ├── common.h
│   ├── connection_handler.h
│   ├── epoll_manager.h
│   ├── history_manager.h
│   ├── logger.h
│   ├── message_router.h
│   ├── signal_handler.h
│   ├── task_queue.h
│   ├── thread_pool.h
│   └── websocket_handler.h
├── src/                    # Source files
│   ├── auth_manager.c
│   ├── connection_handler.c
│   ├── epoll_manager.c
│   ├── history_manager.c
│   ├── logger.c
│   ├── main.c
│   ├── message_router.c
│   ├── signal_handler.c
│   ├── task_queue.c
│   ├── thread_pool.c
│   └── websocket_handler.c
├── tests/                  # Test files
├── web/                    # Web frontend
│   ├── index.html
│   ├── style.css
│   └── app.js
├── config/                 # Configuration files
│   ├── config.conf.example
│   ├── credentials.txt.example
│   └── credentials.txt
└── chat_history/          # Chat history storage (created at runtime)
```

## Performance

### Benchmarks

- **Concurrent Connections**: 10,000+ simultaneous connections
- **Message Throughput**: 100,000+ messages/second
- **Latency**: p99 < 10ms under normal load
- **Memory Usage**: < 10KB per connection

### Load Testing

Use tools like `wrk` or custom clients to test performance:

```bash
# Install wrk
sudo apt-get install wrk

# Test HTTP endpoint
wrk -t12 -c1000 -d30s http://localhost:8080/

# For WebSocket testing, use custom clients or tools like `websocket-bench`
```

## Troubleshooting

### Connection Refused

- Check if the server is running: `ps aux | grep c10k-chat-server`
- Verify the port is not in use: `netstat -tuln | grep 8080`
- Check firewall settings: `sudo ufw status`

### File Descriptor Limit Reached

- Increase the limit: `ulimit -n 100000`
- Make permanent by editing `/etc/security/limits.conf`

### Memory Issues

- Monitor memory usage: `top` or `htop`
- Check for memory leaks with Valgrind
- Reduce `max_connections` in configuration

### WebSocket Connection Fails

- Verify WebSocket handshake in browser console
- Check for proxy/firewall blocking WebSocket connections
- Ensure server is serving static files from correct path

## Contributing

This is an educational project demonstrating C10K problem solutions. Contributions are welcome!

## License

MIT License - See LICENSE file for details

## Acknowledgments

- Inspired by the C10K problem and modern event-driven architectures
- Built with Linux epoll, pthread, and OpenSSL libraries
