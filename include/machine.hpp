#include <array>
#include <cstdint>
#include <stdfloat>
#include <variant>
#include <vector>

namespace Manadrain {
struct I32_ADD {
  std::uint8_t dest;
  std::uint8_t lhs;
  std::uint8_t rhs;
};
struct I64_ADD {
  std::uint8_t dest;
  std::uint8_t lhs;
  std::uint8_t rhs;
};
struct F32_ADD {
  std::uint8_t dest;
  std::uint8_t lhs;
  std::uint8_t rhs;
};
struct LOCAL_LOAD {
  std::size_t offset;
  std::uint8_t reg;
};
struct LOCAL_STOR {
  std::size_t offset;
  std::uint8_t reg;
};
using COMMAND = std::variant<I32_ADD, I64_ADD, F32_ADD, LOCAL_LOAD, LOCAL_STOR>;

struct Machine {
  std::vector<COMMAND> script;
  std::vector<std::uint64_t> local_heap;
  std::array<std::uint64_t, 32> register_file;

  void operator()(I32_ADD cmd);
  void operator()(I64_ADD cmd);
  void operator()(F32_ADD cmd);
  void operator()(LOCAL_LOAD cmd);
  void operator()(LOCAL_STOR cmd);
  void operator()();
};
} // namespace Manadrain
