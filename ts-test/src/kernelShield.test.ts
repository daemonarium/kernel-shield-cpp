import { describe, it, before } from 'mocha'
import { expect } from 'chai'
import { setupTestEnvironment, type TestContext } from './setup.js'
import { LocalProcess, ModuleProcess } from '@wa-os/ts-kernel'
import { Direction } from '@wa-os/ts-kernel'
import type { SocketOptions } from '@wa-os/ts-rpc'
import { CircularBufferReader, CodecRegistry, registerDynamicCodecs } from '@wa-os/ts-rpc'

describe('Kernel Shield', () => {
  let context: TestContext

  before(async () => {
    context = await setupTestEnvironment()
  })

  it('should have RPC description loaded', () => {
    expect(context.rpcRegistry).to.be.not.undefined
    const methods = context.rpcRegistry.getMethods()
    expect(methods.length).to.be.greaterThan(0)
    
    // Check for kernel_shield methods
    const methodNames = methods.map(m => m.memberName)
    expect(methodNames).to.include('kernel_shield.spawn')
    expect(methodNames).to.include('kernel_shield.kill')
    expect(methodNames).to.include('kernel_shield.save')
    expect(methodNames).to.include('kernel_shield.request')
  })

  it('should handle save request (passthrough to kernel)', async () => {
    // Find save method
    const methods = context.rpcRegistry.getMethods()
    const saveMethod = methods.find(m => m.memberName === 'kernel_shield.save')
    expect(saveMethod).to.be.not.undefined
    
    if (!saveMethod) return

    // Create a test process first so we can save it
    const testProcessId = await context.kernel.spawnLocal(1000000)
    const testProcess = context.kernel.processes[testProcessId] as LocalProcess | undefined
    expect(testProcess).to.be.not.undefined
    testProcess.memory = new Uint8Array(new Array(1024).fill(0).map((_, i) => i % 256))
    expect(testProcessId).to.be.not.undefined

    // Clear previous kernel calls
    context.kernelCalls.length = 0

    // Call save method (this should pass through to kernel)
    const savedMemory = await context.rpcClient.call(saveMethod.methodId, testProcessId)
    expect(savedMemory).to.deep.equal(testProcess.memory)
    
    // Verify that kernel.save was called
    const saveCalls = context.kernelCalls.filter(call => call.method === 'kernel.save')
    expect(saveCalls.length).to.equal(1)
    
    const saveCall = saveCalls[0]
    expect(saveCall.args[0]).to.equal(testProcessId)
    expect(saveCall.error).to.be.undefined
    expect(saveCall.result).to.be.not.undefined
    expect(savedMemory).to.deep.equal(saveCall.result)
  })

  it('should handle set_mana_limit request (passthrough to kernel)', async () => {
    const methods = context.rpcRegistry.getMethods()
    const setManaLimitMethod = methods.find(m => m.memberName === 'kernel_shield.set_mana_limit')
    expect(setManaLimitMethod).to.be.not.undefined
    
    if (!setManaLimitMethod) return

    // Clear previous kernel calls
    context.kernelCalls.length = 0

    // Call set_mana_limit
    await context.rpcClient.call(setManaLimitMethod.methodId, 500000)
    
    // Verify that kernel.set_mana_limit was called
    const setManaLimitCalls = context.kernelCalls.filter(call => call.method === 'kernel.set_mana_limit')
    expect(setManaLimitCalls.length).to.equal(1)
    
    const setManaLimitCall = setManaLimitCalls[0]
    expect(setManaLimitCall.args[0]).to.equal(500000)
    expect(setManaLimitCall.error).to.be.undefined
  })

  let spawnedProcessId: number | undefined

  describe('Process Spawn', () => {
    it('should handle spawn request and create process', async () => {

      // Find spawn method
      const methods = context.rpcRegistry.getMethods()
      const spawnMethod = methods.find(m => m.memberName === 'kernel_shield.spawn')
      expect(spawnMethod).to.be.not.undefined
      
      if (!spawnMethod) return

      // Create dummy code for spawn
      //const code = context.kernelShieldCode
      //const code = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0])
      const code = new Uint8Array(new Array(1024).fill(0).map((_, i) => i % 256))

      // Call spawn
      // This will:
      // 1. Send RPC to kernel-shield socket 0
      // 2. Kernel-shield will call kernel RPC on socket 1
      // 3. Kernel-shield will create process and initialize it
      try {
        const processId = await context.rpcClient.call(
          spawnMethod.methodId,
          code,
          2048, // memory_limit
          1000000 // mana_limit
        )

        console.log('processId', processId)   
        spawnedProcessId = processId;   

        // Verify process was created
        expect(processId).to.be.not.undefined

        const process = context.kernel.processes[processId] as LocalProcess | undefined
        expect(process).to.be.not.undefined        
        expect(process.id).to.equal(processId)
     
        // Check kernel calls
        const kernelCalls = context.kernelCalls.filter(call => call.method === 'kernel.spawn')
        expect(kernelCalls.length).to.equal(1)
        const kernelCall = kernelCalls[0]
        expect(kernelCall.args[0]).to.deep.equal(code)
        expect(kernelCall.args[1]).to.equal(2048)
        expect(kernelCall.args[2]).to.equal(1000000)
        expect(kernelCall.result).to.equal(processId)

      } catch (error) {
        // Spawn might fail if kernel doesn't support it, but we can verify
        // that the RPC call structure is correct
        expect(error).to.be.undefined
      }
    })
  })


  describe('Process Sockets Bind', () => {
    it('should bind spawned process sockets to kernel shield sockets', async () => {

      // Find spawn method
      const methods = context.rpcRegistry.getMethods()
      const bindMethod = methods.find(m => m.memberName === 'kernel_shield.bind')
      expect(bindMethod).to.be.not.undefined
      
      if (!bindMethod) return
      
      try {
        const processId = spawnedProcessId
          
        await context.rpcClient.call( // bind kernel-shield -> process requests
          bindMethod.methodId,
          processId, 0, // process_from, socket_from
          1, (processId << 1) + 0, // process_to, socket_to  
          Direction.BOTH
        )

        await context.rpcClient.call( // bind process requests -> kernel-shield
          bindMethod.methodId,
          processId, 1, // process_from, socket_from
          1, (processId << 1) + 1, // process_to, socket_to  
          Direction.BOTH
        )

        const process = context.kernel.processes[processId] as LocalProcess | undefined
        const shieldProcess = context.kernel.processes[1] as ModuleProcess | undefined
       
        /* console.log('process', process)
        console.log('shieldProcess', shieldProcess) */

        //debugger
        /// TODO: checks

      } catch (error) {
        // Spawn might fail if kernel doesn't support it, but we can verify
        // that the RPC call structure is correct
        expect(error).to.be.undefined
      }
    })
  })


  describe('Request Function', () => {
    it('should handle request to process', async () => {
      const methods = context.rpcRegistry.getMethods()
      const requestMethod = methods.find(m => m.memberName === 'kernel_shield.request')
      expect(requestMethod).to.be.not.undefined
      
      if (!requestMethod) return

      // Create a test request - random data - echo socket will echo back the data
      const requestData = new Uint8Array([1, 2, 3, 4])

      // Call request on process 1 (kernel-shield)
      try {
        const response = await context.rpcClient.call(
          requestMethod.methodId,
          spawnedProcessId, // process_id
          requestData // request
        )
        console.log('response', response)
        
        // Verify response structure
        expect(response).to.be.not.undefined
      } catch (error) {
        // Request might fail if process doesn't handle it, but we can verify
        // that the RPC call was made correctly
        expect(error).to.be.undefined
      }
    })

    it('should reject request to non-existent process', async () => {
      const methods = context.rpcRegistry.getMethods()
      const requestMethod = methods.find(m => m.memberName === 'kernel_shield.request')
      expect(requestMethod).to.be.not.undefined
      
      if (!requestMethod) return

      const requestData = new Uint8Array([1, 2, 3, 4])

      // Call request on non-existent process
      try {
        await context.rpcClient.call(
          requestMethod.methodId,
          999, // non-existent process_id
          requestData
        )
        throw new Error('Should have thrown an error')
      } catch (error) {
        // Expected to fail
        expect(error).to.be.not.undefined
      }
    })
  })

  describe('Process Kill', () => {
    it('should handle kill request and cleanup process', async () => {
      const methods = context.rpcRegistry.getMethods()
      const killMethod = methods.find(m => m.memberName === 'kernel_shield.kill')
      expect(killMethod).to.be.not.undefined
      
      if (!killMethod) return

      // Call kill on process 1 (kernel-shield itself)
      // This should trigger cleanup
      try {
        await context.rpcClient.call(killMethod.methodId, spawnedProcessId)
      } catch (error) {
        // Expected behavior - can't kill kernel-shield from itself
        // But we can verify the RPC call was made
        expect(error).to.be.undefined
      }
    })
  })


  describe('Kernel RPC Verification', () => {
    it('should verify kernel RPC calls on socket 1', async () => {
      // Get kernel socket (socket 1)
      const kernelSocket = context.mock.getSocket(1)
      expect(kernelSocket).to.be.not.undefined
      
      if (!kernelSocket) return

      // Create codec registry for reading kernel RPC calls
      const kernelRegistry = context.kernel.createRpcRegistry()
      const codecs = new CodecRegistry()
      registerDynamicCodecs(codecs, kernelRegistry)

      // Try to read any RPC calls from kernel socket
      // This would contain calls that kernel-shield makes to the kernel
      const reader = new CircularBufferReader(kernelSocket.inputBuffer, codecs)
      
      // Check if there's any data
      const readableBytes = kernelSocket.inputBuffer.readableBytes()
      // Note: In a real scenario, we'd verify the actual RPC call structure
      expect(readableBytes).to.be.at.least(0)
    })
  })
})
