import { readFile } from 'fs/promises'
import { join } from 'path'
import { fileURLToPath } from 'url'
import { dirname } from 'path'

import { Kernel, ModuleProcess, LocalProcess, Direction } from '@daemonarium/kernel'
import { SocketRpcServer, CodecRegistry, registerDynamicCodecs } from '@daemonarium/rpc'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

async function hash(code) {
  const hashBuffer = await crypto.subtle.digest('SHA-256', code);             
  const hashHex = Array.from(new Uint8Array(hashBuffer)).map(b => b.toString(16).padStart(2, '0')).join('')
  return hashHex
}

async function main() {
  const kernelShieldPath = join(__dirname, '../build-web/kernel-shield.wasm')
  const testModulePath = join(__dirname, '../build-web/kernel-shield/cpp-test/test-module.wasm')
  const mockModulePath = join(__dirname, '../build-web/kernel-shield/cpp-test/mock-module.wasm')

  let [kernelShieldCode, testModuleCode, mockModuleCode] = await Promise.all([
    readFile(kernelShieldPath),
    readFile(testModulePath),
    readFile(mockModulePath),
  ])

  /* mockModuleCode = new Uint8Array(new Array(1024).fill(0).map((_, i) => i % 256))
  console.log("mock module:")
  for(let i = 0; i < mockModuleCode.length; i+=16) {        
    console.log(`  ${i.toString(16).padStart(4, '0')}:`, Array.from(mockModuleCode.subarray(i, i + 16)).map(b => b.toString(16).padStart(2, '0')).join(' '))
  }
  console.log("mock module length:", mockModuleCode.byteLength, "hash:", await hash(mockModuleCode)) */

  const kernel = new Kernel()

  // Bridge process (LocalProcess, ID 0) for kernel RPC
  const bridge = new LocalProcess(kernel, 0, 1000000)

  const socketOptions = {
    inputBufferSize: 1024,
    outputBufferSize: 1024,
    flushThreshold: 512,
    dataEventThreshold: 0,
    drainEventThreshold: 4,
    flags: 0,
  }

  // Kernel RPC server on bridge socket 1
  const bridgeSocket1 = bridge.createSocket(1, socketOptions)
  const registry = kernel.createRpcRegistry()
  const codecs = new CodecRegistry()
  registerDynamicCodecs(codecs, registry)
  new SocketRpcServer(bridgeSocket1, registry, codecs)

  // Monitor socket
  bridge.createSocket(3, socketOptions)

  // Kernel-shield (process ID 1)
  const kernelShield = new ModuleProcess(kernel, 1, 2048, 1000000)
  await kernelShield.setModuleCode(kernelShieldCode)
  await kernelShield.start()
  if (kernel.processes.length <= 1) kernel.processes.length = 2
  kernel.processes[1] = kernelShield

  // Bind bridge <-> kernel-shield
  bridge.bind(kernelShield, 1, 1, Direction.BOTH)
  bridge.bind(kernelShield, 3, 3, Direction.BOTH)

  // Test module (process ID 2)
  const testProcess = new ModuleProcess(kernel, 2, 2048, 10000000)
  await testProcess.setModuleCode(testModuleCode)
  await testProcess.start()
  if (kernel.processes.length <= 2) kernel.processes.length = 3
  kernel.processes[2] = testProcess

  // Bind test module <-> kernel-shield
  testProcess.bind(kernelShield, 0, 0, Direction.BOTH)

  // Copy mock WASM code into test module memory and trigger tests
  const malloc = testProcess.instance.exports.malloc
  const setMockCode = testProcess.instance.exports.set_mock_code
  const ptr = malloc(mockModuleCode.length)
  new Uint8Array(testProcess.memory.buffer, ptr, mockModuleCode.length).set(mockModuleCode)
  setMockCode(ptr, mockModuleCode.length)

  // Wait for tests to complete
  await new Promise((resolve) => {
    let polls = 0
    const check = () => {
      polls++
      if (polls > 1000) {
        console.error('timeout after 1000 polls')
        resolve()
        return
      }
      try {
        if (testProcess.instance.exports.is_test_done()) {
          resolve()
          return
        }
      } catch (e) {}
      setTimeout(check, 10)
    }
    setTimeout(check, 50)
  })

  // Read test output
  const bufferPtr = malloc(4)
  const outputSize = testProcess.instance.exports.get_test_output(bufferPtr)

  if (outputSize > 0) {
    const outputPtr = new Uint32Array(testProcess.memory.buffer, bufferPtr, 1)[0]
    const outputBytes = new Uint8Array(testProcess.memory.buffer, outputPtr, outputSize)
    console.log(new TextDecoder().decode(outputBytes))
  } else {
    console.log('No test output received')
  }

  process.exit(outputSize > 0 ? 0 : 1)
}

main().catch((error) => {
  console.error('Test runner failed:', error)
  process.exit(1)
})
