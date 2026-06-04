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
struct DuplicateDeclaration {};
struct InvalidVariableAccess {};
struct InvalidFunctionCall {};
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
  using Message =
      std::variant<InvalidNumericLiteral, InvalidPropertyName,
                   InvalidBackslashEscape, DuplicateDeclaration,
                   InvalidFunctionCall, InvalidVariableAccess, MissingFieldName,
                   MissingVariableName, MissingFunctionName, MissingIdentifier,
                   MissingStringLiteral, MissingFormalParameter,
                   MissingPunctuation, UnexpectedStringEnd,
                   UnexpectedCommentEnd, UnexpectedToken>;
  Message message;

  explicit ScriptError(Message msg) : message{msg} {}
  const char *what() const noexcept override { return "script error!"; }
};

struct ObjectShape;
struct FunctionDefinition;

enum class IntrinsicFunction { F_LOG, F_LENGTH };
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
  bool is_unicode;
};
union Value {
  double number;
  const CompactString *compact_string;
  FunctionFrame *function_frame;
  ObjectInstance *object_instance;
};

struct Interim {};
template <typename T> struct ScopeAccess {
  std::size_t frame_offset;
  std::size_t scope_offset;
};
struct StatementIR;
using Unit =
    std::variant<std::monostate, const CompactString *, std::int64_t, double,
                 Interim, Identifier, ScopeAccess<const CompactString *>,
                 ScopeAccess<double>, const FunctionDefinition *,
                 const StatementIR *>;

struct BinaryExpression {
  Unit left;
  Unit right;
  char32_t op;
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
struct FunctionCallExpression {
  Unit callee;
  std::vector<Unit> arguments;
  std::size_t inline_arguments;
};
struct AssignExpression {
  Unit left;
  Unit right;
};

struct ObjectShape {
  std::flat_map<Identifier, Datatype> properties;
};
struct ObjectExpression {
  const ObjectShape *object_shape;
  std::vector<Unit> properties;
  std::size_t inline_properties;
};

using Expression =
    std::variant<Unit, BinaryExpression, LogicalExpression, MemberExpression,
                 FunctionCallExpression, AssignExpression, ObjectExpression>;

struct InitializeVariable {
  Identifier variable_name;
  Unit rvalue;
};
struct InitializeMember {
  Identifier member_name;
  Unit rvalue;
};
template <typename T> struct InitializeScope {
  ScopeAccess<T> accessor;
  T rvalue;
};
struct ReturnStatement {
  Unit argument;
};
using Statement =
    std::variant<Expression, InitializeVariable,
                 InitializeScope<const CompactString *>,
                 InitializeScope<double>, InitializeMember, ReturnStatement>;
struct StatementIR {
  Statement nested;
};

struct FunctionDefinition {
  Datatype return_type;
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  std::flat_map<Identifier, Datatype> local_scope;
  std::vector<const FunctionDefinition *> nested_functions;
  std::list<StatementIR> intermediate;
  std::vector<Statement> program;
};

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

struct ConsoleMessage {
  std::u16string content;
  std::string encode_for_print() const;
};

class Parser;
class Typechecker;

class Machine {
public:
  Machine();

  void collect_console_messages(std::stop_token stopper,
                                std::list<ConsoleMessage> &message_box);
  void evaluate();

private:
  FunctionFrame *current_frame;
  std::shared_ptr<const FunctionDefinition> main_function;
  std::vector<std::shared_ptr<const FunctionDefinition>> function_defs;
  std::vector<std::shared_ptr<const ObjectShape>> object_shapes;
  std::vector<std::shared_ptr<const CompactString>> permanent_strings;

  std::unique_ptr<std::pmr::monotonic_buffer_resource> resource;

  void initialize_descriptors(FunctionFrame &function_frame);

  std::indirect<std::mutex> console_mutex;
  std::indirect<std::condition_variable_any> console_condition;
  std::list<ConsoleMessage> console_messages;

  std::u16string stringify(std::monostate);
  std::u16string stringify(std::u16string_view permanent_string);
  std::u16string stringify(std::int64_t number);
  std::u16string stringify(double number);
  std::u16string stringify(ObjectInstance *object_instance);
  std::u16string stringify(FunctionFrame *function_frame);
  std::u16string stringify(IntrinsicFunction intrinsic_function);

  friend class Parser;
  friend class Typechecker;
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

struct AnalyzedDefinition {
  const FunctionDefinition *model;
  std::unique_ptr<FunctionDefinition> replica;
};

class Typechecker {
public:
  Typechecker(Machine &m) : machine{m} {}
  void typecheck();

private:
  Machine &machine;
  std::vector<std::shared_ptr<const FunctionDefinition>> input_defs;

  AnalyzedDefinition analyzed_definition;
  std::vector<AnalyzedDefinition> closure_trace;

  void analyze_definition();

  void analyze_statement(Expression expression);
  void analyze_statement(InitializeVariable statement);
  void analyze_statement(ReturnStatement statement);
};
} // namespace Manadrain
