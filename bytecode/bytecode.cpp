#include <unistr.h>

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

std::expected<std::uint32_t, READER_ERR> Reader::unsign_leb128() {
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

std::expected<std::int32_t, READER_ERR> Reader::signed_leb128() {
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

std::expected<EXTERN_KIND, READER_ERR>
decode_extern_kind(std::uint32_t extern_kind) {
  switch (extern_kind) {
  case 0:
    return EXTERN_KIND::FUNC;
  case 1:
    return EXTERN_KIND::TABLE;
  case 2:
    return EXTERN_KIND::MEMORY;
  }
  return std::unexpected{INVALID_ERR::EXTERN_KIND};
}

expected_task<void, READER_ERR> Reader::read_type_section() {
  std::uint32_t num_signatures{co_await unsign_leb128()};
  for (std::uint32_t i = 0; i < num_signatures; ++i) {
    co_await read_type_form().ok();
    FUNC_TYPE func_type{};
    std::uint32_t num_params{co_await unsign_leb128()};
    func_type.param_types.resize(num_params);
    for (std::uint32_t j = 0; j < num_params; ++j) {
      std::int32_t param_type{co_await signed_leb128()};
      func_type.param_types[j] = co_await decode_primary_type(param_type);
    }
    std::uint32_t num_results{co_await unsign_leb128()};
    if (num_results > 1)
      co_return std::unexpected{CORRUPT_ERR::MULTIVAL_RET};
    func_type.result_types.resize(num_results);
    for (std::uint32_t j = 0; j < num_results; ++j) {
      std::int32_t result_type{co_await signed_leb128()};
      func_type.result_types[j] = co_await decode_primary_type(result_type);
    }
    module_types.push_back(std::move(func_type));
  }
  co_return {};
}

expected_task<void, READER_ERR> Reader::read_function_section() {
  std::uint32_t signature_count{co_await unsign_leb128()};
  for (std::uint32_t i = 0; i < signature_count; ++i) {
    std::uint32_t sig_index{co_await unsign_leb128()};
    func_headers.push_back(sig_index);
  }
  co_return {};
}

expected_task<void, READER_ERR> Reader::read_export_section() {
  std::uint32_t export_count{co_await unsign_leb128()};
  for (std::uint32_t i = 0; i < export_count; ++i) {
    std::uint32_t str_len{co_await unsign_leb128()};
    if (u8_check(buffer.data() + position, str_len))
      co_return std::unexpected{INVALID_ERR::UTF8_STRING};
    std::string export_name{};
    for (int j = 0; j < str_len; ++j)
      export_name.push_back(buffer[position++]);
    if (export_umap.contains(export_name))
      co_return std::unexpected{CORRUPT_ERR::DUP_EXPORT};
    std::uint32_t kind{co_await read_u32(1)};
    std::uint32_t type_idx{co_await unsign_leb128()};
    auto export_it = export_deq.insert(
        export_deq.end(),
        EXPORT_DESC{std::move(export_name), co_await decode_extern_kind(kind),
                    type_idx});
    std::size_t export_idx = std::distance(export_deq.begin(), export_it);
    export_umap[export_it->name] = export_idx;
  }
  co_return {};
}

expected_task<void, READER_ERR> Reader::read_code_section() {
  std::uint32_t num_function_bodies{co_await unsign_leb128()};
  for (std::uint32_t i = 0; i < num_function_bodies; ++i) {
    std::uint32_t body_size{co_await unsign_leb128()};
    std::uint32_t num_local_decls{co_await unsign_leb128()};
    for (std::uint32_t j = 0; i < num_local_decls; ++i) {
      std::uint32_t num_local_types{co_await unsign_leb128()};
      throw std::runtime_error{"unimplemented!"};
    }
  }
  co_return {};
}

expected_task<void, READER_ERR> Reader::read_sections() {
  while (position < buffer.size()) {
    std::uint32_t section_code{co_await read_u32(1)};
    std::uint32_t section_size{co_await unsign_leb128()};
    std::size_t section_end{position + section_size};
    switch (section_code) {
    case 1:
      co_await read_type_section().ok();
      if (position == section_end)
        break;
      co_return std::unexpected{CORRUPT_ERR::SECTION_TOO_SHORT};
    case 3:
      co_await read_function_section().ok();
      if (position == section_end)
        break;
      co_return std::unexpected{CORRUPT_ERR::SECTION_TOO_SHORT};
    case 7:
      co_await read_export_section().ok();
      if (position == section_end)
        break;
      co_return std::unexpected{CORRUPT_ERR::SECTION_TOO_SHORT};
    case 10:
      co_await read_code_section().ok();
      if (position == section_end)
        break;
      co_return std::unexpected{CORRUPT_ERR::SECTION_TOO_SHORT};
    default:
      co_return std::unexpected{CORRUPT_ERR::SECTION_CODE};
    }
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
