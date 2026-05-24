#include <algorithm>
#include <cassert>
#include <functional>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, Keyword> keyword_atlas{
    {"const", Keyword::K_CONST},       {"let", Keyword::K_LET},
    {"var", Keyword::K_VAR},           {"class", Keyword::K_CLASS},
    {"function", Keyword::K_FUNCTION}, {"return", Keyword::K_RETURN},
    {"import", Keyword::K_IMPORT},     {"export", Keyword::K_EXPORT},
    {"from", Keyword::K_FROM},         {"as", Keyword::K_AS},
    {"default", Keyword::K_DEFAULT},   {"undefined", Keyword::K_UNDEFINED},
    {"null", Keyword::K_NULL},         {"true", Keyword::K_TRUE},
    {"false", Keyword::K_FALSE},       {"if", Keyword::K_IF},
    {"else", Keyword::K_ELSE},         {"while", Keyword::K_WHILE},
    {"for", Keyword::K_FOR},           {"do", Keyword::K_DO},
    {"break", Keyword::K_BREAK},       {"continue", Keyword::K_CONTINUE},
    {"switch", Keyword::K_SWITCH}};
static const std::unordered_map<std::string_view, std::size_t> identifier_atlas{
    {"console", 0}, {"log", 1}};
static const std::array permanent_identifiers{
    std::to_array<std::string_view>({"console", "log"})};
static const std::array shape_console{
    std::to_array<Identifier>({Identifier{1}})};
static const std::array shape_global_this{
    std::to_array<Identifier>({Identifier{0}})};

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
  auto iter_reserved = keyword_atlas.find(identifier_str);
  if (iter_reserved != keyword_atlas.end())
    return iter_reserved->second;
  auto iter_permanent = identifier_atlas.find(identifier_str);
  if (iter_permanent != identifier_atlas.end())
    return Identifier{iter_permanent->second};
  if (not atom_atlas.contains(identifier_str)) {
    std::shared_ptr identifier_buf{
        std::make_shared<char[]>(identifier_str.size())};
    std::memcpy(identifier_buf.get(), identifier_str.data(),
                identifier_str.size());
    atom_pool.push_back({identifier_buf, identifier_str.size()});
    atom_atlas[std::string_view{identifier_buf.get(), identifier_str.size()}] =
        Identifier{permanent_identifiers.size() + atom_pool.size() - 1};
  }
  return atom_atlas[identifier_str];
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
  auto iter_existing = string_atlas.find(literal_str);
  if (iter_existing != string_atlas.end())
    return iter_existing->second;
  std::shared_ptr literal_buf{std::make_shared<char[]>(literal_str.size())};
  std::memcpy(literal_buf.get(), literal_str.data(), literal_str.size());
  ImmuString immu_string{literal_buf, literal_str.size()};
  string_atlas[std::string_view{literal_buf.get(), literal_str.size()}] =
      immu_string;
  return immu_string;
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
  std::shared_ptr definition{std::make_shared<FunctionDefinition>()};
  main_function.definition = definition.get();
  auto match_script_end = [](Token token) {
    return !std::holds_alternative<std::monostate>(token);
  };
  for (Token token :
       traverse_tokens() | std::views::take_while(match_script_end))
    parse_statement(*definition, token);
  function_definitions.push_back(definition);
  instantiate(FunctionHandle{std::unexpected{IntrinsicSigil::H_MAIN}});
}

void Parser::parse_statement(FunctionDefinition &definition, Token leading) {
  Keyword *word_ptr{std::get_if<Keyword>(&leading)};
  if (word_ptr && *word_ptr == Keyword::K_FUNCTION) {
    const FunctionDefinition *nested_definition{parse_function_decl()};
    Identifier function_name{nested_definition->function_name};
    if (std::ranges::binary_search(definition.local_scope, function_name))
      throw ScriptError{DuplicateDeclaration{}};
    definition.nestedly_declared.push_back({function_name, nested_definition});
    auto lower_bound =
        std::ranges::lower_bound(definition.local_scope, function_name);
    definition.local_scope.insert(lower_bound, function_name);
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_LET) {
    VariableDeclaration declaration{parse_variable_decl()};
    Identifier variable_name{declaration.variable_name};
    if (std::ranges::binary_search(definition.local_scope, variable_name))
      throw ScriptError{DuplicateDeclaration{}};
    auto lower_bound =
        std::ranges::lower_bound(definition.local_scope, variable_name);
    definition.local_scope.insert(lower_bound, variable_name);
    definition.body.push_back(declaration);
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_RETURN) {
    definition.body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
    return;
  }
  history_pull();
  definition.body.push_back(parse_expression());
  assert_punct(tokenize(), ';');
}

const FunctionDefinition *Parser::parse_function_decl() {
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
  FunctionDefinition definition{*function_name};
  for (Token token :
       traverse_tokens() | std::views::take_while(match_function_end)) {
    if (std::holds_alternative<std::monostate>(token))
      throw ScriptError{MissingPunctuation{'}'}};
    parse_statement(definition, token);
  }
  std::shared_ptr definition_ptr{
      std::make_shared<FunctionDefinition>(std::move(definition))};
  function_definitions.push_back(definition_ptr);
  return definition_ptr.get();
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
    if constexpr (std::is_same_v<decltype(t), ImmuString> ||
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

std::generator<FunctionClosure *>
Script::climb_closure_stack(FunctionClosure *closure_ptr) {
  while (1) {
    co_yield closure_ptr;
    std::expected parent_handle{closure_ptr->parent_handle};
    if (parent_handle == std::unexpected{IntrinsicSigil::H_NIL})
      break;
    else if (parent_handle == std::unexpected{IntrinsicSigil::H_MAIN})
      closure_ptr = &main_function;
    else {
      std::size_t parent_idx{parent_handle.value()};
      closure_ptr = function_closures[parent_idx].get();
    }
  }
}

std::optional<Dynamic> *Script::get_variable(FunctionClosure &function,
                                             Identifier var_handle) {
  for (FunctionClosure *closure_ptr : climb_closure_stack(&function)) {
    const auto &local_scope{closure_ptr->definition->local_scope};
    auto lower_bound = std::ranges::lower_bound(local_scope, var_handle);
    if (lower_bound == local_scope.end() || *lower_bound != var_handle)
      continue;
    std::ptrdiff_t own_scope_distance{
        std::distance(local_scope.begin(), lower_bound)};
    std::optional<Dynamic> *own_scope_data{closure_ptr->own_scope.data()};
    return std::next(own_scope_data, own_scope_distance);
  }
  return nullptr;
}

Dynamic *Script::get_property(VanillaObject &object,
                              Identifier property_handle) {
  std::span<const Identifier> object_shape{};
  if (object.shape_handle == std::unexpected{IntrinsicSigil::H_CONSOLE})
    object_shape = shape_console;
  else if (object.shape_handle == std::unexpected{IntrinsicSigil::H_GLOBAL})
    object_shape = shape_global_this;
  else {
    std::shared_ptr<const Identifier[]> &shape_ptr{object.shape_handle.value()};
    object_shape = {shape_ptr.get(), object.properties.size()};
  }
  auto lower_bound = std::ranges::lower_bound(object_shape, property_handle);
  if (lower_bound == object_shape.end() || *lower_bound != property_handle)
    return nullptr;
  std::ptrdiff_t property_distance{
      std::distance(object_shape.begin(), lower_bound)};
  return std::next(object.properties.data(), property_distance);
}

Dynamic Script::evaluate_property(Identifier property, std::monostate) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property, ImmuString immu_string) {
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
  VanillaObject *object_ptr{};
  if (object_handle.offset == std::unexpected{IntrinsicSigil::H_CONSOLE})
    object_ptr = &console;
  else {
    std::size_t object_idx{object_handle.offset.value()};
    object_ptr = object_pool[object_idx].get();
  }
  Dynamic *property_ptr{get_property(*object_ptr, property)};
  if (not property_ptr)
    return std::monostate{};
  return *property_ptr;
}

Dynamic Script::evaluate_property(Identifier property,
                                  FunctionHandle function_handle) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &function,
                        const BinaryExpression &expression) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &closure,
                        const MemberExpression &expression) {
  Dynamic dynamic_object{evaluate(closure, expression.object)};
  auto visit_object = [&](auto dynamic_alt) {
    return evaluate_property(expression.property, dynamic_alt);
  };
  Dynamic dynamic_property{dynamic_object.visit(visit_object)};
  return {dynamic_object, dynamic_property};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &closure,
                        const FunctionCallExpression &expression) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(FunctionClosure &closure,
                                                    Identifier identifier) {
  return {std::monostate{}, evaluate(closure, identifier)};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &closure,
                        const ExpressionNode *expr_ptr) {
  return expr_ptr->alt.visit(
      [&](const auto &expr_alt) { return evaluate_callee(closure, expr_alt); });
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(FunctionClosure &closure,
                                                    ImmuString immu_string) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(FunctionClosure &closure,
                                                    std::int64_t number) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(FunctionClosure &closure,
                                                    double number) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(FunctionClosure &closure,
                                                    std::monostate) {
  return {};
}

std::string Script::evaluate_message(std::monostate) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(ImmuString immu_string) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(std::int64_t number) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(double number) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(ObjectHandle object_handle) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(FunctionHandle function_handle) {
  return "<unimplemented>";
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const FunctionCallExpression &expr_call) {
  auto visit_expression = [&](auto expression_alt) {
    return evaluate_callee(closure, expression_alt);
  };
  auto [dynamic_context, dynamic_callee] =
      expr_call.callee.visit(visit_expression);
  FunctionHandle *callee_handle{std::get_if<FunctionHandle>(&dynamic_callee)};
  if (not callee_handle)
    throw ScriptError{InvalidFunctionCall{}};
  auto dynamic_arguments =
      expr_call.arguments | std::views::transform([&](Expression expression) {
        return evaluate(closure, expression);
      });
  if (callee_handle->offset == std::unexpected{IntrinsicSigil::H_LOG}) {
    auto match_message_alt = [&](auto dynamic_alt) {
      return evaluate_message(dynamic_alt);
    };
    auto match_message_arg = [&](Dynamic argument) {
      return argument.visit(match_message_alt);
    };
    auto message_parts = dynamic_arguments |
                         std::views::transform(match_message_arg) |
                         std::views::join_with(' ');
    console_messages.emplace_back(std::from_range, message_parts);
    return std::monostate{};
  } else {
    FunctionClosure &callee{*function_closures[callee_handle->offset.value()]};
    for (auto [index, argument] : dynamic_arguments | std::views::enumerate) {
      Identifier identifier{callee.definition->arguments[index]};
      auto *argument_lvalue{get_variable(callee, identifier)};
      assert(argument_lvalue != nullptr);
      *argument_lvalue = argument;
    }
    for (Statement statement : callee.definition->body)
      evaluate(callee, statement);
    return callee.return_val;
  }
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const BinaryExpression &expression) {
  return {};
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const MemberExpression &expression) {
  return {};
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const ExpressionNode *expr_ptr) {
  return expr_ptr->alt.visit(
      [&](const auto &expr_alt) { return evaluate(closure, expr_alt); });
}

Dynamic Script::evaluate(FunctionClosure &closure, ImmuString immu_string) {
  return Dynamic{immu_string};
}

Dynamic Script::evaluate(FunctionClosure &closure, std::int64_t number) {
  return Dynamic{number};
}

Dynamic Script::evaluate(FunctionClosure &closure, double number) {
  return Dynamic{number};
}

Dynamic Script::evaluate(FunctionClosure &closure, Expression expression) {
  return expression.visit(
      [&](auto expression_alt) { return evaluate(closure, expression_alt); });
}

Dynamic Script::evaluate(FunctionClosure &closure, Identifier identifier) {
  std::optional<Dynamic> *from_scope{get_variable(closure, identifier)};
  if (from_scope && *from_scope)
    return **from_scope;
  Dynamic *from_globalThis{get_property(global_this, identifier)};
  if (from_globalThis)
    return *from_globalThis;
  throw ScriptError{InvalidVariableAccess{}};
}

void Script::evaluate(FunctionClosure &closure,
                      VariableDeclaration declaration) {
  Dynamic initializer_dynamic{evaluate(closure, declaration.initializer)};
  std::optional<Dynamic> *variable_lvalue{
      get_variable(closure, declaration.variable_name)};
  assert(variable_lvalue != nullptr);
  *variable_lvalue = initializer_dynamic;
}

void Script::evaluate(FunctionClosure &closure, ReturnStatement statement) {
  evaluate(closure, statement.argument);
}

void Script::evaluate(FunctionClosure &closure, Statement statement) {
  std::visit([&](auto alternative) { evaluate(closure, alternative); },
             statement);
}

void Script::instantiate(FunctionHandle function_handle) {
  using IndirectFunction = std::expected<FunctionClosure *, IntrinsicSigil>;
  auto match_function_offset = [&](std::size_t pool_idx) {
    return function_closures[pool_idx].get();
  };
  auto match_function_sigil = [&](IntrinsicSigil sigil) {
    return sigil == IntrinsicSigil::H_MAIN ? IndirectFunction{&main_function}
                                           : std::unexpected{sigil};
  };
  std::expected function_offset{function_handle.offset};
  FunctionClosure *closure_ptr{function_offset.transform(match_function_offset)
                                   .or_else(match_function_sigil)
                                   .value()};
  const FunctionDefinition &definition{*closure_ptr->definition};
  closure_ptr->own_scope.resize(definition.local_scope.size());
  for (auto [nested_name, nested_definition] : definition.nestedly_declared) {
    auto *function_lvalue{get_variable(*closure_ptr, nested_name)};
    assert(function_lvalue != nullptr);
    FunctionHandle nested_handle{function_closures.size()};
    function_closures.push_back(std::make_unique<FunctionClosure>(
        nested_definition, function_handle.offset));
    instantiate(nested_handle);
    *function_lvalue = Dynamic{nested_handle};
  }
}

void Script::evaluate() {
  for (Statement statement : main_function.definition->body)
    evaluate(main_function, statement);
}
} // namespace Manadrain
