- Requirement: 
+ language : C
+ environment : Ubuntu 24.04.3
+ language for frontend : HTML, CSS, JS
+ build system : Cmake


WebServer
A web server project chat socket and save the history chat in file system. Create login session with secure

Introduction
The WebServer project is a high-performance, Socket-based Chat Server built from scratch in C, specifically engineered to solve the C10K problem. The primary goal is to handle 10,000+ concurrent client connections on a single Ubuntu 24.04 instance with minimal latency and resource footprint.
Instead of traditional blocking I/O models, this server implements an Event-Driven architecture using Linux epoll in Edge-Triggered mode. By combining Non-blocking I/O with a sophisticated Thread Pool, the server ensures that network communication and data processing (such as chat message routing and history persistence) happen asynchronously. This project dives deep into low-level network programming, kernel-level event notification, and thread-safe data structures.
Features
Massive Concurrency Engine: Utilizes Linux epoll() to monitor thousands of active socket connections simultaneously, achieving high throughput with very low CPU overhead.
Non-Blocking Socket Communication: All I/O operations are performed in non-blocking mode, ensuring the server never "stalls" on a slow client or a large data transfer.
High-Performance Thread Pool: A pre-allocated pool of worker threads consumes tasks from a synchronized queue, decoupling network event detection from business logic (chat processing).
Signal Handling & Graceful Shutdown:
Interrupt Handling: Captures signals like SIGINT (Ctrl+C) and SIGTERM to shut down the server cleanly.
Resource Cleanup: Ensures all active sockets are closed, memory is freed, and the thread pool is destroyed properly before exit.
Data Integrity: Guarantees that pending chat logs are flushed and saved to the file system before the process terminates.
Signal Masking: Implements SIGPIPE ignoring to prevent the server from crashing when a client abruptly disconnects.
Real-time Chat & State Management:
Socket-based Messaging: Efficient routing of chat messages between connected clients.
History Persistence: Fast, asynchronous logging of chat history to the Linux file system.
Secure Session Control: Implementation of secure login mechanisms and session tracking for authenticated users.
Edge-Triggered (ET) Handling: Optimized event handling that triggers only when new data arrives, reducing the number of system calls.
Robust Error Handling: Comprehensive management of socket timeouts, broken pipes, and resource limits (file descriptor exhaustion).
