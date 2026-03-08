#include "process.hpp"

#include <socket/socket.h>
#include <serialization/promise.hpp>
#include <utils/socket_handlers.hpp>
#include "kernel_client.hpp"
#include "rpc_registration.hpp"
#include "kernel_shield_api.hpp"

#include "sockets.hpp"

namespace waos {

  std::vector<std::shared_ptr<waos::process>> processes;
  
  using serialization::promise;

  std::shared_ptr<serialization::promise<void>> process::initialize() {
    struct socket_options socket_options = {
      .input_buffer_size = 1024,
      .output_buffer_size = 1024,
      .flush_threshold = 1024,
      .data_event_threshold = 1024,
      .drain_event_threshold = 1024,
      .flags = 0,
    };
    // reverse order to avoid multiple resizes
    socket_open(kernel_api_socket_id(), &socket_options);
    socket_open(api_socket_id(), &socket_options);

    // Create socket_rpc_server for kernel_api_socket_id
    rpc_server_ = std::make_unique<serialization::socket_rpc_server>(kernel_api_socket_id());
    
    // Register all RPC methods without registering types (types already registered)
    rpc_registration::register_all(rpc_server_.get(), false);
    
    // Register handler for kernel_api_socket_id
    auto server_ptr = rpc_server_.get();
    auto process_id = id;
    socket_handlers::register_socket_handler(kernel_api_socket_id(), [server_ptr, process_id]() {
      kernel_shield_request_context.process_id = process_id;
      server_ptr->handle_data();
    });

    // Create socket_rpc_client for api_socket_id (for sending requests to process)
    rpc_client_ = std::make_unique<serialization::socket_rpc_client>(api_socket_id());
    
    // Register handler for api_socket_id (for receiving responses from process)
    auto client_ptr = rpc_client_.get();
    socket_handlers::register_socket_handler(api_socket_id(), [client_ptr]() {
      client_ptr->handle_data();
    });

    // 1 is the kernel shield process id
    co_await kernel_client::kernel_rpc->monitor(id, 1, (std::uint32_t)base_socket_id::MONITOR);
    
    co_return;
  }

  void process::finalize() {
    socket_handlers::unregister_socket_handler(kernel_api_socket_id());
    socket_handlers::unregister_socket_handler(api_socket_id());
    socket_close(kernel_api_socket_id());
    socket_close(api_socket_id());
  }

  std::shared_ptr<serialization::promise<binary>> process::request(binary request) {
    // Use socket_rpc_client to send request to process
    // function_id 0 is used for process requests, id is the process_id
    return rpc_client_->template call<binary>(0, request);
  }

}