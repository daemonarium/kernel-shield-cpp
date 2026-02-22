#pragma once 

#include <cstdint>


extern "C" __attribute__((import_module("waos"), import_name("waos_socket_flush")))
void waos_socket_flush(std::uint32_t socket);

extern "C" __attribute__((import_module("waos"), import_name("waos_socket_input_drain")))
void waos_socket_input_drain(std::uint32_t socket);

enum class base_socket_id {
  USER = 0, // socket 0 is always used for user requests to kernel shield
  KERNEL = 1, // socket 1 is always used for requests to kernel and responses from kernel
  DEBUG = 2, // socket 2 is always used for debug messages to process
  MONITOR = 3, // socket 3 is always used for monitor messages to process
};

enum class process_socket_id {
  PROCESS_API = 0,
  KERNEL_API = 1
};

