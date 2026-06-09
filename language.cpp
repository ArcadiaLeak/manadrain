#include <algorithm>
#include <cassert>
#include <new>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::flat_map<std::string_view, Keyword> keyword_atlas{
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
static const std::flat_map<std::string_view, std::size_t> identifier_atlas{
    {"console", OFFSET_console},
    {"log", OFFSET_log},
    {"length", OFFSET_length}};
static const std::array persistent_identifiers{
    std::to_array<std::string_view>({"console", "log", "length"})};

std::optional<char32_t> Compiler::forward() {
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

std::generator<std::optional<char32_t>> Compiler::traverse_text() {
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

void Compiler::backward() {
  std::optional<char32_t> behind{backtrace.back()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_u8) | std::views::join);
  backtrace.pop_back();
}

void Compiler::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

Token Compiler::tokenize_identifier(char32_t leading) {
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
  auto iter_persistent = identifier_atlas.find(identifier_str);
  if (iter_persistent != identifier_atlas.end())
    return Identifier{iter_persistent->second};
  Identifier identifier{persistent_identifiers.size() + atom_atlas.size() - 1};
  auto emplace_ret =
      atom_atlas.try_emplace(std::move(identifier_str), identifier);
  return emplace_ret.first->second;
}

Token Compiler::tokenize_string_literal(char32_t separator) {
  std::u16string u16_literal{};
  auto match_literal_end = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view =
      traverse_text() | std::views::take_while(match_literal_end);
  for (std::optional<char32_t> leading : literal_view) {
    if (leading == '\r' && forward() != '\n')
      backward();
    if (not leading || leading == '\r' || leading == '\n')
      throw UnexpectedStringEnd{};
    u16_literal.append_range(traverse_u16(*leading));
  }
  if (std::ranges::all_of(u16_literal, [](char16_t ch) { return ch < 128; })) {
    auto cast_to_ascii = [](char16_t ch) { return static_cast<char>(ch); };
    auto ascii_string = u16_literal | std::views::transform(cast_to_ascii);
    auto [string_iter, _] =
        token_strings.emplace(std::from_range, ascii_string);
    return *string_iter;
  }
  auto [u16string_iter, _] = token_u16strings.emplace(std::move(u16_literal));
  return *u16string_iter;
}

Token Compiler::tokenize_numeric_literal(char32_t leading) {
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

void Compiler::tokenize() {
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
    throw UnexpectedToken{};
  }
  last_token = std::monostate{};
}

void Compiler::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw MissingPunctuation{must_be};
}

void Compiler::parse_text() {
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      break;
    entry_module.parse_statement();
  }
}

void Compiler::ExecutionBoundary::parse_function_stmt() {
  Compiler::FunctionDefinition definition{compiler};
  definition.parse_function_decl();
  if (not definition.function_name)
    throw MissingFunctionName{};
  bool duplicate_exists =
      std::ranges::contains(inner_functions, definition.function_name,
                            [](const auto &el) { return el.function_name; });
  if (duplicate_exists)
    throw DuplicateDeclaration{};
  inner_functions.push_back(std::move(definition));
}

void Compiler::ExecutionBoundary::parse_variable_decl() {
  compiler.tokenize();
  if (not std::holds_alternative<Identifier>(compiler.last_token))
    throw MissingVariableName{};
  Identifier variable_name{std::get<Identifier>(compiler.last_token)};
  compiler.tokenize();
  compiler.assert_punct('=');
  compiler.tokenize();
  std::unique_ptr initialize_variable{std::make_unique<InitializeVariable>()};
  initialize_variable->variable_name = variable_name;
  initialize_variable->rvalue = parse_expression();
  program.push_back(std::move(initialize_variable));
  compiler.assert_punct(';');
  if (local_scope.contains(variable_name))
    throw DuplicateDeclaration{};
  local_scope.try_emplace(variable_name);
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_expression() {
  return parse_assign_expr();
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_postfix_expr() {
  std::unique_ptr postfix_expr{
      compiler.last_token.visit(ParsePrimaryExpression{*this})};
  auto parse_postfix = [&](char32_t punct) -> std::unique_ptr<Expression> {
    switch (punct) {
    case '.':
      return parse_member_expr(std::move(postfix_expr));
    case '(':
      return parse_call_expr(std::move(postfix_expr));
    default:
      return nullptr;
    }
  };
  while (1) {
    compiler.tokenize();
    if (not std::holds_alternative<char32_t>(compiler.last_token))
      break;
    std::unique_ptr ahead{
        parse_postfix(std::get<char32_t>(compiler.last_token))};
    if (not ahead)
      break;
    postfix_expr = std::move(ahead);
  }
  return postfix_expr;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_call_expr(
    std::unique_ptr<Compiler::Expression> callee) {
  std::unique_ptr expression{std::make_unique<AliasedFunctionCall>()};
  expression->callee = std::move(callee);
  while (1) {
    compiler.tokenize();
    if (compiler.last_token == Token{U')'})
      break;
    expression->arguments.push_back(parse_expression());
    if (compiler.last_token == Token{U')'})
      break;
    compiler.assert_punct(',');
  }
  return expression;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_member_expr(
    std::unique_ptr<Compiler::Expression> object) {
  compiler.tokenize();
  if (not std::holds_alternative<Identifier>(compiler.last_token))
    throw MissingFieldName{};
  Identifier field_name{std::get<Identifier>(compiler.last_token)};
  std::unique_ptr expression{std::make_unique<MemberExpression>()};
  expression->object = std::move(object);
  expression->property = field_name;
  return expression;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_additive_expr() {
  std::unique_ptr expr_left{parse_postfix_expr()};
  if (compiler.last_token != Token{U'+'} && compiler.last_token != Token{U'-'})
    return expr_left;
  std::unique_ptr expression{std::make_unique<BinaryExpression>()};
  expression->op = std::get<char32_t>(compiler.last_token);
  compiler.tokenize();
  expression->left = std::move(expr_left);
  expression->right = parse_postfix_expr();
  return expression;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_assign_expr() {
  std::unique_ptr expr_left{parse_logical_disjunct()};
  if (compiler.last_token != Token{U'='})
    return expr_left;
  compiler.tokenize();
  std::unique_ptr expression{std::make_unique<AssignExpression>()};
  expression->left = std::move(expr_left);
  expression->right = parse_assign_expr();
  return expression;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_logical_disjunct() {
  std::unique_ptr expr_left{parse_additive_expr()};
  while (compiler.last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    std::unique_ptr expression{std::make_unique<LogicalExpression>()};
    expression->op = std::get<Operator>(compiler.last_token);
    compiler.tokenize();
    expression->left = std::move(expr_left);
    expression->right = parse_additive_expr();
    expr_left = std::move(expression);
  }
  return expr_left;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::parse_object_literal() {
  std::unique_ptr expression{std::make_unique<ObjectExpression>()};
  compiler.tokenize();
  while (compiler.last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(compiler.last_token))
      throw InvalidPropertyName{};
    Identifier property_name{std::get<Identifier>(compiler.last_token)};
    expression->object_shape.properties.try_emplace(property_name);
    compiler.tokenize();
    expression->keys.push_back(property_name);
    std::unique_ptr<Expression> initializer{};
    if (compiler.last_token == Token{U':'}) {
      compiler.tokenize();
      initializer = parse_expression();
    }
    expression->values.push_back(std::move(initializer));
    if (compiler.last_token != Token{U','})
      break;
    compiler.tokenize();
  }
  compiler.assert_punct('}');
  return expression;
}

std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    std::monostate) {
  throw UnexpectedToken{};
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(Operator op) {
  throw UnexpectedToken{};
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    char32_t punct) {
  if (punct == '{')
    return boundary.parse_object_literal();
  else
    throw UnexpectedToken{};
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    std::int64_t number) {
  std::unique_ptr num_literal{std::make_unique<NumericLiteral>()};
  num_literal->val = static_cast<double>(number);
  return num_literal;
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(double number) {
  std::unique_ptr num_literal{std::make_unique<NumericLiteral>()};
  num_literal->val = number;
  return num_literal;
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    Keyword keyword) {
  if (keyword == Keyword::K_FUNCTION) {
    Compiler::FunctionDefinition definition{boundary.compiler};
    definition.parse_function_decl();
    boundary.inner_functions.push_back(std::move(definition));
  }
  throw UnexpectedToken{};
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    Identifier identifier) {
  std::unique_ptr variable_accessor{std::make_unique<VariableAccessor>()};
  variable_accessor->identifier = identifier;
  return variable_accessor;
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    std::string_view ascii) {
  std::unique_ptr ascii_literal{std::make_unique<AsciiLiteral>()};
  ascii_literal->val = ascii;
  return ascii_literal;
}
std::unique_ptr<Compiler::Expression>
Compiler::ExecutionBoundary::ParsePrimaryExpression::operator()(
    std::u16string_view unicode) {
  std::unique_ptr unicode_literal{std::make_unique<UnicodeLiteral>()};
  unicode_literal->val = unicode;
  return unicode_literal;
}

void Compiler::ExecutionBoundary::parse_statement() {
  Keyword keyword{};
  if (std::holds_alternative<Keyword>(compiler.last_token))
    keyword = std::get<Keyword>(compiler.last_token);
  switch (keyword) {
  case Keyword::K_FUNCTION:
    parse_function_stmt();
    return;
  case Keyword::K_LET:
    parse_variable_decl();
    return;
  case Keyword::K_RETURN: {
    compiler.tokenize();
    std::unique_ptr statement{std::make_unique<ReturnStatement>()};
    statement->argument = parse_expression();
    program.push_back(std::move(statement));
    compiler.assert_punct(';');
    return;
  }
  default:
    std::unique_ptr statement{std::make_unique<ExpressionStatement>()};
    statement->argument = parse_expression();
    program.push_back(std::move(statement));
    compiler.assert_punct(';');
    return;
  }
}

void Compiler::FunctionDefinition::parse_function_decl() {
  compiler.tokenize();
  if (std::holds_alternative<Identifier>(compiler.last_token)) {
    function_name = std::get<Identifier>(compiler.last_token);
    compiler.tokenize();
  }
  compiler.assert_punct('(');
  while (1) {
    compiler.tokenize();
    if (compiler.last_token == Token{U')'})
      break;
    if (not std::holds_alternative<Identifier>(compiler.last_token))
      throw MissingFormalParameter{};
    Identifier parameter{std::get<Identifier>(compiler.last_token)};
    local_scope.try_emplace(parameter);
    arguments.push_back(parameter);
    compiler.tokenize();
    if (compiler.last_token == Token{U')'})
      break;
    compiler.assert_punct(',');
  }
  compiler.tokenize();
  compiler.assert_punct('{');
  while (1) {
    compiler.tokenize();
    if (std::holds_alternative<std::monostate>(compiler.last_token))
      throw MissingPunctuation{'}'};
    char32_t *alter_ptr{std::get_if<char32_t>(&compiler.last_token)};
    if (alter_ptr && *alter_ptr == '}')
      break;
    parse_statement();
  }
}

std::unique_ptr<Compiler::Statement>
Compiler::InitializeVariable::analyze(ExecutionBoundary &boundary) const {
  std::unique_ptr initialize_scope{std::make_unique<InitializeScope>()};
  initialize_scope->local_offset = std::distance(
      boundary.local_scope.begin(), boundary.local_scope.find(variable_name));
  initialize_scope->rvalue = rvalue->analyze_as_value(boundary);
  boundary.local_scope[variable_name] = initialize_scope->rvalue->datatype();
  return initialize_scope;
}

std::unique_ptr<Compiler::Statement>
Compiler::ExpressionStatement::analyze(ExecutionBoundary &boundary) const {
  std::unique_ptr expression_stmt{std::make_unique<ExpressionStatement>()};
  expression_stmt->argument = argument->analyze_as_value(boundary);
  return expression_stmt;
}

std::unique_ptr<Compiler::Statement>
Compiler::ReturnStatement::analyze(ExecutionBoundary &boundary) const {
  if (not boundary.allows_return_stmt())
    throw InvalidReturnStatement{};
  std::unique_ptr return_stmt{std::make_unique<ReturnStatement>()};
  return_stmt->argument = argument->analyze_as_value(boundary);
  boundary.return_type = return_stmt->argument->datatype();
  return return_stmt;
}

std::unique_ptr<Compiler::Expression>
Compiler::VariableAccessor::analyze_as_value(
    ExecutionBoundary &boundary) const {
  const ExecutionBoundary *current{&boundary};
  for (std::size_t i = 0; current; ++i) {
    std::unique_ptr<ScopeAccessor> accessor{current->find_local(identifier)};
    if (not accessor)
      current = current->parent_boundary;
    else {
      accessor->scope_offset = i;
      return accessor;
    }
  }
  if (identifier.offset == OFFSET_console) {
    std::unique_ptr accessor{std::make_unique<IntrinsicAccessor>()};
    accessor->object_type = IntrinsicObject::O_CONSOLE;
    return std::move(accessor);
  }
  throw InvalidVariableAccess{};
}

std::unique_ptr<Compiler::Expression>
Compiler::AsciiLiteral::analyze_as_value(ExecutionBoundary &boundary) const {
  std::unique_ptr copycat{std::make_unique<AsciiLiteral>()};
  copycat->val = val;
  return copycat;
}

std::unique_ptr<Compiler::Expression>
Compiler::UnicodeLiteral::analyze_as_value(ExecutionBoundary &boundary) const {
  std::unique_ptr copycat{std::make_unique<UnicodeLiteral>()};
  copycat->val = val;
  return copycat;
}

std::unique_ptr<Compiler::Expression>
Compiler::NumericLiteral::analyze_as_value(ExecutionBoundary &boundary) const {
  std::unique_ptr copycat{std::make_unique<NumericLiteral>()};
  copycat->val = val;
  return copycat;
}

std::unique_ptr<Compiler::Expression>
Compiler::BinaryExpression::analyze_as_value(
    ExecutionBoundary &boundary) const {
  std::unique_ptr binary_expr{std::make_unique<BinaryExpression>()};
  binary_expr->op = op;
  binary_expr->left = left->analyze_as_value(boundary);
  binary_expr->right = right->analyze_as_value(boundary);
  assert(std::holds_alternative<NumberType>(binary_expr->left->datatype()) &&
         std::holds_alternative<NumberType>(binary_expr->right->datatype()));
  return binary_expr;
}

std::unique_ptr<Compiler::Expression>
Compiler::MemberExpression::analyze_as_value(
    ExecutionBoundary &boundary) const {
  std::unique_ptr object_expr{object->analyze_as_value(boundary)};
  if (std::holds_alternative<StringType>(object_expr->datatype()) &&
      property.offset == OFFSET_length) {
    std::unique_ptr string_length{std::make_unique<StringLength>()};
    string_length->argument = std::move(object_expr);
    return string_length;
  }
  std::unreachable();
}

std::unique_ptr<Compiler::Expression>
Compiler::AliasedFunctionCall::analyze_as_value(
    ExecutionBoundary &boundary) const {
  std::unique_ptr callee_expression{callee->analyze_as_callee(boundary)};
  callee_expression->analyze_arguments(boundary, arguments);
  return callee_expression;
}

void Compiler::DirectFunctionCall::analyze_arguments(
    ExecutionBoundary &boundary,
    std::span<const std::unique_ptr<Expression>> arguments) {}

void Compiler::ConsoleLogCall::analyze_arguments(
    ExecutionBoundary &boundary,
    std::span<const std::unique_ptr<Expression>> positional_args) {
  auto stringify_argument = [&boundary](const auto &argument) {
    std::unique_ptr analyzed_arg{argument->analyze_as_value(boundary)};
    ValueType argument_type{analyzed_arg->datatype()};
    return argument_type.visit(InjectStringify{std::move(analyzed_arg)});
  };
  auto stringified_args =
      positional_args | std::views::transform(stringify_argument);
  arguments = {std::from_range, stringified_args};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::MemberExpression::analyze_as_callee(
    ExecutionBoundary &boundary) const {
  std::unique_ptr analyzed_object{object->analyze_as_value(boundary)};
  return analyzed_object->datatype().visit(FindMethod{property});
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::VariableAccessor::analyze_as_callee(
    ExecutionBoundary &boundary) const {
  const ExecutionBoundary *current{&boundary};
  while (1) {
    const FunctionDefinition *definition{current->find_function(identifier)};
    if (not definition)
      current = current->parent_boundary;
    if (not current)
      throw InvalidVariableAccess{};
    std::unique_ptr direct_call{std::make_unique<DirectFunctionCall>()};
    direct_call->callee = definition;
    return direct_call;
  }
}

std::unique_ptr<Compiler::ScopeAccessor>
Compiler::ExecutionBoundary::find_local(Identifier identifier) const {
  if (not local_scope.contains(identifier))
    return nullptr;
  std::size_t local_offset =
      std::distance(local_scope.begin(), local_scope.find(identifier));
  std::unique_ptr accessor{std::make_unique<ScopeAccessor>()};
  accessor->local_type = local_scope.values()[local_offset];
  accessor->local_offset = local_offset;
  return accessor;
}

const Compiler::FunctionDefinition *
Compiler::ExecutionBoundary::find_function(Identifier identifier) const {
  auto by_name = [](const auto &def) { return def.function_name; };
  auto function_it = std::ranges::find(inner_functions, identifier, by_name);
  return function_it == inner_functions.end() ? nullptr : &(*function_it);
}

void Compiler::analyze_program() { entry_module.analyze(); }

void Compiler::ExecutionBoundary::analyze() {
  for (auto &definition : inner_functions) {
    definition.parent_boundary = this;
    definition.analyze();
  }
  for (auto &statement : program)
    statement = statement->analyze(*this);
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(VoidType) {
  throw MissingMethod{};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(NumberType) {
  throw MissingMethod{};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(StringType) {
  throw MissingMethod{};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(LambdaType) {
  throw MissingMethod{};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(AnyType) {
  throw MissingMethod{};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(const ObjectShape *object_shape) {
  throw MissingMethod{};
}

std::unique_ptr<Compiler::CalleeExpression>
Compiler::FindMethod::operator()(IntrinsicObject intrinsic_object) {
  if (intrinsic_object == IntrinsicObject::O_CONSOLE)
    if (identifier.offset == OFFSET_log)
      return std::make_unique<ConsoleLogCall>();
  throw MissingMethod{};
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(VoidType) {
  return nullptr;
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(NumberType) {
  std::unique_ptr stringify_number{std::make_unique<StringifyNumber>()};
  stringify_number->argument = std::move(expression);
  return stringify_number;
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(StringType) {
  return nullptr;
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(LambdaType) {
  return nullptr;
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(AnyType) {
  return nullptr;
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(const ObjectShape *object_shape) {
  return nullptr;
}

std::unique_ptr<Compiler::Expression>
Compiler::InjectStringify::operator()(IntrinsicObject intrinsic_object) {
  return nullptr;
}
} // namespace Manadrain
