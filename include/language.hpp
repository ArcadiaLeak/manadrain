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
enum class AnchoredWord { MONOSTATE, W_CONSOLE, W_LOG };
struct AtomizedWord {
  std::size_t pool_idx;
  auto operator<=>(const AtomizedWord &) const = default;
};
using AbstractWord = std::variant<ReservedWord, AnchoredWord, AtomizedWord>;
struct Identifier {
  AbstractWord handle;
};
struct StringHandle {
  AbstractWord handle;
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
  AbstractWord property;
};
struct FunctionCallExpression {
  Expression callee;
  std::vector<Expression> arguments;
};
struct MethodCallExpression {
  Expression object;
  AbstractWord property;
  std::vector<Expression> arguments;
};
using ExpressionNode =
    std::variant<BinaryExpression, MemberExpression, FunctionCallExpression,
                 MethodCallExpression>;

struct VariableDeclaration {
  AbstractWord variable_name;
  Expression initializer;
};
struct ReturnStatement {
  Expression argument;
};
using Statement =
    std::variant<Expression, VariableDeclaration, ReturnStatement>;

struct ConsoleHandle {};
using IntrinsicHandle = std::variant<ConsoleHandle>;

struct ObjectHandle {
  std::size_t pool_idx;
  bool null_reference;
};
struct FunctionHandle {
  std::size_t pool_idx;
};
using Dynamic = std::variant<std::monostate, StringHandle, IntrinsicHandle,
                             ObjectHandle, FunctionHandle>;

using ObjectShape = std::vector<AbstractWord>;
using VanillaObject = std::vector<Dynamic>;

struct FunctionBlueprint {
  AbstractWord function_name;
  ObjectShape scope_shape;
  std::vector<std::pair<AbstractWord, std::size_t>> nested_blueprint;
  std::vector<Statement> body;
};
using FunctionScope = std::vector<std::optional<Dynamic>>;
struct VanillaFunction {
  std::size_t blueprint_handle;
  std::optional<std::size_t> parent_handle;
  FunctionScope own_scope;
  Dynamic return_val;
};

struct ShapeTrie {
  std::flat_map<AbstractWord,
                std::variant<std::size_t, std::indirect<ShapeTrie>>>
      children;
};

class Script {
public:
  void execute();

protected:
  std::vector<ExpressionNode> expr_pool;
  std::unordered_map<std::string, std::size_t> atom_atlas;
  std::vector<std::string> atom_pool;

  FunctionHandle main_function;
  std::vector<FunctionBlueprint> blueprint_pool;
  std::vector<VanillaFunction> function_pool;

  ShapeTrie shape_trie;
  std::vector<ObjectShape> shape_pool;
  std::vector<VanillaObject> object_pool;

  FunctionHandle bootstrap(std::size_t blueprint_handle,
                           std::optional<std::size_t> parent_scope);

private:
  std::generator<std::size_t>
  traverse_function_closure(std::size_t function_handle);

  Dynamic global_get(AbstractWord word);
  Dynamic instrinsic_call(ConsoleHandle, AbstractWord property,
                          std::vector<Dynamic> arguments);

  Dynamic exec_reduce(std::size_t function_handle,
                      BinaryExpression &expression);
  Dynamic exec_reduce(std::size_t function_handle,
                      MemberExpression &expression);
  Dynamic exec_reduce(std::size_t function_handle,
                      MethodCallExpression &expression);
  Dynamic exec_reduce(std::size_t function_handle,
                      FunctionCallExpression &expression);
  Dynamic exec_reduce(std::size_t function_handle, Expression expression);
  Dynamic exec_reduce(std::size_t function_handle, Identifier identifier);
  Dynamic exec_reduce(std::size_t function_handle,
                      ExpressionHandle expr_handle);
  Dynamic exec_reduce(std::size_t function_handle, StringHandle string_handle);
  Dynamic exec_reduce(std::size_t function_handle, std::int64_t number);
  Dynamic exec_reduce(std::size_t function_handle, double number);
  Dynamic exec_reduce(std::size_t function_handle, std::monostate) {
    return {};
  }

  void exec_reduce(std::size_t function_handle,
                   VariableDeclaration declaration);
  void exec_reduce(std::size_t function_handle, ReturnStatement statement);
  void exec_reduce(std::size_t function_handle, Statement statement);
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
