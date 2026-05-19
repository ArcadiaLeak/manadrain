#include <algorithm>
#include <cassert>
#include <functional>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, ReservedWord> reserved_pool{
    {"const", ReservedWord::W_CONST},
    {"let", ReservedWord::W_LET},
    {"var", ReservedWord::W_VAR},
    {"class", ReservedWord::W_CLASS},
    {"function", ReservedWord::W_FUNCTION},
    {"return", ReservedWord::W_RETURN},
    {"import", ReservedWord::W_IMPORT},
    {"export", ReservedWord::W_EXPORT},
    {"from", ReservedWord::W_FROM},
    {"as", ReservedWord::W_AS},
    {"default", ReservedWord::W_DEFAULT},
    {"undefined", ReservedWord::W_UNDEFINED},
    {"null", ReservedWord::W_NULL},
    {"true", ReservedWord::W_TRUE},
    {"false", ReservedWord::W_FALSE},
    {"if", ReservedWord::W_IF},
    {"else", ReservedWord::W_ELSE},
    {"while", ReservedWord::W_WHILE},
    {"for", ReservedWord::W_FOR},
    {"do", ReservedWord::W_DO},
    {"break", ReservedWord::W_BREAK},
    {"continue", ReservedWord::W_CONTINUE},
    {"switch", ReservedWord::W_SWITCH}};

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

std::optional<Token> Parser::revoked_pull() {
  if (token_revoked.empty())
    return std::nullopt;
  token_history.splice(token_history.begin(), token_revoked,
                       token_revoked.begin());
  return token_history.front();
}

void Parser::history_push(Token token) { token_history.push_front(token); }
void Parser::history_pull() {
  token_revoked.splice(token_revoked.begin(), token_history,
                       token_history.begin());
}

Token Parser::tokenize_word(char32_t leading) {
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
  return Identifier{iter_atlas->second};
}

Token Parser::tokenize_string_literal(char32_t separator) {
  std::string literal_str{};
  auto match_literal_end = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view =
      traverse_text() | std::views::take_while(match_literal_end);
  for (std::optional<char32_t> leading : literal_view) {
    if (leading == '\r' && forward() != '\n')
      backward();
    if (not leading || leading == '\r' || leading == '\n')
      throw ScriptError{UnexpectedStringEnd{}};
    literal_str.append_range(traverse_ucs4(*leading));
  }
  auto [iter_atlas, did_insert] =
      script.atom_atlas.insert({literal_str, script.atom_pool.size()});
  if (did_insert)
    script.atom_pool.push_back(std::move(literal_str));
  return StringHandle{iter_atlas->second};
}

Token Parser::tokenize_numeric_literal(char32_t leading) {
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
  return NumericLiteral{num_literal};
}

static const std::array legal_punct = std::to_array<char32_t>(
    {'(', ')', '*', '+', '-', '.', '/', ':', ';', '=', '{', '}'});

Token Parser::tokenize() {
  std::optional revoked_opt{revoked_pull()};
  for (Token revoked_token : revoked_opt)
    return revoked_token;
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  for (char32_t leading : traverse_text() | std::views::take_while(does_exist) |
                              std::views::join) {
    Token token_ret{};
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
      throw ScriptError{UnexpectedToken{}};
    history_push(token_ret);
    return token_ret;
  }
  history_push(std::monostate{});
  return std::monostate{};
}

std::generator<Token> Parser::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

void assert_punct(Token token, char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw ScriptError{MissingPunctuation{must_be}};
}

void Parser::parse_text() {
  for (Token token : traverse_tokens()) {
    if (std::holds_alternative<std::monostate>(token))
      break;
    parse_statement(script.main_function.function_scope,
                    script.main_function.function_body, token);
  }
}

void Parser::parse_statement(
    std::flat_map<std::size_t, std::optional<Dynamic>> &block_scope,
    std::vector<Statement> &block_body, Token leading) {
  auto match_reserved = [](auto t) {
    if constexpr (std::is_same_v<decltype(t), ReservedWord>)
      return t;
    return ReservedWord::MONOSTATE;
  };
  ReservedWord word{leading.visit(match_reserved)};
  Statement statement;
  if (word == ReservedWord::W_FUNCTION) {
    FunctionDeclaration declaration{parse_function_decl()};
    if (block_scope.contains(declaration.function_name))
      throw ScriptError{InvalidDeclaration{}};
    block_scope[declaration.function_name] = declaration.function_handle;
  } else if (word == ReservedWord::W_LET) {
    VariableDeclaration declaration{parse_variable_decl()};
    if (block_scope.contains(declaration.variable_name))
      throw ScriptError{InvalidDeclaration{}};
    block_scope[declaration.variable_name] = std::nullopt;
    block_body.push_back(declaration);
  } else if (word == ReservedWord::W_RETURN) {
    block_body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  } else {
    history_pull();
    block_body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  }
}

FunctionDeclaration Parser::parse_function_decl() {
  auto extract_name = [](auto token) -> std::size_t {
    if constexpr (std::is_same_v<decltype(token), Identifier>)
      return token.pool_idx;
    throw ScriptError{MissingFunctionName{}};
  };
  std::size_t function_name{std::visit(extract_name, tokenize())};
  for (char function_punct : std::to_array({'(', ')', '{'}))
    assert_punct(tokenize(), function_punct);
  auto match_function_end = [&](Token token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return !alter_ptr || *alter_ptr != '}';
  };
  std::flat_map<std::size_t, std::optional<Dynamic>> function_scope{};
  std::vector<Statement> function_body{};
  for (Token token :
       traverse_tokens() | std::views::take_while(match_function_end)) {
    if (std::holds_alternative<std::monostate>(token))
      throw ScriptError{MissingPunctuation{'}'}};
    parse_statement(function_scope, function_body, token);
  }
  std::size_t function_handle{script.function_pool.size()};
  script.function_pool.push_back(VanillaFunction{
      function_name, std::move(function_scope), std::move(function_body)});
  return FunctionDeclaration{function_name, function_handle};
}

VariableDeclaration Parser::parse_variable_decl() {
  auto extract_name = [](auto token) -> std::size_t {
    if constexpr (std::is_same_v<decltype(token), Identifier>)
      return token.pool_idx;
    throw ScriptError{MissingVariableName{}};
  };
  VariableDeclaration variable_decl{std::visit(extract_name, tokenize())};
  assert_punct(tokenize(), '=');
  variable_decl.initializer = parse_expression();
  assert_punct(tokenize(), ';');
  return variable_decl;
}

Expression Parser::parse_expression() { return parse_additive_expr(); }

Expression Parser::parse_primary_expr() {
  return tokenize().visit([](auto t) -> Expression {
    if constexpr (std::is_same_v<decltype(t), StringHandle> ||
                  std::is_same_v<decltype(t), NumericLiteral> ||
                  std::is_same_v<decltype(t), Identifier>)
      return t;
    throw ScriptError{UnexpectedToken{}};
  });
}

Expression Parser::parse_postfix_expr() {
  using POSTFIX_REDUCER =
      std::optional<std::copyable_function<Expression(Expression) const>>;
  auto match_reducer = [this](auto t) -> POSTFIX_REDUCER {
    if constexpr (std::is_same_v<decltype(t), char32_t>) {
      if (t == '.')
        return [this](Expression expr) { return parse_member_expr(expr); };
      if (t == '(')
        return [this](Expression expr) { return parse_call_expr(expr); };
    }
    return std::nullopt;
  };
  auto postfix_fold = [](Expression postfix_expr, auto postfix_reducer) {
    return postfix_reducer(postfix_expr);
  };
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto postfix_reducers =
      traverse_tokens() |
      std::views::transform([&](Token t) { return t.visit(match_reducer); }) |
      std::views::take_while(does_exist) | std::views::join;
  Expression postfix_expr{std::ranges::fold_left(
      postfix_reducers, parse_primary_expr(), postfix_fold)};
  history_pull();
  return postfix_expr;
}

Expression Parser::parse_additive_expr() {
  Expression expr_left{parse_postfix_expr()};
  auto match_binary = [](auto t) -> std::optional<char32_t> {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == '+' || t == '-')
        return t;
    return std::nullopt;
  };
  for (char32_t binary_op : tokenize().visit(match_binary)) {
    script.expr_pool.push_back(
        BinaryExpression{expr_left, parse_postfix_expr(), binary_op});
    return ExpressionHandle{script.expr_pool.size() - 1};
  }
  history_pull();
  return expr_left;
}

Expression Parser::parse_member_expr(Expression obj_expr) {
  auto property = tokenize().visit([](auto t) -> Identifier {
    if constexpr (std::is_same_v<decltype(t), Identifier>)
      return t;
    throw ScriptError{MissingFieldName{}};
  });
  script.expr_pool.push_back(MemberExpression{obj_expr, property.pool_idx});
  return ExpressionHandle{script.expr_pool.size() - 1};
}

Expression Parser::parse_call_expr(Expression callee_expr) {
  auto match_rparen = [](auto t) -> bool {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == ')')
        return 1;
    return 0;
  };
  std::vector<Expression> arguments{};
  while (1) {
    if (tokenize().visit(match_rparen))
      break;
    history_pull();
    arguments.push_back(parse_expression());
    if (tokenize().visit(match_rparen))
      break;
    history_pull();
    assert_punct(tokenize(), ',');
  }
  ExpressionHandle expr_handle{script.expr_pool.size()};
  auto bail_function_call = [&]() {
    script.expr_pool.push_back(
        FunctionCallExpression{callee_expr, std::move(arguments)});
    return expr_handle;
  };
  ExpressionHandle *callee_hdl{std::get_if<ExpressionHandle>(&callee_expr)};
  if (not callee_hdl)
    return bail_function_call();
  MemberExpression *expr_member{
      std::get_if<MemberExpression>(&script.expr_pool[callee_hdl->pool_idx])};
  if (not expr_member)
    return bail_function_call();
  script.expr_pool.push_back(MethodCallExpression{
      expr_member->object, expr_member->property, std::move(arguments)});
  return expr_handle;
}

Dynamic VanillaFunction::operator()(std::vector<Dynamic> arguments,
                                    Dynamic context, const Script &script) {
  return std::monostate{};
}

FunctionHandle Script::insert(AbstractFunction function) {
  FunctionHandle function_handle{function_pool.size()};
  function_pool.push_back(std::move(function));
  return function_handle;
}

ObjectHandle Script::insert(PlainObject object) {
  ObjectHandle object_handle{object_pool.size()};
  object_pool.push_back(std::move(object));
  return object_handle;
}

std::size_t Script::attach_atom(std::string atom_str) {
  auto [iter_atlas, did_insert] =
      atom_atlas.insert({atom_str, atom_pool.size()});
  if (did_insert)
    atom_pool.push_back(std::move(atom_str));
  return iter_atlas->second;
}

Dynamic Script::reduce(MethodCallExpression &expr_call) {
  auto reduce_parameter = [this](Expression parameter_expr) {
    return reduce(parameter_expr);
  };
  std::vector<Dynamic> arguments{
      std::from_range,
      std::ranges::transform_view{expr_call.arguments, reduce_parameter}};
  Dynamic context_call{reduce(expr_call.object)};
  ObjectHandle obj_handle{std::get<ObjectHandle>(context_call)};
  PlainObject &context_obj{object_pool[obj_handle.pool_idx.value()]};
  Dynamic method_dynamic{context_obj.at(expr_call.property)};
  FunctionHandle method_handle{std::get<FunctionHandle>(method_dynamic)};
  AbstractFunction &function_ref{function_pool[method_handle.pool_idx]};
  return function_ref(std::move(arguments), obj_handle, *this);
}

Dynamic Script::reduce(FunctionCallExpression &expr_call) {
  auto reduce_parameter = [this](Expression parameter_expr) {
    return reduce(parameter_expr);
  };
  std::vector<Dynamic> arguments{
      std::from_range,
      std::ranges::transform_view{expr_call.arguments, reduce_parameter}};
  Dynamic func_dynamic{reduce(expr_call.callee)};
  FunctionHandle func_handle{std::get<FunctionHandle>(func_dynamic)};
  AbstractFunction &function_ref{function_pool[func_handle.pool_idx]};
  return function_ref(std::move(arguments), std::monostate{}, *this);
}

Dynamic Script::reduce(Expression expression) {
  auto reduce_node = [&](auto &node) -> Dynamic {
    if constexpr (std::is_same_v<decltype(node), FunctionCallExpression &> ||
                  std::is_same_v<decltype(node), MethodCallExpression &>)
      return reduce(node);
    return std::monostate{};
  };
  auto reduce_expr = [&](auto expr) -> Dynamic {
    if constexpr (std::is_same_v<decltype(expr), ExpressionHandle>)
      return std::visit(reduce_node, expr_pool[expr.pool_idx]);
    if constexpr (std::is_same_v<decltype(expr), Identifier>)
      return script_scope.at(expr.pool_idx).value();
    return std::monostate{};
  };
  return expression.visit(reduce_expr);
}

void Script::execute(Statement statement) {
  auto execute_stmt = [this](auto stmt) -> void {
    if constexpr (std::is_same_v<decltype(stmt), Expression>)
      reduce(stmt);
    assert(0);
  };
  statement.visit(execute_stmt);
}

void Script::execute() {
  for (Statement statement : main_function.function_body)
    execute(statement);
}

} // namespace Manadrain
