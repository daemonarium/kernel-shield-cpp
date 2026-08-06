#include "kernel_shield_api.hpp"
#include "kernel_client.hpp"
#include <socket/socket.h>
#include "process.hpp"

namespace waos {
  kernel_shield_request_context_type kernel_shield_request_context;

  std::shared_ptr<serialization::promise<kernel_shield::process_id>> kernel_shield::spawn(binary code, size_t memory_limit, size_t mana_limit) {
    kernel::process_id spawned_by = kernel_shield_request_context.process_id;

    auto process_id = co_await kernel_client::kernel_rpc->spawn(code, memory_limit, mana_limit);

    // Ensure processes vector is large enough
    if(process_id >= processes.size()) {
      processes.resize(process_id + 1);
    }
    
    // Create and store the process    
    auto proc = std::make_shared<process>(process_id, spawned_by, false);
    processes[process_id] = proc;
    
    // Initialize the process and wait for it to complete
    co_await proc->initialize();
    
    co_return process_id;
  }

  std::shared_ptr<serialization::promise<kernel_shield::process_id>> kernel_shield::resurect(binary code, binary memory,
                                                                                              size_t memory_limit, size_t mana_limit) {
    auto process_id = co_await kernel_client::kernel_rpc->resurect(code, memory, memory_limit, mana_limit);
    
    // Ensure processes vector is large enough
    if(process_id >= processes.size()) {
      processes.resize(process_id + 1);
    }
    
    // Create and store the process
    kernel::process_id spawned_by = kernel_shield_request_context.process_id;
    auto proc = std::make_shared<process>(process_id, spawned_by, false);
    processes[process_id] = proc;
    
    // Initialize the process and wait for it to complete
    co_await proc->initialize();
    
    co_return process_id;
  }

  std::shared_ptr<serialization::promise<kernel_shield::process_id>> kernel_shield::spawn_isolated(binary code, size_t memory_limit, size_t mana_limit) {
    kernel::process_id spawned_by = kernel_shield_request_context.process_id;

    auto process_id = co_await kernel_client::kernel_rpc->spawn(code, memory_limit, mana_limit);

    // Ensure processes vector is large enough
    if(process_id >= processes.size()) {
      processes.resize(process_id + 1);
    }
    
    // Create and store the process    
    auto proc = std::make_shared<process>(process_id, spawned_by, true);
    processes[process_id] = proc;
    
    // Initialize the process and wait for it to complete
    co_await proc->initialize();
    
    co_return process_id;
  }

  std::shared_ptr<serialization::promise<kernel_shield::process_id>> kernel_shield::resurect_isolated(binary code, binary memory,
                                                                                              size_t memory_limit, size_t mana_limit) {
    auto process_id = co_await kernel_client::kernel_rpc->resurect(code, memory, memory_limit, mana_limit);
    
    // Ensure processes vector is large enough
    if(process_id >= processes.size()) {
      processes.resize(process_id + 1);
    }
    
    // Create and store the process
    kernel::process_id spawned_by = kernel_shield_request_context.process_id;
    auto proc = std::make_shared<process>(process_id, spawned_by, true);
    processes[process_id] = proc;
    
    // Initialize the process and wait for it to complete
    co_await proc->initialize();
    
    co_return process_id;
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::register_process(kernel_shield::process_id id, kernel_shield::process_id spawned_by, bool isolated) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    if(called_by != 0) {
      throw std::runtime_error("Only the kernel can register processes");
    }
    if(id >= processes.size()) {
      processes.resize(id + 1);
    }
    processes[id] = std::make_shared<process>(id, spawned_by, isolated);
    co_await processes[id]->initialize();
    co_return;
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::unregister_process(kernel_shield::process_id id) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    if(called_by != 0) {
      throw std::runtime_error("Only the kernel can unregister processes");
    }
    if(id >= processes.size()) {
      processes.resize(id + 1);
    }
    if(processes[id] == nullptr) {  
      throw std::runtime_error("Process not found");
    }
    processes[id]->finalize();
    processes[id] = nullptr;
    co_return;
  } 

  std::shared_ptr<serialization::promise<save_result>> kernel_shield::save(kernel_shield::process_id id) {   
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    std::shared_ptr<waos::process> process_ptr = processes[id];
    if(process_ptr == nullptr) {
      return serialization::promise<save_result>::rejected(std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    if(process_ptr->isolated && process_ptr->spawned_by != called_by) {
      return serialization::promise<save_result>::rejected(std::make_exception_ptr(std::runtime_error("Process is isolated and not spawned by the caller")));
    }
    return kernel_client::kernel_rpc->save(id);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::kill(kernel_shield::process_id id) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    std::shared_ptr<waos::process> process_ptr = processes[id];
    if(process_ptr == nullptr) {
      throw std::runtime_error("Process not found");
    }
    if(process_ptr->isolated && process_ptr->spawned_by != called_by) {
      throw std::runtime_error("Process is isolated and not spawned by the caller");
    }
    co_await kernel_client::kernel_rpc->kill(id);

    // Finalize the process before removing it
    if(id < processes.size() && processes[id] != nullptr) {
      auto proc = processes[id];
      proc->finalize();
      // Remove the process from the vector after finalization
      if(id < processes.size()) {
        processes[id] = nullptr;
      }
    }
    co_return;
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::bind(kernel_shield::process_id process_from, kernel_shield::socket_id socket_from, 
                                                                     kernel_shield::process_id process_to, kernel_shield::socket_id socket_to,
                                                                     kernel_shield::direction direction) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    std::shared_ptr<waos::process> process_from_ptr = processes[process_from];
    std::shared_ptr<waos::process> process_to_ptr = processes[process_to];
    if(process_from != 1 && process_from_ptr == nullptr) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process from not found")));
    }
    if(process_to != 1 && process_to_ptr == nullptr) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process to not found")));
    }
    if(process_from != 1 && process_from_ptr->isolated && process_from_ptr->spawned_by != called_by) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process from is isolated and not spawned by the caller")));
    }
    if(process_to != 1 && process_to_ptr->isolated && process_to_ptr->spawned_by != called_by) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process to is isolated and not spawned by the caller")));
    }
    return kernel_client::kernel_rpc->bind(process_from, socket_from, process_to, socket_to, direction);    
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::unbind(kernel_shield::process_id process_from, kernel_shield::socket_id socket_from,
                                                                       kernel_shield::process_id process_to, kernel_shield::socket_id socket_to,
                                                                       kernel_shield::direction direction) {
                                                                        kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    std::shared_ptr<waos::process> process_from_ptr = processes[process_from];
    std::shared_ptr<waos::process> process_to_ptr = processes[process_to];
    if(process_from != 1 && process_from_ptr == nullptr) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process from not found")));
    }
    if(process_to != 1 && process_to_ptr == nullptr) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process to not found")));
    }
    if(process_from != 1 && process_from_ptr->isolated && process_from_ptr->spawned_by != called_by) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process from is isolated and not spawned by the caller")));
    }
    if(process_to != 1 && process_to_ptr->isolated && process_to_ptr->spawned_by != called_by) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process to is isolated and not spawned by the caller")));
    }
    return kernel_client::kernel_rpc->unbind(process_from, socket_from, process_to, socket_to, direction);
  }
      
  std::shared_ptr<serialization::promise<void>> kernel_shield::monitor(kernel_shield::process_id target,
                                                                        kernel_shield::process_id monitor, kernel_shield::socket_id monitor_socket,
                                                                        monitor_config config) {
    return kernel_client::kernel_rpc->monitor(target, monitor, monitor_socket, config);
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::demonitor(kernel_shield::process_id target,
                                                                          kernel_shield::process_id monitor, kernel_shield::socket_id monitor_socket) {
    return kernel_client::kernel_rpc->demonitor(target, monitor, monitor_socket);
  }

  std::shared_ptr<serialization::promise<size_t>> kernel_shield::measure_mana(kernel_shield::process_id process, size_t new_mana) {
    auto mana = co_await kernel_client::kernel_rpc->measure_mana(process, new_mana);
    co_return mana;
  }

  std::shared_ptr<serialization::promise<void>> kernel_shield::set_mana_limit(kernel_shield::process_id process, size_t mana_limit) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    std::shared_ptr<waos::process> process_ptr = processes[process];
    if(process_ptr == nullptr) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    if(process_ptr->isolated) {
      return serialization::promise<void>::rejected(std::make_exception_ptr(std::runtime_error("Process is isolated and cannot set mana limit")));
    }
    return kernel_client::kernel_rpc->set_mana_limit(process, mana_limit);
  }

  std::shared_ptr<serialization::promise<std::vector<kernel::socket_binding>>> kernel_shield::list_socket_bindings(
      kernel_shield::process_id id) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;
    std::shared_ptr<waos::process> process_ptr = processes[id];
    if(process_ptr == nullptr) {
      return serialization::promise<std::vector<kernel::socket_binding>>::rejected(
        std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    if(process_ptr->isolated && process_ptr->spawned_by != called_by) {
      return serialization::promise<std::vector<kernel::socket_binding>>::rejected(
        std::make_exception_ptr(std::runtime_error("Process is isolated and not spawned by the caller")));
    }
    return kernel_client::kernel_rpc->list_socket_bindings(id);
  }

  std::shared_ptr<serialization::promise<binary>> kernel_shield::get_rpc_description(
      kernel_shield::process_id id, kernel_shield::socket_id socket) {
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;
    std::shared_ptr<waos::process> process_ptr = processes[id];
    if(process_ptr == nullptr) {
      return serialization::promise<binary>::rejected(
        std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    if(process_ptr->isolated && process_ptr->spawned_by != called_by) {
      return serialization::promise<binary>::rejected(
        std::make_exception_ptr(std::runtime_error("Process is isolated and not spawned by the caller")));
    }
    return kernel_client::kernel_rpc->get_rpc_description(id, socket);
  }

  std::shared_ptr<serialization::promise<binary>> kernel_shield::request(kernel_shield::process_id process_id, binary request) {
    if(process_id >= processes.size()) {
      return serialization::promise<binary>::rejected(
        std::make_exception_ptr(std::runtime_error("Process not found"))
      );
    }
    kernel_shield::process_id called_by = kernel_shield_request_context.process_id;  
    std::shared_ptr<waos::process> process_ptr = processes[process_id];
    if(process_ptr == nullptr) {
      return serialization::promise<binary>::rejected(std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    if(process_ptr->isolated && process_ptr->spawned_by != called_by) {
      return serialization::promise<binary>::rejected(std::make_exception_ptr(std::runtime_error("Process is isolated and not spawned by the caller")));
    }
    auto process = processes[process_id];
    if(process == nullptr) {
      return serialization::promise<binary>::rejected(std::make_exception_ptr(std::runtime_error("Process not found")));
    }
    return process->request(request);
  }


  kernel_shield kernel_shield_api;
}