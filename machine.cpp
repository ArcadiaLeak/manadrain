#include <algorithm>
#include <cassert>
#include <functional>

#include "machine.hpp"

namespace Manadrain {
std::generator<std::uint8_t> Parser::traverse() {
  while (position < binary_buffer->size()) {
    co_yield (*binary_buffer)[position];
    ++position;
  }
}

std::uint8_t Parser::forward() {
  for (std::uint8_t leading : traverse() | std::views::take(1)) {
    ++position;
    return leading;
  }
  return 0;
}

std::uint32_t Parser::take_varu32() {
  std::inplace_vector<std::uint32_t, 5> pieces{};
  bool go_on{1};
  while (go_on) {
    std::uint8_t piece{forward()};
    pieces.push_back(piece & 0x7f);
    go_on = piece & 0x80;
  }
  std::uint32_t result{0};
  for (int i = 0; i < pieces.size(); ++i)
    result |= pieces[i] << i * 7;
  return result;
}

void Parser::parse_function_type() {
  std::unique_ptr function_type{std::make_unique<FunctionType>()};
  assert(forward() == 0x60);
  std::uint32_t argument_count{take_varu32()};
  auto arguments = std::ranges::take_view{traverse(), argument_count};
  for (std::uint8_t type_tag : arguments)
    function_type->arguments.push_back(type_tag);
  std::uint32_t result_count{take_varu32()};
  assert(result_count == 0 || result_count == 1);
  auto results = std::ranges::take_view{traverse(), result_count};
  for (std::uint8_t type_tag : results)
    function_type->return_type = type_tag;
  function_types.push_back(std::move(function_type));
}

void Parser::parse_import_entry() {
  std::uint32_t module_len{take_varu32()};
  std::string module_name{std::from_range,
                          traverse() | std::views::take(module_len)};
  std::uint32_t field_len{take_varu32()};
  std::string field_name{std::from_range,
                         traverse() | std::views::take(field_len)};
  std::uint8_t descriptor{forward()};
  assert(descriptor == 0);
  std::uint32_t type_index{take_varu32()};
  function_imports.push_back(std::make_unique<FunctionImport>(
      std::move(module_name), std::move(field_name), type_index));
}

void Parser::parse_binary() {
  std::string magic{std::from_range, traverse() | std::views::take(4)};
  std::string_view expected_magic{"\0asm", 4};
  assert(magic == expected_magic);

  std::string version{std::from_range, traverse() | std::views::take(4)};
  std::string_view expected_version{"\1\0\0\0", 4};
  assert(version == expected_version);

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 1);
    take_varu32();
    std::uint32_t type_count{take_varu32()};
    for (int i = 0; i < type_count; ++i)
      parse_function_type();
  }

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 2);
    take_varu32();
    std::uint32_t import_count{take_varu32()};
    for (int i = 0; i < import_count; ++i)
      parse_import_entry();
  }

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 3);
    take_varu32();
    std::uint32_t function_count{take_varu32()};
    for (int i = 0; i < function_count; ++i)
      function_declarations.push_back(forward());
  }

  {
    std::uint8_t section_id{forward()};
    return;
  }
}
}; // namespace Manadrain
