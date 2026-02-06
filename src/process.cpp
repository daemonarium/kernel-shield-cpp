#include "process.hpp"

#include <io/socket.h>
#include <serialization/promise.hpp>

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
    socket_open(monitor_socket_id(), &socket_options);
    socket_open(debug_socket_id(), &socket_options);
    socket_open(kernel_api_socket_id(), &socket_options);
    socket_open(api_socket_id(), &socket_options);
    
    return promise<void>::resolved();
  }

  std::shared_ptr<serialization::promise<void>> process::finalize() {
    socket_close(monitor_socket_id());
    socket_close(debug_socket_id());
    socket_close(kernel_api_socket_id());
    socket_close(api_socket_id());
    
    return promise<void>::resolved();
  }

  std::shared_ptr<serialization::promise<binary>> process::request(binary request) {
    auto p = std::make_shared<promise<binary>>();
    std::uint64_t request_id = last_request_id++;
    response_handlers[request_id] = [p, request_id, this](binary& response) {
      p->resolve(response);
    };

    kernel::socket_id socket_id = api_socket_id();
    struct socket_info* socket = get_socket_info(socket_id);

    uint32_t req_len = request.size() + sizeof(request_id);
    
    circular_buffer_poke(&socket->output_buffer, 0, &req_len, sizeof(req_len));
    size_t request_id_size = circular_buffer_poke_uvarint(&socket->output_buffer, 4,  request_id);
    circular_buffer_poke(&socket->output_buffer, sizeof(req_len) + request_id_size, request.data(), req_len);
    circular_buffer_commit_write(&socket->output_buffer, req_len + request_id_size + sizeof(req_len));

    socket_flush(socket_id);

    return p;
  }  

  void process::handle_response(std::uint32_t request_id, binary& response) {
    auto it = response_handlers.find(request_id);
    if(it == response_handlers.end()) {
      return;
    }
    it->second(response);
    response_handlers.erase(it);
  }

}