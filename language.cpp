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
  atom_pool.push_back(std::make_shared<std::string>(std::move(identifier_str)));
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
  auto [atlas_it, did_insert] =
      string_atlas.try_emplace(std::move(literal_str));
  if (not did_insert)
    return atlas_it->second;
  std::shared_ptr compact_string{std::make_shared<CompactString>()};
  if (std::ranges::all_of(atlas_it->first,
                          [](char16_t ch) { return ch < 128; })) {
    auto cast_to_ascii = [](char16_t ch) { return static_cast<char>(ch); };
    auto ascii_string = atlas_it->first | std::views::transform(cast_to_ascii);
    compact_string->ascii = {std::from_range, ascii_string};
  } else
    compact_string->unicode = atlas_it->first;
  atlas_it->second = compact_string.get();
  machine.permanent_strings.push_back(std::move(compact_string));
  return atlas_it->second;
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
  curr_definition = std::make_unique<FunctionDefinition>();
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      break;
    parse_statement();
  }
  machine.main_function = std::move(curr_definition);
}

Unit Parser::parse_object_literal() {
  std::shared_ptr object_shape{std::make_shared<ObjectShape>()};
  std::size_t statement_idx{curr_definition->intermediate.size()};
  Statement &statement_ir{*curr_definition->intermediate.emplace_back(
      std::make_unique<Statement>())};
  ObjectExpressionIR &object_expression{
      statement_ir.emplace<Expression>().emplace<ObjectExpressionIR>()};
  object_expression.object_shape = object_shape.get();
  tokenize();
  while (last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(last_token))
      throw ScriptError{InvalidPropertyName{}};
    Identifier property_name{std::get<Identifier>(last_token)};
    object_shape->properties.try_emplace(property_name);
    tokenize();
    std::size_t initializer_idx{curr_definition->intermediate.size()};
    Statement &initializer_ir{*curr_definition->intermediate.emplace_back(
        std::make_unique<Statement>())};
    InitializeMember &initialize_member{
        initializer_ir.emplace<InitializeMember>(property_name)};
    if (last_token == Token{U':'}) {
      tokenize();
      initialize_member.rvalue = parse_expression();
    }
    object_expression.properties.push_back(
        ExpressionIR<std::monostate>{initializer_idx});
    if (last_token != Token{U','})
      break;
    tokenize();
  }
  assert_punct('}');
  machine.object_shapes.push_back(std::move(object_shape));
  return ExpressionIR<std::monostate>{statement_idx};
}

Unit Parser::parse_primary_expr() {
  return last_token.visit([this](auto t) -> Unit {
    if constexpr (std::is_same_v<decltype(t), const CompactString *> ||
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
  std::size_t statement_idx{curr_definition->intermediate.size()};
  Statement &statement_ir{*curr_definition->intermediate.emplace_back(
      std::make_unique<Statement>())};
  FunctionCallExpressionIR &expression{
      statement_ir.emplace<Expression>().emplace<FunctionCallExpressionIR>()};
  expression.callee = callee;
  while (1) {
    tokenize();
    if (last_token == Token{U')'})
      break;
    expression.arguments.emplace_back(parse_expression());
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  return ExpressionIR<std::monostate>{statement_idx};
}

Unit Parser::parse_member_expr(Unit object) {
  tokenize();
  if (not std::holds_alternative<Identifier>(last_token))
    throw ScriptError{MissingFieldName{}};
  Identifier field_name{std::get<Identifier>(last_token)};
  std::size_t statement_idx{curr_definition->intermediate.size()};
  curr_definition->intermediate.push_back(
      std::make_unique<Statement>(MemberExpression{object, field_name}));
  return ExpressionIR<std::monostate>{statement_idx};
}

Unit Parser::parse_postfix_expr() {
  Unit postfix_unit{};
  std::optional postfix_opt{parse_primary_expr()};
  while (postfix_opt) {
    postfix_unit = *postfix_opt;
    tokenize();
    std::optional<Unit> ahead{};
    if (last_token == Token{U'.'})
      ahead = parse_member_expr(postfix_unit);
    else if (last_token == Token{U'('})
      ahead = parse_call_expr(postfix_unit);
    postfix_opt = ahead;
  }
  return postfix_unit;
}

Unit Parser::parse_additive_expr() {
  Unit unit_left{parse_postfix_expr()};
  if (last_token != Token{U'+'} && last_token != Token{U'-'})
    return unit_left;
  char32_t binary_op{std::get<char32_t>(last_token)};
  tokenize();
  Unit unit_right{parse_postfix_expr()};
  std::size_t statement_idx{curr_definition->intermediate.size()};
  curr_definition->intermediate.push_back(std::make_unique<Statement>(
      BinaryExpressionIR{unit_left, unit_right, binary_op}));
  return ExpressionIR<std::monostate>{statement_idx};
}

Unit Parser::parse_logical_disjunct() {
  Unit unit_left{parse_additive_expr()};
  while (last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    Operator op{std::get<Operator>(last_token)};
    tokenize();
    Unit unit_right{parse_additive_expr()};
    std::size_t statement_idx{curr_definition->intermediate.size()};
    curr_definition->intermediate.push_back(std::make_unique<Statement>(
        LogicalExpression{unit_left, unit_right, op}));
    unit_left = ExpressionIR<std::monostate>{statement_idx};
  }
  return unit_left;
}

Unit Parser::parse_assign_expr() {
  Unit unit_left{parse_logical_disjunct()};
  if (last_token != Token{U'='})
    return unit_left;
  tokenize();
  Unit unit_right{parse_assign_expr()};
  std::size_t statement_idx{curr_definition->intermediate.size()};
  curr_definition->intermediate.push_back(
      std::make_unique<Statement>(AssignExpression{unit_left, unit_right}));
  return ExpressionIR<std::monostate>{statement_idx};
}

Unit Parser::parse_expression() { return parse_assign_expr(); }

void Parser::parse_function_stmt() {
  const FunctionDefinition *nested_def{parse_function_decl()};
  if (not nested_def->function_name)
    throw ScriptError{MissingFunctionName{}};
  bool duplicate_exists = std::ranges::contains(
      curr_definition->nested_functions, nested_def->function_name,
      [](auto el) { return el->function_name; });
  if (duplicate_exists)
    throw ScriptError{DuplicateDeclaration{}};
  curr_definition->nested_functions.push_back(nested_def);
}

void Parser::parse_statement() {
  Keyword keyword{};
  if (std::holds_alternative<Keyword>(last_token))
    keyword = std::get<Keyword>(last_token);
  switch (keyword) {
  case Keyword::K_FUNCTION:
    parse_function_stmt();
    return;
  case Keyword::K_LET:
    parse_variable_decl();
    return;
  case Keyword::K_RETURN: {
    tokenize();
    ReturnStatement return_statement{parse_expression()};
    curr_definition->program.push_back(return_statement);
    assert_punct(';');
    return;
  }
  default:
    curr_definition->program.push_back(parse_expression());
    assert_punct(';');
    return;
  }
}

const FunctionDefinition *Parser::parse_function_decl() {
  tokenize();
  Identifier *function_name{std::get_if<Identifier>(&last_token)};
  std::unique_ptr definition{std::make_unique<FunctionDefinition>()};
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
    definition->local_scope.try_emplace(*parameter);
    definition->arguments.push_back(*parameter);
    tokenize();
    if (last_token == Token{U')'})
      break;
    assert_punct(',');
  }
  tokenize();
  assert_punct('{');
  curr_definition.swap(definition);
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      throw ScriptError{MissingPunctuation{'}'}};
    char32_t *alter_ptr{std::get_if<char32_t>(&last_token)};
    if (alter_ptr && *alter_ptr == '}')
      break;
    parse_statement();
  }
  curr_definition.swap(definition);
  return machine.function_defs.emplace_back(std::move(definition)).get();
}

void Parser::parse_variable_decl() {
  tokenize();
  if (not std::holds_alternative<Identifier>(last_token))
    throw ScriptError{MissingVariableName{}};
  Identifier variable_name{std::get<Identifier>(last_token)};
  tokenize();
  assert_punct('=');
  tokenize();
  Unit rvalue_unit{parse_expression()};
  curr_definition->program.push_back(
      InitializeVariable{variable_name, rvalue_unit});
  assert_punct(';');
  if (curr_definition->local_scope.contains(variable_name))
    throw ScriptError{DuplicateDeclaration{}};
  curr_definition->local_scope.try_emplace(variable_name);
}

Machine::Machine()
    : resource{std::make_unique<std::pmr::monotonic_buffer_resource>()} {}

std::u16string Machine::stringify(std::monostate) { return u"undefined"; }

std::u16string Machine::stringify(std::u16string_view permanent_string) {
  return std::u16string(permanent_string);
}

std::u16string Machine::stringify(std::int64_t number) {
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

std::u16string Machine::stringify(double number) { return u"<unimplemented>"; }

std::u16string Machine::stringify(FunctionFrame *function_frame) {
  return u"<unimplemented>";
}

std::u16string Machine::stringify(ObjectInstance *object_instance) {
  return u"<unimplemented>";
}

std::u16string Machine::stringify(IntrinsicFunction intrinsic_function) {
  return u"<unimplemented>";
}

void Machine::evaluate() {}

void Machine::collect_console_messages(std::stop_token stopper,
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

Datatype
Typechecker::analyze_initializer(std::size_t local_offset,
                                 ConcreteUnit<const CompactString *> unit_alt) {
  analyzer_stack.back()->replica.program.push_back(
      InitializeScope<const CompactString *>{local_offset, unit_alt});
  return StringType{};
}

Datatype
Typechecker::analyze_initializer(std::size_t local_offset,
                                 ExpressionIR<std::monostate> unit_alt) {
  auto expression_initializer = [&](auto expression_alt) {
    return analyze_initializer(local_offset,
                               analyze_expression(expression_alt));
  };
  Expression expression_ir{std::get<Expression>(
      *analyzer_stack.back()->model->intermediate[unit_alt.intermediate_idx])};
  return expression_ir.visit(expression_initializer);
}

Unit Typechecker::analyze_scope_access(std::size_t scope_offset,
                                       std::size_t local_offset, StringType) {
  return ScopeAccessor<const CompactString *>{scope_offset, local_offset};
}

Unit Typechecker::analyze_unit(std::int64_t number) {
  return static_cast<double>(number);
}

Unit Typechecker::analyze_unit(Identifier identifier) {
  for (auto it = analyzer_stack.rbegin(); it != analyzer_stack.rend(); ++it) {
    const auto &local_scope{it->get()->replica.local_scope};
    if (not local_scope.contains(identifier))
      continue;
    std::size_t scope_offset = std::distance(analyzer_stack.rbegin(), it);
    std::size_t local_offset =
        std::distance(local_scope.begin(), local_scope.find(identifier));
    auto deduce_concrete = [&](auto datatype_alt) {
      return analyze_scope_access(scope_offset, local_offset, datatype_alt);
    };
    return local_scope.values()[local_offset].visit(deduce_concrete);
  }
  throw ScriptError{InvalidVariableAccess{}};
}

Unit Typechecker::analyze_unit(ExpressionIR<std::monostate> expression_ir) {
  auto deduce_concrete = [&](auto expression_alt) {
    return analyze_expression(expression_alt);
  };
  std::size_t intermediate_idx{expression_ir.intermediate_idx};
  Expression expression{std::get<Expression>(
      *analyzer_stack.back()->model->intermediate[intermediate_idx])};
  return expression.visit(deduce_concrete);
}

Unit Typechecker::analyze_member_access(
    Identifier identifier,
    ConcreteUnit<const CompactString *> concrete_object) {
  return std::monostate{};
}

Unit Typechecker::analyze_expression(MemberExpression expression) {
  auto deduce_object = [&](auto unit_alt) { return analyze_unit(unit_alt); };
  Unit concrete_object{expression.object.visit(deduce_object)};
  auto deduce_member_access = [&](auto concrete_alt) {
    return analyze_member_access(expression.property, concrete_alt);
  };
  Unit concrete_property{concrete_object.visit(deduce_member_access)};
  return std::monostate{};
}

Unit Typechecker::analyze_expression(BinaryExpressionIR expression) {
  auto deduce_concrete = [&](auto unit_alt) { return analyze_unit(unit_alt); };
  Unit concrete_left{expression.left.visit(deduce_concrete)};
  Unit concrete_right{expression.right.visit(deduce_concrete)};
  return std::monostate{};
}

void Typechecker::analyze_statement(Expression expression) {}

void Typechecker::analyze_statement(InitializeVariable statement) {
  auto &local_scope{analyzer_stack.back()->replica.local_scope};
  std::size_t local_offset = std::distance(
      local_scope.begin(), local_scope.find(statement.variable_name));
  auto deduce_initializer = [&](auto unit_alt) {
    return analyze_initializer(local_offset, unit_alt);
  };
  Datatype initializer_type{statement.rvalue.visit(deduce_initializer)};
  local_scope[statement.variable_name] = initializer_type;
}

void Typechecker::analyze_statement(ReturnStatement statement) {}

void Typechecker::analyze_definition() {
  for (const FunctionDefinition *nested_definition :
       analyzer_stack.back()->model->nested_functions) {
    analyzer_stack.push_back(
        std::make_unique<AnalyzedDefinition>(nested_definition));
    analyze_definition();
    machine.function_defs.push_back(std::make_shared<FunctionDefinition>(
        std::move(analyzer_stack.back()->replica)));
    analyzer_stack.pop_back();
    analyzer_stack.back()->replica.nested_functions.push_back(
        machine.function_defs.back().get());
  }
  analyzer_stack.back()->replica.local_scope =
      analyzer_stack.back()->model->local_scope;
  for (Statement model_stmt : analyzer_stack.back()->model->program)
    model_stmt.visit([this](auto stmt_alt) {
      if constexpr (std::is_same_v<decltype(stmt_alt), Expression> ||
                    std::is_same_v<decltype(stmt_alt), InitializeVariable> ||
                    std::is_same_v<decltype(stmt_alt), ReturnStatement>)
        analyze_statement(stmt_alt);
      else
        std::unreachable();
    });
}

void Typechecker::typecheck() {
  input_defs.swap(machine.function_defs);
  analyzer_stack.push_back(
      std::make_unique<AnalyzedDefinition>(machine.main_function.get()));
  analyze_definition();
  machine.main_function = std::make_shared<FunctionDefinition>(
      std::move(analyzer_stack.back()->replica));
  analyzer_stack.pop_back();
}
} // namespace Manadrain
