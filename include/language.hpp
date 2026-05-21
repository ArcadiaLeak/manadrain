#include <cstdint>
#include <flat_map>
#include <generator>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
enum IntrinsicWord : std::ptrdiff_t {
  W_CONST,
  W_LET,
  W_VAR,
  W_CLASS,
  W_FUNCTION,
  W_RETURN,
  W_IMPORT,
  W_EXPORT,
  W_FROM,
  W_AS,
  W_DEFAULT,
  W_UNDEFINED,
  W_NULL,
  W_TRUE,
  W_FALSE,
  W_IF,
  W_ELSE,
  W_WHILE,
  W_FOR,
  W_DO,
  W_BREAK,
  W_CONTINUE,
  W_SWITCH,
  W_CONSOLE,
  W_LOG
};
inline constexpr std::ptrdiff_t reserved_count = W_SWITCH + 1;
struct ReservedWord {
  std::ptrdiff_t handle;
};
struct Identifier {
  std::ptrdiff_t handle;
};
struct StringHandle {
  std::ptrdiff_t handle;
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
                           Operator, ReservedWord, Identifier, StringHandle>;

struct InvalidNumericLiteral {};
struct InvalidPropertyName {};
struct InvalidBackslashEscape {};
struct InvalidDeclaration {};
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
struct UnexpectedReservedWord {};
struct UnexpectedStringEnd {};
struct UnexpectedCommentEnd {};
struct UnexpectedToken {};
class ScriptError : public std::exception {
public:
  using Message =
      std::variant<InvalidNumericLiteral, InvalidPropertyName,
                   InvalidBackslashEscape, InvalidDeclaration,
                   InvalidFunctionCall, InvalidVariableAccess, MissingFieldName,
                   MissingVariableName, MissingFunctionName, MissingIdentifier,
                   MissingStringLiteral, MissingFormalParameter,
                   MissingPunctuation, UnexpectedReservedWord,
                   UnexpectedStringEnd, UnexpectedCommentEnd, UnexpectedToken>;
  Message message;

  explicit ScriptError(Message msg) : message{msg} {}
  const char *what() const noexcept override { return "script error!"; }
};

struct ExpressionHandle {
  std::size_t pool_idx;
};
using Expression = std::variant<std::monostate, StringHandle, std::int64_t,
                                double, Identifier, ExpressionHandle>;

struct BinaryExpression {
  Expression left;
  Expression right;
  char32_t op;
};
struct MemberExpression {
  Expression object;
  std::ptrdiff_t property;
};
struct FunctionCallExpression {
  Expression callee;
  std::vector<Expression> arguments;
};
using ExpressionNode =
    std::variant<BinaryExpression, MemberExpression, FunctionCallExpression>;

struct VariableDeclaration {
  std::ptrdiff_t variable_name;
  Expression initializer;
};
struct ReturnStatement {
  Expression argument;
};
using Statement =
    std::variant<Expression, VariableDeclaration, ReturnStatement>;

enum IntrinsicHandle : std::ptrdiff_t { H_CONSOLE, H_GLOBAL, H_LOG, H_MAIN };

struct Dynamic;
struct ObjectHandle {
  std::ptrdiff_t handle;
  bool null_reference;
};
struct FunctionHandle {
  std::ptrdiff_t handle;
  std::optional<std::indirect<Dynamic>> context;
};
struct Dynamic {
  std::variant<std::monostate, StringHandle, std::int64_t, double, ObjectHandle,
               FunctionHandle>
      val;
};

struct VanillaObject {
  std::ptrdiff_t shape_handle;
  std::vector<Dynamic> properties;
};
using ObjectShape = std::vector<std::ptrdiff_t>;

struct FunctionBlueprint {
  std::ptrdiff_t function_name;
  ObjectShape scope_shape;
  std::vector<std::pair<std::ptrdiff_t, std::size_t>> nested_blueprint;
  std::vector<Statement> body;
};
struct VanillaFunction {
  std::size_t blueprint_handle;
  std::optional<std::size_t> parent_handle;
  std::vector<std::optional<Dynamic>> own_scope;
  Dynamic return_val;
};

class Script {
public:
  Script();
  void evaluate();

protected:
  std::vector<ExpressionNode> expr_pool;
  std::unordered_map<std::string, std::ptrdiff_t> atom_atlas;
  std::vector<std::string> atom_pool;

  VanillaFunction main_function;
  std::vector<FunctionBlueprint> blueprint_pool;
  std::vector<VanillaFunction> function_pool;

  std::vector<ObjectShape> shape_pool;
  std::vector<VanillaObject> object_pool;

  FunctionHandle bootstrap(std::size_t blueprint_handle,
                           std::optional<std::size_t> parent_scope);

private:
  VanillaObject console;
  VanillaObject global_this;

  std::generator<std::size_t>
  traverse_function_closure(std::size_t function_handle);
  std::optional<Dynamic> *get_variable(std::size_t function_handle,
                                       std::ptrdiff_t var_handle);
  Dynamic *get_property(std::ptrdiff_t object_handle,
                        std::ptrdiff_t property_handle);

  Dynamic evaluate(std::size_t function_handle, BinaryExpression &expression);
  Dynamic evaluate(std::size_t function_handle, MemberExpression &expression);
  Dynamic evaluate(std::size_t function_handle,
                   FunctionCallExpression &expression);
  Dynamic evaluate(std::size_t function_handle, Expression expression);
  Dynamic evaluate(std::size_t function_handle, Identifier identifier);
  Dynamic evaluate(std::size_t function_handle, ExpressionHandle expr_handle);
  Dynamic evaluate(std::size_t function_handle, StringHandle string_handle);
  Dynamic evaluate(std::size_t function_handle, std::int64_t number);
  Dynamic evaluate(std::size_t function_handle, double number);
  Dynamic evaluate(std::size_t function_handle, std::monostate) { return {}; }

  void evaluate(std::size_t function_handle, VariableDeclaration declaration);
  void evaluate(std::size_t function_handle, ReturnStatement statement);
  void evaluate(std::size_t function_handle, Statement statement);
};

class Parser : public Script {
public:
  std::shared_ptr<const std::uint8_t[]> text_buffer;
  std::size_t text_size;
  void parse_text();

private:
  std::size_t position;
  std::vector<std::int32_t> backtrace;

  std::list<Token> token_history;
  std::list<Token> token_revoked;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  std::optional<Token> revoked_pull();
  void history_pull();
  void history_push(Token token);

  Token tokenize_identifier(char32_t leading);
  Token tokenize_string_literal(char32_t separator);
  Token tokenize_numeric_literal(char32_t leading);
  std::generator<Token> traverse_tokens();
  Token tokenize();

  Expression parse_primary_expr();
  Expression parse_postfix_expr();
  Expression parse_additive_expr();
  Expression parse_member_expr(Expression obj_expr);
  Expression parse_call_expr(Expression callee_expr);
  Expression parse_expression();

  void parse_statement(FunctionBlueprint &blueprint, Token leading);
  std::size_t parse_function_decl();
  VariableDeclaration parse_variable_decl();
};
} // namespace Manadrain
