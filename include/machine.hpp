#include <array>
#include <cstdint>
#include <stdfloat>
#include <string>
#include <unordered_map>
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
struct U64_LOC_LOAD {
  std::size_t offset;
  std::uint8_t reg;
};
struct U64_LOC_STOR {
  std::size_t offset;
  std::uint8_t reg;
};
using COMMAND =
    std::variant<I32_ADD, I64_ADD, F32_ADD, U64_LOC_LOAD, U64_LOC_STOR>;

union UNIFORM {
  std::uint64_t ulong;
  std::uint32_t uint[2];
  std::int64_t slong;
  std::int32_t sint[2];
  std::float64_t float64;
  std::float32_t float32[2];
};

struct Machine {
  std::vector<std::vector<COMMAND>> function_vec;
  std::unordered_map<std::string, std::size_t> funcname_umap;
  std::vector<UNIFORM> local_heap;
  std::array<UNIFORM, 32> register_file;

  void operator()(I32_ADD cmd);
  void operator()(I64_ADD cmd);
  void operator()(F32_ADD cmd);
  void operator()(U64_LOC_LOAD cmd);
  void operator()(U64_LOC_STOR cmd);
  void operator()(std::size_t func_idx);
};
} // namespace Manadrain
