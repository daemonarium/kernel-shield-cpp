

#include <serialization/stream_writer.hpp>
#include <serialization/rpc_server.hpp>
#include <serialization/type_registry.hpp>
#include <serialization/type_description.hpp>
#include <serialization/circular_buffer_reader.hpp>
#include <serialization/circular_buffer_writer.hpp>

#include <io/socket.h>
#include "kernel_shield_api.hpp"
#include "process_event.hpp"
#include "kernel_client.hpp"
#include "process.hpp"
#include "process_event.hpp"

using rpc_server_type = serialization::rpc_server<serialization::circular_buffer_reader, serialization::circular_buffer_writer>;
extern rpc_server_type rpc_server;
extern serialization::type_registry registry;

template <typename T>
void register_rpc(T& t) {        
#ifdef RPC_SERVER
  rpc_server.register_recursive(t, rpc_server);
#endif

#ifdef RPC_TYPE_DESCRIPTION
  registry.register_type(serialization::type_description::create(t, registry));  
#endif
}

template <typename T>
void register_type(T& t) {
#ifdef RPC_SERVER
  rpc_server_type::register_recursive(t, rpc_server);
#endif

#ifdef RPC_TYPE_DESCRIPTION
  registry.register_type(serialization::type_description::create(t, registry));  
#endif
}

rpc_server_type rpc_server;
serialization::type_registry registry;

waos::kernel_shield kernel_shield;

void register_all(){
  register_rpc(kernel_shield);
  waos::process_event_pointer eventPointer;
  register_rpc(eventPointer);
  waos::process_killed_event killedEvent;
  register_rpc(killedEvent);
  waos::process_error_event errorEvent;
  register_rpc(errorEvent);
}

extern "C" size_t rpc_description(std::uint32_t socket, char** buffer) {  
  std::uint32_t process_id = socket >> 2;
  std::uint32_t process_socket_id = socket & 3;

  if(process_socket_id != 0) {
    /// rpc description only for process -> kernel-shield requests
    return 0; 
  }

  serialization::type_description::create(serialization::read_annotation(), registry);
  register_all();
  std::ostringstream output_stream;
  serialization::stream_writer writer(output_stream);  

  writer.register_serialization_function<serialization::type_description>();
  writer.register_serialization_function<serialization::read_annotation>();

  serialize(writer, registry);
  writer.write_rest();

  *buffer = (char*)malloc(output_stream.str().size());
  memcpy(*buffer, output_stream.str().data(), output_stream.str().size());
  return output_stream.str().size();
}

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

void handle_process_response_data(size_t process_id, std::uint32_t socket_id) {
  struct socket_info* socket = get_socket_info(socket_id);
  uint32_t req_len = 0;
  if (circular_buffer_readable_bytes(&socket->input_buffer) < sizeof(req_len)) {
    waos_socket_input_drain(socket_id);
    return; // not enough data yet
  }
  circular_buffer_peek(&socket->input_buffer, 0, &req_len, sizeof(req_len));
  size_t available = circular_buffer_readable_bytes(&socket->input_buffer);
  if (available < sizeof(req_len) + req_len) {
    circular_buffer_reserve_for_reading(&socket->input_buffer, req_len + sizeof(req_len));
    waos_socket_input_drain(socket_id);
    return; // wait for full message
  }

  circular_buffer_skip_read(&socket->input_buffer, sizeof(req_len));

  uint64_t request_id = 0;
  size_t request_id_size = circular_buffer_read_uvarint(&socket->input_buffer, &request_id);
  size_t response_size = req_len - request_id_size - sizeof(req_len);

  waos::binary response(response_size);
  circular_buffer_read(&socket->input_buffer, response.data(), response_size);

  /// we always neet to read it, but if process is not found, we can ignore it

  if(process_id >= waos::processes.size()) {
    return;
  }
  auto process = waos::processes[process_id];
  if(process == nullptr) {
    return;
  }

  process->handle_response(request_id, response);
  
}

void handle_kernel_shield_request_data(size_t process_id, std::uint32_t socket_id) {
  
  struct socket_info* socket = get_socket_info(socket_id);
  // --- read one length-prefixed request from socket0 input ---
  uint32_t req_len = 0;
  if (circular_buffer_readable_bytes(&socket->input_buffer) < sizeof(req_len)) {
    waos_socket_input_drain(socket_id);
    return; // not enough data yet
  }
  circular_buffer_peek(&socket->input_buffer, 0, &req_len, sizeof(req_len));
  size_t available = circular_buffer_readable_bytes(&socket->input_buffer);
  if (available < sizeof(req_len) + req_len) {
    circular_buffer_reserve_for_reading(&socket->input_buffer, req_len + sizeof(req_len));
    waos_socket_input_drain(socket_id);
    return; // wait for full message
  }

  // consume length prefix
  circular_buffer_skip_read(&socket->input_buffer, sizeof(req_len));
  // create reader at the end of the length prefix
  serialization::circular_buffer_reader reader(&socket->input_buffer, 0);

  uint32_t placeholder = 0;
  circular_buffer_poke(&socket->output_buffer, 0, &placeholder, sizeof(placeholder));
  serialization::circular_buffer_writer writer(&socket->output_buffer, sizeof(placeholder));  

  waos::kernel_shield_request_context.process_id = process_id;

  auto promise = rpc_server.handle_call(reader, writer);
  // skip readed bytes
  circular_buffer_skip_read(&socket->input_buffer, req_len);

  promise->on_resolved([&](void) {        
    writer.write_rest();

    uint32_t payload_len = writer.write_offset_ - sizeof(placeholder);
    circular_buffer_poke(&socket->output_buffer, 0, &payload_len, sizeof(payload_len));
    circular_buffer_commit_write(&socket->output_buffer, writer.write_offset_);    

    waos_socket_flush(socket_id); // always flush after full response
  });

  if(!circular_buffer_readable_bytes(&socket->input_buffer)) {
    waos_socket_input_drain(socket_id);
  }
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

  if(socket < 4) {
    if(socket == 0) { // requests from user to kernel shield
      handle_kernel_shield_request_data(0, socket);
    }
    if(socket == 1) { // responses from kernel
      waos::kernel_client::handle_data();      
    }
    if(socket == 2) { // reserved for future use
      
    }
    if(socket == 3) { // monitor messages
      handle_process_monitor_data();      
    }
    return;
  }

  size_t process_id = socket >> 1;
  process_socket_id process_socket_id = static_cast<enum process_socket_id>(socket & 1);

  switch(process_socket_id) {
    case process_socket_id::PROCESS_API: // responses from processes
        handle_process_response_data(process_id, socket);
      break;
    case process_socket_id::KERNEL_API: // requests from processes to kernel shield
        handle_kernel_shield_request_data(process_id, socket);
      break;
  }

}

extern "C" void waos_socket_output_drain(int socket) {
  if(socket != 0) { // ignore for now
    return;
  }

  /// not handled
}

extern "C" void start() {
  register_all();
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
  signal_reader->register_deserialization_function<waos::process_killed_event>();
  signal_reader->register_deserialization_function<waos::process_error_event>();
  
  socket_open((std::uint32_t)base_socket_id::USER, &socket_options); // open socket used to communicate with the shield from outside

}