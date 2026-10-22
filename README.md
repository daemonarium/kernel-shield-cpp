# kernel-shield

A WebAssembly module that sits in front of a Daemonarium kernel as
process 1. Guests talk to this module. This module talks to the
kernel. It is not part of `@daemonarium/kernel`, and it is not a
library you link.

This repository is `daemonarium/kernel-shield-cpp`. The artifact is
`kernel-shield.wasm`.

## Why it is a separate module

The kernel contract is small on purpose: spawn, bind, monitor, mana,
save, and copying bytes on one request socket. That contract is the
RPC in
[rpc-cpp `examples/structures/kernel.hpp`](https://github.com/daemonarium/rpc-cpp/blob/v0.1.0/src/examples/structures/kernel.hpp).

Multiplexing every other process onto that one socket, the process
registry, child initialization, isolated spawn, and `request()` live
here. A later kernel for WAVM, Wasmer, Wasmtime, or WASMEdge
implements only the small contract and loads this same module.
Putting the shield inside the kernel would mean rewriting it for
every virtual machine.

## Build

Stock Clang cannot compile this module. Build the
[reflection-toolchain](https://github.com/daemonarium/reflection-toolchain)
sysroot and point `P2996_COMPILER_SYSROOT` at it.

This module compiles sources from
[rpc-cpp](https://github.com/daemonarium/rpc-cpp) `v0.1.0`:

```
git submodule add https://github.com/daemonarium/rpc-cpp.git extern/rpc-cpp
cd extern/rpc-cpp && git checkout v0.1.0
```

```
cmake -B build-web -S . \
  -DCMAKE_TOOLCHAIN_FILE=$P2996_COMPILER_SYSROOT/toolchain/emscripten-p2996-toolchain.cmake \
  -DWASM=ON
cmake --build build-web
```

`build-web/kernel-shield.wasm` exports `_rpc_description`, `_start`,
`_malloc`, `_waos_socket_data`, and `_waos_socket_output_drain`.

## How a host runs it

The host supplies the Wasm bytes and spawns them as process 1 with
`@daemonarium/kernel`. Bind shield socket 1 to the kernel RPC socket.
Socket 0 is the user API. A guest calls `spawn`, `bind`, and
`request` on socket 0. The shield forwards the kernel calls on
socket 1.

See [docs/examples.md](docs/examples.md).

## Sockets

| Socket | Role |
|--------|------|
| 0 | User API. Guests call the shield here. |
| 1 | The one kernel socket. |
| 2 | Debug. Reserved. |
| 3 | Monitor events from the kernel. |

For each other process id `N`, socket `(N << 1)` is the process API
and socket `(N << 1) | 1` is that process talking back to the shield.

The flag tables, lifecycle, and isolated-process rules are in
[docs/architecture.md](docs/architecture.md).

## License

MIT. Upstream notices are in `NOTICE`. Conduct is in
`CODE_OF_CONDUCT.md`.
