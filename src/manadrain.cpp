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
    std::to_array<std::tuple<std::string_view, TOKEN_KIND, STRICTNESS>>(
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

std::expected<std::size_t, std::monostate> ParseDriver::find_dynamic_atom() {
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

void ParseDriver::skip_lf() {
  std::expected ahead = next(nullptr);
  if (not ahead || ahead == '\n')
    return;
  backtrack(nullptr, 1);
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
  return std::unexpected{PARSE_HEX::NOT_A_DIGIT};
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
  std::expected ahead = next(nullptr);
  if (not ahead)
    return std::unexpected{ahead.error()};
  if (ahead == '{') {
    if (esc.rule == ESC_RULE::REGEXP_ASCII)
      return std::unexpected{PARSE_ESCAPE::MALFORMED};
    return parse_uni_braced(esc);
  }
  backtrack(nullptr, 1);
  return parse_uni_fixed(esc);
}

EXPECT<char32_t> parse_octal(char32_t ahead_digit) {
  if (ahead_digit <= 7)
    return ahead_digit - '0';
  return std::unexpected{PARSE_ESCAPE::NOT_AN_OCTAL_DIGIT};
}

EXPECT<char32_t> ParseDriver::parse_legacy_octal(PARSE_ESCAPE) {
  std::expected ahead = next(nullptr).transform(parse_octal);
  if (not ahead)
    return std::unexpected{ahead.error()};
  if (not *ahead)
    backtrack(nullptr, 1);
  return *ahead;
}

bool ParseDriver::parse_null(PARSE_ESCAPE) {
  std::expected ahead = next(nullptr);
  if (not ahead)
    return 0;
  backtrack(nullptr, 1);
  return std::isdigit(*ahead);
}

EXPECT<char32_t> ParseDriver::parse(PARSE_ESCAPE esc) {
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
      if (parse_null(esc))
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
          return std::unexpected{PARSE_ESCAPE::LEGACY_OCTAL_SEQ};

        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          return std::unexpected{PARSE_ESCAPE::MALFORMED};

        default:
          ch = ch.transform([](char32_t digit) { return digit - '0'; });
          ch = ch.and_then([this](char32_t digit) {
            return parse_legacy_octal(PARSE_ESCAPE{})
                .transform(
                    [digit](char32_t ahead) { return (digit << 3) | ahead; });
          });
          if (not ch || *ch >= 32)
            return ch;
          ch = ch.and_then([this](char32_t digit) {
            return parse_legacy_octal(PARSE_ESCAPE{})
                .transform(
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
      backtrack(nullptr, 1);
      return std::unexpected{PARSE_ESCAPE::PER_SE_BACKSLASH};
  }
}

EXPECT<char32_t> ParseDriver::parse_escape(PARSE_STRING parsing) {
  std::expected ch{next(nullptr)};
  if (not ch)
    return std::unexpected{PARSE_STRING::UNEXPECTED_END};

  switch (*ch) {
    case '\'':
    case '\"':
    case '\0':
    case '\\':
      return ch;
    case '\r':
      skip_lf();
      [[fallthrough]];
    case '\n':
    case 0x2028:
    case 0x2029:
      /* ignore escaped newline sequence */
      return std::unexpected{PARSE_STRING::MUST_CONTINUE};
  }

  ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
  if (strictness == STRICTNESS::STRICT)
    esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
  else if (parsing.sep == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  backtrack(nullptr, 1);

  std::expected esc = parse(PARSE_ESCAPE{esc_rule});
  if (not esc)
    switch (esc.error()) {
      case PARSE_ESCAPE::MALFORMED:
        return std::unexpected{PARSE_STRING::MALFORMED_ESC};
      case PARSE_ESCAPE::LEGACY_OCTAL_SEQ:
        return std::unexpected{PARSE_STRING::LEGACY_OCTAL_SEQ};
      case PARSE_ESCAPE::PER_SE_BACKSLASH:
        return ch;
    }
  return esc;
}

EXPECT<std::monostate> ParseDriver::parse(PARSE_STRING parsing) {
  ch_temp.clear();
  while (1) {
    std::expected ch = next(nullptr);
    if (not ch)
      return std::unexpected{PARSE_STRING::UNEXPECTED_END};
    if (parsing.sep == '`') {
      if (*ch == '\r') {
        skip_lf();
        ch = '\n';
      }
    } else if (*ch == '\r' || *ch == '\n') {
      backtrack(nullptr, 1);
      return std::unexpected{PARSE_STRING::UNEXPECTED_END};
    }
    if (*ch == parsing.sep)
      return std::monostate{};
    int fwd_cnt = 0;
    if (parsing.sep == '`' && *ch == '$' && next(&fwd_cnt) == '{')
      return std::monostate{};
    backtrack(nullptr, fwd_cnt);
    if (*ch == '\\') {
      std::expected esc = parse_escape(parsing);
      if (esc)
        ch = esc;
      else if (esc.error() == PARSE_STRING::MUST_CONTINUE)
        continue;
      else
        return std::unexpected{esc.error()};
    }
    ch_temp.append(codepoint_cv(*ch));
  }
}

EXPECT<std::string> ParseDriver::parse_uchar(PARSE_IDENT ident,
                                             bool beginning,
                                             bool& has_escape) {
  int fwd_cnt = 0;
  std::string ahead{take(&fwd_cnt, 2)};
  if (ahead != "\\u")
    backtrack(nullptr, fwd_cnt);
  std::expected ch_exp = ahead == "\\u"
                             ? parse_uni(PARSE_ESCAPE{ESC_RULE::IDENTIFIER})
                             : next(nullptr);
  has_escape = ahead == "\\u";
  UProperty must_be = beginning ? UCHAR_XID_START : UCHAR_XID_CONTINUE;
  return ch_exp.transform([must_be](char32_t ch) {
    return u_hasBinaryProperty(ch, must_be) ? codepoint_cv(ch) : "";
  });
}

EXPECT<bool> ParseDriver::parse(PARSE_IDENT ident) {
  ch_temp.clear();
  if (ident.is_private)
    ch_temp.push_back('#');

  bool beginning = 1, has_escape = 0;
  while (1) {
    std::expected conv_ch = parse_uchar(PARSE_IDENT{}, 1, has_escape);
    if (not conv_ch)
      return std::unexpected{conv_ch.error()};
    if (conv_ch->empty())
      return has_escape;
    ch_temp.append(*conv_ch);
    if (not next(nullptr))
      return has_escape;
    backtrack(nullptr, 1);
    beginning = 0;
  }
}

EXPECT<std::monostate> ParseDriver::parse(PARSE_TOKEN) {
  TOKEN token{};
  while (1) {
    std::expected result = parse_iter(PARSE_TOKEN{}, token);
    if (result.has_value()) {
      if (*result)
        continue;
      return std::monostate{};
    }
    return std::unexpected{result.error()};
  }
}

bool ParseDriver::parse_comment_line(PARSE_TOKEN) {
  std::expected ch = next(nullptr);
  if (not ch)
    return 0;
  if (is_lineterm(*ch)) {
    backtrack(nullptr, 1);
    return 0;
  }
  return 1;
}

EXPECT<bool> ParseDriver::parse_comment_block(PARSE_TOKEN, bool& newline_seen) {
  std::expected ch_exp = next(nullptr);
  if (not ch_exp)
    return std::unexpected{PARSE_TOKEN::UNCLOSED_COMMENT};
  backtrack(nullptr, 1);
  int fwd_cnt = 0;
  if (take(&fwd_cnt, 2) == "*/")
    return 0;
  backtrack(nullptr, fwd_cnt - 1);
  if (ch_exp.transform([](char32_t ch) { return is_lineterm(ch); }))
    newline_seen = 1;
  return 1;
}

EXPECT<bool> ParseDriver::parse_comment(PARSE_TOKEN, char32_t ch) {
  bool newline_seen = 0;
  if (ch == '/')
    while (1) {
      if (not parse_comment_line(PARSE_TOKEN{}))
        break;
    }
  else if (ch == '*')
    while (1) {
      std::expected comment = parse_comment_block(PARSE_TOKEN{}, newline_seen);
      if (not comment)
        return std::unexpected{comment.error()};
      if (not *comment)
        break;
    }
  else
    backtrack(nullptr, 1);
  return newline_seen;
}

EXPECT<bool> ParseDriver::parse_iter(PARSE_TOKEN, TOKEN& token) {
  std::expected ch{next(nullptr)};
  if (not ch) {
    token.kind = K_TOKEN_EOF;
    return 0;
  }

  switch (*ch) {
    case '\r':
      skip_lf();
      [[fallthrough]];
    case '\n':
    case 0x2028:
    case 0x2029:
      token.newline_seen = 1;
      return 1;
    case '/': {
      std::expected newline_seen =
          next(nullptr).and_then([this](char32_t ahead) {
            return parse_comment(PARSE_TOKEN{}, ahead);
          });
      token.newline_seen = token.newline_seen || newline_seen.value_or(false);
      return newline_seen.transform([](bool) { return 1; });
    }
    case '\'':
    case '"':
      token.str.sep = *ch;
      std::expected str_outcome = parse(PARSE_STRING{token.str.sep});
      if (not str_outcome)
        return std::unexpected{str_outcome.error()};
      else {
        token.kind = K_TOKEN_STRING;
        token.str.atom_idx =
            find_static_atom()
                .or_else([this](std::monostate) { return find_dynamic_atom(); })
                .value();
        return 0;
      }
  }

  if (u_isWhitespace(*ch))
    return 1;

  backtrack(nullptr, 1);
  std::expected ident_outcome = parse(PARSE_IDENT{});
  if (not ident_outcome)
    return std::unexpected{ident_outcome.error()};
  else {
    token.kind = K_TOKEN_IDENT;
    token.ident.has_escape = *ident_outcome;
    token.ident.atom_idx =
        find_static_atom()
            .or_else([this](std::monostate) { return find_dynamic_atom(); })
            .value();
    update_ident(token.kind, token.ident);
    return 0;
  }

  token.kind = K_TOKEN_UCHAR;
  token.uchar = next(nullptr).value();
  return 0;
}

void ParseDriver::update_ident(TOKEN_KIND& kind, TOKEN::PAYLOAD_IDENT& ident) {
  std::size_t i = ident.atom_idx;
  if (i >= reserved_arr.size())
    return;
  auto [literal, tok_kind, tok_strict] = reserved_arr[i];
  if (strictness < tok_strict)
    return;
  if (ident.has_escape)
    ident.is_reserved = 1;
  else
    kind = tok_kind;
}

std::expected<std::size_t, std::monostate> ParseDriver::find_static_atom() {
  for (std::size_t i = 0; i < reserved_arr.size(); i++) {
    auto [literal, _, _] = reserved_arr[i];
    if (ch_temp == literal)
      return i;
  }
  return std::unexpected{std::monostate{}};
}

bool ParseDriver::parse() {
  return 0;
}
}  // namespace Manadrain
