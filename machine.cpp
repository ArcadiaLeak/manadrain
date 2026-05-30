#include <cassert>

#include "machine.hpp"

namespace Manadrain {
std::generator<std::uint8_t> Parser::traverse() {
  while (position < binary_buffer->size()) {
    co_yield (*binary_buffer)[position];
    ++position;
  }
}

std::optional<std::uint8_t> Parser::forward() {
  for (std::uint8_t leading : traverse() | std::views::take(1)) {
    ++position;
    return leading;
  }
  return std::nullopt;
}

std::uint32_t Parser::decode_varuint32() {
  std::uint32_t result{};
  unsigned shift{};
  for (std::uint8_t leading : traverse()) {
    std::uint32_t data_bits{leading & 0x7Fu};
    result |= (data_bits << shift);
    if ((leading & 0x80u) == 0) {
      ++position;
      return result;
    }
    shift += 7;
  }
  std::unreachable();
}

void Parser::parse_function_type() {
  std::uint32_t parameter_count{decode_varuint32()};
  for (std::uint32_t i = 0; i < parameter_count; ++i) {
    std::uint8_t type_tag{forward().value_or(0)};
    assert(type_tag == 0x7f);
  }
  std::uint32_t result_count{decode_varuint32()};
  assert(result_count == 0);
}

void Parser::parse_binary() {
  std::string actual_magic{std::from_range, traverse() | std::views::take(4)};
  std::string_view expected_magic{"\0asm", 4};
  assert(actual_magic == expected_magic);

  std::string actual_version{std::from_range, traverse() | std::views::take(4)};
  std::string_view expected_version{"\1\0\0\0", 4};
  assert(actual_version == expected_version);

  std::uint8_t section_id{forward().value_or(0)};
  assert(section_id == 1);
  decode_varuint32();
  std::uint32_t type_count{decode_varuint32()};
  for (std::uint32_t i = 0; i < type_count; ++i) {
    std::uint8_t type_tag{forward().value_or(0)};
    assert(type_tag == 0x60);
    parse_function_type();
  }
  return;
}
}; // namespace Manadrain
