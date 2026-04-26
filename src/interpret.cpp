#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

#include <unictype.h>
#include <unistr.h>

#include "interpret.hpp"

namespace Interpret {
static bool lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

void append_codepoint(std::string &str, char32_t cp) {
  std::array<std::uint8_t, 6> ubuf;
  int len = u8_uctomb(ubuf.data(), cp, ubuf.size());
  if (len > 0) {
    str.append(reinterpret_cast<const char *>(ubuf.data()),
               static_cast<std::size_t>(len));
    return;
  }
  throw std::runtime_error{"couldn't encode a codepoint!"};
}

char32_t Scanner::unchecked_next() {
  ucs4_t ch;
  int len =
      u8_mbtoucr(&ch, buffer.data() + buffer_idx, buffer.size() - buffer_idx);
  if (len > 0) {
    buffer_idx += len;
    breadcrumb.push(len);
    return ch;
  }
  throw std::runtime_error{"couldn't decode a codepoint!"};
}

std::optional<char32_t> Scanner::next() {
  if (reached_eof())
    return std::nullopt;
  return unchecked_next();
}

void Scanner::prev() {
  if (breadcrumb.empty())
    throw std::runtime_error{"rewind past boundary!"};
  buffer_idx -= breadcrumb.top();
  breadcrumb.pop();
}

std::optional<char32_t> Scanner::peek() {
  std::optional<char32_t> ahead = next();
  if (ahead)
    prev();
  return ahead;
}

void Scanner::backtrack(std::size_t N) {
  for (int i = 0; i < N; ++i)
    prev();
}

void Scanner::chewLF() {
  std::optional ahead{next()};
  if (ahead == '\n')
    return;
  if (ahead)
    prev();
}

std::optional<char32_t> TokString::decode_esc8() {
  if (reached_eof())
    return std::nullopt;
  std::optional ahead{next()};
  std::optional ret_opt = ahead.and_then([](char32_t ch) {
    return ch < '8' ? std::make_optional(ch - '0') : std::nullopt;
  });
  if (ahead && !ret_opt)
    prev();
  return ret_opt;
}

std::optional<int> decode_hex(char32_t digit) {
  if (digit >= '0' && digit <= '9')
    return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    return digit - 'a' + 10;
  return std::nullopt;
}

std::optional<char32_t> TokString::decode_xseq() {
  std::optional<int> hex0 = next().and_then(decode_hex);
  if (not hex0)
    return std::nullopt;
  std::optional<int> hex1 = next().and_then(decode_hex);
  if (not hex1)
    return std::nullopt;
  return (*hex0 << 4) | *hex1;
}

std::optional<char32_t> TokString::decode_uni() {
  std::optional ahead_opt{next()};
  char32_t num{};
  if (ahead_opt == '{') {
    ahead_opt = std::nullopt;
    for (int i = 0; i < 7; ++i) {
      if (not ahead_opt)
        ahead_opt = next();
      if (not ahead_opt)
        return std::nullopt;
      if (ahead_opt == '}')
        return num;
      if (i == 6)
        return std::nullopt;
      std::optional hex = ahead_opt.and_then(decode_hex);
      if (not hex)
        return std::nullopt;
      num = (num << 4) | *hex;
      if (num > 0x10ffff)
        return std::nullopt;
      ahead_opt = std::nullopt;
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (not ahead_opt)
      ahead_opt = next();
    if (not ahead_opt)
      return std::nullopt;
    std::optional hex = ahead_opt.and_then(decode_hex);
    if (not hex)
      return std::nullopt;
    num = (num << 4) | *hex;
    ahead_opt = std::nullopt;
  }
  return num;
}

std::variant<std::monostate, char32_t, PARSE_ERRMSG>
TokString::decode_escape(char32_t ch) {
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
    std::optional hex = decode_xseq();
    if (not hex)
      return INVALID_ERR::MALFORMED_ESCAPE;
    return *hex;
  }
  case 'u': {
    std::optional uni = decode_uni();
    if (not uni)
      return INVALID_ERR::MALFORMED_ESCAPE;
    return *uni;
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
    std::optional<char32_t> digit{};
    char32_t base = ch - '0';
    digit = decode_esc8();
    if (not digit)
      return base;
    base = (base << 3) | *digit;
    if (base >= 32)
      return base;
    digit = decode_esc8();
    if (not digit)
      return base;
    base = (base << 3) | *digit;
    return base;
  }
  case '8':
  case '9':
    return INVALID_ERR::MALFORMED_ESCAPE;
  default:
    return std::monostate{};
  }
}

std::variant<std::monostate, char32_t, PARSE_ERRMSG>
TokString::decode_special(char32_t separator, char32_t ch) {
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
    return decode_escape(unchecked_next());
  default:
    return ch;
  }
}

std::expected<TOKEN, PARSE_ERRMSG> TokString::tokenize(char32_t separator) {
  while (1) {
    std::optional ch_opt{next()};
    if (not ch_opt)
      return std::unexpected{UNEXPECTED_ERR::STRING_END};
    if (ch_opt == separator)
      return TOK_STRING{separator, atomFind()};
    std::variant ch_alter = decode_special(separator, *ch_opt);
    switch (ch_alter.index()) {
    case 0:
      break;
    case 1:
      append_codepoint(my_atom, std::get<1>(ch_alter));
      break;
    default:
      return std::unexpected{std::get<2>(ch_alter)};
    }
  }
}

std::optional<char32_t> TokIdentif::decode_uni() {
  std::optional ahead_opt{next()};
  char32_t num{};
  if (ahead_opt == '{') {
    ahead_opt = std::nullopt;
    for (int i = 0; i < 7; ++i) {
      if (not ahead_opt)
        ahead_opt = next();
      if (not ahead_opt)
        return std::nullopt;
      if (ahead_opt == '}')
        return num;
      if (i == 6)
        return std::nullopt;
      std::optional hex = ahead_opt.and_then(decode_hex);
      if (not hex)
        return std::nullopt;
      num = (num << 4) | *hex;
      if (num > 0x10ffff)
        return std::nullopt;
      ahead_opt = std::nullopt;
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (not ahead_opt)
      ahead_opt = next();
    if (not ahead_opt)
      return std::nullopt;
    std::optional hex = ahead_opt.and_then(decode_hex);
    if (not hex)
      return std::nullopt;
    num = (num << 4) | *hex;
    ahead_opt = std::nullopt;
  }
  return num;
}

bool TokIdentif::encode_uchar(char32_t ch) {
  bool is_legal = my_atom.size() ? uc_is_property_xid_continue(ch)
                                 : uc_is_property_xid_start(ch);
  if (is_legal)
    append_codepoint(my_atom, ch);
  return is_legal;
}

std::expected<int, PARSE_ERRMSG> TokIdentif::decode_escape(char32_t leading) {
  bool has_escape{};
  if (leading == '\\') {
    if (not next().transform([](char32_t ch) { return ch == 'u'; }).value_or(0))
      return std::unexpected{INVALID_ERR::MALFORMED_ESCAPE};
    std::optional<int> ch_uni = decode_uni();
    switch (ch_uni.has_value()) {
    case 1:
      leading = *ch_uni;
      has_escape = 1;
      break;
    default:
      return std::unexpected{INVALID_ERR::MALFORMED_ESCAPE};
    }
  }
  bool success = encode_uchar(leading);
  if (has_escape) {
    if (success)
      return 2;
    return std::unexpected{INVALID_ERR::MALFORMED_ESCAPE};
  }
  return success;
}

std::expected<TOKEN, PARSE_ERRMSG> TokIdentif::tokenize(char32_t leading) {
  bool has_escape = 0;
  while (1) {
    std::expected esc_exp = decode_escape(leading);
    if (not esc_exp)
      return std::unexpected{esc_exp.error()};
    switch (*esc_exp) {
    case 2:
      has_escape = 1;
      [[fallthrough]];
    case 1:
      if (reached_eof())
        break;
      leading = unchecked_next();
      continue;
    }
    break;
  }
  if (my_atom.size() == 0)
    throw std::runtime_error{"tokenizing an identifier failed!"};
  prev();
  return TOK_IDENTI{has_escape, atomFind()};
}

std::expected<TOKEN, PARSE_ERRMSG> Tokenizer::tokenize() {
  newline_seen = 0;
  while (1) {
    if (reached_eof())
      return std::monostate{};
    char32_t leading = unchecked_next();
    switch (leading) {
    case '\'':
    case '"':
      return TokString::tokenize(leading);
    case '\r':
      chewLF();
      [[fallthrough]];
    case '\n':
    case 0x2028:
    case 0x2029:
      newline_seen = 1;
      break;
    case '/': {
      std::optional ahead_opt = next();
      if (ahead_opt == '*') {
        while (1) {
          if (reached_eof())
            return std::unexpected{UNEXPECTED_ERR::COMMENT_END};
          leading = unchecked_next();
          ahead_opt = next();
          if (leading == '*' && ahead_opt == '/')
            break;
          if (ahead_opt)
            prev();
          if (lineterm(leading))
            newline_seen = 1;
        }
        break;
      } else if (ahead_opt == '/') {
        while (1) {
          if (reached_eof())
            break;
          leading = unchecked_next();
          if (lineterm(leading)) {
            prev();
            break;
          }
        }
        break;
      }
      if (ahead_opt == '=')
        return TOK_OPERATOR::DIV_ASSIGN;
      else {
        if (ahead_opt)
          prev();
        return U'/';
      }
    }
    case '\\': {
      std::optional ahead_opt = next();
      if (ahead_opt == 'u') {
        std::optional<int> ch_uni = decode_uni();
        auto is_id_start = [](char32_t ch) {
          return uc_is_property_xid_start(ch);
        };
        if (ch_uni.transform(is_id_start).value_or(0))
          return TokIdentif::tokenize(*ch_uni);
        else
          return std::unexpected{INVALID_ERR::MALFORMED_ESCAPE};
      } else {
        if (ahead_opt)
          prev();
        return U'\\';
      }
    }
    case '=': {
      int i;
      for (i = 0; i < 2; i++) {
        if (reached_eof())
          break;
        leading = unchecked_next();
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
      if (std::isdigit(leading))
        return TokNumber::tokenize(leading);
      if (uc_is_property_white_space(leading))
        break;
      if (uc_is_property_xid_start(leading))
        return TokIdentif::tokenize(leading);
      return leading;
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

std::size_t TokAtom::atomFind() {
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
  return *pos_opt;
}

std::size_t TokAtom::atomAlloc() {
  std::size_t atom_addr = atom_arena.size();
  std::size_t aligned_size = aligned_N(my_atom.size());
  atom_arena.reserve(atom_arena.size() + aligned_size + 8);
  atom_arena.append_range(LE_encode(my_atom.size()));
  atom_arena.append_range(my_atom);
  atom_arena.append_range(
      std::ranges::repeat_view{0, aligned_size - my_atom.size()});
  return atom_addr;
}

void TokNumber::peek_behind_octal(std::optional<char32_t> &trail_opt) {
  int cnt{};
  while (1) {
    if (trail_opt.transform([](char32_t ch) { return ch < '0' || ch > '7'; })
            .value_or(1))
      break;
    trail_opt = next();
    if (trail_opt)
      ++cnt;
  }
  backtrack(cnt);
}

std::optional<BASE_IND> TokNumber::decode_base_ind() {
  std::optional<BASE_IND> prefix{};
  std::optional ahead_opt{next()};
  if (ahead_opt.transform([](char32_t ch) { return std::isdigit(ch); })
          .value_or(0)) {
    std::optional trail_opt{ahead_opt};
    peek_behind_octal(trail_opt);
    prefix = trail_opt == '8' || trail_opt == '9'
                 ? std::nullopt
                 : std::make_optional(BASE_IND::ZERO_LEAD_8);
  }
  if (prefix.transform([](BASE_IND p) { return p == BASE_IND::ZERO_LEAD_8; })
          .value_or(1)) {
    if (not prefix)
      prev();
    if (ahead_opt)
      prev();
  }
  return prefix;
}

int radix_from_ind(std::optional<BASE_IND> ind_opt) {
  do {
    if (not ind_opt)
      break;
    switch (*ind_opt) {
    case BASE_IND::BINARY:
      return 2;
    case BASE_IND::ZERO_LEAD_8:
    case BASE_IND::OCTAL:
      return 8;
    case BASE_IND::HEX:
      return 16;
    }
  } while (0);
  return 10;
}

std::string TokNumber::scan_numseq(std::optional<BASE_IND> base_opt,
                                   std::optional<char32_t> ahead) {
  int radix{radix_from_ind(base_opt)};
  std::string accum{};
  while (1) {
    if (not ahead)
      ahead = next();
    if (not ahead.and_then(decode_hex)
                .transform([radix](int digit) { return radix > digit; })
                .value_or(0))
      break;
    append_codepoint(accum, *ahead);
    ahead = std::nullopt;
  }
  if (ahead)
    prev();
  return accum;
}

std::string TokNumber::FRACTIONAL::collapse() {
  std::string ret{};
  ret.append(whole.repr_s);
  if (frac_s.empty())
    return ret;
  ret.push_back('.');
  ret.append(frac_s);
  return ret;
}

std::expected<TOKEN, PARSE_ERRMSG> TokNumber::tokenize(char32_t leading) {
  if (reached_eof())
    return leading - '0';
  std::optional<BASE_IND> base_opt{};
  if (leading == '0') {
    do {
      base_opt = decode_base_ind();
      /* there must be a digit after the indicator */
      std::optional ahead{next()};
      if (not ahead.and_then(decode_hex))
        return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
      leading = *ahead;
    } while (0);
  }
  FLOAT_REPR repr_node{};
  do {
    WHOLE whole_n{scan_numseq(base_opt, leading)};
    repr_node = whole_n;
    if (base_opt)
      break;
    if (next() != '.') {
      prev();
      break;
    }
    FRACTIONAL frac_n{std::move(whole_n),
                      scan_numseq(std::nullopt, std::nullopt)};
    repr_node = frac_n;
  } while (0);
  do {
    std::optional ahead{peek()};
    if (not ahead)
      break;
    if (not uc_is_property_xid_continue(*ahead))
      break;
    return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
  } while (0);
  int radix{radix_from_ind(base_opt)};
  switch (radix) {
  case 10: {
    double result{};
    std::string repr_s = repr_node.visit([](auto n) { return n.collapse(); });
    auto status =
        std::from_chars(repr_s.data(), repr_s.data() + repr_s.size(), result);
    if (status.ec == std::errc::result_out_of_range)
      break;
    else if (status.ec != std::errc{})
      return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
    return result;
  }
  default: {
    std::uint64_t result{};
    std::string repr_s = std::move(std::get<WHOLE>(repr_node).repr_s);
    auto status = std::from_chars(repr_s.data(), repr_s.data() + repr_s.size(),
                                  result, radix);
    if (status.ec == std::errc::result_out_of_range)
      break;
    else if (status.ec != std::errc{})
      return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
    static constexpr std::uint64_t max_safe_int =
        1LL << std::numeric_limits<double>::digits;
    if (result >= max_safe_int)
      break;
    return static_cast<double>(result);
  }
  }
  return std::numeric_limits<double>::infinity();
}

std::expected<void, PARSE_ERRMSG> Parser::tokenize() {
  std::expected tokenize_ok = Tokenizer::tokenize();
  if (tokenize_ok) {
    my_token = *tokenize_ok;
    return {};
  }
  return std::unexpected{tokenize_ok.error()};
}

#define TRY_EXP(an_expect)                                                     \
  do {                                                                         \
    std::expected ok{an_expect};                                               \
    if (ok)                                                                    \
      break;                                                                   \
    return std::unexpected{ok.error()};                                        \
  } while (0);

std::expected<void, PARSE_ERRMSG> Parser::parse_variable_decl() {
  STMT_VARDECL declaration{};

  std::size_t p_atom{std::get<TOKV_IDENTI>(my_token).p_atom};
  bool valid_beginning{p_atom == S_ATOM_const || p_atom == S_ATOM_let ||
                       p_atom == S_ATOM_var};
  if (not valid_beginning)
    throw std::runtime_error("statement isn't a variable declaration!");
  declaration.p_kind = p_atom;
  TRY_EXP(tokenize())

  if (my_token.index() != TOKV_IDENTI)
    return std::unexpected{NEEDED_ERR::VARIABLE_NAME};
  declaration.identifier = std::get<TOK_IDENTI>(my_token);
  TRY_EXP(tokenize())

  if (my_token != TOKEN{U'='})
    goto wrap_up;
  TRY_EXP(tokenize())

  TRY_EXP(parse_assign_expr())
  declaration.initializer = std::move(my_expression);

wrap_up:
  statements.top().push_back(std::move(declaration));
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_binary_expr() {
  TRY_EXP(parse_postfix_expr())
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
  TRY_EXP(tokenize())
  TRY_EXP(parse_binary_expr())
  my_expression = std::make_unique<EXPR_BINARY>(
      std::move(expr_left), std::move(my_expression), bin_op);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_object_literal() {
  TRY_EXP(tokenize())
  std::vector<EXPR_OBJECT::PROP> prop_vec{};
  while (my_token != TOKEN{U'}'}) {
    EXPR_OBJECT::PROP property{};
    TRY_EXP(parse_property_name())
    property.prop_key = std::move(my_expression);
    TRY_EXP(expect_punct(':'))
    TRY_EXP(tokenize())
    TRY_EXP(parse_assign_expr())
    property.prop_val = std::move(my_expression);
    prop_vec.push_back(std::move(property));
    if (my_token != TOKEN{U','})
      break;
    TRY_EXP(tokenize())
  }
  TRY_EXP(expect_punct('}'))
  TRY_EXP(tokenize())
  my_expression = std::make_unique<EXPR_OBJECT>(std::move(prop_vec));
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
  default:
    return std::unexpected{UNEXPECTED_ERR::THIS_TOKEN};
  }
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '{':
    TRY_EXP(parse_object_literal());
    return {};
  default:
    return std::unexpected{UNEXPECTED_ERR::THIS_TOKEN};
  }
}

std::expected<void, PARSE_ERRMSG> Parser::expect_punct(char32_t punct) {
  if (my_token == TOKEN{punct})
    return {};
  return std::unexpected{PUNCT_ERR{punct}};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_property_name() {
  switch (my_token.index()) {
  case TOKV_PUNCT:
    break;
  default:
    return std::unexpected{INVALID_ERR::PROPERTY_NAME};
  }
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '[':
    TRY_EXP(tokenize())
    TRY_EXP(parse_assign_expr())
    TRY_EXP(expect_punct(']'))
    TRY_EXP(tokenize())
    return {};
  default:
    return std::unexpected{INVALID_ERR::PROPERTY_NAME};
  }
  return std::unexpected{PUNCT_ERR{'}'}};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_call_expr() {
  EXPRESSION callee_expr = std::move(my_expression);
  std::vector<EXPRESSION> arguments{};
  while (1) {
    TRY_EXP(tokenize())
    if (my_token == TOKEN{U')'})
      break;
    std::expected parse_ok = parse_assign_expr();
    if (not parse_ok)
      return std::unexpected{parse_ok.error()};
    if (my_token == TOKEN{U')'})
      break;
    TRY_EXP(expect_punct(','))
    arguments.push_back(std::move(my_expression));
  }
  my_expression =
      std::make_unique<EXPR_CALL>(std::move(callee_expr), std::move(arguments));
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_member_expr() {
  TRY_EXP(tokenize())
  if (my_token.index() == TOKV_IDENTI) {
    my_expression = std::make_unique<EXPR_MEMBER>(
        std::move(my_expression), std::get<TOK_IDENTI>(my_token));
    return {};
  }
  return std::unexpected{NEEDED_ERR::FIELD_NAME};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_access_expr() {
  TRY_EXP(tokenize())
  EXPRESSION object_expr = std::move(my_expression);
  std::expected parse_ok = parse_assign_expr();
  if (not parse_ok)
    return parse_ok;
  TRY_EXP(expect_punct(']'))
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
  TRY_EXP(tokenize())
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
    TRY_EXP(tokenize())
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
    TRY_EXP(tokenize())
    return {};
  }
  bool insertion =
      my_token.index() == TOKV_EOF || my_token == TOKEN{U'}'} || newlineSeen();
  if (insertion)
    return {};
  return std::unexpected{PUNCT_ERR{';'}};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_function_decl() {
  STMT_FUNCDECL funcdecl{};
  TRY_EXP(tokenize())
  if (my_token.index() != TOKV_IDENTI)
    return std::unexpected{NEEDED_ERR::FUNCTION_NAME};
  funcdecl.identifier = std::get<TOKV_IDENTI>(my_token);
  TRY_EXP(tokenize())
  TRY_EXP(expect_punct('('))
  TRY_EXP(tokenize())
  TRY_EXP(expect_punct(')'))
  TRY_EXP(tokenize())
  TRY_EXP(expect_punct('{'))
  TRY_EXP(tokenize())
  statements.emplace();
  while (my_token != TOKEN{U'}'}) {
    TRY_EXP(parse_statement())
  }
  funcdecl.body = std::move(statements.top());
  statements.pop();
  statements.top().push_back(std::move(funcdecl));
  TRY_EXP(tokenize())
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_statement() {
  switch (my_token.index()) {
  case TOKV_IDENTI:
    switch (std::get<TOKV_IDENTI>(my_token).p_atom) {
    case S_ATOM_const:
    case S_ATOM_let:
    case S_ATOM_var:
      TRY_EXP(parse_variable_decl())
      return expect_statement_end();
    case S_ATOM_function:
      return parse_function_decl();
    }
    [[fallthrough]];
  default:
    TRY_EXP(parse_assign_expr())
    statements.top().push_back(std::move(my_expression));
    return expect_statement_end();
  }
}

bool Parser::parse() {
  std::expected ok{tokenize()};
  if (not ok)
    return 1;
  statements.emplace();
  while (1) {
    if (my_token.index() == TOKV_EOF)
      return 0;
    std::expected<void, PARSE_ERRMSG> status = parse_statement();
    if (not status)
      return 1;
  }
}
} // namespace Interpret
