#include <array>
#include <cstdint>
#include <stdfloat>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
struct Machine {
  struct LOAD_IMMEDIATE {
    std::uint8_t dest;
    std::uint64_t val;
  };
  using INSTRUCTION = std::variant<LOAD_IMMEDIATE>;

  struct FUNCTION {
    std::vector<INSTRUCTION> inst_vec;
    const std::vector<std::vector<std::uint64_t>> const_pool;
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
