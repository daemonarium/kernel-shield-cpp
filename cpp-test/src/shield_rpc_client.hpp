#include <serialization/rpc_client.hpp>
#include <serialization/promise.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

#include "shield_rpc_types.hpp"

namespace test {

template <typename rpc_client_type>
class kernel_shield_rpc {
  public:
    kernel_shield_rpc(std::shared_ptr<rpc_client_type> rpc_client) : rpc_client_(rpc_client) {}
    ~kernel_shield_rpc() {}
    
    std::shared_ptr<serialization::promise<std::uint32_t>> spawn(std::vector<std::uint8_t> code, std::uint32_t memory_limit, std::uint32_t mana_limit) {
      return rpc_client_->template call<std::uint32_t>(0, code, memory_limit, mana_limit);
    }

    std::shared_ptr<serialization::promise<std::uint32_t>> resurect(std::vector<std::uint8_t> code, std::vector<std::uint8_t> memory, std::uint32_t memory_limit, std::uint32_t mana_limit) {
      return rpc_client_->template call<std::uint32_t>(1, code, memory, memory_limit, mana_limit);
    }

    std::shared_ptr<serialization::promise<std::vector<std::uint8_t>>> save(std::uint32_t id) {
      return rpc_client_->template call<std::vector<std::uint8_t>>(2, id);
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

    std::shared_ptr<serialization::promise<void>> monitor(std::uint32_t target, std::uint32_t monitor, std::uint32_t monitor_socket) {
      return rpc_client_->template call<void>(6, target, monitor, monitor_socket);
    }

    std::shared_ptr<serialization::promise<void>> demonitor(std::uint32_t target, std::uint32_t monitor, std::uint32_t monitor_socket) {
      return rpc_client_->template call<void>(7, target, monitor, monitor_socket);
    }

    std::shared_ptr<serialization::promise<std::uint32_t>> measure_mana(std::uint32_t process, std::uint32_t new_mana) {
      return rpc_client_->template call<std::uint32_t>(8, process, new_mana);
    }

    std::shared_ptr<serialization::promise<void>> set_mana_limit(std::uint32_t mana_limit) {
      return rpc_client_->template call<void>(9, mana_limit);
    }

    std::shared_ptr<serialization::promise<std::vector<std::uint8_t>>> request(std::uint32_t process_id, std::vector<std::uint8_t> request) {
      return rpc_client_->template call<std::vector<std::uint8_t>>(10, process_id, request);
    }

  private:
    std::shared_ptr<rpc_client_type> rpc_client_;
};


} // namespace test

