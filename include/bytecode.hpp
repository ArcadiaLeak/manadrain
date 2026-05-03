#include <cstdint>
#include <expected>
#include <vector>

namespace Manadrain {
namespace Bytecode {
inline constexpr std::uint32_t WASM_BINARY_MAGIC{0x6d736100};
inline constexpr std::uint32_t WASM_BINARY_VERSION{1};
inline constexpr std::uint32_t WASM_BINARY_LAYER_MODULE{0};

enum class READER_ERR { BAD_MAGIC_VALUE, BAD_WASM_LAYER, BAD_WASM_VERSION };

class Reader {
public:
  void populate(const std::vector<std::uint8_t> &buffer_ref);
  void populate(std::vector<std::uint8_t> &&buffer_ref);
  std::expected<void, READER_ERR> read_module();

private:
  int position;
  std::vector<std::uint8_t> buffer;

  std::uint32_t read_uns(int cnt);
};
} // namespace Bytecode
} // namespace Manadrain
