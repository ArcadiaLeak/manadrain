#include <array>
#include <cstdint>
#include <stdfloat>
#include <variant>
#include <vector>

namespace Manadrain {
namespace Machine {
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

class RegisterFile {
public:
  std::uint64_t &at(std::uint8_t idx) { return regfile[idx & 0b11111]; }

private:
  std::array<std::uint64_t, 32> regfile;
};

class Execution {
public:
  std::vector<COMMAND> script;
  std::vector<std::uint64_t> local_heap;

  void operator()();
  void operator()(I32_ADD cmd);
  void operator()(I64_ADD cmd);
  void operator()(F32_ADD cmd);
  void operator()(LOCAL_LOAD cmd);
  void operator()(LOCAL_STOR cmd);

private:
  RegisterFile regfile;
};
} // namespace Machine
} // namespace Manadrain
