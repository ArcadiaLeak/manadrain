#include <cstdint>
#include <inplace_vector>
#include <optional>
#include <stdfloat>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
enum class MACHINE_DATATYPE { I32T, I64T, F32T, F64T, U32T, U64T };

struct I32_ADD {};
struct I64_ADD {};
struct F32_ADD {};
struct I64_SUB {};
struct I32_PUSH {
  std::int32_t val;
};
struct I64_PUSH {
  std::int64_t val;
};
struct U64_PUSH {
  std::uint64_t val;
};
struct LOC_LOAD {
  std::size_t offset;
};
struct LOC_STORE {
  std::size_t offset;
};
struct LOC_APPEND {};
struct I64_TO_I32 {};
struct U64_TO_I32 {};
struct I32_TO_I64 {
  std::uint8_t adv;
};
using MACHINE_CMD = std::variant<I32_ADD, I64_ADD, F32_ADD, I64_SUB, LOC_LOAD,
                                 LOC_STORE, LOC_APPEND, I32_PUSH, I64_PUSH,
                                 U64_PUSH, I64_TO_I32, U64_TO_I32, I32_TO_I64>;

struct HEAP_TOMBSTONE {};
struct HEAP_VACANCY {
  std::optional<std::size_t> another;
};
using HEAP_SLOT =
    std::variant<std::vector<std::uint64_t>, HEAP_TOMBSTONE, HEAP_VACANCY>;

struct MACHINE_FUNC {
  std::vector<MACHINE_CMD> command_vec;
  const std::vector<std::uint64_t> const_pool;
};

struct Machine {
  std::vector<MACHINE_FUNC> function_vec;
  std::unordered_map<std::string, std::size_t> funcname_umap;
  std::vector<std::uint64_t> local_heap;
  std::inplace_vector<std::uint64_t, 32> register_file;

  std::size_t n_tombstones;
  std::optional<std::size_t> last_vacancy;
  std::vector<HEAP_SLOT> global_heap;

  std::size_t heap_alloc();
  void heap_free(std::size_t);
  bool is_tombstone_ptr(std::uint64_t word);
  void heap_reclaim();

  void operator()(I32_ADD cmd);
  void operator()(I64_ADD cmd);
  void operator()(F32_ADD cmd);
  void operator()(I64_SUB cmd);
  void operator()(LOC_LOAD cmd);
  void operator()(LOC_STORE cmd);
  void operator()(LOC_APPEND cmd);
  void operator()(I32_PUSH cmd);
  void operator()(I64_PUSH cmd);
  void operator()(U64_PUSH cmd);
  void operator()(I64_TO_I32 cmd);
  void operator()(U64_TO_I32 cmd);
  void operator()(I32_TO_I64 cmd);
  void operator()(std::size_t func_idx);
};
} // namespace Manadrain
