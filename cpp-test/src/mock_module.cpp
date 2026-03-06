#include <utils/module.hpp>

#include <string>
#include <vector>
#include <cstdint>

using binary = std::vector<std::uint8_t>;

class mock_api {
  public:
    static binary echo(binary data) {
      return data;
    }
};

mock_api mock;

void register_all() {
  register_rpc(mock);
}
