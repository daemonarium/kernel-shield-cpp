#pragma once

#include <serialization/rpc_client.hpp>
#include <serialization/circular_buffer_reader.hpp>
#include <serialization/circular_buffer_writer.hpp>
#include <serialization/promise.hpp>
#include "kernel_rpc.hpp"

namespace waos {
  namespace kernel_client {

    using rpc_client_type = serialization::rpc_client<
      serialization::circular_buffer_reader, 
      serialization::circular_buffer_writer
    >;    

    extern std::shared_ptr<rpc_client_type> rpc_client;
    extern std::shared_ptr<kernel_rpc<rpc_client_type>> kernel_rpc;

    void start();

    void handle_data();

  }
}

