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

EXPECT<char32_t> ParseDriver::next(int* advance) {
  if (buffer_idx < buffer.size()) {
    UChar32 ch;
    U8_NEXT_OR_FFFD(buffer.data(), buffer_idx, buffer.size(), ch);
    if (advance)
      ++(*advance);
    return ch;
  }
  return std::unexpected{MOVE_BUFIDX::OUT_OF_RANGE};
}

EXPECT<char32_t> ParseDriver::prev(int* advance) {
  if (0 < buffer_idx) {
    UChar32 ch;
    U8_PREV_OR_FFFD(buffer.data(), 0, buffer_idx, ch);
    if (advance)
      --(*advance);
    return ch;
  }
  return std::unexpected{MOVE_BUFIDX::OUT_OF_RANGE};
}

int ParseDriver::backtrack(int* advance, int N) {
  int i = 0;
  while (N + i > 0) {
    std::expected ch{prev(&i)};
    if (not ch)
      break;
  }
  if (advance)
    *advance += i;
  return i;
}

std::string ParseDriver::take(int* advance, int N) {
  std::string ret{};
  int i = 0;
  while (i < N) {
    std::expected ch{next(&i)};
    if (not ch)
      break;
    ret.append(codepoint_cv(*ch));
  }
  if (advance)
    *advance += i;
  return ret;
}

EXPECT<char32_t> ParseDriver::parse_hex(int* advance) {
  std::expected uchar{next(advance)};
  if (not uchar)
    return uchar;
  if (*uchar >= '0' && *uchar <= '9')
    return *uchar - '0';
  if (*uchar >= 'A' && *uchar <= 'F')
    return *uchar - 'A' + 10;
  if (*uchar >= 'a' && *uchar <= 'f')
    return *uchar - 'a' + 10;
  backtrack(advance, 1);
  return std::unexpected{PARSE_DIGIT::NOT_A_DIGIT};
}

EXPECT<char32_t> ParseDriver::parse_hex(PARSE_ESCAPE) {
  std::expected hex0 = parse_hex(nullptr);
  if (not hex0)
    return hex0;
  std::expected hex1 = parse_hex(nullptr);
  if (not hex1) {
    backtrack(nullptr, 1);
    return hex1;
  }
  return (*hex0 << 4) | *hex1;
}

EXPECT<char32_t> ParseDriver::parse_uni_braced(PARSE_ESCAPE) {
  int fwd_cnt = 0;
  char32_t utf16_char = 0;
  while (1) {
    if (utf16_char > 0x10FFFF)
      break;
    std::expected hex = parse_hex(&fwd_cnt);
    if (hex) {
      utf16_char = (utf16_char << 4) | *hex;
      continue;
    }
    std::expected closing = next(&fwd_cnt);
    if (closing == '}')
      return utf16_char;
    break;
  }
  backtrack(nullptr, fwd_cnt);
  return std::unexpected{PARSE_ESCAPE::MALFORMED};
}

EXPECT<char32_t> ParseDriver::parse_uni_fixed(PARSE_ESCAPE esc) {
  char32_t high_surr{}, low_surr{};

  for (int i = 0; i < 4; i++) {
    std::expected hex = parse_hex(nullptr);
    if (not hex) {
      backtrack(nullptr, i);
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    }
    high_surr = (high_surr << 4) | *hex;
  }

  int fwd_cnt = 0;
  if (is_hi_surrogate(high_surr) && esc.rule == ESC_RULE::REGEXP_UTF16 &&
      take(&fwd_cnt, 2) == "\\u") {
    for (int i = 0; i < 4; i++) {
      std::expected hex = parse_hex(&fwd_cnt);
      if (not hex)
        goto return_high;
      low_surr = (low_surr << 4) | *hex;
    }
    goto return_low;
  }

return_high:
  backtrack(nullptr, fwd_cnt);
  return high_surr;

return_low:
  if (not is_lo_surrogate(low_surr))
    goto return_high;
  return from_surrogate(high_surr, low_surr);
}

EXPECT<char32_t> ParseDriver::parse_uni(PARSE_ESCAPE esc) {
  std::expected open_or_uchar{next(nullptr)};
  if (open_or_uchar == '{') {
    if (esc.rule == ESC_RULE::REGEXP_ASCII) {
      backtrack(nullptr, 1);
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    } else
      return parse_uni_braced(esc);
  }
  backtrack(nullptr, 1);
  return parse_uni_fixed(esc);
}

EXPECT<char32_t> ParseDriver::parse_octal(PARSE_ESCAPE esc) {
  int fwd_cnt = 0;
  std::expected ahead =
      next(&fwd_cnt).and_then([](char32_t ahead_digit) -> EXPECT<char32_t> {
        if (ahead_digit > 7)
          return std::unexpected{PARSE_DIGIT::NOT_A_DIGIT};
        return ahead_digit - '0';
      });
  if (not ahead)
    backtrack(nullptr, fwd_cnt);
  return ahead;
}

EXPECT<char32_t> ParseDriver::parse(PARSE_ESCAPE esc) {
  int fwd_cnt = 0;
  std::expected ch{next(nullptr)};
  if (not ch)
    return std::unexpected{PARSE_ESCAPE::PER_SE_BACKSLASH};
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
      if (not next(&fwd_cnt)
                  .transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false)) {
        backtrack(nullptr, fwd_cnt);
        return 0;
      }
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
          ch = ch.transform([](char32_t digit) { return digit - '0'; });
          ch = ch.and_then([&](char32_t digit) {
            return parse_octal(esc).transform(
                [digit](char32_t ahead) { return (digit << 3) | ahead; });
          });
          if (not ch || *ch >= 32)
            return ch;
          ch = ch.and_then([&](char32_t digit) {
            return parse_octal(esc).transform(
                [digit](char32_t ahead) { return (digit << 3) | ahead; });
          });
          return ch;
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

bool ParseDriver::parse() {
  return 0;
}
}  // namespace Manadrain
