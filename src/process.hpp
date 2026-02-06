#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <serialization/annotations.hpp>
#include <serialization/promise.hpp>
#include "serialization/circular_buffer_reader.hpp"
#include "kernel.hpp"

namespace waos {

  class process {
    public:
      kernel::process_id id;
      kernel::process_id spawned_by;
      std::uint32_t last_request_id = 0;

      std::unordered_map<std::uint32_t, std::function<void(binary&)>> response_handlers;

      process(kernel::process_id id, kernel::process_id spawned_by) : id(id), spawned_by(spawned_by), last_request_id(0) {}
      ~process() {}

      std::shared_ptr<serialization::promise<void>> initialize();
      std::shared_ptr<serialization::promise<void>> finalize();

      kernel::socket_id api_socket_id() const {
        return (id << 2) | 0; // socket 0 is always used for requests to process
      }
      kernel::socket_id kernel_api_socket_id() const {
        return (id << 2) | 1; // socket 1 is always used for requests to kernel
      }
      kernel::socket_id debug_socket_id() const {
        return (id << 2) | 2; // socket 1 is always used for debug messages to process
      }
      kernel::socket_id monitor_socket_id() const {
        return (id << 2) | 3; // socket 3 is always used for monitor messages to process
      }

      std::shared_ptr<serialization::promise<binary>> request(binary request);

      void handle_response(std::uint32_t request_id, binary& response);
  };

  extern std::vector<std::shared_ptr<waos::process>> processes;
}