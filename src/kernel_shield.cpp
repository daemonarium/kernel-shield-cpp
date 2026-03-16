#include <serialization/stream_writer.hpp>
#include <serialization/socket_rpc_server.hpp>
#include <serialization/type_registry.hpp>
#include <serialization/type_description.hpp>
#include <serialization/circular_buffer_reader.hpp>
#include <serialization/circular_buffer_writer.hpp>
#include <utils/socket_handlers.hpp>

#include <socket/socket.h>
#include <memory>
#include "kernel_shield_api.hpp"
#include "process_event.hpp"
#include "kernel_client.hpp"
#include "process.hpp"
#include "rpc_registration.hpp"
#include "sockets.hpp"

waos::kernel_shield kernel_shield;
std::unique_ptr<serialization::socket_rpc_server> user_rpc_server_;

size_t user_rpc_description(char** buffer) {
  serialization::type_description::create(serialization::read_annotation(), waos::rpc_registration::registry);
  waos::rpc_registration::register_all(user_rpc_server_.get(), true);
  std::ostringstream output_stream;
  serialization::stream_writer writer(output_stream);  

  writer.register_serialize_typed_function<serialization::type_description>();
  writer.register_serialize_typed_function<serialization::read_annotation>();

  serialize(writer, waos::rpc_registration::registry);
  writer.write_rest();

  *buffer = (char*)malloc(output_stream.str().size());
  memcpy(*buffer, output_stream.str().data(), output_stream.str().size());
  return output_stream.str().size();
}

size_t monitor_rpc_description(char** buffer) {
  serialization::type_description::create(serialization::read_annotation(), waos::rpc_registration::registry);
  waos::rpc_registration::register_process_events(nullptr, true);
  std::ostringstream output_stream;
  serialization::stream_writer writer(output_stream);  

  writer.register_serialize_typed_function<serialization::type_description>();
  writer.register_serialize_typed_function<serialization::read_annotation>();

  serialize(writer, waos::rpc_registration::registry);
  writer.write_rest();

  *buffer = (char*)malloc(output_stream.str().size());
  memcpy(*buffer, output_stream.str().data(), output_stream.str().size());
  return output_stream.str().size();
}

extern "C" size_t rpc_description(std::uint32_t socket_id, char** buffer) {  
  if(socket_id < 4) {
    if(socket_id == 0) { // user requests to kernel shield
      return user_rpc_description(buffer);
    }
    if(socket_id == 3) { // monitor messages
      return monitor_rpc_description(buffer);
    }
    return 0;
  }

  std::uint32_t process_id = socket_id >> 1;
  std::uint32_t process_socket_id = socket_id & 1;

  if(process_socket_id == 0) {  // requests to process and responses from process
    return 0;
  }
  if(process_socket_id == 1) { // requests from process to kernel shield
    return user_rpc_description(buffer);
  }
  return 0;
}


std::unique_ptr<serialization::socket_message_channel> monitor_channel;

extern "C" void waos_socket_data(std::uint32_t socket) {
  socket_handlers::handle_socket_data(socket);
}

extern "C" void waos_socket_output_drain(int socket) {
  socket_handlers::handle_socket_drain(socket);
}

extern "C" void start() {
  sockets_init(8);
  struct socket_options socket_options = {
    .input_buffer_size = 1024,
    .output_buffer_size = 1024,
    .flush_threshold = 1024,
    .data_event_threshold = 1024,
    .drain_event_threshold = 1024,
    .flags = 0,
  };

  waos::kernel_client::start();

  socket_open((std::uint32_t)base_socket_id::MONITOR, &socket_options);
  monitor_channel = std::make_unique<serialization::socket_message_channel>((std::uint32_t)base_socket_id::MONITOR);

  monitor_channel->register_message_type<waos::process_killed_event>();
  monitor_channel->register_message_type<waos::process_error_event>();

  monitor_channel->on_message<waos::process_killed_event>([](waos::process_killed_event &event) {
    auto process_id = event.process;

    if (process_id < waos::processes.size() && waos::processes[process_id] != nullptr) {
      auto proc = waos::processes[process_id];
      proc->finalize();
      if (process_id < waos::processes.size()) {
        waos::processes[process_id] = nullptr;
      }
    }
  });

  monitor_channel->on_message<waos::process_error_event>([](waos::process_error_event &event) {
    auto process_id = event.process;

    if (process_id < waos::processes.size() && waos::processes[process_id] != nullptr) {
      auto proc = waos::processes[process_id];
      proc->finalize();
      if (process_id < waos::processes.size()) {
        waos::processes[process_id] = nullptr;
      }
    }
  });

  socket_handlers::register_socket_handler((std::uint32_t)base_socket_id::MONITOR, []() {
    if (monitor_channel) {
      monitor_channel->handle_data();
    }
  });
  
  socket_open((std::uint32_t)base_socket_id::USER, &socket_options); // open socket used to communicate with the shield from outside
  
  // Initialize socket_rpc_server after socket is opened
  user_rpc_server_ = std::make_unique<serialization::socket_rpc_server>((std::uint32_t)base_socket_id::USER);
  
  // Register all RPC methods after server is initialized
  waos::rpc_registration::register_all(user_rpc_server_.get(), true);
  
  // Register handler for user socket
  auto server_ptr = user_rpc_server_.get();
  socket_handlers::register_socket_handler((std::uint32_t)base_socket_id::USER, [server_ptr]() {
    waos::kernel_shield_request_context.process_id = 0;
    server_ptr->handle_data();
  });
}