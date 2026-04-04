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
                                      std::u32string& storage) {
  storage.clear();
  const ParseState state_backup{state};
  for (std::uint32_t i = 0; i < count; i++) {
    std::optional ch{peek()};
    if (not ch)
      break;
    drop(1);
    storage.push_back(*ch);
  }
  state = state_backup;
  return storage;
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
      if (strictness == STRICTNESS::STRICT)
        esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
      else if (sep == '`')
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

int ParseDriver::parseString_escape_b(STRICTNESS strictness,
                                      char32_t sep,
                                      std::pair<char32_t, BAD_STRING>& either) {
  const ParseState state_backup{state};
  int ok = parseString_escape(strictness, sep, either);
  if (not ok)
    state = state_backup;
  return ok;
}

bool ParseDriver::parseString(STRICTNESS strictness,
                              TOKEN_STRING& token,
                              BAD_STRING& err) {
  std::optional ch{shift()};
  if (not ch || !(*ch == '\'' || *ch == '"' || *ch == '`')) {
    err = BAD_STRING::MISMATCH;
    return 0;
  }
  token.sep = *ch;

  while (true) {
    ch = peek();
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
      int ok = parseString_escape_b(strictness, token.sep, ch_esc);
      if (not ok) {
        err = ch_esc.second;
        return 0;
      } else if (ok == 2)
        continue;
      else
        ch = ch_esc.first;
    }

    std::array<char, 4> cp_storage;
    token.content.append(codepoint_cv(*ch, cp_storage));
  }
}

bool ParseDriver::parseWord_idContinue(char32_t& ch_esc, TOKEN_WORD& word) {
  std::optional ch{peek()};
  if (not ch)
    return 0;
  drop(1);
  if (*ch == '\\' && peek() == 'u') {
    std::pair<char32_t, BAD_ESCAPE> ret_esc{};
    if (not parseEscape_b(ESC_RULE::IDENTIFIER, ret_esc))
      return 0;
    ch = ret_esc.first;
    word.ident_has_escape = true;
  }
  if (not u_hasBinaryProperty(*ch, UCHAR_XID_CONTINUE))
    return 0;
  ch_esc = *ch;
  return 1;
}

bool ParseDriver::parseWord(bool is_private, TOKEN_WORD& word) {
  std::optional id_start{peek()};
  if (not id_start || not u_hasBinaryProperty(*id_start, UCHAR_XID_START))
    return 0;
  drop(1);
  word.is_private = is_private;
  if (is_private)
    word.content.push_back('#');
  std::array<char, 4> cp_storage;
  word.content.append(codepoint_cv(*id_start, cp_storage));

  while (true) {
    const ParseState state_backup{state};
    char32_t ch{};
    if (not parseWord_idContinue(ch, word)) {
      state = state_backup;
      return 1;
    }
    word.content.append(codepoint_cv(ch, cp_storage));
  }
}

std::optional<std::monostate> ParseDriver::parseSpace(bool& newline_seen) {
  const ParseState b{state};
  std::u32string take_buf{};

  while (true) {
    std::optional ch{peek()};

    if (*ch == '\r' || *ch == '\n' || *ch == 0x2028 || *ch == 0x2029) {
      drop(1);
      newline_seen = 1;
      continue;
    }

    if (u_isWhitespace(*ch)) {
      drop(1);
      continue;
    }

    if (take(2, take_buf) == U"//") {
      while (true) {
        ch = peek();
        if (not ch || *ch == '\r' || *ch == '\n' || *ch == 0x2028 ||
            *ch == 0x2029)
          break;
        drop(1);
      }
      continue;
    }

    if (take(2, take_buf) == U"/*") {
      drop(2);
      while (true) {
        take(2, take_buf);
        if (take_buf.empty() || take_buf == U"*/") {
          drop(2);
          break;
        }
        drop(1);
      }
      continue;
    }

    if (b.idx < state.idx)
      return std::monostate{};
    return std::nullopt;
  }
}

std::optional<std::monostate> ParseDriver::parseSpace() {
  bool newline_seen{};
  return parseSpace(newline_seen);
}

bool ParseDriver::parseVardecl(NODE_VARDECL& vardecl) {
  TOKEN_WORD var_kind_s{};
  if (not parseWord(false, var_kind_s))
    return 0;

  static const std::unordered_map<std::string_view, NODE_VARDECL::KIND>
      var_kind_m = {{"let", NODE_VARDECL::KIND_LET{}},
                    {"const", NODE_VARDECL::KIND_CONST{}},
                    {"var", NODE_VARDECL::KIND_VAR{}}};
  auto var_kind_it = var_kind_m.find(var_kind_s.content);
  if (var_kind_it == var_kind_m.end())
    return 0;
  vardecl.kind = var_kind_it->second;

  if (not parseSpace()
              .transform([&](auto) { return parseWord(false, vardecl.name); })
              .value_or(false))
    return 0;

  const ParseState state_backup{state};
  std::optional ch = parseSpace().and_then([this](auto) { return shift(); });
  if (ch != '=')
    state = state_backup;
  else {
    BAD_STRING string_err;
    parseSpace().transform([&](auto) {
      return parseString(STRICTNESS::SLOPPY, vardecl.init, string_err);
    });
  }

  bool newline_seen{};
  ch = parseSpace(newline_seen).and_then([this](auto) { return peek(); });
  if (not ch || *ch == ';' || *ch == '}' || newline_seen)
    return 1;

  return 0;
}

bool ParseDriver::parseStatement() {
  const ParseState state_backup{state};
  parseSpace();

  NODE_VARDECL vardecl{};
  if (not parseVardecl(vardecl)) {
    state = state_backup;
    return 0;
  }

  return 1;
}
}  // namespace Manadrain
