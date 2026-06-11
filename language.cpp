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
class MissingFieldName final : public CompilationError {
public:
  MissingFieldName() : CompilationError{"missing field name!"} {}
};
class InvalidPropertyName final : public CompilationError {
public:
  InvalidPropertyName() : CompilationError{"invalid property name!"} {}
};
class MissingVariableName final : public CompilationError {
public:
  MissingVariableName() : CompilationError{"missing variable name!"} {}
};
class MissingMethod final : public CompilationError {
public:
  MissingMethod() : CompilationError{"missing method!"} {}
};
class InvalidVariableAccess final : public CompilationError {
public:
  InvalidVariableAccess() : CompilationError{"invalid variable access!"} {}
};
class InvalidReturnStatement final : public CompilationError {
public:
  InvalidReturnStatement() : CompilationError{"invalid return statement!"} {}
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

struct VoidType {};
struct NumberType {};
struct StringType {};
struct LambdaType {};
struct AnyType {};
struct ObjectShape;
enum class IntrinsicObject { O_CONSOLE };
using ValueType = std::variant<VoidType, NumberType, StringType, LambdaType,
                               AnyType, const ObjectShape *, IntrinsicObject>;
struct ObjectShape {
  std::flat_map<Identifier, ValueType> properties;
};

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

enum class BinaryOperation { OP_ADD, OP_SUB };
enum class ValueKind { V_NUMBER, V_STRING, V_VOID };

struct ExprLiteral {};
struct ExprScopeAccess {};
template <BinaryOperation> struct ExprBinary {};
struct ExprFunctionCall {};
struct ExprConsole {};
struct ExprStringLength {};
struct ExprStringifyNumber {};
using ExpressionKind =
    std::variant<std::monostate, ExprLiteral, ExprScopeAccess,
                 ExprBinary<BinaryOperation::OP_ADD>,
                 ExprBinary<BinaryOperation::OP_SUB>, ExprFunctionCall,
                 ExprConsole, ExprStringLength, ExprStringifyNumber>;

struct StmtInitialize {};
struct StmtExpression {};
struct StmtReturn {};
using StatementKind =
    std::variant<std::monostate, StmtInitialize, StmtExpression, StmtReturn>;

using MachineString = std::variant<std::string, std::u16string>;
struct ConsoleMessage {
  std::vector<MachineString> parts;
  std::string encode_for_print() const;
};

class Machine {
public:
  Machine();
  std::list<ConsoleMessage> collect_console_messages(std::stop_token stopper);
  void evaluate();

  class ExecutionBoundary {
  public:
    std::flat_map<Identifier, ValueType> local_scope;
    std::vector<std::uint64_t> program;
  };

  const ExecutionBoundary *program_entry;
  std::vector<std::unique_ptr<ExecutionBoundary>> boundaries;
  std::set<MachineString> string_literals;

private:
  struct ExecutionFrame {
    const ExecutionBoundary *boundary;
    std::vector<std::uint64_t>::const_iterator program_iter;
    std::span<std::uint64_t> own_scope;
    std::span<ExecutionFrame *> closure;
    std::uint64_t return_val;
  };

  struct RuntimeMemory {
    std::pmr::monotonic_buffer_resource resource{65536};
    std::pmr::list<ExecutionFrame> execution_frames{&resource};
    std::pmr::list<std::pmr::vector<ExecutionFrame *>> scope_traces{&resource};
    std::pmr::list<std::pmr::vector<std::uint64_t>> structures{&resource};
    std::pmr::list<MachineString> strings{&resource};
  };

  ExecutionFrame *current_frame;
  std::unique_ptr<RuntimeMemory> runtime_memory;
  std::mutex console_mutex;
  std::condition_variable_any console_condition;
  std::list<ConsoleMessage> console_messages;

  struct EvaluateStatement {
    Machine &machine;
    void operator()(std::monostate) { std::unreachable(); }
    void operator()(StmtInitialize);
    void operator()(StmtExpression);
    void operator()(StmtReturn);
  };
  void evaluate_statement();

  struct EvaluateExpression {
    Machine &machine;
    std::uint64_t operator()(std::monostate) { std::unreachable(); }
    std::uint64_t operator()(ExprLiteral);
    std::uint64_t operator()(ExprScopeAccess);
    template <BinaryOperation P> std::uint64_t operator()(ExprBinary<P>);
    std::uint64_t operator()(ExprFunctionCall);
    std::uint64_t operator()(ExprConsole);
    std::uint64_t operator()(ExprStringLength);
    std::uint64_t operator()(ExprStringifyNumber);
  };
  std::uint64_t evaluate_expression();

  std::uint64_t evaluate_function_call(const ExecutionBoundary *boundary);
};

class Compiler {
public:
  Compiler(Machine &m) : machine{m} {}

  class ExecutionBoundary;
  class FunctionDefinition;
  class CalleeExpression;

  struct FindMethod {
    Identifier identifier;
    std::unique_ptr<CalleeExpression> operator()(VoidType);
    std::unique_ptr<CalleeExpression> operator()(NumberType);
    std::unique_ptr<CalleeExpression> operator()(StringType);
    std::unique_ptr<CalleeExpression> operator()(LambdaType);
    std::unique_ptr<CalleeExpression> operator()(AnyType);
    std::unique_ptr<CalleeExpression>
    operator()(const ObjectShape *object_shape);
    std::unique_ptr<CalleeExpression>
    operator()(IntrinsicObject intrinsic_object);
  };

  class Expression {
  public:
    virtual ~Expression() = default;

    virtual ValueType datatype() const = 0;

    virtual std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const = 0;

    virtual std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const = 0;

    virtual std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const = 0;
  };

  struct InjectStringify {
    std::unique_ptr<Expression> expression;
    std::unique_ptr<Expression> operator()(VoidType);
    std::unique_ptr<Expression> operator()(NumberType);
    std::unique_ptr<Expression> operator()(StringType);
    std::unique_ptr<Expression> operator()(LambdaType);
    std::unique_ptr<Expression> operator()(AnyType);
    std::unique_ptr<Expression> operator()(const ObjectShape *object_shape);
    std::unique_ptr<Expression> operator()(IntrinsicObject intrinsic_object);
  };

  class CalleeExpression : public Expression {
  public:
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    virtual void analyze_arguments(
        ExecutionBoundary &boundary,
        std::span<const std::unique_ptr<Expression>> arguments) = 0;
  };

  class AliasedFunctionCall final : public Expression {
  public:
    std::unique_ptr<Expression> callee;
    std::vector<std::unique_ptr<Expression>> arguments;

    ValueType datatype() const override { std::unreachable(); }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class DirectFunctionCall final : public CalleeExpression {
  public:
    const FunctionDefinition *callee;
    std::vector<Identifier> passed_identifiers;
    std::vector<std::unique_ptr<Expression>> passed_values;

    ValueType datatype() const override { return callee->return_type; }

    void analyze_arguments(
        ExecutionBoundary &boundary,
        std::span<const std::unique_ptr<Expression>> arguments) override;

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class ConsoleLogCall final : public CalleeExpression {
  public:
    std::vector<std::unique_ptr<Expression>> arguments;

    ValueType datatype() const override { return VoidType{}; }

    void analyze_arguments(
        ExecutionBoundary &boundary,
        std::span<const std::unique_ptr<Expression>> arguments) override;

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class ObjectExpression final : public Expression {
  public:
    ObjectShape object_shape;
    std::vector<Identifier> keys;
    std::vector<std::unique_ptr<Expression>> values;

    ValueType datatype() const override { std::unreachable(); }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class MemberExpression final : public Expression {
  public:
    std::unique_ptr<Expression> object;
    Identifier property;

    ValueType datatype() const override { std::unreachable(); }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override;

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class StringLength final : public Expression {
  public:
    std::unique_ptr<Expression> argument;

    ValueType datatype() const override { return NumberType{}; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class StringifyNumber final : public Expression {
  public:
    std::unique_ptr<Expression> argument;

    ValueType datatype() const override { return StringType{}; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class BinaryExpression final : public Expression {
  public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    char32_t op;

    ValueType datatype() const override { return NumberType{}; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class LogicalExpression final : public Expression {
  public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    Operator op;

    ValueType datatype() const override { std::unreachable(); }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class AssignExpression final : public Expression {
  public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    ValueType datatype() const override { std::unreachable(); }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class NumericLiteral final : public Expression {
  public:
    double val;

    ValueType datatype() const override { return NumberType{}; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class AsciiLiteral final : public Expression {
  public:
    std::string val;

    ValueType datatype() const override { return StringType{}; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class UnicodeLiteral final : public Expression {
  public:
    std::u16string val;

    ValueType datatype() const override { return StringType{}; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class VariableAccessor final : public Expression {
  public:
    Identifier identifier;

    ValueType datatype() const override { std::unreachable(); }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override;

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class ScopeAccessor final : public Expression {
  public:
    ValueType local_type;
    std::size_t scope_offset;
    std::size_t local_offset;

    ValueType datatype() const override { return local_type; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    };

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class IntrinsicAccessor final : public Expression {
  public:
    IntrinsicObject object_type;

    ValueType datatype() const override { return object_type; }

    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class Statement {
  public:
    virtual ~Statement() = default;
    virtual std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const = 0;
    virtual std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const = 0;
  };

  class InitializeVariable final : public Statement {
  public:
    Identifier variable_name;
    std::unique_ptr<Expression> rvalue;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override;
    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class InitializeScope final : public Statement {
  public:
    std::size_t local_offset;
    std::unique_ptr<Expression> rvalue;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override {
      std::unreachable();
    };
    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class ExpressionStatement final : public Statement {
  public:
    std::unique_ptr<Expression> argument;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override;
    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class ReturnStatement final : public Statement {
  public:
    std::unique_ptr<Expression> argument;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override;
    std::list<std::uint64_t>
    serialize(ExecutionBoundary &boundary) const override;
  };

  class ExecutionBoundary {
  public:
    ExecutionBoundary(Compiler &c);

    const ExecutionBoundary *parent_boundary;
    std::unique_ptr<Machine::ExecutionBoundary> output_boundary;
    std::vector<std::unique_ptr<Statement>> program;

    std::list<FunctionDefinition> inner_functions;
    const FunctionDefinition *find_function(Identifier identifier) const;

    std::flat_map<Identifier, ValueType> local_scope;
    std::unique_ptr<ScopeAccessor> find_local(Identifier identifier) const;

    ValueType return_type;
    virtual bool allows_return_stmt() const = 0;

    void parse_statement();
    void analyze();
    void serialize();
    void export_serial_program();

    const MachineString *push_string_literal(std::string_view ascii);
    const MachineString *push_string_literal(std::u16string_view unicode);

  protected:
    Compiler &compiler;

    void parse_function_stmt();
    void parse_variable_decl();

    std::unique_ptr<Expression> parse_expression();

    std::unique_ptr<Expression> parse_assign_expr();
    std::unique_ptr<Expression> parse_logical_disjunct();
    std::unique_ptr<Expression> parse_additive_expr();
    std::unique_ptr<Expression> parse_object_literal();

    std::unique_ptr<Expression>
    parse_member_expr(std::unique_ptr<Expression> callee);
    std::unique_ptr<Expression>
    parse_call_expr(std::unique_ptr<Expression> callee);
    std::unique_ptr<Expression> parse_postfix_expr();

    struct ParsePrimaryExpression {
      ExecutionBoundary &boundary;
      std::unique_ptr<Expression> operator()(std::monostate);
      std::unique_ptr<Expression> operator()(char32_t punct);
      std::unique_ptr<Expression> operator()(std::int64_t number);
      std::unique_ptr<Expression> operator()(double number);
      std::unique_ptr<Expression> operator()(Operator op);
      std::unique_ptr<Expression> operator()(Keyword keyword);
      std::unique_ptr<Expression> operator()(Identifier identifier);
      std::unique_ptr<Expression> operator()(std::string_view ascii);
      std::unique_ptr<Expression> operator()(std::u16string_view unicode);
    };
  };

  class FunctionDefinition final : public ExecutionBoundary {
  public:
    FunctionDefinition(Compiler &c) : ExecutionBoundary{c} {}

    std::optional<Identifier> function_name;
    std::vector<Identifier> arguments;

    bool allows_return_stmt() const override { return 1; }
    void parse_function_decl();
  };

  class ModuleDefinition final : public ExecutionBoundary {
  public:
    ModuleDefinition(Compiler &c) : ExecutionBoundary{c} {}
    bool allows_return_stmt() const override { return 0; }
  };

  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;
  void parse_text();
  void analyze_program();
  void write_serial_program();

private:
  Machine &machine;

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

  ModuleDefinition entry_module{*this};
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

Compiler::ExecutionBoundary::ExecutionBoundary(Compiler &c) : compiler{c} {
  output_boundary = std::make_unique<Machine::ExecutionBoundary>();
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

std::list<std::uint64_t>
Compiler::InitializeScope::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{StatementKind{StmtInitialize{}}.index(),
                                  local_offset};
  output.splice(output.end(), rvalue->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::ExpressionStatement::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{StatementKind{StmtExpression{}}.index()};
  output.splice(output.end(), argument->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::ReturnStatement::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{StatementKind{StmtReturn{}}.index()};
  output.splice(output.end(), argument->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::DirectFunctionCall::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{
      ExpressionKind{ExprFunctionCall{}}.index(),
      reinterpret_cast<std::uintptr_t>(callee->output_boundary.get())};
  return output;
}

std::list<std::uint64_t>
Compiler::ConsoleLogCall::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{ExpressionKind{ExprConsole{}}.index(),
                                  arguments.size()};
  for (const auto &argument : arguments)
    output.splice(output.end(), argument->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::StringLength::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{ExpressionKind{ExprStringLength{}}.index()};
  output.splice(output.end(), argument->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::StringifyNumber::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{
      ExpressionKind{ExprStringifyNumber{}}.index()};
  output.splice(output.end(), argument->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::BinaryExpression::serialize(ExecutionBoundary &boundary) const {
  ExpressionKind expression_kind{};
  switch (op) {
  case '+':
    expression_kind.emplace<ExprBinary<BinaryOperation::OP_ADD>>();
    break;
  case '-':
    expression_kind.emplace<ExprBinary<BinaryOperation::OP_SUB>>();
    break;
  }
  std::list<std::uint64_t> output{expression_kind.index()};
  output.splice(output.end(), left->serialize(boundary));
  output.splice(output.end(), right->serialize(boundary));
  return output;
}

std::list<std::uint64_t>
Compiler::NumericLiteral::serialize(ExecutionBoundary &boundary) const {
  return {ExpressionKind{ExprLiteral{}}.index(),
          std::bit_cast<std::uint64_t>(val)};
}

std::list<std::uint64_t>
Compiler::AsciiLiteral::serialize(ExecutionBoundary &boundary) const {
  std::list<std::uint64_t> output{ExpressionKind{ExprLiteral{}}.index()};
  const MachineString *string_ptr{boundary.push_string_literal(val)};
  output.push_back(reinterpret_cast<std::uintptr_t>(string_ptr));
  return output;
}

std::list<std::uint64_t>
Compiler::UnicodeLiteral::serialize(ExecutionBoundary &boundary) const {
  return {};
}

std::list<std::uint64_t>
Compiler::ScopeAccessor::serialize(ExecutionBoundary &boundary) const {
  return {ExpressionKind{ExprScopeAccess{}}.index(), scope_offset,
          local_offset};
}

void Compiler::ExecutionBoundary::serialize() {
  for (auto &definition : inner_functions)
    definition.serialize();
  std::list<std::uint64_t> output{};
  for (const auto &statement : program)
    output.splice(output.end(), statement->serialize(*this));
  output_boundary->local_scope = local_scope;
  output_boundary->program = {std::from_range, output};
}

void Compiler::ExecutionBoundary::export_serial_program() {
  for (auto &definition : inner_functions)
    definition.export_serial_program();
  compiler.machine.boundaries.push_back(std::move(output_boundary));
}

void Compiler::write_serial_program() {
  machine.program_entry = entry_module.output_boundary.get();
  entry_module.serialize();
  entry_module.export_serial_program();
}

const MachineString *
Compiler::ExecutionBoundary::push_string_literal(std::string_view ascii) {
  auto [literal_it, _] =
      compiler.machine.string_literals.emplace(std::string{ascii});
  return &(*literal_it);
}

const MachineString *
Compiler::ExecutionBoundary::push_string_literal(std::u16string_view unicode) {
  auto [u16literal_it, _] =
      compiler.machine.string_literals.emplace(std::u16string{unicode});
  return &(*u16literal_it);
}

Machine::Machine() { runtime_memory = std::make_unique<RuntimeMemory>(); }

void Machine::EvaluateStatement::operator()(StmtInitialize) {
  std::size_t local_offset{
      static_cast<std::size_t>(*machine.current_frame->program_iter++)};
  machine.current_frame->own_scope[local_offset] =
      machine.evaluate_expression();
}

void Machine::EvaluateStatement::operator()(StmtExpression) {
  machine.evaluate_expression();
}

void Machine::EvaluateStatement::operator()(StmtReturn) {
  machine.current_frame->return_val = machine.evaluate_expression();
}

template <typename T> static T from_index(std::uint64_t alternative_idx) {
  template for (constexpr std::size_t I :
                std::views::iota(1u, std::meta::variant_size(^^T))) {
    if (I == alternative_idx)
      return T{std::in_place_index<I>};
  }
  return std::monostate{};
}

void Machine::evaluate_statement() {
  from_index<StatementKind>(*current_frame->program_iter++)
      .visit(EvaluateStatement{*this});
}

std::uint64_t Machine::EvaluateExpression::operator()(ExprLiteral) {
  return *machine.current_frame->program_iter++;
}

std::uint64_t Machine::EvaluateExpression::operator()(ExprScopeAccess) {
  std::size_t scope_offset{
      static_cast<std::size_t>(*machine.current_frame->program_iter++)};
  std::size_t local_offset{
      static_cast<std::size_t>(*machine.current_frame->program_iter++)};
  auto zero_offset = std::ranges::single_view{machine.current_frame};
  auto complete_closure =
      std::ranges::concat_view{zero_offset, machine.current_frame->closure};
  return complete_closure[scope_offset]->own_scope[local_offset];
}

template <BinaryOperation P>
std::uint64_t Machine::EvaluateExpression::operator()(ExprBinary<P>) {
  double lhs{std::bit_cast<double>(machine.evaluate_expression())};
  double rhs{std::bit_cast<double>(machine.evaluate_expression())};
  if constexpr (P == BinaryOperation::OP_ADD)
    return std::bit_cast<std::uint64_t>(lhs + rhs);
  if constexpr (P == BinaryOperation::OP_SUB)
    return std::bit_cast<std::uint64_t>(lhs - rhs);
}

std::uint64_t Machine::EvaluateExpression::operator()(ExprFunctionCall) {
  const ExecutionBoundary *boundary{reinterpret_cast<const ExecutionBoundary *>(
      *machine.current_frame->program_iter++)};
  return machine.evaluate_function_call(boundary);
}

std::uint64_t Machine::EvaluateExpression::operator()(ExprConsole) {
  std::size_t n_args{*machine.current_frame->program_iter++};
  std::vector<MachineString> message_parts(n_args);
  for (MachineString &message_part : message_parts) {
    std::uint64_t expr_value{machine.evaluate_expression()};
    message_part = *reinterpret_cast<const MachineString *>(expr_value);
  }
  {
    std::lock_guard console_lock{machine.console_mutex};
    machine.console_messages.emplace_back(std::move(message_parts));
  }
  machine.console_condition.notify_one();
  return 0;
}

std::uint64_t Machine::EvaluateExpression::operator()(ExprStringLength) {
  const MachineString *machine_string{
      reinterpret_cast<const MachineString *>(machine.evaluate_expression())};
  std::size_t string_length{
      machine_string->visit([](auto str) { return str.size(); })};
  return std::bit_cast<std::uint64_t>(static_cast<double>(string_length));
}

std::uint64_t Machine::EvaluateExpression::operator()(ExprStringifyNumber) {
  std::uint64_t expr_value{machine.evaluate_expression()};
  double number{std::bit_cast<double>(expr_value)};
  std::string buffer(24, '\0');
  auto [ptr, ec] =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
  assert(ec == std::errc{});
  buffer.resize(ptr - buffer.data());
  const MachineString *machine_string{
      &machine.runtime_memory->strings.emplace_back(std::move(buffer))};
  return reinterpret_cast<std::uintptr_t>(machine_string);
}

std::uint64_t Machine::evaluate_expression() {
  return from_index<ExpressionKind>(*current_frame->program_iter++)
      .visit(EvaluateExpression{*this});
}

std::uint64_t
Machine::evaluate_function_call(const ExecutionBoundary *boundary) {
  auto scope_trace = std::ranges::concat_view{
      std::ranges::single_view{current_frame}, current_frame->closure};
  current_frame = &runtime_memory->execution_frames.emplace_back();
  current_frame->boundary = boundary;
  current_frame->closure =
      runtime_memory->scope_traces.emplace_back(std::from_range, scope_trace);
  current_frame->program_iter = boundary->program.begin();
  current_frame->own_scope =
      runtime_memory->structures.emplace_back(boundary->local_scope.size());
  while (current_frame->program_iter != boundary->program.end())
    evaluate_statement();
  std::uint64_t return_val{current_frame->return_val};
  current_frame = current_frame->closure.front();
  return return_val;
}

void Machine::evaluate() {
  current_frame = &runtime_memory->execution_frames.emplace_back();
  current_frame->program_iter = program_entry->program.begin();
  current_frame->boundary = program_entry;
  current_frame->own_scope = runtime_memory->structures.emplace_back(
      program_entry->local_scope.size());
  while (current_frame->program_iter != program_entry->program.end())
    evaluate_statement();
}

std::list<ConsoleMessage>
Machine::collect_console_messages(std::stop_token stopper) {
  std::unique_lock console_lock{console_mutex};
  auto check_messages = [&] { return console_messages.size() > 0; };
  console_condition.wait(console_lock, stopper, check_messages);
  std::list<ConsoleMessage> message_box{};
  console_messages.swap(message_box);
  return message_box;
}

static std::inplace_vector<std::uint8_t, 3>
encode_u16_for_print(std::uint16_t uchar) {
  std::array<std::uint8_t, 3> buffer{};
  std::size_t buffer_len{buffer.size()};
  uint8_t *result = u16_to_u8(&uchar, 1, buffer.data(), &buffer_len);
  assert(result != nullptr);
  return {std::from_range, buffer | std::views::take(buffer_len)};
}

struct EncodeMachineString {
  std::string operator()(std::string_view ascii) { return std::string{ascii}; }
  std::string operator()(std::u16string_view unicode) {
    auto encoded_message = unicode |
                           std::views::transform(encode_u16_for_print) |
                           std::views::join;
    return std::string{std::from_range, encoded_message};
  }
};

std::string ConsoleMessage::encode_for_print() const {
  auto encode_message_part = [](const MachineString &machine_string) {
    return machine_string.visit(EncodeMachineString{});
  };
  auto encoded_parts = parts | std::views::transform(encode_message_part) |
                       std::views::join_with(' ');
  return {std::from_range, encoded_parts};
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

void Language::compile_and_execute() {
  Manadrain::Compiler compiler{*machine};
  compiler.text_buffer = std::move(text_buffer);
  compiler.parse_text();
  compiler.analyze_program();
  compiler.write_serial_program();

  machine->evaluate();

  auto console_printer = [&](std::stop_token stopper) {
    std::list<Manadrain::ConsoleMessage> messages{
        machine->collect_console_messages(stopper)};
    for (const Manadrain::ConsoleMessage &message : messages)
      std::println("{}", message.encode_for_print());
  };
  auto console_worker = [&](std::stop_token stopper) {
    do
      console_printer(stopper);
    while (not stopper.stop_requested());
  };
  std::jthread console_thread{console_worker};
}
} // namespace Manadrain
