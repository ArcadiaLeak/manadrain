#include <cassert>
#include <inplace_vector>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"
#include "static_atoms.hpp"

namespace Manadrain {
void Language::prev() {
  assert(not backtrace.empty());
  position -= backtrace.top();
  backtrace.pop();
}

std::int32_t Language::next() {
  if (position >= text_input.size())
    return -1;
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_input.data() + position,
                           text_input.size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push(advance);
    return ch;
  }
}

void Language::backtrack(std::size_t N) {
  for (int i = 0; i < N; ++i)
    prev();
}

std::inplace_vector<char, 6> encode_ucs4(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int len = u8_uctomb(buffer.data(), cp, buffer.size());
  assert(len >= 0);
  std::inplace_vector<char, 6> encoded{};
  for (int i = 0; i < len; ++i)
    encoded.push_back(buffer[i]);
  return encoded;
}

std::generator<char32_t> Language::traverse() {
  while (1) {
    std::int32_t ahead{next()};
    if (ahead == -1)
      co_return;
    co_yield ahead;
  }
}

void Language::skip_lf() {
  std::int32_t ahead{next()};
  if (ahead == '\n')
    return;
  backtrack(ahead != -1);
}

Language::DEDUP_RET Language::deduplicate_string(std::string potential_copy) {
  auto wellknown_it = std::ranges::find(S_ATOM_ARR, potential_copy);
  if (wellknown_it != S_ATOM_ARR.end()) {
    std::size_t wellknown_idx = std::distance(S_ATOM_ARR.begin(), wellknown_it);
    return DEDUP_RET{.pool_idx = wellknown_idx, .is_wellknown = 1};
  }
  auto dedup_it = string_dedup.find(potential_copy);
  if (dedup_it != string_dedup.end())
    return DEDUP_RET{.pool_idx = dedup_it->second, .is_wellknown = 0};
  auto insertion_ret = string_dedup.insert_or_assign(std::move(potential_copy),
                                                     string_pool.size());
  string_pool.push_back(insertion_ret.first->first);
  return DEDUP_RET{.pool_idx = insertion_ret.first->second, .is_wellknown = 0};
}

Language::IDENTIFIER Language::tokenize_identifier(std::int32_t leading) {
  std::string identifier_str{std::from_range, encode_ucs4(leading)};
  for (char32_t code_point :
       std::ranges::take_while_view{traverse(), uc_is_property_xid_continue})
    identifier_str.append_range(encode_ucs4(code_point));
  DEDUP_RET dedup_str{deduplicate_string(std::move(identifier_str))};
  return IDENTIFIER{dedup_str.pool_idx, dedup_str.is_wellknown};
}

std::optional<char32_t> Language::decode_string_escape(std::int32_t ahead) {
  switch (ahead) {
  case '\'':
  case '\\':
  case '"':
  case '`':
    return ahead;
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
  case 0x2028:
  case 0x2029:
    /* ignore escaped newline sequence */
    return std::nullopt;
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  default:
    throw EXCEPTION{INVALID_BACKSLASH_ESCAPE{}};
  }
}

Language::STRING_LITERAL Language::tokenize_string_literal(char32_t separator) {
  std::string literal_str{};
  for (char32_t code_point : traverse()) {
    if (code_point == separator)
      break;
    std::int32_t ahead{next()};
    switch (ahead) {
    case '\r':
      skip_lf();
      [[fallthrough]];
    case '\n':
      if (separator == '`') {
        literal_str.push_back('\n');
        break;
      }
      [[fallthrough]];
    case -1:
      throw EXCEPTION{UNEXPECTED_STRING_END{}};
    case '\\':
      for (char32_t escaped : decode_string_escape(next()))
        literal_str.append_range(encode_ucs4(escaped));
      break;
    default:
      literal_str.append_range(encode_ucs4(ahead));
      break;
    }
  }
  DEDUP_RET dedup_str{deduplicate_string(std::move(literal_str))};
  return STRING_LITERAL{dedup_str.pool_idx, separator, dedup_str.is_wellknown};
}

Language::TOKEN Language::tokenize() { return std::monostate{}; }
} // namespace Manadrain
