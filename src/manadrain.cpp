#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include <format>
#include <optional>
#include <unordered_map>

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
static std::string_view codepoint_cv(char32_t ch,
                                     std::array<char, 4>& cp_storage) {
  std::uint32_t len{}, err{};
  cp_storage = {};
  U8_APPEND(cp_storage.data(), len, cp_storage.size(), ch, err);
  if (err)
    return {};
  return std::string_view{cp_storage.data(), len};
}
static bool is_newline(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

namespace Manadrain {
bool ParseDriver::parseKeyword(TOKEN_TYPE& tok_type, TOKEN_IDENT& tok_ident) {
  static const std::unordered_map<std::string_view, TOKEN_TYPE> kw_sloppy{
      {"var", TOK_VAR{}}, {"const", TOK_CONST{}}};
  static const std::unordered_map<std::string_view, TOKEN_TYPE> kw_strict{
      {"let", TOK_LET{}}};

  auto it = kw_sloppy.find(tok_ident.val);
  if (it != kw_sloppy.end())
    goto found;
  if (state.strictness == STRICTNESS::STRICT) {
    it = kw_strict.find(tok_ident.val);
    if (it != kw_strict.end())
      goto found;
  }
  return 0;

found:
  if (not tok_ident.has_escape)
    tok_type = it->second;
  else {
    tok_ident.is_reserved = 1;
    tok_type = TOK_IDENT{};
  }
  return 1;
}

template <std::size_t N>
std::u32string_view take(ParseDriver& driver, std::array<char32_t, N>& tbuff) {
  const ParseState state_backup{driver.state};
  tbuff = {};
  std::uint32_t i{};
  while (i < N) {
    std::optional ch{driver.peek()};
    if (not ch)
      break;
    driver.drop(1);
    tbuff[i++] = *ch;
  }
  driver.state = state_backup;
  return {tbuff.data(), i};
}

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

void ParseDriver::drop(std::uint32_t count) {
  U8_FWD_N(buffer.data(), state.idx, buffer.size(), count);
}

bool ParseDriver::parseHex_dang(std::uint32_t& digit) {
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

bool ParseDriver::parseHex(std::uint32_t& digit) {
  const ParseState state_backup{state};
  bool ok = parseHex_dang(digit);
  if (not ok)
    state = state_backup;
  return ok;
}

bool ParseDriver::parseEscape_hex(std::pair<char32_t, BAD_ESCAPE>& either) {
  std::uint32_t hex0{};
  if (not parseHex(hex0)) {
    either.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  std::uint32_t hex1{};
  if (not parseHex(hex1)) {
    either.second = BAD_ESCAPE::MALFORMED;
    return 0;
  }
  either.first = (hex0 << 4) | hex1;
  return 1;
}

bool ParseDriver::parseEscape_braceSeq(
    std::pair<char32_t, BAD_ESCAPE>& either) {
  char32_t utf16_char = 0;
  while (1) {
    std::uint32_t hex{};
    if (not parseHex(hex)) {
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
    if (not parseHex(hex)) {
      either.second = BAD_ESCAPE::MALFORMED;
      return 0;
    }
    high_surr = (high_surr << 4) | hex;
  }

  std::array<char32_t, 2> tbuff{};
  if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
      take(*this, tbuff) == U"\\u") {
    drop(2);
    char32_t low_surr = 0;
    for (int i = 0; i < 4; i++) {
      std::uint32_t hex{};
      if (not parseHex(hex)) {
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

bool ParseDriver::parseEscape_dang(ESC_RULE esc_rule,
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

bool ParseDriver::parseEscape(ESC_RULE esc_rule,
                              std::pair<char32_t, BAD_ESCAPE>& either) {
  const ParseState state_backup{state};
  bool ok = parseEscape_dang(esc_rule, either);
  if (not ok)
    state = state_backup;
  return ok;
}

int ParseDriver::parseString_escSeq_dang(
    char32_t sep,
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
      if (state.strictness == STRICTNESS::STRICT)
        esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
      else if (sep == '`')
        esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
      std::pair<char32_t, BAD_ESCAPE> ch_esc{};
      if (parseEscape(esc_rule, ch_esc))
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

int ParseDriver::parseString_escSeq(char32_t sep,
                                    std::pair<char32_t, BAD_STRING>& either) {
  const ParseState state_backup{state};
  int ok = parseString_escSeq_dang(sep, either);
  if (not ok)
    state = state_backup;
  return ok;
}

bool ParseDriver::parseString(TOKEN_STRING& token, BAD_STRING& err) {
  token.sep = *shift();
  while (1) {
    std::optional ch = peek();
    if (not ch) {
      err = BAD_STRING::UNEXPECTED_END;
      return 0;
    }

    if (token.sep == '`') {
      if (*ch == '\r') {
        if (peek() == '\n')
          drop(1);
        ch = '\n';
      }
    } else if (*ch == '\r' || *ch == '\n') {
      err = BAD_STRING::UNEXPECTED_END;
      return 0;
    }

    drop(1);
    if (*ch == token.sep)
      return 1;

    if (*ch == '$' && peek() == '{' && token.sep == '`') {
      drop(1);
      return 1;
    }

    if (*ch == '\\') {
      std::pair<char32_t, BAD_STRING> ch_esc{};
      int ok = parseString_escSeq(token.sep, ch_esc);
      if (not ok) {
        err = ch_esc.second;
        return 0;
      } else if (ok == 2)
        continue;
      else
        ch = ch_esc.first;
    }

    std::array<char, 4> cp_storage{};
    token.val.append(codepoint_cv(*ch, cp_storage));
  }
}

bool ParseDriver::parseIdent_uchar(TOKEN_IDENT& ident, bool beginning) {
  std::optional ch{peek()};
  if (not ch)
    return 0;
  drop(1);

  if (*ch == '\\' && peek() == 'u') {
    std::pair<char32_t, BAD_ESCAPE> ret_esc{};
    if (not parseEscape(ESC_RULE::IDENTIFIER, ret_esc))
      return 0;
    ch = ret_esc.first;
    ident.has_escape = true;
  }

  UProperty must_be = beginning ? UCHAR_XID_START : UCHAR_XID_CONTINUE;
  if (not u_hasBinaryProperty(*ch, must_be))
    return 0;

  std::array<char, 4> cp_storage{};
  ident.val.append(codepoint_cv(*ch, cp_storage));
  return 1;
}

bool ParseDriver::parseIdent(TOKEN_IDENT& ident, bool is_private) {
  ParseState state_backup{state};

  char32_t ch{};
  if (not parseIdent_uchar(ident, 1)) {
    state = state_backup;
    return 0;
  }

  if (is_private)
    ident.val = '#' + ident.val;

  while (1) {
    state_backup = state;
    if (not parseIdent_uchar(ident, 0)) {
      state = state_backup;
      return 1;
    }
  }
}

bool ParseDriver::parseToken_dang(TOKEN& token, TOKEN_ERROR& err) {
  std::array<char32_t, 2> tbuff{};
  while (1) {
    std::optional ch{peek()};
    if (not ch) {
      token.type = TOK_EOF{};
      return 1;
    }

    if (take(*this, tbuff) == U"\r\n") {
      drop(2);
      token.newline_seen = 1;
      continue;
    }

    if (is_newline(*ch)) {
      drop(1);
      token.newline_seen = 1;
      continue;
    }

    if (u_isWhitespace(*ch)) {
      drop(1);
      continue;
    }

    if (take(*this, tbuff) == U"//") {
      while (1) {
        ch = peek();
        if (not ch || is_newline(*ch))
          break;
        drop(1);
      }
      continue;
    }

    if (take(*this, tbuff) == U"/*") {
      drop(2);
      while (1) {
        std::u32string_view tview{take(*this, tbuff)};
        if (tview.empty()) {
          err.type = TOKEN_ERROR::TYPE::ERR_GENERAL;
          err.general = BAD_TOKEN::UNEXPECTED_COMMENT_END;
          return 0;
        }
        if (tview == U"*/") {
          drop(2);
          break;
        }
        if (is_newline(tview.front()))
          token.newline_seen = 1;
        drop(1);
      }
      continue;
    }

    if (parseIdent(token.ident, 0)) {
      if (not parseKeyword(token.type, token.ident))
        token.type = TOK_IDENT{};
      return 1;
    }

    if (*ch == '\'' || *ch == '"' || *ch == '`') {
      if (parseString(token.str, err.str)) {
        token.type = TOK_STRING{};
        return 1;
      }
      err.type = TOKEN_ERROR::TYPE::ERR_STRING;
      return 0;
    }

    return 0;
  }
}

bool ParseDriver::parseToken(TOKEN& token, TOKEN_ERROR& err) {
  const ParseState state_backup{state};
  int ok = parseToken_dang(token, err);
  if (not ok)
    state = state_backup;
  return ok;
}
}  // namespace Manadrain
