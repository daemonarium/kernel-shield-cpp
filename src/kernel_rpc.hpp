#include <serialization/socket_rpc_client.hpp>
#include <serialization/promise.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include "kernel.hpp"

namespace waos {

  // Wire widths must match ts-kernel createGeneratedTypeRegistry (uint32 ids
  // and limits). Compressed varints hid uint64 vs uint32 mismatches; flat uses
  // fixed LE widths so the C++ call template types must match the schema.
  template <typename rpc_client_ptr_type>
  class kernel_rpc {
    public:

      using direction = kernel::direction;
      using process_id = kernel::process_id;
      using socket_id = kernel::socket_id;

      kernel_rpc(rpc_client_ptr_type rpc_client) : rpc_client_(rpc_client) {}
      ~kernel_rpc() {}
      
      std::shared_ptr<serialization::promise<std::uint32_t>> spawn(std::vector<std::uint8_t> code, std::uint32_t memory_limit, std::uint32_t mana_limit) {
        return rpc_client_->template call<std::uint32_t>(0, code, memory_limit, mana_limit);
      }

      std::shared_ptr<serialization::promise<std::uint32_t>> resurect(std::vector<std::uint8_t> code, std::vector<std::uint8_t> memory, std::uint32_t memory_limit, std::uint32_t mana_limit) {
        return rpc_client_->template call<std::uint32_t>(1, code, memory, memory_limit, mana_limit);
      }

      std::shared_ptr<serialization::promise<save_result>> save(std::uint32_t id) {
        return rpc_client_->template call<save_result>(2, id);
      }

      std::shared_ptr<serialization::promise<void>> kill(std::uint32_t id) {
        return rpc_client_->template call<void>(3, id);
      }

      std::shared_ptr<serialization::promise<void>> bind(std::uint32_t process_from, std::uint32_t socket_from, std::uint32_t process_to, std::uint32_t socket_to, direction direction) {
        return rpc_client_->template call<void>(4, process_from, socket_from, process_to, socket_to, direction);
      }

      std::shared_ptr<serialization::promise<void>> unbind(std::uint32_t process_from, std::uint32_t socket_from, std::uint32_t process_to, std::uint32_t socket_to, direction direction) {
        return rpc_client_->template call<void>(5, process_from, socket_from, process_to, socket_to, direction);
      }

      std::shared_ptr<serialization::promise<void>> monitor(std::uint32_t target, std::uint32_t monitor, std::uint32_t monitor_socket, monitor_config config) {
        return rpc_client_->template call<void>(6, target, monitor, monitor_socket, config);
      }

      std::shared_ptr<serialization::promise<void>> demonitor(std::uint32_t target, std::uint32_t monitor, std::uint32_t monitor_socket) {
        return rpc_client_->template call<void>(7, target, monitor, monitor_socket);
      }

      std::shared_ptr<serialization::promise<std::uint32_t>> measure_mana(std::uint32_t process, std::uint32_t new_mana) {
        return rpc_client_->template call<std::uint32_t>(8, process, new_mana);
      }

      std::shared_ptr<serialization::promise<void>> set_mana_limit(std::uint32_t process, std::uint32_t mana_limit) {
        return rpc_client_->template call<void>(9, process, mana_limit);
      }

      std::shared_ptr<serialization::promise<std::vector<kernel::socket_binding>>> list_socket_bindings(std::uint32_t id) {
        return rpc_client_->template call<std::vector<kernel::socket_binding>>(10, id);
      }

      std::shared_ptr<serialization::promise<std::vector<std::uint8_t>>> get_rpc_description(std::uint32_t id, std::uint32_t socket) {
        return rpc_client_->template call<std::vector<std::uint8_t>>(11, id, socket);
      }

    private:
      rpc_client_ptr_type rpc_client_;
  };

};
