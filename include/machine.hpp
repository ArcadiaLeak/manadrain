#include <cstdint>
#include <variant>
#include <vector>

namespace Manadrain {
namespace Machine {
struct ADD_INT32 {
  std::uint8_t dest;
  std::uint8_t lhs;
  std::uint8_t rhs;
};
struct ADD_INT64 {
  std::uint8_t dest;
  std::uint8_t lhs;
  std::uint8_t rhs;
};
using COMMAND = std::variant<ADD_INT32, ADD_INT64>;

struct Execution {
  std::vector<COMMAND> script;
  std::vector<std::uint64_t> register_file;

  void operator()(ADD_INT32 cmd);
  void operator()(ADD_INT64 cmd);
  void operator()();
};
} // namespace Machine
} // namespace Manadrain
