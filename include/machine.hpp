#include <array>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <stdfloat>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
struct Machine {
  static constexpr std::size_t I32T{0};
  static constexpr std::size_t I64T{1};
  static constexpr std::size_t F32T{2};
  static constexpr std::size_t F64T{3};
  static constexpr std::size_t U32T{4};
  static constexpr std::size_t U64T{5};

  using INSTRUCTION = std::variant<std::monostate>;
  struct FUNCTION {
    std::vector<INSTRUCTION> inst_vec;
  };
  std::vector<FUNCTION> function_vec;
  std::unordered_map<std::string, std::size_t> funcname_umap;

  std::array<std::uint64_t, 32> register_file;
  std::vector<std::uint64_t> local_heap;

  using SHARED_HEAP = std::list<std::vector<std::uint64_t>>;
  SHARED_HEAP shared_heap;
  std::pair<std::optional<std::size_t>, SHARED_HEAP::iterator> shared_cache;
  std::map<std::size_t, std::deque<SHARED_HEAP::iterator>> shared_lookup;
};
} // namespace Manadrain
