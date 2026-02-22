#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <serialization/annotations.hpp>
#include <serialization/promise.hpp>
#include <serialization/circular_buffer_reader.hpp>
#include <serialization/socket_rpc_server.hpp>
#include <serialization/socket_rpc_client.hpp>
#include "kernel.hpp"

namespace waos {

  class process {
    public:
      kernel::process_id id;
      kernel::process_id spawned_by;
      std::uint32_t last_request_id = 0;

      std::unique_ptr<serialization::circular_buffer_reader> signal_reader;

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

      serialization::socket_rpc_server* get_rpc_server() const {
        return rpc_server_.get();
      }

      std::shared_ptr<serialization::promise<binary>> request(binary request);

      void read_and_handle_monitor_signal(struct socket_info* socket, size_t signal_len);

    private:
      std::unique_ptr<serialization::socket_rpc_server> rpc_server_;
      std::unique_ptr<serialization::socket_rpc_client> rpc_client_;
  };

  extern std::vector<std::shared_ptr<waos::process>> processes;
}