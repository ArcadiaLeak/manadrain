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

  bool exec(ParseDriver& drv) {
    *this = {};
    while (len < N) {
      std::optional ch{drv.peek()};
      if (not ch)
        break;
      drv.drop(1);
      ch_arr[len++] = *ch;
    }
    return 1;
  }
};

struct PARSE_HEX {
  std::uint32_t digit;

  bool exec(ParseDriver& drv) {
    std::optional uchar{drv.shift()};
    if (not uchar)
      return 1;
    if (*uchar >= '0' && *uchar <= '9') {
      digit = *uchar - '0';
      return 0;
    }
    if (*uchar >= 'A' && *uchar <= 'F') {
      digit = *uchar - 'A' + 10;
      return 0;
    }
    if (*uchar >= 'a' && *uchar <= 'f') {
      digit = *uchar - 'a' + 10;
      return 0;
    }
    return 1;
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

  bool parseHexSeq(ParseDriver& drv) {
    PARSE_HEX hex0{};
    if (drv.exec_command(hex0)) {
      drv.known_err = BAD_ESCAPE::MALFORMED;
      return 1;
    }
    PARSE_HEX hex1{};
    if (drv.exec_command(hex1)) {
      drv.known_err = BAD_ESCAPE::MALFORMED;
      return 1;
    }
    ch_esc = (hex0.digit << 4) | hex1.digit;
    return 0;
  }

  bool parseUniBraced(ParseDriver& drv) {
    char32_t utf16_char = 0;
    while (1) {
      PARSE_HEX hex{};
      if (drv.exec_command(hex)) {
        std::optional close_or_uchar{drv.shift()};
        if (not close_or_uchar) {
          drv.known_err = BAD_ESCAPE::MALFORMED;
          return 1;
        }
        if (close_or_uchar == '}') {
          ch_esc = utf16_char;
          return 0;
        }
      }
      utf16_char = (utf16_char << 4) | hex.digit;
      if (utf16_char > 0x10FFFF) {
        drv.known_err = BAD_ESCAPE::MALFORMED;
        return 1;
      }
    }
  }

  bool parseUniFixed(ParseDriver& drv) {
    char32_t high_surr = 0;
    for (int i = 0; i < 4; i++) {
      PARSE_HEX hex{};
      if (drv.exec_command(hex)) {
        drv.known_err = BAD_ESCAPE::MALFORMED;
        return 1;
      }
      high_surr = (high_surr << 4) | hex.digit;
    }
    if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16) {
      TAKE<2> tcmd{};
      drv.exec_command(tcmd);
      if (tcmd.sv() == U"\\u") {
        drv.drop(2);
        char32_t low_surr = 0;
        for (int i = 0; i < 4; i++) {
          PARSE_HEX hex{};
          if (drv.exec_command(hex)) {
            ch_esc = high_surr;
            return 0;
          }
          low_surr = (low_surr << 4) | hex.digit;
        }
        if (is_lo_surrogate(low_surr)) {
          ch_esc = from_surrogate(high_surr, low_surr);
          return 0;
        }
      }
    }
    ch_esc = high_surr;
    return 0;
  }

  bool parseUniSeq(ParseDriver& drv) {
    std::optional open_or_uchar{drv.shift()};
    if (not open_or_uchar) {
      drv.known_err = BAD_ESCAPE::MALFORMED;
      return 1;
    }
    return open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII
               ? parseUniBraced(drv)
               : parseUniFixed(drv);
  }

  bool exec(ParseDriver& drv) {
    std::optional head{drv.shift()};
    if (not head) {
      drv.known_err = BAD_ESCAPE::PER_SE_BACKSLASH;
      return 1;
    }
    switch (*head) {
      case 'b':
        ch_esc = '\b';
        return 0;
      case 'f':
        ch_esc = '\f';
        return 0;
      case 'n':
        ch_esc = '\n';
        return 0;
      case 'r':
        ch_esc = '\r';
        return 0;
      case 't':
        ch_esc = '\t';
        return 0;
      case 'v':
        ch_esc = '\v';
        return 0;
      case 'x':
        return parseHexSeq(drv);
      case 'u':
        return parseUniSeq(drv);
      case '0': {
        std::optional ahead{drv.peek()};
        if (not ahead.transform([](char32_t uch) { return std::isdigit(uch); })
                    .value_or(false)) {
          ch_esc = 0;
          return 0;
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
            drv.known_err = BAD_ESCAPE::OCTAL_SEQ;
            return 1;

          case ESC_RULE::STRING_IN_TEMPLATE:
          case ESC_RULE::REGEXP_UTF16:
            drv.known_err = BAD_ESCAPE::MALFORMED;
            return 1;

          default:
            ch_esc = *head - '0';
            std::optional<char32_t> ahead;
            ahead = drv.peek().transform(
                [](char32_t ahead_digit) { return ahead_digit - '0'; });
            if (not ahead || *ahead > 7)
              return 0;
            drv.drop(1);
            ch_esc = (ch_esc << 3) | *ahead;

            if (ch_esc >= 32)
              return 0;

            ahead = drv.peek().transform(
                [](char32_t ahead_digit) { return ahead_digit - '0'; });
            if (not ahead || *ahead > 7)
              return 0;
            drv.drop(1);
            ch_esc = (ch_esc << 3) | *ahead;
            return 0;
        }
      case '8':
      case '9':
        if (esc_rule == ESC_RULE::STRING_IN_STRICT_MODE ||
            esc_rule == ESC_RULE::STRING_IN_TEMPLATE) {
          drv.known_err = BAD_ESCAPE::MALFORMED;
          return 1;
        }
        [[fallthrough]];
      default:
        drv.known_err = BAD_ESCAPE::PER_SE_BACKSLASH;
        return 1;
    }
  }
};

struct PARSE_STRING_ESCSEQ {
  char32_t ch_ret;
  bool must_continue;

  bool exec(ParseDriver& drv) {
    std::optional ch{drv.peek()};
    if (not ch) {
      drv.known_err = BAD_STRING::UNEXPECTED_END;
      return 1;
    }
    switch (*ch) {
      case '\'':
      case '\"':
      case '\0':
      case '\\':
        drv.drop(1);
        ch_ret = *ch;
        return 0;
      case '\r':
        if (drv.peek() == '\n')
          drv.drop(1);
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        /* ignore escaped newline sequence */
        drv.drop(1);
        must_continue = 1;
        return 0;
      default:
        ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
        if (drv.strictness == STRICTNESS::STRICT)
          esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
        else if (drv.token.str.sep == '`')
          esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
        PARSE_ESCAPE parse_cmd{esc_rule};
        if (drv.exec_command(parse_cmd)) {
          if (drv.known_err == PARSE_ERROR{BAD_ESCAPE::MALFORMED}) {
            drv.known_err = BAD_STRING::MALFORMED_SEQ_IN_ESCAPE;
            return 1;
          } else if (drv.known_err == PARSE_ERROR{BAD_ESCAPE::OCTAL_SEQ}) {
            drv.known_err = BAD_STRING::OCTAL_SEQ_IN_ESCAPE;
            return 1;
          } else if (drv.known_err == PARSE_ERROR{BAD_ESCAPE::PER_SE_BACKSLASH})
            /* ignore the '\' (could output a warning) */
            drv.drop(1);
        } else
          ch = parse_cmd.ch_esc;
        ch_ret = *ch;
        return 0;
    }
  }
};

struct PARSE_STRING {
  bool exec(ParseDriver& drv) {
    drv.token.str.sep = *drv.shift();
    drv.ch_temp.clear();
    while (1) {
      std::optional ch = drv.peek();
      if (not ch) {
        drv.known_err = BAD_STRING::UNEXPECTED_END;
        return 1;
      }
      if (drv.token.str.sep == '`') {
        if (*ch == '\r') {
          if (drv.peek() == '\n')
            drv.drop(1);
          ch = '\n';
        }
      } else if (*ch == '\r' || *ch == '\n') {
        drv.known_err = BAD_STRING::UNEXPECTED_END;
        return 1;
      }
      drv.drop(1);
      if (*ch == drv.token.str.sep)
        return 0;
      if (*ch == '$' && drv.peek() == '{' && drv.token.str.sep == '`') {
        drv.drop(1);
        return 0;
      }
      if (*ch == '\\') {
        PARSE_STRING_ESCSEQ escseq{};
        if (drv.exec_command(escseq))
          return 1;
        else if (escseq.must_continue)
          continue;
        else
          ch = escseq.ch_ret;
      }
      std::array<char, 4> cp_storage{};
      drv.ch_temp.append(codepoint_cv(*ch, cp_storage));
    }
  }
};

struct PARSE_IDENT_UCHAR {
  bool beginning;

  bool exec(ParseDriver& drv) {
    std::optional ch{drv.peek()};
    if (not ch)
      return 1;
    drv.drop(1);

    if (*ch == '\\' && drv.peek() == 'u') {
      PARSE_ESCAPE parse_cmd{ESC_RULE::IDENTIFIER};
      if (drv.exec_command(parse_cmd))
        return 1;
      ch = parse_cmd.ch_esc;
      drv.token.ident.has_escape = true;
    }

    UProperty must_be = beginning ? UCHAR_XID_START : UCHAR_XID_CONTINUE;
    if (not u_hasBinaryProperty(*ch, must_be))
      return 1;

    std::array<char, 4> cp_storage{};
    drv.ch_temp.append(codepoint_cv(*ch, cp_storage));
    return 0;
  }
};

struct PARSE_IDENT {
  bool is_private;

  bool exec(ParseDriver& drv) {
    drv.ch_temp.clear();

    PARSE_IDENT_UCHAR parse_start{.beginning = 1};
    if (drv.exec_command(parse_start))
      return 1;

    if (is_private)
      drv.ch_temp.insert(drv.ch_temp.begin(), '#');

    while (1) {
      PARSE_IDENT_UCHAR parse_continue{};
      if (drv.exec_command(parse_continue))
        return 0;
    }
  }
};

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
