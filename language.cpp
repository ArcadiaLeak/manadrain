#include <algorithm>
#include <cassert>
#include <functional>
#include <mutex>
#include <print>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_set<std::string_view> reserved_atlas{
    "const",  "let",    "var",   "class",    "function", "return",
    "import", "export", "from",  "as",       "default",  "undefined",
    "null",   "true",   "false", "if",       "else",     "while",
    "for",    "do",     "break", "continue", "switch"};
static const std::unordered_set<std::string_view> intrinsic_atlas{"console",
                                                                  "log"};

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

Token Parser::tokenize_identifier(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_ucs4(leading)};
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto xid_continue_view =
      traverse_text() | std::views::take_while(does_exist) | std::views::join |
      std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_ucs4) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto iter_reserved = reserved_atlas.find(identifier_str);
  if (iter_reserved != reserved_atlas.end())
    return ReservedWord{*iter_reserved};
  auto iter_intrinsic = intrinsic_atlas.find(identifier_str);
  if (iter_intrinsic != intrinsic_atlas.end())
    return Identifier{*iter_intrinsic};
  if (not atom_atlas.contains(identifier_str)) {
    std::shared_ptr identifier_buf{
        std::make_shared<char[]>(identifier_str.size())};
    std::memcpy(identifier_buf.get(), identifier_str.data(),
                identifier_str.size());
    atom_pool.push_back(identifier_buf);
    atom_atlas.insert(
        std::string_view{identifier_buf.get(), identifier_str.size()});
  }
  return Identifier{*atom_atlas.find(identifier_str)};
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
  auto iter_reserved = reserved_atlas.find(literal_str);
  if (iter_reserved != reserved_atlas.end())
    return StringHandle{*iter_reserved};
  auto iter_intrinsic = intrinsic_atlas.find(literal_str);
  if (iter_intrinsic != intrinsic_atlas.end())
    return StringHandle{*iter_intrinsic};
  if (not atom_atlas.contains(literal_str)) {
    std::shared_ptr literal_buf{std::make_shared<char[]>(literal_str.size())};
    std::memcpy(literal_buf.get(), literal_str.data(), literal_str.size());
    atom_pool.push_back(literal_buf);
    atom_atlas.insert(std::string_view{literal_buf.get(), literal_str.size()});
  }
  return StringHandle{*atom_atlas.find(literal_str)};
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
  return num_literal;
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
      token_ret = tokenize_identifier(leading);
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
  FunctionBlueprint blueprint{};
  auto match_script_end = [](Token token) {
    return !std::holds_alternative<std::monostate>(token);
  };
  for (Token token :
       traverse_tokens() | std::views::take_while(match_script_end))
    parse_statement(blueprint, token);
  blueprint_pool.push_back(
      std::make_shared<FunctionBlueprint>(std::move(blueprint)));
}

void Parser::parse_statement(FunctionBlueprint &blueprint, Token leading) {
  ReservedWord *word{std::get_if<ReservedWord>(&leading)};
  Statement statement;
  if (word->sv == "function") {
    const FunctionBlueprint *blueprint_ptr{parse_function_decl()};
    std::string_view function_name{blueprint_ptr->function_name};
    if (std::ranges::binary_search(blueprint.scope_shape, function_name.data()))
      throw ScriptError{InvalidDeclaration{}};
    blueprint.nestedly_declared.push_back(
        {function_name.data(), blueprint_ptr});
    auto lower_bound =
        std::ranges::lower_bound(blueprint.scope_shape, function_name.data());
    blueprint.scope_shape.insert(lower_bound, function_name.data());
  } else if (word->sv == "let") {
    VariableDeclaration declaration{parse_variable_decl()};
    std::string_view variable_name{declaration.variable_name};
    if (std::ranges::binary_search(blueprint.scope_shape, variable_name.data()))
      throw ScriptError{InvalidDeclaration{}};
    auto lower_bound =
        std::ranges::lower_bound(blueprint.scope_shape, variable_name.data());
    blueprint.scope_shape.insert(lower_bound, variable_name.data());
    blueprint.body.push_back(declaration);
  } else if (word->sv == "return") {
    blueprint.body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  } else {
    history_pull();
    blueprint.body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  }
}

const FunctionBlueprint *Parser::parse_function_decl() {
  auto extract_name = [](auto token) -> std::string_view {
    if constexpr (std::is_same_v<decltype(token), Identifier>)
      return token.sv;
    throw ScriptError{MissingFunctionName{}};
  };
  std::string_view function_name{std::visit(extract_name, tokenize())};
  for (char function_punct : std::to_array({'(', ')', '{'}))
    assert_punct(tokenize(), function_punct);
  auto match_function_end = [&](Token token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return !alter_ptr || *alter_ptr != '}';
  };
  FunctionBlueprint blueprint{function_name};
  for (Token token :
       traverse_tokens() | std::views::take_while(match_function_end)) {
    if (std::holds_alternative<std::monostate>(token))
      throw ScriptError{MissingPunctuation{'}'}};
    parse_statement(blueprint, token);
  }
  std::shared_ptr blueprint_ptr{
      std::make_shared<FunctionBlueprint>(std::move(blueprint))};
  blueprint_pool.push_back(blueprint_ptr);
  return blueprint_ptr.get();
}

VariableDeclaration Parser::parse_variable_decl() {
  auto extract_name = [](auto token) -> std::string_view {
    if constexpr (std::is_same_v<decltype(token), Identifier>)
      return token.sv;
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
                  std::is_same_v<decltype(t), Identifier> ||
                  std::is_same_v<decltype(t), std::int64_t> ||
                  std::is_same_v<decltype(t), double>)
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
    std::shared_ptr expr_ptr{std::make_shared<ExpressionNode>(
        BinaryExpression{expr_left, parse_postfix_expr(), binary_op})};
    expr_pool.push_back(expr_ptr);
    return expr_ptr.get();
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
  std::shared_ptr expr_ptr{std::make_shared<ExpressionNode>(
      MemberExpression{obj_expr, property.sv})};
  expr_pool.push_back(expr_ptr);
  return expr_ptr.get();
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
  std::shared_ptr expr_ptr{std::make_shared<ExpressionNode>(
      FunctionCallExpression{callee_expr, std::move(arguments)})};
  expr_pool.push_back(expr_ptr);
  return expr_ptr.get();
}

static std::array shape_console{std::to_array<const char *>({"log"})};
static std::array shape_global_this{std::to_array<const char *>({"console"})};

static std::once_flag static_shape_flag;
static void static_shape_init() {
  std::sort(shape_console.begin(), shape_console.end());
  std::sort(shape_global_this.begin(), shape_global_this.end());
}

Script::Script() : console{shape_console}, global_this{shape_global_this} {
  std::call_once(static_shape_flag, static_shape_init);
  console.properties[std::distance(
      shape_console.begin(),
      std::ranges::lower_bound(shape_console,
                               static_cast<const char *>("log")))] =
      FunctionHandle{~H_LOG};
  global_this.properties[std::distance(
      shape_global_this.begin(),
      std::ranges::lower_bound(shape_global_this,
                               static_cast<const char *>("console")))] =
      ObjectHandle{~H_CONSOLE};
}

Dynamic Script::evaluate(VanillaFunction &function,
                         const FunctionCallExpression &expr_call) {
  Dynamic dynamic_callee{evaluate(function, expr_call.callee)};
  auto match_function_handle = [&](auto dynamic_alt) -> std::ptrdiff_t {
    if constexpr (std::is_same_v<decltype(dynamic_alt), FunctionHandle>)
      return dynamic_alt.handle;
    throw ScriptError{InvalidFunctionCall{}};
  };
  std::ptrdiff_t callee_handle{
      std::visit(match_function_handle, dynamic_callee)};
  assert(callee_handle >= 0);
  const FunctionBlueprint *blueprint_ptr{
      function_pool[callee_handle]->blueprint_handle};
  for (Statement statement : blueprint_ptr->body) {
    evaluate(*function_pool[callee_handle], statement);
    if (std::holds_alternative<ReturnStatement>(statement))
      break;
  }
  return function_pool[callee_handle]->return_val;
}

Dynamic Script::evaluate(VanillaFunction &function,
                         const BinaryExpression &expression) {
  return {};
}

Dynamic Script::evaluate(VanillaFunction &function,
                         const MemberExpression &expression) {
  return {};
}

Dynamic Script::evaluate(VanillaFunction &function,
                         const ExpressionNode *expr_ptr) {
  return expr_ptr->alt.visit([&](const auto &exprnode_alt) {
    return evaluate(function, exprnode_alt);
  });
}

Dynamic Script::evaluate(VanillaFunction &function,
                         StringHandle string_handle) {
  return Dynamic{string_handle};
}
Dynamic Script::evaluate(VanillaFunction &function, std::int64_t number) {
  return Dynamic{number};
}
Dynamic Script::evaluate(VanillaFunction &function, double number) {
  return Dynamic{number};
}

Dynamic Script::evaluate(VanillaFunction &function, Expression expression) {
  return expression.visit(
      [&](auto &expression_alt) { return evaluate(function, expression_alt); });
}

std::generator<std::reference_wrapper<VanillaFunction>>
Script::traverse_function_closure(
    std::reference_wrapper<VanillaFunction> function_ref) {
  while (1) {
    co_yield function_ref;
    std::optional parent_handle{function_ref.get().parent_handle};
    if (not parent_handle)
      break;
    function_ref = *function_pool[*parent_handle];
  }
}

std::optional<Dynamic> *Script::get_variable(VanillaFunction &function,
                                             std::string_view var_handle) {
  for (VanillaFunction &current_function :
       traverse_function_closure(function)) {
    const auto &scope_shape{current_function.blueprint_handle->scope_shape};
    auto lower_bound = std::ranges::lower_bound(scope_shape, var_handle);
    if (lower_bound == scope_shape.end() || *lower_bound != var_handle)
      continue;
    std::ptrdiff_t own_scope_distance{
        std::distance(scope_shape.begin(), lower_bound)};
    std::optional<Dynamic> *own_scope_data{current_function.own_scope.data()};
    return std::next(own_scope_data, own_scope_distance);
  }
  return nullptr;
}

Dynamic *Script::get_property(VanillaObject &object,
                              std::string_view property_handle) {
  auto lower_bound =
      std::ranges::lower_bound(object.object_shape, property_handle.data());
  if (lower_bound == object.object_shape.end() ||
      *lower_bound != property_handle.data())
    return nullptr;
  std::ptrdiff_t property_distance{
      std::distance(object.object_shape.begin(), lower_bound)};
  return std::next(object.properties.data(), property_distance);
}

Dynamic Script::evaluate(VanillaFunction &function, Identifier identifier) {
  std::optional<Dynamic> *local_variable{get_variable(function, identifier.sv)};
  if (local_variable && *local_variable)
    return **local_variable;
  Dynamic *global_property{get_property(global_this, identifier.sv)};
  if (global_property)
    return *global_property;
  throw ScriptError{InvalidVariableAccess{}};
}

void Script::evaluate(VanillaFunction &function,
                      VariableDeclaration declaration) {
  Dynamic initializer_dynamic{evaluate(function, declaration.initializer)};
  std::optional<Dynamic> *variable_lvalue{
      get_variable(function, declaration.variable_name)};
  assert(variable_lvalue != nullptr);
  *variable_lvalue = initializer_dynamic;
}

void Script::evaluate(VanillaFunction &function, ReturnStatement statement) {
  evaluate(function, statement.argument);
}

void Script::evaluate(VanillaFunction &function, Statement statement) {
  std::visit([&](auto alternative) { evaluate(function, alternative); },
             statement);
}

FunctionHandle Script::bootstrap(std::size_t blueprint_handle,
                                 std::optional<std::size_t> parent_handle) {
  ObjectShape &scope_shape = blueprint_pool[blueprint_handle].scope_shape;
  std::ptrdiff_t function_handle = function_pool.size();
  function_pool.push_back(VanillaFunction{blueprint_handle, parent_handle});
  function_pool[function_handle].own_scope.resize(scope_shape.size());
  for (auto [nested_name, nested_blueprint] :
       blueprint_pool[blueprint_handle].nested_blueprint) {
    auto lower_bound = std::ranges::lower_bound(scope_shape, nested_name);
    assert(lower_bound != scope_shape.end() && *lower_bound == nested_name);
    std::size_t scope_idx = std::distance(scope_shape.begin(), lower_bound);
    FunctionHandle nested_function{
        bootstrap(nested_blueprint, function_handle)};
    function_pool[function_handle].own_scope[scope_idx] =
        Dynamic{nested_function};
  }
  return FunctionHandle{function_handle};
}

void Script::evaluate() {
  std::size_t blueprint_handle{
      function_pool[main_function.handle].blueprint_handle};
  for (Statement statement : blueprint_pool[blueprint_handle].body)
    evaluate(main_function.handle, statement);
}
} // namespace Manadrain
