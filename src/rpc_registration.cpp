#include "rpc_registration.hpp"

#include "kernel_shield_api.hpp"
#include "process_event.hpp"


namespace waos {
  namespace rpc_registration {

    serialization::type_registry registry(true);

    void register_process_events(serialization::socket_rpc_server* server, bool register_type) {
      waos::process_event_pointer eventPointer;
      register_rpc(server, eventPointer, register_type);
      waos::process_killed_event killedEvent;
      register_rpc(server, killedEvent, register_type);
      waos::process_error_event errorEvent;
      register_rpc(server, errorEvent, register_type);
    }

    void register_all(serialization::socket_rpc_server* server, bool register_type) {
      register_rpc(server, kernel_shield_api, register_type);
      register_process_events(server, register_type);
    }

  }
}