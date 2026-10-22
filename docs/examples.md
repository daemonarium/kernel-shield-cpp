# Example

The host supplies `kernel-shield.wasm`. This package does not fetch
it. Process 1 is the shield. The `LocalProcess` below is the host
side: socket 0 calls the shield, and socket 1 must answer the kernel
RPC or `spawn` has nowhere to go.

```ts
import { readFile } from 'node:fs/promises'
import {
  CodecRegistry,
  SocketRpcClient,
  deserializeTypeRegistry,
  registerDynamicCodecs,
} from '@daemonarium/rpc'
import { Direction, Kernel, LocalProcess, ModuleProcess } from '@daemonarium/kernel'

const kernel = new Kernel()
const shieldBytes = new Uint8Array(await readFile('kernel-shield.wasm'))

const shield = new ModuleProcess(kernel, 1, 2048, 1_000_000)
await shield.setModuleCode(shieldBytes)
await shield.start()
kernel.processes[1] = shield

const host = new LocalProcess(kernel, 0, 1_000_000)
kernel.processes[0] = host
const opts = {
  inputBufferSize: 4096,
  outputBufferSize: 4096,
  flushThreshold: 512,
  dataEventThreshold: 0,
  drainEventThreshold: 4,
  flags: 0,
}
const user = host.createSocket(0, opts)
host.createSocket(1, opts)
host.bind(shield, 0, 0, Direction.BOTH)
host.bind(shield, 1, 1, Direction.BOTH)

const registry = deserializeTypeRegistry(shield.readRawRpcDescription(0))
registry.assignMethodIds()
const codecs = new CodecRegistry()
registerDynamicCodecs(codecs, registry)
const rpc = new SocketRpcClient(user, registry, codecs)

const spawn = registry.getMethods().find((m) => m.memberName === 'kernel_shield.spawn')
const guestBytes = new Uint8Array(await readFile('guest.wasm'))
const pid = await rpc.call(spawn!.methodId, guestBytes, 65536, 1_000_000)
```

`guest.wasm` is any other module the host wants the shield to spawn.
The kernel behind socket 1 is `@daemonarium/kernel` in this snippet's
tests; a later kernel implements the same socket contract and loads
this same `kernel-shield.wasm`.
