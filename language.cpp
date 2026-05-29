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
  frame.initialize();
}

Unit Parser::parse_object_literal() {
  std::vector<std::pair<Identifier, Unit>> properties{};
  tokenize();
  while (last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(last_token))
      throw ScriptError{InvalidPropertyName{}};
    Identifier property_key{std::get<Identifier>(last_token)};
    tokenize();
    if (last_token == Token{U':'}) {
      tokenize();
      properties.emplace_back(property_key, parse_expression());
    } else
      properties.emplace_back(property_key, std::monostate{});
    if (last_token != Token{U','})
      break;
    tokenize();
  }
  assert_punct('}');
  auto shape_view = std::ranges::transform_view{
      properties, [](const auto &prop_pair) { return prop_pair.first; }};
  auto statement_view = std::ranges::transform_view{
      properties, [](const auto &prop_pair) {
        return InitializeVariable{prop_pair.first, prop_pair.second};
      }};
  std::shared_ptr object_shape{
      std::make_shared<ObjectShape>(std::vector{std::from_range, shape_view})};
  std::ranges::sort(object_shape->properties);
  object_shapes.push_back(object_shape);
  current_function->program.push_back(
      ObjectExpression{object_shape.get(), properties.size()});
  current_function->program.append_range(statement_view);
  return Interim{};
}

Unit Parser::parse_primary_expr() {
  return last_token.visit([this](auto t) -> Unit {
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

Unit Parser::parse_call_expr(Unit context, Unit callee) {
  std::size_t passed_arguments{};
  while (1) {
    tokenize();
    if (last_token == Token{U')'})
      break;
    current_function->program.push_back(parse_expression());
    ++passed_arguments;
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  current_function->program.push_back(
      FunctionCallExpression{context, callee, passed_arguments});
  tokenize();
  return parse_postfix_expr(Interim{});
}

Unit Parser::parse_member_expr(Unit object) {
  tokenize();
  Identifier *field_name{std::get_if<Identifier>(&last_token)};
  if (not field_name)
    throw ScriptError{MissingFieldName{}};
  current_function->program.push_back(MemberExpression{object, *field_name});
  tokenize();
  return last_token == Token{U'('} ? parse_call_expr(object, Interim{})
                                   : parse_postfix_expr(Interim{});
}

Unit Parser::parse_postfix_expr(Unit base_unit) {
  if (last_token == Token{U'.'})
    return parse_member_expr(base_unit);
  if (last_token == Token{U'('})
    return parse_call_expr(std::monostate{}, base_unit);
  return base_unit;
}

Unit Parser::parse_postfix_expr() {
  Unit base_unit{parse_primary_expr()};
  tokenize();
  return parse_postfix_expr(base_unit);
}

Unit Parser::parse_additive_expr() {
  Unit expr_left{parse_postfix_expr()};
  if (last_token == Token{U'+'} || last_token == Token{U'-'}) {
    char32_t binary_op{std::get<char32_t>(last_token)};
    tokenize();
    current_function->program.push_back(
        BinaryExpression{expr_left, parse_postfix_expr(), binary_op});
    return Interim{};
  }
  return expr_left;
}

Unit Parser::parse_logical_disjunct() {
  Unit expr_left{parse_additive_expr()};
  while (last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    Operator op{std::get<Operator>(last_token)};
    tokenize();
    current_function->program.push_back(
        LogicalExpression{expr_left, parse_additive_expr(), op});
    expr_left = Interim{};
  }
  return expr_left;
}

Unit Parser::parse_assign_expr() {
  Unit lhs_expr{parse_logical_disjunct()};
  if (last_token != Token{U'='})
    return lhs_expr;
  tokenize();
  current_function->program.push_back(
      AssignExpression{lhs_expr, parse_assign_expr()});
  return Interim{};
}

Unit Parser::parse_expression() { return parse_assign_expr(); }

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
    InitializeVariable statement{parse_variable_decl()};
    Identifier variable_name{statement.variable_name};
    if (std::ranges::binary_search(current_function->local_scope,
                                   variable_name))
      throw ScriptError{DuplicateDeclaration{}};
    auto lower_bound =
        std::ranges::lower_bound(current_function->local_scope, variable_name);
    current_function->local_scope.insert(lower_bound, variable_name);
    current_function->program.push_back(statement);
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_RETURN) {
    tokenize();
    ReturnStatement statement{parse_expression()};
    assert_punct(';');
    current_function->program.push_back(statement);
    return;
  }
  ExpressionStatement expression{parse_expression()};
  assert_punct(';');
  current_function->program.push_back(expression);
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

InitializeVariable Parser::parse_variable_decl() {
  tokenize();
  Identifier *variable_name{std::get_if<Identifier>(&last_token)};
  if (not variable_name)
    throw ScriptError{MissingVariableName{}};
  InitializeVariable variable_init{*variable_name};
  tokenize();
  assert_punct('=');
  tokenize();
  variable_init.rvalue = parse_expression();
  assert_punct(';');
  return variable_init;
}

Script::Script()
    : resource{std::make_unique<std::pmr::monotonic_buffer_resource>()},
      function_frames{resource.get()}, function_scopes{resource.get()},
      object_instances{resource.get()}, object_properties{resource.get()},
      interim{resource.get()} {}

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

void FunctionFrame::initialize() {
  for (auto [nested_name, nested_definition] : definition->nested_functions) {
    auto *function_lvalue{get_variable(nested_name)};
    assert(function_lvalue != nullptr);
    *function_lvalue = FunctionReference{nested_definition, this};
  }
}

Dynamic UnitVisitor::operator()(std::monostate primitive) { return primitive; }

Dynamic UnitVisitor::operator()(std::u16string_view primitive) {
  return primitive;
}

Dynamic UnitVisitor::operator()(std::int64_t primitive) { return primitive; }

Dynamic UnitVisitor::operator()(double primitive) { return primitive; }

Dynamic UnitVisitor::operator()(Interim) {
  return script.interim.rbegin()[level];
}

Dynamic UnitVisitor::operator()(Identifier identifier) {
  std::optional<Dynamic> *from_scope{
      script.current_frame->get_variable(identifier)};
  if (from_scope && *from_scope)
    return **from_scope;
  Dynamic *from_globalThis{script.global_this.get_property(identifier)};
  if (from_globalThis)
    return *from_globalThis;
  throw ScriptError{InvalidVariableAccess{}};
}

Dynamic ExpressionVisitor::operator()(Unit unit) {
  return unit.visit(UnitVisitor{script});
}

Dynamic ExpressionVisitor::operator()(BinaryExpression expression) {
  Dynamic dynamic_lhs{expression.left.visit(UnitVisitor{script, 1})};
  Dynamic dynamic_rhs{expression.right.visit(UnitVisitor{script})};
  auto visit_operands = [&](auto lhs, auto rhs) -> Dynamic {
    if constexpr (std::is_same_v<decltype(lhs), std::int64_t> &&
                  std::is_same_v<decltype(rhs), std::int64_t>)
      return script.evaluate_operation(expression.op, lhs, rhs);
    else
      return {};
  };
  return std::visit(visit_operands, dynamic_lhs, dynamic_rhs);
}

Dynamic ExpressionVisitor::operator()(LogicalExpression expression) {
  Dynamic dynamic_lhs{expression.left.visit(UnitVisitor{script, 1})};
  Dynamic dynamic_rhs{expression.right.visit(UnitVisitor{script})};
  auto visit_operands = [&](auto lhs, auto rhs) -> Dynamic {
    if constexpr (std::is_same_v<decltype(lhs), std::int64_t> &&
                  std::is_same_v<decltype(rhs), std::int64_t>)
      return script.evaluate_operation(expression.op, lhs, rhs);
    else
      return {};
  };
  return std::visit(visit_operands, dynamic_lhs, dynamic_rhs);
}

Dynamic ExpressionVisitor::operator()(MemberExpression expression) {
  Dynamic dynamic_object{expression.object.visit(UnitVisitor{script, 1})};
  auto visit_object = [&](auto dynamic_alt) {
    return script.evaluate_property(expression.property, dynamic_alt);
  };
  return dynamic_object.visit(visit_object);
}

Dynamic ExpressionVisitor::operator()(FunctionCallExpression) {
  return std::monostate{};
}

Dynamic ExpressionVisitor::operator()(AssignExpression assign_expr) {
  Identifier *assign_identifier{std::get_if<Identifier>(&assign_expr.left)};
  std::optional<Dynamic> *assign_lvalue{
      script.current_frame->get_variable(*assign_identifier)};
  assert(assign_lvalue != nullptr);
  *assign_lvalue = assign_expr.right.visit(UnitVisitor{script});
  return {};
}

Dynamic ExpressionVisitor::operator()(ObjectExpression) {
  return std::monostate{};
}

Dynamic UnitVisitor::operator()(const FunctionDefinition *definition) {
  return FunctionReference{definition, script.current_frame};
}

void StatementVisitor::operator()(Expression expression) {
  script.interim.push_back(expression.visit(ExpressionVisitor{script}));
}

void StatementVisitor::operator()(ExpressionStatement statement) {
  statement.argument.visit(ExpressionVisitor{script});
}

void StatementVisitor::operator()(InitializeVariable statement) {
  Dynamic rvalue_dynamic{statement.rvalue.visit(UnitVisitor{script})};
  std::optional<Dynamic> *variable_lvalue{
      script.current_frame->get_variable(statement.variable_name)};
  assert(variable_lvalue != nullptr);
  *variable_lvalue = rvalue_dynamic;
}

void StatementVisitor::operator()(ReturnStatement statement) {
  script.current_frame->return_val =
      statement.argument.visit(UnitVisitor{script});
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

Dynamic Script::evaluate_property(Identifier property,
                                  FunctionReference reference) {
  return {};
}

Dynamic Script::evaluate_property(Identifier property,
                                  IntrinsicFunction intrinsic_function) {
  return {};
}

void Script::evaluate() {
  while (current_frame->program_count <
         current_frame->definition->program.size())
    current_frame->definition->program[current_frame->program_count++].visit(
        StatementVisitor{*this});
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
