#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include "manadrain.hpp"

static bool is_hi_surrogate(char32_t c) {
  return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */
}
static bool is_lo_surrogate(char32_t c) {
  return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */
}
static char32_t from_surrogate(char32_t hi, char32_t lo) {
  return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
}
static std::string codepoint_cv(char32_t ch) {
  std::uint16_t length{}, is_error{};
  std::array<char, 4> buff{};
  U8_APPEND(buff.data(), length, buff.size(), ch, is_error);
  if (is_error)
    length = 0;
  return std::string{buff.data(), length};
}
static bool is_lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

namespace Manadrain {
static const std::array reserved_arr =
    std::to_array<std::tuple<std::string_view, int, STRICTNESS>>(
        {{"var", K_TOKEN_VAR, STRICTNESS::SLOPPY},
         {"const", K_TOKEN_CONST, STRICTNESS::SLOPPY},
         {"let", K_TOKEN_LET, STRICTNESS::STRICT}});

bool TOKEN::is_pseudo_kind(int rhs_kind) {
  if (ident.has_escape)
    return 0;
  if (ident.atom_idx >= reserved_arr.size())
    return 0;
  int lhs_kind = std::get<1>(reserved_arr[ident.atom_idx]);
  if (lhs_kind == rhs_kind)
    return 1;
  return 0;
}

static int var_intro_cv(int tok_kind) {
  if (tok_kind == K_TOKEN_LET || tok_kind == K_TOKEN_CONST ||
      tok_kind == K_TOKEN_VAR)
    return tok_kind;
  return 0;
}

std::size_t ParseDriver::get_atom() {
  if (not atom_umap.contains(ch_temp)) {
    atom_deq.push_back(std::move(ch_temp));
    atom_umap[atom_deq.back()] = reserved_arr.size() + atom_deq.size() - 1;
  }
  return atom_umap[atom_deq.back()];
}

std::expected<char32_t, int> ParseDriver::next() {
  if (buffer_idx < buffer.size()) {
    UChar32 ch;
    U8_NEXT_OR_FFFD(buffer.data(), buffer_idx, buffer.size(), ch);
    ++fwd_cnt;
    return ch;
  }
  return std::unexpected{MOVE_BUFIDX::OUT_OF_RANGE};
}

std::expected<char32_t, int> ParseDriver::prev() {
  if (0 < buffer_idx) {
    UChar32 ch;
    U8_PREV_OR_FFFD(buffer.data(), 0, buffer_idx, ch);
    --fwd_cnt;
    return ch;
  }
  return std::unexpected{MOVE_BUFIDX::OUT_OF_RANGE};
}

int ParseDriver::backtrack(int N) {
  int i;
  for (i = 0; i < N; i++) {
    std::expected ch{prev()};
    if (not ch)
      break;
  }
  return i;
}

std::string ParseDriver::take(int* actual, int N) {
  std::string ret{};
  int i;
  for (i = 0; i < N; i++) {
    std::expected ch{next()};
    if (not ch)
      break;
    ret.append(codepoint_cv(*ch));
  }
  if (actual)
    *actual = i;
  return ret;
}

std::expected<std::uint32_t, int> ParseDriver::parse_hex() {
  std::expected uchar{next()};
  if (not uchar)
    return std::unexpected{uchar.error()};
  if (*uchar >= '0' && *uchar <= '9')
    return *uchar - '0';
  if (*uchar >= 'A' && *uchar <= 'F')
    return *uchar - 'A' + 10;
  if (*uchar >= 'a' && *uchar <= 'f')
    return *uchar - 'a' + 10;
  backtrack(1);
  return std::unexpected{PARSE_HEX::NOT_A_DIGIT};
}

std::expected<char32_t, int> ParseDriver::parse_hex(PARSE_ESCAPE) {
  std::expected hex0 = parse_hex();
  if (not hex0)
    return std::unexpected{hex0.error()};
  std::expected hex1 = parse_hex();
  if (not hex1) {
    backtrack(1);
    return std::unexpected{hex1.error()};
  }
  return (*hex0 << 4) | *hex1;
}

std::expected<char32_t, int> ParseDriver::parse_uni_braced(PARSE_ESCAPE) {
  reset_fwd();
  char32_t utf16_char = 0;
  while (1) {
    if (utf16_char > 0x10FFFF)
      break;
    std::expected hex = parse_hex();
    if (hex) {
      utf16_char = (utf16_char << 4) | *hex;
      continue;
    }
    std::expected closing = next();
    if (closing == '}')
      return utf16_char;
    break;
  }
  backtrack(fwd_cnt);
  return std::unexpected{PARSE_ESCAPE::MALFORMED};
}

std::expected<char32_t, int> ParseDriver::parse_uni_fixed(PARSE_ESCAPE esc) {
  char32_t high_surr{}, low_surr{};

  for (int i = 0; i < 4; i++) {
    std::expected hex = parse_hex();
    if (not hex) {
      backtrack(i);
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    }
    high_surr = (high_surr << 4) | *hex;
  }

  reset_fwd();
  if (is_hi_surrogate(high_surr) && esc.rule == ESC_RULE::REGEXP_UTF16 &&
      take(nullptr, 2) == "\\u") {
    for (int i = 0; i < 4; i++) {
      std::expected hex = parse_hex();
      if (not hex)
        goto return_high;
      low_surr = (low_surr << 4) | *hex;
    }
    goto return_low;
  }

return_high:
  backtrack(fwd_cnt);
  return high_surr;

return_low:
  if (not is_lo_surrogate(low_surr))
    goto return_high;
  return from_surrogate(high_surr, low_surr);
}

std::expected<char32_t, int> ParseDriver::parse_uni(PARSE_ESCAPE esc) {
  std::expected open_or_uchar{next()};
  if (open_or_uchar == '{') {
    if (esc.rule == ESC_RULE::REGEXP_ASCII) {
      backtrack(1);
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    } else
      return parse_uni_braced(esc);
  }
  backtrack(1);
  return parse_uni_fixed(esc);
}

bool ParseDriver::parse() {
  return 0;
}
}  // namespace Manadrain
