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

std::expected<char32_t, int> ParseDriver::peek() {
  if (buffer_idx >= buffer.size())
    return std::unexpected{MOVE_BUFFER::UNEXPECTED_END};
  UChar32 ch;
  U8_GET(buffer.data(), 0, buffer_idx, buffer.size(), ch);
  if (ch < 0)
    return std::unexpected{MOVE_BUFFER::ILLEGAL_UTF8};
  return ch;
}

void ParseDriver::backtrack(std::uint32_t count) {
  std::uint32_t i;
  for (i = 0; i < count; ++i) {
    std::uint32_t last_idx{buffer_idx};
    U8_BACK_N(buffer.data(), 0, buffer_idx, 1);
    if (buffer_idx == last_idx)
      break;
  }
  fwd_cnt -= i;
}

void ParseDriver::forward(std::uint32_t count) {
  std::uint32_t i;
  for (i = 0; i < count; ++i) {
    std::uint32_t last_idx{buffer_idx};
    U8_FWD_N(buffer.data(), buffer_idx, buffer.size(), 1);
    if (buffer_idx == last_idx)
      break;
  }
  fwd_cnt += i;
}

std::string_view ParseDriver::take(std::uint32_t N) {
  ch_temp.clear();
  while (N) {
    std::expected ch{peek()};
    if (not ch)
      break;
    forward(1);
    ch_temp.append(codepoint_cv(*ch));
    --N;
  }
  return ch_temp;
}

std::expected<std::uint32_t, int> ParseDriver::parse_hex(char32_t uchar) {
  if (uchar >= '0' && uchar <= '9')
    return uchar - '0';
  if (uchar >= 'A' && uchar <= 'F')
    return uchar - 'A' + 10;
  if (uchar >= 'a' && uchar <= 'f')
    return uchar - 'a' + 10;
  return std::unexpected{PARSE_HEX::ILLEGAL_DIGIT};
}

std::expected<std::uint32_t, int> ParseDriver::parse_hex() {
  return peek().and_then([this](char32_t ch) { return parse_hex(ch); });
}

std::expected<char32_t, int> ParseDriver::parse_hex(PARSE_ESCAPE) {
  std::expected hex0 = parse_hex();
  if (not hex0)
    return std::unexpected{hex0.error()};
  forward(1);

  std::expected hex1 = parse_hex();
  if (not hex1) {
    backtrack(1);
    return std::unexpected{hex1.error()};
  }
  forward(1);

  return (*hex0 << 4) | *hex1;
}

std::expected<char32_t, int> ParseDriver::parse_uni_braced(PARSE_ESCAPE) {
  char32_t utf16_char = 0;
  while (1) {
    if (utf16_char > 0x10FFFF)
      break;
    std::expected hex = parse_hex();
    if (hex) {
      forward(1);
      utf16_char = (utf16_char << 4) | *hex;
      continue;
    }
    std::expected closing = peek();
    if (closing == '}') {
      forward(1);
      return utf16_char;
    }
    break;
  }
  return std::unexpected{PARSE_ESCAPE::MALFORMED};
}

std::expected<char32_t, int> ParseDriver::parse_uni_fixed(PARSE_ESCAPE esc) {
  char32_t high_surr{}, low_surr{};

  for (int i = 0; i < 4; i++) {
    std::expected hex = parse_hex();
    if (not hex)
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    forward(1);
    high_surr = (high_surr << 4) | *hex;
  }

  reset_fwd();
  if (is_hi_surrogate(high_surr) && esc.rule == ESC_RULE::REGEXP_UTF16 &&
      take(2) == "\\u") {
    for (int i = 0; i < 4; i++) {
      std::expected hex = parse_hex();
      if (not hex)
        goto return_high;
      forward(1);
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
  std::expected open_or_uchar{peek()};
  if (open_or_uchar == '{') {
    if (esc.rule == ESC_RULE::REGEXP_ASCII)
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    else {
      forward(1);
      return parse_uni_braced(esc);
    }
  }
  return parse_uni_fixed(esc);
}

std::expected<char32_t, int> ParseDriver::parse(PARSE_ESCAPE esc) {
  std::expected ch{peek()};
  if (not ch)
    return std::unexpected{PARSE_ESCAPE::PER_SE_BACKSLASH};
  forward(1);
  switch (*ch) {
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
    case 'x':
      return parse_hex(esc);
    case 'u':
      return parse_uni(esc);
    case '0':
      if (not peek()
                  .transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false))
        return 0;
      [[fallthrough]];
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      switch (esc.rule) {
        case ESC_RULE::STRING_IN_STRICT_MODE:
          return std::unexpected{PARSE_ESCAPE::OCTAL_SEQ};

        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          return std::unexpected{PARSE_ESCAPE::MALFORMED};

        default:
          ch = *ch - '0';
          std::expected<char32_t, int> ahead;
          ahead = peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            return ch;
          forward(1);
          ch = (*ch << 3) | *ahead;

          if (*ch >= 32)
            return ch;

          ahead = peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            return ch;
          forward(1);
          return (*ch << 3) | *ahead;
      }
    case '8':
    case '9':
      if (esc.rule == ESC_RULE::STRING_IN_STRICT_MODE ||
          esc.rule == ESC_RULE::STRING_IN_TEMPLATE)
        return std::unexpected{PARSE_ESCAPE::MALFORMED};
      [[fallthrough]];
    default:
      return std::unexpected{PARSE_ESCAPE::PER_SE_BACKSLASH};
  }
}

std::size_t ParseDriver::get_atom() {
  if (not atom_umap.contains(ch_temp)) {
    atom_deq.push_back(std::move(ch_temp));
    atom_umap[atom_deq.back()] = reserved_arr.size() + atom_deq.size() - 1;
  }
  return atom_umap[atom_deq.back()];
}

bool ParseDriver::parse() {
  return 0;
}
}  // namespace Manadrain
