#include "bytecode.hpp"

namespace Manadrain {
namespace Bytecode {
void Reader::populate(const std::vector<std::uint8_t> &buffer_ref) {
  buffer = buffer_ref;
}
void Reader::populate(std::vector<std::uint8_t> &&buffer_ref) {
  buffer = std::move(buffer_ref);
}

std::uint32_t Reader::read4() {
  int start{position};
  std::uint32_t result{};
  while (position < buffer.size() && position < start + 4) {
    int i{position - start};
    result = buffer[position++] << 8 * i | result;
  }
  return result;
}

constexpr std::uint32_t WASM_BINARY_MAGIC = 0x6d736100;

std::expected<void, READER_ERR> Reader::read_module() {
  if (read4() != WASM_BINARY_MAGIC)
    return std::unexpected{READER_ERR::BAD_MAGIC_VALUE};
  return {};
}
} // namespace Bytecode
} // namespace Manadrain
