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

std::optional<char32_t> Parser::forward() {
  if (position >= text_size) {
    backtrace.push_back(-1);
    return std::nullopt;
  } else {
    ucs4_t ch;
    int advance{
        u8_mbtoucr(&ch, text_buffer.get() + position, text_size - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push_back(std::bit_cast<std::int32_t>(ch));
    return ch;
  }
}

std::generator<std::optional<char32_t>> Parser::traverse_text() {
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

void Parser::backward() {
  std::optional<char32_t> behind{};
  if (backtrace.back() > -1)
    behind = std::bit_cast<char32_t>(backtrace.back());
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop_back();
}

void Parser::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

std::optional<TOKEN> Parser::revoked_pull() {
  if (token_revoked.empty())
    return std::nullopt;
  token_history.splice(token_history.begin(), token_revoked,
                       token_revoked.begin());
  return token_history.front();
}

void Parser::history_push(TOKEN token) { token_history.push_front(token); }
void Parser::history_pull() {
  token_revoked.splice(token_revoked.begin(), token_history,
                       token_history.begin());
}

TOKEN Parser::tokenize_word(char32_t leading) {
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
      script.atom_atlas.insert({identifier_str, script.atom_pool.size()});
  if (did_insert)
    script.atom_pool.push_back(std::move(identifier_str));
  return IDENTIFIER{iter_atlas->second};
}

TOKEN Parser::tokenize_string_literal(char32_t separator) {
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
      script.atom_atlas.insert({literal_str, script.atom_pool.size()});
  if (did_insert)
    script.atom_pool.push_back(std::move(literal_str));
  return STRING_HANDLE{iter_atlas->second};
}

TOKEN Parser::tokenize_numeric_literal(char32_t leading) {
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

TOKEN Parser::tokenize() {
  std::optional revoked_opt{revoked_pull()};
  for (TOKEN revoked_token : revoked_opt)
    return revoked_token;
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
    history_push(token_ret);
    return token_ret;
  }
  history_push(std::monostate{});
  return std::monostate{};
}

std::generator<TOKEN> Parser::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

void assert_punct(TOKEN token, char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw ScriptError{MISSING_PUNCT{must_be}};
}

void Parser::parse_text() {
  for (TOKEN token : traverse_tokens()) {
    if (std::holds_alternative<std::monostate>(token))
      break;
    parse_statement(script.script_scope, script.script_body, token);
  }
}

void Parser::parse_statement(
    std::flat_map<std::size_t, std::optional<DYNAMIC>> &block_scope,
    std::vector<STATEMENT> &block_body, TOKEN leading) {
  auto match_reserved = [](auto t) {
    if constexpr (std::is_same_v<decltype(t), RESERVED>)
      return t;
    return RESERVED::MONOSTATE;
  };
  RESERVED word{leading.visit(match_reserved)};
  STATEMENT statement;
  if (word == RESERVED::W_FUNCTION) {
    FUNCTION_DECL declaration{parse_function_decl()};
    if (block_scope.contains(declaration.function_name))
      throw ScriptError{INVALID_DECLARATION{}};
    block_scope[declaration.function_name] = declaration.function_handle;
  } else if (word == RESERVED::W_LET) {
    VARIABLE_DECL declaration{parse_variable_decl()};
    if (block_scope.contains(declaration.variable_name))
      throw ScriptError{INVALID_DECLARATION{}};
    block_scope[declaration.variable_name] = std::nullopt;
    block_body.push_back(declaration);
  } else if (word == RESERVED::W_RETURN) {
    block_body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  } else {
    history_pull();
    block_body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  }
}

FUNCTION_DECL Parser::parse_function_decl() {
  auto extract_name = [](auto token) -> std::size_t {
    if constexpr (std::is_same_v<decltype(token), IDENTIFIER>)
      return token.pool_idx;
    throw ScriptError{MISSING_FUNCTION_NAME{}};
  };
  std::size_t function_name{std::visit(extract_name, tokenize())};
  for (char function_punct : std::to_array({'(', ')', '{'}))
    assert_punct(tokenize(), function_punct);
  auto match_function_end = [&](TOKEN token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return !alter_ptr || *alter_ptr != '}';
  };
  std::flat_map<std::size_t, std::optional<DYNAMIC>> function_scope{};
  std::vector<STATEMENT> function_body{};
  for (TOKEN token :
       traverse_tokens() | std::views::take_while(match_function_end)) {
    if (std::holds_alternative<std::monostate>(token))
      throw ScriptError{MISSING_PUNCT{'}'}};
    parse_statement(function_scope, function_body, token);
  }
  std::size_t function_handle{script.function_pool.size()};
  script.function_pool.push_back(VanillaFunction{
      function_name, std::move(function_scope), std::move(function_body)});
  return FUNCTION_DECL{function_name, function_handle};
}

VARIABLE_DECL Parser::parse_variable_decl() {
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

EXPRESSION Parser::parse_expression() { return parse_additive_expr(); }

EXPRESSION Parser::parse_primary_expr() {
  return tokenize().visit([](auto t) -> EXPRESSION {
    if constexpr (std::is_same_v<decltype(t), STRING_HANDLE> ||
                  std::is_same_v<decltype(t), NUMERIC_LITERAL> ||
                  std::is_same_v<decltype(t), IDENTIFIER>)
      return t;
    throw ScriptError{UNEXPECTED_TOKEN{}};
  });
}

EXPRESSION Parser::parse_postfix_expr() {
  using POSTFIX_REDUCER =
      std::optional<std::copyable_function<EXPRESSION(EXPRESSION) const>>;
  auto match_reducer = [this](auto t) -> POSTFIX_REDUCER {
    if constexpr (std::is_same_v<decltype(t), char32_t>) {
      if (t == '.')
        return [this](EXPRESSION expr) { return parse_member_expr(expr); };
      if (t == '(')
        return [this](EXPRESSION expr) { return parse_call_expr(expr); };
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
  history_pull();
  return postfix_expr;
}

EXPRESSION Parser::parse_additive_expr() {
  EXPRESSION expr_left{parse_postfix_expr()};
  auto match_binary = [](auto t) -> std::optional<char32_t> {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == '+' || t == '-')
        return t;
    return std::nullopt;
  };
  for (char32_t binary_op : tokenize().visit(match_binary)) {
    script.expr_pool.push_back(
        EXPR_BINARY{expr_left, parse_postfix_expr(), binary_op});
    return EXPR_HANDLE{script.expr_pool.size() - 1};
  }
  history_pull();
  return expr_left;
}

EXPRESSION Parser::parse_member_expr(EXPRESSION obj_expr) {
  auto property = tokenize().visit([](auto t) -> IDENTIFIER {
    if constexpr (std::is_same_v<decltype(t), IDENTIFIER>)
      return t;
    throw ScriptError{MISSING_FIELD_NAME{}};
  });
  script.expr_pool.push_back(EXPR_MEMBER{obj_expr, property.pool_idx});
  return EXPR_HANDLE{script.expr_pool.size() - 1};
}

EXPRESSION Parser::parse_call_expr(EXPRESSION callee_expr) {
  auto match_rparen = [](auto t) -> bool {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == ')')
        return 1;
    return 0;
  };
  EXPR_CALL expr_call{callee_expr};
  while (1) {
    if (tokenize().visit(match_rparen))
      break;
    history_pull();
    expr_call.param_vec.push_back(parse_expression());
    if (tokenize().visit(match_rparen))
      break;
    history_pull();
    assert_punct(tokenize(), ',');
  }
  script.expr_pool.push_back(std::move(expr_call));
  return EXPR_HANDLE{script.expr_pool.size() - 1};
}

DYNAMIC VanillaFunction::operator()(std::vector<DYNAMIC> parameter_vec,
                                    DYNAMIC context, const Script &script) {
  return std::monostate{};
}

FUNCTION_HANDLE Script::insert(FUNCTION function) {
  FUNCTION_HANDLE function_handle{function_pool.size()};
  function_pool.push_back(std::move(function));
  return function_handle;
}

OBJECT_HANDLE Script::insert(OBJECT object) {
  OBJECT_HANDLE object_handle{object_pool.size()};
  object_pool.push_back(std::move(object));
  return object_handle;
}

void Script::attach_atom(std::size_t &atom_hdl, std::string atom_str) {
  auto [iter_atlas, did_insert] =
      atom_atlas.insert({atom_str, atom_pool.size()});
  if (did_insert)
    atom_pool.push_back(std::move(atom_str));
  atom_hdl = iter_atlas->second;
}

void Script::attach_global(std::size_t atom_handle, DYNAMIC dynamic) {
  script_scope[atom_handle] = dynamic;
}

DYNAMIC Script::reduce(EXPR_CALL &expr_call) {
  EXPR_HANDLE expr_handle{std::get<EXPR_HANDLE>(expr_call.callee)};
  EXPR_MEMBER &expr_member{
      std::get<EXPR_MEMBER>(expr_pool[expr_handle.pool_idx])};
  DYNAMIC context_call{reduce(expr_member.object)};
  OBJECT_HANDLE obj_handle{std::get<OBJECT_HANDLE>(context_call)};
  OBJECT &context_obj{object_pool[obj_handle.pool_idx.value()]};
  DYNAMIC dyn_method{context_obj.at(expr_member.property)};
  FUNCTION_HANDLE method_handle{std::get<FUNCTION_HANDLE>(dyn_method)};
  FUNCTION &method_function{function_pool[method_handle.pool_idx]};
  method_function({}, obj_handle, *this);
  return std::monostate{};
}

DYNAMIC Script::reduce(EXPRESSION expression) {
  auto reduce_node = [&](auto &node) -> DYNAMIC {
    if constexpr (std::is_same_v<decltype(node), EXPR_CALL &>)
      return reduce(node);
    return std::monostate{};
  };
  auto reduce_expr = [&](auto expr) -> DYNAMIC {
    if constexpr (std::is_same_v<decltype(expr), EXPR_HANDLE>)
      return std::visit(reduce_node, expr_pool[expr.pool_idx]);
    if constexpr (std::is_same_v<decltype(expr), IDENTIFIER>) {
      std::string &atom_view{atom_pool[expr.pool_idx]};
      std::size_t scope_handle{atom_atlas.at(atom_view)};
      return script_scope.at(scope_handle).value();
    }
    return std::monostate{};
  };
  return expression.visit(reduce_expr);
}

void Script::execute(STATEMENT statement) {
  auto execute_stmt = [this](auto stmt) -> void {
    if constexpr (std::is_same_v<decltype(stmt), EXPRESSION>)
      reduce(stmt);
    assert(0);
  };
  statement.visit(execute_stmt);
}

void Script::execute() {
  for (STATEMENT statement : script_body)
    execute(statement);
}

} // namespace Manadrain
