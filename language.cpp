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
      std::make_unique<std::u16string>(std::move(literal_str)));
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
}

Unit Parser::parse_object_literal() {
  ObjectShape *object_shape{new ObjectShape{}};
  object_shapes.emplace_back(object_shape);
  ObjectExpression *object_expr{new ObjectExpression{}};
  object_expr->object_shape = object_shape;
  expressions.emplace_back(object_expr);
  tokenize();
  while (last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(last_token))
      throw ScriptError{InvalidPropertyName{}};
    Identifier property_name{std::get<Identifier>(last_token)};
    auto ordered_it =
        std::ranges::lower_bound(object_shape->properties, property_name);
    object_shape->properties.insert(ordered_it, property_name);
    tokenize();
    Unit property_rvalue{};
    if (last_token == Token{U':'}) {
      tokenize();
      property_rvalue = parse_expression();
    }
    object_expr->properties.emplace_back(property_name, property_rvalue);
    if (last_token != Token{U','})
      break;
    tokenize();
  }
  assert_punct('}');
  return object_expr;
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

Unit Parser::parse_call_expr(Unit callee) {
  FunctionCallExpression *expression{new FunctionCallExpression{}};
  expression->callee = callee;
  expressions.emplace_back(expression);
  while (1) {
    tokenize();
    if (last_token == Token{U')'})
      break;
    expression->arguments.push_back(parse_expression());
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  tokenize();
  return parse_postfix_expr(expression);
}

Unit Parser::parse_member_expr(Unit object) {
  tokenize();
  Identifier *field_name{std::get_if<Identifier>(&last_token)};
  if (not field_name)
    throw ScriptError{MissingFieldName{}};
  MemberExpression *expression{new MemberExpression{}};
  expression->object = object;
  expression->property = *field_name;
  expressions.emplace_back(expression);
  tokenize();
  return parse_postfix_expr(expression);
}

Unit Parser::parse_postfix_expr(Unit base_unit) {
  if (last_token == Token{U'.'})
    return parse_member_expr(base_unit);
  if (last_token == Token{U'('})
    return parse_call_expr(base_unit);
  return base_unit;
}

Unit Parser::parse_postfix_expr() {
  Unit base_unit{parse_primary_expr()};
  tokenize();
  return parse_postfix_expr(base_unit);
}

Unit Parser::parse_additive_expr() {
  Unit unit_left{parse_postfix_expr()};
  if (last_token == Token{U'+'} || last_token == Token{U'-'}) {
    char32_t binary_op{std::get<char32_t>(last_token)};
    tokenize();
    Unit unit_right{parse_postfix_expr()};
    BinaryExpression *expression{new BinaryExpression{}};
    expression->left = unit_left;
    expression->right = unit_right;
    expression->op = binary_op;
    expressions.emplace_back(expression);
    return expression;
  }
  return unit_left;
}

Unit Parser::parse_logical_disjunct() {
  Unit unit_left{parse_additive_expr()};
  while (last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    Operator op{std::get<Operator>(last_token)};
    tokenize();
    Unit unit_right{parse_additive_expr()};
    LogicalExpression *expression{new LogicalExpression{}};
    expression->left = unit_left;
    expression->right = unit_right;
    expression->op = op;
    expressions.emplace_back(expression);
    unit_left = expression;
  }
  return unit_left;
}

Unit Parser::parse_assign_expr() {
  Unit unit_left{parse_logical_disjunct()};
  if (last_token != Token{U'='})
    return unit_left;
  tokenize();
  Unit unit_right{parse_assign_expr()};
  AssignExpression *expression{new AssignExpression{}};
  expression->left = unit_left;
  expression->right = unit_right;
  expressions.emplace_back(expression);
  return expression;
}

Unit Parser::parse_expression() { return parse_assign_expr(); }

void Parser::parse_statement() {
  Keyword *word_ptr{std::get_if<Keyword>(&last_token)};
  if (word_ptr && *word_ptr == Keyword::K_FUNCTION) {
    const FunctionDefinition *nested_definition{parse_function_decl()};
    std::optional<Identifier> function_name{nested_definition->function_name};
    if (not function_name)
      throw ScriptError{MissingFunctionName{}};
    if (current_function->local_scope.contains(*function_name) ||
        current_function->nested_functions.contains(*function_name))
      throw ScriptError{DuplicateDeclaration{}};
    current_function->nested_functions.insert(
        {*function_name, nested_definition});
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_LET) {
    parse_variable_decl();
    return;
  }
  if (word_ptr && *word_ptr == Keyword::K_RETURN) {
    tokenize();
    auto statement_it{current_function->program.insert(
        current_function->program.end(), ReturnStatement{})};
    std::size_t statement_idx =
        std::distance(current_function->program.begin(), statement_it);
    Unit return_argument{parse_expression()};
    ReturnStatement *statement{std::get_if<ReturnStatement>(
        current_function->program.data() + statement_idx)};
    statement->argument = return_argument;
    assert_punct(';');
    return;
  }
  Unit expression_statement{parse_expression()};
  current_function->program.push_back(expression_statement);
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
    definition->local_scope.insert({*parameter, Primitive::T_ANY});
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

void Parser::parse_variable_decl() {
  tokenize();
  Identifier *varname_ptr{std::get_if<Identifier>(&last_token)};
  if (not varname_ptr)
    throw ScriptError{MissingVariableName{}};
  auto statement_it{current_function->program.insert(
      current_function->program.end(), InitializeVariable{*varname_ptr})};
  std::size_t statement_idx =
      std::distance(current_function->program.begin(), statement_it);
  tokenize();
  assert_punct('=');
  tokenize();
  Unit rvalue_unit{parse_expression()};
  InitializeVariable *variable_init{std::get_if<InitializeVariable>(
      current_function->program.data() + statement_idx)};
  variable_init->rvalue = rvalue_unit;
  assert_punct(';');
  Identifier variable_name{variable_init->variable_name};
  if (current_function->local_scope.contains(variable_name) ||
      current_function->nested_functions.contains(variable_name))
    throw ScriptError{DuplicateDeclaration{}};
  current_function->local_scope.insert({variable_name, Primitive::T_ANY});
}

void Analyzer::analyze_program() {
  for (std::unique_ptr<const FunctionDefinition> &definition :
       function_definitions) {
  }
}
} // namespace Manadrain
