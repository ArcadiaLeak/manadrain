#include <algorithm>
#include <cassert>
#include <functional>
#include <mutex>
#include <print>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, std::size_t> reserved_atlas{
    {"const", 0},    {"let", 1},       {"var", 2},      {"class", 3},
    {"function", 4}, {"return", 5},    {"import", 6},   {"export", 7},
    {"from", 8},     {"as", 9},        {"default", 10}, {"undefined", 11},
    {"null", 12},    {"true", 13},     {"false", 14},   {"if", 15},
    {"else", 16},    {"while", 17},    {"for", 18},     {"do", 19},
    {"break", 20},   {"continue", 21}, {"switch", 22}};
static const std::unordered_map<std::string_view, std::size_t> intrinsic_atlas{
    {"console", 23}, {"log", 24}};
static const std::array permanent_pool{std::to_array<std::string_view>(
    {"const",   "let",       "var",    "class",   "function",
     "return",  "import",    "export", "from",    "as",
     "default", "undefined", "null",   "true",    "false",
     "if",      "else",      "while",  "for",     "do",
     "break",   "continue",  "switch", "console", "log"})};
static std::array shape_console{
    std::to_array<Identifier>({Identifier{1, 24, 3}})};
static std::array shape_global_this{
    std::to_array<Identifier>({Identifier{1, 23, 7}})};

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
    return ReservedWord{iter_reserved->second};
  auto iter_intrinsic = intrinsic_atlas.find(identifier_str);
  if (iter_intrinsic != intrinsic_atlas.end())
    return Identifier{1, iter_intrinsic->second, identifier_str.size()};
  if (not atom_atlas.contains(identifier_str)) {
    std::shared_ptr identifier_buf{
        std::make_shared<char[]>(identifier_str.size())};
    std::memcpy(identifier_buf.get(), identifier_str.data(),
                identifier_str.size());
    atom_pool.push_back(identifier_buf);
    atom_atlas[std::string_view{identifier_buf.get(), identifier_str.size()}] =
        atom_pool.size() - 1;
  }
  return Identifier{0, atom_atlas[identifier_str], identifier_str.size()};
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
    return StringHandle{1, iter_reserved->second, literal_str.size()};
  auto iter_intrinsic = intrinsic_atlas.find(literal_str);
  if (iter_intrinsic != intrinsic_atlas.end())
    return StringHandle{1, iter_intrinsic->second, literal_str.size()};
  if (not atom_atlas.contains(literal_str)) {
    std::shared_ptr literal_buf{std::make_shared<char[]>(literal_str.size())};
    std::memcpy(literal_buf.get(), literal_str.data(), literal_str.size());
    atom_pool.push_back(literal_buf);
    atom_atlas[std::string_view{literal_buf.get(), literal_str.size()}] =
        atom_pool.size() - 1;
  }
  return StringHandle{0, atom_atlas[literal_str], literal_str.size()};
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
  std::shared_ptr blueprint_ptr{std::make_shared<FunctionBlueprint>()};
  main_function.blueprint_ptr = blueprint_ptr.get();
  auto match_script_end = [](Token token) {
    return !std::holds_alternative<std::monostate>(token);
  };
  for (Token token :
       traverse_tokens() | std::views::take_while(match_script_end))
    parse_statement(*blueprint_ptr, token);
  blueprint_pool.push_back(blueprint_ptr);
  instantiate(FunctionHandle{~H_MAIN});
}

void Parser::parse_statement(FunctionBlueprint &blueprint, Token leading) {
  ReservedWord *word_ptr{std::get_if<ReservedWord>(&leading)};
  if (word_ptr && permanent_pool[word_ptr->offset] == "function") {
    const FunctionBlueprint *blueprint_ptr{parse_function_decl()};
    Identifier function_name{blueprint_ptr->function_name};
    if (std::ranges::binary_search(blueprint.scope_shape, function_name))
      throw ScriptError{InvalidDeclaration{}};
    blueprint.nestedly_declared.push_back({function_name, blueprint_ptr});
    auto lower_bound =
        std::ranges::lower_bound(blueprint.scope_shape, function_name);
    blueprint.scope_shape.insert(lower_bound, function_name);
    return;
  }
  if (word_ptr && permanent_pool[word_ptr->offset] == "let") {
    VariableDeclaration declaration{parse_variable_decl()};
    Identifier variable_name{declaration.variable_name};
    if (std::ranges::binary_search(blueprint.scope_shape, variable_name))
      throw ScriptError{InvalidDeclaration{}};
    auto lower_bound =
        std::ranges::lower_bound(blueprint.scope_shape, variable_name);
    blueprint.scope_shape.insert(lower_bound, variable_name);
    blueprint.body.push_back(declaration);
    return;
  }
  if (word_ptr && permanent_pool[word_ptr->offset] == "return") {
    blueprint.body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
    return;
  }
  history_pull();
  blueprint.body.push_back(parse_expression());
  assert_punct(tokenize(), ';');
}

const FunctionBlueprint *Parser::parse_function_decl() {
  Token leading{tokenize()};
  Identifier *function_name{std::get_if<Identifier>(&leading)};
  if (not function_name)
    throw ScriptError{MissingFunctionName{}};
  for (char function_punct : std::to_array({'(', ')', '{'}))
    assert_punct(tokenize(), function_punct);
  auto match_function_end = [&](Token token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return !alter_ptr || *alter_ptr != '}';
  };
  FunctionBlueprint blueprint{*function_name};
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
  Token leading{tokenize()};
  Identifier *variable_name{std::get_if<Identifier>(&leading)};
  if (not variable_name)
    throw ScriptError{MissingVariableName{}};
  VariableDeclaration variable_decl{*variable_name};
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
  Token leading{tokenize()};
  Identifier *field_name{std::get_if<Identifier>(&leading)};
  if (not field_name)
    throw ScriptError{MissingFieldName{}};
  std::shared_ptr expr_ptr{std::make_shared<ExpressionNode>(
      MemberExpression{obj_expr, *field_name})};
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

Script::Script() : console{shape_console}, global_this{shape_global_this} {
  console.properties = {FunctionHandle{~H_LOG}};
  global_this.properties = {ObjectHandle{~H_CONSOLE}};
}

std::generator<std::reference_wrapper<VanillaFunction>>
Script::traverse_function_closure(
    std::reference_wrapper<VanillaFunction> function_ref) {
  while (1) {
    co_yield function_ref;
    std::ptrdiff_t parent_handle{function_ref.get().parent_handle};
    std::optional<VanillaFunction &> parent_opt{};
    if (parent_handle >= 0)
      parent_opt = *function_pool[parent_handle];
    else if (~parent_handle == H_MAIN)
      parent_opt = main_function;
    else if (~parent_handle == H_NIL)
      break;
    function_ref = parent_opt.value();
  }
}

std::optional<Dynamic> *Script::get_variable(VanillaFunction &function,
                                             Identifier var_handle) {
  for (VanillaFunction &current_function :
       traverse_function_closure(function)) {
    const auto &scope_shape{current_function.blueprint_ptr->scope_shape};
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
                              Identifier property_handle) {
  auto lower_bound =
      std::ranges::lower_bound(object.object_shape, property_handle);
  if (lower_bound == object.object_shape.end() ||
      *lower_bound != property_handle)
    return nullptr;
  std::ptrdiff_t property_distance{
      std::distance(object.object_shape.begin(), lower_bound)};
  return std::next(object.properties.data(), property_distance);
}

Dynamic Script::evaluate_property(Identifier property, std::monostate) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  StringHandle string_handle) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property, std::int64_t number) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property, double number) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  ObjectHandle object_handle) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  FunctionHandle function_handle) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(VanillaFunction &function,
                        const BinaryExpression &expression) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(VanillaFunction &function,
                        const MemberExpression &expression) {
  Dynamic dynamic_object{evaluate(function, expression.object)};
  auto visit_object = [&](auto dynamic_alt) {
    return evaluate_property(expression.property, dynamic_alt);
  };
  Dynamic dynamic_property{dynamic_object.visit(visit_object)};
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(VanillaFunction &function,
                        const FunctionCallExpression &expression) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(VanillaFunction &function,
                                                    Identifier identifier) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(VanillaFunction &function,
                        const ExpressionNode *expr_ptr) {
  return expr_ptr->alt.visit([&](const auto &expr_alt) {
    return evaluate_callee(function, expr_alt);
  });
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(VanillaFunction &function, StringHandle string_handle) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(VanillaFunction &function,
                                                    std::int64_t number) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(VanillaFunction &function,
                                                    double number) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(VanillaFunction &function,
                                                    std::monostate) {
  return {};
}

Dynamic Script::evaluate(VanillaFunction &function,
                         const FunctionCallExpression &expr_call) {
  auto visit_expression = [&](auto expression_alt) {
    return evaluate_callee(function, expression_alt);
  };
  std::pair dynamic_callee{expr_call.callee.visit(visit_expression)};
  return {};
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
  return expr_ptr->alt.visit(
      [&](const auto &expr_alt) { return evaluate(function, expr_alt); });
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
      [&](auto expression_alt) { return evaluate(function, expression_alt); });
}

Dynamic Script::evaluate(VanillaFunction &function, Identifier identifier) {
  std::optional<Dynamic> *local_variable{get_variable(function, identifier)};
  if (local_variable && *local_variable)
    return **local_variable;
  Dynamic *global_property{get_property(global_this, identifier)};
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

void Script::instantiate(FunctionHandle function_handle) {
  std::optional<VanillaFunction &> function_opt{};
  if (~function_handle.offset == H_MAIN)
    function_opt = main_function;
  else if (function_handle.offset >= 0)
    function_opt = *function_pool[function_handle.offset];
  VanillaFunction &function_ref{function_opt.value()};
  const auto &scope_shape = function_ref.blueprint_ptr->scope_shape;
  function_ref.own_scope.resize(scope_shape.size());
  for (auto [nested_name, nested_blueprint] :
       function_ref.blueprint_ptr->nestedly_declared) {
    auto lower_bound = std::ranges::lower_bound(scope_shape, nested_name);
    assert(lower_bound != scope_shape.end() && *lower_bound == nested_name);
    std::size_t scope_idx = std::distance(scope_shape.begin(), lower_bound);
    FunctionHandle nested_handle{
        static_cast<std::ptrdiff_t>(function_pool.size())};
    function_pool.push_back(std::make_unique<VanillaFunction>(
        nested_blueprint, function_handle.offset));
    instantiate(nested_handle);
    function_ref.own_scope[scope_idx] = Dynamic{nested_handle};
  }
}

void Script::evaluate() {
  for (Statement statement : main_function.blueprint_ptr->body)
    evaluate(main_function, statement);
}
} // namespace Manadrain
