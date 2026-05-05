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
  std::size_t start{position};
  std::uint32_t result{};
  while (position < buffer.size()) {
    std::size_t i{position - start};
    if (i == cnt)
      return result;
    std::uint32_t single = buffer[position++];
    result = single << 8 * i | result;
  }
  return std::unexpected{CORRUPT_ERR::UNSIGN_FIXED};
}

std::expected<std::uint32_t, READER_ERR> Reader::read_u32_leb128() {
  std::uint32_t payload{}, result{};
  for (int i = 0; i < 4; ++i) {
    if (position >= buffer.size())
      return std::unexpected{CORRUPT_ERR::UNSIGN_LEB128};
    payload = buffer[position] & 0b1111111u;
    result |= payload << i * 7;
    if ((buffer[position++] & 0x80u) == 0)
      return result;
  }
  if (position >= buffer.size())
    return std::unexpected{CORRUPT_ERR::UNSIGN_LEB128};
  payload = buffer[position] & 0b1111111u;
  if (payload & 0b1110000u)
    return std::unexpected{CORRUPT_ERR::UNSIGN_LEB128};
  result |= payload << 28;
  if ((buffer[position++] & 0x80u) == 0)
    return result;
  return std::unexpected{CORRUPT_ERR::UNSIGN_LEB128};
}

std::expected<std::int32_t, READER_ERR> Reader::read_s32_leb128() {
  std::uint32_t result{};
  for (int i = 0; i < 4; ++i) {
    if (position >= buffer.size())
      return std::unexpected{CORRUPT_ERR::SIGNED_LEB128};
    result |= (buffer[position] & 0b1111111u) << i * 7;
    std::uint32_t extension{~0u << (i + 1) * 7};
    extension = buffer[position] & 0x40u ? extension : 0;
    if ((buffer[position++] & 0x80u) == 0)
      return std::bit_cast<std::int32_t>(result | extension);
  }
  if (position >= buffer.size())
    return std::unexpected{CORRUPT_ERR::SIGNED_LEB128};
  std::uint8_t must_be = buffer[position] & 0x40u ? 0b111000u : 0;
  if ((buffer[position] & 0b111000u) != must_be)
    return std::unexpected{CORRUPT_ERR::SIGNED_LEB128};
  result |= (buffer[position] & 0b1111111u) << 28;
  if ((buffer[position++] & 0x80u) == 0)
    return std::bit_cast<std::int32_t>(result);
  return std::unexpected{CORRUPT_ERR::SIGNED_LEB128};
}

expected_task<void, READER_ERR> Reader::read_type_form() {
  std::uint32_t type_form{co_await read_u32(1)};
  if (type_form == 0x60)
    co_return {};
  co_return std::unexpected{UNEXPECT_ERR::TYPE_FORM};
}

std::expected<PRIM_TYPE, READER_ERR>
decode_primary_type(std::int32_t param_type) {
  switch (param_type) {
  case -1:
    return PRIM_TYPE::I32T;
  case -2:
    return PRIM_TYPE::I64T;
  case -3:
    return PRIM_TYPE::F32T;
  case -4:
    return PRIM_TYPE::F64T;
  }
  return std::unexpected{INVALID_ERR::PARAM_TYPE};
}

expected_task<void, READER_ERR> Reader::read_type_section(std::uint32_t size) {
  std::uint32_t num_signatures{co_await read_u32_leb128()};
  for (std::uint32_t i = 0; i < num_signatures; ++i) {
    co_await read_type_form().ok();
    FUNC_TYPE func_type{};
    std::uint32_t num_params{co_await read_u32_leb128()};
    func_type.param_types.resize(num_params);
    for (std::uint32_t j = 0; j < num_params; ++j) {
      std::int32_t param_type{co_await read_s32_leb128()};
      func_type.param_types[j] = co_await decode_primary_type(param_type);
    }
    std::uint32_t num_results{co_await read_u32_leb128()};
    func_type.result_types.resize(num_results);
    for (std::uint32_t j = 0; j < num_results; ++j) {
      std::int32_t result_type{co_await read_s32_leb128()};
      func_type.result_types[j] = co_await decode_primary_type(result_type);
    }
    func_types.push_back(std::move(func_type));
  }
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
    co_return std::unexpected{INVALID_ERR::SECTN_CODE};
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
