#pragma once

#include <serialization/socket_rpc_server.hpp>
#include <serialization/type_registry.hpp>
#include <serialization/type_description.hpp>


namespace waos {
  namespace rpc_registration {

    extern serialization::type_registry registry;

    template <typename T>
    void register_rpc(serialization::socket_rpc_server* server, T& t, bool register_type = true) {
      #ifdef RPC_SERVER
      if(server) {
        server->register_recursive(t);
      }
      #endif

      #ifdef RPC_TYPE_DESCRIPTION
      if(register_type) {
        registry.register_type(serialization::type_description::create(t, registry));
      }
      #endif
    }

    template <typename T>
    void register_exception(serialization::socket_rpc_server* server, T& t, bool register_type = true) {
      #ifdef RPC_TYPE_DESCRIPTION
      if(register_type) {
        registry.register_type(serialization::type_description::create(t, registry));
      }
      #endif

      #ifdef RPC_SERVER
      if(server) {
        server->channel().register_exception_type<T>();
      }
      #endif
    }

    template <typename T>
    void register_type(T& t, bool register_type = true) {
      #ifdef RPC_TYPE_DESCRIPTION
      if(register_type) {
        registry.register_type(serialization::type_description::create(t, registry));
      }
      #endif
    }

    void register_process_events(serialization::socket_rpc_server* server, bool register_type = true);
    void register_all(serialization::socket_rpc_server* server, bool register_type = true);

  }
}