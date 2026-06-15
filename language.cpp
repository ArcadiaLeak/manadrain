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

template <typename T> class UniqueElementList {
private:
  struct Node;
  mutable std::mutex nodes_mutex;
  std::list<Node> nodes;

public:
  class UniqueElement;
  UniqueElementList() = default;

  UniqueElement push(const T &value) { return emplace(value); }
  UniqueElement push(T &&value) { return emplace(std::move(value)); }

  template <typename... Args> UniqueElement emplace(Args &&...args);

  bool empty() const;
  std::size_t size() const;
};

template <typename T> bool UniqueElementList<T>::empty() const {
  std::lock_guard<std::mutex> lock(nodes_mutex);
  return nodes.empty();
}

template <typename T> std::size_t UniqueElementList<T>::size() const {
  std::lock_guard<std::mutex> lock(nodes_mutex);
  return nodes.size();
}

template <typename T>
template <typename... Args>
UniqueElementList<T>::UniqueElement
UniqueElementList<T>::emplace(Args &&...args) {
  std::lock_guard<std::mutex> lock{nodes_mutex};
  auto it = nodes.emplace(nodes.end(), &nodes_mutex, &nodes,
                          std::forward<Args>(args)...);
  it->it = it;
  return UniqueElement{&*it};
}

template <typename T> struct UniqueElementList<T>::Node {
  T data;
  typename std::list<Node>::iterator it;
  std::mutex *owner_mutex;
  std::list<Node> *owner_list;

  template <typename... Args>
  Node(std::mutex *mtx, std::list<Node> *lst, Args &&...args)
      : data{std::forward<Args>(args)...}, owner_mutex{mtx}, owner_list{lst} {}
};

template <typename T> class UniqueElementList<T>::UniqueElement {
public:
  UniqueElement(const UniqueElement &) = delete;
  UniqueElement &operator=(const UniqueElement &) = delete;

  UniqueElement(UniqueElement &&other) noexcept;
  UniqueElement &operator=(UniqueElement &&other) noexcept;

  ~UniqueElement();

  T &operator*() { return node_ptr->data; }
  T *operator->() { return &node_ptr->data; }

private:
  friend class UniqueElementList;
  explicit UniqueElement(Node *node) : node_ptr(node) {}
  Node *node_ptr = nullptr;
};

template <typename T>
UniqueElementList<T>::UniqueElement::UniqueElement(
    UniqueElement &&other) noexcept
    : node_ptr{other.node_ptr} {
  other.node_ptr = nullptr;
}

template <typename T>
UniqueElementList<T>::UniqueElement &
UniqueElementList<T>::UniqueElement::operator=(UniqueElement &&other) noexcept {
  if (this == &other)
    return *this;
  this->~UniqueElement();
  node_ptr = other.node_ptr;
  other.node_ptr = nullptr;
  return *this;
}

template <typename T> UniqueElementList<T>::UniqueElement::~UniqueElement() {
  if (!node_ptr)
    return;
  if constexpr (std::is_nothrow_move_constructible_v<T>) {
    std::unique_lock<std::mutex> lock{*node_ptr->owner_mutex};
    T local_data{std::move(node_ptr->data)};
    node_ptr->owner_list->erase(node_ptr->it);
    lock.unlock();
  } else {
    std::lock_guard<std::mutex> lock{*node_ptr->owner_mutex};
    node_ptr->owner_list->erase(node_ptr->it);
  }
}

static UniqueElementList<std::set<std::variant<std::string, std::u16string>>>
    string_atlas_list{};

using VariantString = std::variant<std::string, std::u16string>;
using StringAtlas = UniqueElementList<std::set<VariantString>>::UniqueElement;

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

  std::flat_map<std::string, Identifier> atom_atlas;
  std::shared_ptr<StringAtlas> string_atlas;

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

class ValueType {
protected:
  ValueType() = default;
  virtual std::size_t type_size() const = 0;
};
class ObjectShape;

class NumberType final : public ValueType {
  std::size_t type_size() const override { return sizeof(double); }
};

class AbstractStringView {
public:
  virtual std::string normalize() const = 0;
  virtual std::size_t size() const = 0;
};

class AsciiStringView final : public AbstractStringView {
public:
  AsciiStringView(std::string_view sv) : ascii_view{sv} {}
  std::string normalize() const override { return std::string{ascii_view}; }
  std::size_t size() const override { return ascii_view.size(); }

private:
  std::string_view ascii_view;
};

class UnicodeStringView final : public AbstractStringView {
public:
  UnicodeStringView(std::u16string_view sv) : unicode_view{sv} {}
  std::string normalize() const override;
  std::size_t size() const override { return unicode_view.size(); }

private:
  std::u16string_view unicode_view;
};

struct VariantType;

using VariantStringView = std::variant<AsciiStringView, UnicodeStringView>;
class StringType final : public ValueType {
  std::size_t type_size() const override { return sizeof(VariantStringView); }
};

struct LambdaDescriptor {};
class LambdaType final : public ValueType {
  std::span<VariantType> signature;
  std::size_t type_size() const override { return sizeof(LambdaDescriptor); }
};

struct VariantType {
  std::variant<NumberType, StringType, LambdaType> alt;
};

class ObjectShape {
public:
  std::flat_map<Identifier, VariantType> properties;
};

struct AnyExpression;
struct AnyStatement;

class FunctionDefinition;
class ScopeAccessor;

class LexicalBoundary {
public:
  virtual FunctionDefinition *find_function(Identifier identifier) = 0;
  virtual std::optional<ScopeAccessor>
  find_local(Identifier identifier) const = 0;
};

template <typename T> class AbstractBoundary : public LexicalBoundary {
public:
  LexicalBoundary *parent_boundary;
  std::list<AnyStatement> program;
  std::vector<std::byte> bytecode;

  std::list<FunctionDefinition> inner_functions;
  FunctionDefinition *find_function(Identifier identifier) override;

  std::flat_map<Identifier, VariantType> local_layout;
  std::optional<ScopeAccessor> find_local(Identifier identifier) const override;

  void serialize();

protected:
  AbstractBoundary() = default;
  void analyze_statements();
  void analyze_inner_functions();
};

class FunctionDefinition final : public AbstractBoundary<FunctionDefinition> {
public:
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  VariantType return_type;

  enum class AnalyzerMark { PENDING, INITIATED, COMPLETE };
  AnalyzerMark analyzer_mark;
  void analyze();

private:
  void analyze_formal_parameters();
};
class ModuleDefinition final : public AbstractBoundary<ModuleDefinition> {
public:
  void analyze();
};

void ModuleDefinition::analyze() {
  analyze_statements();
  analyze_inner_functions();
}

template <typename T> class Parser {
public:
  void parse_function_stmt();
  void parse_variable_decl();
  void parse_statement();

  AnyExpression parse_expression();
  AnyExpression parse_primary_expression();

  AnyExpression parse_assign_expression();
  AnyExpression parse_logical_disjunct();
  AnyExpression parse_additive_expression();
  AnyExpression parse_object_literal();

  void parse_member_expression(AnyExpression &expression);
  void parse_function_call(AnyExpression &expression);
  AnyExpression parse_postfix_expression();

protected:
  Parser(T &b, Tokenizer &t) : boundary{b}, tokenizer{t} {}
  T &boundary;
  Tokenizer &tokenizer;

private:
  friend class Language;
  std::shared_ptr<StringAtlas> string_atlas;
};

class FunctionParser final : public Parser<FunctionDefinition> {
public:
  FunctionParser(FunctionDefinition &b, Tokenizer &t) : Parser{b, t} {}
  void parse_function_decl();
};
class ModuleParser final : public Parser<ModuleDefinition> {
public:
  ModuleParser(ModuleDefinition &b, Tokenizer &t) : Parser{b, t} {}
};

class Expression {
protected:
  Expression() = default;
};

class AliasedFunctionCall final : public Expression {
public:
  AliasedFunctionCall(AnyExpression &&c);
  std::indirect<AnyExpression> callee;
  std::list<AnyExpression> arguments;
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
};

class ScopeAccessor final : public Expression {
public:
  ScopeAccessor() = default;
  VariantType local_type;
  std::array<std::size_t, 2> location;
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
  FunctionDefinition definition;
};

struct AnyExpression {
  std::variant<NumericLiteral, AsciiLiteral, UnicodeLiteral,
               AliasedFunctionCall, MemberExpression, AssignExpression,
               LogicalExpression, BinaryExpression, ObjectExpression,
               VariableAccessor, ScopeAccessor, LambdaExpression>
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

class Statement {
protected:
  Statement() = default;
};

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

class Machine {};

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

std::string UnicodeStringView::normalize() const {
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
        (*string_atlas)->emplace(std::string{std::from_range, ascii_string});
    return std::get<std::string>(*string_iter);
  }
  auto [u16string_iter, _] = (*string_atlas)->emplace(std::move(u16_literal));
  return std::get<std::u16string>(*u16string_iter);
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

void FunctionParser::parse_function_decl() {
  tokenizer.tokenize();
  if (std::holds_alternative<Identifier>(tokenizer.last_token)) {
    boundary.function_name = std::get<Identifier>(tokenizer.last_token);
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
    boundary.local_layout.try_emplace(parameter);
    boundary.arguments.push_back(parameter);
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

template <typename T> void Parser<T>::parse_statement() {
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
    boundary.program.emplace_back(std::move(statement));
    tokenizer.assert_punct(';');
    return;
  }
  default: {
    ExpressionStatement statement{parse_expression()};
    boundary.program.emplace_back(std::move(statement));
    tokenizer.assert_punct(';');
    return;
  }
  }
}

template <typename T> void Parser<T>::parse_function_stmt() {
  FunctionDefinition definition{};
  FunctionParser function_parser{definition, tokenizer};
  function_parser.parse_function_decl();
  if (not definition.function_name)
    throw MissingFunctionName{};
  bool duplicate_exists =
      std::ranges::contains(boundary.inner_functions, definition.function_name,
                            [](const auto &el) { return el.function_name; });
  if (duplicate_exists)
    throw DuplicateDeclaration{};
  boundary.inner_functions.push_back(std::move(definition));
}

template <typename T> AnyExpression Parser<T>::parse_assign_expression() {
  AnyExpression left_expression{parse_logical_disjunct()};
  if (tokenizer.last_token != Token{U'='})
    return left_expression;
  tokenizer.tokenize();
  AssignExpression expression{std::move(left_expression),
                              parse_assign_expression()};
  return AnyExpression{expression};
}

template <typename T> void Parser<T>::parse_variable_decl() {
  tokenizer.tokenize();
  if (not std::holds_alternative<Identifier>(tokenizer.last_token))
    throw MissingVariableName{};
  Identifier variable_name{std::get<Identifier>(tokenizer.last_token)};
  tokenizer.tokenize();
  tokenizer.assert_punct('=');
  tokenizer.tokenize();
  InitializeVariable initialize_variable{parse_expression()};
  initialize_variable.variable_name = variable_name;
  boundary.program.emplace_back(std::move(initialize_variable));
  tokenizer.assert_punct(';');
  if (boundary.local_layout.contains(variable_name))
    throw DuplicateDeclaration{};
  boundary.local_layout.try_emplace(variable_name);
}

template <typename T> AnyExpression Parser<T>::parse_expression() {
  return parse_assign_expression();
}

template <typename T> AnyExpression Parser<T>::parse_logical_disjunct() {
  AnyExpression left_expression{parse_additive_expression()};
right_expression: {
  if (tokenizer.last_token != Token{Operator::LOGICAL_DISJUNCT})
    return left_expression;
  Operator logical_op{std::get<Operator>(tokenizer.last_token)};
  tokenizer.tokenize();
  LogicalExpression expression{std::move(left_expression),
                               parse_additive_expression()};
  expression.op = logical_op;
  left_expression.alt.emplace<LogicalExpression>(std::move(expression));
  goto right_expression;
}
}

template <typename T> AnyExpression Parser<T>::parse_postfix_expression() {
  AnyExpression postfix_expression{parse_primary_expression()};
seek_postfix:
  tokenizer.tokenize();
  if (not std::holds_alternative<char32_t>(tokenizer.last_token))
    return postfix_expression;
  switch (std::get<char32_t>(tokenizer.last_token)) {
  case '.':
    parse_member_expression(postfix_expression);
    goto seek_postfix;
  case '(':
    parse_function_call(postfix_expression);
    goto seek_postfix;
  default:
    return postfix_expression;
  }
}

template <typename T>
void Parser<T>::parse_function_call(AnyExpression &expression) {
  AliasedFunctionCall &function_call{
      expression.alt.emplace<AliasedFunctionCall>(std::move(expression))};
  while (1) {
    tokenizer.tokenize();
    if (tokenizer.last_token == Token{U')'})
      return;
    function_call.arguments.push_back(parse_expression());
    if (tokenizer.last_token == Token{U')'})
      return;
    tokenizer.assert_punct(',');
  }
}

template <typename T>
void Parser<T>::parse_member_expression(AnyExpression &expression) {
  tokenizer.tokenize();
  if (not std::holds_alternative<Identifier>(tokenizer.last_token))
    throw MissingPropertyName{};
  Identifier field_name{std::get<Identifier>(tokenizer.last_token)};
  MemberExpression &member_expression{
      expression.alt.emplace<MemberExpression>(std::move(expression))};
  member_expression.property = field_name;
}

template <typename T> AnyExpression Parser<T>::parse_additive_expression() {
  AnyExpression left_expression{parse_postfix_expression()};
  if (tokenizer.last_token != Token{U'+'} &&
      tokenizer.last_token != Token{U'-'})
    return left_expression;
  char32_t binary_op{std::get<char32_t>(tokenizer.last_token)};
  tokenizer.tokenize();
  BinaryExpression &expression{left_expression.alt.emplace<BinaryExpression>(
      std::move(left_expression), parse_postfix_expression())};
  expression.op = binary_op;
  return left_expression;
}

template <typename T> AnyExpression Parser<T>::parse_object_literal() {
  ObjectExpression object_expression{};
  tokenizer.tokenize();
  while (tokenizer.last_token != Token{U'}'}) {
    if (not std::holds_alternative<Identifier>(tokenizer.last_token))
      throw InvalidPropertyName{};
    Identifier property_name{std::get<Identifier>(tokenizer.last_token)};
    object_expression.object_shape.properties.try_emplace(property_name);
    tokenizer.tokenize();
    object_expression.keys.push_back(property_name);
    std::optional<AnyExpression> initializer{};
    if (tokenizer.last_token == Token{U':'}) {
      tokenizer.tokenize();
      initializer = parse_expression();
    }
    object_expression.values.push_back(std::move(initializer));
    if (tokenizer.last_token != Token{U','})
      break;
    tokenizer.tokenize();
  }
  tokenizer.assert_punct('}');
  return AnyExpression{std::move(object_expression)};
}

template <typename T> AnyExpression Parser<T>::parse_primary_expression() {
  struct TokenVisitor {
    Parser<T> &parser;
    AnyExpression operator()(std::monostate) { throw UnexpectedToken{}; }
    AnyExpression operator()(char32_t punct) {
      if (punct == '{')
        return parser.parse_object_literal();
      else
        throw UnexpectedToken{};
    }
    AnyExpression operator()(std::int64_t number) {
      NumericLiteral num_literal{};
      num_literal.val = static_cast<double>(number);
      return AnyExpression{num_literal};
    }
    AnyExpression operator()(double number) {
      NumericLiteral numeric_literal{};
      numeric_literal.val = number;
      return AnyExpression{numeric_literal};
    }
    AnyExpression operator()(Operator op) { throw UnexpectedToken{}; }
    AnyExpression operator()(Keyword keyword) {
      switch (keyword) {
      case Keyword::K_FUNCTION: {
        LambdaExpression lambda_expression{};
        FunctionParser function_parser{lambda_expression.definition,
                                       parser.tokenizer};
        function_parser.parse_function_decl();
        return AnyExpression{lambda_expression};
      }
      default:
        throw UnexpectedToken{};
      }
    }
    AnyExpression operator()(Identifier identifier) {
      VariableAccessor variable_accessor{};
      variable_accessor.identifier = identifier;
      return AnyExpression{variable_accessor};
    }
    AnyExpression operator()(std::string_view ascii_view) {
      AsciiLiteral ascii_literal{};
      ascii_literal.val_view = ascii_view;
      return AnyExpression{ascii_literal};
    }
    AnyExpression operator()(std::u16string_view unicode_view) {
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
  ScopeAccessor accessor{};
  accessor.local_type = local_layout.values()[local_offset];
  std::get<1>(accessor.location) = local_offset;
  return accessor;
}

template <typename T>
FunctionDefinition *AbstractBoundary<T>::find_function(Identifier identifier) {
  auto by_name = [](const auto &def) { return def.function_name; };
  auto function_it = std::ranges::find(inner_functions, identifier, by_name);
  return function_it == inner_functions.end() ? nullptr : &(*function_it);
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

void Language::compile_and_execute() {
  std::shared_ptr string_atlas{
      std::make_shared<StringAtlas>(string_atlas_list.emplace())};

  Tokenizer tokenizer{};
  tokenizer.text_buffer = std::move(text_buffer);
  tokenizer.string_atlas = string_atlas;

  ModuleDefinition definition{};
  ModuleParser parser{definition, tokenizer};
  parser.string_atlas = string_atlas;

  while (1) {
    tokenizer.tokenize();
    if (std::holds_alternative<std::monostate>(tokenizer.last_token))
      break;
    parser.parse_statement();
  }
}
} // namespace Manadrain
