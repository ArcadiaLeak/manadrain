#include <cassert>
#include <inplace_vector>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"
#include "static_atoms.hpp"

namespace Manadrain {
std::optional<char32_t> Language::forward() {
  if (position >= text_input.size())
    backtrace.emplace();
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_input.data() + position,
                           text_input.size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push(ch);
  }
  return backtrace.top();
}

std::generator<char32_t> Language::traverse() {
  while (1) {
    std::optional<char32_t> ahead{forward()};
    if (not ahead)
      co_return;
    co_yield *ahead;
  }
}

std::inplace_vector<char, 6> encode_ucs4(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int advance{u8_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  std::inplace_vector<char, 6> encoded{};
  for (int i = 0; i < advance; ++i)
    encoded.push_back(buffer[i]);
  return encoded;
}

std::optional<char32_t> Language::backtrack() {
  std::optional<char32_t> behind{backtrace.top()};
  if (behind)
    position -= encode_ucs4(*behind).size();
  backtrace.pop();
  return behind;
}
} // namespace Manadrain
