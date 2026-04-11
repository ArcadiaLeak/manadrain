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
TOKEN::PAYLOAD_STR &tok_string(TOKEN &token) {
  return std::get<TOKEN::PAYLOAD_STR>(token.data);
}
TOKEN::PAYLOAD_IDENT &tok_identifier(TOKEN &token) {
  return std::get<TOKEN::PAYLOAD_IDENT>(token.data);
}

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

bool token_is_pseudo_keyword(TOKEN &token, TOKEN_KIND keyword_kind) {
  if (tok_identifier(token).has_escape)
    return 0;
  if (tok_identifier(token).atom_idx >= reserved_arr.size())
    return 0;
  auto [_, pseudo_kind, _] = reserved_arr[tok_identifier(token).atom_idx];
  if (pseudo_kind == keyword_kind)
    return 1;
  return 0;
}

bool token_is_uchar(TOKEN &token, char32_t uchar) {
  return token.kind == K_TOKEN_UCHAR && std::get<char32_t>(token.data) == uchar;
}

std::optional<std::size_t> ParseDriver::find_dynamic_atom() {
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
    ++mov_since_mark;
    return ch;
  }
  return std::unexpected{PARSE_ERRCODE::OUT_OF_RANGE};
}

EXPECT<char32_t> ParseDriver::prev() {
  if (0 < buffer_idx) {
    UChar32 ch;
    U8_PREV_OR_FFFD(buffer.data(), 0, buffer_idx, ch);
    --mov_since_mark;
    return ch;
  }
  return std::unexpected{PARSE_ERRCODE::OUT_OF_RANGE};
}

int ParseDriver::backtrack(int N) {
  int i = 0;
  for (i = 0; i < N; ++i) {
    std::expected ch{prev()};
    if (not ch)
      break;
  }
  return i;
}

std::string_view ParseDriver::take(int N) {
  str1_temp.clear();
  for (mov_in_take = 0; mov_in_take < N; ++mov_in_take) {
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
  return std::unexpected{PARSE_ERRCODE::MALFORMED_ESCAPE};
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
  mov_since_mark = 0;
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
  backtrack(mov_since_mark);
  return std::unexpected{PARSE_ERRCODE::MALFORMED_ESCAPE};
}

EXPECT<char32_t> ParseDriver::parse_uni_fixed(PARSE_ESCAPE esc) {
  char32_t high_surr{}, low_surr{};

  for (int i = 0; i < 4; i++) {
    std::expected hex = parse_hex();
    if (not hex) {
      backtrack(i);
      return std::unexpected{PARSE_ERRCODE::MALFORMED_ESCAPE};
    }
    high_surr = (high_surr << 4) | *hex;
  }

  mov_since_mark = 0;
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
  backtrack(mov_since_mark);
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
      return std::unexpected{PARSE_ERRCODE::MALFORMED_ESCAPE};
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
      return std::unexpected{PARSE_ERRCODE::LEGACY_OCTAL_SEQ};

    case ESC_RULE::STRING_IN_TEMPLATE:
    case ESC_RULE::REGEXP_UTF16:
      return std::unexpected{PARSE_ERRCODE::MALFORMED_ESCAPE};

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
      return std::unexpected{PARSE_ERRCODE::MALFORMED_ESCAPE};
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
  else if (tok_string(token).sep == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  return parse(PARSE_ESCAPE{esc_rule}, ch);
}

EXPECT_OPT<char32_t> ParseDriver::parse_uchar(PARSE_STRING, char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
    if (tok_string(token).sep == '`')
      return '\n';
    return std::unexpected{PARSE_ERRCODE::STRING_UNEXPECTED_END};
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

EXPECT<void> ParseDriver::parse(PARSE_STRING) {
  str0_temp.clear();
  EXPECT_OPT<char32_t> ch_exp{0};
  while (ch_exp) {
    ch_exp = next()
                 .transform_error([](PARSE_ERRCODE) {
                   return PARSE_ERRCODE::STRING_UNEXPECTED_END;
                 })
                 .and_then([this](char32_t ch) {
                   return parse_uchar(PARSE_STRING{}, ch);
                 });
    if (not ch_exp.value_or(std::nullopt))
      continue;
    char32_t ch = **ch_exp;
    if (ch == tok_string(token).sep)
      break;
    mov_since_mark = 0;
    if (tok_string(token).sep == '`' && ch == '$' && next() == '{')
      break;
    backtrack(mov_since_mark);
    ENCODED_POINT cp = codepoint_cv(ch);
    str0_temp.append(cp.sv());
  }
  return ch_exp.transform([](auto) { return; });
}

EXPECT<ENCODED_POINT> ParseDriver::parse_uchar(PARSE_IDENT, bool beginning) {
  std::string ahead{take(2)};
  if (ahead == "\\u")
    tok_identifier(token).has_escape = 1;
  else
    backtrack(mov_in_take);
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
    std::expected conv_ch = parse_uchar(PARSE_IDENT{}, beginning);
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

EXPECT<void> ParseDriver::parse(PARSE_TOKEN) {
  EXPECT<bool> ret{1};
  while (ret.value_or(0))
    ret = parse_iter(PARSE_TOKEN{});
  return ret.transform([](bool) { return; });
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
    return std::unexpected{PARSE_ERRCODE::COMMENT_UNEXPECTED_END};
  prev();
  if (take(2) == "*/")
    return 0;
  backtrack(mov_in_take);
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
    return parse(PARSE_STRING{}).transform([this]() {
      token.kind = K_TOKEN_STRING;
      tok_string(token).atom_idx =
          find_static_atom()
              .or_else([this]() { return find_dynamic_atom(); })
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
      tok_identifier(token).atom_idx =
          find_static_atom()
              .or_else([this]() { return find_dynamic_atom(); })
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
  std::size_t i = tok_identifier(token).atom_idx;
  if (i >= reserved_arr.size())
    return;
  auto [literal, tok_kind, tok_strict] = reserved_arr[i];
  if (strictness < tok_strict)
    return;
  if (tok_identifier(token).has_escape)
    tok_identifier(token).is_reserved = 1;
  else
    token.kind = tok_kind;
}

std::optional<std::size_t> ParseDriver::find_static_atom() {
  for (std::size_t i = 0; i < reserved_arr.size(); i++) {
    auto [literal, _, _] = reserved_arr[i];
    if (str0_temp == literal)
      return i;
  }
  return std::nullopt;
}

EXPECT<TOKEN_KIND> ParseDriver::parse_init(PARSE_STATEMENT) {
  return parse(PARSE_TOKEN{}).transform([this]() {
    if (token.kind == K_TOKEN_IDENT)
      return token_is_pseudo_keyword(token, K_TOKEN_LET) ? K_TOKEN_LET
                                                         : K_TOKEN_IDENT;
    return token.kind;
  });
}

EXPECT<void> ParseDriver::parse(PARSE_VARDECL) {
  STMT_VARDECL &vardecl = std::get<STMT_VARDECL>(program.back());

  return parse(PARSE_TOKEN{})
      .and_then([this, &vardecl]() {
        if (token.kind == K_TOKEN_IDENT) {
          vardecl.ident = tok_identifier(token);
          return EXPECT<void>{};
        }
        return EXPECT<void>{std::unexpect,
                            PARSE_ERRCODE::VARIABLE_NAME_EXPECTED};
      })
      .and_then([this]() { return parse(PARSE_TOKEN{}); })
      .and_then([this]() {
        if (token_is_uchar(token, '=')) {
          return parse(PARSE_POSTFIX_EXPR{}).and_then([this]() {
            return parse(PARSE_TOKEN{});
          });
        }
        return EXPECT<void>{};
      })
      .and_then([this, &vardecl]() {
        if (token_is_uchar(token, ';')) {
          return EXPECT<void>{};
        }
        return EXPECT<void>{std::unexpect,
                            PARSE_ERRCODE::VARIABLE_NAME_EXPECTED};
      });
}

EXPECT<void> ParseDriver::parse(PARSE_POSTFIX_EXPR) {
  return parse(PARSE_TOKEN{});
}

bool ParseDriver::parse() {
  return parse_init(PARSE_STATEMENT{})
      .transform([this](TOKEN_KIND kind) {
        switch (kind) {
        case K_TOKEN_CONST:
        case K_TOKEN_LET:
        case K_TOKEN_VAR:
          program.emplace_back(STMT_VARDECL{.intro = kind});
          parse(PARSE_VARDECL{});
          return 1;
        }

        return 0;
      })
      .value_or(0);
}
} // namespace Manadrain
