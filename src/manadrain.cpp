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

class Ch4Encoder {
public:
  std::string_view operator()(char32_t cp) {
    UBool has_error{};
    U8_APPEND(buff.data(), length, buff.size(), cp, has_error);
    if (not has_error)
      return {buff.data(), length};
    throw std::runtime_error{"codepoint conversion failed!"};
  }

private:
  std::array<char, 4> buff;
  std::uint32_t length;
};

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

void Scanner::chewLF() {
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
        return TOK_STRING{separator, atomFind()};
      Ch4Encoder encoder{};
      my_atom.append(encoder(ch));
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
  bool is_legal = u_hasBinaryProperty(ch, my_atom.size() ? UCHAR_XID_CONTINUE
                                                         : UCHAR_XID_START);
  if (is_legal) {
    Ch4Encoder encoder{};
    my_atom.append(encoder(ch));
  }
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
  if (my_atom.size() == 0)
    return std::nullopt;
  prev();
  return TOK_IDENTI{has_escape, atomFind()};
}

TOKEN Tokenizer::tokenize() {
  newline_seen = 0;
  while (1) {
    if (reached_eof())
      return std::monostate{};
    char32_t leading = next().value();
    switch (leading) {
    case '\'':
    case '"':
      return StringTokenizer::tokenize(leading);
    case '\r':
      chewLF();
      [[fallthrough]];
    case '\n':
    case 0x2028:
    case 0x2029:
      newline_seen = 1;
      break;
    case '/':
      if (reached_eof())
        return U'/';
      leading = next().value();
      if (leading == '*') {
        while (1) {
          if (reached_eof())
            return UNEXPECTED_ERR::COMMENT_END;
          leading = next().value();
          std::optional ahead_opt = next();
          if (leading == '*' && ahead_opt == '/')
            break;
          if (ahead_opt)
            prev();
          if (lineterm(leading))
            newline_seen = 1;
        }
        break;
      } else if (leading == '/') {
        while (1) {
          if (reached_eof())
            break;
          leading = next().value();
          if (lineterm(leading)) {
            prev();
            break;
          }
        }
        break;
      }
      if (leading == '=')
        return TOK_OPERATOR::DIV_ASSIGN;
      else {
        prev();
        return U'/';
      }
    case '=': {
      int i;
      for (i = 0; i < 2; i++) {
        if (reached_eof())
          break;
        leading = next().value();
        if (leading != '=') {
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
      if (u_isWhitespace(leading))
        break;
      return UNEXPECTED_ERR::THIS_CHAR;
    }
  }
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

std::size_t AtomTokenizer::atomFind() {
  std::optional<std::size_t> pos_opt{};
  for (std::size_t p_atom : atom_prealloc_pos) {
    std::size_t len =
        LE_decode(std::span{atom_arena}.subspan(p_atom).first<8>());
    std::string_view str_atom{atom_arena.data() + p_atom + 8, len};
    if (my_atom == str_atom) {
      pos_opt = p_atom;
      break;
    }
  }
  if (not pos_opt) {
    if (not atom_umap.contains(my_atom))
      atom_umap[my_atom] = atomAlloc();
    pos_opt = atom_umap[my_atom];
  }
  my_atom = {};
  return pos_opt.value();
}

std::size_t AtomTokenizer::atomAlloc() {
  std::size_t atom_addr = atom_arena.size();
  std::size_t aligned_size = aligned_N(my_atom.size());
  atom_arena.reserve(atom_arena.size() + aligned_size + 8);
  atom_arena.append_range(LE_encode(my_atom.size()));
  atom_arena.append_range(my_atom);
  atom_arena.append_range(
      std::ranges::repeat_view{0, aligned_size - my_atom.size()});
  return atom_addr;
}

std::optional<TOKEN> NumberTokenizer::tokenize(char32_t leading) {
  if (not std::isdigit(leading))
    return std::nullopt;
  int radix{10};
  std::optional<char32_t> separator{'_'};
  /* lifting the do-while to a separate method is hard because it
   * writes to the locals above */
  do {
    if (reached_eof() || leading != '0')
      break;
    if (not peek()
                .transform([](char32_t ch) { return std::isdigit(ch); })
                .value())
      break;
    separator = std::nullopt;
    std::optional<char32_t> ahead{};
    int i{};
    while (1) {
      if (reached_eof())
        break;
      auto is_octal = [](char32_t ch) { return ch >= '0' && ch <= '7'; };
      ahead = next();
      if (is_octal(ahead.value()))
        ++i;
      else {
        prev();
        break;
      }
    }
    backtrack(i);
    auto beyond_octal = [](char32_t ch) { return ch == '8' || ch == '9'; };
    if (not ahead.transform(beyond_octal).value_or(0))
      radix = 8;
    else {
      prev();
      break;
    }
    if (peek().and_then(hex_conv).value_or(16) < radix)
      break;
    return NUMBER_ERR::INVALID_LITERAL;
  } while (0);
  std::string charconv_in{};
  while (1) {
    if (reached_eof())
      break;
    char32_t ch = next().value();
    if (not std::isdigit(ch)) {
      prev();
      break;
    }
    Ch4Encoder encoder{};
    charconv_in.append(encoder(ch));
  }
  do {
    if (reached_eof())
      break;
    char32_t ch = peek().value();
    bool id_continue_ahead = u_hasBinaryProperty(ch, UCHAR_XID_CONTINUE);
    if (not id_continue_ahead)
      break;
    return NUMBER_ERR::INVALID_LITERAL;
  } while (0);
  std::uint64_t num{};
  std::from_chars_result status = std::from_chars(
      charconv_in.data(), charconv_in.data() + charconv_in.size(), num, radix);
  bool has_overflow = status.ec == std::errc::result_out_of_range ||
                      num >= 1LL << std::numeric_limits<double>::digits;
  return has_overflow ? std::numeric_limits<double>::infinity()
                      : static_cast<double>(num);
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
