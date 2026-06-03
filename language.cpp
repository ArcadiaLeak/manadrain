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
      std::make_shared<CompactString>(std::move(literal_str)));
  std::u16string_view permanent_view{
      std::get<std::u16string>(*permanent_strings.back())};
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
  definition->definition_idx = function_definitions.size();
  function_definitions.emplace_back(definition);
}

Unit Parser::parse_object_literal() {
  std::shared_ptr object_shape{std::make_shared<ObjectShape>()};
  object_shapes.push_back(object_shape);
  Expression &expression{
      current_function->parsed_program.emplace_back().emplace<Expression>()};
  ObjectExpression &object_expression{
      expression.emplace<ObjectExpression>(object_shape.get())};
  tokenize();
  while (last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(last_token))
      throw ScriptError{InvalidPropertyName{}};
    Identifier property_name{std::get<Identifier>(last_token)};
    object_shape->properties.emplace(property_name, std::monostate{});
    ++object_expression.passed_properties;
    tokenize();
    auto initializer_iter = current_function->parsed_program.emplace(
        current_function->parsed_program.end());
    InitializeMember &initialize_member{
        initializer_iter->emplace<InitializeMember>(property_name)};
    if (last_token == Token{U':'}) {
      tokenize();
      initialize_member.rvalue = parse_expression();
    }
    if (last_token != Token{U','})
      break;
    tokenize();
  }
  assert_punct('}');
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

Expression Parser::parse_call_expr(Unit callee) {
  std::size_t passed_arguments{};
  while (1) {
    tokenize();
    if (last_token == Token{U')'})
      break;
    Unit argument_unit{parse_expression()};
    if (not std::holds_alternative<Interim>(argument_unit))
      current_function->parsed_program.emplace_back(argument_unit);
    ++passed_arguments;
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  return FunctionCallExpression{callee, passed_arguments};
}

Expression Parser::parse_member_expr(Unit object) {
  tokenize();
  Identifier *field_name{std::get_if<Identifier>(&last_token)};
  if (not field_name)
    throw ScriptError{MissingFieldName{}};
  return MemberExpression{object, *field_name};
}

Unit Parser::parse_postfix_expr() {
  auto base_iter = current_function->parsed_program.emplace(
      current_function->parsed_program.end());
  Unit postfix_unit{parse_primary_expr()};
  while (1) {
    tokenize();
    if (last_token == Token{U'.'}) {
      *base_iter = parse_member_expr(postfix_unit);
      base_iter = current_function->parsed_program.emplace(base_iter);
      postfix_unit = Interim{};
    } else if (last_token == Token{U'('}) {
      *base_iter = parse_call_expr(postfix_unit);
      base_iter = current_function->parsed_program.emplace(base_iter);
      postfix_unit = Interim{};
    } else {
      current_function->parsed_program.erase(base_iter);
      return postfix_unit;
    }
  }
}

Unit Parser::parse_additive_expr() {
  auto base_iter = current_function->parsed_program.emplace(
      current_function->parsed_program.end());
  Unit unit_left{parse_postfix_expr()};
  if (last_token == Token{U'+'} || last_token == Token{U'-'}) {
    char32_t binary_op{std::get<char32_t>(last_token)};
    tokenize();
    Unit unit_right{parse_postfix_expr()};
    *base_iter = BinaryExpression{unit_left, unit_right, binary_op};
    return Interim{};
  }
  current_function->parsed_program.erase(base_iter);
  return unit_left;
}

Unit Parser::parse_logical_disjunct() {
  auto base_iter = current_function->parsed_program.emplace(
      current_function->parsed_program.end());
  Unit unit_left{parse_additive_expr()};
  while (last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    Operator op{std::get<Operator>(last_token)};
    tokenize();
    Unit unit_right{parse_additive_expr()};
    unit_left = Interim{};
    *base_iter = LogicalExpression{unit_left, unit_right, op};
    base_iter = current_function->parsed_program.emplace(base_iter);
  }
  current_function->parsed_program.erase(base_iter);
  return unit_left;
}

Unit Parser::parse_assign_expr() {
  auto base_iter = current_function->parsed_program.emplace(
      current_function->parsed_program.end());
  Unit unit_left{parse_logical_disjunct()};
  if (last_token == Token{U'='}) {
    tokenize();
    Unit unit_right{parse_assign_expr()};
    *base_iter = AssignExpression{unit_left, unit_right};
    return Interim{};
  }
  current_function->parsed_program.erase(base_iter);
  return unit_left;
}

Unit Parser::parse_expression() { return parse_assign_expr(); }

void Parser::parse_statement() {
  Keyword *word_ptr{std::get_if<Keyword>(&last_token)};
  if (word_ptr && *word_ptr == Keyword::K_FUNCTION) {
    const FunctionDefinition *nested_definition{parse_function_decl()};
    std::optional<Identifier> function_name{nested_definition->function_name};
    if (not function_name)
      throw ScriptError{MissingFunctionName{}};
    if (current_function->nested_functions.contains(*function_name))
      throw ScriptError{DuplicateDeclaration{}};
    current_function->nested_functions.emplace(*function_name,
                                               nested_definition);
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_LET) {
    parse_variable_decl();
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_RETURN) {
    tokenize();
    auto statement_it{current_function->parsed_program.emplace(
        current_function->parsed_program.end())};
    Unit return_argument{parse_expression()};
    *statement_it = ReturnStatement{return_argument};
    assert_punct(';');
    return;
  }
  parse_expression();
  assert_punct(';');
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
    definition->local_scope.emplace(*parameter, std::monostate{});
    definition->arguments.push_back(*parameter);
    tokenize();
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  tokenize();
  assert_punct('{');
  function_trace.push_back(current_function);
  current_function = definition;
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      throw ScriptError{MissingPunctuation{'}'}};
    char32_t *alter_ptr{std::get_if<char32_t>(&last_token)};
    if (alter_ptr && *alter_ptr == '}')
      break;
    parse_statement();
  }
  current_function = function_trace.back();
  function_trace.pop_back();
  definition->definition_idx = function_definitions.size();
  function_definitions.emplace_back(definition);
  return definition;
}

void Parser::parse_variable_decl() {
  tokenize();
  if (not std::holds_alternative<Identifier>(last_token))
    throw ScriptError{MissingVariableName{}};
  Identifier variable_name{std::get<Identifier>(last_token)};
  auto statement_it{current_function->parsed_program.emplace(
      current_function->parsed_program.end())};
  tokenize();
  assert_punct('=');
  tokenize();
  Unit rvalue_unit{parse_expression()};
  statement_it->emplace<InitializeVariable>(variable_name, rvalue_unit);
  assert_punct(';');
  if (current_function->local_scope.contains(variable_name))
    throw ScriptError{DuplicateDeclaration{}};
  current_function->local_scope.emplace(variable_name, std::monostate{});
}

Script::Script()
    : resource{std::make_unique<std::pmr::monotonic_buffer_resource>()} {}

std::u16string Script::stringify(std::monostate) { return u"undefined"; }

std::u16string Script::stringify(std::u16string_view permanent_string) {
  return std::u16string(permanent_string);
}

std::u16string Script::stringify(std::int64_t number) {
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

std::u16string Script::stringify(double number) { return u"<unimplemented>"; }

std::u16string Script::stringify(FunctionFrame *function_frame) {
  return u"<unimplemented>";
}

std::u16string Script::stringify(ObjectInstance *object_instance) {
  return u"<unimplemented>";
}

std::u16string Script::stringify(IntrinsicFunction intrinsic_function) {
  return u"<unimplemented>";
}

void Script::evaluate() {}

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

Datatype AnalyzeUnit::operator()(std::monostate unit) {
  return Primitive::T_ANY;
}

Datatype AnalyzeUnit::operator()(std::u16string_view unit) {
  return Primitive::T_STRING;
}

Datatype AnalyzeUnit::operator()(std::int64_t unit) {
  return Primitive::T_NUMBER;
}

Datatype AnalyzeUnit::operator()(double unit) { return Primitive::T_NUMBER; }

Datatype AnalyzeUnit::operator()(Interim) {
  return std::get<Expression>(*parser.program_it++)
      .visit(AnalyzeExpression{parser});
}

std::optional<Datatype>
FunctionDefinition::analyze_variable(Identifier identifier) {
  if (auto it = local_scope.find(identifier); it != local_scope.end())
    return it->second;
  if (auto it = nested_functions.find(identifier); it != nested_functions.end())
    return it->second;
  return std::nullopt;
}

Datatype AnalyzeUnit::operator()(Identifier identifier) {
  auto complete_trace = std::ranges::concat_view{
      parser.function_trace, std::ranges::single_view{parser.current_function}};
  for (FunctionDefinition *definition : complete_trace | std::views::reverse)
    if (auto datatype = definition->analyze_variable(identifier); datatype)
      return *datatype;
  if (identifier.offset == OFFSET_console)
    return IntrinsicObject::O_CONSOLE;
  throw ScriptError{InvalidVariableAccess{}};
}

Datatype AnalyzeUnit::operator()(ScopeAccess scope_access) {
  std::unreachable();
}

Datatype AnalyzeUnit::operator()(const FunctionDefinition *definition) {
  return definition;
}

Datatype PropertyDatatype::operator()(std::monostate object_type) {
  return Primitive::T_ANY;
}
Datatype PropertyDatatype::operator()(Primitive object_type) {
  return Primitive::T_ANY;
}
Datatype PropertyDatatype::operator()(const ObjectShape *object_type) {
  return Primitive::T_ANY;
}
Datatype PropertyDatatype::operator()(const FunctionDefinition *object_type) {
  return Primitive::T_ANY;
}
Datatype PropertyDatatype::operator()(IntrinsicFunction object_type) {
  return Primitive::T_ANY;
}
Datatype PropertyDatatype::operator()(IntrinsicObject object_type) {
  if (object_type == IntrinsicObject::O_CONSOLE &&
      property.offset == OFFSET_log)
    return IntrinsicFunction::F_LOG;
  else
    return Primitive::T_ANY;
}

std::pair<Datatype, Datatype>
AnalyzeFunctionCallee::operator()(MemberExpression expression) {
  Datatype object_type{expression.object.visit(AnalyzeUnit{parser})};
  Datatype property_type{
      object_type.visit(PropertyDatatype{expression.property})};
  return {object_type, property_type};
}

std::pair<Datatype, Datatype> AnalyzeFunctionCallee::operator()(Unit unit) {
  return unit.visit(*this);
}

std::pair<Datatype, Datatype> AnalyzeFunctionCallee::operator()(Interim) {
  return std::get<Expression>(*parser.program_it++).visit(*this);
}

std::pair<Datatype, Datatype>
AnalyzeFunctionCallee::operator()(Identifier identifier) {
  Datatype callee_type{AnalyzeUnit{parser}(identifier)};
  return {std::monostate{}, callee_type};
}

Datatype AnalyzeExpression::operator()(Unit unit) {
  return unit.visit(AnalyzeUnit{parser});
}

Datatype AnalyzeExpression::operator()(BinaryExpression expression) {
  return Primitive::T_ANY;
}

Datatype AnalyzeExpression::operator()(LogicalExpression expression) {
  return Primitive::T_ANY;
}

Datatype AnalyzeExpression::operator()(MemberExpression expression) {
  Datatype object_type{expression.object.visit(AnalyzeUnit{parser})};
  Datatype property_type{
      object_type.visit(PropertyDatatype{expression.property})};
  return Primitive::T_ANY;
}

Datatype AnalyzeExpression::operator()(FunctionCallExpression expression) {
  auto [context, callee] =
      expression.callee.visit(AnalyzeFunctionCallee{parser});
  for (std::size_t i = 0; i < expression.passed_arguments; ++i)
    std::get<Expression>(*parser.program_it++).visit(*this);
  if (callee == Datatype{IntrinsicFunction::F_LOG})
    return Primitive::T_NUMBER;
  else
    return Primitive::T_ANY;
}

Datatype AnalyzeExpression::operator()(AssignExpression expression) {
  return Primitive::T_ANY;
}

Datatype AnalyzeExpression::operator()(ObjectExpression expression) {
  return Primitive::T_ANY;
}

void AnalyzeStatement::operator()(Expression statement) {
  statement.visit(AnalyzeExpression{parser});
}

void AnalyzeStatement::operator()(InitializeVariable statement) {
  auto complete_trace = std::ranges::concat_view{
      parser.function_trace, std::ranges::single_view{parser.current_function}};
  for (auto [frame_offset, definition] :
       complete_trace | std::views::enumerate | std::views::reverse) {
    if (not definition->local_scope.contains(statement.variable_name))
      continue;
    auto variable_it = definition->local_scope.find(statement.variable_name);
    std::size_t scope_offset{static_cast<std::size_t>(
        std::distance(definition->local_scope.begin(), variable_it))};
    ScopeAccess scope_access{static_cast<std::size_t>(frame_offset),
                             scope_offset};
    assert(0);
  }
}

void AnalyzeStatement::operator()(
    InitializeScope<const CompactString *> statement) {
  std::unreachable();
}

void AnalyzeStatement::operator()(InitializeScope<double> statement) {
  std::unreachable();
}

void AnalyzeStatement::operator()(InitializeMember statement) {
  std::unreachable();
}

void AnalyzeStatement::operator()(ReturnStatement statement) {
  std::unreachable();
}

void Parser::analyze_program() {
  for (auto nested_entry : current_function->nested_functions) {
    function_trace.push_back(current_function);
    std::size_t definition_idx{nested_entry.second->definition_idx};
    current_function = function_definitions[definition_idx].get();
    analyze_program();
    current_function = function_trace.back();
    function_trace.pop_back();
  }
  std::list<Statement> &prg{current_function->parsed_program};
  for (program_it = prg.begin(); program_it != prg.end(); ++program_it)
    program_it->visit(AnalyzeStatement{*this});
}
} // namespace Manadrain
