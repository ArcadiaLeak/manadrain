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
static bool is_lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

namespace Manadrain {
ENCODED_POINT codepoint_cv(char32_t ch) {
  std::uint16_t length{}, is_error{};
  std::array<char, 4> buff{};
  U8_APPEND(buff.data(), length, buff.size(), ch, is_error);
  if (is_error)
    length = 0;
  return {buff, length};
}

static const std::array reserved_arr =
    std::to_array<std::tuple<std::string_view, TOKEN_KIND, STRICTNESS>>(
        {{"var", K_TOKEN_VAR, STRICTNESS::SLOPPY},
         {"const", K_TOKEN_CONST, STRICTNESS::SLOPPY},
         {"let", K_TOKEN_LET, STRICTNESS::STRICT}});

bool TOKEN::is_pseudo_kind(int rhs_kind) {
  if (ident().has_escape)
    return 0;
  if (ident().atom_idx >= reserved_arr.size())
    return 0;
  auto [_, lhs_kind, _] = reserved_arr[ident().atom_idx];
  if (lhs_kind == rhs_kind)
    return 1;
  return 0;
}

std::expected<std::size_t, std::monostate> ParseDriver::find_dynamic_atom() {
  if (not atom_umap.contains(str0_temp)) {
    atom_deq.push_back(std::move(str0_temp));
    atom_umap[atom_deq.back()] = reserved_arr.size() + atom_deq.size() - 1;
  }
  return atom_umap[atom_deq.back()];
}

EXPECT<char32_t> ParseDriver::next() {
  if (buffer_idx < buffer.size()) {
    UChar32 ch;
    U8_NEXT_OR_FFFD(buffer.data(), buffer_idx, buffer.size(), ch);
    for (int &el_int : int_temp)
      ++el_int;
    return ch;
  }
  return std::unexpected{PARSE_ERRCODE::OUT_OF_RANGE};
}

EXPECT<char32_t> ParseDriver::prev() {
  if (0 < buffer_idx) {
    UChar32 ch;
    U8_PREV_OR_FFFD(buffer.data(), 0, buffer_idx, ch);
    for (int &el_int : int_temp)
      --el_int;
    return ch;
  }
  return std::unexpected{PARSE_ERRCODE::OUT_OF_RANGE};
}

void ParseDriver::backtrack(int N) {
  int &i = std::get<1>(int_temp);
  i = 0;
  while (N + i > 0) {
    std::expected ch{prev()};
    if (not ch)
      break;
  }
}

std::string_view ParseDriver::take(int N) {
  str1_temp.clear();
  int &i = std::get<1>(int_temp);
  i = 0;
  while (i < N) {
    std::expected ch{next()};
    if (not ch)
      break;
    ENCODED_POINT cp = codepoint_cv(*ch);
    str1_temp.append(cp.sv());
  }
  return str1_temp;
}

void ParseDriver::skip_lf() {
  std::expected ahead = next();
  if (not ahead || ahead == '\n')
    return;
  prev();
}

EXPECT<char32_t> ParseDriver::parse_hex() {
  std::expected uchar{next()};
  if (not uchar)
    return uchar;
  if (*uchar >= '0' && *uchar <= '9')
    return *uchar - '0';
  if (*uchar >= 'A' && *uchar <= 'F')
    return *uchar - 'A' + 10;
  if (*uchar >= 'a' && *uchar <= 'f')
    return *uchar - 'a' + 10;
  prev();
  return std::unexpected{PARSE_ERRCODE::HEX__NOT_A_DIGIT};
}

EXPECT<char32_t> ParseDriver::parse_hex(PARSE_ESCAPE) {
  std::expected hex0 = parse_hex();
  if (not hex0)
    return hex0;
  std::expected hex1 = parse_hex();
  if (not hex1) {
    prev();
    return hex1;
  }
  return (*hex0 << 4) | *hex1;
}

EXPECT<char32_t> ParseDriver::parse_uni_braced(PARSE_ESCAPE) {
  std::get<0>(int_temp) = 0;
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
  backtrack(std::get<0>(int_temp));
  return std::unexpected{PARSE_ERRCODE::ESCAPE__MALFORMED};
}

EXPECT<char32_t> ParseDriver::parse_uni_fixed(PARSE_ESCAPE esc) {
  char32_t high_surr{}, low_surr{};

  for (int i = 0; i < 4; i++) {
    std::expected hex = parse_hex();
    if (not hex) {
      backtrack(i);
      return std::unexpected{PARSE_ERRCODE::ESCAPE__MALFORMED};
    }
    high_surr = (high_surr << 4) | *hex;
  }

  std::get<0>(int_temp) = 0;
  if (is_hi_surrogate(high_surr) && esc.rule == ESC_RULE::REGEXP_UTF16 &&
      take(2) == "\\u") {
    for (int i = 0; i < 4; i++) {
      std::expected hex = parse_hex();
      if (not hex)
        goto return_high;
      low_surr = (low_surr << 4) | *hex;
    }
    goto return_low;
  }

return_high:
  backtrack(std::get<0>(int_temp));
  return high_surr;

return_low:
  if (not is_lo_surrogate(low_surr))
    goto return_high;
  return from_surrogate(high_surr, low_surr);
}

EXPECT<char32_t> ParseDriver::parse_uni(PARSE_ESCAPE esc) {
  std::expected brace_or_digit = next();
  if (brace_or_digit == '{') {
    if (esc.rule == ESC_RULE::REGEXP_ASCII)
      return std::unexpected{PARSE_ERRCODE::ESCAPE__MALFORMED};
    return parse_uni_braced(esc);
  }
  prev();
  return brace_or_digit.and_then(
      [this, esc](char32_t) { return parse_uni_fixed(esc); });
}

char32_t ParseDriver::parse_oct_digit(PARSE_ESCAPE, char32_t oct) {
  std::expected ahead =
      next().transform([](char32_t ch_ahead) { return ch_ahead - '0'; });
  if (ahead.transform([](char32_t digit) { return digit > 7; }).value_or(0)) {
    prev();
    return oct;
  }
  return ahead.transform([oct](char32_t digit) { return (oct << 3) | digit; })
      .value_or(oct);
}

bool ParseDriver::parse_null(PARSE_ESCAPE) {
  std::expected ahead = next();
  if (not ahead)
    return 0;
  prev();
  return std::isdigit(*ahead);
}

EXPECT_OPT<char32_t> ParseDriver::parse(PARSE_ESCAPE esc, char32_t ch) {
  switch (ch) {
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
      return std::unexpected{PARSE_ERRCODE::ESCAPE__LEGACY_OCTAL_SEQ};

    case ESC_RULE::STRING_IN_TEMPLATE:
    case ESC_RULE::REGEXP_UTF16:
      return std::unexpected{PARSE_ERRCODE::ESCAPE__MALFORMED};

    default: {
      char32_t oct = ch - '0';
      oct = parse_oct_digit(PARSE_ESCAPE{}, oct);
      if (oct >= 32)
        return oct;
      oct = parse_oct_digit(PARSE_ESCAPE{}, oct);
      return oct;
    }
    }
  case '8':
  case '9':
    if (esc.rule == ESC_RULE::STRING_IN_STRICT_MODE ||
        esc.rule == ESC_RULE::STRING_IN_TEMPLATE)
      return std::unexpected{PARSE_ERRCODE::ESCAPE__MALFORMED};
    [[fallthrough]];
  default:
    return std::nullopt;
  }
}

EXPECT_OPT<char32_t> ParseDriver::parse_escape(PARSE_STRING, char32_t ch) {
  switch (ch) {
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
    return std::nullopt;
  }
  ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
  if (strictness == STRICTNESS::STRICT)
    esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
  else if (token.str().sep == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  return parse(PARSE_ESCAPE{esc_rule}, ch);
}

EXPECT_OPT<char32_t> ParseDriver::parse_uchar(PARSE_STRING, char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
    if (token.str().sep == '`')
      return '\n';
    return std::unexpected{PARSE_ERRCODE::STRING__UNEXPECTED_END};
  case '\\': {
    std::expected ahead = next();
    if (not ahead)
      return std::nullopt;
    return ahead.and_then([this](char32_t next_ch) {
      return parse_escape(PARSE_STRING{}, next_ch);
    });
  }
  default:
    return ch;
  }
}

EXPECT<std::monostate> ParseDriver::parse(PARSE_STRING) {
  EXPECT<std::monostate> ret{};
  str0_temp.clear();
  while (ret) {
    std::expected ch_exp = next()
                               .transform_error([](PARSE_ERRCODE) {
                                 return PARSE_ERRCODE::STRING__UNEXPECTED_END;
                               })
                               .and_then([this](char32_t ch) {
                                 return parse_uchar(PARSE_STRING{}, ch);
                               });
    ret = ch_exp.transform([](auto) { return std::monostate{}; });
    if (not ch_exp.value_or(std::nullopt))
      continue;
    char32_t ch = **ch_exp;
    if (ch == token.str().sep)
      break;
    std::get<0>(int_temp) = 0;
    if (token.str().sep == '`' && ch == '$' && next() == '{')
      break;
    backtrack(std::get<0>(int_temp));
    ENCODED_POINT cp = codepoint_cv(ch);
    str0_temp.append(cp.sv());
  }
  return ret;
}

EXPECT<ENCODED_POINT> ParseDriver::parse_uchar(PARSE_IDENT ident,
                                               bool beginning) {
  std::get<0>(int_temp) = 0;
  std::string ahead{take(2)};
  if (ahead == "\\u")
    token.ident().has_escape = 1;
  else
    backtrack(std::get<0>(int_temp));
  UProperty must_be = beginning ? UCHAR_XID_START : UCHAR_XID_CONTINUE;
  std::expected ch_exp =
      ahead == "\\u" ? parse_uni(PARSE_ESCAPE{ESC_RULE::IDENTIFIER}) : next();
  return ch_exp.transform([must_be](char32_t ch) {
    return u_hasBinaryProperty(ch, must_be) ? codepoint_cv(ch)
                                            : ENCODED_POINT{};
  });
}

EXPECT<bool> ParseDriver::parse(PARSE_IDENT ident) {
  str0_temp.clear();
  if (ident.is_private)
    str0_temp.push_back('#');

  bool beginning = 1;
  while (1) {
    std::expected conv_ch = parse_uchar(PARSE_IDENT{}, 1);
    if (not conv_ch)
      return std::unexpected{conv_ch.error()};
    if (conv_ch->length == 0) {
      prev();
      return !beginning;
    }
    str0_temp.append(conv_ch->sv());
    if (not next())
      return 1;
    prev();
    beginning = 0;
  }
}

EXPECT<std::monostate> ParseDriver::parse(PARSE_TOKEN) {
  while (1) {
    std::expected result = parse_iter(PARSE_TOKEN{});
    if (result.has_value()) {
      if (*result)
        continue;
      return std::monostate{};
    }
    return std::unexpected{result.error()};
  }
}

bool ParseDriver::parse_comment_line(PARSE_TOKEN) {
  std::expected ch = next();
  if (not ch)
    return 0;
  if (is_lineterm(*ch)) {
    prev();
    return 0;
  }
  return 1;
}

EXPECT<bool> ParseDriver::parse_comment_block(PARSE_TOKEN) {
  if (not next())
    return std::unexpected{PARSE_ERRCODE::UNCLOSED_COMMENT};
  prev();
  std::get<0>(int_temp) = 0;
  if (take(2) == "*/")
    return 0;
  backtrack(std::get<0>(int_temp));
  if (next().transform([](char32_t ch) { return is_lineterm(ch); }).value_or(0))
    token.newline_seen = 1;
  return 1;
}

EXPECT<bool> ParseDriver::parse_comment(PARSE_TOKEN, char32_t ch) {
  switch (ch) {
  case '/':
    while (1) {
      if (not parse_comment_line(PARSE_TOKEN{}))
        break;
    }
    break;
  case '*':
    while (1) {
      std::expected comment = parse_comment_block(PARSE_TOKEN{});
      if (not comment)
        return std::unexpected{comment.error()};
      if (not *comment)
        break;
    }
    break;
  default:
    prev();
    break;
  }
  return 1;
}

EXPECT<bool> ParseDriver::parse_iter(PARSE_TOKEN) {
  std::expected ch{next()};
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
    std::expected ahead = next();
    if (not ahead)
      return 1;
    return parse_comment(PARSE_TOKEN{}, *ahead);
  }

  case '\'':
  case '"':
    token.data = TOKEN::PAYLOAD_STR{.sep = *ch};
    return parse(PARSE_STRING{}).transform([this](std::monostate) {
      token.kind = K_TOKEN_STRING;
      token.str().atom_idx =
          find_static_atom()
              .or_else([this](std::monostate) { return find_dynamic_atom(); })
              .value();
      return 0;
    });
  }

  if (u_isWhitespace(*ch))
    return 1;

  prev();
  token.data = TOKEN::PAYLOAD_IDENT{};
  std::expected ident_outcome = parse(PARSE_IDENT{});
  if (ident_outcome.value_or(1))
    return ident_outcome.transform([this](bool) {
      token.kind = K_TOKEN_IDENT;
      token.ident().atom_idx =
          find_static_atom()
              .or_else([this](std::monostate) { return find_dynamic_atom(); })
              .value();
      update_token_ident();
      return 0;
    });

  return next().transform([this](char32_t ch_tok) {
    token.kind = K_TOKEN_UCHAR;
    token.data = ch_tok;
    return 0;
  });
}

void ParseDriver::update_token_ident() {
  std::size_t i = token.ident().atom_idx;
  if (i >= reserved_arr.size())
    return;
  auto [literal, tok_kind, tok_strict] = reserved_arr[i];
  if (strictness < tok_strict)
    return;
  if (token.ident().has_escape)
    token.ident().is_reserved = 1;
  else
    token.kind = tok_kind;
}

std::expected<std::size_t, std::monostate> ParseDriver::find_static_atom() {
  for (std::size_t i = 0; i < reserved_arr.size(); i++) {
    auto [literal, _, _] = reserved_arr[i];
    if (str0_temp == literal)
      return i;
  }
  return std::unexpected{std::monostate{}};
}

TOKEN_KIND ParseDriver::parse_init(PARSE_STATEMENT) {
  if (token.is_pseudo_kind(K_TOKEN_LET))
    return K_TOKEN_LET;
  return token.kind;
}

EXPECT<std::monostate> ParseDriver::parse(PARSE_VARIABLE_DECLARATION) {
  std::expected tok_expect = parse(PARSE_TOKEN{});
  bool token_is_ident = tok_expect
                            .transform([this](std::monostate) {
                              return token.kind == K_TOKEN_IDENT;
                            })
                            .value_or(0);
  if (not token_is_ident)
    return std::unexpected{PARSE_ERRCODE::VARIABLE_NAME_EXPECTED};
  tok_expect = parse(PARSE_TOKEN{});
  bool has_assign =
      tok_expect
          .transform([this](std::monostate) {
            return token.kind == K_TOKEN_UCHAR && token.uchar() == '=';
          })
          .value_or(0);
  if (has_assign) {
    tok_expect = parse(PARSE_TOKEN{});
    tok_expect = parse(PARSE_TOKEN{});
  }
  return std::monostate{};
}

bool ParseDriver::parse() {
  std::expected intro = parse(PARSE_TOKEN{}).transform([this](std::monostate) {
    return parse_init(PARSE_STATEMENT{});
  });
  if (not intro)
    return 0;

  switch (*intro) {
  case K_TOKEN_CONST:
  case K_TOKEN_LET:
  case K_TOKEN_VAR:
    parse(PARSE_VARIABLE_DECLARATION{});
    return 1;
  }

  return 0;
}
} // namespace Manadrain
