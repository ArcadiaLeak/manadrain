#include <algorithm>
#include <charconv>
#include <limits>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"
#include "static_atoms.hpp"

namespace Manadrain {
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
      err_opt = UNEXPECT_ERR::STRING_END;
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
          err_opt = UNEXPECT_ERR::STRING_END;
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
            return std::unexpected{UNEXPECT_ERR::COMMENT_END};
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
  auto it_prealloc = std::ranges::find(S_ATOM_ARR, needle);
  if (it_prealloc != S_ATOM_ARR.end())
    return std::distance(S_ATOM_ARR.begin(), it_prealloc);
  auto it_umap = atom_umap.find(needle);
  if (it_umap != atom_umap.end())
    return it_umap->second;
  auto it_deq = atom_deq.insert(atom_deq.end(), std::move(needle));
  std::size_t atom_idx = std::distance(atom_deq.begin(), it_deq) << 16;
  atom_umap[*it_deq] = atom_idx;
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
  if (not tokenize_ok)
    return std::unexpected{tokenize_ok.error()};
  my_token = *tokenize_ok;
  return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_variable_decl() {
  DECL_VARIABLE declaration{};

  std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
  bool valid_beginning{atom_sh == S_ATOM_const || atom_sh == S_ATOM_let ||
                       atom_sh == S_ATOM_var};
  if (not valid_beginning)
    throw std::runtime_error("statement isn't a variable declaration!");
  declaration.kind = atom_sh;
  co_await tokenize();

  if (my_token.index() != TOKV_IDENTI)
    co_return std::unexpected{REQUIRED_ERR::VARIABLE_NAME};
  declaration.identifier = std::get<TOK_IDENTI>(my_token);
  co_await tokenize();

  if (my_token != TOKEN{U'='})
    goto wrap_up;
  co_await tokenize();

  co_await parse_assign_expr().ok();
  declaration.initializer = std::move(my_expression);

wrap_up:
  my_statement = std::move(declaration);
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_additive_expr() {
  co_await parse_postfix_expr().ok();
  do {
    if (my_token == TOKEN{U'+'})
      break;
    if (my_token == TOKEN{U'-'})
      break;
    co_return {};
  } while (0);
  EXPRESSION expr_left = std::move(my_expression);
  TOKEN op = my_token;
  co_await tokenize();
  co_await parse_postfix_expr().ok();
  my_expression = std::make_unique<EXPR_BINARY>(std::move(expr_left),
                                                std::move(my_expression), op);
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_relation_expr() {
  co_await parse_additive_expr().ok();
  do {
    if (my_token == TOKEN{U'>'})
      break;
    if (my_token == TOKEN{U'<'})
      break;
    co_return {};
  } while (0);
  EXPRESSION expr_left = std::move(my_expression);
  TOKEN op = my_token;
  co_await tokenize();
  co_await parse_additive_expr().ok();
  my_expression = std::make_unique<EXPR_BINARY>(std::move(expr_left),
                                                std::move(my_expression), op);
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_equality_expr() {
  co_await parse_relation_expr().ok();
  do {
    if (my_token == TOKEN{TOK_OPERATOR::DOUBLE_EQUALS})
      break;
    if (my_token == TOKEN{TOK_OPERATOR::TRIPLE_EQUALS})
      break;
    co_return {};
  } while (0);
  EXPRESSION expr_left = std::move(my_expression);
  TOKEN op = my_token;
  co_await tokenize();
  co_await parse_relation_expr().ok();
  my_expression = std::make_unique<EXPR_BINARY>(std::move(expr_left),
                                                std::move(my_expression), op);
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_object_literal() {
  co_await tokenize();
  std::vector<EXPR_OBJECT::PROPERTY> prop_vec{};
  while (my_token != TOKEN{U'}'}) {
    EXPRESSION prop_key{};
    do {
      if (my_token == TOKEN{U'['}) {
        co_await tokenize();
        co_await parse_assign_expr().ok();
        prop_key = std::move(my_expression);
        co_await expect_punct(']');
        co_await tokenize();
        break;
      }
      if (my_token.index() == TOKV_IDENTI) {
        prop_key = std::get<TOK_IDENTI>(my_token);
        co_await tokenize();
        break;
      }
      co_return std::unexpected{INVALID_ERR::PROPERTY_NAME};
    } while (0);
    EXPR_OBJECT::PROPERTY property{};
    do {
      if (my_token == TOKEN{U':'}) {
        co_await tokenize();
        co_await parse_assign_expr().ok();
        property = EXPR_OBJECT::KEY_VALUE{std::move(prop_key),
                                          std::move(my_expression)};
        break;
      }
      if (my_token == TOKEN{U'('}) {
        co_await parse_function_decl(std::move(prop_key)).ok();
        property = std::move(std::get<DECL_FUNCTION>(my_statement));
        co_await tokenize();
        break;
      } else {
        property =
            EXPR_OBJECT::KEY_VALUE{std::move(prop_key), std::monostate{}};
        break;
      }
      co_return std::unexpected{PUNCT_ERR{U':'}};
    } while (0);
    prop_vec.push_back(std::move(property));
    if (my_token != TOKEN{U','})
      break;
    co_await tokenize();
  }
  co_await expect_punct('}');
  co_await tokenize();
  my_expression = std::make_unique<EXPR_OBJECT>(std::move(prop_vec));
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_primary_expr() {
  switch (my_token.index()) {
  case TOKV_STRING:
    my_expression = std::get<TOKV_STRING>(my_token);
    co_return {};
  case TOKV_IDENTI:
    switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
    case S_ATOM_function: {
      co_await tokenize();
      co_await parse_function_decl(std::monostate{}).ok();
      my_expression = std::make_unique<DECL_FUNCTION>(
          std::move(std::get<DECL_FUNCTION>(my_statement)));
      co_return {};
    }
    default:
      my_expression = std::get<TOKV_IDENTI>(my_token);
      co_return {};
    }
  case TOKV_NUMBER:
    my_expression = std::get<TOKV_NUMBER>(my_token);
    co_return {};
  case TOKV_PUNCT:
    if (my_token == TOKEN{U'{'}) {
      co_await parse_object_literal().ok();
      co_return {};
    }
    [[fallthrough]];
  default:
    co_return std::unexpected{UNEXPECT_ERR::THIS_TOKEN};
  }
}

std::expected<void, PARSE_ERRMSG> Parser::expect_punct(char32_t punct) {
  if (my_token == TOKEN{punct})
    return {};
  return std::unexpected{PUNCT_ERR{punct}};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_call_expr() {
  EXPRESSION callee_expr = std::move(my_expression);
  std::vector<EXPRESSION> arguments{};
  while (1) {
    co_await tokenize();
    if (my_token == TOKEN{U')'})
      break;
    co_await parse_assign_expr().ok();
    if (my_token == TOKEN{U')'})
      break;
    co_await expect_punct(',');
    arguments.push_back(std::move(my_expression));
  }
  my_expression =
      std::make_unique<EXPR_CALL>(std::move(callee_expr), std::move(arguments));
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_member_expr() {
  co_await tokenize();
  if (my_token.index() == TOKV_IDENTI) {
    my_expression = std::make_unique<EXPR_MEMBER>(
        std::move(my_expression), std::get<TOK_IDENTI>(my_token));
    co_return {};
  }
  co_return std::unexpected{REQUIRED_ERR::FIELD_NAME};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_access_expr() {
  co_await tokenize();
  EXPRESSION object_expr = std::move(my_expression);
  co_await parse_assign_expr().ok();
  co_await expect_punct(']');
  my_expression = std::make_unique<EXPR_ACCESS>(std::move(object_expr),
                                                std::move(my_expression));
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_assign_expr() {
  co_await parse_logical_disjunct().ok();
  if (my_token != TOKEN{U'='})
    co_return {};
  co_await tokenize();
  EXPRESSION lhs_expr = std::move(my_expression);
  co_await parse_assign_expr().ok();
  my_expression = std::make_unique<EXPR_ASSIGN>(std::move(lhs_expr),
                                                std::move(my_expression));
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_logical_conjunct() {
  co_await parse_equality_expr().ok();
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_CONJUNCT}) {
    EXPRESSION expr_left = std::move(my_expression);
    TOKEN op = my_token;
    co_await tokenize();
    co_await parse_equality_expr().ok();
    my_expression = std::make_unique<EXPR_LOGICAL>(
        std::move(expr_left), std::move(my_expression), op);
  }
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_logical_disjunct() {
  co_await parse_logical_conjunct().ok();
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_DISJUNCT}) {
    EXPRESSION expr_left = std::move(my_expression);
    TOKEN op = my_token;
    co_await tokenize();
    co_await parse_logical_conjunct().ok();
    my_expression = std::make_unique<EXPR_LOGICAL>(
        std::move(expr_left), std::move(my_expression), op);
  }
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_postfix_expr() {
  co_await parse_primary_expr().ok();
  while (1) {
    bool go_on{};
    co_await tokenize();
    if (my_token.index() != TOKV_PUNCT)
      break;
    switch (std::get<TOKV_PUNCT>(my_token)) {
    case '.':
      co_await parse_member_expr().ok();
      go_on = 1;
      break;
    case '(':
      co_await parse_call_expr().ok();
      go_on = 1;
      break;
    case '[':
      co_await parse_access_expr().ok();
      go_on = 1;
      break;
    }
    if (not go_on)
      break;
  }
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_paren_expr() {
  co_await expect_punct('(');
  co_await tokenize();
  co_await parse_assign_expr().ok();
  co_await expect_punct(')');
  co_await tokenize();
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::expect_statement_end() {
  if (my_token == TOKEN{U';'}) {
    co_await tokenize();
    co_return {};
  }
  bool insertion =
      my_token.index() == TOKV_EOF || my_token == TOKEN{U'}'} || newlineSeen();
  if (insertion)
    co_return {};
  co_return std::unexpected{PUNCT_ERR{';'}};
}

expected_task<void, PARSE_ERRMSG>
Parser::parse_function_decl(EXPRESSION identifier) {
  DECL_FUNCTION declaration{std::move(identifier)};
  co_await expect_punct('(');
  co_await tokenize();
  while (my_token != TOKEN{U')'}) {
    if (my_token.index() != TOKV_IDENTI)
      co_return std::unexpected{REQUIRED_ERR::FORMAL_PARAMETER};
    std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
    if (is_reserved(atom_sh))
      co_return std::unexpected{KEYWORD_ERR{atom_sh}};
    declaration.arguments.push_back(atom_sh);
    co_await tokenize();
    if (my_token == TOKEN{U')'})
      break;
    co_await expect_punct(',');
    co_await tokenize();
  }
  co_await tokenize();
  co_await expect_punct(':');
  co_await tokenize();
  if (my_token.index() != TOKV_IDENTI)
    co_return std::unexpected{REQUIRED_ERR::RETURN_TYPE};
  declaration.return_type = std::get<TOKV_IDENTI>(my_token);
  co_await tokenize();
  co_await expect_punct('{');
  co_await tokenize();
  while (my_token != TOKEN{U'}'}) {
    co_await parse_statement();
    declaration.subprogram.push_back(std::move(my_statement));
  }
  my_statement = std::move(declaration);
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_import() {
  DECL_IMPORT declaration{};
  co_await expect_punct('{');
  co_await tokenize();
  while (my_token != TOKEN{U'}'}) {
    if (my_token.index() != TOKV_IDENTI)
      co_return std::unexpected{REQUIRED_ERR::IDENTIFIER};
    declaration.specifiers.push_back(std::get<TOKV_IDENTI>(my_token));
    co_await tokenize();
    if (my_token != TOKEN{U','})
      break;
    co_await tokenize();
  }
  co_await expect_punct('}');
  co_await tokenize();
  if (my_token != TOKEN{TOK_IDENTI{0, S_ATOM_from}})
    co_return std::unexpected{REQUIRED_ERR::FROM_CLAUSE};
  co_await tokenize();
  if (my_token.index() != TOKV_STRING)
    co_return std::unexpected{REQUIRED_ERR::STRING_LITERAL};
  declaration.source = std::get<TOKV_STRING>(my_token);
  co_await tokenize();
  co_await expect_statement_end().ok();
  my_statement = std::move(declaration);
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_stmt_expression() {
  co_await parse_assign_expr().ok();
  my_statement = std::move(my_expression);
  co_await expect_statement_end().ok();
  co_return {};
}

expected_task<void, PARSE_ERRMSG> Parser::parse_punct_statement() {
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '{': {
    STMT_BLOCK statement{};
    co_await tokenize();
    while (my_token != TOKEN{U'}'}) {
      co_await parse_statement();
      statement.subprogram.push_back(std::move(my_statement));
    }
    co_await tokenize();
    my_statement = std::move(statement);
    co_return {};
  }
  default:
    co_await parse_stmt_expression().ok();
    co_return {};
  }
}

expected_task<void, PARSE_ERRMSG> Parser::parse_ident_statement() {
  switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var:
    co_await parse_variable_decl().ok();
    co_await expect_statement_end().ok();
    co_return {};
  case S_ATOM_function: {
    co_await tokenize();
    if (my_token.index() != TOKV_IDENTI)
      co_return std::unexpected{REQUIRED_ERR::FUNCTION_NAME};
    TOK_IDENTI identifier{std::get<TOKV_IDENTI>(my_token)};
    co_await tokenize();
    co_await parse_function_decl(identifier).ok();
    co_await tokenize();
    co_return {};
  }
  case S_ATOM_return: {
    STMT_RETURN statement{};
    co_await tokenize();
    co_await parse_assign_expr().ok();
    statement.argument = std::move(my_expression);
    co_await expect_statement_end().ok();
    my_statement = std::move(statement);
    co_return {};
  }
  case S_ATOM_import:
    co_await tokenize();
    co_await parse_import().ok();
    co_return {};
  case S_ATOM_if: {
    STMT_IF statement{};
    co_await tokenize();
    co_await parse_paren_expr().ok();
    statement.condition = std::move(my_expression);
    co_await parse_statement();
    statement.consequent = std::make_unique<STATEMENT>(std::move(my_statement));
    my_statement = std::move(statement);
    co_return {};
  }
  default:
    co_await parse_stmt_expression().ok();
    co_return {};
  }
}

std::expected<void, PARSE_ERRMSG> Parser::parse_statement() {
  switch (my_token.index()) {
  case TOKV_IDENTI:
    return parse_ident_statement().ok();
  case TOKV_PUNCT:
    return parse_punct_statement().ok();
  default:
    return parse_stmt_expression().ok();
  }
}

expected_task<void, PARSE_ERRMSG> Parser::parse() {
  skip_shebang();
  co_await tokenize();
  while (1) {
    if (my_token.index() == TOKV_EOF)
      co_return {};
    co_await parse_statement();
    program.push_back(std::move(my_statement));
  }
}
} // namespace Manadrain
