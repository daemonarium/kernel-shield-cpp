#include "kernel_client.hpp"
#include <io/socket.h>

extern "C" __attribute__((import_module("waos"), import_name("waos_socket_flush")))
void waos_socket_flush(std::uint32_t socket);

namespace waos {
  namespace kernel_client {

    std::shared_ptr<rpc_client_type> rpc_client;
    std::shared_ptr<waos::kernel_rpc<rpc_client_type>> kernel_rpc;

    std::unique_ptr<serialization::circular_buffer_reader> reader;
    std::unique_ptr<serialization::circular_buffer_writer> writer;

    struct socket_info* socket1;

    void start() {      
      struct socket_options socket_options = {
        .input_buffer_size = 1024,
        .output_buffer_size = 1024,
        .flush_threshold = 1024,
        .data_event_threshold = 1024,
        .drain_event_threshold = 1024,
        .flags = 0,
      };
      socket_open(1, &socket_options);
      socket1 = get_socket_info(1);
      reader = std::make_unique<serialization::circular_buffer_reader>(&socket1->input_buffer, 0);
      writer = std::make_unique<serialization::circular_buffer_writer>(&socket1->output_buffer, 0);

      rpc_client = std::make_shared<rpc_client_type>(*writer, []() {
        uint32_t placeholder = 0;
        circular_buffer_poke(&socket1->output_buffer, 0, &placeholder, sizeof(placeholder));
        writer->write_offset_ = sizeof(placeholder);        
      },[]() {
        writer->write_rest();
        uint32_t payload_len = writer->write_offset_ - sizeof(payload_len);
        circular_buffer_poke(&socket1->output_buffer, 0, &payload_len, sizeof(payload_len));
        circular_buffer_commit_write(&socket1->output_buffer, writer->write_offset_);
        waos_socket_flush(1);
      });

      kernel_rpc = std::make_shared<waos::kernel_rpc<rpc_client_type>>(rpc_client);
    }

    void handle_data() {      
      rpc_client->handle_response(*reader);
    }
  }
}