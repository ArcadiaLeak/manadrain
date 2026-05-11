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
  if (position >= text_input.size()) {
    backtrace.emplace();
    return -1;
  } else {
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

Language::IDENTIFIER Language::tokenize_identifier(std::int32_t leading) {
  std::string identifier_str{std::from_range, encode_ucs4(leading)};
  for (char32_t code_point :
       std::ranges::take_while_view{traverse(), uc_is_property_xid_continue})
    identifier_str.append_range(encode_ucs4(code_point));
  auto wellknown_it = std::ranges::find(S_ATOM_ARR, identifier_str);
  if (wellknown_it != S_ATOM_ARR.end()) {
    std::size_t wellknown_idx = std::distance(S_ATOM_ARR.begin(), wellknown_it);
    return IDENTIFIER{.pool_idx = wellknown_idx, .is_wellknown = 1};
  }
  auto dedup_it = string_dedup.find(identifier_str);
  if (dedup_it != string_dedup.end())
    return IDENTIFIER{.pool_idx = dedup_it->second, .is_wellknown = 0};
  auto insertion_ret = string_dedup.insert_or_assign(std::move(identifier_str),
                                                     string_pool.size());
  string_pool.push_back(insertion_ret.first->first);
  return IDENTIFIER{.pool_idx = insertion_ret.first->second, .is_wellknown = 0};
}

Language::TOKEN Language::tokenize() { return std::monostate{}; }
} // namespace Manadrain
