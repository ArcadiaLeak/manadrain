#include "bytecode.hpp"

namespace Manadrain {
namespace Bytecode {
void Reader::populate(const std::vector<std::uint8_t> &buffer_ref) {
  buffer = buffer_ref;
}
void Reader::populate(std::vector<std::uint8_t> &&buffer_ref) {
  buffer = std::move(buffer_ref);
}

std::uint32_t Reader::read_uns(int cnt) {
  int start{position};
  std::uint32_t result{};
  while (position < buffer.size()) {
    int i = position - start;
    if (i == cnt)
      break;
    std::uint32_t single = buffer[position++];
    result = single << 8 * i | result;
  }
  return result;
}

std::expected<void, READER_ERR> Reader::read_module() {
  if (read_uns(4) != WASM_BINARY_MAGIC)
    return std::unexpected{READER_ERR::BAD_MAGIC_VALUE};
  std::uint32_t version{read_uns(2)}, layer{read_uns(2)};
  if (layer != WASM_BINARY_LAYER_MODULE)
    return std::unexpected{READER_ERR::BAD_WASM_LAYER};
  if (version != WASM_BINARY_VERSION)
    return std::unexpected{READER_ERR::BAD_WASM_VERSION};
  return {};
}
} // namespace Bytecode
} // namespace Manadrain
