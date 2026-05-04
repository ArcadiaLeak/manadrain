#include "bytecode.hpp"

namespace Manadrain {
namespace Bytecode {
void Reader::populate(const std::vector<std::uint8_t> &buffer_ref) {
  buffer = buffer_ref;
}
void Reader::populate(std::vector<std::uint8_t> &&buffer_ref) {
  buffer = std::move(buffer_ref);
}

std::expected<std::uint32_t, READER_ERR> Reader::read_u32(int cnt) {
  int start{position};
  std::uint32_t result{};
  while (position < buffer.size()) {
    int i = position - start;
    if (i == cnt)
      return result;
    std::uint32_t single = buffer[position++];
    result = single << 8 * i | result;
  }
  return std::unexpected{INVALID_ERR::BYTE_SEQUENCE};
}

std::expected<std::uint32_t, READER_ERR> Reader::read_u32_leb128() {
  int start{position};
  std::uint32_t result{};
  while (position < buffer.size()) {
    int count{position - start};
    if (count == 5)
      break;
    int shift{count * 7};
    std::uint32_t payload{buffer[position] & 0b1111111u};
    result |= (payload << shift);
    if ((buffer[position++] & 0b10000000u) == 0)
      return result;
  }
  return std::unexpected{INVALID_ERR::BYTE_SEQUENCE};
}

expected_task<void, READER_ERR> Reader::read_type_section(std::uint32_t size) {
  co_return {};
}

expected_task<void, READER_ERR> Reader::read_sections() {
  while (position < buffer.size()) {
    std::uint32_t section_code{co_await read_u32(1)};
    std::uint32_t section_size{co_await read_u32_leb128()};
    if (section_code == 1) {
      co_await read_type_section(section_size).ok();
      continue;
    }
    co_return std::unexpected{INVALID_ERR::SECTION_CODE};
  }
}

expected_task<void, READER_ERR> Reader::read_module() {
  if (read_u32(4) != WASM_BINARY_MAGIC)
    co_return std::unexpected{INVALID_ERR::WASM_MAGIC};
  std::uint32_t version{co_await read_u32(2)}, layer{co_await read_u32(2)};
  if (layer != WASM_BINARY_LAYER_MODULE)
    co_return std::unexpected{INVALID_ERR::WASM_LAYER};
  if (version != WASM_BINARY_VERSION)
    co_return std::unexpected{INVALID_ERR::WASM_VERSN};
  co_return read_sections().ok();
}
} // namespace Bytecode
} // namespace Manadrain
