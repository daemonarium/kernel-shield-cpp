#include "rpc_registration.hpp"

#include "kernel_shield_api.hpp"
#include "process_event.hpp"


namespace waos {
  namespace rpc_registration {

    serialization::type_registry registry(true);

    void register_process_events(serialization::socket_rpc_server* server, bool register_type) {
      waos::process_killed_event killedEvent;
      register_rpc(server, killedEvent, register_type);
      waos::process_error_event errorEvent;
      register_rpc(server, errorEvent, register_type);
      waos::process_copy_event copyEvent;
      register_rpc(server, copyEvent, register_type);
      waos::process_data_event dataEvent;
      register_rpc(server, dataEvent, register_type);
      waos::process_drain_event drainEvent;
      register_rpc(server, drainEvent, register_type);
      waos::process_save_event saveEvent;
      register_rpc(server, saveEvent, register_type);
      waos::process_memory_event memoryEvent;
      register_rpc(server, memoryEvent, register_type);
      waos::process_socket_state_event socketStateEvent;
      register_rpc(server, socketStateEvent, register_type);
      waos::process_measure_mana_event measureManaEvent;
      register_rpc(server, measureManaEvent, register_type);
      waos::process_set_mana_limit_event setManaLimitEvent;
      register_rpc(server, setManaLimitEvent, register_type);
      waos::process_bind_event bindEvent;
      register_rpc(server, bindEvent, register_type);
      waos::process_unbind_event unbindEvent;
      register_rpc(server, unbindEvent, register_type);
    }

    void register_all(serialization::socket_rpc_server* server, bool register_type) {
      if(server != nullptr) server->register_basic_types();
      register_rpc(server, kernel_shield_api, register_type);
      register_process_events(server, register_type);
      std::runtime_error ex("Runtime error");
      register_exception(server, ex, register_type);
    }

  }
}
