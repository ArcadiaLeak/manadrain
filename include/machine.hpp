#include <array>
#include <cstdint>
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

  struct SHARED_HEAP {
    std::uint64_t counter;
    using CONTAINER =
        std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>;
    CONTAINER entry_umap;
    std::array<CONTAINER::node_type, 2> cache;
    bool last_extracted;
  };
  SHARED_HEAP shared_heap;

  class NULL_HANDLE_ERROR : public std::exception {
  public:
    const char *what() const noexcept override { return "null handle error!"; }
  };

  SHARED_HEAP::CONTAINER::node_type &heap_get(std::uint64_t handle);
  std::size_t heap_alloc();
  void heap_free(std::size_t handle);
};
} // namespace Manadrain
