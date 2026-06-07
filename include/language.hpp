#include <condition_variable>
#include <cstdint>
#include <flat_map>
#include <generator>
#include <inplace_vector>
#include <list>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <variant>
#include <vector>

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
struct Identifier {
  std::size_t offset;
  auto operator<=>(const Identifier &) const = default;
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
struct CompactString;
using Token =
    std::variant<std::monostate, char32_t, std::int64_t, double, Operator,
                 Keyword, Identifier, const CompactString *>;

struct InvalidNumericLiteral {};
struct InvalidPropertyName {};
struct InvalidBackslashEscape {};
struct InvalidExpression {};
struct InvalidVariableAccess {};
struct InvalidPropertyAccess {};
struct InvalidFunctionCall {};
struct DuplicateDeclaration {};
struct MissingFieldName {};
struct MissingVariableName {};
struct MissingFunctionName {};
struct MissingIdentifier {};
struct MissingStringLiteral {};
struct MissingFormalParameter {};
struct MissingPunctuation {
  char32_t must_be;
};
struct UnexpectedStringEnd {};
struct UnexpectedCommentEnd {};
struct UnexpectedToken {};
class ScriptError : public std::exception {
public:
  using Message = std::variant<
      InvalidNumericLiteral, InvalidPropertyName, InvalidBackslashEscape,
      InvalidExpression, InvalidFunctionCall, InvalidVariableAccess,
      InvalidPropertyAccess, DuplicateDeclaration, MissingFieldName,
      MissingVariableName, MissingFunctionName, MissingIdentifier,
      MissingStringLiteral, MissingFormalParameter, MissingPunctuation,
      UnexpectedStringEnd, UnexpectedCommentEnd, UnexpectedToken>;
  Message message;

  explicit ScriptError(Message msg) : message{msg} {}
  const char *what() const noexcept override { return "script error!"; }
};

struct ObjectShape;
struct FunctionDefinition;

enum class IntrinsicFunction { F_LOG };
enum class IntrinsicObject { O_CONSOLE };
struct NumberType {};
struct StringType {};
struct LambdaType {};
struct AnyType {};
using Datatype =
    std::variant<std::monostate, NumberType, StringType, LambdaType, AnyType,
                 const ObjectShape *, IntrinsicObject>;

struct FunctionFrame;
struct ObjectInstance;

struct CompactString {
  std::string ascii;
  std::u16string unicode;
};
union Value {
  double number;
  const CompactString *compact_string;
  FunctionFrame *function_frame;
  ObjectInstance *object_instance;
};

template <typename T> struct Interim {};
template <typename T> struct ScopeAccessor {
  std::size_t scope_offset;
  std::size_t local_offset;
};
template <typename T> struct ExpressionIR {
  std::size_t intermediate_idx;
};
template <typename T>
using ConcreteUnit =
    std::variant<T, ScopeAccessor<T>, Interim<T>, ExpressionIR<T>>;
using Unit =
    std::variant<std::monostate, std::int64_t, Identifier,
                 ConcreteUnit<const CompactString *>, ConcreteUnit<double>,
                 const FunctionDefinition *, ExpressionIR<std::monostate>,
                 IntrinsicObject, IntrinsicFunction>;

struct BinaryExpression {
  Unit left;
  Unit right;
  char32_t op;
};
struct StringConcat {
  ConcreteUnit<const CompactString *> left;
  ConcreteUnit<const CompactString *> right;
};
struct StringLength {
  ConcreteUnit<const CompactString *> argument;
};
struct Addition {
  ConcreteUnit<double> left;
  ConcreteUnit<double> right;
};
struct Subtraction {
  ConcreteUnit<double> left;
  ConcreteUnit<double> right;
};
struct LogicalExpression {
  Unit left;
  Unit right;
  Operator op;
};
struct MemberExpression {
  Unit object;
  Identifier property;
};
struct FunctionCallAST {
  Unit callee;
  std::vector<Unit> arguments;
};
struct FunctionCallIR {
  const FunctionDefinition *callee;
};
struct FunctionCall {
  const FunctionDefinition *callee;
  std::size_t inline_arguments;
};
struct AssignExpression {
  Unit left;
  Unit right;
};

struct ObjectShape {
  std::flat_map<Identifier, Datatype> properties;
};
struct ObjectExpressionIR {
  const ObjectShape *object_shape;
  std::vector<Unit> properties;
};
struct ObjectExpression {
  const ObjectShape *object_shape;
  std::size_t inline_properties;
};

template <typename T> struct Stringify {
  ConcreteUnit<T> argument;
};

struct InitializeVariable {
  Identifier variable_name;
  Unit rvalue;
};
struct InitializeMember {
  Identifier member_name;
  Unit rvalue;
};
template <typename T> struct InitializeScope {
  std::size_t local_offset;
  ConcreteUnit<T> rvalue;
};
struct ReturnStatementIR {
  Unit argument;
};
template <typename T> struct ReturnStatement {
  ConcreteUnit<T> argument;
};
struct ConsoleLogIR {
  std::vector<ConcreteUnit<const CompactString *>> arguments;
};
struct ConsoleLog {
  std::size_t inline_arguments;
};
using StatementIR =
    std::variant<Unit, BinaryExpression, StringConcat, StringLength, Addition,
                 Subtraction, LogicalExpression, MemberExpression,
                 FunctionCallAST, FunctionCallIR, ConsoleLogIR,
                 AssignExpression, ObjectExpressionIR, ObjectExpression,
                 InitializeVariable, InitializeScope<const CompactString *>,
                 InitializeScope<double>, InitializeMember, ReturnStatementIR,
                 Stringify<double>>;
using Statement =
    std::variant<Unit, StringConcat, StringLength, Addition, Subtraction,
                 ConsoleLog, InitializeScope<const CompactString *>,
                 InitializeScope<double>, ReturnStatement<double>,
                 const StatementIR *, Stringify<double>, FunctionCall>;

inline constexpr std::size_t stmt_align =
    std::max({alignof(Unit), alignof(StringConcat), alignof(StringLength),
              alignof(Addition), alignof(Subtraction), alignof(ConsoleLog),
              alignof(InitializeScope<const CompactString *>),
              alignof(InitializeScope<double>),
              alignof(ReturnStatement<double>), alignof(const StatementIR *),
              alignof(Stringify<double>), alignof(FunctionCall)});
inline constexpr std::size_t stmt_size =
    std::max({sizeof(Unit), sizeof(StringConcat), sizeof(StringLength),
              sizeof(Addition), sizeof(Subtraction), sizeof(ConsoleLog),
              sizeof(InitializeScope<const CompactString *>),
              sizeof(InitializeScope<double>), sizeof(ReturnStatement<double>),
              sizeof(const StatementIR *), sizeof(Stringify<double>),
              sizeof(FunctionCall)});

template <typename T> inline constexpr std::size_t stmt_type{0};
template <> inline constexpr std::size_t stmt_type<Unit>{1};
template <> inline constexpr std::size_t stmt_type<StringConcat>{2};
template <> inline constexpr std::size_t stmt_type<StringLength>{3};
template <> inline constexpr std::size_t stmt_type<Addition>{4};
template <> inline constexpr std::size_t stmt_type<Subtraction>{5};
template <> inline constexpr std::size_t stmt_type<ConsoleLog>{6};
template <>
inline constexpr std::size_t stmt_type<InitializeScope<const CompactString *>>{
    7};
template <> inline constexpr std::size_t stmt_type<InitializeScope<double>>{8};
template <> inline constexpr std::size_t stmt_type<ReturnStatement<double>>{9};
template <> inline constexpr std::size_t stmt_type<const StatementIR *>{10};
template <> inline constexpr std::size_t stmt_type<Stringify<double>>{11};
template <> inline constexpr std::size_t stmt_type<FunctionCall>{12};

struct RawStatement {
  std::size_t type_idx;
  alignas(stmt_align) std::byte buffer[stmt_size];

  RawStatement(const RawStatement &other) = delete;
  RawStatement &operator=(const RawStatement &other) = delete;
  RawStatement(RawStatement &&other) noexcept = delete;
  RawStatement &operator=(RawStatement &&other) noexcept = delete;

  template <typename T> RawStatement(T stmt_alt);
  ~RawStatement();

  template <typename T, typename Self, typename F>
  auto &&dispatch(this Self &&self, F &&functor) {
    using PerfectT =
        std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                           std::add_const_t<T>, T>;
    auto *laundered_ptr =
        std::launder(reinterpret_cast<PerfectT *>(self.buffer));
    return std::forward<F>(functor)(std::forward_like<Self>(*laundered_ptr));
  }

  template <typename Self, typename F>
  auto &&visit(this Self &&self, F &&functor) {
    switch (self.tag) {
    case 1:
      return std::forward<Self>(self).template dispatch<int>(
          std::forward<F>(functor));
    case 2:
      return std::forward<Self>(self).template dispatch<double>(
          std::forward<F>(functor));
    default:
      std::unreachable();
    }
  }
};

struct FunctionDefinition {
  Datatype return_type;
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  std::flat_map<Identifier, Datatype> local_scope;
  std::vector<const FunctionDefinition *> nested_functions;
  std::vector<std::unique_ptr<StatementIR>> intermediate;
  std::vector<Statement> program;

  void replicate(const FunctionDefinition &model);
  Unit analyze_identifier(std::size_t scope_offset,
                          Identifier identifier) const;
};

struct FunctionFrame {
  const FunctionDefinition *definition;
  std::vector<Statement>::const_iterator program_iter;
  std::span<Value> own_scope;
  std::span<FunctionFrame *> closure;
  Value return_val;
};

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

struct ConsoleMessage {
  std::u16string content;
  std::string encode_for_print() const;
};

struct RuntimeMemory {
  std::pmr::monotonic_buffer_resource resource{65536};
  std::pmr::list<FunctionFrame> function_frames{&resource};
  std::pmr::list<std::pmr::vector<FunctionFrame *>> scope_traces{&resource};
  std::pmr::list<std::pmr::vector<Value>> structures{&resource};
};

class Parser;
class Analyzer;

class Machine {
public:
  void collect_console_messages(std::stop_token stopper,
                                std::list<ConsoleMessage> &message_box);
  void evaluate();

private:
  FunctionFrame *current_frame;
  std::shared_ptr<const FunctionDefinition> main_function;
  std::vector<std::shared_ptr<const FunctionDefinition>> function_definitions;
  std::vector<std::shared_ptr<const ObjectShape>> object_shapes;
  std::vector<std::shared_ptr<const CompactString>> permanent_strings;

  std::indirect<RuntimeMemory> runtime_memory;
  std::indirect<std::mutex> console_mutex;
  std::indirect<std::condition_variable_any> console_condition;
  std::list<ConsoleMessage> console_messages;

  struct EvaluateStatement {
    Machine &machine;
    void operator()(ConsoleLog statement);
    template <typename T> void operator()(T statement) { std::unreachable(); }
  };

  std::u16string stringify(std::monostate);
  std::u16string stringify(std::u16string_view permanent_string);
  std::u16string stringify(std::int64_t number);
  std::u16string stringify(double number);
  std::u16string stringify(ObjectInstance *object_instance);
  std::u16string stringify(FunctionFrame *function_frame);
  std::u16string stringify(IntrinsicFunction intrinsic_function);

  friend class Parser;
  friend class Analyzer;
};

class Parser {
public:
  Parser(Machine &m) : machine{m} {}
  std::shared_ptr<const std::vector<std::uint8_t>> text_buffer;
  void parse_text();

private:
  Machine &machine;

  std::vector<std::shared_ptr<const std::string>> atom_pool;
  std::unordered_map<std::string_view, Identifier> atom_atlas;
  std::unordered_map<std::u16string, const CompactString *> string_atlas;

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

  std::unique_ptr<FunctionDefinition> curr_definition;

  Unit parse_object_literal();
  Unit parse_primary_expr();
  Unit parse_member_expr(Unit object);
  Unit parse_call_expr(Unit callee);
  Unit parse_postfix_expr();
  Unit parse_additive_expr();
  Unit parse_logical_disjunct();
  Unit parse_assign_expr();
  Unit parse_expression();

  void parse_statement();
  const FunctionDefinition *parse_function_decl();
  void parse_function_stmt();
  void parse_variable_decl();
};

struct AnalyzerFrame {
  const FunctionDefinition *model;
  std::shared_ptr<FunctionDefinition> output;
};

class Analyzer {
public:
  Analyzer(Machine &m) : machine{m} {}
  void analyze();

protected:
  Machine &machine;
  std::flat_map<const FunctionDefinition *, const FunctionDefinition *>
      correspondence;
  std::vector<std::shared_ptr<const FunctionDefinition>> definitions;
  std::vector<AnalyzerFrame> frames;

private:
  void analyze_definition();
  virtual void analyze_statement(Statement statement) = 0;
};

class Typechecker final : public Analyzer {
public:
  Typechecker(Machine &m) : Analyzer{m} {}

private:
  void analyze_statement(Statement statement) override;

  struct AnalyzeStatement {
    Typechecker &checker;
    void operator()(Unit unit);
    void operator()(const StatementIR *statement_ir);
    void operator()(InitializeVariable statement);
    void operator()(ReturnStatementIR statement);
    template <typename T> void operator()(T statement) { std::unreachable(); }
  };

  struct AnalyzeInitializer {
    Typechecker &checker;
    std::size_t local_offset;
    Datatype operator()(ConcreteUnit<const CompactString *> unit_alt);
    Datatype operator()(ExpressionIR<std::monostate> unit_alt);
    Datatype operator()(ConcreteUnit<double> unit_alt);
    template <typename T> Datatype operator()(T unit_alt) {
      std::unreachable();
    }
  };

  struct AnalyzeReturnStatement {
    Typechecker &checker;
    Datatype operator()(ConcreteUnit<double> unit_alt);
    template <typename T> Datatype operator()(T unit_alt) {
      std::unreachable();
    }
  };

  struct AnalyzeExpression {
    Typechecker &checker;
    Unit operator()(const BinaryExpression &expression);
    Unit operator()(const MemberExpression &expression);
    Unit operator()(const FunctionCallAST &expression);
    template <typename T> Unit operator()(const T &expression) {
      std::unreachable();
    }
  };

  struct AnalyzeUnit {
    Typechecker &checker;
    Unit operator()(std::int64_t number);
    Unit operator()(Identifier identifier);
    Unit operator()(ExpressionIR<std::monostate> expression_ir);
    template <typename T> Unit operator()(T unit_alt) { std::unreachable(); }
  };

  struct AnalyzeMemberAccess {
    Typechecker &checker;
    Identifier identifier;
    Unit operator()(ConcreteUnit<const CompactString *> concrete_object);
    Unit operator()(IntrinsicObject concrete_object);
    template <typename T> Unit operator()(T concrete_object) {
      std::unreachable();
    }
  };

  struct AnalyzeFunctionCall {
    Typechecker &checker;
    std::vector<Unit> arguments;
    Unit operator()(const FunctionDefinition *callee);
    Unit operator()(IntrinsicFunction callee);
    template <typename T> Unit operator()(T callee) { std::unreachable(); }
  };

  struct AnalyzeFunctionReturn {
    Typechecker &checker;
    FunctionCallIR function_call_ir;
    Unit operator()(NumberType);
    template <typename T> Unit operator()(T) { std::unreachable(); }
  };

  struct AnalyzeAddition {
    Typechecker &checker;
    Unit operator()(ConcreteUnit<double> concrete_left,
                    ConcreteUnit<double> concrete_right);
    template <typename T, typename U>
    Unit operator()(T concrete_left, U concrete_right) {
      std::unreachable();
    }
  };

  struct AnalyzeSubtraction {
    Typechecker &checker;
    Unit operator()(ConcreteUnit<double> concrete_left,
                    ConcreteUnit<double> concrete_right);
    template <typename T, typename U>
    Unit operator()(T concrete_left, U concrete_right) {
      std::unreachable();
    }
  };

  struct InjectStringify {
    Typechecker &checker;
    ConcreteUnit<const CompactString *>
    operator()(ConcreteUnit<double> number_unit);
    template <typename T>
    ConcreteUnit<const CompactString *> operator()(T unit_alt) {
      std::unreachable();
    }
  };
};

class Inliner final : public Analyzer {
public:
  Inliner(Machine &m) : Analyzer{m} {}

private:
  void analyze_statement(Statement statement) override;

  struct AnalyzeStatement {
    Inliner &inliner;
    template <typename T> void operator()(InitializeScope<T> statement);
    template <typename T> void operator()(ReturnStatement<T> statement);
    void operator()(Unit statement);
    void operator()(ExpressionIR<std::monostate> unit);
    template <typename T> void operator()(T statement) { std::unreachable(); }
  };

  template <typename T> struct AnalyzeUnit {
    Inliner &inliner;
    ConcreteUnit<T> operator()(T unit) { return unit; }
    ConcreteUnit<T> operator()(ScopeAccessor<T> unit) { return unit; }
    ConcreteUnit<T> operator()(ExpressionIR<T> unit);
    ConcreteUnit<T> operator()(Interim<T> unit) { std::unreachable(); }
  };

  struct AnalyzeExpression {
    Inliner &inliner;
    void operator()(const Addition &expression);
    void operator()(const Subtraction &expression);
    void operator()(const StringLength &expression);
    void operator()(const ConsoleLogIR &expression);
    void operator()(const FunctionCallIR &expression);
    template <typename T> void operator()(const Stringify<T> &expression);
    template <typename T> void operator()(const T &expression) {
      std::unreachable();
    }
  };
};
} // namespace Manadrain
