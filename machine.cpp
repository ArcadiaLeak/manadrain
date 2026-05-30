#include <cassert>

#include "machine.hpp"

namespace Manadrain {
std::generator<std::uint8_t> Parser::traverse_binary() {
  while (position < binary_buffer->size()) {
    co_yield (*binary_buffer)[position];
    ++position;
  }
}

void Parser::parse_binary() {
  std::string actual_magic{std::from_range,
                           traverse_binary() | std::views::take(4)};
  std::string_view expected_magic{"\0asm", 4};
  assert(actual_magic == expected_magic);
}
}; // namespace Manadrain
