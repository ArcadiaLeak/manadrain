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
    {"switch", Keyword::K_SWITCH},     {"typeof", Keyword::K_TYPEOF}};
static const std::unordered_map<std::string_view, std::size_t> identifier_atlas{
    {"console", OFFSET_console},
    {"log", OFFSET_log},
    {"length", OFFSET_length}};
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
        {'(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '=', '{', '}'})};
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
  FunctionDefinition *definition{new FunctionDefinition{}};
  current_function = definition;
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      break;
    parse_statement();
  }
  function_definitions.emplace_back(definition);
  FunctionFrame &frame{function_frames.emplace_back()};
  frame.definition = definition;
  frame.own_scope =
      function_scopes.emplace_back(definition->local_scope.size());
  current_frame = &frame;
  initialize(frame);
}

void Parser::parse_statement() {
  Keyword *word_ptr{std::get_if<Keyword>(&last_token)};
  if (word_ptr && *word_ptr == Keyword::K_FUNCTION) {
    const FunctionDefinition *nested_definition{parse_function_decl()};
    std::optional<Identifier> function_name{nested_definition->function_name};
    if (not function_name)
      throw ScriptError{MissingFunctionName{}};
    if (std::ranges::binary_search(current_function->local_scope,
                                   *function_name))
      throw ScriptError{DuplicateDeclaration{}};
    current_function->nested_functions.push_back(
        {*function_name, nested_definition});
    auto lower_bound =
        std::ranges::lower_bound(current_function->local_scope, *function_name);
    current_function->local_scope.insert(lower_bound, *function_name);
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_LET) {
    WriteVariable statement{parse_variable_decl()};
    Identifier variable_name{statement.variable_name};
    if (std::ranges::binary_search(current_function->local_scope,
                                   variable_name))
      throw ScriptError{DuplicateDeclaration{}};
    auto lower_bound =
        std::ranges::lower_bound(current_function->local_scope, variable_name);
    current_function->local_scope.insert(lower_bound, variable_name);
    current_function->body.push_back(statement);
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_RETURN) {
    tokenize();
    ReturnStatement statement{parse_expression()};
    assert_punct(';');
    current_function->body.push_back(statement);
    return;
  }
  Expression expression{parse_expression()};
  assert_punct(';');
  current_function->body.push_back(expression);
  return;
}

const FunctionDefinition *Parser::parse_function_decl() {
  tokenize();
  Identifier *function_name{std::get_if<Identifier>(&last_token)};
  FunctionDefinition *definition{new FunctionDefinition{}};
  if (function_name) {
    definition->function_name = *function_name;
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
    auto lower_bound =
        std::ranges::lower_bound(definition->local_scope, *parameter);
    definition->local_scope.insert(lower_bound, *parameter);
    definition->arguments.push_back(*parameter);
    tokenize();
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  tokenize();
  assert_punct('{');
  std::swap(current_function, definition);
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      throw ScriptError{MissingPunctuation{'}'}};
    char32_t *alter_ptr{std::get_if<char32_t>(&last_token)};
    if (alter_ptr && *alter_ptr == '}')
      break;
    parse_statement();
  }
  std::swap(current_function, definition);
  function_definitions.emplace_back(definition);
  return definition;
}

WriteVariable Parser::parse_variable_decl() {
  tokenize();
  Identifier *variable_name{std::get_if<Identifier>(&last_token)};
  if (not variable_name)
    throw ScriptError{MissingVariableName{}};
  WriteVariable variable_decl{*variable_name};
  tokenize();
  assert_punct('=');
  tokenize();
  variable_decl.rvalue = parse_expression();
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
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == '{')
        return parse_object_literal();
    throw ScriptError{UnexpectedToken{}};
  });
}

Expression Parser::parse_paren_expr() {
  assert_punct('(');
  tokenize();
  Expression expression{parse_expression()};
  assert_punct(')');
  tokenize();
  return expression;
}

Expression Parser::parse_object_literal() {
  std::vector<std::pair<Identifier, Expression>> prop_vec{};
  tokenize();
  while (last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(last_token))
      throw ScriptError{InvalidPropertyName{}};
    Identifier prop_key{std::get<Identifier>(last_token)};
    tokenize();
    if (last_token == Token{U':'}) {
      tokenize();
      prop_vec.emplace_back(prop_key, parse_expression());
    } else
      prop_vec.emplace_back(prop_key, std::monostate{});
    if (last_token != Token{U','})
      break;
    tokenize();
  }
  assert_punct('}');
  auto pluck_identifier = [](const auto &prop_pair) { return prop_pair.first; };
  std::vector<Identifier> shape_vec{
      std::from_range, prop_vec | std::views::transform(pluck_identifier)};
  std::shared_ptr object_shape{
      std::make_shared<ObjectShape>(std::move(shape_vec))};
  std::ranges::sort(object_shape->properties);
  object_shapes.push_back(object_shape);
  std::shared_ptr expr_ptr{std::make_shared<ReferentialExpression>(
      ObjectExpression{object_shape.get(), std::move(prop_vec)})};
  referential_expressions.push_back(expr_ptr);
  return expr_ptr.get();
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
      FunctionCallExpression{callee_expr, arguments.size()})};
  referential_expressions.push_back(expr_ptr);
  return expr_ptr.get();
}

Script::Script()
    : resource{std::make_unique<std::pmr::monotonic_buffer_resource>()},
      function_frames{resource.get()}, function_scopes{resource.get()},
      object_instances{resource.get()}, object_properties{resource.get()} {}

std::optional<Dynamic> *FunctionFrame::get_variable(Identifier var_handle) {
  const auto &local_scope{definition->local_scope};
  auto lower_bound = std::ranges::lower_bound(local_scope, var_handle);
  if (lower_bound == local_scope.end() || *lower_bound != var_handle)
    return closure ? closure->get_variable(var_handle) : nullptr;
  std::ptrdiff_t own_scope_distance{
      std::distance(local_scope.begin(), lower_bound)};
  std::optional<Dynamic> *own_scope_data{own_scope.data()};
  return std::next(own_scope_data, own_scope_distance);
}

Dynamic *VanillaObject::get_property(Identifier property) {
  std::span<const Identifier> shape_view{object_shape->properties};
  auto lower_bound = std::ranges::lower_bound(shape_view, property);
  if (lower_bound == shape_view.end() || *lower_bound != property)
    return nullptr;
  std::ptrdiff_t property_distance{
      std::distance(shape_view.begin(), lower_bound)};
  return std::next(properties.data(), property_distance);
}

Dynamic *GlobalObject::get_property(Identifier property) {
  if (property == Identifier{OFFSET_console})
    return &console;
  return {};
}

Dynamic *ConsoleObject::get_property(Identifier property) {
  if (property == Identifier{OFFSET_log})
    return &log;
  return {};
}

Dynamic Script::evaluate_property(Identifier property, std::monostate) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  std::u16string_view permanent_string) {
  if (property == Identifier{OFFSET_length})
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
  Dynamic *property_ptr{object_instance->get_property(property)};
  if (not property_ptr)
    return std::monostate{};
  return *property_ptr;
}

Dynamic Script::evaluate_property(Identifier property, FunctionFrame *frame) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  IntrinsicFunction intrinsic_function) {
  return {};
}

std::pair<Dynamic, Dynamic> Script::evaluate_callee(Identifier identifier) {
  return {std::monostate{}, evaluate(identifier)};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(const MemberExpression &expression) {
  Dynamic dynamic_object{evaluate(expression.object)};
  auto visit_object = [&](auto dynamic_alt) {
    return evaluate_property(expression.property, dynamic_alt);
  };
  Dynamic dynamic_property{dynamic_object.visit(visit_object)};
  return {dynamic_object, dynamic_property};
}

std::pair<Dynamic, Dynamic>
Script::evaluate_callee(const ReferentialExpression *expr_ptr) {
  auto visit_expression = [&](const auto &expr_alt) {
    if constexpr (std::is_same_v<decltype(expr_alt), const MemberExpression &>)
      return evaluate_callee(expr_alt);
    return std::pair<Dynamic, Dynamic>{};
  };
  return expr_ptr->alt.visit(visit_expression);
}

std::u16string Script::evaluate_message(std::monostate) { return u"undefined"; }
std::u16string Script::evaluate_message(std::u16string_view permanent_string) {
  return std::u16string(permanent_string);
}
std::u16string Script::evaluate_message(std::int64_t number) {
  static constexpr int buffer_size =
      std::numeric_limits<std::int64_t>::digits10 + 2;
  std::array<char, buffer_size> buffer{};
  auto [ptr, ec] =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
  assert(ec == std::errc{});
  std::size_t message_size{
      static_cast<std::size_t>(std::distance(buffer.data(), ptr))};
  std::string_view message_view{buffer.data(), message_size};
  return {std::from_range, message_view};
}
std::u16string Script::evaluate_message(double number) {
  return u"<unimplemented>";
}
std::u16string Script::evaluate_message(FunctionFrame *frame) {
  return u"<unimplemented>";
}
std::u16string Script::evaluate_message(ObjectInstance *object_instance) {
  return u"<unimplemented>";
}
std::u16string Script::evaluate_message(IntrinsicFunction intrinsic_function) {
  return u"<unimplemented>";
}

Dynamic Script::evaluate_function_call(const FunctionCallExpression &expr_call,
                                       Dynamic context,
                                       FunctionFrame *callee_ptr) {
  FunctionFrame &copied_callee{function_frames.emplace_back()};
  copied_callee.closure = callee_ptr->closure;
  copied_callee.definition = callee_ptr->definition;
  copied_callee.own_scope =
      function_scopes.emplace_back(callee_ptr->definition->local_scope.size());
  initialize(copied_callee);
  std::size_t arguments_begin{current_frame->program_count};
  while (current_frame->program_count <
         arguments_begin + expr_call.passed_arguments) {
    std::size_t argument_index{current_frame->program_count - arguments_begin};
    Identifier identifier{callee_ptr->definition->arguments[argument_index]};
    auto *argument_lvalue{current_frame->get_variable(identifier)};
    assert(argument_lvalue != nullptr);
    const Expression *argument_expr{std::get_if<Expression>(
        &current_frame->definition->body[current_frame->program_count++])};
    assert(argument_expr != nullptr);
    *argument_lvalue = evaluate(*argument_expr);
  }
  FunctionFrame *caller_frame{current_frame};
  current_frame = &copied_callee;
  while (current_frame->program_count < callee_ptr->definition->body.size())
    evaluate(callee_ptr->definition->body[current_frame->program_count++]);
  current_frame = caller_frame;
  return copied_callee.return_val;
}

Dynamic Script::evaluate_function_call(const FunctionCallExpression &expr_call,
                                       Dynamic context,
                                       IntrinsicFunction intrinsic_function) {
  if (intrinsic_function == IntrinsicFunction::F_LOG) {
    std::vector<Dynamic> dynamic_arguments{expr_call.passed_arguments};
    std::size_t arguments_begin{current_frame->program_count};
    while (current_frame->program_count <
           arguments_begin + expr_call.passed_arguments) {
      std::size_t argument_index{current_frame->program_count -
                                 arguments_begin};
      const Expression *argument_expr{std::get_if<Expression>(
          &current_frame->definition->body[current_frame->program_count++])};
      assert(argument_expr != nullptr);
      dynamic_arguments[argument_index] = evaluate(*argument_expr);
    }
    auto match_message_alt = [&](auto dynamic_alt) {
      return evaluate_message(dynamic_alt);
    };
    auto match_message_arg = [&](Dynamic argument) {
      return argument.visit(match_message_alt);
    };
    auto message_parts = dynamic_arguments |
                         std::views::transform(match_message_arg) |
                         std::views::join_with(' ');
    {
      std::lock_guard console_lock{*console_mutex};
      console_messages.emplace_back(
          std::u16string{std::from_range, message_parts});
    }
    console_condition->notify_one();
  }
  return {};
}

Dynamic Script::evaluate_operation(char32_t op, std::int64_t lhs,
                                   std::int64_t rhs) {
  if (op == '+')
    return lhs + rhs;
  if (op == '-')
    return lhs - rhs;
  std::unreachable();
}

Dynamic Script::evaluate_operation(Operator op, std::int64_t lhs,
                                   std::int64_t rhs) {
  if (op == Operator::LOGICAL_DISJUNCT)
    return lhs ? lhs : rhs;
  std::unreachable();
}

Dynamic Script::evaluate(const FunctionCallExpression &expr_call) {
  auto visit_expression = [this](auto expression_alt) {
    if constexpr (std::is_same_v<decltype(expression_alt), Identifier> ||
                  std::is_same_v<decltype(expression_alt),
                                 const ReferentialExpression *>)
      return evaluate_callee(expression_alt);
    return std::pair<Dynamic, Dynamic>{};
  };
  auto [dynamic_context, dynamic_callee] =
      expr_call.callee.visit(visit_expression);
  auto visit_dynamic = [&](auto dynamic_alt) -> Dynamic {
    if constexpr (std::is_same_v<decltype(dynamic_alt), FunctionFrame *> ||
                  std::is_same_v<decltype(dynamic_alt), IntrinsicFunction>)
      return evaluate_function_call(expr_call, dynamic_context, dynamic_alt);
    throw ScriptError{InvalidFunctionCall{}};
  };
  return dynamic_callee.visit(visit_dynamic);
}

Dynamic Script::evaluate(const BinaryExpression &expression) {
  Dynamic dynamic_lhs{evaluate(expression.left)};
  Dynamic dynamic_rhs{evaluate(expression.right)};
  auto visit_operands = [&](auto lhs, auto rhs) -> Dynamic {
    if constexpr (std::is_same_v<decltype(lhs), std::int64_t> &&
                  std::is_same_v<decltype(rhs), std::int64_t>)
      return evaluate_operation(expression.op, lhs, rhs);
    return {};
  };
  return std::visit(visit_operands, dynamic_lhs, dynamic_rhs);
}

Dynamic Script::evaluate(const LogicalExpression &expression) {
  Dynamic dynamic_lhs{evaluate(expression.left)};
  Dynamic dynamic_rhs{evaluate(expression.right)};
  auto visit_operands = [&](auto lhs, auto rhs) -> Dynamic {
    if constexpr (std::is_same_v<decltype(lhs), std::int64_t> &&
                  std::is_same_v<decltype(rhs), std::int64_t>)
      return evaluate_operation(expression.op, lhs, rhs);
    return {};
  };
  return std::visit(visit_operands, dynamic_lhs, dynamic_rhs);
}

Dynamic Script::evaluate(const AssignExpression &assign_expr) {
  auto evaluate_lvalue = [this](auto expression) -> std::optional<Dynamic> * {
    if constexpr (std::is_same_v<decltype(expression), Identifier>)
      return current_frame->get_variable(expression);
    return nullptr;
  };
  std::optional<Dynamic> *assign_lvalue{
      assign_expr.left.visit(evaluate_lvalue)};
  assert(assign_lvalue != nullptr);
  *assign_lvalue = evaluate(assign_expr.right);
  return {};
}

Dynamic Script::evaluate(const ObjectExpression &object_expr) {
  VanillaObject &instance{object_instances.emplace_back()};
  instance.object_shape = object_expr.object_shape;
  instance.properties = object_properties.emplace_back(
      object_expr.object_shape->properties.size());
  for (auto [identifier, expression] : object_expr.properties) {
    Dynamic *property_ptr{instance.get_property(identifier)};
    *property_ptr = evaluate(expression);
  }
  return &instance;
}

Dynamic Script::evaluate(const MemberExpression &expression) {
  Dynamic dynamic_object{evaluate(expression.object)};
  auto visit_object = [&](auto dynamic_alt) {
    return evaluate_property(expression.property, dynamic_alt);
  };
  return dynamic_object.visit(visit_object);
}

Dynamic Script::evaluate(const FunctionDefinition *definition) {
  FunctionFrame &frame{function_frames.emplace_back()};
  frame.definition = definition;
  frame.closure = current_frame;
  frame.own_scope =
      function_scopes.emplace_back(definition->local_scope.size());
  return &frame;
}

Dynamic Script::evaluate(const ReferentialExpression *expr_ptr) {
  return expr_ptr->alt.visit(
      [this](const auto &expr_alt) { return evaluate(expr_alt); });
}

Dynamic Script::evaluate(std::u16string_view permanent_string) {
  return permanent_string;
}

Dynamic Script::evaluate(std::int64_t number) { return number; }

Dynamic Script::evaluate(double number) { return number; }

Dynamic Script::evaluate(Expression expression) {
  return expression.visit(
      [this](auto expression_alt) { return evaluate(expression_alt); });
}

Dynamic Script::evaluate(Identifier identifier) {
  std::optional<Dynamic> *from_scope{current_frame->get_variable(identifier)};
  if (from_scope && *from_scope)
    return **from_scope;
  Dynamic *from_globalThis{global_this.get_property(identifier)};
  if (from_globalThis)
    return *from_globalThis;
  throw ScriptError{InvalidVariableAccess{}};
}

void Script::evaluate_statement(Expression expression) { evaluate(expression); }

void Script::evaluate_statement(WriteVariable statement) {
  Dynamic rvalue_dynamic{evaluate(statement.rvalue)};
  std::optional<Dynamic> *variable_lvalue{
      current_frame->get_variable(statement.variable_name)};
  assert(variable_lvalue != nullptr);
  *variable_lvalue = rvalue_dynamic;
}

void Script::evaluate_statement(ReturnStatement statement) {
  current_frame->return_val = evaluate(statement.argument);
}

void Script::evaluate(Statement statement) {
  statement.visit([&](auto alternative) { evaluate_statement(alternative); });
}

void Script::initialize(FunctionFrame &frame) {
  const FunctionDefinition &definition{*frame.definition};
  for (auto [nested_name, nested_definition] : definition.nested_functions) {
    auto *function_lvalue{frame.get_variable(nested_name)};
    assert(function_lvalue != nullptr);
    FunctionFrame &nested_frame{function_frames.emplace_back()};
    nested_frame.definition = nested_definition;
    nested_frame.closure = &frame;
    nested_frame.own_scope =
        function_scopes.emplace_back(nested_definition->local_scope.size());
    *function_lvalue = &function_frames.back();
  }
}

void Script::evaluate() {
  for (Statement statement : current_frame->definition->body)
    evaluate(statement);
}

void Script::collect_console_messages(std::stop_token stopper,
                                      std::list<ConsoleMessage> &message_box) {
  auto check_messages = [&] { return console_messages.size() > 0; };
  std::unique_lock console_lock{*console_mutex};
  console_condition->wait(console_lock, stopper, check_messages);
  console_messages.swap(message_box);
}

static std::inplace_vector<std::uint8_t, 3>
encode_u16_for_print(std::uint16_t uchar) {
  std::array<std::uint8_t, 3> buffer{};
  std::size_t buffer_len{buffer.size()};
  uint8_t *result = u16_to_u8(&uchar, 1, buffer.data(), &buffer_len);
  assert(result != nullptr);
  return {std::from_range, buffer | std::views::take(buffer_len)};
}

std::string ConsoleMessage::encode_for_print() const {
  std::string u8_message{};
  for (char16_t uchar : content)
    u8_message.append_range(encode_u16_for_print(uchar));
  return u8_message;
}
} // namespace Manadrain
