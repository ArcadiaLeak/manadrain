#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include "manadrain.hpp"

namespace Manadrain {
static bool lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}
static const std::array reserved_arr =
    std::to_array<std::tuple<P_ATOM, KEYWORD_KIND, STRICTNESS>>(
        {{S_ATOM_var, KEYWORD_KIND::K_VAR, STRICTNESS::SLOPPY},
         {S_ATOM_const, KEYWORD_KIND::K_CONST, STRICTNESS::SLOPPY},
         {S_ATOM_let, KEYWORD_KIND::K_LET, STRICTNESS::STRICT}});

std::optional<KEYWORD_KIND> TOK_IDENTI::match_keyword(STRICTNESS strictness) {
  if (p_atom.pageid >= 0)
    return std::nullopt;
  for (auto rsv_word : reserved_arr) {
    auto [tok_atom, tok_kind, tok_strict] = rsv_word;
    if (p_atom == tok_atom && strictness >= tok_strict && !has_escape)
      return tok_kind;
  }
  return std::nullopt;
}

bool AtomPage::check_for_count(int N) { return ch_bitset.count() >= N; }

bool AtomPage::check_for_window(int N) {
  std::bitset inv_bitset = ~ch_bitset;
  for (int i = 0; i < N - 1; ++i) {
    if (inv_bitset.none())
      return 0;
    inv_bitset &= inv_bitset << 1;
  }
  return inv_bitset.any();
}

int AtomPage::scan_for_window(int N) {
  int n_zeros = 0;
  for (int i = 0; i < ch_bitset.size(); ++i) {
    if (ch_bitset[i])
      n_zeros = 0;
    else {
      n_zeros++;
      if (n_zeros == N) {
        int ret = i - N + 1;
        return ret;
      }
    }
  }
  throw std::runtime_error{"window seeking failed!"};
}

std::optional<std::uint16_t> AtomPage::try_allocate(int N) {
  if (not check_for_count(N))
    return std::nullopt;
  if (not check_for_window(N))
    return std::nullopt;
  int offset = scan_for_window(N);
  allocate(offset, N);
  return static_cast<std::uint16_t>(offset);
}

void AtomPage::allocate(int offset, int N) {
  for (int i = offset; i < offset + N; ++i)
    ch_bitset[i] = 1;
}

void ParseDriver::str1_encode(char32_t cp) {
  std::array<char, 4> buff{};
  UBool is_error{};
  std::uint32_t length{};
  U8_APPEND(buff.data(), length, buff.size(), cp, is_error);
  if (is_error)
    throw std::runtime_error{"codepoint conversion failed!"};
  str1_temp.append(std::string_view{buff.data(), length});
}

std::optional<char32_t> ParseDriver::next() {
  if (reached_eof())
    return std::nullopt;
  UChar32 ch;
  U8_NEXT_OR_FFFD(buffer.data(), buffer_idx, buffer.size(), ch);
  return ch;
}

void ParseDriver::prev() { U8_BACK_1(buffer.data(), 0, buffer_idx); }

std::optional<char32_t> ParseDriver::peek() {
  std::optional<char32_t> ahead = next();
  switch (ahead.has_value()) {
  case 0:
    return std::nullopt;
  case 1:
    prev();
    return ahead.value();
  }
}

void ParseDriver::backtrack(std::size_t N) {
  for (int i = 0; i < N; ++i)
    prev();
}

void ParseDriver::skip_lf() {
  if (reached_eof() || next().value() == '\n')
    return;
  prev();
}

char32_t ParseDriver::parse_octo(PARSE_ESCAPE, char32_t octo) {
  if (reached_eof())
    return octo;
  char32_t ahead = next().value() - '0';
  if (ahead > 7) {
    prev();
    return octo;
  }
  return (octo << 3) | ahead;
}

std::optional<int> hex_conv(char32_t digit) {
  if (digit >= '0' && digit <= '9')
    return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    return digit - 'a' + 10;
  return std::nullopt;
}

std::expected<char32_t, PARSE_ERRMSG> ParseDriver::parse_hex(PARSE_ESCAPE) {
  std::optional<int> hex0 = next().and_then(hex_conv);
  if (not hex0)
    return std::unexpected{ESCAPE_ERR::MALFORMED};
  std::optional<int> hex1 = next().and_then(hex_conv);
  if (not hex1)
    return std::unexpected{ESCAPE_ERR::MALFORMED};
  return (hex0.value() << 4) | hex1.value();
}

std::optional<std::expected<char32_t, PARSE_ERRMSG>>
ParseDriver::parse(PARSE_ESCAPE esc, char32_t ch) {
  switch (ch) {
  case 'b':
    return U'\b';
  case 'f':
    return U'\f';
  case 'n':
    return U'\n';
  case 'r':
    return U'\r';
  case 't':
    return U'\t';
  case 'v':
    return U'\v';
  case 'x':
    return parse_hex(PARSE_ESCAPE{});
  case '0':
    if (peek()
            .transform([](char32_t ahead) { return std::isdigit(ahead); })
            .value_or(0))
      return U'\0';
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
      return std::unexpected{ESCAPE_ERR::OCTAL_BANNED};

    case ESC_RULE::STRING_IN_TEMPLATE:
    case ESC_RULE::REGEXP_UTF16:
      return std::unexpected{ESCAPE_ERR::MALFORMED};

    default: {
      char32_t oct = ch - '0';
      oct = parse_octo(PARSE_ESCAPE{}, oct);
      if (oct >= 32)
        return oct;
      oct = parse_octo(PARSE_ESCAPE{}, oct);
      return oct;
    }
    }
  case '8':
  case '9':
    if (esc.rule == ESC_RULE::STRING_IN_STRICT_MODE ||
        esc.rule == ESC_RULE::STRING_IN_TEMPLATE)
      return std::unexpected{ESCAPE_ERR::MALFORMED};
    [[fallthrough]];
  default:
    return std::nullopt;
  }
}

std::optional<std::expected<char32_t, PARSE_ERRMSG>>
ParseDriver::parse_escape(PARSE_STRING, char32_t separator, char32_t ch) {
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
  else if (separator == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  return parse(PARSE_ESCAPE{esc_rule}, ch);
}

std::optional<std::expected<char32_t, PARSE_ERRMSG>>
ParseDriver::parse_uchar(PARSE_STRING, char32_t separator, char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
    if (separator == '`')
      return U'\n';
    return std::unexpected{UNEXPECTED_ERR::STRING_END};
  case '\\':
    if (reached_eof())
      return std::nullopt;
    return parse_escape(PARSE_STRING{}, separator, next().value());
  default:
    return ch;
  }
}

std::expected<void, PARSE_ERRMSG> ParseDriver::parse_atom(PARSE_STRING,
                                                          char32_t separator) {
  str1_temp = {};
  while (1) {
    if (reached_eof())
      return std::unexpected{UNEXPECTED_ERR::STRING_END};
    std::optional ch_opt =
        parse_uchar(PARSE_STRING{}, separator, next().value());
    if (ch_opt)
      switch (ch_opt->has_value()) {
      case 1: {
        if (separator == ch_opt->value())
          return {};
        str1_encode(ch_opt->value());
        continue;
      }
      default:
        return std::unexpected{ch_opt->error()};
      }
  }
}

std::optional<char32_t> ParseDriver::parse_uni_fixed(PARSE_IDENT,
                                                     char32_t leading) {
  char32_t curr{leading}, num{};
  for (int i = 0; i < 4; ++i) {
    std::optional hex = hex_conv(curr);
    if (not hex.has_value())
      return std::nullopt;
    num = (num << 4) | hex.value();
    if (reached_eof())
      return std::nullopt;
    curr = next().value();
  }
  return num;
}

std::optional<char32_t> ParseDriver::parse_uni_braced(PARSE_IDENT) {
  if (reached_eof())
    return std::nullopt;
  char32_t num = 0, curr{next().value()};
  for (int i = 0; i < 6; ++i) {
    std::optional hex = hex_conv(curr);
    if (not hex.has_value())
      break;
    num = (num << 4) | hex.value();
    if (num > UCHAR_MAX_VALUE)
      return std::nullopt;
    if (reached_eof())
      return std::nullopt;
    curr = next().value();
  }
  return curr == '}' ? std::make_optional(num) : std::nullopt;
}

std::optional<char32_t> ParseDriver::parse_uni(PARSE_IDENT) {
  if (reached_eof())
    return std::nullopt;
  char32_t leading = next().value();
  return leading == '{' ? parse_uni_braced(PARSE_IDENT{})
                        : parse_uni_fixed(PARSE_IDENT{}, leading);
}

bool ParseDriver::parse_uchar(PARSE_IDENT, char32_t ch) {
  bool is_legal = u_hasBinaryProperty(ch, str1_temp.size() ? UCHAR_XID_CONTINUE
                                                           : UCHAR_XID_START);
  if (is_legal)
    str1_encode(ch);
  return is_legal;
}

std::expected<int, PARSE_ERRMSG> ParseDriver::parse_escape(PARSE_IDENT,
                                                           char32_t leading) {
  bool has_escape{};
  if (leading == '\\') {
    if (not next().transform([](char32_t ch) { return ch == 'u'; }).value_or(0))
      return std::unexpected{ESCAPE_ERR::MALFORMED};
    std::optional<int> ch_uni = parse_uni(PARSE_IDENT{});
    switch (ch_uni.has_value()) {
    case 1:
      leading = ch_uni.value();
      has_escape = 1;
      break;
    default:
      return std::unexpected{ESCAPE_ERR::MALFORMED};
    }
  }
  bool success = parse_uchar(PARSE_IDENT{}, leading);
  if (has_escape) {
    if (success)
      return 2;
    return std::unexpected{ESCAPE_ERR::MALFORMED};
  }
  return success;
}

std::optional<std::expected<TOK_IDENTI, PARSE_ERRMSG>>
ParseDriver::parse(PARSE_IDENT) {
  str1_temp = {};
  bool has_escape = 0;

repeat: {
  if (reached_eof())
    goto done;
  std::expected esc_exp = parse_escape(PARSE_IDENT{}, next().value());
  switch (esc_exp.has_value()) {
  case 1:
    if (esc_exp.value() == 2)
      has_escape = 1;
    if (esc_exp.value())
      goto repeat;
    prev();
    goto done;
  default:
    return std::unexpected{esc_exp.error()};
  }
}

done:
  if (str1_temp.size() == 0)
    return std::nullopt;
  else {
    P_ATOM pos_atom = find_static_atom()
                          .or_else([this]() { return find_dynamic_atom(); })
                          .value();
    return TOK_IDENTI{has_escape, pos_atom};
  }
}

bool ParseDriver::skip_comment_line() {
  if (reached_eof())
    return 0;
  if (lineterm(next().value())) {
    prev();
    return 0;
  }
  return 1;
}

std::expected<bool, PARSE_ERRMSG> ParseDriver::skip_comment_block() {
  std::array<char32_t, 2> ch{};
  int i;
  for (i = 0; i < 2; ++i) {
    if (reached_eof())
      break;
    ch[i] = next().value();
  }
  if (i == 0)
    return std::unexpected{UNEXPECTED_ERR::COMMENT_END};
  if (ch[0] == '*' && ch[1] == '/')
    return 0;
  backtrack(--i);
  if (lineterm(ch[0]))
    newline_seen = 1;
  return 1;
}

std::expected<bool, PARSE_ERRMSG> ParseDriver::skip_comment(char32_t ch) {
  switch (ch) {
  case '/':
    while (1) {
      if (not skip_comment_line())
        return 1;
    }
  case '*':
    while (1) {
      std::expected comment = skip_comment_block();
      switch (comment.has_value()) {
      case 1:
        if (not comment.value())
          return 1;
        continue;
      case 0:
        return std::unexpected{comment.error()};
      }
    }
  default:
    prev();
    return 0;
  }
}

std::expected<bool, PARSE_ERRMSG> ParseDriver::skip_ws_1(char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];

  case '\n':
  case 0x2028:
  case 0x2029:
    newline_seen = 1;
    return 1;

  case '/': {
    if (reached_eof())
      return 1;
    std::expected comment_exp = skip_comment(next().value());
    if (comment_exp.has_value() && !comment_exp.value())
      prev();
    return comment_exp;
  }

  default: {
    bool ws = u_isWhitespace(ch);
    if (not ws)
      prev();
    return ws;
  }
  }
}

std::optional<TOKEN> ParseDriver::tokenize_lookahead(char32_t leading) {
  switch (leading) {
  case '0':
    switch (next().value()) {
    case 'x':
    case 'X':
      return std::nullopt;
    case 'o':
    case 'O':
      return std::nullopt;
    case 'b':
    case 'B':
      return std::nullopt;
    default:
      prev();
      break;
    }
    [[fallthrough]];
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9': {
    str1_temp = static_cast<char>(leading);
    while (std::isdigit(buffer[buffer_idx]))
      str1_temp.push_back(buffer[buffer_idx++]);
    if (peek()
            .transform([](char32_t ch) {
              return u_hasBinaryProperty(ch, UCHAR_XID_CONTINUE);
            })
            .value_or(0))
      return NUMBER_ERR::INVALID_LITERAL;
    int radix{10};
    if (leading == '0' && std::ranges::none_of(str1_temp, [](char ch) {
          return ch == '8' || ch == '9';
        }))
      radix = 8;
    std::uint64_t num{};
    std::from_chars_result res = std::from_chars(
        str1_temp.data(), str1_temp.data() + str1_temp.size(), num, radix);
    if (res.ec == std::errc::result_out_of_range ||
        num >= 1LL << std::numeric_limits<double>::digits)
      return NUMBER_ERR::INTEGER_OVERFLOW;
    return static_cast<double>(num);
  }
  case '\'':
  case '"': {
    std::expected str_exp = parse_atom(PARSE_STRING{}, leading);
    switch (str_exp.has_value()) {
    case 1: {
      P_ATOM pos_atom = find_static_atom()
                            .or_else([this]() { return find_dynamic_atom(); })
                            .value();
      return TOK_STRING{.separator = leading, .p_atom = pos_atom};
    }
    default:
      return str_exp.error();
    }
  }
  case '=': {
    int i;
    for (i = 0; i < 2; i++) {
      std::optional ahead = next();
      if (not ahead.transform([](char32_t ch) { return ch == '='; })
                  .value_or(0)) {
        if (ahead)
          prev();
        break;
      }
    }
    switch (i) {
    default:
      return U'=';
    case 1:
      return TOK_OPERATOR::EQ_SLOPPY;
    case 2:
      return TOK_OPERATOR::EQ_STRICT;
    }
  }
  default:
    prev();
    return std::nullopt;
  }
}

std::optional<TOKEN> ParseDriver::tokenize_identi_or_punct() {
  std::optional ident_opt = parse(PARSE_IDENT{});
  if (ident_opt)
    switch (ident_opt->has_value()) {
    case 1:
      return ident_opt->value();
    default:
      return ident_opt->error();
    }
  return next().value();
}

TOKEN ParseDriver::tokenize() {
  newline_seen = 0;
  bool go_on{1};
  while (go_on) {
    if (reached_eof())
      return std::monostate{};
    std::expected ws_exp = skip_ws_1(next().value());
    switch (ws_exp.has_value()) {
    case 1:
      go_on = ws_exp.value();
      break;
    case 0:
      return ws_exp.error();
    }
  }
  return tokenize_lookahead(next().value())
      .or_else([this]() { return tokenize_identi_or_punct(); })
      .value();
}

std::optional<P_ATOM> ParseDriver::find_static_atom() {
  for (std::uint16_t i = 0; i < reserved_arr.size(); i++) {
    auto [p_atom, _, _] = reserved_arr[i];
    if (str1_temp ==
        std::string_view{neg1_page_buf.data() + p_atom.offset * ATOM_BLOCK,
                         p_atom.length})
      return p_atom;
  }
  return std::nullopt;
}

std::uint16_t block_N(std::size_t len) {
  std::size_t N = len / ATOM_BLOCK + (len % ATOM_BLOCK != 0);
  return static_cast<std::uint16_t>(N % ATOM_PAGE);
}

std::optional<P_ATOM> ParseDriver::find_dynamic_atom() {
  if (str1_temp.size() > ATOM_BLOCK * ATOM_PAGE)
    throw std::runtime_error{"a string literal exceeds limit!"};
  if (not atom_umap.contains(str1_temp)) {
    P_ATOM p_atom = alloc_dynamic_atom();
    int ch_offset = p_atom.offset * ATOM_BLOCK;
    std::string_view atom_strview{
        atom_deq[p_atom.pageid].ch_arr.data() + ch_offset, p_atom.length};
    atom_umap[atom_strview] = p_atom;
  }
  return atom_umap[str1_temp];
}

P_ATOM ParseDriver::alloc_dynamic_atom() {
  std::uint16_t offset = 0,
                length = static_cast<std::uint16_t>(str1_temp.size());
  std::int16_t i;
  for (i = 0; i < atom_deq.size(); ++i) {
    std::optional offset_opt = atom_deq[i].try_allocate(block_N(length));
    if (not offset_opt)
      continue;
    offset = offset_opt.value();
    goto copy_and_return;
  }
  if (atom_deq.size() == std::numeric_limits<std::int16_t>::max())
    throw std::runtime_error{"out of space for an atom!"};
  atom_deq.emplace_back();
  atom_deq[i].allocate(0, block_N(length));

copy_and_return:
  std::ranges::copy(str1_temp,
                    atom_deq[i].ch_arr.begin() + offset * ATOM_BLOCK);
  return P_ATOM{i, offset, length};
}

std::expected<void, PARSE_ERRMSG>
ParseDriver::parse_variable_decl(std::size_t idx) {
  bool try_init{1};

  token_curr = tokenize();
  if (token_curr.index() != TOKV_IDENTI)
    return std::unexpected{NEEDED_ERR::VARIABLE_NAME};
  std::get<STMT_VARDECL>(program[idx]).identifier =
      std::get<TOK_IDENTI>(token_curr);
  token_curr = tokenize();

identif_end:
  if (try_init)
    goto check_init;
  return {};

check_init:
  if (token_curr == TOKEN{U'='}) {
    token_curr = tokenize();
    EXPR_PTR binary_expr = parse_binary_expr();
    switch (binary_expr->index()) {
    case EXPRV_ERROR:
      return std::unexpected{std::get<EXPRV_ERROR>(*binary_expr)};
    default:
      std::get<STMT_VARDECL>(program[idx]).initializer = binary_expr;
      break;
    }
  }
  try_init = 0;
  goto identif_end;
}

EXPR_PTR ParseDriver::parse_binary_expr() {
  EXPR_PTR lhs_expression = parse_postfix_expr();
  if (token_curr.index() != TOKV_OP)
    return lhs_expression;
  switch (std::get<TOKV_OP>(token_curr)) {
  case TOK_OPERATOR::EQ_SLOPPY:
  case TOK_OPERATOR::EQ_STRICT:
    break;
  default:
    return lhs_expression;
  }
  EXPR_PTR bin_expr = std::make_shared<EXPRESSION>(EXPR_BINARY{
      .left = lhs_expression, .bin_op = std::get<TOKV_OP>(token_curr)});
  token_curr = tokenize();
  EXPR_PTR rhs_expression = parse_binary_expr();
  if (rhs_expression->index() == EXPRV_ERROR)
    return rhs_expression;
  std::get_if<EXPR_BINARY>(bin_expr.get())->right = rhs_expression;
  return bin_expr;
}

EXPRESSION ParseDriver::parse_primary_expr(char32_t punct) {
  switch (std::get<TOKV_PUNCT>(token_curr)) {
  case '{':
    token_curr = tokenize();
    return token_curr == TOKEN{U'}'} ? EXPRESSION{EXPR_OBJECT{}}
                                     : EXPRESSION{NEEDED_ERR::CLOSING_BRACE};
  default:
    return UNEXPECTED_ERR::THIS_TOKEN;
  }
}

EXPRESSION ParseDriver::parse_primary_expr() {
  switch (token_curr.index()) {
  case TOKV_STRING:
    return std::get<TOKV_STRING>(token_curr);
  case TOKV_IDENTI:
    return std::get<TOKV_IDENTI>(token_curr);
  case TOKV_NUMBER:
    return std::get<TOKV_NUMBER>(token_curr);
  case TOKV_PUNCT:
    return parse_primary_expr(std::get<TOKV_PUNCT>(token_curr));
  case TOKV_ERROR:
    return std::get<TOKV_ERROR>(token_curr);
  default:
    return UNEXPECTED_ERR::THIS_TOKEN;
  }
}

std::pair<bool, EXPR_PTR> ParseDriver::parse_arg_expr() {
  token_curr = tokenize();
  if (token_curr == TOKEN{U')'})
    return {0, nullptr};
  EXPR_PTR arg_expr = parse_binary_expr();
  if (token_curr == TOKEN{U')'})
    return std::make_pair(0, arg_expr);
  if (token_curr != TOKEN{U','})
    return std::make_pair(0, std::make_shared<EXPRESSION>(NEEDED_ERR::COMMA));
  else
    return std::make_pair(1, arg_expr);
}

EXPR_PTR ParseDriver::parse_call_expr(EXPR_PTR callee) {
  EXPR_PTR call_expr = std::make_shared<EXPRESSION>(EXPR_CALL{callee});
  bool go_on{1};
  while (go_on) {
    std::pair<bool, EXPR_PTR> arg_expr = parse_arg_expr();
    if (not arg_expr.second)
      break;
    if (arg_expr.second->index() == EXPRV_ERROR)
      return arg_expr.second;
    go_on = arg_expr.first;
    std::get_if<EXPR_CALL>(call_expr.get())
        ->arguments.push_back(arg_expr.second);
  }
  return call_expr;
}

EXPR_PTR ParseDriver::parse_member_expr(EXPR_PTR object) {
  token_curr = tokenize();
  return token_curr.index() == TOKV_IDENTI
             ? std::make_shared<EXPRESSION>(
                   EXPR_MEMBER{object, std::get<TOK_IDENTI>(token_curr)})
             : std::make_shared<EXPRESSION>(NEEDED_ERR::FIELD_NAME);
}

EXPR_PTR ParseDriver::parse_array_access(EXPR_PTR object) {
  token_curr = tokenize();
  EXPR_PTR property_expr = parse_binary_expr();
  if (property_expr->index() == EXPRV_ERROR)
    return property_expr;
  return token_curr == TOKEN{U']'}
             ? std::make_shared<EXPRESSION>(
                   EXPR_ARRACCESS{object, property_expr})
             : std::make_shared<EXPRESSION>(NEEDED_ERR::CLOSING_BRACKET);
}

std::pair<bool, EXPR_PTR> ParseDriver::parse_postfix_expr(EXPR_PTR expression) {
  switch (std::get<TOKV_PUNCT>(token_curr)) {
  case '.':
    return std::make_pair(1, parse_member_expr(expression));
  case '(':
    return std::make_pair(1, parse_call_expr(expression));
  case '[':
    return std::make_pair(1, parse_array_access(expression));
  default:
    return std::make_pair(0, expression);
  }
}

EXPR_PTR ParseDriver::parse_postfix_expr() {
  EXPR_PTR expression = std::make_shared<EXPRESSION>(parse_primary_expr());
  switch (expression->index()) {
  case EXPRV_ERROR:
    return expression;
  default:
    break;
  }
  bool go_on{1};
  while (go_on) {
    token_curr = tokenize();
    if (token_curr.index() != TOKV_PUNCT)
      break;
    auto postfix_expr = parse_postfix_expr(expression);
    switch (postfix_expr.second->index()) {
    case EXPRV_ERROR:
      return postfix_expr.second;
    default:
      go_on = postfix_expr.first;
      expression = postfix_expr.second;
      break;
    }
  }
  return expression;
}

bool ParseDriver::parse() {
  token_curr = tokenize();
  while (1) {
    std::optional<PARSE_ERRMSG> errmsg{};

    switch (token_curr.index()) {
    case TOKV_EOF:
      return 1;
    case TOKV_ERROR:
      errmsg = std::get<TOKV_ERROR>(token_curr);
      goto fail;
    case TOKV_IDENTI:
      if (std::get<TOKV_IDENTI>(token_curr).match_keyword(STRICTNESS::STRICT))
        goto parse_vardecl;
      [[fallthrough]];
    default:
      goto parse_binary;
    }

  fail:
    if (not errmsg)
      throw std::runtime_error{"parsing failed without a message!"};
    return 0;

  parse_vardecl: {
    program.emplace_back(
        STMT_VARDECL{.rule = std::get<TOKV_IDENTI>(token_curr)
                                 .match_keyword(STRICTNESS::STRICT)
                                 .value()});
    std::expected decl_exp = parse_variable_decl(program.size() - 1);
    switch (decl_exp.has_value()) {
    case 1:
      goto statement_end;
    case 0:
      errmsg = decl_exp.error();
      goto fail;
    }
  }

  parse_binary: {
    EXPR_PTR binary_expr = parse_binary_expr();
    switch (binary_expr->index()) {
    default:
      program.push_back(*binary_expr);
      goto statement_end;
    case EXPRV_ERROR:
      errmsg = std::get<EXPRV_ERROR>(*binary_expr);
      goto fail;
    }
  }

  statement_end:
    if (token_curr == TOKEN{U';'}) {
      token_curr = tokenize();
      continue;
    } else if (token_curr.index() == TOKV_EOF || token_curr == TOKEN{U'}'} ||
               newline_seen)
      continue;
    errmsg = NEEDED_ERR::SEMICOLON;
    goto fail;
  }
}
} // namespace Manadrain
