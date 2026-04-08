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
static std::string_view codepoint_cv(char32_t ch,
                                     std::array<char, 4>& cp_storage) {
  std::uint32_t len{}, err{};
  cp_storage = {};
  U8_APPEND(cp_storage.data(), len, cp_storage.size(), ch, err);
  if (err)
    return {};
  return std::string_view{cp_storage.data(), len};
}
static bool is_lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

namespace Manadrain {
static const std::array reserved_arr =
    std::to_array<std::tuple<std::string_view, TOKEN_TYPE, STRICTNESS>>(
        {{"var", TOKEN_TYPE::T_VAR, STRICTNESS::SLOPPY},
         {"const", TOKEN_TYPE::T_CONST, STRICTNESS::SLOPPY},
         {"let", TOKEN_TYPE::T_LET, STRICTNESS::STRICT}});

bool TOKEN::is_pseudo_keyword(TOKEN_TYPE tok_type) {
  if (ident.has_escape)
    return 0;
  if (ident.atom_idx >= reserved_arr.size())
    return 0;
  if (std::get<1>(reserved_arr[ident.atom_idx]) == tok_type)
    return 1;
  return 0;
}

bool TOKEN::is_vardecl_intro() {
  switch (type) {
    case TOKEN_TYPE::T_IDENT:
      if (is_pseudo_keyword(TOKEN_TYPE::T_LET)) {
        type = TOKEN_TYPE::T_LET;
        return 1;
      }
      return 0;
    case TOKEN_TYPE::T_CONST:
    case TOKEN_TYPE::T_LET:
    case TOKEN_TYPE::T_VAR:
      return 1;
    default:
      return 0;
  }
}

std::optional<char32_t> ParseDriver::peek() {
  if (buffer_idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_GET(buffer.data(), 0, buffer_idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

std::optional<char32_t> ParseDriver::shift() {
  if (buffer_idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_NEXT(buffer.data(), buffer_idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

void ParseDriver::drop(std::uint32_t count) {
  U8_FWD_N(buffer.data(), buffer_idx, buffer.size(), count);
}

template <std::size_t N>
struct TAKE {
  std::array<char32_t, N> ch_arr;
  std::uint32_t len;

  std::u32string_view sv() { return {ch_arr.data(), len}; }

  CMD_EXIT operator()(ParseDriver& driver) {
    *this = {};
    while (len < N) {
      std::optional ch{driver.peek()};
      if (not ch)
        break;
      driver.drop(1);
      ch_arr[len++] = *ch;
    }
    return PARSE_OK::REVERT;
  }
};

struct PARSE_HEX {
  std::uint32_t digit;

  CMD_EXIT operator()(ParseDriver& driver) {
    std::optional uchar{driver.shift()};
    if (not uchar)
      return PARSE_ERR{};
    if (*uchar >= '0' && *uchar <= '9') {
      digit = *uchar - '0';
      return PARSE_OK::COMMIT;
    }
    if (*uchar >= 'A' && *uchar <= 'F') {
      digit = *uchar - 'A' + 10;
      return PARSE_OK::COMMIT;
    }
    if (*uchar >= 'a' && *uchar <= 'f') {
      digit = *uchar - 'a' + 10;
      return PARSE_OK::COMMIT;
    }
    return PARSE_ERR{};
  }
};

enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

struct PARSE_ESCAPE {
  ESC_RULE esc_rule;
  char32_t ch_esc;

  CMD_EXIT parseHexSeq(ParseDriver& driver) {
    PARSE_HEX hex0{};
    if (driver.call_command(hex0))
      return PARSE_ERR{BAD_ESCAPE::MALFORMED};
    PARSE_HEX hex1{};
    if (driver.call_command(hex1))
      return PARSE_ERR{BAD_ESCAPE::MALFORMED};
    ch_esc = (hex0.digit << 4) | hex1.digit;
    return PARSE_OK::COMMIT;
  }

  CMD_EXIT parseUniBraced(ParseDriver& driver) {
    char32_t utf16_char = 0;
    while (1) {
      PARSE_HEX hex{};
      if (driver.call_command(hex)) {
        std::optional close_or_uchar{driver.shift()};
        if (not close_or_uchar)
          return PARSE_ERR{BAD_ESCAPE::MALFORMED};
        if (close_or_uchar == '}') {
          ch_esc = utf16_char;
          return PARSE_OK::COMMIT;
        }
      }
      utf16_char = (utf16_char << 4) | hex.digit;
      if (utf16_char > 0x10FFFF)
        return PARSE_ERR{BAD_ESCAPE::MALFORMED};
    }
  }

  CMD_EXIT parseUniFixed(ParseDriver& driver) {
    char32_t high_surr = 0;
    for (int i = 0; i < 4; i++) {
      PARSE_HEX hex{};
      if (driver.call_command(hex))
        return PARSE_ERR{BAD_ESCAPE::MALFORMED};
      high_surr = (high_surr << 4) | hex.digit;
    }
    if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16) {
      TAKE<2> tcmd{};
      driver.call_command(tcmd);
      if (tcmd.sv() == U"\\u") {
        driver.drop(2);
        char32_t low_surr = 0;
        for (int i = 0; i < 4; i++) {
          PARSE_HEX hex{};
          if (driver.call_command(hex)) {
            ch_esc = high_surr;
            return PARSE_OK::COMMIT;
          }
          low_surr = (low_surr << 4) | hex.digit;
        }
        if (is_lo_surrogate(low_surr)) {
          ch_esc = from_surrogate(high_surr, low_surr);
          return PARSE_OK::COMMIT;
        }
      }
    }
    ch_esc = high_surr;
    return PARSE_OK::COMMIT;
  }

  CMD_EXIT parseUniSeq(ParseDriver& driver) {
    std::optional open_or_uchar{driver.shift()};
    if (not open_or_uchar)
      return PARSE_ERR{BAD_ESCAPE::MALFORMED};
    return open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII
               ? parseUniBraced(driver)
               : parseUniFixed(driver);
  }

  CMD_EXIT operator()(ParseDriver& driver) {
    std::optional head{driver.shift()};
    if (not head)
      return PARSE_ERR{BAD_ESCAPE::PER_SE_BACKSLASH};
    switch (*head) {
      case 'b':
        ch_esc = '\b';
        return PARSE_OK::COMMIT;
      case 'f':
        ch_esc = '\f';
        return PARSE_OK::COMMIT;
      case 'n':
        ch_esc = '\n';
        return PARSE_OK::COMMIT;
      case 'r':
        ch_esc = '\r';
        return PARSE_OK::COMMIT;
      case 't':
        ch_esc = '\t';
        return PARSE_OK::COMMIT;
      case 'v':
        ch_esc = '\v';
        return PARSE_OK::COMMIT;
      case 'x':
        return parseHexSeq(driver);
      case 'u':
        return parseUniSeq(driver);
      case '0': {
        std::optional ahead{driver.peek()};
        if (not ahead.transform([](char32_t uch) { return std::isdigit(uch); })
                    .value_or(false)) {
          ch_esc = 0;
          return PARSE_OK::COMMIT;
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
            return PARSE_ERR{BAD_ESCAPE::OCTAL_SEQ};

          case ESC_RULE::STRING_IN_TEMPLATE:
          case ESC_RULE::REGEXP_UTF16:
            return PARSE_ERR{BAD_ESCAPE::MALFORMED};

          default:
            ch_esc = *head - '0';
            std::optional<char32_t> ahead;
            ahead = driver.peek().transform(
                [](char32_t ahead_digit) { return ahead_digit - '0'; });
            if (not ahead || *ahead > 7)
              return PARSE_OK::COMMIT;
            driver.drop(1);
            ch_esc = (ch_esc << 3) | *ahead;

            if (ch_esc >= 32)
              return PARSE_OK::COMMIT;

            ahead = driver.peek().transform(
                [](char32_t ahead_digit) { return ahead_digit - '0'; });
            if (not ahead || *ahead > 7)
              return PARSE_OK::COMMIT;
            driver.drop(1);
            ch_esc = (ch_esc << 3) | *ahead;
            return PARSE_OK::COMMIT;
        }
      case '8':
      case '9':
        if (esc_rule == ESC_RULE::STRING_IN_STRICT_MODE ||
            esc_rule == ESC_RULE::STRING_IN_TEMPLATE)
          return PARSE_ERR{BAD_ESCAPE::MALFORMED};
        [[fallthrough]];
      default:
        return PARSE_ERR{BAD_ESCAPE::PER_SE_BACKSLASH};
    }
  }
};

struct PARSE_STRING_ESCSEQ {
  char32_t ch_ret;
  bool must_continue;

  CMD_EXIT operator()(ParseDriver& driver) {
    std::optional ch{driver.peek()};
    if (not ch)
      return PARSE_ERR{BAD_STRING::UNEXPECTED_END};
    switch (*ch) {
      case '\'':
      case '\"':
      case '\0':
      case '\\':
        driver.drop(1);
        ch_ret = *ch;
        return PARSE_OK::COMMIT;
      case '\r':
        if (driver.peek() == '\n')
          driver.drop(1);
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        /* ignore escaped newline sequence */
        driver.drop(1);
        must_continue = 1;
        return PARSE_OK::COMMIT;
      default:
        ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
        if (driver.strictness == STRICTNESS::STRICT)
          esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
        else if (driver.token.str.sep == '`')
          esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
        PARSE_ESCAPE parse_cmd{esc_rule};
        if (driver.call_command(parse_cmd))
          switch (std::get<BAD_ESCAPE>(driver.known_err)) {
            case BAD_ESCAPE::MALFORMED:
              return PARSE_ERR{BAD_STRING::MALFORMED_SEQ_IN_ESCAPE};
            case BAD_ESCAPE::OCTAL_SEQ:
              return PARSE_ERR{BAD_STRING::OCTAL_SEQ_IN_ESCAPE};
            case BAD_ESCAPE::PER_SE_BACKSLASH:
              driver.drop(1);
              break;
          }
        else
          ch = parse_cmd.ch_esc;
        ch_ret = *ch;
        return PARSE_OK::COMMIT;
    }
  }
};

struct PARSE_STRING {
  CMD_EXIT operator()(ParseDriver& driver) {
    driver.token.str.sep = *driver.shift();
    driver.ch_temp.clear();
    while (1) {
      std::optional ch = driver.peek();
      if (not ch)
        return PARSE_ERR{BAD_STRING::UNEXPECTED_END};
      if (driver.token.str.sep == '`') {
        if (*ch == '\r') {
          if (driver.peek() == '\n')
            driver.drop(1);
          ch = '\n';
        }
      } else if (*ch == '\r' || *ch == '\n')
        return PARSE_ERR{BAD_STRING::UNEXPECTED_END};
      driver.drop(1);
      if (*ch == driver.token.str.sep)
        return PARSE_OK::COMMIT;
      if (*ch == '$' && driver.peek() == '{' && driver.token.str.sep == '`') {
        driver.drop(1);
        return PARSE_OK::COMMIT;
      }
      if (*ch == '\\') {
        PARSE_STRING_ESCSEQ escseq{};
        if (driver.call_command(escseq))
          return PARSE_ERR{driver.known_err};
        else if (escseq.must_continue)
          continue;
        else
          ch = escseq.ch_ret;
      }
      std::array<char, 4> cp_storage{};
      driver.ch_temp.append(codepoint_cv(*ch, cp_storage));
    }
  }
};

struct PARSE_IDENT_UCHAR {
  bool beginning;

  CMD_EXIT operator()(ParseDriver& driver) {
    std::optional ch{driver.peek()};
    if (not ch)
      return PARSE_ERR{};
    driver.drop(1);

    if (*ch == '\\' && driver.peek() == 'u') {
      PARSE_ESCAPE parse_cmd{ESC_RULE::IDENTIFIER};
      if (driver.call_command(parse_cmd))
        return PARSE_ERR{};
      ch = parse_cmd.ch_esc;
      driver.token.ident.has_escape = true;
    }

    UProperty must_be = beginning ? UCHAR_XID_START : UCHAR_XID_CONTINUE;
    if (not u_hasBinaryProperty(*ch, must_be))
      return PARSE_ERR{};

    std::array<char, 4> cp_storage{};
    driver.ch_temp.append(codepoint_cv(*ch, cp_storage));
    return PARSE_OK::COMMIT;
  }
};

struct PARSE_IDENT {
  bool is_private;

  CMD_EXIT operator()(ParseDriver& driver) {
    driver.ch_temp.clear();

    PARSE_IDENT_UCHAR parse_start{.beginning = 1};
    if (driver.call_command(parse_start))
      return PARSE_ERR{};

    if (is_private)
      driver.ch_temp.insert(driver.ch_temp.begin(), '#');

    while (1) {
      PARSE_IDENT_UCHAR parse_continue{};
      if (driver.call_command(parse_continue))
        return PARSE_OK::COMMIT;
    }
  }
};

bool ParseDriver::find_static_atom(PARSE_IDENT&) {
  for (std::size_t i = 0; i < reserved_arr.size(); i++) {
    auto [literal, token_type, r_strict] = reserved_arr[i];
    if (ch_temp != literal)
      continue;
    token.ident.atom_idx = i;
    if (strictness < r_strict)
      return 1;
    if (token.ident.has_escape) {
      token.ident.is_reserved = 1;
      return 1;
    }
    token.type = token_type;
    return 1;
  }
  return 0;
}

bool ParseDriver::find_static_atom(PARSE_STRING&) {
  for (std::size_t i = 0; i < reserved_arr.size(); i++) {
    if (ch_temp != std::get<0>(reserved_arr[i]))
      continue;
    token.str.atom_idx = i;
    return 1;
  }
  return 0;
}

struct PARSE_TOKEN {
  CMD_EXIT operator()(ParseDriver& driver) { return driver.parse_token(); }
};

CMD_EXIT ParseDriver::parse_token() {
  TAKE<2> tcmd{};
  while (1) {
    call_command(tcmd);
    if (tcmd.sv().empty()) {
      token.type = TOKEN_TYPE::T_EOF;
      return PARSE_OK::COMMIT;
    }
    switch (tcmd.sv().front()) {
      case '\r':
        if (tcmd.sv() == U"\r\n")
          drop(1);
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        drop(1);
        token.newline_seen = 1;
        continue;
      case '/':
        if (tcmd.sv() == U"//") {
          while (1) {
            std::optional ch = peek();
            if (not ch || is_lineterm(*ch))
              break;
            drop(1);
          }
          continue;
        } else if (tcmd.sv() == U"/*") {
          drop(2);
          while (1) {
            call_command(tcmd);
            if (tcmd.sv().empty()) {
              token.type = TOKEN_TYPE::T_ERROR;
              return PARSE_ERR{BAD_COMMENT::UNEXPECTED_END};
            }
            if (tcmd.sv() == U"*/") {
              drop(2);
              break;
            }
            if (is_lineterm(tcmd.sv().front()))
              token.newline_seen = 1;
            drop(1);
          }
          continue;
        }
        break;
      case '\'':
      case '"': {
        PARSE_STRING string_cmd{};
        if (not call_command(string_cmd)) {
          token.type = TOKEN_TYPE::T_STRING;
          if (find_static_atom(string_cmd))
            return PARSE_OK::COMMIT;
          token.str.atom_idx = obtain_atom();
          return PARSE_OK::COMMIT;
        }
        token.type = TOKEN_TYPE::T_ERROR;
        return PARSE_ERR{known_err};
      }
      default:
        if (u_isWhitespace(tcmd.sv().front())) {
          drop(1);
          continue;
        } else {
          PARSE_IDENT ident_cmd{};
          if (not call_command(ident_cmd)) {
            token.type = TOKEN_TYPE::T_IDENT;
            if (find_static_atom(ident_cmd))
              return PARSE_OK::COMMIT;
            token.ident.atom_idx = obtain_atom();
            return PARSE_OK::COMMIT;
          }
          drop(1);
          token.type = TOKEN_TYPE::T_UCHAR;
          token.uchar = tcmd.sv().front();
          return PARSE_OK::COMMIT;
        }
    }
  }
}

std::size_t ParseDriver::obtain_atom() {
  if (not atom_umap.contains(ch_temp)) {
    atom_deq.push_back(std::move(ch_temp));
    atom_umap[atom_deq.back()] = reserved_arr.size() + atom_deq.size() - 1;
  }
  return atom_umap[atom_deq.back()];
}

bool ParseDriver::parse() {
  return 1;
}
}  // namespace Manadrain
