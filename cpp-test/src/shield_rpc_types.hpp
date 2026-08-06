#pragma once

#include <cstdint>
#include <exception>
#include <future>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <system_error>
#include <typeinfo>
#include <variant>
#include <vector>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <new>

#include <serialization/annotations.hpp>
#include <serialization/rpc.hpp>
#include <serialization/type_description.hpp>
#include <serialization/type_registry.hpp>

namespace test {

using binary = std::vector<std::uint8_t>;

struct monitor_config {
  std::uint32_t flags{};
};

struct save_result {
  std::uint64_t snapshot_uid{};
  binary memory{};
  std::size_t mana_used{};
  std::size_t mana_limit{};
};

enum class direction {
  NONE = 0,
  IN = 1,
  OUT = 2,
  BOTH = 3
};

struct socket_binding {
  std::uint32_t socket{};
  std::uint32_t peer_process{};
  std::uint32_t peer_socket{};
  direction direction{};
};

class kernel_shield {
 public:
  kernel_shield() = default;

};

class process_event {
 public:
  process_event() = default;

  std::uint32_t process{};
};

class process_killed_event : public process_event {
 public:
  process_killed_event() = default;

};

class process_error_event : public process_event {
 public:
  process_error_event() = default;

  std::string error{};
};

template <typename Registry>
void register_types(Registry& registry) {
  (void)registry;
  // Register type rpc_request
  registry.register_type(serialization::type_description::create(serialization::rpc_request{}, registry));
  // Register type rpc_response
  registry.register_type(serialization::type_description::create(serialization::rpc_response{}, registry));
  // Register type rpc_error
  registry.register_type(serialization::type_description::create(serialization::rpc_error{}, registry));
  // Register type read_annotation
  registry.register_type(serialization::type_description::create(serialization::read_annotation{}, registry));
  // Register type member_annotation
  registry.register_type(serialization::type_description::create(serialization::member_annotation{}, registry));
  // Register type kernel_shield
  registry.register_type(serialization::type_description::create(kernel_shield{}, registry));
  // Register type process_killed_event
  registry.register_type(serialization::type_description::create(process_killed_event{}, registry));
  // Register type process_event
  registry.register_type(serialization::type_description::create(process_event{}, registry));
  // Register type process_error_event
  registry.register_type(serialization::type_description::create(process_error_event{}, registry));
  // Register type direction
  registry.register_type(serialization::type_description::create(direction{}, registry));
  // Register exception unknown_exception
  registry.register_type(serialization::type_description::create(serialization::unknown_exception{}, registry));
  // Register exception runtime_error
  registry.register_type(serialization::type_description::create(std::runtime_error(std::string{}), registry));
}

template <typename CodecRegistry>
void register_codecs(CodecRegistry& codecs) {
  (void)codecs;
  codecs.template register_dynamic_type<serialization::read_annotation>();
  codecs.template register_dynamic_type<serialization::member_annotation>();
  codecs.template register_message_type<serialization::rpc_request>();
  codecs.template register_message_type<serialization::rpc_response>();
  codecs.template register_message_type<serialization::rpc_error>();
  codecs.template register_message_type<process_killed_event>();
  codecs.template register_message_type<process_error_event>();
  codecs.template register_exception_type<serialization::unknown_exception>();
  codecs.template register_exception_type<std::runtime_error>();
}

} // namespace test

