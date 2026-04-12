#include <stdexcept>

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
static const std::array reserved_arr =
    std::to_array<std::tuple<std::string_view, TOKEN_KIND, STRICTNESS>>(
        {{"var", K_TOKEN_VAR, STRICTNESS::SLOPPY},
         {"const", K_TOKEN_CONST, STRICTNESS::SLOPPY},
         {"let", K_TOKEN_LET, STRICTNESS::STRICT}});

bool token_is_pseudo_keyword(TOKEN &token, TOKEN_KIND keyword_kind) {
  TOKEN::PAYLOAD_IDENT identifier_ref =
      std::get<TOKEN::PAYLOAD_IDENT>(token.data);
  if (identifier_ref.has_escape)
    return 0;
  if (identifier_ref.atom_idx >= reserved_arr.size())
    return 0;
  auto [_, pseudo_kind, _] = reserved_arr[identifier_ref.atom_idx];
  if (pseudo_kind == keyword_kind)
    return 1;
  return 0;
}

bool token_is_uchar(TOKEN &token, char32_t uchar) {
  return token.kind == K_TOKEN_UCHAR && std::get<char32_t>(token.data) == uchar;
}

void ParseDriver::codepoint_cv(char32_t cp) {
  std::array<char, 4> buff{};
  UBool is_error{};
  std::uint32_t length{};
  U8_APPEND(buff.data(), length, buff.size(), cp, is_error);
  if (is_error)
    throw std::runtime_error{"codepoint conversion failed!"};
  str1_temp.append(std::string_view{buff.data(), length});
}

std::optional<std::size_t> ParseDriver::find_dynamic_atom() {
  if (not atom_umap.contains(str1_temp)) {
    atom_deq.push_back(std::move(str1_temp));
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

std::size_t ParseDriver::backtrack(std::size_t N) {
  std::size_t i;
  for (i = 0; i < N; ++i) {
    std::expected ch{prev()};
    if (not ch)
      break;
  }
  return i;
}

std::u32string_view ParseDriver::take(int N) {
  str4_temp.clear();
  while (str4_temp.size() < N) {
    std::expected ch{next()};
    if (not ch)
      break;
    str4_temp.push_back(*ch);
  }
  return str4_temp;
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
      take(2) == U"\\u") {
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
  else if (std::get<TOKEN::PAYLOAD_STR>(token.data).separator == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  return parse(PARSE_ESCAPE{esc_rule}, ch);
}

EXPECT_OPT<char32_t> ParseDriver::parse_uchar(PARSE_STRING, char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
    if (std::get<TOKEN::PAYLOAD_STR>(token.data).separator == '`')
      return '\n';
    return std::unexpected{PARSE_ERRCODE::UNEXPECTED_STRING_END};
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

bool ParseDriver::should_append(PARSE_STRING, char32_t ch) {
  char32_t separator = std::get<TOKEN::PAYLOAD_STR>(token.data).separator;
  if (ch == separator)
    return 0;
  std::expected ahead = next();
  if (separator == '`' && ch == '$' && ahead == '{')
    return 0;
  if (ahead)
    prev();
  return 1;
}

EXPECT<void> ParseDriver::parse(PARSE_STRING) {
  str1_temp.clear();
  EXPECT_OPT<char32_t> uchar_expect_opt{0};
  while (uchar_expect_opt) {
    uchar_expect_opt = next()
                           .transform_error([](PARSE_ERRCODE) {
                             return PARSE_ERRCODE::UNEXPECTED_STRING_END;
                           })
                           .and_then([this](char32_t ch) {
                             return parse_uchar(PARSE_STRING{}, ch);
                           });
    std::optional should_append_opt =
        uchar_expect_opt.value_or(std::nullopt).transform([this](char32_t ch) {
          return should_append(PARSE_STRING{}, ch);
        });
    if (not should_append_opt)
      continue;
    if (not *should_append_opt)
      break;
    codepoint_cv(**uchar_expect_opt);
  }
  return uchar_expect_opt.transform([](auto) { return; });
}

bool ParseDriver::is_allowed_uchar(PARSE_IDENT ident, char32_t ch) {
  UProperty must_be = str1_temp.size() == ident.is_private ? UCHAR_XID_START
                                                           : UCHAR_XID_CONTINUE;
  return u_hasBinaryProperty(ch, must_be);
}

EXPECT<bool> ParseDriver::parse_uchar(PARSE_IDENT ident) {
  if (take(2) == U"\\u") {
    std::get<TOKEN::PAYLOAD_IDENT>(token.data).has_escape = 1;
    return parse_uni(PARSE_ESCAPE{ESC_RULE::IDENTIFIER})
        .transform([this, ident](char32_t ch) {
          if (not is_allowed_uchar(ident, ch))
            return 0;
          codepoint_cv(ch);
          return 1;
        });
  } else {
    int i;
    for (i = 0; i < str4_temp.size(); i++) {
      char32_t ch = str4_temp[i];
      if (not is_allowed_uchar(ident, ch))
        break;
      codepoint_cv(ch);
    }
    backtrack(str4_temp.size() - i);
    return i > 0;
  }
}

EXPECT<bool> ParseDriver::parse(PARSE_IDENT ident) {
  str1_temp.clear();
  if (ident.is_private)
    str1_temp.push_back('#');

  EXPECT<bool> uchar_exp{1};
  while (uchar_exp.value_or(0))
    uchar_exp = parse_uchar(ident);
  return uchar_exp.transform(
      [this, ident](bool) { return str1_temp.size() > ident.is_private; });
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
  if (take(2) == U"*/")
    return 0;
  if (str4_temp.empty())
    return std::unexpected{PARSE_ERRCODE::UNEXPECTED_COMMENT_END};
  if (is_lineterm(str4_temp.front()))
    token.newline_seen = 1;
  str4_temp.pop_back();
  backtrack(str4_temp.size());
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
    token.data = TOKEN::PAYLOAD_STR{.separator = *ch};
    return parse(PARSE_STRING{}).transform([this]() {
      token.kind = K_TOKEN_STRING;
      std::get<TOKEN::PAYLOAD_STR>(token.data).atom_idx =
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
      std::get<TOKEN::PAYLOAD_IDENT>(token.data).atom_idx =
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
  TOKEN::PAYLOAD_IDENT &identifier_ref =
      std::get<TOKEN::PAYLOAD_IDENT>(token.data);
  std::size_t i = identifier_ref.atom_idx;
  if (i >= reserved_arr.size())
    return;
  auto [literal, tok_kind, tok_strict] = reserved_arr[i];
  if (strictness < tok_strict)
    return;
  if (identifier_ref.has_escape)
    identifier_ref.is_reserved = 1;
  else
    token.kind = tok_kind;
}

std::optional<std::size_t> ParseDriver::find_static_atom() {
  for (std::size_t i = 0; i < reserved_arr.size(); i++) {
    auto [literal, _, _] = reserved_arr[i];
    if (str1_temp == literal)
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
  STMT_VARDECL &declaration_ref = std::get<STMT_VARDECL>(program.back());
  std::expected identifier_exp = parse(PARSE_TOKEN{}).and_then([this]() {
    return token.kind == K_TOKEN_IDENT
               ? EXPECT<void>{}
               : EXPECT<void>{std::unexpect,
                              PARSE_ERRCODE::NEEDED_VARIABLE_NAME};
  });
  if (identifier_exp)
    declaration_ref.identifier = std::get<TOKEN::PAYLOAD_IDENT>(token.data);
  std::expected postfix_exp =
      identifier_exp.and_then([this]() { return parse(PARSE_TOKEN{}); })
          .and_then([this]() {
            return token_is_uchar(token, '=') ? parse(PARSE_POSTFIX_EXPR{})
                                              : EXPECT<bool>{0};
          });
  if (postfix_exp.value_or(0)) {
    declaration_ref.initializer = expression;
    postfix_exp = parse(PARSE_TOKEN{}).transform([]() { return 0; });
  }
  if (postfix_exp
          .transform([this](bool) { return !token_is_uchar(token, ';'); })
          .value_or(0)) {
    token.data = TOKEN::PAYLOAD_ERR{U';'};
    return EXPECT<void>{std::unexpect, PARSE_ERRCODE::NEEDED_SPECIFICLY};
  }
  return postfix_exp.transform([](bool) { return; });
}

EXPECT<bool> ParseDriver::parse(PARSE_POSTFIX_EXPR) {
  return parse(PARSE_TOKEN{}).and_then([this]() {
    switch (token.kind) {
    case K_TOKEN_STRING:
      expression = std::get<TOKEN::PAYLOAD_STR>(token.data);
      return EXPECT<bool>{1};
    default:
      return EXPECT<bool>{std::unexpect, PARSE_ERRCODE::UNEXPECTED_TOKEN};
    }
  });
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
