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
  function_entries.push_back(
      std::make_unique<FunctionEntry>(function_types[type_index].get()));
  function_imports.push_back(std::make_unique<FunctionImport>(
      std::move(module_name), std::move(field_name),
      function_entries.back().get()));
}

void Parser::parse_table_entry() {
  std::uint8_t element_type{forward()};
  assert(element_type == 0x70);
  std::uint32_t has_maximum{take_varu32()};
  function_tables.push_back(std::make_unique<FunctionTable>(
      take_varu32(), has_maximum ? take_varu32() : 0));
}

void Parser::parse_function_entry() {
  function_entries.push_back(
      std::make_unique<FunctionEntry>(function_types[forward()].get()));
}

void Parser::parse_memory_entry() {
  std::uint32_t has_maximum{take_varu32()};
  memory_entries.push_back(std::make_unique<MemoryEntry>(
      take_varu32(), has_maximum ? take_varu32() : 0));
}

void Parser::parse_global_entry() {
  GlobalEntry global_entry{};
  global_entry.data_type = forward();
  global_entry.allow_mut = forward();
  auto match_opcode_end = [](std::uint8_t opcode) { return opcode != 0x0b; };
  global_entry.init = {std::from_range,
                       traverse() | std::views::take_while(match_opcode_end)};
  ++position;
  global_entries.push_back(
      std::make_unique<GlobalEntry>(std::move(global_entry)));
}

void Parser::parse_export_entry() {
  std::uint32_t export_len{take_varu32()};
  std::string export_name{std::from_range,
                          traverse() | std::views::take(export_len)};
  std::uint8_t kind{forward()};
  std::uint32_t entry_index{take_varu32()};
  switch (kind) {
  case 0:
    function_exports.push_back(std::make_unique<FunctionExport>(
        export_name, function_entries[entry_index].get()));
    break;
  case 1:
    table_exports.push_back(std::make_unique<TableExport>(
        export_name, function_tables[entry_index].get()));
    break;
  case 2:
    memory_exports.push_back(std::make_unique<MemoryExport>(
        export_name, memory_entries[entry_index].get()));
    break;
  default:
    std::unreachable();
  }
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
      parse_function_entry();
  }

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 4);
    take_varu32();
    std::uint32_t table_count{take_varu32()};
    for (int i = 0; i < table_count; ++i)
      parse_table_entry();
  }

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 5);
    take_varu32();
    std::uint32_t memory_count{take_varu32()};
    for (int i = 0; i < memory_count; ++i)
      parse_memory_entry();
  }

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 6);
    take_varu32();
    std::uint32_t global_count{take_varu32()};
    for (int i = 0; i < global_count; ++i)
      parse_global_entry();
  }

  {
    std::uint8_t section_id{forward()};
    assert(section_id == 7);
    take_varu32();
    std::uint32_t export_count{take_varu32()};
    for (int i = 0; i < export_count; ++i)
      parse_export_entry();
  }
}
}; // namespace Manadrain
