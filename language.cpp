#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <debugging>
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
static thread_local VariantError error_descriptor{};

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

using VariantString = std::variant<std::string, std::u16string>;

class Tokenizer {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;

private:
  friend class Language;
  template <typename T> friend class Parser;
  friend class FunctionParser;
  friend class ModuleParser;

  std::flat_map<std::string, Identifier> atom_atlas;
  std::set<VariantString> *string_atlas;

  std::size_t position;
  std::vector<std::optional<char32_t>> backtrace;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  std::optional<Token> tokenize_identifier(char32_t leading);
  std::optional<Token> tokenize_string_literal(char32_t separator);
  std::optional<Token> tokenize_numeric_literal(char32_t leading);

  Token last_token;
  std::optional<std::monostate> assert_punct(char32_t must_be);
  std::optional<std::monostate> tokenize();
};

struct AnyExpression;

class ValueType {
protected:
  ValueType() = default;

public:
  auto operator<=>(const ValueType &) const = default;
};

class NumberType final : public ValueType {
public:
  NumberType() = default;
  auto operator<=>(const NumberType &) const = default;
};

using VariantStringView = std::variant<std::string_view, std::u16string_view>;
class StringType final : public ValueType {
public:
  StringType() = default;
  auto operator<=>(const StringType &) const = default;
};

struct DynamicValue;
class DynamicType final : public ValueType {
public:
  DynamicType() = default;
  auto operator<=>(const DynamicType &) const = default;
};

class LambdaType;

class VariantType {
public:
  using Alternative =
      std::variant<NumberType, StringType, const LambdaType *, DynamicType>;
  Alternative alt;

  VariantType() = delete;
  VariantType(Alternative a) : alt{a} {};

  auto operator<=>(const VariantType &) const = default;
};

struct DynamicValue {};
struct LambdaDescriptor {};

class LambdaType final : public ValueType {
public:
  LambdaType() = default;
  std::vector<VariantType> parameter_types;
  VariantType return_type;
  auto operator<=>(const LambdaType &) const = default;
};

template <typename T> struct TypeVisitor {
  std::optional<T> operator()(NumberType type_alt) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(StringType type_alt) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const LambdaType *type_alt) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(DynamicType type_alt) {
    std::breakpoint();
    return std::nullopt;
  }
};

class ObjectShape {
public:
  std::flat_map<Identifier, std::optional<VariantType>> properties;
};

struct AnyStatement;

class FunctionDefinition;
class ScopeAccessor;

struct LayoutEntry {
  std::optional<VariantType> variant_type;
  std::size_t offset;
};

class LexicalBoundary {
public:
  LexicalBoundary *parent_boundary;
  std::flat_map<Identifier, LayoutEntry> local_layout;

  virtual FunctionDefinition *find_function(Identifier identifier) = 0;
  virtual std::optional<ScopeAccessor>
  find_local(Identifier identifier) const = 0;
  virtual std::optional<std::monostate>
  put_return_type(VariantType variant_type) = 0;
};

struct Heap {
  std::list<FunctionDefinition> function_definition_list;
  std::list<std::list<AnyStatement>> program_list;
};

template <typename T> class AbstractBoundary : public LexicalBoundary {
public:
  std::vector<std::byte> bytecode;

  std::list<AnyStatement> *program;
  std::vector<FunctionDefinition *> inner_functions;
  Heap *heap;

  FunctionDefinition *find_function(Identifier identifier) override;
  std::optional<ScopeAccessor> find_local(Identifier identifier) const override;
  std::optional<std::monostate> serialize();

protected:
  AbstractBoundary() = default;
  std::optional<std::monostate> analyze_statements();
  std::optional<std::monostate> analyze_inner_functions();
};

class FunctionDefinition final : public AbstractBoundary<FunctionDefinition> {
public:
  std::optional<Identifier> function_name;

  std::optional<VariantType> return_type;
  std::vector<Identifier> arguments;

  enum class AnalyzerMark { PENDING, INITIATED, COMPLETE };
  AnalyzerMark analyzer_mark;
  std::optional<std::monostate> analyze();

  std::optional<std::monostate>
  put_return_type(VariantType variant_type) override {
    return_type.emplace(variant_type);
    return std::monostate{};
  }

private:
  std::optional<std::monostate> analyze_formal_parameters();
};
class ModuleDefinition final : public AbstractBoundary<ModuleDefinition> {
public:
  std::optional<std::monostate> analyze();

  std::optional<std::monostate>
  put_return_type(VariantType variant_type) override {
    std::breakpoint();
    error_descriptor.emplace<InvalidReturnStatement>();
    return std::nullopt;
  }
};

template <typename T> class Parser {
public:
  std::optional<std::monostate> parse_function_stmt();
  std::optional<std::monostate> parse_variable_decl();
  std::optional<std::monostate> parse_statement();

  std::optional<AnyExpression> parse_expression();
  std::optional<AnyExpression> parse_primary_expression();

  std::optional<AnyExpression> parse_assign_expression();
  std::optional<AnyExpression> parse_logical_disjunct();
  std::optional<AnyExpression> parse_additive_expression();
  std::optional<AnyExpression> parse_object_literal();

  std::optional<std::monostate>
  parse_member_expression(AnyExpression &expression);
  std::optional<std::monostate> parse_function_call(AnyExpression &expression);
  std::optional<AnyExpression> parse_postfix_expression();

protected:
  Parser(T &b, Tokenizer &t) : boundary{b}, tokenizer{t} {}
  T &boundary;
  Tokenizer &tokenizer;

private:
  friend class Language;
  std::set<VariantString> *string_atlas;
};

class FunctionParser final : public Parser<FunctionDefinition> {
public:
  FunctionParser(FunctionDefinition &b, Tokenizer &t) : Parser{b, t} {}
  std::optional<std::monostate> parse_function_decl();
};
class ModuleParser final : public Parser<ModuleDefinition> {
public:
  ModuleParser(ModuleDefinition &b, Tokenizer &t) : Parser{b, t} {}
  std::optional<std::monostate> parse_module();
};

class Expression {
protected:
  Expression() = default;
};

class AliasedFunctionCall final : public Expression {
public:
  AliasedFunctionCall(AnyExpression &&c);
  std::indirect<AnyExpression> callee;
  std::vector<AnyExpression> arguments;
};

class DirectFunctionCall final : public Expression {
public:
  DirectFunctionCall() = default;
  const FunctionDefinition *callee;
  std::vector<Identifier> passed_identifiers;
  std::vector<AnyExpression> passed_values;
};

class MemberExpression final : public Expression {
public:
  MemberExpression(AnyExpression &&obj);
  std::indirect<AnyExpression> object;
  Identifier property;
};

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
  BinaryExpression(AnyExpression &&l, AnyExpression &&r);
  std::indirect<AnyExpression> left;
  std::indirect<AnyExpression> right;
  char32_t op;
};

class ObjectExpression final : public Expression {
public:
  ObjectExpression() = default;
  ObjectShape object_shape;
  std::vector<Identifier> keys;
  std::vector<std::optional<AnyExpression>> values;
};

class VariableAccessor final : public Expression {
public:
  VariableAccessor() = default;
  Identifier identifier;
  std::optional<ScopeAccessor>
  find_local_linkedly(const LexicalBoundary *boundary,
                      std::size_t scope_offset = 0) const;
  FunctionDefinition *find_function_linkedly(LexicalBoundary *boundary) const;
};

class ScopeAccessor final : public Expression {
public:
  ScopeAccessor(VariantType t) : local_type{t} {}
  VariantType local_type;
  std::array<std::size_t, 2> location;
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
  ConsoleCall() = default;
  std::vector<AnyExpression> arguments;
};

class LengthIntrinsic final : public Expression {
public:
  LengthIntrinsic(AnyExpression &&arg);
  std::indirect<AnyExpression> argument;
};

class StringifyIntrinsic final : public Expression {
public:
  StringifyIntrinsic(AnyExpression &&arg);
  std::indirect<AnyExpression> argument;
};

struct AnyExpression {
  std::variant<NumericLiteral, AsciiLiteral, UnicodeLiteral,
               AliasedFunctionCall, DirectFunctionCall, MemberExpression,
               AssignExpression, LogicalExpression, BinaryExpression,
               ObjectExpression, VariableAccessor, ScopeAccessor,
               IntrinsicAccessor, LambdaExpression, ConsoleCall,
               LengthIntrinsic, StringifyIntrinsic>
      alt;
};

AliasedFunctionCall::AliasedFunctionCall(AnyExpression &&c)
    : callee{std::move(c)} {};

MemberExpression::MemberExpression(AnyExpression &&obj)
    : object{std::move(obj)} {};

AssignExpression::AssignExpression(AnyExpression &&l, AnyExpression &&r)
    : left{std::move(l)}, right{std::move(r)} {};

LogicalExpression::LogicalExpression(AnyExpression &&l, AnyExpression &&r)
    : left{std::move(l)}, right{std::move(r)} {};

BinaryExpression::BinaryExpression(AnyExpression &&l, AnyExpression &&r)
    : left{std::move(l)}, right{std::move(r)} {};

LengthIntrinsic::LengthIntrinsic(AnyExpression &&arg)
    : argument{std::move(arg)} {};

StringifyIntrinsic::StringifyIntrinsic(AnyExpression &&arg)
    : argument{std::move(arg)} {};

template <typename T> struct ExpressionVisitor {
  std::optional<T> operator()(const NumericLiteral &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const AsciiLiteral &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const UnicodeLiteral &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const AliasedFunctionCall &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const DirectFunctionCall &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const MemberExpression &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const AssignExpression &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const LogicalExpression &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const BinaryExpression &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const ObjectExpression &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const VariableAccessor &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const ScopeAccessor &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const IntrinsicAccessor &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const LambdaExpression &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const ConsoleCall &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const LengthIntrinsic &expression) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const StringifyIntrinsic &expression) {
    std::breakpoint();
    return std::nullopt;
  }
};

class Statement {
protected:
  Statement() = default;
};

class InitializeVariable final : public Statement {
public:
  InitializeVariable(AnyExpression v) : rvalue{std::move(v)} {};
  Identifier variable_name;
  std::indirect<AnyExpression> rvalue;
};

class InitializeScope final : public Statement {
public:
  InitializeScope(AnyExpression v) : rvalue{std::move(v)} {};
  std::size_t local_offset;
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
  std::variant<InitializeVariable, InitializeScope, ReturnStatement,
               ExpressionStatement>
      alt;
};

template <typename T> struct StatementVisitor {
  std::optional<T> operator()(const InitializeVariable &statement) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const InitializeScope &statement) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const ReturnStatement &statement) {
    std::breakpoint();
    return std::nullopt;
  }
  std::optional<T> operator()(const ExpressionStatement &statement) {
    std::breakpoint();
    return std::nullopt;
  }
};

struct ConsoleMessage {
  std::vector<VariantString> parts;
  std::string encode_for_print() const;
};

struct RuntimeFrame {
  const LexicalBoundary *boundary;
  std::vector<std::byte>::const_iterator bytecode_it;
  std::span<std::byte> local_memory;
  std::span<RuntimeFrame *> closure;
  std::span<std::byte> return_val;
};

struct RuntimeMemory {
  std::pmr::monotonic_buffer_resource resource{65536};
  std::pmr::list<RuntimeFrame> runtime_frames{&resource};
  std::pmr::list<std::pmr::vector<RuntimeFrame *>> scope_traces{&resource};
  std::pmr::list<std::pmr::vector<std::byte>> structures{&resource};
  std::pmr::list<VariantString> strings{&resource};
};

class Machine {
public:
  Machine() : runtime_memory{std::make_unique<RuntimeMemory>()} {}
  std::list<ConsoleMessage> collect_console_messages(std::stop_token stopper);

private:
  RuntimeFrame *top_frame;
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

std::string normalize(std::u16string_view unicode_view) {
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

std::optional<Token> Tokenizer::tokenize_identifier(char32_t leading) {
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

std::optional<Token> Tokenizer::tokenize_string_literal(char32_t separator) {
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
      error_descriptor.emplace<UnexpectedStringEnd>();
      return std::nullopt;
    }
    u16_literal.append_range(traverse_u16(*leading));
  }
  if (std::ranges::all_of(u16_literal, [](char16_t ch) { return ch < 128; })) {
    auto cast_to_ascii = [](char16_t ch) { return static_cast<char>(ch); };
    auto ascii_string = u16_literal | std::views::transform(cast_to_ascii);
    auto [string_iter, _] =
        string_atlas->emplace(std::string{std::from_range, ascii_string});
    return std::get<std::string>(*string_iter);
  }
  auto [u16string_iter, _] = string_atlas->emplace(std::move(u16_literal));
  return std::get<std::u16string>(*u16string_iter);
}

std::optional<Token> Tokenizer::tokenize_numeric_literal(char32_t leading) {
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

std::optional<std::monostate> Tokenizer::tokenize() {
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
      return std::monostate{};
    } else
      backward();
    if (std::ranges::binary_search(std::to_array({'"', '\'', '`'}), leading)) {
      std::optional token_opt{tokenize_string_literal(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return std::monostate{};
      } else
        return std::nullopt;
    }
    if (uc_is_property_xid_start(leading) || leading == '_') {
      std::optional token_opt{tokenize_identifier(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return std::monostate{};
      } else
        return std::nullopt;
    }
    static const std::array legal_punct{std::to_array<char32_t>(
        {'(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '=', '{', '}'})};
    if (std::ranges::binary_search(legal_punct, leading)) {
      last_token.emplace<char32_t>(leading);
      return std::monostate{};
    }
    if (std::isdigit(leading)) {
      std::optional token_opt{tokenize_numeric_literal(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return std::monostate{};
      } else
        return std::nullopt;
    }
    std::breakpoint();
    error_descriptor.emplace<UnexpectedToken>();
    return std::nullopt;
  }
  last_token.emplace<std::monostate>();
  return std::monostate{};
}

std::optional<std::monostate> Tokenizer::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return std::monostate{};
  else {
    std::breakpoint();
    error_descriptor.emplace<MissingPunctuation>(must_be);
    return std::nullopt;
  }
}

std::optional<std::monostate> FunctionParser::parse_function_decl() {
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (not std::holds_alternative<Identifier>(tokenizer.last_token))
    goto after_name;
  boundary.function_name = std::get<Identifier>(tokenizer.last_token);
  if (not tokenizer.tokenize())
    return std::nullopt;
after_name:
  if (not tokenizer.assert_punct('('))
    return std::nullopt;
formal_parameter: {
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (tokenizer.last_token == Token{U')'})
    goto after_parameters;
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    error_descriptor.emplace<MissingFormalParameter>();
    return std::nullopt;
  }
  Identifier parameter{std::get<Identifier>(tokenizer.last_token)};
  boundary.local_layout.try_emplace(parameter);
  boundary.arguments.push_back(parameter);
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (tokenizer.last_token == Token{U')'})
    goto after_parameters;
  if (not tokenizer.assert_punct(','))
    return std::nullopt;
  goto formal_parameter;
}
after_parameters:
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (not tokenizer.assert_punct('{'))
    return std::nullopt;
body_statement: {
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (std::holds_alternative<std::monostate>(tokenizer.last_token)) {
    std::breakpoint();
    error_descriptor.emplace<MissingPunctuation>('}');
    return std::nullopt;
  }
  char32_t *alter_ptr{std::get_if<char32_t>(&tokenizer.last_token)};
  if (alter_ptr && *alter_ptr == '}')
    return std::monostate{};
  if (not parse_statement())
    return std::nullopt;
  goto body_statement;
}
}

std::optional<std::monostate> ModuleParser::parse_module() {
repeat:
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (std::holds_alternative<std::monostate>(tokenizer.last_token))
    return std::monostate{};
  if (not parse_statement())
    return std::nullopt;
  goto repeat;
}

template <typename T>
std::optional<std::monostate> Parser<T>::parse_statement() {
  Keyword keyword{};
  if (std::holds_alternative<Keyword>(tokenizer.last_token))
    keyword = std::get<Keyword>(tokenizer.last_token);
  switch (keyword) {
  case Keyword::K_FUNCTION:
    return parse_function_stmt();
  case Keyword::K_LET:
    return parse_variable_decl();
  case Keyword::K_RETURN:
    if (not tokenizer.tokenize())
      return std::nullopt;
    if (auto stmt_opt = parse_expression(); not stmt_opt)
      return std::nullopt;
    else {
      ReturnStatement statement{*stmt_opt};
      boundary.program->emplace_back(std::move(statement));
      return tokenizer.assert_punct(';');
    }
  default:
    if (auto stmt_opt = parse_expression(); not stmt_opt)
      return std::nullopt;
    else {
      ExpressionStatement statement{*stmt_opt};
      boundary.program->emplace_back(std::move(statement));
      return tokenizer.assert_punct(';');
    }
  }
}

template <typename T>
std::optional<std::monostate> Parser<T>::parse_function_stmt() {
  FunctionDefinition *definition{
      &boundary.heap->function_definition_list.emplace_back()};
  definition->parent_boundary = &boundary;
  definition->program = &boundary.heap->program_list.emplace_back();
  definition->heap = boundary.heap;
  FunctionParser function_parser{*definition, tokenizer};
  if (not function_parser.parse_function_decl())
    return std::nullopt;
  if (not definition->function_name) {
    std::breakpoint();
    error_descriptor.emplace<MissingFunctionName>();
    return std::nullopt;
  }
  bool duplicate_exists =
      std::ranges::contains(boundary.inner_functions, definition->function_name,
                            [](const auto &el) { return el->function_name; });
  if (duplicate_exists) {
    std::breakpoint();
    error_descriptor.emplace<DuplicateDeclaration>();
    return std::nullopt;
  } else {
    boundary.inner_functions.push_back(definition);
    return std::monostate{};
  }
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_assign_expression() {
  std::optional left_expression{parse_logical_disjunct()};
  if (not left_expression)
    return std::nullopt;
  if (tokenizer.last_token != Token{U'='})
    return left_expression;
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (auto expr_opt = parse_assign_expression(); not expr_opt)
    return std::nullopt;
  else {
    AssignExpression expression{std::move(*left_expression),
                                std::move(*expr_opt)};
    return AnyExpression{std::move(expression)};
  }
}

template <typename T>
std::optional<std::monostate> Parser<T>::parse_variable_decl() {
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    error_descriptor.emplace<MissingVariableName>();
    return std::nullopt;
  }
  Identifier variable_name{std::get<Identifier>(tokenizer.last_token)};
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (not tokenizer.assert_punct('='))
    return std::nullopt;
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (auto expr_opt = parse_expression(); not expr_opt)
    return std::nullopt;
  else {
    InitializeVariable initialize_variable{std::move(*expr_opt)};
    initialize_variable.variable_name = variable_name;
    boundary.program->emplace_back(std::move(initialize_variable));
  }
  if (not tokenizer.assert_punct(';'))
    return std::nullopt;
  if (boundary.local_layout.contains(variable_name)) {
    std::breakpoint();
    error_descriptor.emplace<DuplicateDeclaration>();
    return std::nullopt;
  } else {
    boundary.local_layout.try_emplace(variable_name);
    return std::monostate{};
  }
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_expression() {
  return parse_assign_expression();
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_logical_disjunct() {
  std::optional left_expression{parse_additive_expression()};
  if (not left_expression)
    return std::nullopt;
right_expression: {
  if (tokenizer.last_token != Token{Operator::LOGICAL_DISJUNCT})
    return left_expression;
  Operator logical_op{std::get<Operator>(tokenizer.last_token)};
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (auto expr_opt = parse_additive_expression(); not expr_opt)
    return std::nullopt;
  else {
    LogicalExpression expression{std::move(*left_expression),
                                 std::move(*expr_opt)};
    expression.op = logical_op;
    left_expression->alt.template emplace<LogicalExpression>(
        std::move(expression));
  }
  goto right_expression;
}
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_postfix_expression() {
  std::optional postfix_expression{parse_primary_expression()};
postfix_iteration:
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (not std::holds_alternative<char32_t>(tokenizer.last_token))
    return postfix_expression;
  switch (std::get<char32_t>(tokenizer.last_token)) {
  case '.':
    if (not parse_member_expression(*postfix_expression))
      return std::nullopt;
    else
      goto postfix_iteration;
  case '(':
    if (not parse_function_call(*postfix_expression))
      return std::nullopt;
    else
      goto postfix_iteration;
  default:
    return postfix_expression;
  }
}

template <typename T>
std::optional<std::monostate>
Parser<T>::parse_function_call(AnyExpression &expression) {
  AnyExpression callee{std::move(expression)};
  AliasedFunctionCall &function_call{
      expression.alt.emplace<AliasedFunctionCall>(std::move(callee))};
  while (1) {
    if (not tokenizer.tokenize())
      return std::nullopt;
    if (tokenizer.last_token == Token{U')'})
      return std::monostate{};
    if (auto expr_opt = parse_expression(); not expr_opt)
      return std::nullopt;
    else
      function_call.arguments.push_back(std::move(*expr_opt));
    if (tokenizer.last_token == Token{U')'})
      return std::monostate{};
    if (not tokenizer.assert_punct(','))
      return std::nullopt;
  }
}

template <typename T>
std::optional<std::monostate>
Parser<T>::parse_member_expression(AnyExpression &expression) {
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    error_descriptor.emplace<MissingPropertyName>();
    return std::nullopt;
  } else {
    Identifier field_name{std::get<Identifier>(tokenizer.last_token)};
    AnyExpression member_object{std::move(expression)};
    MemberExpression &member_expression{
        expression.alt.emplace<MemberExpression>(std::move(member_object))};
    member_expression.property = field_name;
    return std::monostate{};
  }
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_additive_expression() {
  std::optional expression{parse_postfix_expression()};
  if (not expression)
    return std::nullopt;
  if (tokenizer.last_token != Token{U'+'} &&
      tokenizer.last_token != Token{U'-'})
    return expression;
  char32_t binary_op{std::get<char32_t>(tokenizer.last_token)};
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (auto expr_opt = parse_postfix_expression(); not expr_opt)
    return std::nullopt;
  else {
    BinaryExpression binary_expression{std::move(*expression),
                                       std::move(*expr_opt)};
    binary_expression.op = binary_op;
    return AnyExpression{std::move(binary_expression)};
  }
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_object_literal() {
  ObjectExpression object_expression{};
  if (not tokenizer.tokenize())
    return std::nullopt;
object_property: {
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    error_descriptor.emplace<InvalidPropertyName>();
    return std::nullopt;
  }
  Identifier property_name{std::get<Identifier>(tokenizer.last_token)};
  object_expression.object_shape.properties.try_emplace(property_name);
  if (not tokenizer.tokenize())
    return std::nullopt;
  object_expression.keys.push_back(property_name);
  std::optional<AnyExpression> initializer{};
  if (tokenizer.last_token != Token{U':'})
    goto after_initializer;
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (auto expr_opt = parse_expression(); not expr_opt)
    return std::nullopt;
  else
    initializer.emplace(std::move(*expr_opt));
after_initializer:
  object_expression.values.push_back(std::move(initializer));
  if (tokenizer.last_token != Token{U','})
    goto after_properties;
  if (not tokenizer.tokenize())
    return std::nullopt;
  if (tokenizer.last_token != Token{U'}'})
    goto object_property;
}
after_properties:
  tokenizer.assert_punct('}');
  return AnyExpression{std::move(object_expression)};
}

template <typename T>
std::optional<AnyExpression> Parser<T>::parse_primary_expression() {
  struct TokenVisitor {
    Parser<T> &parser;
    std::optional<AnyExpression> operator()(std::monostate) {
      std::breakpoint();
      error_descriptor.emplace<UnexpectedToken>();
      return std::nullopt;
    }
    std::optional<AnyExpression> operator()(char32_t punct) {
      if (punct == '{')
        return parser.parse_object_literal();
      else {
        std::breakpoint();
        error_descriptor.emplace<UnexpectedToken>();
        return std::nullopt;
      }
    }
    std::optional<AnyExpression> operator()(std::int64_t number) {
      NumericLiteral num_literal{};
      num_literal.val = static_cast<double>(number);
      return AnyExpression{num_literal};
    }
    std::optional<AnyExpression> operator()(double number) {
      NumericLiteral numeric_literal{};
      numeric_literal.val = number;
      return AnyExpression{numeric_literal};
    }
    std::optional<AnyExpression> operator()(Operator op) {
      std::breakpoint();
      error_descriptor.emplace<UnexpectedToken>();
      return std::nullopt;
    }
    std::optional<AnyExpression> operator()(Keyword keyword) {
      switch (keyword) {
      case Keyword::K_FUNCTION: {
        LambdaExpression lambda_expression{};
        lambda_expression.definition =
            &parser.boundary.heap->function_definition_list.emplace_back();
        FunctionParser function_parser{*lambda_expression.definition,
                                       parser.tokenizer};
        function_parser.parse_function_decl();
        return AnyExpression{lambda_expression};
      }
      default:
        std::breakpoint();
        error_descriptor.emplace<UnexpectedToken>();
        return std::nullopt;
      }
    }
    std::optional<AnyExpression> operator()(Identifier identifier) {
      VariableAccessor variable_accessor{};
      variable_accessor.identifier = identifier;
      return AnyExpression{variable_accessor};
    }
    std::optional<AnyExpression> operator()(std::string_view ascii_view) {
      AsciiLiteral ascii_literal{};
      ascii_literal.val_view = ascii_view;
      return AnyExpression{ascii_literal};
    }
    std::optional<AnyExpression> operator()(std::u16string_view unicode_view) {
      UnicodeLiteral unicode_literal{};
      unicode_literal.val_view = unicode_view;
      return AnyExpression{unicode_literal};
    }
  };
  return tokenizer.last_token.visit(TokenVisitor{*this});
}

template <typename T>
std::optional<ScopeAccessor>
AbstractBoundary<T>::find_local(Identifier identifier) const {
  if (not local_layout.contains(identifier))
    return std::nullopt;
  std::size_t local_offset =
      std::distance(local_layout.begin(), local_layout.find(identifier));
  assert(local_layout.values()[local_offset].variant_type.has_value());
  ScopeAccessor accessor{*local_layout.values()[local_offset].variant_type};
  std::get<1>(accessor.location) = local_offset;
  return accessor;
}

template <typename T>
FunctionDefinition *AbstractBoundary<T>::find_function(Identifier identifier) {
  auto by_name = [](auto definition) { return definition->function_name; };
  auto function_it = std::ranges::find(inner_functions, identifier, by_name);
  return function_it == inner_functions.end() ? nullptr : *function_it;
}

template <typename T>
std::optional<std::monostate> AbstractBoundary<T>::analyze_inner_functions() {
  for (FunctionDefinition *definition : inner_functions) {
    definition->parent_boundary = this;
    if (not definition->analyze())
      return std::nullopt;
  }
  return std::monostate{};
}

struct PropertyFinder final : TypeVisitor<AnyExpression> {
  using TypeVisitor<AnyExpression>::operator();
  std::optional<AnyExpression> operator()(StringType);
  AnyExpression source_expression;
  Identifier identifier;
  PropertyFinder(AnyExpression e) : source_expression{std::move(e)} {}
};

struct RvalueAnalyzer;

struct CalleeAnalyzer final : ExpressionVisitor<AnyExpression> {
  using ExpressionVisitor<AnyExpression>::operator();

  std::optional<AnyExpression> operator()(const MemberExpression &expression);
  std::optional<AnyExpression> operator()(const VariableAccessor &expression);

  CalleeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  std::span<const AnyExpression> arguments;
};

struct RvalueAnalyzer final : ExpressionVisitor<AnyExpression> {
  using ExpressionVisitor<AnyExpression>::operator();

  std::optional<AnyExpression> operator()(const AsciiLiteral &ascii_literal) {
    return AnyExpression{ascii_literal};
  }
  std::optional<AnyExpression>
  operator()(const NumericLiteral &numeric_literal) {
    return AnyExpression{numeric_literal};
  }
  std::optional<AnyExpression>
  operator()(const AliasedFunctionCall &expression);
  std::optional<AnyExpression> operator()(const VariableAccessor &expression);
  std::optional<AnyExpression> operator()(const BinaryExpression &expression);
  std::optional<AnyExpression> operator()(const MemberExpression &expression);

  RvalueAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

struct MethodAnalyzer final : ExpressionVisitor<AnyExpression> {
  using ExpressionVisitor<AnyExpression>::operator();

  std::optional<AnyExpression>
  operator()(const IntrinsicAccessor &intrinsic_accessor);

  MethodAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  Identifier identifier;
  std::span<const AnyExpression> arguments;
};

struct DatatypeAnalyzer final : ExpressionVisitor<VariantType> {
  using ExpressionVisitor<VariantType>::operator();

  std::optional<VariantType> operator()(const AsciiLiteral &) {
    return VariantType{StringType{}};
  }
  std::optional<VariantType> operator()(const ScopeAccessor &accessor) {
    return accessor.local_type;
  }
  std::optional<VariantType> operator()(const BinaryExpression &) {
    return VariantType{NumberType{}};
  }
  std::optional<VariantType> operator()(const DirectFunctionCall &direct_call) {
    assert(direct_call.callee != nullptr);
    assert(direct_call.callee->return_type.has_value());
    return direct_call.callee->return_type;
  }

  DatatypeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

struct ExpressionStringifier final : TypeVisitor<AnyExpression> {
  using TypeVisitor<AnyExpression>::operator();
  std::optional<AnyExpression> operator()(NumberType);
  AnyExpression source_expression;
  ExpressionStringifier(AnyExpression e) : source_expression{std::move(e)} {}
};

std::optional<AnyExpression> PropertyFinder::operator()(StringType) {
  switch (identifier.offset) {
  case OFFSET_length:
    return AnyExpression{LengthIntrinsic{std::move(source_expression)}};
  default:
    return std::nullopt;
  }
}

std::optional<AnyExpression> ExpressionStringifier::operator()(NumberType) {
  return AnyExpression{StringifyIntrinsic{std::move(source_expression)}};
}

std::optional<AnyExpression>
RvalueAnalyzer::operator()(const AliasedFunctionCall &function_call) {
  CalleeAnalyzer callee_visitor{boundary};
  callee_visitor.arguments = function_call.arguments;
  return function_call.callee->alt.visit(callee_visitor);
}

std::optional<AnyExpression>
RvalueAnalyzer::operator()(const VariableAccessor &variable_accessor) {
  std::optional<ScopeAccessor> scope_accessor{
      variable_accessor.find_local_linkedly(&boundary)};
  if (scope_accessor)
    return AnyExpression{*scope_accessor};
  switch (variable_accessor.identifier.offset) {
  case OFFSET_console: {
    IntrinsicAccessor intrinsic_accessor{};
    intrinsic_accessor.object_type = IntrinsicObject::O_CONSOLE;
    return AnyExpression{intrinsic_accessor};
  }
  default:
    std::breakpoint();
    error_descriptor.emplace<InvalidVariableAccess>();
    return std::nullopt;
  }
}

std::optional<AnyExpression>
RvalueAnalyzer::operator()(const BinaryExpression &expression) {
  std::optional left_analyzed{expression.left->alt.visit(*this)};
  if (not left_analyzed)
    return std::nullopt;
  std::optional right_analyzed{expression.right->alt.visit(*this)};
  if (not right_analyzed)
    return std::nullopt;
  BinaryExpression output_expression{std::move(*left_analyzed),
                                     std::move(*right_analyzed)};
  output_expression.op = expression.op;
  return AnyExpression{std::move(output_expression)};
}

std::optional<AnyExpression>
RvalueAnalyzer::operator()(const MemberExpression &expression) {
  std::optional object_analyzed{expression.object->alt.visit(*this)};
  if (not object_analyzed)
    return std::nullopt;
  DatatypeAnalyzer datatype_visitor{boundary};
  std::optional datatype_analyzed{object_analyzed->alt.visit(datatype_visitor)};
  assert(datatype_analyzed.has_value());
  PropertyFinder property_visitor{std::move(*object_analyzed)};
  property_visitor.identifier = expression.property;
  std::optional property_expression{
      datatype_analyzed->alt.visit(property_visitor)};
  if (not property_expression) {
    std::breakpoint();
    error_descriptor.emplace<InvalidPropertyAccess>();
    return std::nullopt;
  }
  return property_expression;
}

std::optional<AnyExpression>
MethodAnalyzer::operator()(const IntrinsicAccessor &intrinsic_accessor) {
  if (intrinsic_accessor.object_type != IntrinsicObject::O_CONSOLE)
    return std::nullopt;
  if (identifier.offset != OFFSET_log)
    return std::nullopt;
  ConsoleCall console_call{};
  for (const AnyExpression &argument : arguments) {
    RvalueAnalyzer rvalue_visitor{boundary};
    std::optional argument_analyzed{argument.alt.visit(rvalue_visitor)};
    if (not argument_analyzed)
      return std::nullopt;
    DatatypeAnalyzer datatype_visitor{boundary};
    std::optional argument_type{argument_analyzed->alt.visit(datatype_visitor)};
    if (not argument_type)
      return std::nullopt;
    ExpressionStringifier stringifier{std::move(*argument_analyzed)};
    std::optional stringified_argument{argument_type->alt.visit(stringifier)};
    if (not stringified_argument)
      return std::nullopt;
    console_call.arguments.push_back(std::move(*stringified_argument));
  }
  return AnyExpression{std::move(console_call)};
}

std::optional<AnyExpression>
CalleeAnalyzer::operator()(const MemberExpression &expression) {
  std::optional<AnyExpression> member_object{
      expression.object->alt.visit(RvalueAnalyzer{boundary})};
  if (not member_object)
    return std::nullopt;
  MethodAnalyzer method_visitor{boundary};
  method_visitor.identifier = expression.property;
  method_visitor.arguments = arguments;
  if (auto method_opt = member_object->alt.visit(method_visitor);
      not method_opt) {
    std::breakpoint();
    error_descriptor.emplace<InvalidMethodAccess>();
    return std::nullopt;
  } else
    return *method_opt;
}

std::optional<AnyExpression>
CalleeAnalyzer::operator()(const VariableAccessor &variable_accessor) {
  FunctionDefinition *definition{
      variable_accessor.find_function_linkedly(&boundary)};
  if (not definition) {
    std::breakpoint();
    error_descriptor.emplace<InvalidVariableAccess>();
    return std::nullopt;
  }
  if (not definition->analyze())
    return std::nullopt;
  DirectFunctionCall direct_call{};
  direct_call.callee = definition;
  return AnyExpression{std::move(direct_call)};
}

FunctionDefinition *
VariableAccessor::find_function_linkedly(LexicalBoundary *boundary) const {
  if (not boundary)
    return nullptr;
  FunctionDefinition *definition{boundary->find_function(identifier)};
  return definition ? definition
                    : find_function_linkedly(boundary->parent_boundary);
}

std::optional<ScopeAccessor>
VariableAccessor::find_local_linkedly(const LexicalBoundary *boundary,
                                      std::size_t scope_offset) const {
  if (not boundary)
    return std::nullopt;
  std::optional<ScopeAccessor> scope_accessor{boundary->find_local(identifier)};
  if (scope_accessor) {
    std::get<0>(scope_accessor->location) = scope_offset;
    return scope_accessor;
  }
  return find_local_linkedly(boundary->parent_boundary, scope_offset + 1);
}

struct StatementAnalyzer final : StatementVisitor<AnyStatement> {
  using StatementVisitor<AnyStatement>::operator();

  std::optional<AnyStatement> operator()(const ExpressionStatement &statement);
  std::optional<AnyStatement> operator()(const InitializeVariable &statement);
  std::optional<AnyStatement> operator()(const ReturnStatement &statement);
  std::optional<AnyStatement> operator()(const InitializeScope &statement) {
    std::unreachable();
  }

  StatementAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

std::optional<AnyStatement>
StatementAnalyzer::operator()(const ExpressionStatement &statement) {
  std::optional argument_analyzed{
      statement.argument->alt.visit(RvalueAnalyzer{boundary})};
  if (not argument_analyzed)
    return std::nullopt;
  return AnyStatement{ExpressionStatement{std::move(*argument_analyzed)}};
}

std::optional<AnyStatement>
StatementAnalyzer::operator()(const InitializeVariable &statement) {
  std::optional<AnyExpression> rvalue_analyzed{
      statement.rvalue->alt.visit(RvalueAnalyzer{boundary})};
  if (not rvalue_analyzed)
    return std::nullopt;
  InitializeScope initialize_scope{std::move(*rvalue_analyzed)};
  std::optional<VariantType> datatype_analyzed{
      initialize_scope.rvalue->alt.visit(DatatypeAnalyzer{boundary})};
  assert(datatype_analyzed.has_value());
  boundary.local_layout[statement.variable_name].variant_type.emplace(
      *datatype_analyzed);
  auto variable_it = boundary.local_layout.find(statement.variable_name);
  initialize_scope.local_offset =
      std::distance(boundary.local_layout.begin(), variable_it);
  return AnyStatement{std::move(initialize_scope)};
}

std::optional<AnyStatement>
StatementAnalyzer::operator()(const ReturnStatement &statement) {
  RvalueAnalyzer rvalue_visitor{boundary};
  std::optional argument_analyzed{
      statement.argument->alt.visit(rvalue_visitor)};
  if (not argument_analyzed)
    return std::nullopt;
  DatatypeAnalyzer datatype_visitor{boundary};
  std::optional argument_type{argument_analyzed->alt.visit(datatype_visitor)};
  assert(argument_type.has_value());
  if (not boundary.put_return_type(*argument_type))
    return std::nullopt;
  return AnyStatement{ReturnStatement{std::move(*argument_analyzed)}};
}

template <typename T>
std::optional<std::monostate> AbstractBoundary<T>::analyze_statements() {
  for (AnyStatement &any_statement : *program) {
    StatementAnalyzer visitor{*this};
    std::optional output_statement{any_statement.alt.visit(visitor)};
    if (not output_statement)
      return std::nullopt;
    any_statement = std::move(*output_statement);
  }
  return std::monostate{};
}

std::optional<std::monostate> FunctionDefinition::analyze_formal_parameters() {
  for (Identifier argument : arguments)
    local_layout.find(argument)->second.variant_type.emplace(DynamicType{});
  return std::monostate{};
}

std::optional<std::monostate> FunctionDefinition::analyze() {
  switch (analyzer_mark) {
  case AnalyzerMark::PENDING:
    analyzer_mark = AnalyzerMark::INITIATED;
    if (not analyze_formal_parameters())
      return std::nullopt;
    else if (not analyze_statements())
      return std::nullopt;
    else if (not analyze_inner_functions())
      return std::nullopt;
    analyzer_mark = AnalyzerMark::COMPLETE;
    return std::monostate{};
  case AnalyzerMark::INITIATED:
    std::breakpoint();
    error_descriptor.emplace<UnresolvableCircularity>();
    return std::nullopt;
  case AnalyzerMark::COMPLETE:
    return std::monostate{};
  }
  std::unreachable();
}

std::optional<std::monostate> ModuleDefinition::analyze() {
  if (not analyze_statements())
    return std::nullopt;
  else if (not analyze_inner_functions())
    return std::nullopt;
  return std::monostate{};
}

enum class SerialExpression : std::uint8_t {
  MONOSTATE,
  LITERAL,
  SCOPE_ACCESSOR,
  ADDITION,
  SUBTRACTION,
  FUNCTION_CALL,
  CONSOLE_CALL,
  LENGTH_INTRINSIC,
  STRINGIFY_INTRINSIC
};
enum class SerialStatement : std::uint8_t {
  MONOSTATE,
  INITIALIZE_SCOPE,
  EXPRESSION,
  RETURN_STMT
};

struct StatementSerializer final : StatementVisitor<std::monostate> {
  using StatementVisitor<std::monostate>::operator();

  std::optional<std::monostate> operator()(const InitializeScope &statement);
  std::optional<std::monostate>
  operator()(const InitializeVariable &statement) {
    std::unreachable();
  }

  StatementSerializer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  std::vector<std::byte> bytecode;
};

struct ExpressionSerializer final : ExpressionVisitor<std::monostate> {
  using ExpressionVisitor<std::monostate>::operator();

  ExpressionSerializer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  std::vector<std::byte> bytecode;
};

template <typename T>
std::optional<std::monostate> AbstractBoundary<T>::serialize() {
  for (FunctionDefinition *definition : inner_functions)
    if (not definition->serialize())
      return std::nullopt;
  for (const AnyStatement &statement : *program) {
    StatementSerializer stmt_visitor{*this};
    statement.alt.visit(stmt_visitor);
  }
  return std::monostate{};
}

struct TypeSize : TypeVisitor<std::size_t> {
  using TypeVisitor<std::size_t>::operator();

  std::optional<std::size_t> operator()(NumberType) { return sizeof(double); }
  std::optional<std::size_t> operator()(StringType) {
    return sizeof(VariantStringView);
  }
  std::optional<std::size_t> operator()(DynamicType) {
    return sizeof(DynamicValue);
  }
  std::optional<std::size_t> operator()(LambdaType) {
    return sizeof(LambdaDescriptor);
  }
};

std::optional<std::monostate>
StatementSerializer::operator()(const InitializeScope &initialize_scope) {
  std::size_t byte_offset{};
  for (std::size_t i = 0; i < initialize_scope.local_offset; ++i) {
    std::optional variant_type{boundary.local_layout.values()[i].variant_type};
    if (not variant_type)
      return std::nullopt;
    TypeSize size_visitor{};
    std::optional type_size{variant_type->alt.visit(size_visitor)};
    if (not type_size)
      return std::nullopt;
    byte_offset += *type_size;
  }
  bytecode.push_back(
      std::byte{std::to_underlying(SerialStatement::INITIALIZE_SCOPE)});
  using serial_offset_t = std::array<std::byte, sizeof(std::size_t)>;
  bytecode.append_range(std::bit_cast<serial_offset_t>(byte_offset));
  ExpressionSerializer expression_serializer{boundary};
  initialize_scope.rvalue->alt.visit(expression_serializer);
  bytecode.append_range(std::move(expression_serializer.bytecode));
  return std::monostate{};
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

bool Language::compile_and_execute() {
  std::set<LambdaType> lambda_types{};
  std::set<VariantString> string_atlas{};
  Heap heap{};

  Tokenizer tokenizer{};
  tokenizer.text_buffer = std::move(text_buffer);
  tokenizer.string_atlas = &string_atlas;

  ModuleDefinition definition{};
  definition.program = &heap.program_list.emplace_back();
  definition.heap = &heap;

  ModuleParser parser{definition, tokenizer};
  parser.string_atlas = &string_atlas;

  if (not parser.parse_module()) {
    variant_error = error_descriptor;
    return 0;
  } else if (not definition.analyze()) {
    variant_error = error_descriptor;
    return 0;
  } else if (not definition.serialize()) {
    variant_error = error_descriptor;
    return 0;
  } else
    return 1;
}
} // namespace Manadrain
