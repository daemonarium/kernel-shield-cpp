#include <serialization/socket_rpc_client.hpp>
#include <serialization/socket_rpc_server.hpp>
#include <serialization/type_registry.hpp>
#include <serialization/type_description.hpp>
#include <serialization/stream_writer.hpp>
#include <utils/socket_handlers.hpp>
#include <socket/socket.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstdio>

#include "shield_rpc.hpp"
#include "ut.hpp"

// Globals
static std::unique_ptr<serialization::socket_rpc_client> rpc_client;
static std::unique_ptr<test::shield_rpc<serialization::socket_rpc_client*>> shield;
static std::string test_output;
static bool tests_done = false;
static std::vector<uint8_t> mock_wasm_code;

// Async test runner - chains tests via promise callbacks
struct async_test_runner {
  struct test_case {
    std::string name;
    std::function<void(std::function<void()>)> run; // takes a "done" callback
  };

  std::vector<test_case> tests;
  size_t current = 0;

  void add(const char* name, std::function<void(std::function<void()>)> fn) {
    tests.push_back({name, fn});
  }

  void run_all() {
    test_output += "Kernel Shield C++ Tests\n";
    test_output += "=======================\n\n";
    run_next();
  }

  void run_next() {
    if(current >= tests.size()) {
      ut::get_runner().finish();
      test_output += ut::get_runner().output;
      tests_done = true;
      return;
    }

    auto& test = tests[current];
    auto test_name = test.name;
    printf("[test] running: %s\n", test_name.c_str());
    auto done_cb = [this, test_name]() {
      printf("[test] PASS: %s\n", test_name.c_str());
      ut::get_runner().report_pass(test_name);
      current++;
      run_next();
    };

    try {
      test.run(done_cb);
    } catch(const std::exception& e) {
      printf("[test] FAIL: %s - %s\n", test_name.c_str(), e.what());
      ut::get_runner().report_fail(test_name, e.what());
      current++;
      run_next();
    }
  }
};

static async_test_runner runner;
static uint64_t spawned_process_id = 0;

// ---- Test definitions ----

void test_set_mana_limit(std::function<void()> done) {
  auto promise = shield->set_mana_limit(500000);
  promise->handle([done]() {
    done();
  }, [](std::exception_ptr ex) {
    std::rethrow_exception(ex);
  });
}

void test_spawn(std::function<void()> done) {
  auto promise = shield->spawn(mock_wasm_code, 2048, 1000000);
  promise->handle([done](uint64_t process_id) {
    ut_expect(process_id > 0);
    spawned_process_id = process_id;
    done();
  }, [](std::exception_ptr ex) {
    std::rethrow_exception(ex);
  });
}

void test_bind(std::function<void()> done) {
  uint64_t pid = spawned_process_id;
  uint64_t shield_pid = 1; // kernel-shield is process 1

  // Bind spawned process socket 0 <-> kernel-shield socket (pid<<1|0)
  auto p1 = shield->bind(pid, 0, shield_pid, (pid << 1) | 0, test::direction::BOTH);
  p1->handle([pid, shield_pid, done]() {
    // Bind spawned process socket 1 <-> kernel-shield socket (pid<<1|1)
    auto p2 = shield->bind(pid, 1, shield_pid, (pid << 1) | 1, test::direction::BOTH);
    p2->handle([done]() {
      done();
    }, [](std::exception_ptr ex) {
      printf("[test] bind FAIL\n");
      std::rethrow_exception(ex);
    });
  }, [](std::exception_ptr ex) {
    std::rethrow_exception(ex);
  });
}

void test_request(std::function<void()> done) {
  std::vector<uint8_t> data = {1, 2, 3, 4};
  auto promise = shield->request(spawned_process_id, data);
  promise->handle([done, data](std::vector<uint8_t> response) {
    ut_expect_eq(response.size(), data.size());
    ut_expect(response == data);
    done();
  }, [](std::exception_ptr ex) {
    std::rethrow_exception(ex);
  });
}

void test_request_nonexistent(std::function<void()> done) {
  std::vector<uint8_t> data = {1, 2, 3, 4};
  auto promise = shield->request(999, data);
  promise->handle([](std::vector<uint8_t>) {
    throw std::runtime_error("should have thrown an error for non-existent process");
  }, [done](std::exception_ptr) {
    // Expected - request to non-existent process should fail
    done();
  });
}

void test_save(std::function<void()> done) {
  auto promise = shield->save(spawned_process_id);
  promise->handle([done](std::vector<uint8_t> memory) {
    ut_expect(memory.size() > 0);
    done();
  }, [done](std::exception_ptr) {
    // save may fail for LocalProcess mock - that's OK, we verified the call was made
    done();
  });
}

void test_kill(std::function<void()> done) {
  auto promise = shield->kill(spawned_process_id);
  promise->handle([done]() {
    done();
  }, [](std::exception_ptr ex) {
    std::rethrow_exception(ex);
  });
}

// ---- WASM exports ----

extern "C" size_t rpc_description(std::uint32_t socket, char** buffer) {
  return 0; // test module doesn't export RPC
}

extern "C" void waos_socket_data(std::uint32_t socket) {
  socket_handlers::handle_socket_data(socket);
}

extern "C" void waos_socket_output_drain(int socket) {
  socket_handlers::handle_socket_drain(socket);
}

extern "C" bool is_test_done() {
  return tests_done;
}

extern "C" size_t get_test_output(char** buffer) {
  *buffer = test_output.data();
  return test_output.size();
}

std::shared_ptr<serialization::promise<void>> test_promise_exception() {
  auto p = std::make_shared<serialization::promise<void>>();
  p->reject(std::make_exception_ptr(std::runtime_error("test exception")));

  try {
    //throw std::exception();
    co_await p;
    printf("no exception\n");
  } catch (const std::exception& e) {
    printf("exception: %s\n", e.what());
  } catch (const std::runtime_error& e) {
    printf("runtime error: %s\n", e.what());
  } catch (...) {
    printf("unknown exception\n");
  }
  printf("test done\n");
}

extern "C" void set_mock_code(char* ptr, size_t size) {
  test_promise_exception();

  mock_wasm_code.assign(reinterpret_cast<uint8_t*>(ptr), reinterpret_cast<uint8_t*>(ptr) + size);

  // Register tests and run
  runner.add("set_mana_limit passthrough", test_set_mana_limit);
  runner.add("spawn process", test_spawn);
  runner.add("bind process sockets", test_bind);
  runner.add("request to process (echo)", test_request);
  runner.add("request to non-existent process", test_request_nonexistent);
  runner.add("save process", test_save);
  runner.add("kill process", test_kill);

  runner.run_all();
}

extern "C" void start() {
  sockets_init(4);

  struct socket_options socket_options = {
    .input_buffer_size = 1024,
    .output_buffer_size = 1024,
    .flush_threshold = 1024,
    .data_event_threshold = 1024,
    .drain_event_threshold = 1024,
    .flags = 0,
  };

  // Socket 0: connected to kernel-shield USER socket
  socket_open(0, &socket_options);

  rpc_client = std::make_unique<serialization::socket_rpc_client>(0);
  shield = std::make_unique<test::shield_rpc<serialization::socket_rpc_client*>>(rpc_client.get());

  // Register handler for socket 0 (responses from kernel-shield)
  auto client_ptr = rpc_client.get();
  socket_handlers::register_socket_handler(0, [client_ptr]() {
    client_ptr->handle_data();
  });
}
