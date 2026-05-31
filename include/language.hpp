#include <condition_variable>
#include <cstdint>
#include <deque>
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

struct FunctionDefinition;

struct Interim {};
struct ScopeAccess {
  std::size_t frame_offset;
  std::size_t scope_offset;
};
using Unit =
    std::variant<std::monostate, std::u16string_view, std::int64_t, double,
                 Interim, Identifier, ScopeAccess, const FunctionDefinition *>;

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
  std::vector<Identifier> properties;
};
struct ObjectExpression {
  const ObjectShape *object_shape;
  std::size_t passed_properties;
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
struct InitializeScope {
  ScopeAccess accessor;
  Unit rvalue;
};
struct ReturnStatement {
  Unit argument;
};
using Statement = std::variant<Expression, InitializeVariable, InitializeMember,
                               InitializeScope, ReturnStatement>;

enum class IntrinsicFunction { F_LOG };

struct FunctionDefinition {
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  std::vector<Identifier> local_scope;
  std::vector<std::pair<Identifier, const FunctionDefinition *>>
      nested_functions;
  std::vector<Statement> program;
};

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

class Analyzer;

struct AnalyzeUnit {
  Analyzer &analyzer;
  Unit operator()(Interim);
  Unit operator()(Identifier);
  Unit operator()(ScopeAccess);
  template <typename T> Unit operator()(T arg) { return arg; }
};

struct AnalyzeExpression {
  Analyzer &analyzer;
  void operator()(Unit);
  void operator()(BinaryExpression);
  void operator()(LogicalExpression);
  void operator()(MemberExpression);
  void operator()(FunctionCallExpression);
  void operator()(AssignExpression);
  void operator()(ObjectExpression);
};

struct AnalyzeStatement {
  Analyzer &analyzer;
  void operator()(Expression);
  void operator()(InitializeVariable);
  void operator()(InitializeMember);
  void operator()(InitializeScope);
  void operator()(ReturnStatement);
};

class Analyzer {
public:
  void analyze_program();

protected:
  std::vector<std::shared_ptr<const FunctionDefinition>> function_definitions;
  std::vector<std::shared_ptr<const ObjectShape>> object_shapes;
  std::vector<std::shared_ptr<const std::u16string>> permanent_strings;

private:
  friend struct AnalyzeStatement;
};

class Parser : public Analyzer {
public:
  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;
  void parse_text();

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

  FunctionDefinition *current_function;
  std::size_t program_count;
  std::vector<bool> scope_trace;
  void analyze_scope_access();

  Unit parse_object_literal();
  Unit parse_primary_expr();
  Unit parse_member_expr(std::size_t base_idx, Unit object);
  Unit parse_call_expr(std::size_t base_idx, Unit callee);
  Unit parse_postfix_expr(std::size_t base_idx, Unit base_unit);
  Unit parse_postfix_expr();
  Unit parse_additive_expr();
  Unit parse_logical_disjunct();
  Unit parse_assign_expr();
  Unit parse_expression();

  void parse_statement();
  const FunctionDefinition *parse_function_decl();
  void parse_variable_decl();
};
} // namespace Manadrain
