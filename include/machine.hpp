#include <array>
#include <cstdint>
#include <optional>
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
struct I32_IMM_LOAD {
  std::uint8_t dest;
  std::int32_t val;
};
struct U64_LOC_LOAD {
  std::size_t offset;
  std::uint8_t reg;
};
struct U64_LOC_STOR {
  std::size_t offset;
  std::uint8_t reg;
};
using COMMAND = std::variant<I32_ADD, I64_ADD, F32_ADD, U64_LOC_LOAD,
                             U64_LOC_STOR, I32_IMM_LOAD>;

union UNIFORM {
  std::uint64_t ulong;
  std::uint32_t uint[2];
  std::int64_t slong;
  std::int32_t sint[2];
  std::float64_t float64;
  std::float32_t float32[2];
};

struct HEAP_TOMBSTONE {};
struct HEAP_VACANCY {
  std::optional<std::size_t> another;
};
using HEAP_SLOT =
    std::variant<std::vector<UNIFORM>, HEAP_TOMBSTONE, HEAP_VACANCY>;

struct Machine {
  std::vector<std::vector<COMMAND>> function_vec;
  std::unordered_map<std::string, std::size_t> funcname_umap;
  std::vector<UNIFORM> local_heap;
  std::array<UNIFORM, 32> register_file;

  std::size_t n_tombstones;
  std::optional<std::size_t> last_vacancy;
  std::vector<HEAP_SLOT> global_heap;

  std::size_t heap_alloc();
  void heap_free(std::size_t);
  bool is_tombstone_ptr(UNIFORM word);
  void heap_reclaim();

  void operator()(I32_ADD cmd);
  void operator()(I64_ADD cmd);
  void operator()(F32_ADD cmd);
  void operator()(U64_LOC_LOAD cmd);
  void operator()(U64_LOC_STOR cmd);
  void operator()(I32_IMM_LOAD cmd);
  void operator()(std::size_t func_idx);
};
} // namespace Manadrain
