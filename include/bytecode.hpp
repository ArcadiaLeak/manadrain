#include <cstdint>
#include <optional>
#include <vector>

#include "expected_task.hpp"

namespace Manadrain {
namespace Bytecode {
inline constexpr std::uint32_t WASM_BINARY_MAGIC{0x6d736100};
inline constexpr std::uint32_t WASM_BINARY_VERSION{1};
inline constexpr std::uint32_t WASM_BINARY_LAYER_MODULE{0};

enum class INVALID_ERR {
  WASM_MAGIC,
  WASM_LAYER,
  WASM_VERSN,
  BYTE_SEQUENCE,
  SECTION_CODE
};
using READER_ERR = INVALID_ERR;

class Reader {
public:
  void populate(const std::vector<std::uint8_t> &buffer_ref);
  void populate(std::vector<std::uint8_t> &&buffer_ref);
  expected_task<void, READER_ERR> read_module();

private:
  int position;
  std::vector<std::uint8_t> buffer;

  std::expected<std::uint32_t, READER_ERR> read_u32(int cnt);
  std::expected<std::uint32_t, READER_ERR> read_u32_leb128();

  expected_task<void, READER_ERR> read_sections();
  expected_task<void, READER_ERR> read_type_section(std::uint32_t size);
};
} // namespace Bytecode
} // namespace Manadrain
