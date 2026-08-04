# Kernel Shield

## Overview

Kernel Shield is a middleware module that acts as an intermediary layer between user processes and the kernel in the WAOS (WebAssembly Operating System) architecture. It provides enhanced process management, lifecycle handling, and additional communication capabilities beyond the basic kernel API.

## Purpose

The Kernel Shield module serves several key purposes:

1. **Request Multiplexing**: Kernel Shield is essential because the micro-kernel has only a single socket for receiving requests. Kernel Shield aggregates requests from multiple processes and forwards them to the kernel over this single connection, then distributes responses back to the appropriate processes. This multiplexing capability allows multiple processes to communicate with the kernel simultaneously through Kernel Shield's single connection.

2. **Process Lifecycle Management**: Automatically handles process initialization and finalization when processes are spawned or killed
3. **Process State Tracking**: Maintains a registry of all active processes and their metadata
4. **Enhanced Communication**: Provides additional communication channels between processes and the kernel
5. **Request Routing**: Routes requests between processes and handles responses
6. **Event Handling**: Manages process lifecycle events (killed, error) and cleanup

## Architecture

Kernel Shield operates as a separate process (process ID 1) that intercepts and enhances kernel operations. It maintains:

- A vector of process objects tracking all spawned processes
- Socket connections for each process (API socket and kernel API socket)
- Request/response handlers for inter-process communication
- Monitor event handlers for process lifecycle events

## API Functions

### Kernel API (Shielded)

All kernel functions are wrapped and enhanced:

#### Process Management

- **`spawn(code, memory_limit, mana_limit)`**: Creates a new process with automatic initialization
  - Spawns the process via kernel RPC
  - Creates and stores a process object
  - Automatically initializes the process (opens sockets, sets up monitoring)
  - Returns the process ID

- **`resurect(code, memory, memory_limit, mana_limit)`**: Resurrects a process from saved state
  - Similar to spawn but restores process from saved memory state
  - Automatically initializes the resurrected process

- **`save(id)`**: Saves process state (passed through to kernel)
  - Returns `save_result { snapshot_uid, memory, mana_used, mana_limit }`

- **`kill(id)`**: Terminates a process with automatic cleanup
  - Kills the process via kernel RPC
  - Automatically finalizes the process (closes sockets, cleans up resources)
  - Removes the process from the registry

#### Socket Management

- **`bind(process_from, socket_from, process_to, socket_to, direction)`**: Binds sockets between processes (passed through to kernel)
- **`unbind(process_from, socket_from, process_to, socket_to, direction)`**: Unbinds sockets (passed through to kernel)

#### Monitoring

- **`monitor(target, monitor, monitor_socket, monitor_config)`**: Sets up process monitoring (passed through to kernel)
  - `monitor_config.flags` selects which event kinds are delivered (see kernel `monitor_flag`)
- **`demonitor(target, monitor, monitor_socket)`**: Removes monitoring (passed through to kernel)

#### Resource Management

- **`measure_mana(process, new_mana)`**: Measures and optionally updates process mana (passed through to kernel)
- **`set_mana_limit(mana_limit)`**: Sets global mana limit (passed through to kernel)

### Additional API

#### Process Communication

- **`request(process_id, request)`**: Sends a request to a specific process and waits for response
  - Validates that the process exists
  - Routes the request to the process's API socket
  - Returns a promise that resolves with the process's response
  - Throws an exception if the process is not found

## Socket Architecture

Kernel Shield uses a structured socket numbering scheme:

### Base Sockets (0-3)
- **Socket 0 (USER)**: External requests to kernel shield
- **Socket 1 (KERNEL)**: Communication with the kernel
- **Socket 2 (DEBUG)**: Reserved for debug messages
- **Socket 3 (MONITOR)**: Monitor event messages from kernel

### Process Sockets
For each process (ID = `process_id`):
- **Socket `(process_id << 1) | 0` (PROCESS_API)**: Requests to the process, responses from the process
- **Socket `(process_id << 1) | 1` (KERNEL_API)**: Requests from the process to kernel shield

## Process Lifecycle

### Initialization

When a process is spawned or resurrected:

1. Process is created via kernel RPC
2. Process object is created and stored in the processes vector
3. Sockets are opened (API socket and kernel API socket)
4. Process is automatically monitored by kernel shield (process ID 1) with `monitor_config` flags `KILLED | ERROR` (lifecycle only)
5. Process is ready to receive requests

### Finalization

When a process is killed:

1. Kill request is sent to kernel
2. Process sockets are closed
3. Process object is removed from the registry
4. All resources are cleaned up

### Event Handling

Kernel Shield listens for process events on the MONITOR socket:

- **`process_killed_event`**: Process was terminated
- **`process_error_event`**: Process encountered an error

Both events trigger automatic finalization and cleanup.

## Request Context

Kernel Shield maintains a request context (`kernel_shield_request_context`) that tracks:

- **`process_id`**: The ID of the process making the current request

This context is used to track which process spawned new processes, enabling proper parent-child relationships.

## Implementation Details

### Coroutines

The module uses C++20 coroutines (`co_await`, `co_return`) for asynchronous operations, providing clean and readable async code:

```cpp
std::shared_ptr<serialization::promise<process_id>> spawn(...) {
  auto process_id = co_await kernel_client::kernel_rpc->spawn(...);
  // ... setup process ...
  co_await proc->initialize();
  co_return process_id;
}
```

### Error Handling

- Processes that don't exist return rejected promises with appropriate error messages
- Exceptions from kernel operations are automatically propagated
- Process finalization errors are handled gracefully

## Usage

Kernel Shield is typically used as a WASM module that runs as process ID 1. It:

1. Initializes socket connections
2. Sets up RPC server for handling requests
3. Registers all API functions and types
4. Starts listening for socket events
5. Routes requests between processes and kernel

The module provides a safer and more feature-rich interface to kernel operations while maintaining compatibility with the underlying kernel API.
