#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <debugging>
#include <expected>
#include <flat_map>
#include <generator>
#include <inplace_vector>
#include <list>
#include <memory>
#include <memory_resource>
#include <mutex>
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
class UnexpectedStringEnd final : public LanguageError {
public:
  UnexpectedStringEnd() : LanguageError{"unexpected string end!"} {}
};
class UnexpectedToken final : public LanguageError {
public:
  UnexpectedToken() : LanguageError{"unexpected token!"} {}
};
class MissingPunctuation final : public LanguageError {
public:
  MissingPunctuation(char32_t ch)
      : LanguageError{"missing punctuation!"}, must_be{ch} {}
  char32_t must_be;
};
class MissingFormalParameter final : public LanguageError {
public:
  MissingFormalParameter() : LanguageError{"missing formal parameter!"} {}
};
class MissingFunctionName final : public LanguageError {
public:
  MissingFunctionName() : LanguageError{"missing function_name!"} {}
};
class DuplicateDeclaration final : public LanguageError {
public:
  DuplicateDeclaration() : LanguageError{"duplicate declaration!"} {}
};
class MissingPropertyName final : public LanguageError {
public:
  MissingPropertyName() : LanguageError{"missing property name!"} {}
};
class InvalidPropertyName final : public LanguageError {
public:
  InvalidPropertyName() : LanguageError{"invalid property name!"} {}
};
class InvalidPropertyAccess final : public LanguageError {
public:
  InvalidPropertyAccess() : LanguageError{"invalid property access!"} {}
};
class MissingVariableName final : public LanguageError {
public:
  MissingVariableName() : LanguageError{"missing variable name!"} {}
};
class InvalidMethodAccess final : public LanguageError {
public:
  InvalidMethodAccess() : LanguageError{"invalid method access!"} {}
};
class InvalidVariableAccess final : public LanguageError {
public:
  InvalidVariableAccess() : LanguageError{"invalid variable access!"} {}
};
class InvalidReturnStatement final : public LanguageError {
public:
  InvalidReturnStatement() : LanguageError{"invalid return statement!"} {}
};
class InvalidAssignment final : public LanguageError {
public:
  InvalidAssignment() : LanguageError{"invalid assignment!"} {}
};
class UnresolvableCircularity final : public LanguageError {
public:
  UnresolvableCircularity() : LanguageError{"unresolvable circularity!"} {}
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

class Tokenizer {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;

private:
  friend class Language;
  template <typename T> friend class Parser;
  friend class FunctionParser;
  friend class ModuleParser;

  std::flat_map<std::string, Identifier> atom_atlas;
  std::set<std::string> *string_atlas;
  std::set<std::u16string> *u16string_atlas;

  std::size_t position;
  std::vector<std::optional<char32_t>> backtrace;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  std::expected<Token, LanguageError *> tokenize_identifier(char32_t leading);
  std::expected<Token, LanguageError *>
  tokenize_string_literal(char32_t separator);
  std::expected<Token, LanguageError *>
  tokenize_numeric_literal(char32_t leading);

  Token last_token;
  std::expected<void, LanguageError *> assert_punct(char32_t must_be);
  std::expected<void, LanguageError *> tokenize();
};

struct AnyExpression;

class ValueType {
protected:
  ValueType() = default;

public:
  auto operator<=>(const ValueType &) const = default;
};

struct NumberType final : public ValueType {
  explicit NumberType() = default;
  auto operator<=>(const NumberType &) const = default;
  using Representation = double;
};

using VariantStringView = std::variant<std::string_view, std::u16string_view>;
struct StringType final : public ValueType {
  explicit StringType() = default;
  auto operator<=>(const StringType &) const = default;
  using Representation = VariantStringView;
};

struct DynamicValue {};
struct DynamicType final : public ValueType {
  explicit DynamicType() = default;
  auto operator<=>(const DynamicType &) const = default;
  using Representation = DynamicValue;
};

struct LambdaSignature;
struct LambdaDescriptor {};
struct LambdaType final : public ValueType {
  explicit LambdaType() = default;
  const LambdaSignature *signature;
  auto operator<=>(const LambdaType &) const = default;
  using Representation = LambdaDescriptor;
};

struct VariantType {
  using Alternative =
      std::variant<NumberType, StringType, LambdaType, DynamicType>;
  std::optional<Alternative> alt;
  VariantType() = default;
  VariantType(Alternative a) : alt{std::move(a)} {};
  auto operator<=>(const VariantType &) const = default;
};

struct LambdaSignature {
  std::vector<VariantType> parameter_types;
  VariantType return_type;
  auto operator<=>(const LambdaSignature &) const = default;
};

class ObjectShape {
public:
  std::flat_map<Identifier, VariantType> properties;
};

struct AnyStatement;

class FunctionDefinition;

struct LocalLayoutEntry {
  Identifier identifier;
  VariantType variant_type;
  std::size_t offset;
};

struct LocalLayout {
  std::vector<LocalLayoutEntry> entries;
  std::size_t largest_alignment;
  std::size_t total_bytes;
};

struct Program {
  std::pmr::monotonic_buffer_resource resource;
  std::pmr::list<AnyStatement> statements{&resource};
};

struct FunctionTape {
  std::list<FunctionDefinition> function_definition_list;
  std::vector<std::unique_ptr<Program>> all_programs;
};

class LexicalBoundary {
public:
  LexicalBoundary *parent_boundary;
  FunctionTape *tape;

  std::vector<FunctionDefinition *> inner_functions;

  std::size_t program_idx;
  LocalLayout local_layout;

  virtual FunctionDefinition *find_function(Identifier identifier) = 0;
  virtual std::optional<LocalLayoutEntry>
  find_local(Identifier identifier) const = 0;

  virtual FunctionDefinition *as_function_definition() = 0;
  virtual const FunctionDefinition *as_function_definition() const = 0;
};

template <typename T> class AbstractBoundary : public LexicalBoundary {
public:
  FunctionDefinition *find_function(Identifier identifier) override;
  std::optional<LocalLayoutEntry>
  find_local(Identifier identifier) const override;

protected:
  AbstractBoundary() = default;
  std::expected<void, LanguageError *> analyze_statements();
  std::expected<void, LanguageError *> analyze_inner_functions();
};

class FunctionDefinition final : public AbstractBoundary<FunctionDefinition> {
public:
  std::optional<Identifier> function_name;

  VariantType return_type;
  std::vector<Identifier> arguments;

  enum class AnalyzerMark { PENDING, INITIATED, COMPLETE };
  AnalyzerMark analyzer_mark;
  std::expected<void, LanguageError *> analyze();

  FunctionDefinition *as_function_definition() override { return this; }
  const FunctionDefinition *as_function_definition() const override {
    return this;
  }

private:
  std::expected<void, LanguageError *> do_analyze();
  std::expected<void, LanguageError *> analyze_formal_parameters();
};

class ModuleDefinition final : public AbstractBoundary<ModuleDefinition> {
public:
  std::expected<void, LanguageError *> analyze();

  FunctionDefinition *as_function_definition() override { return nullptr; }
  const FunctionDefinition *as_function_definition() const override {
    return nullptr;
  }
};

template <typename T> class Parser {
public:
  std::expected<void, LanguageError *> parse_function_stmt();
  std::expected<void, LanguageError *> parse_return_stmt();
  std::expected<void, LanguageError *> parse_expression_stmt();
  std::expected<void, LanguageError *> parse_variable_decl();
  std::expected<void, LanguageError *> parse_statement();

  std::expected<AnyExpression, LanguageError *> parse_expression();
  std::expected<AnyExpression, LanguageError *> parse_assign_expression();
  std::expected<AnyExpression, LanguageError *> parse_logical_disjunct();
  std::expected<AnyExpression, LanguageError *> parse_additive_expression();

protected:
  Parser(T &b, Tokenizer &t) : boundary{b}, tokenizer{t} {}
  T &boundary;
  Tokenizer &tokenizer;

private:
  struct PostfixExpressionSaga {
    std::expected<AnyExpression, LanguageError *>
    parse_member_expression(AnyExpression expression);
    std::expected<AnyExpression, LanguageError *>
    parse_function_call(AnyExpression expression);
    std::expected<AnyExpression, LanguageError *>
    operator()(AnyExpression expression);
    Parser &parser;
  };
  std::expected<AnyExpression, LanguageError *> parse_postfix_expression();

  struct PrimaryExpressionSaga;
  std::expected<AnyExpression, LanguageError *> parse_primary_expression();

  friend class Language;
};

class FunctionParser final : public Parser<FunctionDefinition> {
public:
  FunctionParser(FunctionDefinition &b, Tokenizer &t) : Parser{b, t} {}
  std::expected<void, LanguageError *> parse_function_decl();
};
class ModuleParser final : public Parser<ModuleDefinition> {
public:
  ModuleParser(ModuleDefinition &b, Tokenizer &t) : Parser{b, t} {}
  std::expected<void, LanguageError *> parse_module();
};

class Expression {
protected:
  Expression() = default;
};

class AliasedFunctionCall final : public Expression {
public:
  AliasedFunctionCall(std::pmr::memory_resource *resource_ptr)
      : callee{std::allocator_arg, resource_ptr}, arguments{resource_ptr} {};
  std::pmr::indirect<AnyExpression> callee;
  std::pmr::vector<AnyExpression> arguments;
};

template <typename T> class DirectFunctionCall final : public Expression {
public:
  DirectFunctionCall(std::pmr::memory_resource *resource_ptr)
      : passed_identifiers{resource_ptr}, passed_values{resource_ptr} {}
  const FunctionDefinition *callee;
  T return_type;
  std::pmr::vector<Identifier> passed_identifiers;
  std::pmr::vector<AnyExpression> passed_values;
};

class MemberExpression final : public Expression {
public:
  MemberExpression(std::pmr::memory_resource *resource_ptr)
      : object{std::allocator_arg, resource_ptr} {}
  std::pmr::indirect<AnyExpression> object;
  Identifier property;
};

class AssignExpression final : public Expression {
public:
  AssignExpression(std::pmr::memory_resource *resource_ptr)
      : left{std::allocator_arg, resource_ptr},
        right{std::allocator_arg, resource_ptr} {}
  std::pmr::indirect<AnyExpression> left;
  std::pmr::indirect<AnyExpression> right;
};

class LogicalExpression final : public Expression {
public:
  LogicalExpression(std::pmr::memory_resource *resource_ptr)
      : left{std::allocator_arg, resource_ptr},
        right{std::allocator_arg, resource_ptr} {}
  std::pmr::indirect<AnyExpression> left;
  std::pmr::indirect<AnyExpression> right;
  Operator op;
};

template <char32_t, typename T>
class BinaryExpression final : public Expression {
public:
  BinaryExpression(std::pmr::memory_resource *resource_ptr)
      : left{std::allocator_arg, resource_ptr},
        right{std::allocator_arg, resource_ptr} {}
  std::pmr::indirect<AnyExpression> left;
  std::pmr::indirect<AnyExpression> right;
};

class VariableAccessor final : public Expression {
public:
  VariableAccessor() = default;
  Identifier identifier;
  AnyExpression find_local_linkedly(const LexicalBoundary *boundary,
                                    std::size_t scope_offset = 0) const;
  FunctionDefinition *find_function_linkedly(LexicalBoundary *boundary) const;
};

template <typename T> class ScopeAccessor final : public Expression {
public:
  ScopeAccessor() = default;
  T local_type;
  std::size_t scope_offset;
  std::size_t local_offset;
};

enum class IntrinsicObject { O_CONSOLE };
class IntrinsicAccessor final : public Expression {
public:
  IntrinsicAccessor() = default;
  IntrinsicObject object_type;
};

class NumericLiteral final : public Expression {
public:
  NumericLiteral() = default;
  double val;
};

class AsciiLiteral final : public Expression {
public:
  AsciiLiteral() = default;
  std::string_view val_view;
};

class UnicodeLiteral final : public Expression {
public:
  UnicodeLiteral() = default;
  std::u16string_view val_view;
};

class LambdaExpression final : public Expression {
public:
  LambdaExpression() = default;
  FunctionDefinition *definition;
};

class ConsoleCall final : public Expression {
public:
  ConsoleCall(std::pmr::memory_resource *resource_ptr)
      : arguments{resource_ptr} {}
  std::pmr::list<AnyExpression> arguments;
};

class LengthIntrinsic final : public Expression {
public:
  LengthIntrinsic(std::pmr::memory_resource *resource_ptr)
      : argument{std::allocator_arg, resource_ptr} {}
  std::pmr::indirect<AnyExpression> argument;
};

class StringifyIntrinsic final : public Expression {
public:
  StringifyIntrinsic(std::pmr::memory_resource *resource_ptr)
      : argument{std::allocator_arg, resource_ptr} {}
  std::pmr::indirect<AnyExpression> argument;
};

struct AnyExpression {
  using Alternative = std::variant<
      NumericLiteral, AsciiLiteral, UnicodeLiteral, AliasedFunctionCall,
      DirectFunctionCall<NumberType>, MemberExpression, AssignExpression,
      LogicalExpression, BinaryExpression<'+', NumberType>,
      BinaryExpression<'-', NumberType>, BinaryExpression<'+', DynamicType>,
      BinaryExpression<'-', DynamicType>, VariableAccessor,
      ScopeAccessor<StringType>, ScopeAccessor<NumberType>, IntrinsicAccessor,
      LambdaExpression, ConsoleCall, LengthIntrinsic, StringifyIntrinsic>;
  std::optional<Alternative> alt;
  AnyExpression() = default;
  AnyExpression(Alternative a) : alt{std::move(a)} {};

  AnyExpression(AnyExpression &&other) noexcept = default;
  AnyExpression &operator=(AnyExpression &&other) noexcept = default;

  AnyExpression(const AnyExpression &other) = delete;
  AnyExpression &operator=(const AnyExpression &other) = delete;
};

class Statement {
protected:
  Statement() = default;
};

class InitializeVariable final : public Statement {
public:
  InitializeVariable(std::pmr::memory_resource *resource_ptr)
      : rvalue{std::allocator_arg, resource_ptr} {};
  Identifier variable_name;
  std::pmr::indirect<AnyExpression> rvalue;
};

template <typename T> class InitializeScope final : public Statement {
public:
  InitializeScope(std::pmr::memory_resource *resource_ptr)
      : rvalue{std::allocator_arg, resource_ptr} {};
  std::size_t local_offset;
  std::pmr::indirect<AnyExpression> rvalue;
  T datatype;
};

template <typename T> class ReturnStatement final : public Statement {
public:
  ReturnStatement(std::pmr::memory_resource *resource_ptr)
      : argument{std::allocator_arg, resource_ptr} {};
  std::pmr::indirect<AnyExpression> argument;
};

class ExpressionStatement final : public Statement {
public:
  ExpressionStatement(std::pmr::memory_resource *resource_ptr)
      : argument{std::allocator_arg, resource_ptr} {};
  std::pmr::indirect<AnyExpression> argument;
};

struct AnyStatement {
  using Alternative =
      std::variant<InitializeVariable, InitializeScope<NumberType>,
                   InitializeScope<StringType>, ReturnStatement<void>,
                   ReturnStatement<NumberType>, ExpressionStatement>;
  std::optional<Alternative> alt;
  AnyStatement() = default;
  AnyStatement(Alternative a) : alt{std::move(a)} {};

  AnyStatement(AnyStatement &&other) noexcept = default;
  AnyStatement &operator=(AnyStatement &&other) noexcept = default;

  AnyStatement(const AnyStatement &other) = delete;
  AnyStatement &operator=(const AnyStatement &other) = delete;
};

struct ConsoleMessage {
  std::vector<VariantStringView> parts;
  std::string encode_for_print() const;
};

struct RuntimeBytesDeleter {
  std::pmr::memory_resource *resource;
  std::size_t total_bytes;
  std::size_t alignment;
  void operator()(void *ptr_bytes) const noexcept;
};

void RuntimeBytesDeleter::operator()(void *ptr_bytes) const noexcept {
  if (not ptr_bytes)
    return;
  resource->deallocate(ptr_bytes, total_bytes, alignment);
}

class BoundaryFrame {
public:
  BoundaryFrame(Machine &m, const LexicalBoundary &b,
                std::pmr::memory_resource *r);
  Machine &machine;
  const LexicalBoundary &boundary;
  std::pmr::vector<BoundaryFrame *> closure_stack;
  std::unique_ptr<void, RuntimeBytesDeleter> local_memory;
  std::unique_ptr<void, RuntimeBytesDeleter> return_memory;
};

struct RuntimeMemory {
  std::pmr::monotonic_buffer_resource resource{65536};
  std::pmr::list<BoundaryFrame> boundary_frames{&resource};
  std::pmr::list<std::string> strings{&resource};
};

class Machine {
public:
  Machine() : runtime_memory{std::make_unique<RuntimeMemory>()} {}
  std::list<ConsoleMessage> collect_console_messages(std::stop_token stopper);
  std::expected<void, LanguageError *>
  evaluate(const LexicalBoundary &boundary);

private:
  friend class BoundaryFrame;
  template <typename T> friend struct ExpressionEvaluator;
  friend struct StatementEvaluator;

  std::unique_ptr<RuntimeMemory> runtime_memory;
  std::mutex console_mutex;
  std::condition_variable_any console_condition;
  std::list<ConsoleMessage> console_messages;
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

std::expected<Token, LanguageError *>
Tokenizer::tokenize_identifier(char32_t leading) {
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
  Identifier identifier{persistent_identifiers.size() + atom_atlas.size()};
  auto emplace_ret =
      atom_atlas.try_emplace(std::move(identifier_str), identifier);
  return emplace_ret.first->second;
}

std::expected<Token, LanguageError *>
Tokenizer::tokenize_string_literal(char32_t separator) {
  std::u16string u16_literal{};
  auto match_literal_end = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view =
      traverse_text() | std::views::take_while(match_literal_end);
  for (std::optional<char32_t> leading : literal_view) {
    if (leading == '\r' && forward() != '\n')
      backward();
    if (not leading || leading == '\r' || leading == '\n') {
      std::breakpoint();
      return std::unexpected(new UnexpectedStringEnd{});
    }
    u16_literal.append_range(traverse_u16(*leading));
  }
  if (std::ranges::all_of(u16_literal, [](char16_t ch) { return ch < 128; })) {
    auto cast_to_ascii = [](char16_t ch) { return static_cast<char>(ch); };
    auto ascii_string = u16_literal | std::views::transform(cast_to_ascii);
    auto [string_iter, _] =
        string_atlas->emplace(std::from_range, ascii_string);
    return *string_iter;
  }
  auto [u16string_iter, _] = u16string_atlas->insert(std::move(u16_literal));
  return *u16string_iter;
}

std::expected<Token, LanguageError *>
Tokenizer::tokenize_numeric_literal(char32_t leading) {
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

std::expected<void, LanguageError *> Tokenizer::tokenize() {
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
      return std::expected<void, LanguageError *>{};
    } else
      backward();
    if (std::ranges::binary_search(std::to_array({'"', '\'', '`'}), leading)) {
      std::expected token_opt{tokenize_string_literal(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return std::expected<void, LanguageError *>{};
      } else
        return std::unexpected(token_opt.error());
    }
    if (uc_is_property_xid_start(leading) || leading == '_') {
      std::expected token_opt{tokenize_identifier(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return std::expected<void, LanguageError *>{};
      } else
        return std::unexpected(token_opt.error());
    }
    static const std::array legal_punct{std::to_array<char32_t>(
        {'(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '=', '{', '}'})};
    if (std::ranges::binary_search(legal_punct, leading)) {
      last_token.emplace<char32_t>(leading);
      return std::expected<void, LanguageError *>{};
    }
    if (std::isdigit(leading)) {
      std::expected token_opt{tokenize_numeric_literal(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return std::expected<void, LanguageError *>{};
      } else
        return std::unexpected(token_opt.error());
    }
    std::breakpoint();
    return std::unexpected(new UnexpectedToken{});
  }
  last_token.emplace<std::monostate>();
  return std::expected<void, LanguageError *>{};
}

std::expected<void, LanguageError *> Tokenizer::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return std::expected<void, LanguageError *>{};
  else {
    std::breakpoint();
    return std::unexpected(new MissingPunctuation{must_be});
  }
}

std::expected<void, LanguageError *> FunctionParser::parse_function_decl() {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
  if (not std::holds_alternative<Identifier>(tokenizer.last_token))
    goto after_name;
  boundary.function_name = std::get<Identifier>(tokenizer.last_token);
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
after_name:
  if (auto void_opt = tokenizer.assert_punct('('); not void_opt)
    return std::unexpected(void_opt.error());
formal_parameter: {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
  if (tokenizer.last_token == Token{U')'})
    goto after_parameters;
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    return std::unexpected(new MissingFormalParameter{});
  }
  Identifier parameter{std::get<Identifier>(tokenizer.last_token)};
  auto &layout_entry = boundary.local_layout.entries.emplace_back();
  layout_entry.identifier = parameter;
  boundary.arguments.push_back(parameter);
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
  if (tokenizer.last_token == Token{U')'})
    goto after_parameters;
  if (auto void_opt = tokenizer.assert_punct(','); not void_opt)
    return std::unexpected(void_opt.error());
  goto formal_parameter;
}
after_parameters:
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
  if (auto void_opt = tokenizer.assert_punct('{'); not void_opt)
    return std::unexpected(void_opt.error());
body_statement: {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
  if (std::holds_alternative<std::monostate>(tokenizer.last_token)) {
    std::breakpoint();
    return std::unexpected(new MissingPunctuation{'}'});
  }
  char32_t *alter_ptr{std::get_if<char32_t>(&tokenizer.last_token)};
  if (alter_ptr && *alter_ptr == '}')
    return std::expected<void, LanguageError *>{};
  if (auto void_opt = parse_statement(); not void_opt)
    return std::unexpected(void_opt.error());
  goto body_statement;
}
}

std::expected<void, LanguageError *> ModuleParser::parse_module() {
  while (1) {
    if (auto token_result = tokenizer.tokenize(); not token_result)
      return std::unexpected(token_result.error());
    if (std::holds_alternative<std::monostate>(tokenizer.last_token))
      return std::expected<void, LanguageError *>{};
    if (auto statement_result = parse_statement(); not statement_result)
      return std::unexpected(statement_result.error());
  }
}

template <typename T>
std::expected<void, LanguageError *> Parser<T>::parse_statement() {
  Keyword keyword{};
  if (std::holds_alternative<Keyword>(tokenizer.last_token))
    keyword = std::get<Keyword>(tokenizer.last_token);
  switch (keyword) {
  case Keyword::K_FUNCTION:
    return parse_function_stmt();
  case Keyword::K_LET:
    return parse_variable_decl();
  case Keyword::K_RETURN:
    return parse_return_stmt();
  default:
    return parse_expression_stmt();
  }
}

template <typename T>
std::expected<void, LanguageError *> Parser<T>::parse_return_stmt() {
  if (auto token_opt = tokenizer.tokenize(); not token_opt)
    return std::unexpected(token_opt.error());
  if (auto expression_result = parse_expression(); not expression_result)
    return std::unexpected(expression_result.error());
  else {
    auto &program = *boundary.tape->all_programs[boundary.program_idx];
    auto &any_statement = program.statements.emplace_back();
    any_statement.alt.emplace(std::in_place_type<ReturnStatement<void>>,
                              &program.resource);
    auto &return_statement =
        std::get<ReturnStatement<void>>(*any_statement.alt);
    return_statement.argument = std::move(*expression_result);
    return tokenizer.assert_punct(';');
  }
}

template <typename T>
std::expected<void, LanguageError *> Parser<T>::parse_expression_stmt() {
  if (auto expression_result = parse_expression(); not expression_result)
    return std::unexpected(expression_result.error());
  else {
    auto &program = *boundary.tape->all_programs[boundary.program_idx];
    auto &any_statement = program.statements.emplace_back();
    any_statement.alt.emplace(std::in_place_type<ExpressionStatement>,
                              &program.resource);
    auto &expression_statement =
        std::get<ExpressionStatement>(*any_statement.alt);
    expression_statement.argument = std::move(*expression_result);
    return tokenizer.assert_punct(';');
  }
}

template <typename T>
std::expected<void, LanguageError *> Parser<T>::parse_function_stmt() {
  FunctionDefinition *definition{
      &boundary.tape->function_definition_list.emplace_back()};
  definition->parent_boundary = &boundary;
  definition->program_idx = boundary.tape->all_programs.size();
  boundary.tape->all_programs.push_back(std::make_unique<Program>());
  definition->tape = boundary.tape;
  FunctionParser function_parser{*definition, tokenizer};
  if (auto void_opt = function_parser.parse_function_decl(); not void_opt)
    return std::unexpected(void_opt.error());
  if (not definition->function_name) {
    std::breakpoint();
    return std::unexpected(new MissingFunctionName{});
  }
  bool duplicate_exists =
      std::ranges::contains(boundary.inner_functions, definition->function_name,
                            [](const auto &el) { return el->function_name; });
  if (duplicate_exists) {
    std::breakpoint();
    return std::unexpected(new DuplicateDeclaration{});
  } else {
    boundary.inner_functions.push_back(definition);
    return std::expected<void, LanguageError *>{};
  }
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::parse_assign_expression() {
  auto left_expression = parse_logical_disjunct();
  if (not left_expression)
    return std::unexpected(left_expression.error());
  if (tokenizer.last_token != Token{U'='})
    return left_expression;
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  auto expression_result = parse_assign_expression();
  if (not expression_result)
    return std::unexpected(expression_result.error());
  AssignExpression assign_expression{
      &boundary.tape->all_programs[boundary.program_idx]->resource};
  assign_expression.left = std::move(*left_expression);
  assign_expression.right = std::move(*expression_result);
  return AnyExpression{std::move(assign_expression)};
}

template <typename T>
std::expected<void, LanguageError *> Parser<T>::parse_variable_decl() {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return std::unexpected(void_opt.error());
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    return std::unexpected(new MissingVariableName{});
  }
  Identifier variable_name{std::get<Identifier>(tokenizer.last_token)};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  if (auto assert_result = tokenizer.assert_punct('='); not assert_result)
    return std::unexpected(assert_result.error());
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  if (auto expression_result = parse_expression(); not expression_result)
    return std::unexpected(expression_result.error());
  else {
    auto &program = *boundary.tape->all_programs[boundary.program_idx];
    auto &any_statement = program.statements.emplace_back();
    any_statement.alt.emplace(std::in_place_type<InitializeVariable>,
                              &program.resource);
    auto &initialize_variable =
        std::get<InitializeVariable>(*any_statement.alt);
    initialize_variable.variable_name = variable_name;
    initialize_variable.rvalue = std::move(*expression_result);
  }
  if (auto assert_result = tokenizer.assert_punct(';'); not assert_result)
    return std::unexpected(assert_result.error());
  if (std::ranges::contains(boundary.local_layout.entries, variable_name,
                            &LocalLayoutEntry::identifier)) {
    std::breakpoint();
    return std::unexpected(new DuplicateDeclaration{});
  } else {
    auto &layout_entry = boundary.local_layout.entries.emplace_back();
    layout_entry.identifier = variable_name;
    return std::expected<void, LanguageError *>{};
  }
}

template <typename T>
std::expected<AnyExpression, LanguageError *> Parser<T>::parse_expression() {
  return parse_assign_expression();
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::parse_logical_disjunct() {
  auto left_expression = parse_additive_expression();
  if (not left_expression)
    return std::unexpected(left_expression.error());
right_expression: {
  if (tokenizer.last_token != Token{Operator::LOGICAL_DISJUNCT})
    return left_expression;
  Operator logical_op{std::get<Operator>(tokenizer.last_token)};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  if (auto expression_result = parse_additive_expression();
      not expression_result)
    return std::unexpected(expression_result.error());
  else {
    LogicalExpression expression{
        &boundary.tape->all_programs[boundary.program_idx]->resource};
    expression.left = std::move(*left_expression);
    expression.right = std::move(*expression_result);
    expression.op = logical_op;
    left_expression->alt.template emplace<LogicalExpression>(
        std::move(expression));
  }
  goto right_expression;
}
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::PostfixExpressionSaga::operator()(AnyExpression expression) {
  if (auto token_result = parser.tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  if (auto *punctuation = std::get_if<char32_t>(&parser.tokenizer.last_token);
      not punctuation)
    return expression;
  else if (*punctuation == '.')
    return parse_member_expression(std::move(expression));
  else if (*punctuation == '(')
    return parse_function_call(std::move(expression));
  else
    return expression;
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::PostfixExpressionSaga::parse_function_call(
    AnyExpression expression) {
  auto &program =
      *parser.boundary.tape->all_programs[parser.boundary.program_idx];
  AliasedFunctionCall function_call{&program.resource};
  function_call.callee = std::move(expression);
  while (1) {
    if (auto token_result = parser.tokenizer.tokenize(); not token_result)
      return std::unexpected(token_result.error());
    if (parser.tokenizer.last_token == Token{U')'})
      break;
    auto argument_expression = parser.parse_expression();
    if (not argument_expression)
      return std::unexpected(argument_expression.error());
    function_call.arguments.push_back(std::move(*argument_expression));
    if (parser.tokenizer.last_token == Token{U')'})
      break;
    auto assert_result = parser.tokenizer.assert_punct(',');
    if (not assert_result)
      return std::unexpected(assert_result.error());
  }
  return std::expected<AnyExpression, LanguageError *>(std::in_place,
                                                       std::move(function_call))
      .and_then(*this);
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::PostfixExpressionSaga::parse_member_expression(
    AnyExpression expression) {
  if (auto token_result = parser.tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  if (auto *field_name = std::get_if<Identifier>(&parser.tokenizer.last_token);
      not field_name) {
    std::breakpoint();
    return std::unexpected(new MissingPropertyName{});
  } else {
    auto &program =
        *parser.boundary.tape->all_programs[parser.boundary.program_idx];
    MemberExpression member_expression(&program.resource);
    member_expression.property = *field_name;
    member_expression.object = std::move(expression);
    return std::expected<AnyExpression, LanguageError *>(
               std::in_place, std::move(member_expression))
        .and_then(*this);
  }
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::parse_postfix_expression() {
  return parse_primary_expression().and_then(PostfixExpressionSaga(*this));
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::parse_additive_expression() {
  auto make_expression = [this]<char32_t C>(AnyExpression &lhs_expression,
                                            AnyExpression &rhs_expression) {
    auto &program = *boundary.tape->all_programs[boundary.program_idx];
    BinaryExpression<C, DynamicType> binary_expression(&program.resource);
    binary_expression.left = std::move(lhs_expression);
    binary_expression.right = std::move(rhs_expression);
    return AnyExpression{std::move(binary_expression)};
  };
  auto lhs_expression = parse_postfix_expression();
  if (not lhs_expression)
    return std::unexpected(lhs_expression.error());
  auto *operator_ahead = std::get_if<char32_t>(&tokenizer.last_token);
  static const std::array additive_punct = std::to_array<char32_t>({'+', '-'});
  bool should_proceed =
      operator_ahead && std::ranges::contains(additive_punct, *operator_ahead);
  if (not should_proceed)
    return std::move(*lhs_expression);
  char32_t binary_op{*operator_ahead};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return std::unexpected(token_result.error());
  auto rhs_expression = parse_postfix_expression();
  if (not rhs_expression)
    return std::unexpected(rhs_expression.error());
  switch (binary_op) {
  case '+':
    return make_expression.template operator()<'+'>(*lhs_expression,
                                                    *rhs_expression);
  case '-':
    return make_expression.template operator()<'-'>(*lhs_expression,
                                                    *rhs_expression);
  }
  std::unreachable();
}

template <typename T> class Parser<T>::PrimaryExpressionSaga {
private:
  Parser &parser;

public:
  PrimaryExpressionSaga(Parser &p) : parser{p} {}

  std::expected<AnyExpression, LanguageError *> operator()(char32_t punct);
  std::expected<AnyExpression, LanguageError *> operator()(Keyword keyword);

  std::expected<AnyExpression, LanguageError *> operator()(std::monostate) {
    std::breakpoint();
    return std::unexpected(new UnexpectedToken{});
  }

  std::expected<AnyExpression, LanguageError *>
  operator()(std::int64_t number) {
    NumericLiteral num_literal{};
    num_literal.val = static_cast<double>(number);
    return AnyExpression{num_literal};
  }

  std::expected<AnyExpression, LanguageError *> operator()(double number) {
    NumericLiteral numeric_literal{};
    numeric_literal.val = number;
    return AnyExpression{numeric_literal};
  }

  std::expected<AnyExpression, LanguageError *> operator()(Operator op) {
    std::breakpoint();
    return std::unexpected(new UnexpectedToken{});
  }

  std::expected<AnyExpression, LanguageError *>
  operator()(Identifier identifier) {
    VariableAccessor variable_accessor{};
    variable_accessor.identifier = identifier;
    return AnyExpression{variable_accessor};
  }

  std::expected<AnyExpression, LanguageError *>
  operator()(std::string_view ascii_view) {
    AsciiLiteral ascii_literal{};
    ascii_literal.val_view = ascii_view;
    return AnyExpression{ascii_literal};
  }

  std::expected<AnyExpression, LanguageError *>
  operator()(std::u16string_view unicode_view) {
    UnicodeLiteral unicode_literal{};
    unicode_literal.val_view = unicode_view;
    return AnyExpression{unicode_literal};
  }
};

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::PrimaryExpressionSaga::operator()(char32_t punct) {
  std::breakpoint();
  return std::unexpected(new UnexpectedToken{});
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::PrimaryExpressionSaga::operator()(Keyword keyword) {
  if (keyword != Keyword::K_FUNCTION) {
    std::breakpoint();
    return std::unexpected(new UnexpectedToken{});
  }
  LambdaExpression lambda_expression{};
  lambda_expression.definition =
      &parser.boundary.tape->function_definition_list.emplace_back();
  lambda_expression.definition->parent_boundary = &parser.boundary;
  lambda_expression.definition->program_idx =
      parser.boundary.tape->all_programs.size();
  parser.boundary.tape->all_programs.push_back(std::make_unique<Program>());
  lambda_expression.definition->tape = parser.boundary.tape;
  FunctionParser function_parser{*lambda_expression.definition,
                                 parser.tokenizer};
  if (auto void_opt = function_parser.parse_function_decl(); not void_opt)
    return std::unexpected(void_opt.error());
  return AnyExpression{lambda_expression};
}

template <typename T>
std::expected<AnyExpression, LanguageError *>
Parser<T>::parse_primary_expression() {
  return tokenizer.last_token.visit(PrimaryExpressionSaga(*this));
}

template <typename T>
std::optional<LocalLayoutEntry>
AbstractBoundary<T>::find_local(Identifier identifier) const {
  auto placed_before = [](const auto &e) {
    return e.variant_type.alt.has_value();
  };
  auto entries = local_layout.entries | std::views::take_while(placed_before);
  auto entry_it =
      std::ranges::find(entries, identifier, &LocalLayoutEntry::identifier);
  return entry_it != entries.end() ? std::make_optional(*entry_it)
                                   : std::nullopt;
}

template <typename T>
FunctionDefinition *AbstractBoundary<T>::find_function(Identifier identifier) {
  auto by_name = [](auto definition) { return definition->function_name; };
  auto function_it = std::ranges::find(inner_functions, identifier, by_name);
  return function_it == inner_functions.end() ? nullptr : *function_it;
}

template <typename T>
std::expected<void, LanguageError *>
AbstractBoundary<T>::analyze_inner_functions() {
  for (FunctionDefinition *definition : inner_functions) {
    definition->parent_boundary = this;
    if (auto void_opt = definition->analyze(); not void_opt)
      return std::unexpected(void_opt.error());
  }
  return std::expected<void, LanguageError *>{};
}

struct PropertyFinder {
  std::expected<AnyExpression, LanguageError *> operator()(StringType);
  template <std::derived_from<ValueType> T>
  std::expected<AnyExpression, LanguageError *> operator()(T) {
    std::unreachable();
  }

  LexicalBoundary &boundary;
  AnyExpression source_expression;
  Identifier identifier;
  PropertyFinder(LexicalBoundary &b, AnyExpression e)
      : boundary{b}, source_expression{std::move(e)} {}
};

struct RvalueAnalyzer;

struct CalleeAnalyzer {
  std::expected<AnyExpression, LanguageError *>
  operator()(const MemberExpression &expression);
  std::expected<AnyExpression, LanguageError *>
  operator()(const VariableAccessor &expression);
  template <std::derived_from<Expression> T>
  std::expected<AnyExpression, LanguageError *>
  operator()(const T &expression) {
    std::unreachable();
  }

  CalleeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  std::span<const AnyExpression> arguments;
};

struct RvalueAnalyzer {
  std::expected<AnyExpression, LanguageError *>
  operator()(const AsciiLiteral &ascii_literal) {
    return AnyExpression{ascii_literal};
  }
  std::expected<AnyExpression, LanguageError *>
  operator()(const NumericLiteral &numeric_literal) {
    return AnyExpression{numeric_literal};
  }
  std::expected<AnyExpression, LanguageError *>
  operator()(const AliasedFunctionCall &expression);
  std::expected<AnyExpression, LanguageError *>
  operator()(const VariableAccessor &expression);
  template <char32_t C>
  std::expected<AnyExpression, LanguageError *>
  operator()(const BinaryExpression<C, DynamicType> &expression);
  std::expected<AnyExpression, LanguageError *>
  operator()(const MemberExpression &expression);
  template <std::derived_from<Expression> T>
  std::expected<AnyExpression, LanguageError *>
  operator()(const T &expression) {
    std::unreachable();
  }

  RvalueAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

struct MethodAnalyzer {
  std::expected<AnyExpression, LanguageError *>
  operator()(const IntrinsicAccessor &intrinsic_accessor);
  template <std::derived_from<Expression> T>
  std::expected<AnyExpression, LanguageError *>
  operator()(const T &expression) {
    std::unreachable();
  }

  MethodAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  Identifier identifier;
  std::span<const AnyExpression> arguments;
};

struct DatatypeAnalyzer {
  VariantType operator()(const NumericLiteral &expression) {
    return VariantType{NumberType{}};
  }
  VariantType operator()(const LengthIntrinsic &expression) {
    return VariantType{NumberType{}};
  }
  VariantType operator()(const AsciiLiteral &expression) {
    return VariantType{StringType{}};
  }
  VariantType operator()(const UnicodeLiteral &expression) {
    return VariantType{StringType{}};
  }
  template <typename T>
  VariantType operator()(const ScopeAccessor<T> &accessor) {
    return VariantType{accessor.local_type};
  }
  template <char32_t C, typename T>
  VariantType operator()(const BinaryExpression<C, T> &expression) {
    return VariantType{T{}};
  }
  template <typename T>
  VariantType operator()(const DirectFunctionCall<T> &direct_call) {
    return VariantType{direct_call.return_type};
  }
  template <std::derived_from<Expression> T>
  VariantType operator()(const T &expression) {
    std::unreachable();
  }

  DatatypeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

std::expected<AnyExpression, LanguageError *>
PropertyFinder::operator()(StringType) {
  switch (identifier.offset) {
  case OFFSET_length: {
    LengthIntrinsic length_intrinsic{
        &boundary.tape->all_programs[boundary.program_idx]->resource};
    length_intrinsic.argument = std::move(source_expression);
    return AnyExpression{std::move(length_intrinsic)};
  }
  default:
    std::breakpoint();
    return std::unexpected(new InvalidPropertyAccess{});
  }
}

std::expected<AnyExpression, LanguageError *>
RvalueAnalyzer::operator()(const AliasedFunctionCall &function_call) {
  CalleeAnalyzer callee_visitor{boundary};
  callee_visitor.arguments = function_call.arguments;
  return function_call.callee->alt->visit(callee_visitor);
}

std::expected<AnyExpression, LanguageError *>
RvalueAnalyzer::operator()(const VariableAccessor &variable_accessor) {
  AnyExpression scope_accessor{
      variable_accessor.find_local_linkedly(&boundary)};
  if (scope_accessor.alt)
    return scope_accessor;
  switch (variable_accessor.identifier.offset) {
  case OFFSET_console: {
    IntrinsicAccessor intrinsic_accessor{};
    intrinsic_accessor.object_type = IntrinsicObject::O_CONSOLE;
    return AnyExpression{intrinsic_accessor};
  }
  default:
    std::breakpoint();
    return std::unexpected(new InvalidVariableAccess{});
  }
}

template <char32_t> struct BinaryExpressionAnalyzer {
  template <typename T, typename U>
  AnyExpression operator()(T lhs_datatype, U rhs_datatype) {
    std::unreachable();
  }
  AnyExpression operator()(NumberType, NumberType);
  AnyExpression lhs_analyzed;
  AnyExpression rhs_analyzed;

  BinaryExpressionAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

template <char32_t C>
AnyExpression BinaryExpressionAnalyzer<C>::operator()(NumberType, NumberType) {
  Program &program = *boundary.tape->all_programs[boundary.program_idx];
  BinaryExpression<C, NumberType> output_expression(&program.resource);
  output_expression.left = std::move(lhs_analyzed);
  output_expression.right = std::move(rhs_analyzed);
  return AnyExpression{std::move(output_expression)};
}

template <char32_t C>
std::expected<AnyExpression, LanguageError *>
RvalueAnalyzer::operator()(const BinaryExpression<C, DynamicType> &expression) {
  DatatypeAnalyzer datatype_visitor{boundary};
  std::expected<AnyExpression, LanguageError *> lhs_analyzed{
      expression.left->alt->visit(*this)};
  if (not lhs_analyzed)
    return std::unexpected(lhs_analyzed.error());
  VariantType lhs_datatype{lhs_analyzed->alt->visit(datatype_visitor)};
  std::expected<AnyExpression, LanguageError *> rhs_analyzed{
      expression.right->alt->visit(*this)};
  if (not rhs_analyzed)
    return std::unexpected(rhs_analyzed.error());
  VariantType rhs_datatype{rhs_analyzed->alt->visit(datatype_visitor)};
  BinaryExpressionAnalyzer<C> operand_visitor{boundary};
  operand_visitor.lhs_analyzed = std::move(*lhs_analyzed);
  operand_visitor.rhs_analyzed = std::move(*rhs_analyzed);
  return std::visit(operand_visitor, *lhs_datatype.alt, *rhs_datatype.alt);
}

std::expected<AnyExpression, LanguageError *>
RvalueAnalyzer::operator()(const MemberExpression &expression) {
  auto object_analyzed = expression.object->alt->visit(*this);
  if (not object_analyzed)
    return std::unexpected(object_analyzed.error());
  DatatypeAnalyzer datatype_visitor{boundary};
  auto datatype_analyzed = object_analyzed->alt->visit(datatype_visitor);
  PropertyFinder property_visitor{boundary, std::move(*object_analyzed)};
  property_visitor.identifier = expression.property;
  return datatype_analyzed.alt->visit(property_visitor);
}

std::expected<AnyExpression, LanguageError *>
MethodAnalyzer::operator()(const IntrinsicAccessor &intrinsic_accessor) {
  if (intrinsic_accessor.object_type != IntrinsicObject::O_CONSOLE)
    return std::unexpected(new InvalidMethodAccess{});
  if (identifier.offset != OFFSET_log)
    return std::unexpected(new InvalidMethodAccess{});
  ConsoleCall console_call{
      &boundary.tape->all_programs[boundary.program_idx]->resource};
  for (const AnyExpression &argument : arguments) {
    std::expected<AnyExpression, LanguageError *> argument_analyzed{
        argument.alt->visit(RvalueAnalyzer{boundary})};
    if (not argument_analyzed)
      return std::unexpected(argument_analyzed.error());
    AnyExpression &expression = console_call.arguments.emplace_back();
    std::swap(expression, *argument_analyzed);
  }
  return AnyExpression{std::move(console_call)};
}

std::expected<AnyExpression, LanguageError *>
CalleeAnalyzer::operator()(const MemberExpression &expression) {
  auto member_object = expression.object->alt->visit(RvalueAnalyzer{boundary});
  if (not member_object)
    return std::unexpected(member_object.error());
  MethodAnalyzer method_visitor{boundary};
  method_visitor.identifier = expression.property;
  method_visitor.arguments = arguments;
  return member_object->alt->visit(method_visitor);
}

struct DirectFunctionCallAnalyzer {
  template <typename T> AnyExpression operator()(T) { std::unreachable(); }
  AnyExpression operator()(NumberType datatype);
  LexicalBoundary &boundary;
  FunctionDefinition *definition;
};

AnyExpression DirectFunctionCallAnalyzer::operator()(NumberType datatype) {
  DirectFunctionCall<NumberType> direct_call{
      &boundary.tape->all_programs[boundary.program_idx]->resource};
  direct_call.callee = definition;
  direct_call.return_type = datatype;
  return AnyExpression{std::move(direct_call)};
}

std::expected<AnyExpression, LanguageError *>
CalleeAnalyzer::operator()(const VariableAccessor &variable_accessor) {
  FunctionDefinition *definition{
      variable_accessor.find_function_linkedly(&boundary)};
  if (not definition) {
    std::breakpoint();
    return std::unexpected(new InvalidVariableAccess{});
  } else if (auto definition_analysis = definition->analyze();
             not definition_analysis) {
    return std::unexpected(definition_analysis.error());
  } else {
    DirectFunctionCallAnalyzer return_visitor{boundary, definition};
    return definition->return_type.alt->visit(return_visitor);
  }
}

FunctionDefinition *
VariableAccessor::find_function_linkedly(LexicalBoundary *boundary) const {
  if (not boundary)
    return nullptr;
  FunctionDefinition *definition{boundary->find_function(identifier)};
  return definition ? definition
                    : find_function_linkedly(boundary->parent_boundary);
}

struct ScopeAccessorAnalyzer {
  template <typename T> AnyExpression operator()(T);
  AnyExpression operator()(LambdaType datatype) { std::unreachable(); }
  AnyExpression operator()(DynamicType datatype) { std::unreachable(); }
  std::size_t scope_offset;
  std::size_t local_offset;
};

template <typename T>
AnyExpression ScopeAccessorAnalyzer::operator()(T datatype) {
  ScopeAccessor<T> scope_accessor{};
  scope_accessor.scope_offset = scope_offset;
  scope_accessor.local_offset = local_offset;
  return AnyExpression{std::move(scope_accessor)};
}

AnyExpression
VariableAccessor::find_local_linkedly(const LexicalBoundary *boundary,
                                      std::size_t scope_offset) const {
  if (not boundary)
    return AnyExpression{};
  std::optional<LocalLayoutEntry> entry_opt{boundary->find_local(identifier)};
  if (entry_opt) {
    ScopeAccessorAnalyzer datatype_visitor{};
    datatype_visitor.scope_offset = scope_offset;
    datatype_visitor.local_offset = entry_opt->offset;
    return entry_opt->variant_type.alt->visit(datatype_visitor);
  } else
    return find_local_linkedly(boundary->parent_boundary, scope_offset + 1);
}

struct InitializeScopeAnalyzer {
  template <typename T>
  std::expected<AnyStatement, LanguageError *> operator()(T datatype);
  std::expected<AnyStatement, LanguageError *>
  operator()(DynamicType datatype) {
    std::unreachable();
  }
  std::expected<AnyStatement, LanguageError *> operator()(LambdaType datatype) {
    std::unreachable();
  }

  InitializeScopeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  AnyExpression rvalue_analyzed;
  Identifier variable_name;
};

struct StatementAnalyzer {
  std::expected<AnyStatement, LanguageError *>
  operator()(const ExpressionStatement &statement);
  std::expected<AnyStatement, LanguageError *>
  operator()(const InitializeVariable &statement);
  std::expected<AnyStatement, LanguageError *>
  operator()(const ReturnStatement<void> &statement);
  std::expected<AnyStatement, LanguageError *>
  operator()(const ReturnStatement<NumberType> &statement) {
    std::unreachable();
  }
  template <typename T>
  std::expected<AnyStatement, LanguageError *>
  operator()(const InitializeScope<T> &statement) {
    std::unreachable();
  }

  StatementAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

std::expected<AnyStatement, LanguageError *>
StatementAnalyzer::operator()(const ExpressionStatement &statement) {
  std::expected argument_analyzed{
      statement.argument->alt->visit(RvalueAnalyzer{boundary})};
  if (not argument_analyzed)
    return std::unexpected(argument_analyzed.error());
  ExpressionStatement expression_statement{
      &boundary.tape->all_programs[boundary.program_idx]->resource};
  expression_statement.argument = std::move(*argument_analyzed);
  return AnyStatement::Alternative(std::move(expression_statement));
}

std::expected<AnyStatement, LanguageError *>
StatementAnalyzer::operator()(const InitializeVariable &statement) {
  std::expected<AnyExpression, LanguageError *> rvalue_analyzed{
      statement.rvalue->alt->visit(RvalueAnalyzer{boundary})};
  if (not rvalue_analyzed)
    return std::unexpected(rvalue_analyzed.error());
  VariantType datatype_analyzed{
      rvalue_analyzed->alt->visit(DatatypeAnalyzer{boundary})};
  InitializeScopeAnalyzer initialize_scope_analyzer{boundary};
  initialize_scope_analyzer.rvalue_analyzed = std::move(*rvalue_analyzed);
  initialize_scope_analyzer.variable_name = statement.variable_name;
  return datatype_analyzed.alt->visit(initialize_scope_analyzer);
}

template <typename T>
std::expected<AnyStatement, LanguageError *>
InitializeScopeAnalyzer::operator()(T datatype) {
  InitializeScope<T> initialize_scope{
      &boundary.tape->all_programs[boundary.program_idx]->resource};
  initialize_scope.rvalue = std::move(rvalue_analyzed);
  initialize_scope.datatype = datatype;
  auto &entries = boundary.local_layout.entries;
  auto entry_it =
      std::ranges::find(entries, variable_name, &LocalLayoutEntry::identifier);
  auto type_is_known = [](auto &e) { return e.variant_type.alt.has_value(); };
  assert(entry_it != entries.end());
  assert(std::all_of(entries.begin(), entry_it, type_is_known));
  std::size_t total_bytes{boundary.local_layout.total_bytes};
  std::size_t alignment{alignof(typename T::Representation)};
  std::size_t offset{(total_bytes + alignment - 1) & ~(alignment - 1)};
  initialize_scope.local_offset = offset;
  entry_it->offset = offset;
  entry_it->variant_type = VariantType{datatype};
  boundary.local_layout.total_bytes =
      offset + sizeof(typename T::Representation);
  boundary.local_layout.largest_alignment =
      std::max(boundary.local_layout.largest_alignment, alignment);
  return AnyStatement{std::move(initialize_scope)};
}

struct ReturnStatementAnalyzer {
  LexicalBoundary &boundary;
  AnyExpression argument;

  AnyStatement operator()(NumberType);
  template <typename T> AnyStatement operator()(T datatype) {
    std::unreachable();
  }
};

AnyStatement ReturnStatementAnalyzer::operator()(NumberType) {
  ReturnStatement<NumberType> return_statement{
      &boundary.tape->all_programs[boundary.program_idx]->resource};
  return_statement.argument = std::move(argument);
  return AnyStatement::Alternative(std::move(return_statement));
}

std::expected<AnyStatement, LanguageError *>
StatementAnalyzer::operator()(const ReturnStatement<void> &statement) {
  auto argument_analyzed =
      statement.argument->alt->visit(RvalueAnalyzer{boundary});
  if (not argument_analyzed)
    return std::unexpected(argument_analyzed.error());
  if (FunctionDefinition *definition = boundary.as_function_definition();
      not definition) {
    std::breakpoint();
    return std::unexpected(new InvalidReturnStatement{});
  } else {
    definition->return_type =
        argument_analyzed->alt->visit(DatatypeAnalyzer{boundary});
    ReturnStatementAnalyzer return_visitor{boundary,
                                           std::move(*argument_analyzed)};
    return definition->return_type.alt->visit(return_visitor);
  }
}

template <typename T>
std::expected<void, LanguageError *> AbstractBoundary<T>::analyze_statements() {
  std::unique_ptr<Program> input_program{std::make_unique<Program>()};
  std::swap(tape->all_programs[program_idx], input_program);
  for (const AnyStatement &any_statement : input_program->statements) {
    std::expected<AnyStatement, LanguageError *> analyzed_statement{
        any_statement.alt->visit(StatementAnalyzer{*this})};
    if (not analyzed_statement.has_value())
      return std::unexpected{analyzed_statement.error()};
    tape->all_programs[program_idx]->statements.push_back(
        std::move(*analyzed_statement));
  }
  std::ranges::sort(local_layout.entries, {}, &LocalLayoutEntry::identifier);
  return std::expected<void, LanguageError *>{};
}

std::expected<void, LanguageError *>
FunctionDefinition::analyze_formal_parameters() {
  for (Identifier argument : arguments) {
    auto layout_entry_it = std::ranges::find(local_layout.entries, argument,
                                             &LocalLayoutEntry::identifier);
    layout_entry_it->variant_type.alt.emplace(std::in_place_type<DynamicType>);
  }
  return std::expected<void, LanguageError *>{};
}

std::expected<void, LanguageError *> FunctionDefinition::do_analyze() {
  if (auto parameters_ok = analyze_formal_parameters(); not parameters_ok)
    return std::unexpected(parameters_ok.error());
  else if (auto statements_ok = analyze_statements(); not statements_ok)
    return std::unexpected(statements_ok.error());
  else if (auto inner_functions_ok = analyze_inner_functions();
           not inner_functions_ok)
    return std::unexpected(inner_functions_ok.error());
  else
    return std::expected<void, LanguageError *>{};
}

std::expected<void, LanguageError *> FunctionDefinition::analyze() {
  switch (analyzer_mark) {
  case AnalyzerMark::PENDING: {
    analyzer_mark = AnalyzerMark::INITIATED;
    auto analyzer_result = do_analyze();
    analyzer_mark = AnalyzerMark::COMPLETE;
    return analyzer_result;
  }
  case AnalyzerMark::INITIATED:
    std::breakpoint();
    return std::unexpected(new UnresolvableCircularity{});
  case AnalyzerMark::COMPLETE:
    return std::expected<void, LanguageError *>{};
  }
  std::unreachable();
}

std::expected<void, LanguageError *> ModuleDefinition::analyze() {
  if (auto void_opt = analyze_statements(); not void_opt)
    return std::unexpected(void_opt.error());
  if (auto void_opt = analyze_inner_functions(); not void_opt)
    return std::unexpected(void_opt.error());
  return std::expected<void, LanguageError *>{};
}

struct StatementEvaluator {
  template <typename T>
  std::expected<void, LanguageError *> operator()(const InitializeScope<T> &);
  template <typename T>
  std::expected<void, LanguageError *> operator()(const T &s) {
    std::unreachable();
  }
  BoundaryFrame &self_frame;
};

template <typename T> struct ExpressionEvaluator {
  template <typename U>
  std::expected<T, LanguageError *> operator()(const U &e) {
    std::unreachable();
  }
  BoundaryFrame &self_frame;
};

template <>
template <>
std::expected<void, LanguageError *>
ExpressionEvaluator<void>::operator()(const ConsoleCall &expression) {
  auto evaluate_argument = [&](const AnyExpression &argument) {
    ExpressionEvaluator<VariantStringView> argument_visitor{self_frame};
    return argument.alt->visit(argument_visitor);
  };
  auto argument_values =
      expression.arguments | std::ranges::views::transform(evaluate_argument);
  std::vector<VariantStringView> message_parts{};
  for (auto message_part : argument_values) {
    if (message_part) {
      message_parts.push_back(*message_part);
      continue;
    }
    return std::unexpected(message_part.error());
  }
  {
    std::lock_guard console_lock{self_frame.machine.console_mutex};
    ConsoleMessage &console_message =
        self_frame.machine.console_messages.emplace_back();
    console_message.parts = std::move(message_parts);
  }
  self_frame.machine.console_condition.notify_one();
  return std::expected<void, LanguageError *>{};
}

template <>
template <>
std::expected<double, LanguageError *> ExpressionEvaluator<double>::operator()(
    const DirectFunctionCall<NumberType> &expression) {
  const LexicalBoundary &boundary = *expression.callee;
  RuntimeMemory &memory = *self_frame.machine.runtime_memory;
  BoundaryFrame &child_frame = memory.boundary_frames.emplace_back(
      self_frame.machine, boundary, &memory.resource);
  child_frame.closure_stack.reserve(self_frame.closure_stack.size() + 1);
  child_frame.closure_stack.push_back(&child_frame);
  child_frame.closure_stack.append_range(self_frame.closure_stack);
  const Program &program = *boundary.tape->all_programs[boundary.program_idx];
  for (const AnyStatement &statement : program.statements) {
    auto statement_result =
        statement.alt->visit(StatementEvaluator{child_frame});
    if (not statement_result)
      return std::unexpected(statement_result.error());
  }
  return *std::launder(reinterpret_cast<double *>(
      static_cast<char *>(child_frame.return_memory.get())));
}

template <>
template <>
std::expected<VariantStringView, LanguageError *>
ExpressionEvaluator<VariantStringView>::operator()(
    const DirectFunctionCall<NumberType> &expression) {
  auto original_value =
      ExpressionEvaluator<double>{self_frame}.operator()(expression);
  if (not original_value)
    return std::unexpected(original_value.error());
  std::string buffer(24, '\0');
  auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                 *original_value);
  assert(ec == std::errc{});
  buffer.resize(ptr - buffer.data());
  std::string_view stringified_value{
      self_frame.machine.runtime_memory->strings.emplace_back(buffer)};
  return stringified_value;
}

template <>
template <>
std::expected<VariantStringView, LanguageError *>
ExpressionEvaluator<VariantStringView>::operator()(
    const AsciiLiteral &expression) {
  return expression.val_view;
}

template <>
template <>
std::expected<double, LanguageError *>
ExpressionEvaluator<double>::operator()(const NumericLiteral &expression) {
  return expression.val;
}

template <>
template <>
std::expected<double, LanguageError *>
ExpressionEvaluator<double>::operator()(const LengthIntrinsic &expression) {
  ExpressionEvaluator<VariantStringView> expression_visitor{self_frame};
  auto argument_value = expression.argument->alt->visit(expression_visitor);
  if (not argument_value)
    return std::unexpected(argument_value.error());
  std::size_t argument_size{
      argument_value->visit([](const auto &sv) { return sv.size(); })};
  return static_cast<double>(argument_size);
}

template <>
template <>
std::expected<double, LanguageError *> ExpressionEvaluator<double>::operator()(
    const BinaryExpression<'+', NumberType> &expression) {
  ExpressionEvaluator<double> operand_visitor{self_frame};
  auto lhs_value = expression.left->alt->visit(operand_visitor);
  if (not lhs_value)
    return std::unexpected(lhs_value.error());
  auto rhs_value = expression.right->alt->visit(operand_visitor);
  if (not rhs_value)
    return std::unexpected(rhs_value.error());
  return *lhs_value + *rhs_value;
}

template <>
template <>
std::expected<double, LanguageError *> ExpressionEvaluator<double>::operator()(
    const BinaryExpression<'-', NumberType> &expression) {
  ExpressionEvaluator<double> operand_visitor{self_frame};
  auto lhs_value = expression.left->alt->visit(operand_visitor);
  if (not lhs_value)
    return std::unexpected(lhs_value.error());
  auto rhs_value = expression.right->alt->visit(operand_visitor);
  if (not rhs_value)
    return std::unexpected(rhs_value.error());
  return *lhs_value - *rhs_value;
}

template <>
template <>
std::expected<VariantStringView, LanguageError *>
ExpressionEvaluator<VariantStringView>::operator()(
    const ScopeAccessor<StringType> &expression) {
  BoundaryFrame &source_frame =
      *self_frame.closure_stack[expression.scope_offset];
  char *source_memory = static_cast<char *>(source_frame.local_memory.get()) +
                        expression.local_offset;
  VariantStringView *source_ptr =
      std::launder(reinterpret_cast<VariantStringView *>(source_memory));
  return *source_ptr;
}

template <>
template <>
std::expected<double, LanguageError *> ExpressionEvaluator<double>::operator()(
    const ScopeAccessor<NumberType> &expression) {
  BoundaryFrame &source_frame =
      *self_frame.closure_stack[expression.scope_offset];
  char *source_memory = static_cast<char *>(source_frame.local_memory.get()) +
                        expression.local_offset;
  double *source_ptr = std::launder(reinterpret_cast<double *>(source_memory));
  return *source_ptr;
}

template <>
std::expected<void, LanguageError *>
StatementEvaluator::operator()(const ExpressionStatement &statement) {
  return statement.argument->alt->visit(ExpressionEvaluator<void>{self_frame});
}

template <>
std::expected<void, LanguageError *>
StatementEvaluator::operator()(const ReturnStatement<NumberType> &statement) {
  auto argument_value =
      statement.argument->alt->visit(ExpressionEvaluator<double>{self_frame});
  if (not argument_value)
    return std::unexpected(argument_value.error());
  char *destination = static_cast<char *>(self_frame.return_memory.get());
  new (destination) double{*argument_value};
  return {};
}

template <typename T>
std::expected<void, LanguageError *>
StatementEvaluator::operator()(const InitializeScope<T> &statement) {
  char *destination = static_cast<char *>(self_frame.local_memory.get()) +
                      statement.local_offset;
  using R = T::Representation;
  ExpressionEvaluator<R> expression_visitor{self_frame};
  auto expression_value = statement.rvalue->alt->visit(expression_visitor);
  if (not expression_value)
    return std::unexpected(expression_value.error());
  new (destination) R{*expression_value};
  return {};
}

std::expected<void, LanguageError *>
Machine::evaluate(const LexicalBoundary &boundary) {
  BoundaryFrame &self_frame = runtime_memory->boundary_frames.emplace_back(
      *this, boundary, &runtime_memory->resource);
  self_frame.closure_stack.push_back(&self_frame);
  const auto &program = *boundary.tape->all_programs[boundary.program_idx];
  for (const AnyStatement &statement : program.statements) {
    auto statement_result =
        statement.alt->visit(StatementEvaluator{self_frame});
    if (not statement_result)
      return std::unexpected(statement_result.error());
  }
  return std::expected<void, LanguageError *>{};
}

std::list<ConsoleMessage>
Machine::collect_console_messages(std::stop_token stopper) {
  std::unique_lock console_lock{console_mutex};
  auto check_messages = [&] { return console_messages.size() > 0; };
  console_condition.wait(console_lock, stopper, check_messages);
  std::list<ConsoleMessage> message_list{};
  console_messages.swap(message_list);
  return message_list;
}

BoundaryFrame::BoundaryFrame(Machine &m, const LexicalBoundary &b,
                             std::pmr::memory_resource *r)
    : machine{m}, boundary{b}, closure_stack{r} {
  std::size_t total_bytes = boundary.local_layout.total_bytes,
              alignment = boundary.local_layout.largest_alignment;
  if (total_bytes == 0)
    return;
  void *local_pointer =
      machine.runtime_memory->resource.allocate(total_bytes, alignment);
  RuntimeBytesDeleter local_deleter{&machine.runtime_memory->resource,
                                    total_bytes, alignment};
  local_memory = {local_pointer, local_deleter};
  const FunctionDefinition *definition = boundary.as_function_definition();
  if (not definition || not definition->return_type.alt)
    return;
  auto size_visitor = []<typename T>(T datatype) {
    return sizeof(typename T::Representation);
  };
  auto return_size = definition->return_type.alt->visit(size_visitor);
  auto align_visitor = []<typename T>(T datatype) {
    return alignof(typename T::Representation);
  };
  auto return_alignment = definition->return_type.alt->visit(align_visitor);
  void *return_pointer =
      machine.runtime_memory->resource.allocate(return_size, return_alignment);
  RuntimeBytesDeleter return_deleter{&machine.runtime_memory->resource,
                                     return_size, return_alignment};
  return_memory = {return_pointer, return_deleter};
}

struct VariantStringEncoder {
  std::string operator()(std::u16string_view unicode_view);
  std::string operator()(std::string_view ascii_view) {
    return std::string{ascii_view};
  }
};

std::string VariantStringEncoder::operator()(std::u16string_view unicode_view) {
  auto encode_u16 =
      [](std::uint16_t uchar) -> std::inplace_vector<std::uint8_t, 3> {
    std::array<std::uint8_t, 3> buffer{};
    std::size_t buffer_len{buffer.size()};
    uint8_t *result = u16_to_u8(&uchar, 1, buffer.data(), &buffer_len);
    assert(result != nullptr);
    return {std::from_range, buffer | std::views::take(buffer_len)};
  };
  auto encoded_message =
      unicode_view | std::views::transform(encode_u16) | std::views::join;
  return std::string{std::from_range, encoded_message};
}

std::string ConsoleMessage::encode_for_print() const {
  auto encode_message_part = [](VariantStringView sv) {
    return sv.visit(VariantStringEncoder{});
  };
  auto encoded_parts = parts | std::views::transform(encode_message_part) |
                       std::views::join_with(' ');
  return std::string{std::from_range, encoded_parts};
}

struct ConsoleWorker {
  void print_messages(std::stop_token stopper);
  void operator()(std::stop_token stopper);
  Machine &machine;
};

void ConsoleWorker::print_messages(std::stop_token stopper) {
  std::list<ConsoleMessage> messages{machine.collect_console_messages(stopper)};
  for (const ConsoleMessage &message : messages)
    std::println("{}", message.encode_for_print());
};

void ConsoleWorker::operator()(std::stop_token stopper) {
  do
    print_messages(stopper);
  while (not stopper.stop_requested());
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

void Language::compile_and_execute() {
  std::set<LambdaSignature> lambda_signatures{};
  std::set<std::string> string_atlas{};
  std::set<std::u16string> u16string_atlas{};
  FunctionTape tape{};

  Tokenizer tokenizer{};
  tokenizer.text_buffer = std::move(text_buffer);
  tokenizer.string_atlas = &string_atlas;
  tokenizer.u16string_atlas = &u16string_atlas;

  ModuleDefinition definition{};
  definition.program_idx = tape.all_programs.size();
  tape.all_programs.push_back(std::make_unique<Program>());
  definition.tape = &tape;

  ModuleParser parser{definition, tokenizer};
  Machine machine{};
  ConsoleWorker console_worker{machine};
  std::jthread console_thread(console_worker);

  auto result = parser.parse_module()
                    .and_then([&] { return definition.analyze(); })
                    .and_then([&] { return machine.evaluate(definition); });
  if (not result)
    error_occurred = std::unique_ptr<LanguageError>{result.error()};
}
} // namespace Manadrain
