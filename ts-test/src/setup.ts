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
  SocketRpcServer,
  TypeRegistry,
  CodecRegistry,
  registerDynamicCodecs,
  deserializeTypeRegistry,
  LocalSocket,
} from '@wa-os/ts-rpc'
import type { SocketOptions } from '@wa-os/ts-rpc'
import {
  createGeneratedTypeRegistry,
  direction_15,
  type direction_15 as Direction15,
} from '@wa-os/ts-kernel/src/kernelApi.js'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

export interface MockKernelCall {
  method: string
  args: any[]
  result?: any
  error?: Error
}

export interface TestContext {
  kernel: Kernel
  kernelShield: ModuleProcess
  kernelShieldCode: Uint8Array,
  mock: LocalProcess
  rpcClient: SocketRpcClient
  rpcRegistry: TypeRegistry
  kernelRpcServer: SocketRpcServer
  kernelCalls: MockKernelCall[]
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
    inputBufferSize: 2024,
    outputBufferSize: 2024,
    flushThreshold: 512,
    dataEventThreshold: 0,
    drainEventThreshold: 4,
    flags: 0,
  }

  // Create sockets in mock for intercepting kernel-shield communication
  // Socket 0: USER - external requests to kernel-shield
  const mockSocket0 = mock.createSocket(0, socketOptions)
  // Socket 1: KERNEL - kernel communication
  const mockSocket1 = mock.createSocket(1, socketOptions)
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

  // Create mock kernel RPC server on socket 1
  const kernelCalls: MockKernelCall[] = []
  const kernelRegistryResult = createGeneratedTypeRegistry()
  const { registry: kernelRegistry, types: kernelTypes } = kernelRegistryResult

  // Set up mock implementations
  kernelTypes['kernel_spawn_10'].implementation = async (
    code: Uint8Array,
    memory_limit: number,
    mana_limit: number
  ): Promise<number> => {
    const call: MockKernelCall = {
      method: 'kernel.spawn',
      args: [code, memory_limit, mana_limit],
    }
    kernelCalls.push(call)
    //const processId = await kernel.spawn(code, memory_limit, mana_limit)
    const processId = await kernel.spawnLocal(mana_limit)
    const process = kernel.processes[processId] as LocalProcess | undefined
    /// Create echo - everything process receives on socket 0 will be echoed back on socket 0
    const requestFromShieldSocket = process.createSocket(0, {
      inputBufferSize: 1024,
      outputBufferSize: 1024,
      flushThreshold: 512,
      dataEventThreshold: 0,
      drainEventThreshold: 4,
      flags: 0,
    })
    requestFromShieldSocket.dataEventCallback = (dataSize: number) => {
      while(true) {
        if(requestFromShieldSocket.inputBuffer.readableBytes() < 4) {
          return requestFromShieldSocket.drain()
        }
        const lengthArray = new Uint32Array(1)
        const lengthBytes = new Uint8Array(lengthArray.buffer)
        requestFromShieldSocket.inputBuffer.peek(0, lengthBytes, 4)
        const length = lengthArray[0]
        if(requestFromShieldSocket.inputBuffer.readableBytes() < length + 4) {
          return requestFromShieldSocket.drain()
        }

        requestFromShieldSocket.inputBuffer.skipRead(4)

        const { value: requestId, bytesRead: requestIdBytes } = requestFromShieldSocket.inputBuffer.readUVarInt()
        const { value: functionId, bytesRead: functionIdBytes } = requestFromShieldSocket.inputBuffer.readUVarInt()        

        if(functionId !== 0) {
          throw new Error('Invalid function id')
        }

        const { value: requestLength, bytesRead: requestLengthBytes } = requestFromShieldSocket.inputBuffer.readUVarInt()

        if(requestLength != (length - requestIdBytes - functionIdBytes - requestLengthBytes)) {
          throw new Error('Invalid request length - message corrupted')
        }

        const data = new Uint8Array(requestLength)
        requestFromShieldSocket.inputBuffer.read(data, requestLength)
         
        requestFromShieldSocket.outputBuffer.poke(0, lengthBytes, 4)
        const responseIdBytes = requestFromShieldSocket.outputBuffer.pokeUVarInt(4, requestId)        
        requestFromShieldSocket.outputBuffer.poke(4 + responseIdBytes, new Uint8Array([1]), 1) // success
        const responseLengthBytes = requestFromShieldSocket.outputBuffer.pokeUVarInt(4 + responseIdBytes + 1, requestLength)
        requestFromShieldSocket.outputBuffer.poke(4 + responseIdBytes + 1 + responseLengthBytes, data, requestLength)
        lengthArray[0] = responseIdBytes + 1 + responseLengthBytes + requestLength
        requestFromShieldSocket.outputBuffer.poke(0, lengthBytes, 4)
        requestFromShieldSocket.commitWrite(4 + responseIdBytes + 1 + responseLengthBytes + requestLength)
        requestFromShieldSocket.flush()
      }


      debugger
      /// copy from input buffer to output buffer
      const copiedBytes = requestFromShieldSocket.inputBuffer.copyTo(requestFromShieldSocket.outputBuffer)
      if (copiedBytes > 0) {
        /// handle the data
        requestFromShieldSocket.flush()
      }
    }

    const requestToShieldSocket = process.createSocket(1, {
      inputBufferSize: 1024,
      outputBufferSize: 1024,
      flushThreshold: 512,
      dataEventThreshold: 0,
      drainEventThreshold: 4,
      flags: 0,
    })

    call.result = processId
    return processId
  }

  kernelTypes['kernel_resurect_11'].implementation = async (
    code: Uint8Array,
    memory: Uint8Array,
    memory_limit: number,
    mana_limit: number
  ): Promise<number> => {
    const call: MockKernelCall = {
      method: 'kernel.resurect',
      args: [code, memory, memory_limit, mana_limit],
    }
    kernelCalls.push(call)
    const processId = await kernel.resurect(code, memory, memory_limit, mana_limit)
    call.result = processId
    return processId
  }

  kernelTypes['kernel_save_12'].implementation = async (
    id: number
  ): Promise<Uint8Array> => {
    const call: MockKernelCall = {
      method: 'kernel.save',
      args: [id],
    }
    kernelCalls.push(call)
    try {
      const result = await kernel.save(id)
      call.result = result
      return result
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_kill_13'].implementation = async (
    id: number
  ): Promise<void> => {
    const call: MockKernelCall = {
      method: 'kernel.kill',
      args: [id],
    }
    kernelCalls.push(call)
    try {
      await kernel.kill(id)
      call.result = undefined
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_bind_14'].implementation = async (
    process_from: number,
    socket_from: number,
    process_to: number,
    socket_to: number,
    direction: Direction15
  ): Promise<void> => {
    const call: MockKernelCall = {
      method: 'kernel.bind',
      args: [process_from, socket_from, process_to, socket_to, direction],
    }
    kernelCalls.push(call)
    try {
      await kernel.bind(process_from, socket_from, process_to, socket_to, direction)
      call.result = undefined
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_unbind_16'].implementation = async (
    process_from: number,
    socket_from: number,
    process_to: number,
    socket_to: number,
    direction: Direction15
  ): Promise<void> => {
    const call: MockKernelCall = {
      method: 'kernel.unbind',
      args: [process_from, socket_from, process_to, socket_to, direction],
    }
    kernelCalls.push(call)
    try {
      await kernel.unbind(process_from, socket_from, process_to, socket_to, direction)
      call.result = undefined
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_monitor_17'].implementation = async (
    target: number,
    monitor: number,
    monitor_socket: number
  ): Promise<void> => {
    const call: MockKernelCall = {
      method: 'kernel.monitor',
      args: [target, monitor, monitor_socket],
    }
    kernelCalls.push(call)
    try {
      await kernel.monitor(target, monitor, monitor_socket)
      call.result = undefined
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_demonitor_18'].implementation = async (
    target: number,
    monitor: number,
    monitor_socket: number
  ): Promise<void> => {
    const call: MockKernelCall = {
      method: 'kernel.demonitor',
      args: [target, monitor, monitor_socket],
    }
    kernelCalls.push(call)
    try {
      await kernel.demonitor(target, monitor, monitor_socket)
      call.result = undefined
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_measure_mana_19'].implementation = async (
    process: number,
    new_mana: number
  ): Promise<number> => {
    const call: MockKernelCall = {
      method: 'kernel.measure_mana',
      args: [process, new_mana],
    }
    kernelCalls.push(call)
    try {
      const result = await kernel.measure_mana(process, new_mana)
      call.result = result
      return result
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelTypes['kernel_set_mana_limit_20'].implementation = async (
    mana_limit: number
  ): Promise<void> => {
    const call: MockKernelCall = {
      method: 'kernel.set_mana_limit',
      args: [mana_limit],
    }
    kernelCalls.push(call)
    try {
      await kernel.set_mana_limit(mana_limit)
      call.result = undefined
    } catch (error) {
      call.error = error instanceof Error ? error : new Error(String(error))
      throw error
    }
  }

  kernelRegistry.assignMethodIds()

  // Create codec registry for kernel RPC
  const kernelCodecs = new CodecRegistry()
  registerDynamicCodecs(kernelCodecs, kernelRegistry)

  // Create SocketRpcServer for handling kernel RPC calls on socket 1
  const kernelRpcServer = new SocketRpcServer(
    mockSocket1,
    kernelRegistry,
    kernelCodecs
  )

  return {
    kernel,
    kernelShield,
    mock,
    rpcClient,
    rpcRegistry,
    kernelRpcServer,
    kernelCalls,
    kernelShieldCode: wasmCode,
  }
}
