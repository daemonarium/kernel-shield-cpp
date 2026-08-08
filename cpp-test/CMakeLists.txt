cmake_minimum_required(VERSION 3.17)
project(kernelShieldCppTest)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WASI_STUBS_FILES ${CMAKE_SOURCE_DIR}/cpp-rpc/src/utils/wasi_stubs.cpp)
set(MODULE_FILES ${CMAKE_SOURCE_DIR}/cpp-rpc/src/utils/module.cpp)
set(WASM_FILES ${CMAKE_SOURCE_DIR}/cpp-rpc/src/utils/socket_handlers.cpp)

aux_source_directory(${CMAKE_SOURCE_DIR}/cpp-rpc/src/io IO_FILES)
aux_source_directory(${CMAKE_SOURCE_DIR}/cpp-rpc/src/socket SOCKET_FILES)

set(COMMON_LINK_OPTIONS
  -s STANDALONE_WASM=1
  -fwasm-exceptions
  -mbulk-memory
  -Wl,--import-memory
  --no-entry
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=640kB
  -sTOTAL_STACK=20000
  -sERROR_ON_UNDEFINED_SYMBOLS=0
  -Wl,--allow-undefined-file=${CMAKE_SOURCE_DIR}/cpp-rpc/src/examples/modules/allowed-undefined-symbols.syms
)

if(WASM)

  # Mock module - echo server (same stubs/link shape as cpp-rpc example modules so
  # rpc_description can serialize via ostringstream without trapping).
  add_executable(mock-module src/mock_module.cpp ${MODULE_FILES} ${WASM_FILES} ${IO_FILES} ${SOCKET_FILES} ${WASI_STUBS_FILES})
  target_compile_options(mock-module PRIVATE
    -fwasm-exceptions
  )
  target_link_options(mock-module PRIVATE
    ${COMMON_LINK_OPTIONS}
    -sEXPORTED_FUNCTIONS=_rpc_description,_start,_malloc,_waos_socket_data,_waos_socket_output_drain
  )
  target_include_directories(mock-module PRIVATE src ${CMAKE_SOURCE_DIR}/cpp-rpc/src)
  target_compile_definitions(mock-module PRIVATE RPC_SERVER RPC_TYPE_DESCRIPTION)
  set_target_properties(mock-module PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kernel-shield/cpp-test
    SUFFIX ".wasm"
  )

  # Test module - runs tests against kernel-shield
  add_executable(test-module src/test_module.cpp ${WASM_FILES} ${IO_FILES} ${SOCKET_FILES})
  target_compile_options(test-module PRIVATE
    -fwasm-exceptions
  )
  target_link_options(test-module PRIVATE
    ${COMMON_LINK_OPTIONS}
    -sEXPORTED_FUNCTIONS=_rpc_description,_start,_malloc,_waos_socket_data,_waos_socket_output_drain,_is_test_done,_get_test_output,_set_mock_code
  )
  target_include_directories(test-module PRIVATE src ${CMAKE_SOURCE_DIR}/cpp-rpc/src)
  target_compile_definitions(test-module PRIVATE RPC_SERVER)
  set_target_properties(test-module PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kernel-shield/cpp-test
    SUFFIX ".wasm"
  )

endif()
