#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"
#include "persistent_atoms.hpp"

namespace Manadrain {
namespace Language {
static bool lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

std::generator<char> traverse_ucs4(ucs4_t cp) {
  std::array<std::uint8_t, 6> ubuf{};
  int len = u8_uctomb(ubuf.data(), cp, ubuf.size());
  if (len < 0)
    throw std::runtime_error{"invalid 4-byte UTF!"};
  for (int i = 0; i < len; ++i)
    co_yield ubuf[i];
}

bool Scanner::reached_end() { return position >= buffer.size(); }

void Scanner::prev() {
  if (backtrace.empty())
    throw std::runtime_error{"rewind past boundary!"};
  position -= backtrace.top();
  backtrace.pop();
}

char32_t Scanner::unchecked_next() {
  ucs4_t ch;
  int len = u8_mbtoucr(&ch, buffer.data() + position, buffer.size() - position);
  if (len < 0)
    throw std::runtime_error{"invalid 1-byte UTF!"};
  position += len;
  backtrace.push(len);
  return ch;
}

std::optional<char32_t> Scanner::next() {
  if (reached_end())
    return std::nullopt;
  return unchecked_next();
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

void Scanner::skip_lf() {
  std::optional ahead{next()};
  if (ahead == '\n')
    return;
  if (ahead)
    prev();
}

void Scanner::skip_shebang() {
  std::optional ahead{next()};
  if (ahead != '#') {
    backtrack(ahead.has_value());
    return;
  }
  ahead = next();
  if (ahead != '!') {
    backtrack(ahead.has_value() + 1);
    return;
  }
  while (1) {
    ahead = next();
    if (not ahead.transform(lineterm).value_or(1))
      continue;
    backtrack(ahead.has_value());
    return;
  }
}

std::optional<char32_t> TokAtom::decode_string_esc8() {
  if (reached_end())
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

std::optional<char32_t> TokAtom::decode_string_xseq() {
  std::optional<int> hex0 = next().and_then(decode_hex);
  if (not hex0)
    return std::nullopt;
  std::optional<int> hex1 = next().and_then(decode_hex);
  if (not hex1)
    return std::nullopt;
  return (*hex0 << 4) | *hex1;
}

std::optional<char32_t> TokAtom::decode_string_uni() {
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

std::generator<std::expected<char32_t, PARSE_ERRMSG>>
TokAtom::traverse_string(char32_t separator) {
  std::optional<PARSE_ERRMSG> err_opt{};
  bool backslash_seen{};
  while (not err_opt) {
    std::optional ch_opt{next()};
    if (not ch_opt) {
      err_opt = UNEXPECTED_ERR::STRING_END;
      break;
    }
    if (backslash_seen) {
      backslash_seen = 0;
      switch (*ch_opt) {
      case '\'':
      case '\\':
      case '"':
      case '`':
        co_yield *ch_opt;
        break;
      case '\r':
        skip_lf();
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        /* ignore escaped newline sequence */
        break;
      case 'b':
        co_yield '\b';
        break;
      case 'f':
        co_yield '\f';
        break;
      case 'n':
        co_yield '\n';
        break;
      case 'r':
        co_yield '\r';
        break;
      case 't':
        co_yield '\t';
        break;
      case 'v':
        co_yield '\v';
        break;
      case 'x': {
        std::optional hex = decode_string_xseq();
        if (hex)
          co_yield *hex;
        else
          err_opt = INVALID_ERR::BACKSLASH_ESCAPE;
        break;
      }
      case 'u': {
        std::optional uni = decode_string_uni();
        if (uni)
          co_yield *uni;
        else
          err_opt = INVALID_ERR::BACKSLASH_ESCAPE;
        break;
      }
      case '0':
        if (not peek()
                    .transform(
                        [](char32_t ahead) { return std::isdigit(ahead); })
                    .value_or(0)) {
          co_yield '\0';
          break;
        }
        [[fallthrough]];
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7': {
        char32_t base = *ch_opt - '0';
        do {
          std::optional<char32_t> digit{};
          digit = decode_string_esc8();
          if (not digit)
            break;
          base = (base << 3) | *digit;
          if (base >= 32)
            break;
          digit = decode_string_esc8();
          if (not digit)
            break;
          base = (base << 3) | *digit;
        } while (0);
        co_yield base;
        break;
      }
      case '8':
      case '9':
        err_opt = INVALID_ERR::BACKSLASH_ESCAPE;
        break;
      default:
        prev();
        break;
      }
    } else {
      if (ch_opt == separator)
        co_return;
      switch (*ch_opt) {
      case '\r':
        skip_lf();
        [[fallthrough]];
      case '\n':
        if (separator == '`')
          co_yield '\n';
        else
          err_opt = UNEXPECTED_ERR::STRING_END;
        break;
      case '\\':
        backslash_seen = 1;
        break;
      default:
        co_yield *ch_opt;
        break;
      }
    }
  }
  co_yield std::unexpected{*err_opt};
}

std::expected<TOKEN, PARSE_ERRMSG>
TokAtom::tokenize_string(char32_t separator) {
  std::string needle{};
  for (std::expected schar : traverse_string(separator)) {
    if (not schar)
      return std::unexpected{schar.error()};
    needle.append_range(traverse_ucs4(*schar));
  }
  return TOK_STRING{separator, atom_find(std::move(needle))};
}

std::optional<char32_t> TokAtom::decode_identif_uni() {
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

std::generator<std::optional<char32_t>>
TokAtom::traverse_identif(bool &has_escape) {
  while (1) {
    std::optional ch_opt{next()};
    if (not ch_opt)
      co_return;
    if (uc_is_property_xid_continue(*ch_opt)) {
      co_yield *ch_opt;
      continue;
    }
    if (ch_opt != '\\') {
      prev();
      co_return;
    }
    if (next() != 'u') {
      co_yield std::nullopt;
      break;
    }
    std::optional ch_uni{decode_identif_uni()};
    if (not ch_uni) {
      co_yield std::nullopt;
      break;
    }
    has_escape = 1;
    if (uc_is_property_xid_continue(*ch_uni)) {
      co_yield *ch_uni;
      continue;
    } else {
      co_yield std::nullopt;
      break;
    }
  }
}

std::expected<TOKEN, PARSE_ERRMSG> TokAtom::tokenize_identif(char32_t leading) {
  std::string needle{};
  needle.append_range(traverse_ucs4(leading));
  bool has_escape = 0;
  for (std::optional ichar : traverse_identif(has_escape)) {
    if (not ichar)
      return std::unexpected{INVALID_ERR::BACKSLASH_ESCAPE};
    needle.append_range(traverse_ucs4(*ichar));
  }
  return TOK_IDENTI{has_escape, atom_find(std::move(needle))};
}

std::expected<TOKEN, PARSE_ERRMSG> Tokenizer::tokenize() {
  newline_seen = 0;
  while (1) {
    if (reached_end())
      return std::monostate{};
    char32_t leading = unchecked_next();
    switch (leading) {
    case '\'':
    case '"':
    case '`':
      return tokenize_string(leading);
    case '\r':
      skip_lf();
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
          if (reached_end())
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
          if (reached_end())
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
        return TOK_OPERATOR::DIVIDE_ASSIGN;
      else {
        if (ahead_opt)
          prev();
        return U'/';
      }
    }
    case '\\': {
      std::optional ahead_opt = next();
      if (ahead_opt == 'u') {
        std::optional<int> ch_uni = decode_identif_uni();
        auto is_id_start = [](char32_t ch) {
          return uc_is_property_xid_start(ch) || ch == '_';
        };
        if (ch_uni.transform(is_id_start).value_or(0))
          return tokenize_identif(*ch_uni);
        else
          return std::unexpected{INVALID_ERR::BACKSLASH_ESCAPE};
      } else {
        if (ahead_opt)
          prev();
        return U'\\';
      }
    }
    case '=': {
      int i;
      for (i = 0; i < 2; i++) {
        if (reached_end())
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
        return TOK_OPERATOR::DOUBLE_EQUALS;
      case 2:
        return TOK_OPERATOR::TRIPLE_EQUALS;
      }
    }
    case '&': {
      std::optional ch_opt{next()};
      if (ch_opt == '=')
        return TOK_OPERATOR::BITWISE_CONJUNCT_ASSIGN;
      if (ch_opt != '&') {
        backtrack(ch_opt.has_value());
        return U'&';
      }
      ch_opt = next();
      if (ch_opt == '=')
        return TOK_OPERATOR::LOGICAL_CONJUNCT_ASSIGN;
      else {
        backtrack(ch_opt.has_value());
        return TOK_OPERATOR::LOGICAL_CONJUNCT;
      }
    }
    case '|': {
      std::optional ch_opt{next()};
      if (ch_opt == '=')
        return TOK_OPERATOR::BITWISE_DISJUNCT_ASSIGN;
      if (ch_opt != '|') {
        backtrack(ch_opt.has_value());
        return U'|';
      }
      ch_opt = next();
      if (ch_opt == '=')
        return TOK_OPERATOR::LOGICAL_DISJUNCT_ASSIGN;
      else {
        backtrack(ch_opt.has_value());
        return TOK_OPERATOR::LOGICAL_DISJUNCT;
      }
    }
    default:
      if (std::isdigit(leading))
        return TokNumber::tokenize(leading);
      if (uc_is_property_white_space(leading))
        break;
      if (uc_is_property_xid_start(leading) || leading == '_')
        return tokenize_identif(leading);
      return leading;
    }
  }
}

std::size_t TokAtom::atom_find(std::string needle) {
  auto it_prealloc = std::ranges::find(persistent_arr, needle);
  if (it_prealloc != persistent_arr.end())
    return std::distance(persistent_arr.begin(), it_prealloc);
  auto it_umap = atom_umap.find(needle);
  if (it_umap != atom_umap.end())
    return it_umap->second;
  auto it_vec = atom_vec.insert(atom_vec.end(), std::move(needle));
  std::size_t atom_idx = std::distance(atom_vec.begin(), it_vec) << 16;
  atom_umap[*it_vec] = atom_idx;
  return atom_idx;
}

std::optional<TOK_0PREFIX> TokNumber::decode_0prefix() {
  std::u32string ahead{};
  if (reached_end())
    goto bail;
  ahead.push_back(unchecked_next());
  if (std::isdigit(ahead.front())) {
    while (1) {
      if (ahead.back() < '0' || ahead.back() > '7')
        break;
      std::optional trail_opt{next()};
      if (not trail_opt)
        break;
      ahead.push_back(*trail_opt);
    }
    if (ahead.back() == '8' || ahead.back() == '9')
      goto bail;
    backtrack(ahead.size());
    return TOK_0PREFIX::ZERO;
  }
bail:
  backtrack(ahead.size() + 1);
  return std::nullopt;
}

int radix_from_ind(std::optional<TOK_0PREFIX> ind_opt) {
  do {
    if (not ind_opt)
      break;
    switch (*ind_opt) {
    case TOK_0PREFIX::ZERO_B:
      return 2;
    case TOK_0PREFIX::ZERO:
    case TOK_0PREFIX::ZERO_O:
      return 8;
    case TOK_0PREFIX::ZERO_X:
      return 16;
    }
  } while (0);
  return 10;
}

std::string TokNumber::scan_numseq(std::optional<TOK_0PREFIX> base_opt,
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
    accum.append_range(traverse_ucs4(*ahead));
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
  if (reached_end())
    return leading - '0';
  std::optional<TOK_0PREFIX> base_opt{};
  do {
    if (leading == '0') {
      base_opt = decode_0prefix();
      /* there must be a digit after the indicator */
      std::optional ahead{next()};
      if (not ahead.and_then(decode_hex))
        return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
      leading = *ahead;
    }
  } while (0);
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
  bool is_bigint{};
  do {
    std::optional ahead{next()};
    if (ahead != 'n')
      backtrack(ahead.has_value());
    else if (repr_node.index() > 0 || base_opt == TOK_0PREFIX::ZERO)
      return std::unexpected{INVALID_ERR::BIGINT_LITERAL};
    else {
      is_bigint = 1;
      ahead = peek();
    }
    if (not ahead || !uc_is_property_xid_continue(*ahead))
      break;
    return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
  } while (0);
  int radix{radix_from_ind(base_opt)};
  do {
    if (is_bigint) {
      auto bigint_it = bigint_vec.insert(
          bigint_vec.end(),
          mpz_class{std::get<WHOLE>(repr_node).repr_s, radix});
      std::size_t bigint_idx = std::distance(bigint_vec.begin(), bigint_it);
      return TOK_BIGINT{bigint_idx};
    } else if (radix == 10) {
      double result{};
      std::string repr_s = repr_node.visit([](auto n) { return n.collapse(); });
      auto status =
          std::from_chars(repr_s.data(), repr_s.data() + repr_s.size(), result);
      if (status.ec == std::errc::result_out_of_range)
        break;
      else if (status.ec != std::errc{})
        return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
      return result;
    } else {
      std::uint64_t result{};
      std::string repr_s = std::move(std::get<WHOLE>(repr_node).repr_s);
      auto status = std::from_chars(
          repr_s.data(), repr_s.data() + repr_s.size(), result, radix);
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
  } while (0);
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

#define TRY_EXP(void_exp)                                                      \
  do {                                                                         \
    std::expected ok{void_exp};                                                \
    if (ok)                                                                    \
      break;                                                                   \
    return std::unexpected{ok.error()};                                        \
  } while (0);

std::expected<void, PARSE_ERRMSG> Parser::parse_variable_decl() {
  DECL_VARIABLE declaration{};

  std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
  bool valid_beginning{atom_sh == S_ATOM_const || atom_sh == S_ATOM_let ||
                       atom_sh == S_ATOM_var};
  if (not valid_beginning)
    throw std::runtime_error("statement isn't a variable declaration!");
  declaration.kind = atom_sh;
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
  my_statement = std::move(declaration);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_additive_expr() {
  TRY_EXP(parse_postfix_expr())
  do {
    if (my_token == TOKEN{U'+'})
      break;
    if (my_token == TOKEN{U'-'})
      break;
    return {};
  } while (0);
  EXPRESSION expr_left = std::move(my_expression);
  TOKEN op = my_token;
  TRY_EXP(tokenize())
  TRY_EXP(parse_postfix_expr())
  my_expression = std::make_unique<EXPR_BINARY>(std::move(expr_left),
                                                std::move(my_expression), op);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_relation_expr() {
  TRY_EXP(parse_additive_expr())
  do {
    if (my_token == TOKEN{U'>'})
      break;
    if (my_token == TOKEN{U'<'})
      break;
    return {};
  } while (0);
  EXPRESSION expr_left = std::move(my_expression);
  TOKEN op = my_token;
  TRY_EXP(tokenize())
  TRY_EXP(parse_additive_expr())
  my_expression = std::make_unique<EXPR_BINARY>(std::move(expr_left),
                                                std::move(my_expression), op);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_equality_expr() {
  TRY_EXP(parse_relation_expr())
  do {
    if (my_token == TOKEN{TOK_OPERATOR::DOUBLE_EQUALS})
      break;
    if (my_token == TOKEN{TOK_OPERATOR::TRIPLE_EQUALS})
      break;
    return {};
  } while (0);
  EXPRESSION expr_left = std::move(my_expression);
  TOKEN op = my_token;
  TRY_EXP(tokenize())
  TRY_EXP(parse_relation_expr())
  my_expression = std::make_unique<EXPR_BINARY>(std::move(expr_left),
                                                std::move(my_expression), op);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_object_literal() {
  TRY_EXP(tokenize())
  std::vector<EXPR_OBJECT::PROPERTY> prop_vec{};
  while (my_token != TOKEN{U'}'}) {
    EXPRESSION prop_key{};
    do {
      if (my_token == TOKEN{U'['}) {
        TRY_EXP(tokenize())
        TRY_EXP(parse_assign_expr())
        prop_key = std::move(my_expression);
        TRY_EXP(expect_punct(']'))
        TRY_EXP(tokenize())
        break;
      }
      if (my_token.index() == TOKV_IDENTI) {
        prop_key = std::get<TOK_IDENTI>(my_token);
        TRY_EXP(tokenize())
        break;
      }
      return std::unexpected{INVALID_ERR::PROPERTY_NAME};
    } while (0);
    EXPR_OBJECT::PROPERTY property{};
    do {
      if (my_token == TOKEN{U':'}) {
        TRY_EXP(tokenize())
        TRY_EXP(parse_assign_expr())
        property = EXPR_OBJECT::KEY_VALUE{std::move(prop_key),
                                          std::move(my_expression)};
        break;
      }
      if (my_token == TOKEN{U'('}) {
        TRY_EXP(parse_function_decl(std::move(prop_key)))
        property = std::move(std::get<DECL_FUNCTION>(my_statement));
        TRY_EXP(tokenize())
        break;
      } else {
        property =
            EXPR_OBJECT::KEY_VALUE{std::move(prop_key), std::monostate{}};
        break;
      }
      return std::unexpected{PUNCT_ERR{U':'}};
    } while (0);
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
    switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
    case S_ATOM_function: {
      TRY_EXP(tokenize())
      TRY_EXP(parse_function_decl(std::monostate{}))
      my_expression = std::make_unique<DECL_FUNCTION>(
          std::move(std::get<DECL_FUNCTION>(my_statement)));
      return {};
    }
    default:
      my_expression = std::get<TOKV_IDENTI>(my_token);
      return {};
    }
  case TOKV_NUMBER:
    my_expression = std::get<TOKV_NUMBER>(my_token);
    return {};
  case TOKV_PUNCT:
    if (my_token == TOKEN{U'{'}) {
      TRY_EXP(parse_object_literal());
      return {};
    }
    [[fallthrough]];
  default:
    return std::unexpected{UNEXPECTED_ERR::THIS_TOKEN};
  }
}

std::expected<void, PARSE_ERRMSG> Parser::expect_punct(char32_t punct) {
  if (my_token == TOKEN{punct})
    return {};
  return std::unexpected{PUNCT_ERR{punct}};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_call_expr() {
  EXPRESSION callee_expr = std::move(my_expression);
  std::vector<EXPRESSION> arguments{};
  while (1) {
    TRY_EXP(tokenize())
    if (my_token == TOKEN{U')'})
      break;
    TRY_EXP(parse_assign_expr())
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
  TRY_EXP(parse_logical_disjunct())
  if (my_token != TOKEN{U'='})
    return {};
  TRY_EXP(tokenize())
  EXPRESSION lhs_expr = std::move(my_expression);
  TRY_EXP(parse_assign_expr())
  my_expression = std::make_unique<EXPR_ASSIGN>(std::move(lhs_expr),
                                                std::move(my_expression));
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_logical_conjunct() {
  TRY_EXP(parse_equality_expr())
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_CONJUNCT}) {
    EXPRESSION expr_left = std::move(my_expression);
    TOKEN op = my_token;
    TRY_EXP(tokenize())
    TRY_EXP(parse_equality_expr())
    my_expression = std::make_unique<EXPR_LOGICAL>(
        std::move(expr_left), std::move(my_expression), op);
  }
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_logical_disjunct() {
  TRY_EXP(parse_logical_conjunct())
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_DISJUNCT}) {
    EXPRESSION expr_left = std::move(my_expression);
    TOKEN op = my_token;
    TRY_EXP(tokenize())
    TRY_EXP(parse_logical_conjunct())
    my_expression = std::make_unique<EXPR_LOGICAL>(
        std::move(expr_left), std::move(my_expression), op);
  }
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_postfix_expr() {
  TRY_EXP(parse_primary_expr())
  while (1) {
    bool go_on{};
    TRY_EXP(tokenize())
    if (my_token.index() != TOKV_PUNCT)
      break;
    switch (std::get<TOKV_PUNCT>(my_token)) {
    case '.':
      TRY_EXP(parse_member_expr())
      go_on = 1;
      break;
    case '(':
      TRY_EXP(parse_call_expr())
      go_on = 1;
      break;
    case '[':
      TRY_EXP(parse_access_expr())
      go_on = 1;
      break;
    }
    if (not go_on)
      break;
  }
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_paren_expr() {
  TRY_EXP(expect_punct('('))
  TRY_EXP(tokenize())
  TRY_EXP(parse_assign_expr())
  TRY_EXP(expect_punct(')'))
  TRY_EXP(tokenize())
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

std::expected<void, PARSE_ERRMSG>
Parser::parse_function_decl(EXPRESSION identifier) {
  DECL_FUNCTION declaration{std::move(identifier)};
  TRY_EXP(expect_punct('('))
  TRY_EXP(tokenize())
  while (my_token != TOKEN{U')'}) {
    if (my_token.index() != TOKV_IDENTI)
      return std::unexpected{NEEDED_ERR::FORMAL_PARAMETER};
    std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
    if (is_reserved(atom_sh))
      return std::unexpected{RESERVED_ERR{atom_sh}};
    declaration.arguments.push_back(atom_sh);
    TRY_EXP(tokenize())
    if (my_token == TOKEN{U')'})
      break;
    TRY_EXP(expect_punct(','))
    TRY_EXP(tokenize())
  }
  TRY_EXP(tokenize())
  TRY_EXP(expect_punct('{'))
  TRY_EXP(tokenize())
  while (my_token != TOKEN{U'}'}) {
    TRY_EXP(parse_statement())
    declaration.subprogram.push_back(std::move(my_statement));
  }
  my_statement = std::move(declaration);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_import() {
  DECL_IMPORT declaration{};
  TRY_EXP(expect_punct('{'))
  TRY_EXP(tokenize())
  while (my_token != TOKEN{U'}'}) {
    if (my_token.index() != TOKV_IDENTI)
      return std::unexpected{NEEDED_ERR::IDENTIFIER};
    declaration.specifiers.push_back(std::get<TOKV_IDENTI>(my_token));
    TRY_EXP(tokenize())
    if (my_token != TOKEN{U','})
      break;
    TRY_EXP(tokenize())
  }
  TRY_EXP(expect_punct('}'))
  TRY_EXP(tokenize())
  if (my_token != TOKEN{TOK_IDENTI{0, S_ATOM_from}})
    return std::unexpected{NEEDED_ERR::FROM_CLAUSE};
  TRY_EXP(tokenize())
  if (my_token.index() != TOKV_STRING)
    return std::unexpected{NEEDED_ERR::STRING_LITERAL};
  declaration.source = std::get<TOKV_STRING>(my_token);
  TRY_EXP(tokenize())
  TRY_EXP(expect_statement_end())
  my_statement = std::move(declaration);
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_stmt_expression() {
  TRY_EXP(parse_assign_expr())
  my_statement = std::move(my_expression);
  TRY_EXP(expect_statement_end())
  return {};
}

std::expected<void, PARSE_ERRMSG> Parser::parse_punct_statement() {
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '{': {
    STMT_BLOCK statement{};
    TRY_EXP(tokenize())
    while (my_token != TOKEN{U'}'}) {
      TRY_EXP(parse_statement())
      statement.subprogram.push_back(std::move(my_statement));
    }
    TRY_EXP(tokenize())
    my_statement = std::move(statement);
    return {};
  }
  default:
    TRY_EXP(parse_stmt_expression())
    return {};
  }
}

std::expected<void, PARSE_ERRMSG> Parser::parse_ident_statement() {
  switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var:
    TRY_EXP(parse_variable_decl())
    TRY_EXP(expect_statement_end())
    return {};
  case S_ATOM_function: {
    TRY_EXP(tokenize())
    if (my_token.index() != TOKV_IDENTI)
      return std::unexpected{NEEDED_ERR::FUNCTION_NAME};
    TOK_IDENTI identifier{std::get<TOKV_IDENTI>(my_token)};
    TRY_EXP(tokenize())
    TRY_EXP(parse_function_decl(identifier))
    TRY_EXP(tokenize())
    return {};
  }
  case S_ATOM_return: {
    STMT_RETURN statement{};
    TRY_EXP(tokenize())
    TRY_EXP(parse_assign_expr())
    statement.argument = std::move(my_expression);
    TRY_EXP(expect_statement_end())
    my_statement = std::move(statement);
    return {};
  }
  case S_ATOM_import:
    TRY_EXP(tokenize())
    TRY_EXP(parse_import())
    return {};
  case S_ATOM_if: {
    STMT_IF statement{};
    TRY_EXP(tokenize())
    TRY_EXP(parse_paren_expr())
    statement.condition = std::move(my_expression);
    TRY_EXP(parse_statement())
    statement.consequent = std::make_unique<STATEMENT>(std::move(my_statement));
    my_statement = std::move(statement);
    return {};
  }
  default:
    TRY_EXP(parse_stmt_expression())
    return {};
  }
}

std::expected<void, PARSE_ERRMSG> Parser::parse_statement() {
  switch (my_token.index()) {
  case TOKV_IDENTI:
    return parse_ident_statement();
  case TOKV_PUNCT:
    return parse_punct_statement();
  default:
    return parse_stmt_expression();
  }
}

std::expected<void, PARSE_ERRMSG> Parser::parse() {
  skip_shebang();
  TRY_EXP(tokenize())
  while (1) {
    if (my_token.index() == TOKV_EOF)
      return {};
    std::expected status{parse_statement()};
    if (not status)
      return std::unexpected{status.error()};
    program.push_back(std::move(my_statement));
  }
}
} // namespace Language
} // namespace Manadrain
