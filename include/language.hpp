#include <any>
#include <cstdint>
#include <flat_map>
#include <generator>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Manadrain {
struct ReservedWord {
  std::string_view sv;
};
struct Identifier {
  std::string_view sv;
};
struct StringHandle {
  std::string_view sv;
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

struct ExpressionNode;
using Expression = std::variant<std::monostate, StringHandle, std::int64_t,
                                double, Identifier, const ExpressionNode *>;
struct BinaryExpression {
  Expression left;
  Expression right;
  char32_t op;
};
struct MemberExpression {
  Expression object;
  std::string_view property;
};
struct FunctionCallExpression {
  Expression callee;
  std::vector<Expression> arguments;
};
struct ExpressionNode {
  std::variant<BinaryExpression, MemberExpression, FunctionCallExpression> alt;
};

struct VariableDeclaration {
  std::string_view variable_name;
  Expression initializer;
};
struct ReturnStatement {
  Expression argument;
};
using Statement =
    std::variant<Expression, VariableDeclaration, ReturnStatement>;

enum IntrinsicHandle : std::ptrdiff_t { H_CONSOLE, H_GLOBAL, H_LOG, H_MAIN };
struct ObjectHandle {
  std::ptrdiff_t handle;
};
struct FunctionHandle {
  std::ptrdiff_t handle;
  std::any context;
};
using Dynamic = std::variant<std::monostate, StringHandle, std::int64_t, double,
                             ObjectHandle, FunctionHandle>;

struct VanillaObject {
  std::span<const char *const> object_shape;
  std::vector<Dynamic> properties;
};

struct FunctionBlueprint {
  std::string_view function_name;
  std::vector<const char *> scope_shape;
  std::vector<std::pair<std::string_view, const FunctionBlueprint *>>
      nestedly_declared;
  std::vector<Statement> body;
};
struct VanillaFunction {
  const FunctionBlueprint *blueprint_handle;
  std::optional<std::size_t> parent_handle;
  std::vector<std::optional<Dynamic>> own_scope;
  Dynamic return_val;
};

class Script {
public:
  Script();
  void evaluate();

protected:
  std::unordered_set<std::string_view> atom_atlas;
  std::vector<std::shared_ptr<const char[]>> atom_pool;
  std::vector<std::shared_ptr<const ExpressionNode>> expr_pool;

  VanillaFunction main_function;
  std::vector<std::shared_ptr<const FunctionBlueprint>> blueprint_pool;
  std::vector<std::indirect<VanillaFunction>> function_pool;

  std::vector<std::shared_ptr<const char *const[]>> shape_pool;
  std::vector<std::indirect<VanillaObject>> object_pool;

  FunctionHandle bootstrap(std::size_t blueprint_handle,
                           std::optional<std::size_t> parent_scope);

private:
  VanillaObject console;
  VanillaObject global_this;

  std::generator<std::reference_wrapper<VanillaFunction>>
  traverse_function_closure(
      std::reference_wrapper<VanillaFunction> function_ref);
  std::optional<Dynamic> *get_variable(VanillaFunction &function,
                                       std::string_view var_handle);
  Dynamic *get_property(VanillaObject &object,
                        std::string_view property_handle);

  Dynamic evaluate(VanillaFunction &function,
                   const BinaryExpression &expression);
  Dynamic evaluate(VanillaFunction &function,
                   const MemberExpression &expression);
  Dynamic evaluate(VanillaFunction &function,
                   const FunctionCallExpression &expression);
  Dynamic evaluate(VanillaFunction &function, Expression expression);
  Dynamic evaluate(VanillaFunction &function, Identifier identifier);
  Dynamic evaluate(VanillaFunction &function, const ExpressionNode *expr_ptr);
  Dynamic evaluate(VanillaFunction &function, StringHandle string_handle);
  Dynamic evaluate(VanillaFunction &function, std::int64_t number);
  Dynamic evaluate(VanillaFunction &function, double number);
  Dynamic evaluate(VanillaFunction &function, std::monostate) { return {}; }

  void evaluate(VanillaFunction &function, VariableDeclaration declaration);
  void evaluate(VanillaFunction &function, ReturnStatement statement);
  void evaluate(VanillaFunction &function, Statement statement);
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
  const FunctionBlueprint *parse_function_decl();
  VariableDeclaration parse_variable_decl();
};
} // namespace Manadrain
