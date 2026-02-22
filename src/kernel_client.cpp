#include "kernel_client.hpp"
#include <io/socket.h>
#include <utils/socket_handlers.hpp>

namespace waos {
  namespace kernel_client {

    std::shared_ptr<rpc_client_type> rpc_client;
    std::shared_ptr<waos::kernel_rpc<rpc_client_type*>> kernel_rpc;

    void start() {      
      struct socket_options socket_options = {
        .input_buffer_size = 1024,
        .output_buffer_size = 1024,
        .flush_threshold = 1024,
        .data_event_threshold = 1024,
        .drain_event_threshold = 1024,
        .flags = 0,
      };
      std::uint32_t socket_id = 1;
      socket_open(socket_id, &socket_options);

      rpc_client = std::make_shared<rpc_client_type>(socket_id);
      kernel_rpc = std::make_shared<waos::kernel_rpc<rpc_client_type*>>(rpc_client.get());

      // Register handler for socket 1
      auto client_ptr = rpc_client.get();
      socket_handlers::register_socket_handler(socket_id, [client_ptr]() {
        client_ptr->handle_data();
      });
    }
  }
}