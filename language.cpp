#include <algorithm>
#include <cassert>
#include <charconv>
#include <functional>
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
  assert(len >= 0);
  for (int i = 0; i < len; ++i)
    co_yield ubuf[i];
}

bool Scanner::reached_end() { return position >= buffer.size(); }

void Scanner::prev() {
  assert(not backtrace.empty());
  position -= backtrace.top();
  backtrace.pop();
}

char32_t Scanner::unchecked_next() {
  ucs4_t ch;
  int len = u8_mbtoucr(&ch, buffer.data() + position, buffer.size() - position);
  assert(len >= 0);
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

std::generator<std::expected<char32_t, PARSE_ERROR::MESSAGE>>
TokAtom::traverse_string(char32_t separator) {
  std::optional<PARSE_ERROR::MESSAGE> err_opt{};
  bool backslash_seen{};
  while (not err_opt) {
    std::optional ch_opt{next()};
    if (not ch_opt) {
      err_opt = UNEXPECTED_STRING_END{};
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
          err_opt = INVALID_BACKSLASH_ESCAPE{};
        break;
      }
      case 'u': {
        std::optional uni = decode_string_uni();
        if (uni)
          co_yield *uni;
        else
          err_opt = INVALID_BACKSLASH_ESCAPE{};
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
        err_opt = INVALID_BACKSLASH_ESCAPE{};
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
          err_opt = UNEXPECTED_STRING_END{};
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

std::expected<TOKEN, PARSE_ERROR::MESSAGE>
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

std::expected<TOKEN, PARSE_ERROR::MESSAGE>
TokAtom::tokenize_identif(char32_t leading) {
  std::string needle{};
  needle.append_range(traverse_ucs4(leading));
  bool has_escape = 0;
  for (std::optional ichar : traverse_identif(has_escape)) {
    if (not ichar)
      return std::unexpected{INVALID_BACKSLASH_ESCAPE{}};
    needle.append_range(traverse_ucs4(*ichar));
  }
  return TOK_IDENTI{has_escape, atom_find(std::move(needle))};
}

std::expected<TOKEN, PARSE_ERROR::MESSAGE> Tokenizer::tokenize() {
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
            return std::unexpected{UNEXPECTED_COMMENT_END{}};
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
          return std::unexpected{INVALID_BACKSLASH_ESCAPE{}};
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
  std::size_t atom_idx =
      (1 << 15) | (std::distance(atom_deq.begin(), it_deq) << 16);
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

std::expected<TOKEN, PARSE_ERROR::MESSAGE>
TokNumber::tokenize(char32_t leading) {
  if (reached_end())
    return std::int64_t{leading - '0'};
  std::optional<TOK_0PREFIX> base_opt{};
  do {
    if (leading == '0') {
      base_opt = decode_0prefix();
      /* there must be a digit after the indicator */
      std::optional ahead{next()};
      if (not ahead.and_then(decode_hex))
        return std::unexpected{INVALID_NUMBER_LITERAL{}};
      leading = *ahead;
    }
  } while (0);
  NUM_REPRESENT num_repr{};
  do {
    WHOLE whole_n{scan_numseq(base_opt, leading)};
    num_repr = whole_n;
    if (base_opt)
      break;
    if (next() != '.') {
      prev();
      break;
    }
    FRACTIONAL frac_n{std::move(whole_n),
                      scan_numseq(std::nullopt, std::nullopt)};
    num_repr = frac_n;
  } while (0);
  bool is_bigint{};
  do {
    std::optional ahead{next()};
    if (ahead != 'n')
      backtrack(ahead.has_value());
    else if (num_repr.index() > 0 || base_opt == TOK_0PREFIX::ZERO)
      return std::unexpected{INVALID_BIGINT_LITERAL{}};
    else {
      is_bigint = 1;
      ahead = peek();
    }
    if (not ahead || !uc_is_property_xid_continue(*ahead))
      break;
    return std::unexpected{INVALID_NUMBER_LITERAL{}};
  } while (0);
  int radix{radix_from_ind(base_opt)};
  do {
    if (is_bigint) {
      auto bigint_it = bigint_vec.insert(
          bigint_vec.end(), mpz_class{std::get<WHOLE>(num_repr).repr_s, radix});
      std::size_t bigint_idx = std::distance(bigint_vec.begin(), bigint_it);
      return TOK_BIGINT{bigint_idx};
    } else if (std::holds_alternative<WHOLE>(num_repr)) {
      std::int64_t result{};
      std::string repr_s = std::move(std::get<WHOLE>(num_repr).repr_s);
      auto status = std::from_chars(
          repr_s.data(), repr_s.data() + repr_s.size(), result, radix);
      if (status.ec != std::errc{})
        return std::unexpected{INVALID_NUMBER_LITERAL{}};
      return result;
    } else {
      double result{};
      std::string repr_s = num_repr.visit([](auto n) { return n.collapse(); });
      auto status =
          std::from_chars(repr_s.data(), repr_s.data() + repr_s.size(), result);
      if (status.ec == std::errc::result_out_of_range)
        return std::numeric_limits<double>::infinity();
      else if (status.ec != std::errc{})
        return std::unexpected{INVALID_NUMBER_LITERAL{}};
      return result;
    }
  } while (0);
}

void Parser::tokenize() {
  std::expected tokenize_ok = Tokenizer::tokenize();
  if (tokenize_ok) {
    my_token = *tokenize_ok;
    return;
  }
  throw PARSE_ERROR{tokenize_ok.error()};
}

STATEMENT Parser::parse_variable_decl() {
  DECL_VARIABLE declaration{};

  std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
  assert(atom_sh == S_ATOM_const || atom_sh == S_ATOM_let ||
         atom_sh == S_ATOM_var);
  declaration.kind = atom_sh;
  tokenize();

  if (my_token.index() != TOKV_IDENTI)
    throw PARSE_ERROR{MISSING_VARIABLE_NAME{}};
  declaration.identifier = std::get<TOK_IDENTI>(my_token);
  tokenize();
  declaration.datatype = parse_type_annotation();
  tokenize();

  if (my_token != TOKEN{U'='})
    return declaration;
  tokenize();

  declaration.initializer = parse_assign_expr();
  return declaration;
}

EXPRESSION Parser::parse_additive_expr() {
  EXPRESSION expr_left{parse_postfix_expr()};
  do {
    if (my_token == TOKEN{U'+'})
      break;
    if (my_token == TOKEN{U'-'})
      break;
    return expr_left;
  } while (0);
  TOKEN op = my_token;
  tokenize();
  EXPR_BINARY binary_expr{expr_left, parse_postfix_expr(), op};
  return EXPR_PTR{std::distance(expr_vec.begin(),
                                expr_vec.insert(expr_vec.end(), binary_expr))};
}

EXPRESSION Parser::parse_relation_expr() {
  EXPRESSION expr_left{parse_additive_expr()};
  do {
    if (my_token == TOKEN{U'>'})
      break;
    if (my_token == TOKEN{U'<'})
      break;
    return expr_left;
  } while (0);
  TOKEN op = my_token;
  tokenize();
  EXPR_BINARY binary_expr{expr_left, parse_additive_expr(), op};
  return EXPR_PTR{std::distance(expr_vec.begin(),
                                expr_vec.insert(expr_vec.end(), binary_expr))};
}

EXPRESSION Parser::parse_equality_expr() {
  EXPRESSION expr_left{parse_relation_expr()};
  do {
    if (my_token == TOKEN{TOK_OPERATOR::DOUBLE_EQUALS})
      break;
    if (my_token == TOKEN{TOK_OPERATOR::TRIPLE_EQUALS})
      break;
    return expr_left;
  } while (0);
  TOKEN op = my_token;
  tokenize();
  EXPR_BINARY binary_expr{expr_left, parse_relation_expr(), op};
  return EXPR_PTR{std::distance(expr_vec.begin(),
                                expr_vec.insert(expr_vec.end(), binary_expr))};
}

EXPRESSION Parser::parse_object_literal() {
  tokenize();
  std::vector<std::pair<EXPRESSION, EXPRESSION>> prop_vec{};
  while (my_token != TOKEN{U'}'}) {
    EXPRESSION prop_key{};
    do {
      if (my_token == TOKEN{U'['}) {
        tokenize();
        prop_key = parse_assign_expr();
        expect_punct(']');
        tokenize();
        break;
      }
      if (my_token.index() == TOKV_IDENTI) {
        prop_key = std::get<TOK_IDENTI>(my_token);
        tokenize();
        break;
      }
      throw PARSE_ERROR{INVALID_PROPERTY_NAME{}};
    } while (0);
    if (my_token == TOKEN{U':'}) {
      tokenize();
      prop_vec.emplace_back(prop_key, parse_assign_expr());
    } else
      prop_vec.emplace_back(prop_key, std::monostate{});
    if (my_token != TOKEN{U','})
      break;
    tokenize();
  }
  expect_punct('}');
  tokenize();
  auto ret_expr =
      expr_vec.insert(expr_vec.end(), EXPR_OBJECT{std::move(prop_vec)});
  return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

EXPRESSION Parser::parse_primary_expr() {
  switch (my_token.index()) {
  case TOKV_STRING:
    return std::get<TOKV_STRING>(my_token);
  case TOKV_IDENTI:
    return std::get<TOKV_IDENTI>(my_token);
  case TOKV_FLOAT:
    return EXPR_NUMBER{std::get<TOKV_FLOAT>(my_token)};
  case TOKV_INT:
    return EXPR_NUMBER{std::get<TOKV_INT>(my_token)};
  case TOKV_PUNCT:
    if (my_token == TOKEN{U'{'})
      return parse_object_literal();
    [[fallthrough]];
  default:
    throw PARSE_ERROR{UNEXPECTED_TOKEN{}};
  }
}

void Parser::expect_punct(char32_t punct) {
  if (my_token == TOKEN{punct})
    return;
  throw PARSE_ERROR{MISSING_PUNCT{punct}};
}

EXPRESSION Parser::parse_call_expr(EXPRESSION callee_expr) {
  std::vector<EXPRESSION> arguments{};
  while (1) {
    tokenize();
    if (my_token == TOKEN{U')'})
      break;
    EXPRESSION arg_expr{parse_assign_expr()};
    if (my_token == TOKEN{U')'})
      break;
    expect_punct(',');
    arguments.push_back(arg_expr);
  }
  auto ret_expr = expr_vec.insert(expr_vec.end(),
                                  EXPR_CALL{callee_expr, std::move(arguments)});
  return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

EXPRESSION Parser::parse_member_expr(EXPRESSION obj_expr) {
  tokenize();
  if (my_token.index() != TOKV_IDENTI)
    throw PARSE_ERROR{MISSING_FIELD_NAME{}};
  auto ret_expr = expr_vec.insert(
      expr_vec.end(), EXPR_MEMBER{obj_expr, std::get<TOK_IDENTI>(my_token)});
  return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

EXPRESSION Parser::parse_access_expr(EXPRESSION obj_expr) {
  tokenize();
  EXPRESSION prop_expr{parse_assign_expr()};
  expect_punct(']');
  auto ret_expr =
      expr_vec.insert(expr_vec.end(), EXPR_ACCESS{obj_expr, prop_expr});
  return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

EXPRESSION Parser::parse_assign_expr() {
  EXPRESSION lhs_expr{parse_logical_disjunct()};
  if (my_token != TOKEN{U'='})
    return lhs_expr;
  tokenize();
  auto ret_expr = expr_vec.insert(expr_vec.end(),
                                  EXPR_ASSIGN{lhs_expr, parse_assign_expr()});
  return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

EXPRESSION Parser::parse_logical_conjunct() {
  EXPRESSION expr_left{parse_equality_expr()};
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_CONJUNCT}) {
    TOKEN op = my_token;
    tokenize();
    auto ret_expr = expr_vec.insert(
        expr_vec.end(), EXPR_LOGIC{expr_left, parse_equality_expr(), op});
    expr_left = EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
  }
  return expr_left;
}

EXPRESSION Parser::parse_logical_disjunct() {
  EXPRESSION expr_left{parse_logical_conjunct()};
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_DISJUNCT}) {
    TOKEN op = my_token;
    tokenize();
    auto ret_expr = expr_vec.insert(
        expr_vec.end(), EXPR_LOGIC{expr_left, parse_logical_conjunct(), op});
    expr_left = EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
  }
  return expr_left;
}

EXPRESSION Parser::parse_postfix_expr() {
  EXPRESSION base_expr{parse_primary_expr()};
  while (1) {
    tokenize();
    if (my_token.index() != TOKV_PUNCT)
      return base_expr;
    switch (std::get<TOKV_PUNCT>(my_token)) {
    case '(':
      base_expr = parse_call_expr(base_expr);
      continue;
    case '.':
      base_expr = parse_member_expr(base_expr);
      continue;
    case '[':
      base_expr = parse_access_expr(base_expr);
      continue;
    default:
      return base_expr;
    }
  }
}

EXPRESSION Parser::parse_paren_expr() {
  expect_punct('(');
  tokenize();
  EXPRESSION assign_expr{parse_assign_expr()};
  expect_punct(')');
  tokenize();
  return assign_expr;
}

void Parser::expect_statement_end() {
  if (my_token == TOKEN{U';'})
    tokenize();
  else {
    bool insertion = my_token.index() == TOKV_EOF || my_token == TOKEN{U'}'} ||
                     newlineSeen();
    if (insertion)
      return;
    throw PARSE_ERROR{MISSING_PUNCT{';'}};
  }
}

std::size_t Parser::parse_type_annotation() {
  expect_punct(':');
  tokenize();
  if (my_token.index() == TOKV_IDENTI &&
      !std::get<TOKV_IDENTI>(my_token).has_escape) {
    switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
    case S_ATOM_int:
      return DATATYPE_I32;
    case S_ATOM_string:
      return DATATYPE_STR;
    }
  }
  throw PARSE_ERROR{INVALID_TYPE_ANNOTATION{}};
}

STATEMENT Parser::parse_function_decl(EXPRESSION identifier) {
  DECL_FUNCTION declaration{identifier};
  expect_punct('(');
  tokenize();
  while (my_token != TOKEN{U')'}) {
    if (my_token.index() != TOKV_IDENTI)
      throw PARSE_ERROR{MISSING_FORMAL_PARAMETER{}};
    std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
    if (is_reserved(atom_sh))
      throw PARSE_ERROR{UNEXPECTED_RESERVED_WORD{}};
    declaration.arguments.push_back(atom_sh);
    tokenize();
    if (my_token == TOKEN{U')'})
      break;
    expect_punct(',');
    tokenize();
  }
  tokenize();
  declaration.return_type = parse_type_annotation();
  tokenize();
  expect_punct('{');
  tokenize();
  while (my_token != TOKEN{U'}'})
    declaration.subprogram.push_back(parse_statement());
  auto ret_stmt = stmt_vec.insert(stmt_vec.end(), std::move(declaration));
  return STMT_PTR{std::distance(stmt_vec.begin(), ret_stmt)};
}

STATEMENT Parser::parse_import() {
  DECL_IMPORT declaration{};
  auto specifier_it = specifier_vec.emplace(specifier_vec.end());
  declaration.specifiers = std::distance(specifier_vec.begin(), specifier_it);
  expect_punct('{');
  tokenize();
  while (my_token != TOKEN{U'}'}) {
    if (my_token.index() != TOKV_IDENTI)
      throw PARSE_ERROR{MISSING_IDENTIFIER{}};
    specifier_it->push_back(std::get<TOKV_IDENTI>(my_token));
    tokenize();
    if (my_token != TOKEN{U','})
      break;
    tokenize();
  }
  expect_punct('}');
  tokenize();
  if (my_token != TOKEN{TOK_IDENTI{false, S_ATOM_from}})
    throw PARSE_ERROR{MISSING_FROM_CLAUSE{}};
  tokenize();
  if (my_token.index() != TOKV_STRING)
    throw PARSE_ERROR{MISSING_STRING_LITERAL{}};
  declaration.source = std::get<TOKV_STRING>(my_token);
  tokenize();
  expect_statement_end();
  return declaration;
}

STATEMENT Parser::parse_stmt_expression() {
  EXPRESSION expr{parse_assign_expr()};
  expect_statement_end();
  return expr;
}

STATEMENT Parser::parse_punct_statement() {
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '{': {
    STMT_BLOCK statement{};
    tokenize();
    while (my_token != TOKEN{U'}'})
      statement.subprogram.push_back(parse_statement());
    tokenize();
    auto ret_stmt = stmt_vec.insert(stmt_vec.end(), std::move(statement));
    return STMT_PTR{std::distance(stmt_vec.begin(), ret_stmt)};
  }
  default:
    return parse_stmt_expression();
  }
}

STATEMENT Parser::parse_ident_statement() {
  switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var: {
    STATEMENT ret_stmt{parse_variable_decl()};
    expect_statement_end();
    return ret_stmt;
  }
  case S_ATOM_function: {
    tokenize();
    if (my_token.index() != TOKV_IDENTI)
      throw PARSE_ERROR{MISSING_FUNCTION_NAME{}};
    TOK_IDENTI identifier{std::get<TOKV_IDENTI>(my_token)};
    tokenize();
    STATEMENT ret_stmt{parse_function_decl(identifier)};
    tokenize();
    return ret_stmt;
  }
  case S_ATOM_return: {
    tokenize();
    STMT_RETURN ret_stmt{parse_assign_expr()};
    expect_statement_end();
    return ret_stmt;
  }
  case S_ATOM_import:
    tokenize();
    return parse_import();
  case S_ATOM_if: {
    tokenize();
    auto ret_stmt = stmt_vec.insert(
        stmt_vec.end(), STMT_IF{parse_paren_expr(), parse_statement()});
    return STMT_PTR{std::distance(stmt_vec.begin(), ret_stmt)};
  }
  default:
    return parse_stmt_expression();
  }
}

STATEMENT Parser::parse_statement() {
  switch (my_token.index()) {
  case TOKV_IDENTI:
    return parse_ident_statement();
  case TOKV_PUNCT:
    return parse_punct_statement();
  default:
    return parse_stmt_expression();
  }
}

void Parser::parse() {
  skip_shebang();
  tokenize();
  while (1) {
    if (my_token.index() == TOKV_EOF)
      return;
    program.push_back(parse_statement());
  }
}

std::size_t get_iatom(DECL_FUNCTION &decl) {
  switch (decl.identifier.index()) {
  case EXPRV_STRING:
    return std::get<EXPRV_STRING>(decl.identifier).atom_sh;
  case EXPRV_IDENTI:
    return std::get<EXPRV_IDENTI>(decl.identifier).atom_sh;
  default:
    throw COMPILE_ERROR{COMPILE_ERROR::MESSAGE::UNSUPPORTED};
  }
}

void Language::operator()(std::int64_t num) {
  scope_stack.top().command_vec.push_back(I64_PUSH{num});
  regfile_type.push_back(DATATYPE_I64);
}

void Language::operator()(EXPR_NUMBER expr) { expr.alt.visit(*this); }

void Language::operator()(TOK_IDENTI identifier) {
  auto local_it{scope_stack.top().local_vec.begin()};
  std::size_t offset{};
  while (1) {
    if (local_it == scope_stack.top().local_vec.end())
      throw COMPILE_ERROR{COMPILE_ERROR::MESSAGE::VOID_IDENTIF};
    if (local_it->identifier == identifier.atom_sh)
      break;
    ++local_it;
    ++offset;
  }
  switch (local_it->datatype) {
  case DATATYPE_I32:
    scope_stack.top().command_vec.push_back(LOC_LOAD{offset});
    regfile_type.push_back(DATATYPE_I32);
    return;
  default:
    throw COMPILE_ERROR{COMPILE_ERROR::MESSAGE::UNSUPPORTED};
  }
}

void Language::operator()(TOK_STRING token_str) {
  if (not static_umap.contains(token_str.atom_sh)) {
    std::string_view str_view{token_str.atom_sh >= (1 << 15)
                                  ? atom_deq[token_str.atom_sh >> 16]
                                  : S_ATOM_ARR[token_str.atom_sh]};
    std::size_t offset{machine.static_pool.size()};
    std::size_t length{str_view.size() >> 3};
    length += (str_view.size() & 0b111) > 0;
    machine.static_pool.resize(offset + length);
    std::memcpy(machine.static_pool.data() + offset, str_view.data(),
                str_view.size());
    static_umap[token_str.atom_sh] = STATIC_ENTRY{offset, length};
  }
}

void Language::operator()(EXPR_BINARY &expr) {
  expr.left.visit(*this);
  expr.right.visit(*this);
  bool matched{1};
  do {
    if (regfile_type.at(regfile_type.size() - 2) == DATATYPE_I64) {
      if (expr.op == TOKEN{U'+'} &&
          regfile_type.at(regfile_type.size() - 1) == DATATYPE_I64) {
        scope_stack.top().command_vec.push_back(I64_ADD{});
        break;
      }
      if (expr.op == TOKEN{U'+'} &&
          regfile_type.at(regfile_type.size() - 1) == DATATYPE_I32) {
        scope_stack.top().command_vec.push_back(I32_TO_I64{0});
        scope_stack.top().command_vec.push_back(I64_ADD{});
        break;
      }
      if (expr.op == TOKEN{U'-'} &&
          regfile_type.at(regfile_type.size() - 1) == DATATYPE_I32) {
        scope_stack.top().command_vec.push_back(I32_TO_I64{0});
        scope_stack.top().command_vec.push_back(I64_SUB{});
        break;
      }
    }
    if (regfile_type.at(regfile_type.size() - 2) == DATATYPE_I32) {
      if (expr.op == TOKEN{U'+'} &&
          regfile_type.at(regfile_type.size() - 1) == DATATYPE_I64) {
        scope_stack.top().command_vec.push_back(I32_TO_I64{1});
        scope_stack.top().command_vec.push_back(I64_ADD{});
        break;
      }
      if (expr.op == TOKEN{U'-'} &&
          regfile_type.at(regfile_type.size() - 1) == DATATYPE_I64) {
        scope_stack.top().command_vec.push_back(I32_TO_I64{1});
        scope_stack.top().command_vec.push_back(I64_SUB{});
        break;
      }
    }
    matched = 0;
  } while (0);
  if (matched) {
    regfile_type.pop_back();
    return;
  }
  throw COMPILE_ERROR{COMPILE_ERROR::MESSAGE::UNSUPPORTED};
}

void Language::operator()(EXPR_PTR expr_ptr) {
  expr_vec[expr_ptr.expr_idx].visit(*this);
}

MACHINE_CMD Language::make_cast(bool is_implicit, std::uint8_t adv,
                                std::size_t from, std::size_t to) {
  std::optional<MACHINE_CMD> cmd{};
  if (from == DATATYPE_I64 && to == DATATYPE_I32)
    cmd = I64_TO_I32{};
  else if (from == DATATYPE_U64 && to == DATATYPE_I32)
    cmd = U64_TO_I32{};
  else if (from == DATATYPE_I32 && to == DATATYPE_I64)
    cmd = I32_TO_I64{adv};
  if (cmd) {
    regfile_type.back() = to;
    return *cmd;
  }
  throw COMPILE_ERROR{COMPILE_ERROR::MESSAGE::TYPE_MISMATCH};
}

void Language::operator()(STMT_RETURN ret_stmt) {
  ret_stmt.argument.visit(*this);
  if (regfile_type.back() != scope_stack.top().return_type) {
    MACHINE_CMD cast_cmd{
        make_cast(true, 0, regfile_type.back(), scope_stack.top().return_type)};
    scope_stack.top().command_vec.push_back(cast_cmd);
  }
}

void Language::operator()(DECL_VARIABLE &decl) {
  decl.initializer.visit(*this);
  if (regfile_type.back() != decl.datatype) {
    MACHINE_CMD cast_cmd{
        make_cast(true, 0, regfile_type.back(), decl.datatype)};
    scope_stack.top().command_vec.push_back(cast_cmd);
  }
  scope_stack.top().command_vec.push_back(LOC_APPEND{});
  scope_stack.top().local_vec.push_back(
      {decl.datatype, decl.identifier.atom_sh});
  regfile_type.pop_back();
}

void Language::operator()(DECL_FUNCTION &decl) {
  std::size_t atom_sh{get_iatom(decl)};
  if (is_reserved(atom_sh))
    throw COMPILE_ERROR{COMPILE_ERROR::MESSAGE::RESERVED_WORD};
  scope_stack.push(FUNCTION_IR{.return_type = decl.return_type});
  std::string_view func_name{atom_deq[atom_sh >> 16]};
  for (STATEMENT &func_stmt : decl.subprogram)
    func_stmt.visit(*this);
  std::size_t func_idx{machine.function_vec.size()};
  machine.funcname_umap.insert(std::make_pair(func_name, func_idx));
  machine.function_vec.emplace_back(std::move(scope_stack.top().command_vec));
  scope_stack.pop();
}

void Language::operator()(STMT_PTR stmt_ptr) {
  stmt_vec[stmt_ptr.stmt_idx].visit(*this);
}

void Language::compile() {
  for (STATEMENT &statement : program)
    statement.visit(*this);
}
} // namespace Manadrain
