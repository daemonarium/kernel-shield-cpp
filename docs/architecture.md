# Architecture

The kernel RPC this module calls is the header in
[rpc-cpp](https://github.com/daemonarium/rpc-cpp)
`src/examples/structures/kernel.hpp`. Guests do not call that header.
They call the shield API in `src/kernel_shield_api.hpp`. USER and
KERNEL_API sockets use flat message framing (`flat_socket_rpc_*`).
Monitor traffic is flat as well. This module does not use compressed
module RPC.

## Purpose

1. **Request multiplexing.** The kernel has one socket for requests.
   The shield aggregates calls from many processes, forwards them on
   that socket, and returns each response to the caller.
2. **Process lifecycle.** Spawn and kill initialize and finalize the
   child. The kernel itself does not keep this registry.
3. **Process state.** A vector of process objects, including who
   spawned the process and whether it is isolated.
4. **Request routing.** `request(process_id, bytes)` delivers a call
   to another process's API socket.
5. **Death events.** Monitor messages on socket 3 (`process_killed_event`,
   `process_error_event`) finalize the registry entry.

## Where it runs

The shield is process 1. It keeps:

- a vector of process objects
- two sockets per process (API socket and kernel-API socket)
- request and response handlers
- monitor handlers for lifecycle events

## Shielded kernel API

These calls go through to the kernel, with the registry work added
around them.

### Process management

- `spawn(code, memory_limit, mana_limit)` spawns via the kernel,
  stores a process object, initializes it (opens sockets, monitors
  it), and returns the process id.
- `resurect(code, memory, memory_limit, mana_limit)` restores a saved
  memory image and then initializes the same way. The spelling is the
  wire name.
- `spawn_isolated` and `resurect_isolated` are the same calls with
  the isolated bit set. An isolated process accepts `save`, `bind`,
  `list_socket_bindings`, `get_rpc_description`, and `request` only
  from its spawner.
- `register_process(id, spawned_by, isolated)` and
  `unregister_process(id)` let the host (caller process 0) insert or
  remove a process the kernel already created. Any other caller is
  rejected.
- `save(id)` passes through and returns
  `save_result { snapshot_uid, memory, mana_used, mana_limit }`.
  The process must be registered. An isolated process answers only
  its spawner.
- `kill(id)` kills via the kernel, closes the process sockets, and
  drops the registry entry.

### Socket management

- `bind(process_from, socket_from, process_to, socket_to, direction)`
- `unbind(process_from, socket_from, process_to, socket_to, direction)`

Both pass through. Isolated endpoints are checked the same way as
`save`.

### Monitoring

- `monitor(target, monitor, monitor_socket, monitor_config)` passes
  through. `monitor_config.flags` selects which events the kernel
  emits.
- `demonitor(target, monitor, monitor_socket)` passes through.

On spawn, the shield itself monitors the child with `KILLED | ERROR`
only.

### Resource management

- `measure_mana(process, new_mana)` passes through.
- `set_mana_limit(process, mana_limit)` passes through. An isolated
  process cannot change its own limit.

### Inspection

- `list_socket_bindings(id)` returns
  `{ socket, peer_process, peer_socket, direction }` rows. Missing
  process throws. An isolated process answers only its spawner.
- `get_rpc_description(id, socket)` returns the type-registry blob
  for that socket, or an empty blob when the process has none. Same
  capability check as `save`.

## Additional API

`request(process_id, request)` sends bytes to that process's API
socket and waits for the response. A missing process rejects the
promise.

## Sockets

Base sockets:

- 0 USER. External requests to the shield.
- 1 KERNEL. Requests to the kernel and responses from it.
- 2 DEBUG. Reserved.
- 3 MONITOR. Monitor events from the kernel.

For process id `N`:

- `(N << 1) | 0` PROCESS_API. Requests to the process, responses back.
- `(N << 1) | 1` KERNEL_API. Requests from the process to the shield.

## Lifecycle

### Initialization

1. The kernel returns a new process id.
2. The shield stores a process object, recording the caller as
   `spawned_by`.
3. It opens the API socket and the kernel-API socket.
4. It monitors the child with `KILLED | ERROR`.
5. The process can take requests.

### Finalization

1. `kill` goes to the kernel.
2. The process sockets close.
3. The object leaves the registry.

### Events

Socket 3 delivers `process_killed_event` and `process_error_event`.
Both finalize the registry entry.

## Request context

`kernel_shield_request_context.process_id` is the caller of the
current shield RPC. `spawn` uses it as `spawned_by`. Isolated checks
compare it with that field. Process 0 is the host.

## Coroutines

The module uses C++20 coroutines for the kernel round-trip:

```cpp
std::shared_ptr<serialization::promise<process_id>> spawn(...) {
  auto process_id = co_await kernel_client::kernel_rpc->spawn(...);
  // ... setup process ...
  co_await proc->initialize();
  co_return process_id;
}
```

A missing process rejects the promise. Exceptions from the kernel
propagate. Finalization errors are caught so one dead child does not
take down process 1.
