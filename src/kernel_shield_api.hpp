#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <serialization/annotations.hpp>
#include <serialization/promise.hpp>
#include "kernel.hpp"
#include "process_event.hpp"

namespace waos {

  using binary = std::vector<std::uint8_t>;

  struct range {
    binary gt_key;
    bool gt_inclusive;
    binary lt_key;
    bool lt_inclusive;
    size_t limit;
    range() {}
  };

  struct kernel_shield_request_context_type {
    kernel::process_id process_id;
  };

  extern kernel_shield_request_context_type kernel_shield_request_context;

  class kernel_shield {
    public:
      kernel_shield() = default;

      using process_id = kernel::process_id;
      using socket_id = kernel::socket_id;
      using direction = kernel::direction;

      /// Kernel API shielded:

      static std::shared_ptr<serialization::promise<process_id>> spawn(binary code, size_t memory_limit, size_t mana_limit);
      static std::shared_ptr<serialization::promise<process_id>> resurect(binary code, binary memory,
                                                                          size_t memory_limit, size_t mana_limit);

      static std::shared_ptr<serialization::promise<process_id>> spawn_isolated(binary code, size_t memory_limit, size_t mana_limit);
      static std::shared_ptr<serialization::promise<process_id>> resurect_isolated(binary code, binary memory,
                                                                                    size_t memory_limit, size_t mana_limit);

      static std::shared_ptr<serialization::promise<void>> register_process(process_id id, process_id spawned_by, bool isolated);
      static std::shared_ptr<serialization::promise<void>> unregister_process(process_id id);
      
      static std::shared_ptr<serialization::promise<binary>> save(process_id id);
      static std::shared_ptr<serialization::promise<void>> kill(process_id id);

      static std::shared_ptr<serialization::promise<void>> bind(process_id process_from, socket_id socket_from, 
                   process_id process_to, socket_id socket_to,
                   direction direction);
      static std::shared_ptr<serialization::promise<void>> unbind(process_id process_from, socket_id socket_from,
                      process_id process_to, socket_id socket_to,
                      direction direction);
      
      static std::shared_ptr<serialization::promise<void>> monitor(process_id target,
                                                            process_id monitor, socket_id monitor_socket);
      static std::shared_ptr<serialization::promise<void>> demonitor(process_id target,
                                                              process_id monitor, socket_id monitor_socket);

      static std::shared_ptr<serialization::promise<size_t>> measure_mana(process_id process, size_t new_mana);

      static std::shared_ptr<serialization::promise<void>> set_mana_limit(process_id process, size_t mana_limit);

      /// Additional API:

      static std::shared_ptr<serialization::promise<binary>> request(process_id process_id, binary request);
  };

  extern kernel_shield kernel_shield_api;

}