#include <cassert>
#include <inplace_vector>
#include <unordered_set>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_set<std::string_view> reserved_words{
    "const",  "let",    "var",   "class",    "function", "return",
    "import", "export", "from",  "as",       "default",  "undefined",
    "null",   "true",   "false", "if",       "else",     "while",
    "for",    "do",     "break", "continue", "switch",   "int",
    "long",   "uint",   "ulong", "float",    "double",   "string"};

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

std::generator<std::optional<char32_t>> Language::traverse() {
  while (1)
    co_yield forward();
}

std::generator<char> traverse_ucs4(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int advance{u8_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  for (int i = 0; i < advance; ++i)
    co_yield buffer[i];
}

void Language::backward() {
  std::optional behind{backtrace.top()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop();
}

void Language::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

bool has_code_point(std::optional<char32_t> point_opt) {
  return point_opt.has_value();
}

IDENTIFIER Language::tokenize_identifier() {
  std::string identifier_str{};
  for (char32_t leading :
       forward() | std::views::take_while(uc_is_property_xid_start)) {
    identifier_str.append_range(traverse_ucs4(leading));
    auto xid_continue_view =
        traverse() | std::views::take_while(has_code_point) | std::views::join |
        std::views::take_while(uc_is_property_xid_continue) |
        std::views::transform(traverse_ucs4) | std::views::join;
    identifier_str.append_range(xid_continue_view);
    backward();
    auto reserved_it = reserved_words.find(identifier_str);
    if (reserved_it != reserved_words.end())
      return IDENTIFIER{*reserved_it};
    auto insertion_ret = string_pool.insert(std::move(identifier_str));
    return IDENTIFIER{*insertion_ret.first};
  }
  throw LanguageError{MISSING_IDENTIFIER{}};
}
} // namespace Manadrain
