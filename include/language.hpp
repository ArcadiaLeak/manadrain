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
enum class ReservedWord {
  MONOSTATE,
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
  W_SWITCH
};
struct Identifier {
  std::size_t pool_idx;
};
struct StringHandle {
  std::size_t pool_idx;
};
using NumericLiteral = std::variant<std::int64_t, double>;
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
using Token = std::variant<std::monostate, char32_t, Operator, NumericLiteral,
                           ReservedWord, Identifier, StringHandle>;

struct InvalidNumericLiteral {};
struct InvalidPropertyName {};
struct InvalidBackslashEscape {};
struct InvalidDeclaration {};
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
                   InvalidBackslashEscape, InvalidDeclaration, MissingFieldName,
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
using Expression = std::variant<std::monostate, StringHandle, NumericLiteral,
                                Identifier, ExpressionHandle>;

struct BinaryExpression {
  Expression left;
  Expression right;
  char32_t op;
};
struct MemberExpression {
  Expression object;
  std::size_t property;
};
struct FunctionCallExpression {
  Expression callee;
  std::vector<Expression> arguments;
};
struct MethodCallExpression {
  Expression object;
  std::size_t property;
  std::vector<Expression> arguments;
};
using ExpressionNode =
    std::variant<BinaryExpression, MemberExpression, FunctionCallExpression,
                 MethodCallExpression>;

struct FunctionHandle {
  std::size_t pool_idx;
};
struct ObjectHandle {
  std::optional<std::size_t> pool_idx;
};
using Dynamic =
    std::variant<std::monostate, StringHandle, FunctionHandle, ObjectHandle>;

struct FunctionDeclaration {
  std::size_t function_name;
  FunctionHandle function_handle;
};
struct VariableDeclaration {
  std::size_t variable_name;
  Expression initializer;
};
struct ReturnStatement {
  Expression return_expr;
};
using Statement =
    std::variant<Expression, VariableDeclaration, ReturnStatement>;

using PlainObject = std::flat_map<std::size_t, Manadrain::Dynamic>;

class Script;
using AbstractFunction = std::copyable_function<Dynamic(
    std::vector<Dynamic> arguments, Dynamic context, const Script &script)>;

using FunctionOwnScope = std::flat_map<std::size_t, std::optional<Dynamic>>;
class VanillaFunction {
public:
  std::size_t function_name;
  const FunctionOwnScope scope_blueprint;
  std::vector<Statement> function_body;
  Dynamic operator()(std::vector<Dynamic> arguments, Dynamic context,
                     const Script &script);
};

class Script {
public:
  std::vector<ExpressionNode> expr_pool;
  std::unordered_map<std::string, std::size_t> atom_atlas;
  std::vector<std::string> atom_pool;

  VanillaFunction main_function;
  std::vector<AbstractFunction> function_pool;
  std::vector<PlainObject> object_pool;

  ObjectHandle insert(PlainObject object);
  FunctionHandle insert(AbstractFunction function);
  std::size_t attach_atom(std::string atom_str);
  void execute();

private:
  Dynamic reduce(MethodCallExpression &expr_call);
  Dynamic reduce(FunctionCallExpression &expr_call);
  Dynamic reduce(Expression expression);

  void execute(Statement statement);
};

class Parser {
public:
  Script script;
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

  Token tokenize_word(char32_t leading);
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

  void parse_statement(
      std::flat_map<std::size_t, std::optional<Dynamic>> &block_scope,
      std::vector<Statement> &body, Token leading);
  FunctionDeclaration parse_function_decl();
  VariableDeclaration parse_variable_decl();
};
} // namespace Manadrain
