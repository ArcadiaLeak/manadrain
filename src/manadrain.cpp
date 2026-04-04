#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include <format>
#include <optional>

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

namespace Manadrain {
std::optional<char32_t> ParseDriver::peek() {
  if (state.idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_GET(buffer.data(), 0, state.idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

std::optional<char32_t> ParseDriver::shift() {
  if (state.idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_NEXT(buffer.data(), state.idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

std::u32string_view ParseDriver::take(std::uint32_t count,
                                      std::u32string& buf) {
  buf.clear();
  const ParseState state_backup{state};
  for (std::uint32_t i = 0; i < count; i++) {
    std::optional ch{peek()};
    if (not ch)
      break;
    drop(1);
    buf.push_back(*ch);
  }
  state = state_backup;
  return buf;
}

void ParseDriver::drop(std::uint32_t count) {
  U8_FWD_N(buffer.data(), state.idx, buffer.size(), count);
}

bool ParseDriver::parseHex(std::uint32_t& digit) {
  std::optional uchar{shift()};
  if (not uchar)
    return 0;
  if (*uchar >= '0' && *uchar <= '9') {
    digit = *uchar - '0';
    return 1;
  }
  if (*uchar >= 'A' && *uchar <= 'F') {
    digit = *uchar - 'A' + 10;
    return 1;
  }
  if (*uchar >= 'a' && *uchar <= 'f') {
    digit = *uchar - 'a' + 10;
    return 1;
  }
  return 0;
}

bool ParseDriver::parseHex_b(std::uint32_t& digit) {
  const ParseState state_backup{state};
  bool ok = parseHex(digit);
  if (not ok)
    state = state_backup;
  return ok;
}

bool ParseDriver::parseEscape_hex(std::pair<char32_t, BAD_ESCAPE>& either) {
  std::uint32_t hex0{};
  if (not parseHex_b(hex0)) {
    either.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  std::uint32_t hex1{};
  if (not parseHex_b(hex1)) {
    either.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  either.first = (hex0 << 4) | hex1;
  return 1;
}

bool ParseDriver::parseEscape_braceSeq(
    std::pair<char32_t, BAD_ESCAPE>& either) {
  char32_t utf16_char = 0;
  while (true) {
    std::uint32_t hex{};
    if (not parseHex_b(hex)) {
      std::optional close_or_uchar{shift()};
      if (not close_or_uchar) {
        either.second = BAD_ESCAPE::MALFORMED;
        return 0;
      }
      if (close_or_uchar == '}') {
        either.first = utf16_char;
        return 1;
      }
    }
    utf16_char = (utf16_char << 4) | hex;
    if (utf16_char > 0x10FFFF) {
      either.second = BAD_ESCAPE::MALFORMED;
      return 0;
    }
  }
}

bool ParseDriver::parseEscape_fixedSeq(
    ESC_RULE esc_rule,
    std::pair<char32_t, BAD_ESCAPE>& either) {
  char32_t high_surr = 0;
  for (int i = 0; i < 4; i++) {
    std::uint32_t hex{};
    if (not parseHex_b(hex)) {
      either.second = BAD_ESCAPE::MALFORMED;
      return 0;
    }
    high_surr = (high_surr << 4) | hex;
  }

  std::u32string take_buf{};
  if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
      take(2, take_buf) == U"\\u") {
    drop(2);
    char32_t low_surr = 0;
    for (int i = 0; i < 4; i++) {
      std::uint32_t hex{};
      if (not parseHex_b(hex)) {
        either.first = high_surr;
        return 1;
      }
      low_surr = (low_surr << 4) | hex;
    }
    if (is_lo_surrogate(low_surr)) {
      either.first = from_surrogate(high_surr, low_surr);
      return 1;
    }
  }

  either.first = high_surr;
  return 1;
}

bool ParseDriver::parseEscape_uni(ESC_RULE esc_rule,
                                  std::pair<char32_t, BAD_ESCAPE>& either) {
  std::optional open_or_uchar{shift()};
  if (not open_or_uchar) {
    either.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  return open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII
             ? parseEscape_braceSeq(either)
             : parseEscape_fixedSeq(esc_rule, either);
}

bool ParseDriver::parseEscape(ESC_RULE esc_rule,
                              std::pair<char32_t, BAD_ESCAPE>& either) {
  std::optional head{shift()};
  if (not head) {
    either.second = BAD_ESCAPE::PER_SE_BACKSLASH;
    return 0;
  }
  switch (*head) {
    case 'b':
      either.first = '\b';
      return 1;
    case 'f':
      either.first = '\f';
      return 1;
    case 'n':
      either.first = '\n';
      return 1;
    case 'r':
      either.first = '\r';
      return 1;
    case 't':
      either.first = '\t';
      return 1;
    case 'v':
      either.first = '\v';
      return 1;
    case 'x':
      return parseEscape_hex(either);
    case 'u':
      return parseEscape_uni(esc_rule, either);
    case '0': {
      std::optional ahead{peek()};
      if (not ahead.transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false)) {
        either.first = 0;
        return 1;
      }
    }
      [[fallthrough]];
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      switch (esc_rule) {
        case ESC_RULE::STRING_IN_STRICT_MODE:
          either.second = BAD_ESCAPE::OCTAL_SEQ;
          return 0;

        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          either.second = BAD_ESCAPE::MALFORMED;
          return 0;

        default:
          either.first = *head - '0';
          std::optional<char32_t> ahead;
          ahead = peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            return 1;
          drop(1);
          either.first = (either.first << 3) | *ahead;

          if (either.first >= 32)
            return 1;

          ahead = peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            return 1;
          drop(1);
          either.first = (either.first << 3) | *ahead;
          return 1;
      }
    case '8':
    case '9':
      if (esc_rule == ESC_RULE::STRING_IN_STRICT_MODE ||
          esc_rule == ESC_RULE::STRING_IN_TEMPLATE) {
        either.second = BAD_ESCAPE::MALFORMED;
        return 0;
      }
      [[fallthrough]];
    default:
      either.second = BAD_ESCAPE::PER_SE_BACKSLASH;
      return 0;
  }
}

bool ParseDriver::parseEscape_b(ESC_RULE esc_rule,
                                std::pair<char32_t, BAD_ESCAPE>& either) {
  const ParseState state_backup{state};
  bool ok = parseEscape(esc_rule, either);
  if (not ok)
    state = state_backup;
  return ok;
}

int ParseDriver::parseString_escape(STRICTNESS strictness,
                                    const TokenString& token,
                                    std::pair<char32_t, BAD_STRING>& either) {
  std::optional ch{peek()};
  if (not ch) {
    either.second = BAD_STRING::UNEXPECTED_END;
    return 0;
  }
  switch (*ch) {
    case '\'':
    case '\"':
    case '\0':
    case '\\':
      drop(1);
      either.first = *ch;
      return 1;
    case '\r':
      if (peek() == '\n')
        drop(1);
      [[fallthrough]];
    case '\n':
    case 0x2028:
    case 0x2029:
      /* ignore escaped newline sequence */
      drop(1);
      either.first = *ch;
      return 2;
    default:
      ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
      if (strictness == STRICTNESS::STRICT)
        esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
      else if (token.sep == '`')
        esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
      std::pair<char32_t, BAD_ESCAPE> ch_esc{};
      if (parseEscape_b(esc_rule, ch_esc))
        ch = ch_esc.first;
      else if (ch_esc.second == BAD_ESCAPE::MALFORMED) {
        either.second = BAD_STRING::MALFORMED_SEQ_IN_ESCAPE;
        return 0;
      } else if (ch_esc.second == BAD_ESCAPE::OCTAL_SEQ) {
        either.second = BAD_STRING::OCTAL_SEQ_IN_ESCAPE;
        return 0;
      } else if (ch_esc.second == BAD_ESCAPE::PER_SE_BACKSLASH)
        /* ignore the '\' (could output a warning) */
        drop(1);
      either.first = *ch;
      return 1;
  }
}
}  // namespace Manadrain
