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

std::generator<std::expected<char32_t, PARSE_ERR>>
TokAtom::traverse_string(char32_t separator) {
  std::optional<PARSE_ERR> err_opt{};
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

std::expected<TOKEN, PARSE_ERR> TokAtom::tokenize_string(char32_t separator) {
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

std::expected<TOKEN, PARSE_ERR> TokAtom::tokenize_identif(char32_t leading) {
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

std::expected<TOKEN, PARSE_ERR> Tokenizer::tokenize() {
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

std::expected<TOKEN, PARSE_ERR> TokNumber::tokenize(char32_t leading) {
  if (reached_end())
    return std::int64_t{leading - '0'};
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
          bigint_vec.end(), mpz_class{std::get<WHOLE>(num_repr).repr_s, radix});
      std::size_t bigint_idx = std::distance(bigint_vec.begin(), bigint_it);
      return TOK_BIGINT{bigint_idx};
    } else if (std::holds_alternative<WHOLE>(num_repr)) {
      std::int64_t result{};
      std::string repr_s = std::move(std::get<WHOLE>(num_repr).repr_s);
      auto status = std::from_chars(
          repr_s.data(), repr_s.data() + repr_s.size(), result, radix);
      if (status.ec != std::errc{})
        return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
      return result;
    } else {
      double result{};
      std::string repr_s = num_repr.visit([](auto n) { return n.collapse(); });
      auto status =
          std::from_chars(repr_s.data(), repr_s.data() + repr_s.size(), result);
      if (status.ec == std::errc::result_out_of_range)
        return std::numeric_limits<double>::infinity();
      else if (status.ec != std::errc{})
        return std::unexpected{INVALID_ERR::NUMBER_LITERAL};
      return result;
    }
  } while (0);
}

std::expected<void, PARSE_ERR> Parser::tokenize() {
  std::expected tokenize_ok = Tokenizer::tokenize();
  if (not tokenize_ok)
    return std::unexpected{tokenize_ok.error()};
  my_token = *tokenize_ok;
  return {};
}

expected_task<STATEMENT, PARSE_ERR> Parser::parse_variable_decl() {
  DECL_VARIABLE declaration{};

  std::size_t atom_sh{std::get<TOKV_IDENTI>(my_token).atom_sh};
  assert(atom_sh == S_ATOM_const || atom_sh == S_ATOM_let ||
         atom_sh == S_ATOM_var);
  declaration.kind = atom_sh;
  co_await tokenize();

  if (my_token.index() != TOKV_IDENTI)
    co_return std::unexpected{REQUIRED_ERR::VARIABLE_NAME};
  declaration.identifier = std::get<TOK_IDENTI>(my_token);
  co_await tokenize();

  if (my_token != TOKEN{U'='})
    co_return declaration;
  co_await tokenize();

  declaration.initializer = co_await parse_assign_expr().ok();
  co_return declaration;
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_additive_expr() {
  EXPRESSION expr_left{co_await parse_postfix_expr().ok()};
  do {
    if (my_token == TOKEN{U'+'})
      break;
    if (my_token == TOKEN{U'-'})
      break;
    co_return expr_left;
  } while (0);
  TOKEN op = my_token;
  co_await tokenize();
  auto ret_expr = expr_vec.insert(
      expr_vec.end(),
      EXPR_BINARY{expr_left, co_await parse_postfix_expr().ok(), op});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_relation_expr() {
  EXPRESSION expr_left{co_await parse_additive_expr().ok()};
  do {
    if (my_token == TOKEN{U'>'})
      break;
    if (my_token == TOKEN{U'<'})
      break;
    co_return expr_left;
  } while (0);
  TOKEN op = my_token;
  co_await tokenize();
  auto ret_expr = expr_vec.insert(
      expr_vec.end(),
      EXPR_BINARY{expr_left, co_await parse_additive_expr().ok(), op});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_equality_expr() {
  EXPRESSION expr_left{co_await parse_relation_expr().ok()};
  do {
    if (my_token == TOKEN{TOK_OPERATOR::DOUBLE_EQUALS})
      break;
    if (my_token == TOKEN{TOK_OPERATOR::TRIPLE_EQUALS})
      break;
    co_return expr_left;
  } while (0);
  TOKEN op = my_token;
  co_await tokenize();
  auto ret_expr = expr_vec.insert(
      expr_vec.end(),
      EXPR_BINARY{expr_left, co_await parse_relation_expr().ok(), op});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_object_literal() {
  co_await tokenize();
  std::vector<std::pair<EXPRESSION, EXPRESSION>> prop_vec{};
  while (my_token != TOKEN{U'}'}) {
    EXPRESSION prop_key{};
    do {
      if (my_token == TOKEN{U'['}) {
        co_await tokenize();
        prop_key = co_await parse_assign_expr().ok();
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
    if (my_token == TOKEN{U':'}) {
      co_await tokenize();
      prop_vec.emplace_back(prop_key, co_await parse_assign_expr().ok());
    } else
      prop_vec.emplace_back(prop_key, std::monostate{});
    if (my_token != TOKEN{U','})
      break;
    co_await tokenize();
  }
  co_await expect_punct('}');
  co_await tokenize();
  auto ret_expr =
      expr_vec.insert(expr_vec.end(), EXPR_OBJECT{std::move(prop_vec)});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

std::expected<EXPRESSION, PARSE_ERR> Parser::parse_primary_expr() {
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
      return parse_object_literal().ok();
    [[fallthrough]];
  default:
    return std::unexpected{UNEXPECT_ERR::THIS_TOKEN};
  }
}

std::expected<void, PARSE_ERR> Parser::expect_punct(char32_t punct) {
  if (my_token == TOKEN{punct})
    return {};
  return std::unexpected{PUNCT_ERR{punct}};
}

expected_task<EXPRESSION, PARSE_ERR>
Parser::parse_call_expr(EXPRESSION callee_expr) {
  std::vector<EXPRESSION> arguments{};
  while (1) {
    co_await tokenize();
    if (my_token == TOKEN{U')'})
      break;
    EXPRESSION arg_expr{co_await parse_assign_expr().ok()};
    if (my_token == TOKEN{U')'})
      break;
    co_await expect_punct(',');
    arguments.push_back(arg_expr);
  }
  auto ret_expr = expr_vec.insert(expr_vec.end(),
                                  EXPR_CALL{callee_expr, std::move(arguments)});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR>
Parser::parse_member_expr(EXPRESSION obj_expr) {
  co_await tokenize();
  if (my_token.index() != TOKV_IDENTI)
    co_return std::unexpected{REQUIRED_ERR::FIELD_NAME};
  auto ret_expr = expr_vec.insert(
      expr_vec.end(), EXPR_MEMBER{obj_expr, std::get<TOK_IDENTI>(my_token)});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR>
Parser::parse_access_expr(EXPRESSION obj_expr) {
  co_await tokenize();
  EXPRESSION prop_expr{co_await parse_assign_expr().ok()};
  co_await expect_punct(']');
  auto ret_expr =
      expr_vec.insert(expr_vec.end(), EXPR_ACCESS{obj_expr, prop_expr});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_assign_expr() {
  EXPRESSION lhs_expr{co_await parse_logical_disjunct().ok()};
  if (my_token != TOKEN{U'='})
    co_return lhs_expr;
  co_await tokenize();
  auto ret_expr = expr_vec.insert(
      expr_vec.end(), EXPR_ASSIGN{lhs_expr, co_await parse_assign_expr().ok()});
  co_return EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_logical_conjunct() {
  EXPRESSION expr_left{co_await parse_equality_expr().ok()};
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_CONJUNCT}) {
    TOKEN op = my_token;
    co_await tokenize();
    auto ret_expr = expr_vec.insert(
        expr_vec.end(),
        EXPR_LOGIC{expr_left, co_await parse_equality_expr().ok(), op});
    expr_left = EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
  }
  co_return expr_left;
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_logical_disjunct() {
  EXPRESSION expr_left{co_await parse_logical_conjunct().ok()};
  while (my_token == TOKEN{TOK_OPERATOR::LOGICAL_DISJUNCT}) {
    TOKEN op = my_token;
    co_await tokenize();
    auto ret_expr = expr_vec.insert(
        expr_vec.end(),
        EXPR_LOGIC{expr_left, co_await parse_logical_conjunct().ok(), op});
    expr_left = EXPR_PTR{std::distance(expr_vec.begin(), ret_expr)};
  }
  co_return expr_left;
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_postfix_expr() {
  EXPRESSION base_expr{co_await parse_primary_expr()};
  while (1) {
    co_await tokenize();
    if (my_token.index() != TOKV_PUNCT)
      co_return base_expr;
    switch (std::get<TOKV_PUNCT>(my_token)) {
    case '(':
      base_expr = co_await parse_call_expr(base_expr).ok();
      continue;
    case '.':
      base_expr = co_await parse_member_expr(base_expr).ok();
      continue;
    case '[':
      base_expr = co_await parse_access_expr(base_expr).ok();
      continue;
    default:
      co_return base_expr;
    }
  }
}

expected_task<EXPRESSION, PARSE_ERR> Parser::parse_paren_expr() {
  co_await expect_punct('(');
  co_await tokenize();
  EXPRESSION assign_expr{co_await parse_assign_expr().ok()};
  co_await expect_punct(')');
  co_await tokenize();
  co_return assign_expr;
}

expected_task<void, PARSE_ERR> Parser::expect_statement_end() {
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

expected_task<STATEMENT, PARSE_ERR>
Parser::parse_function_decl(EXPRESSION identifier) {
  DECL_FUNCTION declaration{identifier};
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
  switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
  case S_ATOM_int:
    declaration.return_type = MACHINE_DATATYPE::I32T;
    break;
  default:
    co_return std::unexpected{INVALID_ERR::RETURN_TYPE};
  }
  co_await tokenize();
  co_await expect_punct('{');
  co_await tokenize();
  while (my_token != TOKEN{U'}'})
    declaration.subprogram.push_back(co_await parse_statement());
  auto ret_stmt = stmt_vec.insert(stmt_vec.end(), std::move(declaration));
  co_return STMT_PTR{std::distance(stmt_vec.begin(), ret_stmt)};
}

expected_task<STATEMENT, PARSE_ERR> Parser::parse_import() {
  DECL_IMPORT declaration{};
  auto specifier_it = specifier_vec.emplace(specifier_vec.end());
  declaration.specifiers = std::distance(specifier_vec.begin(), specifier_it);
  co_await expect_punct('{');
  co_await tokenize();
  while (my_token != TOKEN{U'}'}) {
    if (my_token.index() != TOKV_IDENTI)
      co_return std::unexpected{REQUIRED_ERR::IDENTIFIER};
    specifier_it->push_back(std::get<TOKV_IDENTI>(my_token));
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
  co_return declaration;
}

expected_task<STATEMENT, PARSE_ERR> Parser::parse_stmt_expression() {
  EXPRESSION expr{co_await parse_assign_expr().ok()};
  co_await expect_statement_end().ok();
  co_return expr;
}

expected_task<STATEMENT, PARSE_ERR> Parser::parse_punct_statement() {
  switch (std::get<TOKV_PUNCT>(my_token)) {
  case '{': {
    STMT_BLOCK statement{};
    co_await tokenize();
    while (my_token != TOKEN{U'}'})
      statement.subprogram.push_back(co_await parse_statement());
    co_await tokenize();
    auto ret_stmt = stmt_vec.insert(stmt_vec.end(), std::move(statement));
    co_return STMT_PTR{std::distance(stmt_vec.begin(), ret_stmt)};
  }
  default:
    co_return parse_stmt_expression().ok();
  }
}

expected_task<STATEMENT, PARSE_ERR> Parser::parse_ident_statement() {
  switch (std::get<TOKV_IDENTI>(my_token).atom_sh) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var: {
    STATEMENT ret_stmt{co_await parse_variable_decl().ok()};
    co_await expect_statement_end().ok();
    co_return ret_stmt;
  }
  case S_ATOM_function: {
    co_await tokenize();
    if (my_token.index() != TOKV_IDENTI)
      co_return std::unexpected{REQUIRED_ERR::FUNCTION_NAME};
    TOK_IDENTI identifier{std::get<TOKV_IDENTI>(my_token)};
    co_await tokenize();
    STATEMENT ret_stmt{co_await parse_function_decl(identifier).ok()};
    co_await tokenize();
    co_return ret_stmt;
  }
  case S_ATOM_return: {
    co_await tokenize();
    STMT_RETURN ret_stmt{co_await parse_assign_expr().ok()};
    co_await expect_statement_end().ok();
    co_return ret_stmt;
  }
  case S_ATOM_import:
    co_await tokenize();
    co_return parse_import().ok();
  case S_ATOM_if: {
    co_await tokenize();
    auto ret_stmt = stmt_vec.insert(
        stmt_vec.end(),
        STMT_IF{co_await parse_paren_expr().ok(), co_await parse_statement()});
    co_return STMT_PTR{std::distance(stmt_vec.begin(), ret_stmt)};
  }
  default:
    co_return parse_stmt_expression().ok();
  }
}

std::expected<STATEMENT, PARSE_ERR> Parser::parse_statement() {
  switch (my_token.index()) {
  case TOKV_IDENTI:
    return parse_ident_statement().ok();
  case TOKV_PUNCT:
    return parse_punct_statement().ok();
  default:
    return parse_stmt_expression().ok();
  }
}

expected_task<void, PARSE_ERR> Parser::parse() {
  skip_shebang();
  co_await tokenize();
  while (1) {
    if (my_token.index() == TOKV_EOF)
      co_return {};
    program.push_back(co_await parse_statement());
  }
}

std::expected<std::size_t, COMPILE_ERR> get_iatom(DECL_FUNCTION &decl) {
  switch (decl.identifier.index()) {
  case EXPRV_STRING:
    return std::get<EXPRV_STRING>(decl.identifier).atom_sh;
  case EXPRV_IDENTI:
    return std::get<EXPRV_IDENTI>(decl.identifier).atom_sh;
  default:
    return std::unexpected{COMPILE_ERR::UNSUPPORTED};
  }
}

expected_task<void, COMPILE_ERR> Language::operator()(std::int64_t num) {
  scope_stack.top().command_vec.push_back(I64_IMM_LOAD{regfile_idx, num});
  regfile_type[regfile_idx] = MACHINE_DATATYPE::I64T;
  co_return {};
}

expected_task<void, COMPILE_ERR> Language::operator()(EXPR_NUMBER expr) {
  co_return expr.alt.visit(*this).ok();
}

expected_task<void, COMPILE_ERR> Language::operator()(EXPR_BINARY &expr) {
  std::uint8_t lhs_reg{regfile_idx};
  expr.left.visit(*this);
  regfile_idx += static_cast<std::uint8_t>(sizeof(UNIFORM));
  std::uint8_t rhs_reg{regfile_idx};
  expr.right.visit(*this);
  regfile_idx -= static_cast<std::uint8_t>(sizeof(UNIFORM));
  if (regfile_type[lhs_reg] != regfile_type[rhs_reg])
    co_return std::unexpected{COMPILE_ERR::TYPE_MISMATCH};
  std::optional<MACHINE_CMD> cmd{};
  if (expr.op == TOKEN{U'+'} && regfile_type[lhs_reg] == MACHINE_DATATYPE::I64T)
    cmd = I64_ADD{regfile_idx, lhs_reg, rhs_reg};
  if (cmd) {
    scope_stack.top().command_vec.push_back(*cmd);
    co_return {};
  }
  co_return std::unexpected{COMPILE_ERR::UNSUPPORTED};
}

expected_task<void, COMPILE_ERR> Language::operator()(EXPR_PTR expr_ptr) {
  co_return expr_vec[expr_ptr.expr_idx].visit(*this).ok();
}

std::expected<MACHINE_CMD, COMPILE_ERR>
Language::append_cast(bool is_implicit, MACHINE_DATATYPE from,
                      MACHINE_DATATYPE to) {
  std::optional<MACHINE_CMD> cmd{};
  if (from == MACHINE_DATATYPE::I64T && to == MACHINE_DATATYPE::I32T)
    cmd = I64_TO_I32{regfile_idx};
  else if (from == MACHINE_DATATYPE::U64T && to == MACHINE_DATATYPE::I32T)
    cmd = U64_TO_I32{regfile_idx};
  if (cmd) {
    regfile_type[regfile_idx] = to;
    return *cmd;
  }
  return std::unexpected{COMPILE_ERR::TYPE_MISMATCH};
}

expected_task<void, COMPILE_ERR> Language::operator()(STMT_RETURN ret_stmt) {
  co_await ret_stmt.argument.visit(*this).ok();
  if (regfile_type[regfile_idx] == scope_stack.top().return_type)
    co_return {};
  scope_stack.top().command_vec.push_back(co_await append_cast(
      true, regfile_type[regfile_idx], scope_stack.top().return_type));
}

expected_task<void, COMPILE_ERR> Language::operator()(DECL_FUNCTION &decl) {
  std::size_t atom_sh{co_await get_iatom(decl)};
  if (is_reserved(atom_sh))
    co_return std::unexpected{COMPILE_ERR::RESERVED_WORD};
  scope_stack.push(FUNCTION_IR{decl.return_type});
  std::string_view func_name{atom_deq[atom_sh >> 16]};
  for (STATEMENT &func_stmt : decl.subprogram)
    co_await func_stmt.visit(*this).ok();
  std::size_t func_idx{machine.function_vec.size()};
  machine.funcname_umap.insert(std::make_pair(func_name, func_idx));
  machine.function_vec.emplace_back(std::move(scope_stack.top().command_vec));
  scope_stack.pop();
}

expected_task<void, COMPILE_ERR> Language::operator()(STMT_PTR stmt_ptr) {
  co_return stmt_vec[stmt_ptr.stmt_idx].visit(*this).ok();
}

expected_task<void, COMPILE_ERR> Language::compile() {
  for (STATEMENT &statement : program)
    co_await statement.visit(*this).ok();
  co_return {};
}
} // namespace Manadrain
