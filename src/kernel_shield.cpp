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


std::unique_ptr<serialization::circular_buffer_reader> signal_reader;

void handle_process_monitor_data() {
  struct socket_info* socket = get_socket_info((std::uint32_t)base_socket_id::MONITOR);
  // --- read one length-prefixed request from socket0 input ---
  uint32_t signal_len = 0;
  if (circular_buffer_readable_bytes(&socket->input_buffer) < sizeof(signal_len)) {
    waos_socket_input_drain((std::uint32_t)base_socket_id::MONITOR);
    return; // not enough data yet
  }
  circular_buffer_peek(&socket->input_buffer, 0, &signal_len, sizeof(signal_len));
  size_t available = circular_buffer_readable_bytes(&socket->input_buffer);
  if (available < sizeof(signal_len) + signal_len) {
    circular_buffer_reserve_for_reading(&socket->input_buffer, signal_len + sizeof(signal_len));
    waos_socket_input_drain((std::uint32_t)base_socket_id::MONITOR);
    return; // wait for full message
  }

  std::shared_ptr<waos::process_event> event_pointer;
  signal_reader->read_typed(event_pointer);
  signal_reader->read_rest();
  if(!event_pointer) return; // no event
  auto process_id = event_pointer->process;
  std::shared_ptr<waos::process_killed_event> killed_event = std::dynamic_pointer_cast<waos::process_killed_event>(event_pointer);
  std::shared_ptr<waos::process_error_event> error_event = std::dynamic_pointer_cast<waos::process_error_event>(event_pointer);
  if(killed_event) {

  }
  if(error_event) {
    
  }
  
  // Finalize the process before removing it
  if(process_id < waos::processes.size() && waos::processes[process_id] != nullptr) {
    auto proc = waos::processes[process_id];
    proc->finalize();
    // Remove the process from the vector after finalization
    if(process_id < waos::processes.size()) {
      waos::processes[process_id] = nullptr;
    }
  }
  // Process not found, resolve anyway
  
}

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
  signal_reader = std::make_unique<serialization::circular_buffer_reader>(
    &get_socket_info((std::uint32_t)base_socket_id::MONITOR)->input_buffer, 0
  );    
  signal_reader->register_deserialize_typed_function<waos::process_killed_event>();
  signal_reader->register_deserialize_typed_function<waos::process_error_event>();
  
  // Register handler for monitor socket
  socket_handlers::register_socket_handler((std::uint32_t)base_socket_id::MONITOR, []() {
    handle_process_monitor_data();
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