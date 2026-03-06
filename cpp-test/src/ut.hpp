#pragma once

#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace ut {

  struct test_result {
    std::string name;
    bool passed;
    std::string message;
  };

  struct runner {
    std::vector<test_result> results;
    std::string output;
    bool done = false;
    int total_passed = 0;
    int total_failed = 0;

    void report_pass(const std::string& name) {
      results.push_back({name, true, ""});
      output += "  PASS: " + name + "\n";
      total_passed++;
    }

    void report_fail(const std::string& name, const std::string& reason) {
      results.push_back({name, false, reason});
      output += "  FAIL: " + name + " - " + reason + "\n";
      total_failed++;
    }

    void finish() {
      output += "\n";
      output += std::to_string(total_passed) + " passed, "
              + std::to_string(total_failed) + " failed\n";
      done = true;
    }

    bool all_passed() const {
      return total_failed == 0;
    }
  };

  inline runner& get_runner() {
    static runner r;
    return r;
  }

  struct expect_check {
    bool condition;
    std::string file;
    int line;
    std::string expr;

    expect_check(bool c, const char* f, int l, const char* e)
      : condition(c), file(f), line(l), expr(e) {}

    ~expect_check() noexcept(false) {
      if(!condition) {
        throw std::runtime_error(
          std::string(file) + ":" + std::to_string(line) + " expect(" + expr + ") failed");
      }
    }
  };

  template<typename A, typename B>
  struct eq_check {
    A a;
    B b;
    std::string file;
    int line;
    std::string expr_a;
    std::string expr_b;

    eq_check(A a_, B b_, const char* f, int l, const char* ea, const char* eb)
      : a(a_), b(b_), file(f), line(l), expr_a(ea), expr_b(eb) {}

    ~eq_check() noexcept(false) {
      if(!(a == b)) {
        std::ostringstream ss;
        ss << file << ":" << line << " expect_eq(" << expr_a << ", " << expr_b << ") failed: "
           << a << " != " << b;
        throw std::runtime_error(ss.str());
      }
    }
  };

}

#define ut_expect(cond) ::ut::expect_check(cond, __FILE__, __LINE__, #cond)
#define ut_expect_eq(a, b) ::ut::eq_check(a, b, __FILE__, __LINE__, #a, #b)
