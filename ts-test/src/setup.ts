import { readFile } from 'fs/promises'
import { join } from 'path'
import { fileURLToPath } from 'url'
import { dirname } from 'path'

import { Kernel } from '@wa-os/ts-kernel'
import { ModuleProcess } from '@wa-os/ts-kernel'
import { LocalProcess } from '@wa-os/ts-kernel'
import { Direction } from '@wa-os/ts-kernel'
import {
  SocketRpcClient,
  TypeRegistry,
  CodecRegistry,
  registerDynamicCodecs,
  deserializeTypeRegistry,
  LocalSocket,
} from '@wa-os/ts-rpc'
import type { SocketOptions } from '@wa-os/ts-rpc'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

export interface TestContext {
  kernel: Kernel
  kernelShield: ModuleProcess
  mock: LocalProcess
  rpcClient: SocketRpcClient
  rpcRegistry: TypeRegistry
}

/**
 * Set up test environment with Kernel, kernel-shield ModuleProcess, and mock LocalProcess
 */
export async function setupTestEnvironment(): Promise<TestContext> {
  // Create Kernel instance
  const kernel = new Kernel()

  // Load kernel-shield WASM
  const wasmPath = join(__dirname, '../../../cmake-build-emscripten/kernel-shield.wasm')
  const wasmCode = await readFile(wasmPath)

  // Create kernel-shield ModuleProcess (process ID 1)
  const kernelShield = new ModuleProcess(kernel, 1, 2048, 1000000)
  await kernelShield.setModuleCode(wasmCode)
  await kernelShield.start()

  // Manually add kernel-shield to kernel processes array
  if (kernel.processes.length <= 1) {
    kernel.processes.length = 2
  }
  kernel.processes[1] = kernelShield

  // Create LocalProcess mock (process ID 0)
  const mock = new LocalProcess(kernel, 0, 1000000)

  // Socket options
  const socketOptions: SocketOptions = {
    inputBufferSize: 1024,
    outputBufferSize: 1024,
    flushThreshold: 1024,
    dataEventThreshold: 1024,
    drainEventThreshold: 1024,
    flags: 0,
  }

  // Create sockets in mock for intercepting kernel-shield communication
  // Socket 0: USER - external requests to kernel-shield
  const mockSocket0 = mock.createSocket(0, socketOptions)
  // Socket 1: KERNEL - kernel communication
  mock.createSocket(1, socketOptions)
  // Socket 2: DEBUG - debug messages
  mock.createSocket(2, socketOptions)
  // Socket 3: MONITOR - monitor events
  mock.createSocket(3, socketOptions)

  // Bind mock sockets to kernel-shield sockets
  // Mock socket 0 → kernel-shield socket 0 (USER)
  mock.bind(kernelShield, 0, 0, Direction.BOTH)
  // Mock socket 1 → kernel-shield socket 1 (KERNEL)
  //debugger
  mock.bind(kernelShield, 1, 1, Direction.BOTH)
  // Mock socket 2 → kernel-shield socket 2 (RESERVED)
  //mock.bind(kernelShield, 2, 2, Direction.BOTH)
  // Mock socket 3 → kernel-shield socket 3 (MONITOR)
  mock.bind(kernelShield, 3, 3, Direction.BOTH)

  // Get RPC description from kernel-shield (socket 0)
  const rpcDescriptionBytes = kernelShield.readRawRpcDescription(0)
  const rpcRegistry = deserializeTypeRegistry(rpcDescriptionBytes)
  rpcRegistry.assignMethodIds()

  // Set RPC description for mock socket 0
  mock.setRpcDescription(0, rpcRegistry)

  // Create codec registry
  const codecs = new CodecRegistry()
  registerDynamicCodecs(codecs, rpcRegistry)

  // Create SocketRpcClient for sending requests to kernel-shield
  // Use the socket we created earlier (mockSocket0 is already a LocalSocket)
  const rpcClient = new SocketRpcClient(
    mockSocket0, // LocalSocket
    rpcRegistry,
    codecs
  )

  return {
    kernel,
    kernelShield,
    mock,
    rpcClient,
    rpcRegistry,
  }
}
