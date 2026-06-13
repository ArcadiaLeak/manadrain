#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <flat_map>
#include <generator>
#include <inplace_vector>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
#include <meta>
#include <mutex>
#include <new>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <thread>
#include <variant>
#include <vector>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
class CompilationError : public std::runtime_error {
public:
  CompilationError(const char *msg) : std::runtime_error{msg} {}
  CompilationError() : std::runtime_error{"compilation error!"} {}
};
class UnexpectedStringEnd final : public CompilationError {
public:
  UnexpectedStringEnd() : CompilationError{"unexpected string end!"} {}
};
class UnexpectedToken final : public CompilationError {
public:
  UnexpectedToken() : CompilationError{"unexpected token!"} {}
};
class MissingPunctuation final : public CompilationError {
public:
  MissingPunctuation(char32_t ch)
      : CompilationError{"missing punctuation!"}, must_be{ch} {}
  char32_t must_be;
};
class MissingFormalParameter final : public CompilationError {
public:
  MissingFormalParameter() : CompilationError{"missing formal parameter!"} {}
};
class MissingFunctionName final : public CompilationError {
public:
  MissingFunctionName() : CompilationError{"missing function_name!"} {}
};
class DuplicateDeclaration final : public CompilationError {
public:
  DuplicateDeclaration() : CompilationError{"duplicate declaration!"} {}
};
class MissingPropertyName final : public CompilationError {
public:
  MissingPropertyName() : CompilationError{"missing property name!"} {}
};
class InvalidPropertyName final : public CompilationError {
public:
  InvalidPropertyName() : CompilationError{"invalid property name!"} {}
};
class InvalidPropertyAccess final : public CompilationError {
public:
  InvalidPropertyAccess() : CompilationError{"invalid property access!"} {}
};
class MissingVariableName final : public CompilationError {
public:
  MissingVariableName() : CompilationError{"missing variable name!"} {}
};
class InvalidMethodAccess final : public CompilationError {
public:
  InvalidMethodAccess() : CompilationError{"invalid method access!"} {}
};
class InvalidVariableAccess final : public CompilationError {
public:
  InvalidVariableAccess() : CompilationError{"invalid variable access!"} {}
};
class InvalidReturnStatement final : public CompilationError {
public:
  InvalidReturnStatement() : CompilationError{"invalid return statement!"} {}
};
class InvalidAssignment final : public CompilationError {
public:
  InvalidAssignment() : CompilationError{"invalid assignment!"} {}
};
class UnresolvableCircularity final : public CompilationError {
public:
  UnresolvableCircularity() : CompilationError{"unresolvable circularity!"} {}
};

enum class Keyword {
  MONOSTATE,
  K_CONST,
  K_LET,
  K_VAR,
  K_CLASS,
  K_FUNCTION,
  K_RETURN,
  K_IMPORT,
  K_EXPORT,
  K_FROM,
  K_AS,
  K_DEFAULT,
  K_UNDEFINED,
  K_NULL,
  K_TRUE,
  K_FALSE,
  K_IF,
  K_ELSE,
  K_WHILE,
  K_FOR,
  K_DO,
  K_BREAK,
  K_CONTINUE,
  K_SWITCH,
  K_TYPEOF
};
enum class Operator {
  DOUBLE_EQUALS,
  TRIPLE_EQUALS,
  BANG_EQUALS,
  BANG_DOUBLE_EQUALS,
  DIVIDE_ASSIGN,
  BITWISE_CONJUNCT_ASSIGN,
  LOGICAL_CONJUNCT_ASSIGN,
  LOGICAL_CONJUNCT,
  BITWISE_DISJUNCT_ASSIGN,
  LOGICAL_DISJUNCT_ASSIGN,
  LOGICAL_DISJUNCT
};

struct Identifier {
  std::size_t offset;
  auto operator<=>(const Identifier &) const = default;
};

using Token =
    std::variant<std::monostate, char32_t, std::int64_t, double, Operator,
                 Keyword, Identifier, std::string_view, std::u16string_view>;

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

class Machine {};

class LexicalBlock;
class FunctionDefinition;

class Tokenizer {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;

private:
  friend class Language;
  friend class LexicalBlock;
  friend class FunctionDefinition;

  std::map<std::string, Identifier> atom_atlas;
  std::set<std::string> token_strings;
  std::set<std::u16string> token_u16strings;

  std::size_t position;
  std::vector<std::optional<char32_t>> backtrace;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  Token tokenize_identifier(char32_t leading);
  Token tokenize_string_literal(char32_t separator);
  Token tokenize_numeric_literal(char32_t leading);

  Token last_token;
  void assert_punct(char32_t must_be);
  void tokenize();
};

using VariantString = std::variant<std::string, std::u16string>;
struct AnyExpression;
struct AnyStatement;

class ValueType {};
class ScopeAccessor final : ValueType {};
using AnyValueType = std::variant<ScopeAccessor>;

class LexicalBlock {
public:
  virtual ~LexicalBlock() = default;
  LexicalBlock(LexicalBlock &&) noexcept = default;
  LexicalBlock &operator=(LexicalBlock &&) noexcept = default;
  LexicalBlock() = default;

  class Parser;

  LexicalBlock *parent_block;
  std::list<AnyStatement> program;
  std::vector<std::byte> bytecode;

  std::list<FunctionDefinition> inner_functions;
  FunctionDefinition *find_function(Identifier identifier);

  std::flat_map<Identifier, AnyValueType> local_layout;
  ScopeAccessor find_local(Identifier identifier) const;

  AnyValueType return_type;

  void analyze();
  void serialize();

  const VariantString *push_string_literal(std::string_view ascii);
  const VariantString *push_string_literal(std::u16string_view unicode);

protected:
  void analyze_statements();
  void analyze_inner_functions();
};

class Expression {};

class AssignExpression final : public Expression {
public:
  AssignExpression(AnyExpression &&l, AnyExpression &&r);
  std::indirect<AnyExpression> left;
  std::indirect<AnyExpression> right;
};

class LogicalExpression final : public Expression {
public:
  LogicalExpression(AnyExpression &&l, AnyExpression &&r);
  std::indirect<AnyExpression> left;
  std::indirect<AnyExpression> right;
  Operator op;
};

class BinaryExpression final : public Expression {
public:
  std::indirect<AnyExpression> left;
  std::indirect<AnyExpression> right;
  char32_t op;
};

struct AnyExpression {
  std::variant<AssignExpression, LogicalExpression, BinaryExpression> alt;
};

AssignExpression::AssignExpression(AnyExpression &&l, AnyExpression &&r)
    : left{std::move(l)}, right{std::move(r)} {};

LogicalExpression::LogicalExpression(AnyExpression &&l, AnyExpression &&r)
    : left{std::move(l)}, right{std::move(r)} {};

class Statement {};

class InitializeVariable final : public Statement {
public:
  InitializeVariable(AnyExpression v) : rvalue{v} {};
  Identifier variable_name;
  std::indirect<AnyExpression> rvalue;
};

class ReturnStatement final : public Statement {
public:
  ReturnStatement(AnyExpression e) : argument{std::move(e)} {};
  std::indirect<AnyExpression> argument;
};

class ExpressionStatement final : public Statement {
public:
  ExpressionStatement(AnyExpression e) : argument{std::move(e)} {};
  std::indirect<AnyExpression> argument;
};

struct AnyStatement {
  std::variant<InitializeVariable, ReturnStatement, ExpressionStatement> alt;
};

class FunctionDefinition final : public LexicalBlock {
public:
  class Parser;

  enum class AnalyzerMark { PENDING, INITIATED, COMPLETE };
  AnalyzerMark analyzer_mark;
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;

  void analyze();

private:
  void analyze_formal_parameters();
};

class LexicalBlock::Parser {
public:
  virtual ~Parser() = default;
  Parser(LexicalBlock &b, Tokenizer &t) : block{b}, tokenizer{t} {}

  void parse_statement();
  void parse_function_stmt();
  void parse_variable_decl();

  AnyExpression parse_expression() { return parse_assign_expression(); }
  AnyExpression parse_primary_expression();

  AnyExpression parse_assign_expression();
  AnyExpression parse_logical_disjunct();
  AnyExpression parse_additive_expression();
  AnyExpression parse_object_literal();

  void parse_member_expression(AnyExpression &expression);
  void parse_function_call(AnyExpression &expression);
  AnyExpression parse_postfix_expression();

protected:
  LexicalBlock &block;
  Tokenizer &tokenizer;
};

class FunctionDefinition::Parser final : public LexicalBlock::Parser {
public:
  Parser(FunctionDefinition &d, Tokenizer &t) : LexicalBlock::Parser{d, t} {}
  void parse_function_decl();

private:
  FunctionDefinition &definition() {
    return static_cast<FunctionDefinition &>(block);
  }
};

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

std::optional<char32_t> Tokenizer::forward() {
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

std::generator<std::optional<char32_t>> Tokenizer::traverse_text() {
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

void Tokenizer::backward() {
  std::optional<char32_t> behind{backtrace.back()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_u8) | std::views::join);
  backtrace.pop_back();
}

void Tokenizer::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

Token Tokenizer::tokenize_identifier(char32_t leading) {
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

Token Tokenizer::tokenize_string_literal(char32_t separator) {
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

Token Tokenizer::tokenize_numeric_literal(char32_t leading) {
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

void Tokenizer::tokenize() {
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

void Tokenizer::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw MissingPunctuation{must_be};
}

void LexicalBlock::Parser::parse_statement() {
  Keyword keyword{};
  if (std::holds_alternative<Keyword>(tokenizer.last_token))
    keyword = std::get<Keyword>(tokenizer.last_token);
  switch (keyword) {
  case Keyword::K_FUNCTION:
    parse_function_stmt();
    return;
  case Keyword::K_LET:
    parse_variable_decl();
    return;
  case Keyword::K_RETURN: {
    tokenizer.tokenize();
    ReturnStatement statement{parse_expression()};
    block.program.emplace_back(std::move(statement));
    tokenizer.assert_punct(';');
    return;
  }
  default: {
    ExpressionStatement statement{parse_expression()};
    block.program.emplace_back(std::move(statement));
    tokenizer.assert_punct(';');
    return;
  }
  }
}

void LexicalBlock::Parser::parse_function_stmt() {
  FunctionDefinition definition{};
  FunctionDefinition::Parser definition_parser{definition, tokenizer};
  definition_parser.parse_function_decl();
  if (not definition.function_name)
    throw MissingFunctionName{};
  bool duplicate_exists =
      std::ranges::contains(block.inner_functions, definition.function_name,
                            [](const auto &el) { return el.function_name; });
  if (duplicate_exists)
    throw DuplicateDeclaration{};
  block.inner_functions.push_back(std::move(definition));
}

AnyExpression LexicalBlock::Parser::parse_assign_expression() {
  AnyExpression left_expression{parse_logical_disjunct()};
  if (tokenizer.last_token != Token{U'='})
    return left_expression;
  tokenizer.tokenize();
  AssignExpression expression{std::move(left_expression),
                              parse_assign_expression()};
  return AnyExpression{expression};
}

void LexicalBlock::Parser::parse_variable_decl() {
  tokenizer.tokenize();
  if (not std::holds_alternative<Identifier>(tokenizer.last_token))
    throw MissingVariableName{};
  Identifier variable_name{std::get<Identifier>(tokenizer.last_token)};
  tokenizer.tokenize();
  tokenizer.assert_punct('=');
  tokenizer.tokenize();
  InitializeVariable initialize_variable{parse_expression()};
  initialize_variable.variable_name = variable_name;
  block.program.emplace_back(std::move(initialize_variable));
  tokenizer.assert_punct(';');
  if (block.local_layout.contains(variable_name))
    throw DuplicateDeclaration{};
  block.local_layout.try_emplace(variable_name);
}

AnyExpression LexicalBlock::Parser::parse_logical_disjunct() {
  AnyExpression left_expression{parse_additive_expression()};
  while (tokenizer.last_token == Token{Operator::LOGICAL_DISJUNCT}) {
    Operator logical_op{std::get<Operator>(tokenizer.last_token)};
    tokenizer.tokenize();
    LogicalExpression expression{std::move(left_expression),
                                 parse_additive_expression()};
    expression.op = logical_op;
    left_expression.alt.emplace<LogicalExpression>(std::move(expression));
  }
  return left_expression;
}

AnyExpression LexicalBlock::Parser::parse_additive_expression() {
  std::unique_ptr left_expression{parse_postfix_expression()};
  if (tokenizer.last_token != Token{U'+'} &&
      tokenizer.last_token != Token{U'-'})
    return left_expression;
  std::unique_ptr expression{std::make_unique<BinaryExpression>()};
  expression->op = std::get<char32_t>(tokenizer.last_token);
  tokenizer.tokenize();
  expression->left = std::move(left_expression);
  expression->right = parse_postfix_expression();
  return expression;
}

AnyExpression LexicalBlock::Parser::parse_postfix_expression() {
  std::unique_ptr postfix_expression{parse_primary_expression()};
  bool hit_default{};
  while (not hit_default) {
    tokenizer.tokenize();
    if (not std::holds_alternative<char32_t>(tokenizer.last_token))
      break;
    switch (std::get<char32_t>(tokenizer.last_token)) {
    case '.':
      parse_member_expression(postfix_expression);
      break;
    case '(':
      parse_function_call(postfix_expression);
      break;
    default:
      hit_default = 1;
      break;
    }
  }
  return postfix_expression;
}

void FunctionDefinition::Parser::parse_function_decl() {
  tokenizer.tokenize();
  if (std::holds_alternative<Identifier>(tokenizer.last_token)) {
    definition().function_name = std::get<Identifier>(tokenizer.last_token);
    tokenizer.tokenize();
  }
  tokenizer.assert_punct('(');
  while (1) {
    tokenizer.tokenize();
    if (tokenizer.last_token == Token{U')'})
      break;
    if (not std::holds_alternative<Identifier>(tokenizer.last_token))
      throw MissingFormalParameter{};
    Identifier parameter{std::get<Identifier>(tokenizer.last_token)};
    block.local_layout.try_emplace(parameter);
    definition().arguments.push_back(parameter);
    tokenizer.tokenize();
    if (tokenizer.last_token == Token{U')'})
      break;
    tokenizer.assert_punct(',');
  }
  tokenizer.tokenize();
  tokenizer.assert_punct('{');
  while (1) {
    tokenizer.tokenize();
    if (std::holds_alternative<std::monostate>(tokenizer.last_token))
      throw MissingPunctuation{'}'};
    char32_t *alter_ptr{std::get_if<char32_t>(&tokenizer.last_token)};
    if (alter_ptr && *alter_ptr == '}')
      break;
    parse_statement();
  }
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

void Language::compile_and_execute() {
  Tokenizer tokenizer{};
  tokenizer.text_buffer = std::move(text_buffer);

  LexicalBlock block{};
  LexicalBlock::Parser block_parser{block, tokenizer};
  while (1) {
    tokenizer.tokenize();
    if (std::holds_alternative<std::monostate>(tokenizer.last_token))
      break;
    block_parser.parse_statement();
  }
}
} // namespace Manadrain
