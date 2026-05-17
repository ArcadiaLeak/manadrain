#include <algorithm>
#include <cassert>
#include <functional>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, RESERVED> reserved_pool{
    {"const", RESERVED::W_CONST},       {"let", RESERVED::W_LET},
    {"var", RESERVED::W_VAR},           {"class", RESERVED::W_CLASS},
    {"function", RESERVED::W_FUNCTION}, {"return", RESERVED::W_RETURN},
    {"import", RESERVED::W_IMPORT},     {"export", RESERVED::W_EXPORT},
    {"from", RESERVED::W_FROM},         {"as", RESERVED::W_AS},
    {"default", RESERVED::W_DEFAULT},   {"undefined", RESERVED::W_UNDEFINED},
    {"null", RESERVED::W_NULL},         {"true", RESERVED::W_TRUE},
    {"false", RESERVED::W_FALSE},       {"if", RESERVED::W_IF},
    {"else", RESERVED::W_ELSE},         {"while", RESERVED::W_WHILE},
    {"for", RESERVED::W_FOR},           {"do", RESERVED::W_DO},
    {"break", RESERVED::W_BREAK},       {"continue", RESERVED::W_CONTINUE},
    {"switch", RESERVED::W_SWITCH}};

std::optional<char32_t> Script::forward() {
  if (position >= text_source.size())
    backtrace.emplace();
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_source.data() + position,
                           text_source.size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push(ch);
  }
  return backtrace.top();
}

std::generator<std::optional<char32_t>> Script::traverse_text() {
  while (1)
    co_yield forward();
}

std::generator<char> traverse_ucs4(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int advance{u8_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  for (int i = 0; i < advance; ++i)
    co_yield buffer[i];
}

void Script::backward() {
  std::optional behind{backtrace.top()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop();
}

void Script::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

TOKEN Script::tokenize_word(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_ucs4(leading)};
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto xid_continue_view =
      traverse_text() | std::views::take_while(does_exist) | std::views::join |
      std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_ucs4) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto iter_reserved = reserved_pool.find(identifier_str);
  if (iter_reserved != reserved_pool.end())
    return iter_reserved->second;
  auto [iter_atlas, did_insert] =
      atom_atlas.insert({identifier_str, atom_pool.size()});
  if (did_insert)
    atom_pool.push_back(std::move(identifier_str));
  return IDENTIFIER{iter_atlas->second};
}

TOKEN Script::tokenize_string_literal(char32_t separator) {
  std::string literal_str{};
  auto match_nullopt = [](auto an_optional) { return an_optional.has_value(); };
  auto match_literal_end = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view = traverse_text() | std::views::take_while(match_nullopt) |
                      std::views::join |
                      std::views::take_while(match_literal_end);
  for (char32_t leading : literal_view) {
    if (leading == '\r' && forward() != '\n')
      backward();
    if (leading == '\r')
      leading = '\n';
    literal_str.append_range(traverse_ucs4(leading));
  }
  auto [iter_atlas, did_insert] =
      atom_atlas.insert({literal_str, atom_pool.size()});
  if (did_insert)
    atom_pool.push_back(std::move(literal_str));
  return STRING_LITERAL{iter_atlas->second};
}

TOKEN Script::tokenize_numeric_literal(char32_t leading) {
  std::string numeric_str{std::from_range, traverse_ucs4(leading)};
  auto match_nullopt = [](auto an_optional) { return an_optional.has_value(); };
  auto match_digit = [](char32_t code_point) {
    return std::isdigit(code_point);
  };
  numeric_str.append_range(
      traverse_text() | std::views::take_while(match_nullopt) |
      std::views::join | std::views::take_while(match_digit) |
      std::views::transform(traverse_ucs4) | std::views::join);
  backward();
  std::int64_t num_literal{};
  std::from_chars(numeric_str.data(), numeric_str.data() + numeric_str.size(),
                  num_literal);
  return NUMERIC_LITERAL{num_literal};
}

static const std::array legal_punct = std::to_array<char32_t>(
    {'(', ')', '*', '+', '-', '.', '/', ':', ';', '=', '{', '}'});

TOKEN Script::tokenize() {
  std::stack<std::optional<char32_t>, std::vector<std::optional<char32_t>>>
      local_backtrace{};
  local_backtrace.swap(backtrace);
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  for (char32_t leading : traverse_text() | std::views::take_while(does_exist) |
                              std::views::join) {
    TOKEN token_ret{};
    if (uc_is_property_white_space(leading))
      continue;
    else if (std::ranges::any_of(
                 std::to_array({'`', '\'', '"'}),
                 [leading](char quote) { return leading == quote; }))
      token_ret = tokenize_string_literal(leading);
    else if (uc_is_property_xid_start(leading) || leading == '_')
      token_ret = tokenize_word(leading);
    else if (std::ranges::binary_search(legal_punct, leading))
      token_ret = leading;
    else if (std::isdigit(leading))
      token_ret = tokenize_numeric_literal(leading);
    else
      throw ScriptError{UNEXPECTED_TOKEN{}};
    return token_ret;
  }
  return std::monostate{};
}

std::generator<TOKEN> Script::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

void assert_punct(TOKEN token, char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw ScriptError{MISSING_PUNCT{must_be}};
}

void Script::parse_text() {
  for (TOKEN token : traverse_tokens()) {
    if (std::holds_alternative<std::monostate>(token))
      break;
    STATEMENT stmt{parse_statement(token)};
    script_body.push_back(std::move(stmt));
  }
}

STATEMENT Script::parse_statement(TOKEN leading) {
  return leading.visit([this](auto t) -> STATEMENT {
    if constexpr (std::is_same_v<decltype(t), RESERVED>) {
      if (t == RESERVED::W_FUNCTION)
        return parse_function_decl();
      if (t == RESERVED::W_LET)
        return parse_variable_decl();
      if (t == RESERVED::W_RETURN)
        return parse_stmt_return();
    }
    throw ScriptError{UNEXPECTED_TOKEN{}};
  });
}

STATEMENT Script::parse_stmt_return() {
  STMT_RETURN statement{parse_expression()};
  assert_punct(tokenize(), ';');
  return statement;
}

STATEMENT Script::parse_function_decl() {
  auto extract_name = [](auto token) -> std::size_t {
    if constexpr (std::is_same_v<decltype(token), IDENTIFIER>)
      return token.pool_idx;
    throw ScriptError{MISSING_FUNCTION_NAME{}};
  };
  std::size_t function_name{std::visit(extract_name, tokenize())};
  assert_punct(tokenize(), '(');
  assert_punct(tokenize(), ')');
  assert_punct(tokenize(), '{');
  auto match_function_end = [&](TOKEN token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return alter_ptr && *alter_ptr == '}';
  };
  std::vector<STATEMENT> function_body{};
  for (TOKEN token :
       traverse_tokens() | std::views::take_while(match_function_end)) {
    if (std::holds_alternative<std::monostate>(token))
      throw ScriptError{MISSING_PUNCT{'}'}};
    function_body.push_back(parse_statement(token));
  }
  func_pool.push_back(FUNCTION_DECL{function_name, std::move(function_body)});
  return FUNCTION_IDX{func_pool.size() - 1};
}

STATEMENT Script::parse_variable_decl() {
  auto extract_name = [](auto token) -> std::size_t {
    if constexpr (std::is_same_v<decltype(token), IDENTIFIER>)
      return token.pool_idx;
    throw ScriptError{MISSING_VARIABLE_NAME{}};
  };
  VARIABLE_DECL variable_decl{std::visit(extract_name, tokenize())};
  assert_punct(tokenize(), '=');
  variable_decl.initializer = parse_expression();
  assert_punct(tokenize(), ';');
  return variable_decl;
}

EXPRESSION Script::parse_expression() { return parse_additive_expr(); }

EXPRESSION Script::parse_primary_expr() {
  return tokenize().visit([](auto t) -> EXPRESSION {
    if constexpr (std::is_same_v<decltype(t), STRING_LITERAL> ||
                  std::is_same_v<decltype(t), NUMERIC_LITERAL> ||
                  std::is_same_v<decltype(t), IDENTIFIER>)
      return t;
    throw ScriptError{UNSUPPORTED{}};
  });
}

EXPRESSION Script::parse_postfix_expr() {
  using POSTFIX_REDUCER =
      std::optional<std::copyable_function<EXPRESSION(EXPRESSION) const>>;
  auto match_reducer = [this](auto t) -> POSTFIX_REDUCER {
    if constexpr (std::is_same_v<decltype(t), char32_t>) {
      if (t == '.')
        return [this](EXPRESSION expr) { return parse_member_expr(expr); };
    }
    return std::nullopt;
  };
  auto postfix_fold = [](EXPRESSION postfix_expr, auto postfix_reducer) {
    return postfix_reducer(postfix_expr);
  };
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto postfix_reducers =
      traverse_tokens() |
      std::views::transform([&](TOKEN t) { return t.visit(match_reducer); }) |
      std::views::take_while(does_exist) | std::views::join;
  EXPRESSION postfix_expr{std::ranges::fold_left(
      postfix_reducers, parse_primary_expr(), postfix_fold)};
  backward(backtrace.size());
  return postfix_expr;
}

EXPRESSION Script::parse_additive_expr() {
  EXPRESSION expr_left{parse_postfix_expr()};
  auto match_binary = [](auto t) -> std::optional<char32_t> {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == '+' || t == '-')
        return t;
    return std::nullopt;
  };
  for (char32_t binary_op : tokenize().visit(match_binary)) {
    expr_pool.push_back(
        EXPR_BINARY{expr_left, parse_postfix_expr(), binary_op});
    return EXPR_IDX{expr_pool.size() - 1};
  }
  backward(backtrace.size());
  return expr_left;
}

EXPRESSION Script::parse_member_expr(EXPRESSION obj_expr) {
  auto property = tokenize().visit([](auto t) -> IDENTIFIER {
    if constexpr (std::is_same_v<decltype(t), IDENTIFIER>)
      return t;
    throw ScriptError{MISSING_FIELD_NAME{}};
  });
  expr_pool.push_back(EXPR_MEMBER{obj_expr, property});
  return EXPR_IDX{expr_pool.size() - 1};
}
} // namespace Manadrain
