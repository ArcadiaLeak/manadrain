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

void AtomTokenizer::encode_string_atom(char32_t cp) {
  std::array<char, 4> buff{};
  UBool is_error{};
  std::uint32_t length{};
  U8_APPEND(buff.data(), length, buff.size(), cp, is_error);
  if (is_error)
    throw std::runtime_error{"codepoint conversion failed!"};
  my_string_atom.append(std::string_view{buff.data(), length});
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
      encode_string_atom(ch);
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
  bool is_legal = u_hasBinaryProperty(
      ch, my_string_atom.size() ? UCHAR_XID_CONTINUE : UCHAR_XID_START);
  if (is_legal)
    encode_string_atom(ch);
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
  if (my_string_atom.size() == 0)
    return std::nullopt;
  prev();
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
  case '"':
    return StringTokenizer::tokenize(leading);
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
  if (token_opt)
    return token_opt.value();
  return leading;
}

std::size_t aligned_N(std::size_t N) {
  return ((N + MEMORY_ALIGNMENT - 1) / MEMORY_ALIGNMENT) * MEMORY_ALIGNMENT;
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

std::size_t AtomTokenizer::find_atom() {
  std::optional<std::size_t> pos_opt{};
  for (std::size_t p_atom : atom_prealloc_pos) {
    std::size_t len =
        LE_decode(std::span{atom_arena}.subspan(p_atom).first<8>());
    std::string_view str_atom{atom_arena.data() + p_atom + 8, len};
    if (my_string_atom == str_atom) {
      pos_opt = p_atom;
      break;
    }
  }
  if (not pos_opt) {
    if (not atom_umap.contains(my_string_atom))
      atom_umap[my_string_atom] = alloc_atom();
    pos_opt = atom_umap[my_string_atom];
  }
  my_string_atom = {};
  return pos_opt.value();
}

std::size_t AtomTokenizer::alloc_atom() {
  std::size_t atom_addr = atom_arena.size();
  std::size_t aligned_size = aligned_N(my_string_atom.size());
  atom_arena.reserve(atom_arena.size() + aligned_size + 8);
  atom_arena.append_range(LE_encode(my_string_atom.size()));
  atom_arena.append_range(my_string_atom);
  atom_arena.append_range(
      std::ranges::repeat_view{0, aligned_size - my_string_atom.size()});
  return atom_addr;
}

static constexpr int RADIX_MAX = 36;

static int to_digit(char32_t ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  else if (ch >= 'A' && ch <= 'Z')
    return ch - 'A' + 10;
  else if (ch >= 'a' && ch <= 'z')
    return ch - 'a' + 10;
  else
    return RADIX_MAX;
}

static constexpr std::array<std::uint8_t, RADIX_MAX - 1> max_digits_table{{
    64, 80, 32, 55, 49, 45, 21, 40, 38, 37, 35, 34, 33, 32, 16, 31, 30, 30,
    29, 29, 28, 28, 27, 27, 27, 26, 26, 26, 26, 25, 12, 25, 25, 24, 24,
}};

static constexpr std::array<std::uint8_t, RADIX_MAX - 1> digits_per_limb_table{
    {32, 20, 16, 13, 12, 11, 10, 10, 9, 9, 8, 8, 8, 8, 8, 7, 7, 7,
     7,  7,  7,  7,  6,  6,  6,  6,  6, 6, 6, 6, 6, 6, 6, 6, 6}};

static constexpr std::array<std::uint32_t, RADIX_MAX - 1> radix_base_table{{
    0x00000000, 0xcfd41b91, 0x00000000, 0x48c27395, 0x81bf1000, 0x75db9c97,
    0x40000000, 0xcfd41b91, 0x3b9aca00, 0x8c8b6d2b, 0x19a10000, 0x309f1021,
    0x57f6c100, 0x98c29b81, 0x00000000, 0x18754571, 0x247dbc80, 0x3547667b,
    0x4c4b4000, 0x6b5a6e1d, 0x94ace180, 0xcaf18367, 0x0b640000, 0x0e8d4a51,
    0x1269ae40, 0x17179149, 0x1cb91000, 0x23744899, 0x2b73a840, 0x34e63b41,
    0x40000000, 0x4cfa3cc1, 0x5c13d840, 0x6d91b519, 0x81bf1000,
}};

std::optional<TOKEN> NumberTokenizer::tokenize(char32_t leading) {
  if (not std::isdigit(leading))
    return std::nullopt;
  int radix{10};
  bool has_legacy_octal{};
  std::optional<char32_t> separator{'_'};
  /* lifting the do-while to a separate method is hard because it
   * writes to the locals above */
  do {
    if (leading != '0')
      break;
    std::optional ahead{next()};
    if (not ahead.transform([](char32_t ch) { return std::isdigit(ch); })
                .value_or(0))
      break;
    has_legacy_octal = 1;
    separator = std::nullopt;
    int i{};
    while (1) {
      bool has_octal =
          ahead.transform([](char32_t ch) { return ch >= '0' && ch <= '7'; })
              .value_or(0);
      if (not has_octal)
        break;
      ahead = next();
      ++i;
    }
    backtrack(i);
    if (not ahead.transform([](char32_t ch) { return ch == '8' || ch == '9'; })
                .value_or(0))
      radix = 8;
    else {
      prev();
      break;
    }
    if (to_digit(peek().value()) < radix)
      break;
    return NUMBER_ERR::INVALID_LITERAL;
  } while (0);
  int max_digits = max_digits_table[radix - 2];
  int digits_per_limb = digits_per_limb_table[radix - 2];
  std::uint32_t radix_base = radix_base_table[radix - 2];
  MULTIPLE_PRECISION_BINARY mbp{.length = 1};
  while (1) {
    std::optional digit =
        next().transform([](char32_t ch) { return to_digit(ch); });
    if (digit.transform([radix](int i) { return i >= radix; })) {
      prev();
      break;
    }
  }
  return std::nullopt;
}

std::variant<std::monostate, STMT_VARDECL, PARSE_ERRMSG>
Parser::parse_variable_decl() {
  STMT_VARDECL declaration{};

  std::size_t p_atom = std::get<TOKV_IDENTI>(my_token).p_atom;
  switch (p_atom) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var:
    declaration.p_kind = p_atom;
    my_token = tokenize();
    break;
  default:
    return std::monostate{};
  }

  if (my_token.index() != TOKV_IDENTI)
    return NEEDED_ERR::VARIABLE_NAME;
  declaration.identifier = std::get<TOK_IDENTI>(my_token);
  my_token = tokenize();

  if (my_token != TOKEN{U'='})
    return declaration;
  my_token = tokenize();

  std::expected parse_ok = parse_assign_expr();
  if (not parse_ok)
    return parse_ok.error();
  declaration.initializer = std::move(my_expression);

  return declaration;
}

std::expected<void, PARSE_ERRMSG> Parser::parse_binary_expr() {
  std::expected<void, PARSE_ERRMSG> parse_ok{};
  parse_ok = parse_postfix_expr();
  if (not parse_ok)
    return parse_ok;
  if (my_token.index() != TOKV_OP)
    return {};
  switch (std::get<TOKV_OP>(my_token)) {
  case TOK_OPERATOR::EQ_SLOPPY:
  case TOK_OPERATOR::EQ_STRICT:
    break;
  default:
    return {};
  }
  EXPRESSION expr_left = std::move(my_expression);
  TOK_OPERATOR bin_op = std::get<TOKV_OP>(my_token);
  my_token = tokenize();
  parse_ok = parse_binary_expr();
  if (not parse_ok)
    return parse_ok;
  my_expression = std::make_unique<EXPR_BINARY>(
      std::move(expr_left), std::move(my_expression), bin_op);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_primary_expr() {
  switch (my_token.index()) {
  case TOKV_STRING:
    my_expression = std::get<TOKV_STRING>(my_token);
    return {};
  case TOKV_IDENTI:
    my_expression = std::get<TOKV_IDENTI>(my_token);
    return {};
  case TOKV_NUMBER:
    my_expression = std::get<TOKV_NUMBER>(my_token);
    return {};
  case TOKV_PUNCT:
    break;
  case TOKV_ERROR:
    return std::unexpected{std::get<TOKV_ERROR>(my_token)};
  default:
    return std::unexpected{UNEXPECTED_ERR::THIS_TOKEN};
  }
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '{':
    break;
  default:
    return std::unexpected{UNEXPECTED_ERR::THIS_TOKEN};
  }
  my_token = tokenize();
  if (my_token == TOKEN{U'}'}) {
    my_expression = std::make_unique<EXPR_OBJECT>();
    return {};
  }
  return std::unexpected{NEEDED_ERR::CLOSING_BRACE};
}

std::expected<bool, PARSE_ERRMSG> Parser::parse_arg_expr() {
  my_token = tokenize();
  if (my_token == TOKEN{U')'})
    return 0;
  std::expected parse_ok = parse_assign_expr();
  if (not parse_ok)
    return std::unexpected{parse_ok.error()};
  if (my_token == TOKEN{U')'})
    return 0;
  if (my_token != TOKEN{U','})
    return std::unexpected{NEEDED_ERR::COMMA};
  else
    return 1;
}

std::expected<void, PARSE_ERRMSG> Parser::parse_call_expr() {
  EXPRESSION callee_expr = std::move(my_expression);
  std::vector<EXPRESSION> arguments{};
  while (1) {
    std::expected parse_ok = parse_arg_expr();
    if (not parse_ok)
      return std::unexpected{parse_ok.error()};
    arguments.push_back(std::move(my_expression));
    if (not parse_ok.value())
      break;
  }
  my_expression =
      std::make_unique<EXPR_CALL>(std::move(callee_expr), std::move(arguments));
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_member_expr() {
  my_token = tokenize();
  if (my_token.index() == TOKV_IDENTI) {
    my_expression = std::make_unique<EXPR_MEMBER>(
        std::move(my_expression), std::get<TOK_IDENTI>(my_token));
    return {};
  }
  return std::unexpected{NEEDED_ERR::FIELD_NAME};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_access_expr() {
  my_token = tokenize();
  EXPRESSION object_expr = std::move(my_expression);
  std::expected parse_ok = parse_assign_expr();
  if (not parse_ok)
    return parse_ok;
  if (my_token != TOKEN{U']'})
    return std::unexpected{NEEDED_ERR::CLOSING_BRACKET};
  my_expression = std::make_unique<EXPR_ACCESS>(std::move(object_expr),
                                                std::move(my_expression));
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_assign_expr() {
  std::expected<void, PARSE_ERRMSG> parse_ok{};
  parse_ok = parse_binary_expr();
  if (not parse_ok)
    return parse_ok;
  if (my_token != TOKEN{U'='})
    return {};
  my_token = tokenize();
  EXPRESSION lhs_expr = std::move(my_expression);
  parse_ok = parse_assign_expr();
  if (not parse_ok)
    return parse_ok;
  my_expression = std::make_unique<EXPR_ASSIGN>(std::move(lhs_expr),
                                                std::move(my_expression));
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_postfix_expr() {
  std::expected<void, PARSE_ERRMSG> parse_ok{};
  parse_ok = parse_primary_expr();
  if (not parse_ok)
    return parse_ok;
  while (1) {
    bool go_on{};
    my_token = tokenize();
    if (my_token.index() != TOKV_PUNCT)
      break;
    switch (std::get<TOKV_PUNCT>(my_token)) {
    case '.':
      parse_ok = parse_member_expr();
      go_on = 1;
      break;
    case '(':
      parse_ok = parse_call_expr();
      go_on = 1;
      break;
    case '[':
      parse_ok = parse_access_expr();
      go_on = 1;
      break;
    }
    if (not parse_ok)
      return parse_ok;
    if (not go_on)
      break;
  }
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::expect_statement_end() {
  if (my_token == TOKEN{U';'}) {
    my_token = tokenize();
    return {};
  }
  bool insertion =
      my_token.index() == TOKV_EOF || my_token == TOKEN{U'}'} || newlineSeen();
  if (insertion)
    return {};
  return std::unexpected{NEEDED_ERR::SEMICOLON};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_statement() {
  switch (my_token.index()) {
  case TOKV_ERROR:
    return std::unexpected{std::get<TOKV_ERROR>(my_token)};
  case TOKV_IDENTI: {
    auto declaration = parse_variable_decl();
    switch (declaration.index()) {
    case 0:
      break;
    case 1:
      program.push_back(std::move(std::get<1>(declaration)));
      return expect_statement_end();
    case 2:
      return std::unexpected{std::get<2>(declaration)};
    }
    [[fallthrough]];
  }
  default: {
    std::expected parse_ok = parse_assign_expr();
    if (not parse_ok)
      return parse_ok;
    program.push_back(std::move(my_expression));
    return expect_statement_end();
  }
  }
}

bool Parser::parse() {
  my_token = tokenize();
  while (1) {
    if (my_token.index() == TOKV_EOF)
      return 0;
    std::expected<void, PARSE_ERRMSG> status = parse_statement();
    if (not status)
      return 1;
  }
}
} // namespace Manadrain
