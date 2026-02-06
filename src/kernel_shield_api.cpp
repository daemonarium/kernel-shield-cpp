#include "kernel_shield_api.hpp"
#include "kernel_client.hpp"
#include <io/socket.h>
#include "process.hpp"

namespace waos {
  kernel_shield_request_context_type kernel_shield_request_context;

  std::shared_ptr<serialization::promise<kernel_shield::process_id>> kernel_shield::spawn(binary code, size_t memory_limit, size_t mana_limit) {
    return kernel_client::kernel_rpc->spawn(code, memory_limit, mana_limit);
  }

  std::shared_ptr<serialization::promise<kernel_shield::process_id>> kernel_shield::resurect(binary code, binary memory,
                                                                                              size_t memory_limit, size_t mana_limit) {
    return kernel_client::kernel_rpc->resurect(code, memory, memory_limit, mana_limit);
  }

  std::shared_ptr<serialization::promise<binary>> kernel_shield::save(kernel_shield::process_id id) {
    return kernel_client::kernel_rpc->save(id);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::kill(kernel_shield::process_id id) {
    return kernel_client::kernel_rpc->kill(id);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::bind(kernel_shield::process_id process_from, kernel_shield::socket_id socket_from, 
                                                                     kernel_shield::process_id process_to, kernel_shield::socket_id socket_to,
                                                                     kernel_shield::direction direction) {
    return kernel_client::kernel_rpc->bind(process_from, socket_from, process_to, socket_to, direction);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::unbind(kernel_shield::process_id process_from, kernel_shield::socket_id socket_from,
                                                                       kernel_shield::process_id process_to, kernel_shield::socket_id socket_to,
                                                                       kernel_shield::direction direction) {
    return kernel_client::kernel_rpc->unbind(process_from, socket_from, process_to, socket_to, direction);
  }
      
  std::shared_ptr<serialization::promise<void>> kernel_shield::monitor(kernel_shield::process_id target,
                                                                        kernel_shield::process_id monitor, kernel_shield::socket_id monitor_socket) {
    return kernel_client::kernel_rpc->monitor(target, monitor, monitor_socket);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::demonitor(kernel_shield::process_id target,
                                                                          kernel_shield::process_id monitor, kernel_shield::socket_id monitor_socket) {
    return kernel_client::kernel_rpc->demonitor(target, monitor, monitor_socket);
  }

  std::shared_ptr<serialization::promise<size_t>> kernel_shield::measure_mana(kernel_shield::process_id process, size_t new_mana) {
    return kernel_client::kernel_rpc->measure_mana(process, new_mana);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::set_mana_limit(size_t mana_limit) {
    return kernel_client::kernel_rpc->set_mana_limit(mana_limit);
  }

  std::shared_ptr<serialization::promise<binary>> kernel_shield::request(kernel_shield::process_id process_id, binary request) {
    if(process_id >= processes.size()) {
      return serialization::promise<binary>::rejected(
        std::make_exception_ptr(std::runtime_error("Process not found"))
      );
    }
    auto process = processes[process_id];
    if(process == nullptr) {
      return serialization::promise<binary>::rejected(std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    return process->request(request);
  }

}