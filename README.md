# Multithreaded Request Server

## Overview
This C-based project implements a high-performance multithreaded server designed to handle client requests with configurable scheduling policies. The server demonstrates advanced concepts in systems programming including thread synchronization, socket programming, and request queue management.

## Key Features
- **Multithreaded Architecture**: Implements a configurable worker pool to process requests concurrently
- **Queue Management**: Supports different scheduling policies (FIFO and SJN) with configurable queue sizes
- **Thread Synchronization**: Uses semaphores for proper thread coordination and data protection
- **Socket Programming**: Implements full TCP/IP socket communication between client and server
- **Performance Metrics**: Tracks and reports response times and system utilization

## Technical Implementation
- Written in C using POSIX threads and system calls
- Thread-safe queue implementation with mutex protection
- Dynamic worker thread creation and management
- Real-time performance monitoring and statistics collection
- Low-level system programming using clone() for thread creation

## Scheduling Policies
- **FIFO (First-In-First-Out)**: Processes requests in the order they arrive
- **SJN (Shortest Job Next)**: Prioritizes shorter requests to minimize average response time

## Architecture
The server consists of:
1. A main thread that handles incoming client connections
2. A configurable number of worker threads that process client requests
3. A shared request queue managed with proper synchronization
4. Performance tracking components that measure system efficiency

## Usage
The server can be run with various parameters:
```
./server_pol -q <queue_size> -w <workers> -p <policy: FIFO | SJN> <port_number>
```

This project demonstrates the core concepts needed for building highly concurrent, efficient communication systems - directly applicable to telecommunications gateway development and network infrastructure applications.
