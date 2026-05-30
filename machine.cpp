#include <bitset>
#include <cassert>
#include <functional>

#include "machine.hpp"

namespace Manadrain {
std::generator<std::uint8_t> Parser::head_traverse() {
  while (position < binary_buffer->size()) {
    std::uint8_t leading{(*binary_buffer)[position]};
    ++position;
    co_yield leading;
  }
}

std::generator<std::uint8_t> Parser::tail_traverse() {
  while (position < binary_buffer->size()) {
    co_yield (*binary_buffer)[position];
    ++position;
  }
}

std::optional<std::uint8_t> Parser::forward() {
  for (std::uint8_t leading : head_traverse() | std::views::take(1))
    return leading;
  return std::nullopt;
}

std::uint32_t Parser::decode_varuint32() {
  std::uint64_t result{};
  unsigned piece_shift{};
  for (std::bitset<8> piece : head_traverse()) {
    bool ahead{piece[7]};
    piece[7] = 0;
    result |= piece.to_ulong() << piece_shift;
    if (not ahead)
      break;
    piece_shift += 7;
  }
  return static_cast<std::uint32_t>(result);
}

void Parser::parse_function_type() {
  std::unique_ptr definition{std::make_unique<FunctionDefinition>()};
  std::uint32_t parameter_count{decode_varuint32()};
  for (std::uint8_t type_tag :
       tail_traverse() | std::views::take(parameter_count))
    definition->arguments.push_back(type_tag);
  std::uint32_t result_count{decode_varuint32()};
  assert(result_count == 0 || result_count == 1);
  for (std::uint8_t type_tag : tail_traverse() | std::views::take(result_count))
    definition->return_type = type_tag;
  function_definitions.push_back(std::move(definition));
}

void Parser::parse_binary() {
  std::string actual_magic{std::from_range,
                           tail_traverse() | std::views::take(4)};
  std::string_view expected_magic{"\0asm", 4};
  assert(actual_magic == expected_magic);

  std::string actual_version{std::from_range,
                             tail_traverse() | std::views::take(4)};
  std::string_view expected_version{"\1\0\0\0", 4};
  assert(actual_version == expected_version);

  auto decode_custom = [&]() -> void { std::unreachable(); };
  auto decode_type = [&]() -> void {
    std::uint32_t type_count{decode_varuint32()};
    for (std::uint8_t type_tag :
         head_traverse() | std::views::take(type_count)) {
      assert(type_tag == 0x60);
      parse_function_type();
    }
  };
  auto decode_import = [&]() -> void { assert(0); };

  using SectionReader = std::move_only_function<void() const>;
  std::array section_readers{std::to_array<SectionReader>({
      decode_custom,
      decode_type,
      decode_import,
  })};

  for (std::uint8_t section_id : head_traverse()) {
    decode_varuint32();
    section_readers.at(section_id)();
  }
}
}; // namespace Manadrain
