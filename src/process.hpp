#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <serialization/annotations.hpp>
#include <serialization/promise.hpp>
#include <serialization/circular_buffer_reader.hpp>
#include "kernel.hpp"

namespace waos {

  class process {
    public:
      kernel::process_id id;
      kernel::process_id spawned_by;
      std::uint32_t last_request_id = 0;

      std::unique_ptr<serialization::circular_buffer_reader> signal_reader;

      std::unordered_map<std::uint32_t, std::function<void(binary&)>> response_handlers;

      process(kernel::process_id id, kernel::process_id spawned_by) 
        : id(id), spawned_by(spawned_by), last_request_id(0) {}
      ~process() {}

      std::shared_ptr<serialization::promise<void>> initialize();
      void finalize();

      kernel::socket_id api_socket_id() const {
        return (id << 1) | 0; // socket 0 is always used for requests to process
      }
      kernel::socket_id kernel_api_socket_id() const {
        return (id << 1) | 1; // socket 1 is always used for requests to kernel
      }

      std::shared_ptr<serialization::promise<binary>> request(binary request);

      void handle_response(std::uint32_t request_id, binary& response);

      void read_and_handle_monitor_signal(struct socket_info* socket, size_t signal_len);
  };

  extern std::vector<std::shared_ptr<waos::process>> processes;
}