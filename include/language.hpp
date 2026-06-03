#include <condition_variable>
#include <cstdint>
#include <deque>
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
#include <unordered_set>
#include <variant>
#include <vector>

namespace Manadrain {
enum class Keyword {
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
using Token = std::variant<std::monostate, char32_t, std::int64_t, double,
                           Operator, Keyword, Identifier, std::u16string_view>;

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

enum class Primitive { T_ANY, T_NUMBER, T_STRING };
enum class IntrinsicFunction { F_LOG, F_LENGTH };
enum class IntrinsicObject { O_CONSOLE };
using Datatype = std::variant<std::monostate, Primitive, const ObjectShape *,
                              const FunctionDefinition *, IntrinsicFunction,
                              IntrinsicObject>;

struct PropertyDatatype {
  Identifier property;
  Datatype operator()(std::monostate object_type);
  Datatype operator()(Primitive object_type);
  Datatype operator()(const ObjectShape *object_type);
  Datatype operator()(const FunctionDefinition *object_type);
  Datatype operator()(IntrinsicFunction object_type);
  Datatype operator()(IntrinsicObject object_type);
};

struct Interim {};
struct ScopeAccess {
  std::size_t frame_offset;
  std::size_t scope_offset;
};
using Unit =
    std::variant<std::monostate, std::u16string_view, std::int64_t, double,
                 Interim, Identifier, ScopeAccess, const FunctionDefinition *>;

class Parser;

struct AnalyzeUnit {
  Parser &parser;
  Datatype operator()(std::monostate unit);
  Datatype operator()(std::u16string_view unit);
  Datatype operator()(std::int64_t unit);
  Datatype operator()(double unit);
  Datatype operator()(Interim);
  Datatype operator()(Identifier identifier);
  Datatype operator()(ScopeAccess scope_access);
  Datatype operator()(const FunctionDefinition *definition);
};

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
  std::size_t passed_arguments;
};
struct AssignExpression {
  Unit left;
  Unit right;
};

struct ObjectInstance;
struct ObjectShape {
  std::flat_map<Identifier, Datatype> properties;
};
struct ObjectExpression {
  const ObjectShape *object_shape;
  std::size_t passed_properties;
};

using Expression =
    std::variant<Unit, BinaryExpression, LogicalExpression, MemberExpression,
                 FunctionCallExpression, AssignExpression, ObjectExpression>;

struct AnalyzeExpression {
  Parser &parser;
  Datatype operator()(Unit unit);
  Datatype operator()(BinaryExpression expression);
  Datatype operator()(LogicalExpression expression);
  Datatype operator()(MemberExpression expression);
  Datatype operator()(FunctionCallExpression expression);
  Datatype operator()(AssignExpression expression);
  Datatype operator()(ObjectExpression expression);
};

struct AnalyzeFunctionCallee {
  Parser &parser;
  std::pair<Datatype, Datatype> operator()(Unit unit);
  std::pair<Datatype, Datatype> operator()(Interim);
  std::pair<Datatype, Datatype> operator()(Identifier identifier);
  std::pair<Datatype, Datatype> operator()(MemberExpression expression);
  template <typename T> std::pair<Datatype, Datatype> operator()(T expression) {
    return {Primitive::T_ANY, Primitive::T_ANY};
  }
};

struct FunctionFrame;

using CompactString = std::variant<std::string, std::u16string>;
union Value {
  double number;
  const CompactString *compact_string;
  FunctionFrame *function_frame;
  ObjectInstance *object_instance;
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
  ScopeAccess accessor;
  T rvalue;
};
struct ReturnStatement {
  Unit argument;
};
using Statement =
    std::variant<Expression, InitializeVariable,
                 InitializeScope<const CompactString *>,
                 InitializeScope<double>, InitializeMember, ReturnStatement>;

struct FunctionDefinition {
  std::size_t definition_idx;
  Datatype return_type;
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  std::flat_map<Identifier, Datatype> local_scope;
  std::flat_map<Identifier, const FunctionDefinition *> nested_functions;
  std::list<Statement> parsed_program;
  std::vector<Statement> analyzed_program;
  std::optional<Datatype> analyze_variable(Identifier identifier);
};

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

struct ConsoleMessage {
  std::u16string content;
  std::string encode_for_print() const;
};

class Script {
public:
  Script();

  void collect_console_messages(std::stop_token stopper,
                                std::list<ConsoleMessage> &message_box);
  void evaluate();

protected:
  FunctionFrame *current_frame;
  std::vector<std::unique_ptr<FunctionDefinition>> function_definitions;
  std::vector<std::shared_ptr<const ObjectShape>> object_shapes;
  std::vector<std::shared_ptr<const CompactString>> permanent_strings;

  std::unique_ptr<std::pmr::monotonic_buffer_resource> resource;

  void initialize_descriptors(FunctionFrame &function_frame);

private:
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
};

struct AnalyzeStatement {
  Parser &parser;
  void operator()(Expression statement);
  void operator()(InitializeVariable statement);
  void operator()(InitializeScope<const CompactString *> statement);
  void operator()(InitializeScope<double> statement);
  void operator()(InitializeMember statement);
  void operator()(ReturnStatement statement);
};

class Parser : public Script {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;
  void parse_text();
  void analyze_program();

private:
  std::vector<std::unique_ptr<const std::string>> atom_pool;
  std::unordered_map<std::string_view, Identifier> atom_atlas;
  std::unordered_set<std::u16string_view> string_atlas;

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

  std::vector<FunctionDefinition *> function_trace;
  FunctionDefinition *current_function;
  std::list<Statement>::iterator program_it;
  std::vector<bool> scope_trace;

  Unit parse_object_literal();
  Unit parse_primary_expr();
  Expression parse_member_expr(Unit object);
  Expression parse_call_expr(Unit callee);
  Unit parse_postfix_expr();
  Unit parse_additive_expr();
  Unit parse_logical_disjunct();
  Unit parse_assign_expr();
  Unit parse_expression();

  void parse_statement();
  const FunctionDefinition *parse_function_decl();
  void parse_variable_decl();

  friend struct AnalyzeUnit;
  friend struct AnalyzeExpression;
  friend struct AnalyzeStatement;
  friend struct AnalyzeFunctionCallee;
};
} // namespace Manadrain
