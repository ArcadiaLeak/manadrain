#include <algorithm>
#include <cassert>
#include <functional>
#include <inplace_vector>

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
    {"console", IDENT_console}, {"log", IDENT_log}, {"length", IDENT_length}};
static const std::array permanent_identifiers{
    std::to_array<std::string_view>({"console", "log", "length"})};

std::optional<char32_t> Parser::forward() {
  if (position >= text_buffer->size())
    backtrace.push_back(std::nullopt);
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_buffer->data() + position,
                           text_buffer->size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push_back(ch);
  }
  return backtrace.back();
}

std::generator<std::optional<char32_t>> Parser::traverse_text() {
  while (1)
    co_yield forward();
}

std::inplace_vector<char, 6> traverse_u8(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int advance{u8_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  return {std::from_range, buffer | std::views::take(advance)};
}

std::inplace_vector<char16_t, 2> traverse_u16(ucs4_t cp) {
  std::array<std::uint16_t, 2> buffer{};
  int advance{u16_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  return {std::from_range, buffer | std::views::take(advance)};
}

void Parser::backward() {
  std::optional<char32_t> behind{backtrace.back()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_u8) | std::views::join);
  backtrace.pop_back();
}

void Parser::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

Token Parser::tokenize_identifier(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_u8(leading)};
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto xid_continue_view =
      traverse_text() | std::views::take_while(does_exist) | std::views::join |
      std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_u8) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto iter_reserved = keyword_atlas.find(identifier_str);
  if (iter_reserved != keyword_atlas.end())
    return iter_reserved->second;
  auto iter_permanent = identifier_atlas.find(identifier_str);
  if (iter_permanent != identifier_atlas.end())
    return Identifier{iter_permanent->second};
  if (atom_atlas.contains(identifier_str))
    return atom_atlas[identifier_str];
  atom_pool.push_back(std::make_unique<std::string>(std::move(identifier_str)));
  std::string_view identifier_view{*atom_pool.back()};
  atom_atlas[identifier_view] =
      Identifier{permanent_identifiers.size() + atom_pool.size() - 1};
  return atom_atlas[identifier_view];
}

Token Parser::tokenize_string_literal(char32_t separator) {
  std::u16string literal_str{};
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
    literal_str.append_range(traverse_u16(*leading));
  }
  auto iter_existing = string_atlas.find(literal_str);
  if (iter_existing != string_atlas.end())
    return *iter_existing;
  permanent_strings.push_back(
      std::make_shared<std::u16string>(std::move(literal_str)));
  std::u16string_view permanent_view{*permanent_strings.back()};
  string_atlas.insert(permanent_view);
  return permanent_view;
}

Token Parser::tokenize_numeric_literal(char32_t leading) {
  std::string numeric_str{std::from_range, traverse_u8(leading)};
  auto match_nullopt = [](auto an_optional) { return an_optional.has_value(); };
  auto match_digit = [](char32_t code_point) {
    return std::isdigit(code_point);
  };
  numeric_str.append_range(
      traverse_text() | std::views::take_while(match_nullopt) |
      std::views::join | std::views::take_while(match_digit) |
      std::views::transform(traverse_u8) | std::views::join);
  backward();
  std::int64_t num_literal{};
  std::from_chars(numeric_str.data(), numeric_str.data() + numeric_str.size(),
                  num_literal);
  return num_literal;
}

void Parser::tokenize() {
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto match_lineterm = [](std::optional<char32_t> uchar) {
    static const std::array lineterm{std::to_array<std::optional<char32_t>>(
        {std::nullopt, '\n', '\r', 0x2028, 0x2029})};
    return !std::ranges::binary_search(lineterm, uchar);
  };
  for (char32_t leading : traverse_text() | std::views::take_while(does_exist) |
                              std::views::join) {
    if (uc_is_property_white_space(leading))
      continue;
    std::inplace_vector<char32_t, 4> headbuf{leading};
    headbuf.append_range(forward());
    if (std::ranges::equal(headbuf, std::to_array({'/', '/'}))) {
      auto comment = traverse_text() | std::views::take_while(match_lineterm) |
                     std::views::join;
      std::ranges::for_each(comment, [](auto) {});
      backward();
      continue;
    } else
      backward();
    headbuf = {leading};
    headbuf.append_range(forward());
    if (std::ranges::equal(headbuf, std::to_array({'|', '|'}))) {
      last_token = Operator::LOGICAL_DISJUNCT;
      return;
    } else
      backward();
    if (std::ranges::binary_search(std::to_array({'"', '\'', '`'}), leading)) {
      last_token = tokenize_string_literal(leading);
      return;
    }
    if (uc_is_property_xid_start(leading) || leading == '_') {
      last_token = tokenize_identifier(leading);
      return;
    }
    static const std::array legal_punct{std::to_array<char32_t>(
        {'(', ')', '*', '+', '-', '.', '/', ':', ';', '=', '{', '}'})};
    if (std::ranges::binary_search(legal_punct, leading)) {
      last_token = leading;
      return;
    }
    if (std::isdigit(leading)) {
      last_token = tokenize_numeric_literal(leading);
      return;
    }
    throw ScriptError{UnexpectedToken{}};
  }
  last_token = std::monostate{};
}

void Parser::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw ScriptError{MissingPunctuation{must_be}};
}

void Parser::parse_text() {
  std::shared_ptr definition{std::make_shared<FunctionDefinition>()};
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      break;
    parse_statement(*definition);
  }
  function_definitions.push_back(definition);
  main_function->definition = definition.get();
  main_function->own_scope.resize(definition->local_scope.size());
  initialize(*main_function);
}

void Parser::parse_statement(FunctionDefinition &definition) {
  Keyword *word_ptr{std::get_if<Keyword>(&last_token)};
  if (word_ptr && *word_ptr == Keyword::K_FUNCTION) {
    const FunctionDefinition *nested_definition{parse_function_decl()};
    std::optional<Identifier> function_name{nested_definition->function_name};
    if (not function_name)
      throw ScriptError{MissingFunctionName{}};
    if (std::ranges::binary_search(definition.local_scope, *function_name))
      throw ScriptError{DuplicateDeclaration{}};
    definition.nested_functions.push_back({*function_name, nested_definition});
    auto lower_bound =
        std::ranges::lower_bound(definition.local_scope, *function_name);
    definition.local_scope.insert(lower_bound, *function_name);
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
    tokenize();
    ReturnStatement statement{parse_expression()};
    definition.body.push_back(std::move(statement));
    assert_punct(';');
    return;
  }
  definition.body.push_back(parse_expression());
  assert_punct(';');
}

const FunctionDefinition *Parser::parse_function_decl() {
  tokenize();
  Identifier *function_name{std::get_if<Identifier>(&last_token)};
  FunctionDefinition definition{};
  if (function_name) {
    definition.function_name = *function_name;
    tokenize();
  }
  assert_punct('(');
  while (1) {
    tokenize();
    if (last_token == Token{U')'})
      break;
    Identifier *parameter{std::get_if<Identifier>(&last_token)};
    if (not parameter)
      throw ScriptError{MissingFormalParameter{}};
    definition.arguments.push_back(*parameter);
    tokenize();
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  tokenize();
  assert_punct('{');
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      throw ScriptError{MissingPunctuation{'}'}};
    char32_t *alter_ptr{std::get_if<char32_t>(&last_token)};
    if (alter_ptr && *alter_ptr == '}')
      break;
    parse_statement(definition);
  }
  std::shared_ptr definition_ptr{
      std::make_shared<FunctionDefinition>(std::move(definition))};
  function_definitions.push_back(definition_ptr);
  return definition_ptr.get();
}

VariableDeclaration Parser::parse_variable_decl() {
  tokenize();
  Identifier *variable_name{std::get_if<Identifier>(&last_token)};
  if (not variable_name)
    throw ScriptError{MissingVariableName{}};
  VariableDeclaration variable_decl{*variable_name};
  tokenize();
  assert_punct('=');
  tokenize();
  variable_decl.initializer = parse_expression();
  assert_punct(';');
  return variable_decl;
}

Expression Parser::parse_expression() { return parse_assign_expr(); }

Expression Parser::parse_primary_expr() {
  return last_token.visit([this](auto t) -> Expression {
    if constexpr (std::is_same_v<decltype(t), std::u16string_view> ||
                  std::is_same_v<decltype(t), Identifier> ||
                  std::is_same_v<decltype(t), std::int64_t> ||
                  std::is_same_v<decltype(t), double>)
      return t;
    if constexpr (std::is_same_v<decltype(t), Keyword>)
      if (t == Keyword::K_FUNCTION)
        return parse_function_decl();
    throw ScriptError{UnexpectedToken{}};
  });
}

Expression Parser::parse_postfix_expr() {
  using PostfixReducer =
      std::optional<std::copyable_function<Expression(Expression) const>>;
  auto match_reducer = [this](auto t) -> PostfixReducer {
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
  Expression postfix_expr{parse_primary_expr()};
  while (1) {
    tokenize();
    PostfixReducer postfix_reducer{last_token.visit(match_reducer)};
    if (not postfix_reducer)
      break;
    postfix_expr = postfix_fold(postfix_expr, *postfix_reducer);
  }
  return postfix_expr;
}

Expression Parser::parse_additive_expr() {
  Expression expr_left{parse_postfix_expr()};
  if (last_token == Token{U'+'} || last_token == Token{U'-'}) {
    char32_t binary_op{std::get<char32_t>(last_token)};
    tokenize();
    std::shared_ptr expr_ptr{std::make_shared<ReferentialExpression>(
        BinaryExpression{expr_left, parse_postfix_expr(), binary_op})};
    referential_expressions.push_back(expr_ptr);
    return expr_ptr.get();
  }
  return expr_left;
}

Expression Parser::parse_logical_disjunct() {
  Expression expr_left{parse_additive_expr()};
  while (last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    Operator op{std::get<Operator>(last_token)};
    tokenize();
    std::shared_ptr expr_ptr{std::make_shared<ReferentialExpression>(
        LogicalExpression{expr_left, parse_additive_expr(), op})};
    referential_expressions.push_back(expr_ptr);
    expr_left = expr_ptr.get();
  }
  return expr_left;
}

Expression Parser::parse_assign_expr() {
  Expression lhs_expr{parse_logical_disjunct()};
  if (last_token != Token{U'='})
    return lhs_expr;
  tokenize();
  std::shared_ptr expr_ptr{std::make_shared<ReferentialExpression>(
      AssignExpression{lhs_expr, parse_assign_expr()})};
  referential_expressions.push_back(expr_ptr);
  return expr_ptr.get();
}

Expression Parser::parse_member_expr(Expression obj_expr) {
  tokenize();
  Identifier *field_name{std::get_if<Identifier>(&last_token)};
  if (not field_name)
    throw ScriptError{MissingFieldName{}};
  std::shared_ptr expr_ptr{std::make_shared<ReferentialExpression>(
      MemberExpression{obj_expr, *field_name})};
  referential_expressions.push_back(expr_ptr);
  return expr_ptr.get();
}

Expression Parser::parse_call_expr(Expression callee_expr) {
  std::vector<Expression> arguments{};
  while (1) {
    tokenize();
    if (last_token == Token{U')'})
      break;
    arguments.push_back(parse_expression());
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  std::shared_ptr expr_ptr{std::make_shared<ReferentialExpression>(
      FunctionCallExpression{callee_expr, std::move(arguments)})};
  referential_expressions.push_back(expr_ptr);
  return expr_ptr.get();
}

Script::Script()
    : resource{std::make_unique<std::pmr::monotonic_buffer_resource>()},
      function_closures{resource.get()}, object_instances{resource.get()} {
  function_closures.push_back(FunctionClosure{
      .own_scope = std::pmr::vector<std::optional<Dynamic>>(resource.get())});
  main_function = &function_closures.back();
  object_instances.push_back(
      ObjectInstance{.object_shape = shape_global_this,
                     .properties = std::pmr::vector<Dynamic>(
                         {IntrinsicObject::O_CONSOLE}, resource.get())});
  global_this = &object_instances.back();
  object_instances.push_back(
      ObjectInstance{.object_shape = shape_console,
                     .properties = std::pmr::vector<Dynamic>(
                         {IntrinsicFunction::F_LOG}, resource.get())});
  console = &object_instances.back();
}

std::generator<FunctionClosure *>
Script::climb_closure_stack(FunctionClosure *closure_ptr) {
  while (1) {
    co_yield closure_ptr;
    if (not closure_ptr->parent_closure)
      break;
    closure_ptr = closure_ptr->parent_closure;
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

Dynamic *Script::get_property(ObjectInstance &object_instance,
                              Identifier property_handle) {
  std::span<const Identifier> object_shape{object_instance.object_shape};
  auto lower_bound = std::ranges::lower_bound(object_shape, property_handle);
  if (lower_bound == object_shape.end() || *lower_bound != property_handle)
    return nullptr;
  std::ptrdiff_t property_distance{
      std::distance(object_shape.begin(), lower_bound)};
  return std::next(object_instance.properties.data(), property_distance);
}

Dynamic Script::evaluate_property(Identifier property, std::monostate) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  std::u16string_view permanent_string) {
  if (property == Identifier{IDENT_length})
    return static_cast<std::int64_t>(permanent_string.size());
  return {};
}

Dynamic Script::evaluate_property(Identifier property, std::int64_t number) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property, double number) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  ObjectInstance *object_instance) {
  Dynamic *property_ptr{get_property(*object_instance, property)};
  if (not property_ptr)
    return std::monostate{};
  return *property_ptr;
}

Dynamic Script::evaluate_property(Identifier property,
                                  FunctionClosure *closure) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  IntrinsicObject intrinsic_object) {
  if (intrinsic_object == IntrinsicObject::O_CONSOLE)
    return evaluate_property(property, console);
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  IntrinsicFunction intrinsic_function) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &function,
                        const BinaryExpression &expression) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &function,
                        const LogicalExpression &expression) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &closure,
                        const AssignExpression &expression) {
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
                        const FunctionDefinition *definition) {
  return {};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &closure,
                        const ReferentialExpression *expr_ptr) {
  return expr_ptr->alt.visit(
      [&](const auto &expr_alt) { return evaluate_callee(closure, expr_alt); });
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(FunctionClosure &closure,
                        std::u16string_view permanent_string) {
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
std::string Script::evaluate_message(std::u16string_view permanent_string) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(std::int64_t number) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(double number) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(FunctionClosure *closure) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(ObjectInstance *object_instance) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(IntrinsicObject intrinsic_object) {
  return "<unimplemented>";
}
std::string Script::evaluate_message(IntrinsicFunction intrinsic_function) {
  return "<unimplemented>";
}

Dynamic Script::evaluate_function_call(FunctionClosure &closure,
                                       const FunctionCallExpression &expr_call,
                                       Dynamic context,
                                       FunctionClosure *callee_ptr) {
  auto evaluate_argument = [&](Expression expression) {
    return evaluate(closure, expression);
  };
  auto dynamic_arguments = expr_call.arguments |
                           std::views::transform(evaluate_argument) |
                           std::views::enumerate;
  function_closures.push_back(FunctionClosure{*callee_ptr});
  FunctionClosure *copied_callee_ptr{&function_closures.back()};
  initialize(*copied_callee_ptr);
  for (auto [index, argument] : dynamic_arguments) {
    Identifier identifier{callee_ptr->definition->arguments[index]};
    auto *argument_lvalue{get_variable(*copied_callee_ptr, identifier)};
    assert(argument_lvalue != nullptr);
    *argument_lvalue = argument;
  }
  for (Statement statement : callee_ptr->definition->body)
    evaluate(*copied_callee_ptr, statement);
  return copied_callee_ptr->return_val;
}

Dynamic Script::evaluate_function_call(FunctionClosure &closure,
                                       const FunctionCallExpression &expr_call,
                                       Dynamic context,
                                       IntrinsicFunction intrinsic_function) {
  if (intrinsic_function == IntrinsicFunction::F_LOG) {
    auto match_message_alt = [&](auto dynamic_alt) {
      return evaluate_message(dynamic_alt);
    };
    auto match_message_arg = [&](Dynamic argument) {
      return argument.visit(match_message_alt);
    };
    auto evaluate_argument = [&](Expression expression) {
      return evaluate(closure, expression);
    };
    auto message_parts =
        expr_call.arguments | std::views::transform(evaluate_argument) |
        std::views::transform(match_message_arg) | std::views::join_with(' ');
    console_messages.emplace_back(std::from_range, message_parts);
  }
  return {};
}

Dynamic Script::evaluate_operation(char32_t op, std::int64_t lhs,
                                   std::int64_t rhs) {
  if (op == '+')
    return std::int64_t{lhs + rhs};
  if (op == '-')
    return std::int64_t{lhs - rhs};
  std::unreachable();
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const FunctionCallExpression &expr_call) {
  auto visit_expression = [&](auto expression_alt) {
    return evaluate_callee(closure, expression_alt);
  };
  auto [dynamic_context, dynamic_callee] =
      expr_call.callee.visit(visit_expression);
  auto visit_dynamic = [&](auto dynamic_alt) -> Dynamic {
    if constexpr (std::is_same_v<decltype(dynamic_alt), FunctionClosure *> ||
                  std::is_same_v<decltype(dynamic_alt), IntrinsicFunction>)
      return evaluate_function_call(closure, expr_call, dynamic_context,
                                    dynamic_alt);
    throw ScriptError{InvalidFunctionCall{}};
  };
  return dynamic_callee.visit(visit_dynamic);
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const BinaryExpression &expression) {
  Dynamic dynamic_lhs{evaluate(closure, expression.left)};
  Dynamic dynamic_rhs{evaluate(closure, expression.right)};
  auto visit_operands = [&](auto lhs, auto rhs) -> Dynamic {
    if constexpr (std::is_same_v<decltype(lhs), std::int64_t> &&
                  std::is_same_v<decltype(rhs), std::int64_t>)
      return evaluate_operation(expression.op, lhs, rhs);
    return {};
  };
  return std::visit(visit_operands, dynamic_lhs, dynamic_rhs);
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const LogicalExpression &expression) {
  return {};
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const AssignExpression &expression) {
  return {};
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const MemberExpression &expression) {
  Dynamic dynamic_object{evaluate(closure, expression.object)};
  auto visit_object = [&](auto dynamic_alt) {
    return evaluate_property(expression.property, dynamic_alt);
  };
  return dynamic_object.visit(visit_object);
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const FunctionDefinition *definition) {
  return {};
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         const ReferentialExpression *expr_ptr) {
  return expr_ptr->alt.visit(
      [&](const auto &expr_alt) { return evaluate(closure, expr_alt); });
}

Dynamic Script::evaluate(FunctionClosure &closure,
                         std::u16string_view permanent_string) {
  return permanent_string;
}

Dynamic Script::evaluate(FunctionClosure &closure, std::int64_t number) {
  return number;
}

Dynamic Script::evaluate(FunctionClosure &closure, double number) {
  return number;
}

Dynamic Script::evaluate(FunctionClosure &closure, Expression expression) {
  return expression.visit(
      [&](auto expression_alt) { return evaluate(closure, expression_alt); });
}

Dynamic Script::evaluate(FunctionClosure &closure, Identifier identifier) {
  std::optional<Dynamic> *from_scope{get_variable(closure, identifier)};
  if (from_scope && *from_scope)
    return **from_scope;
  Dynamic *from_globalThis{get_property(*global_this, identifier)};
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
  closure.return_val = evaluate(closure, statement.argument);
}

void Script::evaluate(FunctionClosure &closure, Statement statement) {
  std::visit([&](auto alternative) { evaluate(closure, alternative); },
             statement);
}

void Script::initialize(FunctionClosure &closure) {
  const FunctionDefinition &definition{*closure.definition};
  for (auto [nested_name, nested_definition] : definition.nested_functions) {
    auto *function_lvalue{get_variable(closure, nested_name)};
    assert(function_lvalue != nullptr);
    function_closures.push_back(FunctionClosure{
        .definition = nested_definition,
        .parent_closure = &closure,
        .own_scope = std::pmr::vector<std::optional<Dynamic>>(
            nested_definition->local_scope.size(), resource.get())});
    *function_lvalue = &function_closures.back();
  }
}

void Script::evaluate() {
  for (Statement statement : main_function->definition->body)
    evaluate(*main_function, statement);
}
} // namespace Manadrain
