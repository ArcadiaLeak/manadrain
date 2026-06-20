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

template <typename T> using Fallible = std::expected<T, VariantError>;
template <typename T> using Throw = std::unexpected<T>;
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
  std::set<std::string> *string_atlas;
  std::set<std::u16string> *u16string_atlas;

  std::size_t position;
  std::vector<std::optional<char32_t>> backtrace;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  Fallible<Token> tokenize_identifier(char32_t leading);
  Fallible<Token> tokenize_string_literal(char32_t separator);
  Fallible<Token> tokenize_numeric_literal(char32_t leading);

  Token last_token;
  Fallible<void> assert_punct(char32_t must_be);
  Fallible<void> tokenize();
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
  VariantType(Alternative a) : alt{std::move(a)} {};

  auto operator<=>(const VariantType &) const = default;
  std::size_t type_size() const;
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

std::size_t VariantType::type_size() const {
  struct SizeVisitor {
    std::size_t operator()(NumberType) { return sizeof(double); }
    std::size_t operator()(StringType) { return sizeof(VariantStringView); }
    std::size_t operator()(DynamicType) { return sizeof(DynamicValue); }
    std::size_t operator()(const LambdaType *) {
      return sizeof(LambdaDescriptor);
    }
  };
  return alt.visit(SizeVisitor{});
}

class ObjectShape {
public:
  std::flat_map<Identifier, std::optional<VariantType>> properties;
};

struct AnyStatement;

class FunctionDefinition;
class ScopeAccessor;

struct LayoutEntry {
  std::optional<VariantType> variant_type;
  std::optional<std::size_t> offset;
};

struct ProgramTape {
  std::pmr::monotonic_buffer_resource resource;
  std::list<FunctionDefinition> function_definition_list;
  std::list<std::list<AnyStatement>> program_list;
};

class LexicalBoundary {
public:
  LexicalBoundary *parent_boundary;

  std::list<AnyStatement> *program;
  std::vector<FunctionDefinition *> inner_functions;
  ProgramTape *tape;

  std::flat_map<Identifier, LayoutEntry> local_layout;

  virtual FunctionDefinition *find_function(Identifier identifier) = 0;
  virtual std::optional<ScopeAccessor>
  find_local(Identifier identifier) const = 0;

  virtual Fallible<void> put_return_type(VariantType variant_type) = 0;
};

template <typename T> class AbstractBoundary : public LexicalBoundary {
public:
  FunctionDefinition *find_function(Identifier identifier) override;
  std::optional<ScopeAccessor> find_local(Identifier identifier) const override;
  void serialize();

protected:
  AbstractBoundary() = default;
  Fallible<void> analyze_statements();
  Fallible<void> analyze_inner_functions();
};

class FunctionDefinition final : public AbstractBoundary<FunctionDefinition> {
public:
  std::optional<Identifier> function_name;

  std::optional<VariantType> return_type;
  std::vector<Identifier> arguments;

  enum class AnalyzerMark { PENDING, INITIATED, COMPLETE };
  AnalyzerMark analyzer_mark;
  Fallible<void> analyze();

  Fallible<void> put_return_type(VariantType variant_type) override {
    return_type.emplace(variant_type);
    return Fallible<void>{};
  }

private:
  Fallible<void> analyze_formal_parameters();
};
class ModuleDefinition final : public AbstractBoundary<ModuleDefinition> {
public:
  Fallible<void> analyze();

  Fallible<void> put_return_type(VariantType variant_type) override {
    std::breakpoint();
    return Throw<InvalidReturnStatement>(std::in_place);
  }
};

template <typename T> class Parser {
public:
  Fallible<void> parse_function_stmt();
  Fallible<void> parse_return_stmt();
  Fallible<void> parse_expression_stmt();
  Fallible<void> parse_variable_decl();
  Fallible<void> parse_statement();

  Fallible<AnyExpression> parse_expression();
  Fallible<AnyExpression> parse_assign_expression();
  Fallible<AnyExpression> parse_logical_disjunct();
  Fallible<AnyExpression> parse_additive_expression();
  Fallible<AnyExpression> parse_object_literal();

protected:
  Parser(T &b, Tokenizer &t) : boundary{b}, tokenizer{t} {}
  T &boundary;
  Tokenizer &tokenizer;

private:
  struct PostfixExpressionSaga {
    Fallible<AnyExpression> parse_member_expression(AnyExpression expression);
    Fallible<AnyExpression> parse_function_call(AnyExpression expression);
    Fallible<AnyExpression> operator()(AnyExpression expression);
    Parser &parser;
  };
  Fallible<AnyExpression> parse_postfix_expression();

  struct PrimaryExpressionSaga;
  Fallible<AnyExpression> parse_primary_expression();

  friend class Language;
};

class FunctionParser final : public Parser<FunctionDefinition> {
public:
  FunctionParser(FunctionDefinition &b, Tokenizer &t) : Parser{b, t} {}
  Fallible<void> parse_function_decl();
};
class ModuleParser final : public Parser<ModuleDefinition> {
public:
  ModuleParser(ModuleDefinition &b, Tokenizer &t) : Parser{b, t} {}
  Fallible<void> parse_module();
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
  using Alternative =
      std::variant<NumericLiteral, AsciiLiteral, UnicodeLiteral,
                   AliasedFunctionCall, DirectFunctionCall, MemberExpression,
                   AssignExpression, LogicalExpression, BinaryExpression,
                   ObjectExpression, VariableAccessor, ScopeAccessor,
                   IntrinsicAccessor, LambdaExpression, ConsoleCall,
                   LengthIntrinsic, StringifyIntrinsic>;
  Alternative alt;
  AnyExpression(Alternative a) : alt{std::move(a)} {};
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

template <typename T> class ResourceDeleter {
private:
  std::pmr::memory_resource *resource;

public:
  ResourceDeleter(std::pmr::memory_resource *r) : resource{r} {}
  void operator()(T *pointer);
};

template <typename T> void ResourceDeleter<T>::operator()(T *pointer) {
  std::pmr::polymorphic_allocator<> allocator(resource);
  allocator.delete_object<T>(pointer);
}

template <typename T>
using ResourcePointer = std::unique_ptr<T, ResourceDeleter<T>>;

struct AnyStatement {
  using Alternative = std::variant<
      ResourcePointer<InitializeVariable>, ResourcePointer<InitializeScope>,
      ResourcePointer<ReturnStatement>, ResourcePointer<ExpressionStatement>>;
  Alternative alt;
  AnyStatement(Alternative a) : alt{std::move(a)} {};
};

struct ConsoleMessage {
  std::vector<VariantString> parts;
  std::string encode_for_print() const;
};

struct RuntimeFrame {
  const LexicalBoundary *boundary;
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

Fallible<Token> Tokenizer::tokenize_identifier(char32_t leading) {
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

Fallible<Token> Tokenizer::tokenize_string_literal(char32_t separator) {
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
      return Throw<UnexpectedStringEnd>(std::in_place);
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

Fallible<Token> Tokenizer::tokenize_numeric_literal(char32_t leading) {
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

Fallible<void> Tokenizer::tokenize() {
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
      return Fallible<void>{};
    } else
      backward();
    if (std::ranges::binary_search(std::to_array({'"', '\'', '`'}), leading)) {
      std::expected token_opt{tokenize_string_literal(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return Fallible<void>{};
      } else
        return Throw(token_opt.error());
    }
    if (uc_is_property_xid_start(leading) || leading == '_') {
      std::expected token_opt{tokenize_identifier(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return Fallible<void>{};
      } else
        return Throw(token_opt.error());
    }
    static const std::array legal_punct{std::to_array<char32_t>(
        {'(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '=', '{', '}'})};
    if (std::ranges::binary_search(legal_punct, leading)) {
      last_token.emplace<char32_t>(leading);
      return Fallible<void>{};
    }
    if (std::isdigit(leading)) {
      std::expected token_opt{tokenize_numeric_literal(leading)};
      if (token_opt) {
        last_token = *token_opt;
        return Fallible<void>{};
      } else
        return Throw(token_opt.error());
    }
    std::breakpoint();
    return Throw<UnexpectedToken>(std::in_place);
  }
  last_token.emplace<std::monostate>();
  return Fallible<void>{};
}

Fallible<void> Tokenizer::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return Fallible<void>{};
  else {
    std::breakpoint();
    return Throw<MissingPunctuation>(std::in_place, must_be);
  }
}

Fallible<void> FunctionParser::parse_function_decl() {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
  if (not std::holds_alternative<Identifier>(tokenizer.last_token))
    goto after_name;
  boundary.function_name = std::get<Identifier>(tokenizer.last_token);
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
after_name:
  if (auto void_opt = tokenizer.assert_punct('('); not void_opt)
    return Throw(void_opt.error());
formal_parameter: {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
  if (tokenizer.last_token == Token{U')'})
    goto after_parameters;
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    return Throw<MissingFormalParameter>(std::in_place);
  }
  Identifier parameter{std::get<Identifier>(tokenizer.last_token)};
  boundary.local_layout.try_emplace(parameter);
  boundary.arguments.push_back(parameter);
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
  if (tokenizer.last_token == Token{U')'})
    goto after_parameters;
  if (auto void_opt = tokenizer.assert_punct(','); not void_opt)
    return Throw(void_opt.error());
  goto formal_parameter;
}
after_parameters:
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
  if (auto void_opt = tokenizer.assert_punct('{'); not void_opt)
    return Throw(void_opt.error());
body_statement: {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
  if (std::holds_alternative<std::monostate>(tokenizer.last_token)) {
    std::breakpoint();
    return Throw<MissingPunctuation>(std::in_place, '}');
  }
  char32_t *alter_ptr{std::get_if<char32_t>(&tokenizer.last_token)};
  if (alter_ptr && *alter_ptr == '}')
    return Fallible<void>{};
  if (auto void_opt = parse_statement(); not void_opt)
    return Throw(void_opt.error());
  goto body_statement;
}
}

Fallible<void> ModuleParser::parse_module() {
  while (1) {
    if (auto token_result = tokenizer.tokenize(); not token_result)
      return Throw(token_result.error());
    if (std::holds_alternative<std::monostate>(tokenizer.last_token))
      return Fallible<void>{};
    if (auto statement_result = parse_statement(); not statement_result)
      return Throw(statement_result.error());
  }
}

template <typename T> Fallible<void> Parser<T>::parse_statement() {
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

template <typename T> Fallible<void> Parser<T>::parse_return_stmt() {
  if (auto token_opt = tokenizer.tokenize(); not token_opt)
    return Throw(token_opt.error());
  if (auto expression_result = parse_expression(); not expression_result)
    return Throw(expression_result.error());
  else {
    std::pmr::polymorphic_allocator<> allocator(&boundary.tape->resource);
    auto statement_pointer =
        allocator.new_object<ReturnStatement>(std::move(*expression_result));
    boundary.program->emplace_back(ResourcePointer<ReturnStatement>(
        statement_pointer, allocator.resource()));
    return tokenizer.assert_punct(';');
  }
}

template <typename T> Fallible<void> Parser<T>::parse_expression_stmt() {
  if (auto expression_result = parse_expression(); not expression_result)
    return Throw(expression_result.error());
  else {
    std::pmr::polymorphic_allocator<> allocator(&boundary.tape->resource);
    auto statement_pointer = allocator.new_object<ExpressionStatement>(
        std::move(*expression_result));
    boundary.program->emplace_back(ResourcePointer<ExpressionStatement>(
        statement_pointer, allocator.resource()));
    return tokenizer.assert_punct(';');
  }
}

template <typename T> Fallible<void> Parser<T>::parse_function_stmt() {
  FunctionDefinition *definition{
      &boundary.tape->function_definition_list.emplace_back()};
  definition->parent_boundary = &boundary;
  definition->program = &boundary.tape->program_list.emplace_back();
  definition->tape = boundary.tape;
  FunctionParser function_parser{*definition, tokenizer};
  if (auto void_opt = function_parser.parse_function_decl(); not void_opt)
    return Throw(void_opt.error());
  if (not definition->function_name) {
    std::breakpoint();
    return Throw<MissingFunctionName>(std::in_place);
  }
  bool duplicate_exists =
      std::ranges::contains(boundary.inner_functions, definition->function_name,
                            [](const auto &el) { return el->function_name; });
  if (duplicate_exists) {
    std::breakpoint();
    return Throw<DuplicateDeclaration>(std::in_place);
  } else {
    boundary.inner_functions.push_back(definition);
    return Fallible<void>{};
  }
}

template <typename T>
Fallible<AnyExpression> Parser<T>::parse_assign_expression() {
  auto left_expression = parse_logical_disjunct();
  if (not left_expression)
    return Throw(left_expression.error());
  if (tokenizer.last_token != Token{U'='})
    return left_expression;
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  auto expression_result = parse_assign_expression();
  if (not expression_result)
    return Throw(expression_result.error());
  return AnyExpression::Alternative(std::in_place_type<AssignExpression>,
                                    std::move(*left_expression),
                                    std::move(*expression_result));
}

template <typename T> Fallible<void> Parser<T>::parse_variable_decl() {
  if (auto void_opt = tokenizer.tokenize(); not void_opt)
    return Throw(void_opt.error());
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    return Throw<MissingVariableName>(std::in_place);
  }
  Identifier variable_name{std::get<Identifier>(tokenizer.last_token)};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (auto assert_result = tokenizer.assert_punct('='); not assert_result)
    return Throw(assert_result.error());
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (auto expression_result = parse_expression(); not expression_result)
    return Throw(expression_result.error());
  else {
    std::pmr::polymorphic_allocator<> allocator(&boundary.tape->resource);
    auto statement_pointer =
        allocator.new_object<InitializeVariable>(std::move(*expression_result));
    statement_pointer->variable_name = variable_name;
    boundary.program->emplace_back(ResourcePointer<InitializeVariable>(
        statement_pointer, allocator.resource()));
  }
  if (auto assert_result = tokenizer.assert_punct(';'); not assert_result)
    return Throw(assert_result.error());
  if (boundary.local_layout.contains(variable_name)) {
    std::breakpoint();
    return Throw<DuplicateDeclaration>(std::in_place);
  } else {
    boundary.local_layout.try_emplace(variable_name);
    return Fallible<void>{};
  }
}

template <typename T> Fallible<AnyExpression> Parser<T>::parse_expression() {
  return parse_assign_expression();
}

template <typename T>
Fallible<AnyExpression> Parser<T>::parse_logical_disjunct() {
  auto left_expression = parse_additive_expression();
  if (not left_expression)
    return Throw(left_expression.error());
right_expression: {
  if (tokenizer.last_token != Token{Operator::LOGICAL_DISJUNCT})
    return left_expression;
  Operator logical_op{std::get<Operator>(tokenizer.last_token)};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (auto expression_result = parse_additive_expression();
      not expression_result)
    return Throw(expression_result.error());
  else {
    LogicalExpression expression{std::move(*left_expression),
                                 std::move(*expression_result)};
    expression.op = logical_op;
    left_expression->alt.template emplace<LogicalExpression>(
        std::move(expression));
  }
  goto right_expression;
}
}

template <typename T>
Fallible<AnyExpression>
Parser<T>::PostfixExpressionSaga::operator()(AnyExpression expression) {
  if (auto token_result = parser.tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
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
Fallible<AnyExpression> Parser<T>::PostfixExpressionSaga::parse_function_call(
    AnyExpression expression) {
  AliasedFunctionCall function_call(std::move(expression));
  while (1) {
    if (auto token_result = parser.tokenizer.tokenize(); not token_result)
      return Throw(token_result.error());
    if (parser.tokenizer.last_token == Token{U')'})
      break;
    auto argument_expression = parser.parse_expression();
    if (not argument_expression)
      return Throw(argument_expression.error());
    function_call.arguments.push_back(std::move(*argument_expression));
    if (parser.tokenizer.last_token == Token{U')'})
      break;
    if (auto assert_result = parser.tokenizer.assert_punct(',');
        not assert_result)
      return Throw(assert_result.error());
  }
  return Fallible<AnyExpression>(std::in_place, std::move(function_call))
      .and_then(*this);
}

template <typename T>
Fallible<AnyExpression>
Parser<T>::PostfixExpressionSaga::parse_member_expression(
    AnyExpression expression) {
  if (auto token_result = parser.tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (auto *field_name = std::get_if<Identifier>(&parser.tokenizer.last_token);
      not field_name) {
    std::breakpoint();
    return Throw<MissingPropertyName>(std::in_place);
  } else {
    MemberExpression member_expression(std::move(expression));
    member_expression.property = *field_name;
    return Fallible<AnyExpression>(std::in_place, std::move(member_expression))
        .and_then(*this);
  }
}

template <typename T>
Fallible<AnyExpression> Parser<T>::parse_postfix_expression() {
  return parse_primary_expression().and_then(PostfixExpressionSaga(*this));
}

template <typename T>
Fallible<AnyExpression> Parser<T>::parse_additive_expression() {
  auto left_expression = parse_postfix_expression();
  if (not left_expression)
    return Throw(left_expression.error());
  auto *operator_ahead = std::get_if<char32_t>(&tokenizer.last_token);
  static const std::array additive_punct = std::to_array<char32_t>({'+', '-'});
  bool should_proceed =
      operator_ahead && std::ranges::contains(additive_punct, *operator_ahead);
  if (not should_proceed)
    return std::move(*left_expression);
  char32_t binary_op{*operator_ahead};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (auto right_expression = parse_postfix_expression(); not right_expression)
    return Throw(right_expression.error());
  else {
    BinaryExpression binary_expression{std::move(*left_expression),
                                       std::move(*right_expression)};
    binary_expression.op = binary_op;
    return AnyExpression{std::move(binary_expression)};
  }
}

template <typename T>
Fallible<AnyExpression> Parser<T>::parse_object_literal() {
  ObjectExpression object_expression{};
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
object_property: {
  if (not std::holds_alternative<Identifier>(tokenizer.last_token)) {
    std::breakpoint();
    return Throw<InvalidPropertyName>(std::in_place);
  }
  Identifier property_name{std::get<Identifier>(tokenizer.last_token)};
  object_expression.object_shape.properties.try_emplace(property_name);
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  object_expression.keys.push_back(property_name);
  std::optional<AnyExpression> initializer{};
  if (tokenizer.last_token != Token{U':'})
    goto after_initializer;
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (auto expression_result = parse_expression(); not expression_result)
    return Throw(expression_result.error());
  else
    initializer.emplace(std::move(*expression_result));
after_initializer:
  object_expression.values.push_back(std::move(initializer));
  if (tokenizer.last_token != Token{U','})
    goto after_properties;
  if (auto token_result = tokenizer.tokenize(); not token_result)
    return Throw(token_result.error());
  if (tokenizer.last_token != Token{U'}'})
    goto object_property;
}
after_properties:
  if (auto assert_result = tokenizer.assert_punct('}'); not assert_result)
    return Throw(assert_result.error());
  return AnyExpression{std::move(object_expression)};
}

template <typename T> class Parser<T>::PrimaryExpressionSaga {
private:
  Parser &parser;

public:
  PrimaryExpressionSaga(Parser &p) : parser{p} {}

  Fallible<AnyExpression> operator()(char32_t punct);
  Fallible<AnyExpression> operator()(Keyword keyword);

  Fallible<AnyExpression> operator()(std::monostate) {
    std::breakpoint();
    return Throw<UnexpectedToken>(std::in_place);
  }

  Fallible<AnyExpression> operator()(std::int64_t number) {
    NumericLiteral num_literal{};
    num_literal.val = static_cast<double>(number);
    return AnyExpression{num_literal};
  }

  Fallible<AnyExpression> operator()(double number) {
    NumericLiteral numeric_literal{};
    numeric_literal.val = number;
    return AnyExpression{numeric_literal};
  }

  Fallible<AnyExpression> operator()(Operator op) {
    std::breakpoint();
    return Throw<UnexpectedToken>(std::in_place);
  }

  Fallible<AnyExpression> operator()(Identifier identifier) {
    VariableAccessor variable_accessor{};
    variable_accessor.identifier = identifier;
    return AnyExpression{variable_accessor};
  }

  Fallible<AnyExpression> operator()(std::string_view ascii_view) {
    AsciiLiteral ascii_literal{};
    ascii_literal.val_view = ascii_view;
    return AnyExpression{ascii_literal};
  }

  Fallible<AnyExpression> operator()(std::u16string_view unicode_view) {
    UnicodeLiteral unicode_literal{};
    unicode_literal.val_view = unicode_view;
    return AnyExpression{unicode_literal};
  }
};

template <typename T>
Fallible<AnyExpression>
Parser<T>::PrimaryExpressionSaga::operator()(char32_t punct) {
  if (punct == '{')
    return parser.parse_object_literal();
  else {
    std::breakpoint();
    return Throw<UnexpectedToken>(std::in_place);
  }
}

template <typename T>
Fallible<AnyExpression>
Parser<T>::PrimaryExpressionSaga::operator()(Keyword keyword) {
  if (keyword != Keyword::K_FUNCTION) {
    std::breakpoint();
    return Throw<UnexpectedToken>(std::in_place);
  }
  LambdaExpression lambda_expression{};
  lambda_expression.definition =
      &parser.boundary.tape->function_definition_list.emplace_back();
  FunctionParser function_parser{*lambda_expression.definition,
                                 parser.tokenizer};
  if (auto void_opt = function_parser.parse_function_decl(); not void_opt)
    return Throw(void_opt.error());
  return AnyExpression{lambda_expression};
}

template <typename T>
Fallible<AnyExpression> Parser<T>::parse_primary_expression() {
  return tokenizer.last_token.visit(PrimaryExpressionSaga(*this));
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
Fallible<void> AbstractBoundary<T>::analyze_inner_functions() {
  for (FunctionDefinition *definition : inner_functions) {
    definition->parent_boundary = this;
    if (auto void_opt = definition->analyze(); not void_opt)
      return Throw(void_opt.error());
  }
  return Fallible<void>{};
}

struct PropertyFinder {
  Fallible<AnyExpression> operator()(StringType);
  template <std::derived_from<ValueType> T>
  Fallible<AnyExpression> operator()(T) {
    std::unreachable();
  }
  Fallible<AnyExpression> operator()(const LambdaType *) { std::unreachable(); }

  AnyExpression source_expression;
  Identifier identifier;
  PropertyFinder(AnyExpression e) : source_expression{std::move(e)} {}
};

struct RvalueAnalyzer;

struct CalleeAnalyzer {
  Fallible<AnyExpression> operator()(const MemberExpression &expression);
  Fallible<AnyExpression> operator()(const VariableAccessor &expression);
  template <std::derived_from<Expression> T>
  Fallible<AnyExpression> operator()(const T &expression) {
    std::unreachable();
  }

  CalleeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  std::span<const AnyExpression> arguments;
};

struct RvalueAnalyzer {
  Fallible<AnyExpression> operator()(const AsciiLiteral &ascii_literal) {
    return AnyExpression{ascii_literal};
  }
  Fallible<AnyExpression> operator()(const NumericLiteral &numeric_literal) {
    return AnyExpression{numeric_literal};
  }
  Fallible<AnyExpression> operator()(const AliasedFunctionCall &expression);
  Fallible<AnyExpression> operator()(const VariableAccessor &expression);
  Fallible<AnyExpression> operator()(const BinaryExpression &expression);
  Fallible<AnyExpression> operator()(const MemberExpression &expression);
  template <std::derived_from<Expression> T>
  Fallible<AnyExpression> operator()(const T &expression) {
    std::unreachable();
  }

  RvalueAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

struct MethodAnalyzer {
  Fallible<AnyExpression>
  operator()(const IntrinsicAccessor &intrinsic_accessor);
  template <std::derived_from<Expression> T>
  Fallible<AnyExpression> operator()(const T &expression) {
    std::unreachable();
  }

  MethodAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
  Identifier identifier;
  std::span<const AnyExpression> arguments;
};

struct DatatypeAnalyzer {
  VariantType operator()(const AsciiLiteral &) {
    return VariantType{StringType{}};
  }
  VariantType operator()(const ScopeAccessor &accessor) {
    return accessor.local_type;
  }
  VariantType operator()(const BinaryExpression &) {
    return VariantType{NumberType{}};
  }
  VariantType operator()(const DirectFunctionCall &direct_call) {
    assert(direct_call.callee != nullptr);
    assert(direct_call.callee->return_type.has_value());
    return *direct_call.callee->return_type;
  }
  template <std::derived_from<Expression> T> VariantType operator()(const T &) {
    std::unreachable();
  }

  DatatypeAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

struct ExpressionStringifier {
  AnyExpression operator()(NumberType);
  template <std::derived_from<ValueType> T> AnyExpression operator()(T) {
    std::unreachable();
  }
  AnyExpression operator()(const LambdaType *) { std::unreachable(); }

  AnyExpression source_expression;
  ExpressionStringifier(AnyExpression e) : source_expression{std::move(e)} {}
};

Fallible<AnyExpression> PropertyFinder::operator()(StringType) {
  switch (identifier.offset) {
  case OFFSET_length:
    return Fallible<AnyExpression>(
        std::in_place, LengthIntrinsic{std::move(source_expression)});
  default:
    std::breakpoint();
    return Throw<InvalidPropertyAccess>(std::in_place);
  }
}

AnyExpression ExpressionStringifier::operator()(NumberType) {
  return AnyExpression::Alternative(std::in_place_type<StringifyIntrinsic>,
                                    std::move(source_expression));
}

Fallible<AnyExpression>
RvalueAnalyzer::operator()(const AliasedFunctionCall &function_call) {
  CalleeAnalyzer callee_visitor{boundary};
  callee_visitor.arguments = function_call.arguments;
  return function_call.callee->alt.visit(callee_visitor);
}

Fallible<AnyExpression>
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
    return Throw<InvalidVariableAccess>(std::in_place);
  }
}

Fallible<AnyExpression>
RvalueAnalyzer::operator()(const BinaryExpression &expression) {
  if (auto left_analyzed = expression.left->alt.visit(*this); not left_analyzed)
    return left_analyzed;
  else if (auto right_analyzed = expression.right->alt.visit(*this);
           not right_analyzed)
    return right_analyzed;
  else {
    BinaryExpression output_expression{std::move(*left_analyzed),
                                       std::move(*right_analyzed)};
    output_expression.op = expression.op;
    return AnyExpression{std::move(output_expression)};
  }
}

Fallible<AnyExpression>
RvalueAnalyzer::operator()(const MemberExpression &expression) {
  auto object_analyzed = expression.object->alt.visit(*this);
  if (not object_analyzed)
    return Throw(object_analyzed.error());
  DatatypeAnalyzer datatype_visitor{boundary};
  auto datatype_analyzed = object_analyzed->alt.visit(datatype_visitor);
  PropertyFinder property_visitor{std::move(*object_analyzed)};
  property_visitor.identifier = expression.property;
  return datatype_analyzed.alt.visit(property_visitor);
}

Fallible<AnyExpression>
MethodAnalyzer::operator()(const IntrinsicAccessor &intrinsic_accessor) {
  if (intrinsic_accessor.object_type != IntrinsicObject::O_CONSOLE)
    return Throw<InvalidMethodAccess>(std::in_place);
  if (identifier.offset != OFFSET_log)
    return Throw<InvalidMethodAccess>(std::in_place);
  ConsoleCall console_call{};
  for (const AnyExpression &argument : arguments) {
    RvalueAnalyzer rvalue_visitor{boundary};
    std::expected argument_analyzed{argument.alt.visit(rvalue_visitor)};
    if (not argument_analyzed)
      return Throw(argument_analyzed.error());
    DatatypeAnalyzer datatype_visitor{boundary};
    auto argument_type = argument_analyzed->alt.visit(datatype_visitor);
    ExpressionStringifier stringifier{std::move(*argument_analyzed)};
    AnyExpression stringified_argument{argument_type.alt.visit(stringifier)};
    console_call.arguments.push_back(std::move(stringified_argument));
  }
  return AnyExpression{std::move(console_call)};
}

Fallible<AnyExpression>
CalleeAnalyzer::operator()(const MemberExpression &expression) {
  auto member_object = expression.object->alt.visit(RvalueAnalyzer{boundary});
  if (not member_object)
    return Throw(member_object.error());
  MethodAnalyzer method_visitor{boundary};
  method_visitor.identifier = expression.property;
  method_visitor.arguments = arguments;
  return member_object->alt.visit(method_visitor);
}

Fallible<AnyExpression>
CalleeAnalyzer::operator()(const VariableAccessor &variable_accessor) {
  FunctionDefinition *definition{
      variable_accessor.find_function_linkedly(&boundary)};
  if (not definition) {
    std::breakpoint();
    return Throw<InvalidVariableAccess>(std::in_place);
  }
  if (auto analyze_result = definition->analyze(); not analyze_result)
    return Throw(analyze_result.error());
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

struct StatementAnalyzer {
  Fallible<AnyStatement>
  operator()(ResourcePointer<ExpressionStatement> &statement);
  Fallible<AnyStatement>
  operator()(ResourcePointer<InitializeVariable> &statement);
  Fallible<AnyStatement>
  operator()(ResourcePointer<ReturnStatement> &statement);
  Fallible<AnyStatement>
  operator()(ResourcePointer<InitializeScope> &statement) {
    std::unreachable();
  }

  StatementAnalyzer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

Fallible<AnyStatement>
StatementAnalyzer::operator()(ResourcePointer<ExpressionStatement> &statement) {
  std::expected argument_analyzed{
      statement->argument->alt.visit(RvalueAnalyzer{boundary})};
  if (not argument_analyzed)
    return Throw(argument_analyzed.error());
  std::pmr::polymorphic_allocator<> allocator(&boundary.tape->resource);
  auto statement_pointer =
      allocator.new_object<ExpressionStatement>(std::move(*argument_analyzed));
  return AnyStatement::Alternative(std::in_place_index<3>, statement_pointer,
                                   allocator.resource());
}

Fallible<AnyStatement>
StatementAnalyzer::operator()(ResourcePointer<InitializeVariable> &statement) {
  auto rvalue_analyzed = statement->rvalue->alt.visit(RvalueAnalyzer{boundary});
  if (not rvalue_analyzed)
    return Throw(rvalue_analyzed.error());
  InitializeScope initialize_scope{std::move(*rvalue_analyzed)};
  VariantType datatype_analyzed{
      initialize_scope.rvalue->alt.visit(DatatypeAnalyzer{boundary})};
  boundary.local_layout[statement->variable_name].variant_type.emplace(
      datatype_analyzed);
  auto variable_it = boundary.local_layout.find(statement->variable_name);
  initialize_scope.local_offset =
      std::distance(boundary.local_layout.begin(), variable_it);
  std::pmr::polymorphic_allocator<> allocator(&boundary.tape->resource);
  auto statement_pointer =
      allocator.new_object<InitializeScope>(std::move(initialize_scope));
  return AnyStatement::Alternative(std::in_place_index<1>, statement_pointer,
                                   allocator.resource());
}

Fallible<AnyStatement>
StatementAnalyzer::operator()(ResourcePointer<ReturnStatement> &statement) {
  RvalueAnalyzer rvalue_visitor{boundary};
  auto argument_analyzed = statement->argument->alt.visit(rvalue_visitor);
  if (not argument_analyzed)
    return Throw(argument_analyzed.error());
  DatatypeAnalyzer datatype_visitor{boundary};
  VariantType argument_type{argument_analyzed->alt.visit(datatype_visitor)};
  if (auto void_opt = boundary.put_return_type(argument_type); not void_opt)
    return Throw(void_opt.error());
  std::pmr::polymorphic_allocator<> allocator(&boundary.tape->resource);
  auto statement_pointer =
      allocator.new_object<ReturnStatement>(std::move(*argument_analyzed));
  return AnyStatement::Alternative(std::in_place_index<2>, statement_pointer,
                                   allocator.resource());
}

template <typename T> Fallible<void> AbstractBoundary<T>::analyze_statements() {
  for (AnyStatement &any_statement : *program) {
    StatementAnalyzer visitor{*this};
    auto output_statement = any_statement.alt.visit(visitor);
    if (not output_statement)
      return Throw(output_statement.error());
    std::swap(any_statement, *output_statement);
  }
  return Fallible<void>{};
}

Fallible<void> FunctionDefinition::analyze_formal_parameters() {
  for (Identifier argument : arguments)
    local_layout.find(argument)->second.variant_type.emplace(DynamicType{});
  return Fallible<void>{};
}

Fallible<void> FunctionDefinition::analyze() {
  switch (analyzer_mark) {
  case AnalyzerMark::PENDING:
    analyzer_mark = AnalyzerMark::INITIATED;
    if (auto void_opt = analyze_formal_parameters(); not void_opt)
      return Throw(void_opt.error());
    if (auto void_opt = analyze_statements(); not void_opt)
      return Throw(void_opt.error());
    if (auto void_opt = analyze_inner_functions(); not void_opt)
      return Throw(void_opt.error());
    analyzer_mark = AnalyzerMark::COMPLETE;
    return Fallible<void>{};
  case AnalyzerMark::INITIATED:
    std::breakpoint();
    return Throw<UnresolvableCircularity>(std::in_place);
  case AnalyzerMark::COMPLETE:
    return Fallible<void>{};
  }
  std::unreachable();
}

Fallible<void> ModuleDefinition::analyze() {
  if (auto void_opt = analyze_statements(); not void_opt)
    return Throw(void_opt.error());
  if (auto void_opt = analyze_inner_functions(); not void_opt)
    return Throw(void_opt.error());
  return Fallible<void>{};
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

struct StatementSerializer {
  template <std::derived_from<Statement> T>
  void operator()(const ResourcePointer<T> &statement) {
    std::unreachable();
  }
  void operator()(const ResourcePointer<InitializeScope> &statement);

  StatementSerializer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

void StatementSerializer::operator()(
    const ResourcePointer<InitializeScope> &initialize_scope) {
  std::size_t byte_offset{};
  for (std::size_t i = 0; i < initialize_scope->local_offset; ++i) {
    std::optional variant_type{boundary.local_layout.values()[i].variant_type};
    assert(variant_type.has_value());
    byte_offset += variant_type->type_size();
  }
  Identifier entry_key{
      boundary.local_layout.keys()[initialize_scope->local_offset]};
  LayoutEntry &entry_value{boundary.local_layout[entry_key]};
  entry_value.offset = byte_offset;
  assert(entry_value.variant_type.has_value());
}

struct ExpressionSerializer {
  template <std::derived_from<Expression> T>
  void operator()(const T &expression) {
    std::unreachable();
  }

  ExpressionSerializer(LexicalBoundary &b) : boundary{b} {}
  LexicalBoundary &boundary;
};

template <typename T> void AbstractBoundary<T>::serialize() {
  for (FunctionDefinition *definition : inner_functions)
    definition->serialize();
  for (const AnyStatement &statement : *program) {
    StatementSerializer statement_serializer{*this};
    statement.alt.visit(statement_serializer);
  }
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

bool Language::compile_and_execute() {
  std::set<LambdaType> lambda_types{};
  std::set<std::string> string_atlas{};
  std::set<std::u16string> u16string_atlas{};
  ProgramTape tape{};

  Tokenizer tokenizer{};
  tokenizer.text_buffer = std::move(text_buffer);
  tokenizer.string_atlas = &string_atlas;
  tokenizer.u16string_atlas = &u16string_atlas;

  ModuleDefinition definition{};
  definition.program = &tape.program_list.emplace_back();
  definition.tape = &tape;

  ModuleParser parser{definition, tokenizer};

  if (auto parse_result = parser.parse_module(); not parse_result) {
    variant_error = parse_result.error();
    return 0;
  }
  if (auto analyze_result = definition.analyze(); not analyze_result) {
    variant_error = analyze_result.error();
    return 0;
  }
  definition.serialize();

  return 1;
}
} // namespace Manadrain
