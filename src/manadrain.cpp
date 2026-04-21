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

void AtomTokenizer::str1_encode(char32_t cp) {
  std::array<char, 4> buff{};
  UBool is_error{};
  std::uint32_t length{};
  U8_APPEND(buff.data(), length, buff.size(), cp, is_error);
  if (is_error)
    throw std::runtime_error{"codepoint conversion failed!"};
  str1_temp.append(std::string_view{buff.data(), length});
}

std::optional<char32_t> Scanner::next() {
  if (reached_eof())
    return std::nullopt;
  UChar32 ch;
  U8_NEXT_OR_FFFD(buffer.data(), buffer_idx, buffer.size(), ch);
  return ch;
}

void Scanner::prev() { U8_BACK_1(buffer.data(), 0, buffer_idx); }

std::optional<char32_t> Scanner::peek() {
  std::optional<char32_t> ahead = next();
  switch (ahead.has_value()) {
  case 0:
    return std::nullopt;
  case 1:
    prev();
    return ahead.value();
  }
}

void Scanner::backtrack(std::size_t N) {
  for (int i = 0; i < N; ++i)
    prev();
}

void SpaceChewer::chewLF() {
  if (reached_eof() || next().value() == '\n')
    return;
  prev();
}

std::optional<char32_t> StringTokenizer::decode_octal() {
  if (reached_eof())
    return std::nullopt;
  char32_t ahead = next().value();
  if (ahead > '7') {
    prev();
    return std::nullopt;
  }
  return ahead - '0';
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

std::optional<char32_t> StringTokenizer::decode_hex() {
  std::optional<int> hex0 = next().and_then(hex_conv);
  if (not hex0)
    return std::nullopt;
  std::optional<int> hex1 = next().and_then(hex_conv);
  if (not hex1)
    return std::nullopt;
  return (hex0.value() << 4) | hex1.value();
}

std::variant<std::monostate, char32_t, PARSE_ERRMSG>
StringTokenizer::decode_escape(char32_t ch) {
  switch (ch) {
  case '\'':
  case '\"':
  case '\0':
  case '\\':
    return ch;
  case '\r':
    chewLF();
    [[fallthrough]];
  case '\n':
  case 0x2028:
  case 0x2029:
    /* ignore escaped newline sequence */
    return std::monostate{};
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
  case 'x': {
    std::optional hex = decode_hex();
    if (not hex)
      return ESCAPE_ERR::MALFORMED;
    return hex.value();
  }
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
  case '7': {
    std::optional<char32_t> dec{};
    char32_t base = ch - '0';
    dec = decode_octal();
    if (not dec)
      return base;
    base = (base << 3) | dec.value();
    if (base >= 32)
      return base;
    dec = decode_octal();
    if (not dec)
      return base;
    base = (base << 3) | dec.value();
    return base;
  }
  case '8':
  case '9':
    return ESCAPE_ERR::MALFORMED;
  default:
    return std::monostate{};
  }
}

std::variant<std::monostate, char32_t, PARSE_ERRMSG>
StringTokenizer::decode_special(char32_t separator, char32_t ch) {
  switch (ch) {
  case '\r':
    chewLF();
    [[fallthrough]];
  case '\n':
    if (separator == '`')
      return U'\n';
    return UNEXPECTED_ERR::STRING_END;
  case '\\':
    if (reached_eof())
      return std::monostate{};
    return decode_escape(next().value());
  default:
    return ch;
  }
}

TOKEN StringTokenizer::tokenize(char32_t separator) {
  while (1) {
    if (reached_eof())
      return UNEXPECTED_ERR::STRING_END;
    std::variant ch_alter = decode_special(separator, next().value());
    switch (ch_alter.index()) {
    case 0:
      break;
    case 1: {
      char32_t ch = std::get<1>(ch_alter);
      if (ch == separator)
        return TOK_STRING{separator, find_atom()};
      str1_encode(ch);
      continue;
    }
    default:
      return std::get<2>(ch_alter);
    }
  }
}

std::optional<char32_t>
IdentifierTokenizer::decode_uni_fixed(char32_t leading) {
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

std::optional<char32_t> IdentifierTokenizer::decode_uni_braced() {
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

std::optional<char32_t> IdentifierTokenizer::decode_uni() {
  if (reached_eof())
    return std::nullopt;
  char32_t leading = next().value();
  return leading == '{' ? decode_uni_braced() : decode_uni_fixed(leading);
}

bool IdentifierTokenizer::encode_uchar(char32_t ch) {
  bool is_legal = u_hasBinaryProperty(ch, str1_temp.size() ? UCHAR_XID_CONTINUE
                                                           : UCHAR_XID_START);
  if (is_legal)
    str1_encode(ch);
  return is_legal;
}

std::expected<int, PARSE_ERRMSG>
IdentifierTokenizer::decode_escape(char32_t leading) {
  bool has_escape{};
  if (leading == '\\') {
    if (not next().transform([](char32_t ch) { return ch == 'u'; }).value_or(0))
      return std::unexpected{ESCAPE_ERR::MALFORMED};
    std::optional<int> ch_uni = decode_uni();
    switch (ch_uni.has_value()) {
    case 1:
      leading = ch_uni.value();
      has_escape = 1;
      break;
    default:
      return std::unexpected{ESCAPE_ERR::MALFORMED};
    }
  }
  bool success = encode_uchar(leading);
  if (has_escape) {
    if (success)
      return 2;
    return std::unexpected{ESCAPE_ERR::MALFORMED};
  }
  return success;
}

std::optional<TOKEN> IdentifierTokenizer::tokenize(char32_t leading) {
  bool has_escape = 0;
  while (1) {
    std::expected esc_exp = decode_escape(leading);
    if (not esc_exp)
      return esc_exp.error();
    switch (esc_exp.value()) {
    case 2:
      has_escape = 1;
      [[fallthrough]];
    case 1:
      if (reached_eof())
        break;
      leading = next().value();
      continue;
    }
    break;
  }
  if (str1_temp.size() == 0)
    return std::nullopt;
  return TOK_IDENTI{has_escape, find_atom()};
}

bool SpaceChewer::chew_comment_line() {
  if (reached_eof())
    return 0;
  if (lineterm(next().value())) {
    prev();
    return 0;
  }
  return 1;
}

std::expected<bool, PARSE_ERRMSG> SpaceChewer::chew_comment_block() {
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

std::expected<bool, PARSE_ERRMSG> SpaceChewer::chew_comment(char32_t ch) {
  switch (ch) {
  case '/':
    while (1) {
      if (not chew_comment_line())
        return 1;
    }
  case '*':
    while (1) {
      std::expected comment = chew_comment_block();
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

std::expected<bool, PARSE_ERRMSG> SpaceChewer::chewSpace1(char32_t ch) {
  switch (ch) {
  case '\r':
    chewLF();
    [[fallthrough]];

  case '\n':
  case 0x2028:
  case 0x2029:
    newline_seen = 1;
    return 1;

  case '/': {
    if (reached_eof())
      return 1;
    std::expected comment_exp = chew_comment(next().value());
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

std::optional<TOKEN> Tokenizer::tokenize_lookahead(char32_t leading) {
  switch (leading) {
  case '\'':
  case '"': {
    TOKEN str_token = StringTokenizer::tokenize(leading);
    str1_temp = {};
    return str_token;
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
    return std::nullopt;
  }
}

TOKEN Tokenizer::tokenize() {
  unseeNewline();
  bool go_on{1};
  while (go_on) {
    if (reached_eof())
      return std::monostate{};
    std::expected ws_exp = chewSpace1(next().value());
    switch (ws_exp.has_value()) {
    case 1:
      go_on = ws_exp.value();
      break;
    case 0:
      return ws_exp.error();
    }
  }
  char32_t leading = next().value();
  std::optional<TOKEN> token_opt = tokenize_lookahead(leading);
  if (token_opt)
    return token_opt.value();
  token_opt = NumberTokenizer::tokenize(leading);
  if (token_opt)
    return token_opt.value();
  token_opt = IdentifierTokenizer::tokenize(leading);
  str1_temp = {};
  if (token_opt)
    return token_opt.value();
  return leading;
}

std::size_t aligned_N(std::size_t len) {
  return len / MEMORY_ALIGNMENT + (len % MEMORY_ALIGNMENT != 0);
}

std::size_t AtomTokenizer::find_atom() {
  if (not atom_umap.contains(str1_temp))
    atom_umap[str1_temp] = alloc_atom();
  return atom_umap[str1_temp];
}

std::array<char, 8> LE_encode(std::size_t N) {
  std::array<char, 8> bytes;
  for (int i = 0; i < 8; ++i)
    bytes[i] = static_cast<char>((N >> (i * 8)) & 0xFF);
  return bytes;
}

std::size_t LE_decode(std::span<char, 8> bytes) {
  std::size_t N = 0;
  for (std::size_t i = 0; i < 8; ++i)
    N |= static_cast<std::size_t>(bytes[i]) << (i * 8);
  return N;
}

std::size_t AtomTokenizer::alloc_atom() {
  std::size_t atom_addr = atom_arena.size();
  atom_arena.append_range(LE_encode(str1_temp.size()));
  str1_temp.resize(aligned_N(str1_temp.size()));
  atom_arena.append_range(str1_temp);
  return atom_addr;
}

std::optional<TOKEN> NumberTokenizer::tokenize(char32_t leading) {
  return std::nullopt;
}

std::variant<std::monostate, STMT_VARDECL, PARSE_ERRMSG>
Parser::parse_variable_decl() {
  STMT_VARDECL declaration{};

  std::size_t p_atom = std::get<TOKV_IDENTI>(token_curr).p_atom;
  switch (p_atom) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var:
    declaration.p_kind = p_atom;
    token_curr = tokenize();
    break;
  default:
    return std::monostate{};
  }

  if (token_curr.index() != TOKV_IDENTI)
    return NEEDED_ERR::VARIABLE_NAME;
  declaration.identifier = std::get<TOK_IDENTI>(token_curr);
  token_curr = tokenize();

  if (token_curr != TOKEN{U'='})
    return declaration;
  token_curr = tokenize();

  EXPR_PTR initializer = parse_assign_expr();
  if (initializer->index() == EXPRV_ERROR)
    return std::get<EXPRV_ERROR>(*initializer);
  declaration.initializer = initializer;

  return declaration;
}

EXPR_PTR Parser::parse_binary_expr() {
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

EXPRESSION Parser::parse_primary_expr(char32_t punct) {
  switch (std::get<TOKV_PUNCT>(token_curr)) {
  case '{':
    token_curr = tokenize();
    return token_curr == TOKEN{U'}'} ? EXPRESSION{EXPR_OBJECT{}}
                                     : EXPRESSION{NEEDED_ERR::CLOSING_BRACE};
  default:
    return UNEXPECTED_ERR::THIS_TOKEN;
  }
}

EXPRESSION Parser::parse_primary_expr() {
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

std::pair<bool, EXPR_PTR> Parser::parse_arg_expr() {
  token_curr = tokenize();
  if (token_curr == TOKEN{U')'})
    return {0, nullptr};
  EXPR_PTR arg_expr = parse_assign_expr();
  if (token_curr == TOKEN{U')'})
    return std::make_pair(0, arg_expr);
  if (token_curr != TOKEN{U','})
    return std::make_pair(0, std::make_shared<EXPRESSION>(NEEDED_ERR::COMMA));
  else
    return std::make_pair(1, arg_expr);
}

EXPR_PTR Parser::parse_call_expr(EXPR_PTR callee) {
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

EXPR_PTR Parser::parse_member_expr(EXPR_PTR object) {
  token_curr = tokenize();
  return token_curr.index() == TOKV_IDENTI
             ? std::make_shared<EXPRESSION>(
                   EXPR_MEMBER{object, std::get<TOK_IDENTI>(token_curr)})
             : std::make_shared<EXPRESSION>(NEEDED_ERR::FIELD_NAME);
}

EXPR_PTR Parser::parse_array_access(EXPR_PTR object) {
  token_curr = tokenize();
  EXPR_PTR property_expr = parse_assign_expr();
  if (property_expr->index() == EXPRV_ERROR)
    return property_expr;
  return token_curr == TOKEN{U']'}
             ? std::make_shared<EXPRESSION>(
                   EXPR_ARRACCESS{object, property_expr})
             : std::make_shared<EXPRESSION>(NEEDED_ERR::CLOSING_BRACKET);
}

EXPR_PTR Parser::parse_assign_expr() {
  EXPR_PTR binary_expr = parse_binary_expr();
  if (binary_expr->index() == EXPRV_ERROR)
    return binary_expr;
  if (token_curr != TOKEN{U'='})
    return binary_expr;
  token_curr = tokenize();
  EXPR_PTR rhs_expr = parse_assign_expr();
  if (rhs_expr->index() == EXPRV_ERROR)
    return rhs_expr;
  return std::make_shared<EXPRESSION>(EXPR_ASSIGN{binary_expr, rhs_expr});
}

std::pair<bool, EXPR_PTR> Parser::parse_postfix_expr(EXPR_PTR expression) {
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

EXPR_PTR Parser::parse_postfix_expr() {
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

std::expected<void, PARSE_ERRMSG> Parser::expect_statement_end() {
  if (token_curr == TOKEN{U';'}) {
    token_curr = tokenize();
    return {};
  }
  bool insertion = token_curr.index() == TOKV_EOF ||
                   token_curr == TOKEN{U'}'} || newlineSeen();
  if (insertion)
    return {};
  return std::unexpected{NEEDED_ERR::SEMICOLON};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_statement() {
  switch (token_curr.index()) {
  case TOKV_ERROR:
    return std::unexpected{std::get<TOKV_ERROR>(token_curr)};
  case TOKV_IDENTI: {
    auto declaration = parse_variable_decl();
    switch (declaration.index()) {
    case 0:
      break;
    case 1:
      program.push_back(std::get<1>(declaration));
      return expect_statement_end();
    case 2:
      return std::unexpected{std::get<2>(declaration)};
    }
    [[fallthrough]];
  }
  default: {
    EXPR_PTR expression = parse_assign_expr();
    if (expression->index() == EXPRV_ERROR)
      return std::unexpected{std::get<EXPRV_ERROR>(*expression)};
    program.push_back(*expression);
    return expect_statement_end();
  }
  }
}

bool Parser::parse() {
  token_curr = tokenize();
  while (1) {
    if (token_curr.index() == TOKV_EOF)
      return 0;
    std::expected<void, PARSE_ERRMSG> status = parse_statement();
    if (not status)
      return 1;
  }
}
} // namespace Manadrain
