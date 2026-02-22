#pragma once

#include <serialization/socket_rpc_client.hpp>
#include <serialization/promise.hpp>
#include "kernel_rpc.hpp"

namespace waos {
  namespace kernel_client {

    using rpc_client_type = serialization::socket_rpc_client;

    extern std::shared_ptr<rpc_client_type> rpc_client;
    extern std::shared_ptr<kernel_rpc<rpc_client_type*>> kernel_rpc;

    void start();

  }
}

