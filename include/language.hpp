#include <cstdint>
#include <expected>
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
struct ReservedWord {
  std::size_t offset;
};
struct Identifier {
  bool permanent;
  std::size_t offset;
  std::size_t size;
  auto operator<=>(const Identifier &) const = default;
};
struct StringHandle {
  bool permanent;
  std::size_t offset;
  std::size_t size;
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
struct UnexpectedReservedWord {};
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
  Identifier property;
};
struct FunctionCallExpression {
  Expression callee;
  std::vector<Expression> arguments;
};
struct ExpressionNode {
  std::variant<BinaryExpression, MemberExpression, FunctionCallExpression> alt;
};

struct VariableDeclaration {
  Identifier variable_name;
  Expression initializer;
};
struct ReturnStatement {
  Expression argument;
};
using Statement =
    std::variant<Expression, VariableDeclaration, ReturnStatement>;

enum class IntrinsicSigil { H_NIL, H_CONSOLE, H_GLOBAL, H_LOG, H_MAIN };
struct ObjectHandle {
  std::expected<std::size_t, IntrinsicSigil> offset;
};
struct FunctionHandle {
  std::expected<std::size_t, IntrinsicSigil> offset;
};
using Dynamic = std::variant<std::monostate, StringHandle, std::int64_t, double,
                             ObjectHandle, FunctionHandle>;

struct VanillaObject {
  std::expected<std::shared_ptr<const Identifier[]>, IntrinsicSigil>
      shape_handle;
  std::vector<Dynamic> properties;
};

struct FunctionBlueprint {
  Identifier function_name;
  std::vector<Identifier> arguments;
  std::vector<Identifier> local_scope;
  std::vector<std::pair<Identifier, const FunctionBlueprint *>>
      nestedly_declared;
  std::vector<Statement> body;
};
struct VanillaFunction {
  const FunctionBlueprint *blueprint_ptr;
  std::expected<std::size_t, IntrinsicSigil> parent_handle{
      std::unexpect, IntrinsicSigil::H_NIL};
  std::vector<std::optional<Dynamic>> own_scope;
  Dynamic return_val;
};

class Script {
public:
  void evaluate();

protected:
  std::unordered_map<std::string_view, std::size_t> atom_atlas;
  std::vector<std::shared_ptr<const char[]>> atom_pool;
  std::vector<std::shared_ptr<const ExpressionNode>> expr_pool;

  VanillaFunction main_function;
  std::vector<std::shared_ptr<const FunctionBlueprint>> blueprint_pool;
  std::vector<std::unique_ptr<VanillaFunction>> function_pool;

  std::vector<std::weak_ptr<const Identifier[]>> shape_pool;
  std::vector<std::unique_ptr<VanillaObject>> object_pool;

  void instantiate(FunctionHandle function_handle);

private:
  VanillaObject global_this{
      .shape_handle = std::unexpected{IntrinsicSigil::H_GLOBAL},
      .properties = {ObjectHandle{std::unexpected{IntrinsicSigil::H_CONSOLE}}}};
  VanillaObject console{
      .shape_handle = std::unexpected{IntrinsicSigil::H_CONSOLE},
      .properties = {FunctionHandle{std::unexpected{IntrinsicSigil::H_LOG}}}};
  std::vector<std::string> console_messages;

  std::generator<VanillaFunction *>
  traverse_function_closure(VanillaFunction *function_ptr);
  std::optional<Dynamic> *get_variable(VanillaFunction &function,
                                       Identifier var_handle);
  Dynamic *get_property(VanillaObject &object, Identifier property_handle);

  std::string evaluate_message(std::monostate);
  std::string evaluate_message(StringHandle string_handle);
  std::string evaluate_message(std::int64_t number);
  std::string evaluate_message(double number);
  std::string evaluate_message(ObjectHandle object_handle);
  std::string evaluate_message(FunctionHandle function_handle);

  Dynamic evaluate_property(Identifier property, std::monostate);
  Dynamic evaluate_property(Identifier property, StringHandle string_handle);
  Dynamic evaluate_property(Identifier property, std::int64_t number);
  Dynamic evaluate_property(Identifier property, double number);
  Dynamic evaluate_property(Identifier property, ObjectHandle object_handle);
  Dynamic evaluate_property(Identifier property,
                            FunctionHandle function_handle);

  std::pair<Dynamic, Dynamic>
  evaluate_callee(VanillaFunction &function,
                  const BinaryExpression &expression);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(VanillaFunction &function,
                  const MemberExpression &expression);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(VanillaFunction &function,
                  const FunctionCallExpression &expression);
  std::pair<Dynamic, Dynamic> evaluate_callee(VanillaFunction &function,
                                              Identifier identifier);
  std::pair<Dynamic, Dynamic> evaluate_callee(VanillaFunction &function,
                                              const ExpressionNode *expr_ptr);
  std::pair<Dynamic, Dynamic> evaluate_callee(VanillaFunction &function,
                                              StringHandle string_handle);
  std::pair<Dynamic, Dynamic> evaluate_callee(VanillaFunction &function,
                                              std::int64_t number);
  std::pair<Dynamic, Dynamic> evaluate_callee(VanillaFunction &function,
                                              double number);
  std::pair<Dynamic, Dynamic> evaluate_callee(VanillaFunction &function,
                                              std::monostate);

  Dynamic evaluate(VanillaFunction &function,
                   const BinaryExpression &expression);
  Dynamic evaluate(VanillaFunction &function,
                   const MemberExpression &expression);
  Dynamic evaluate(VanillaFunction &function,
                   const FunctionCallExpression &expression);
  Dynamic evaluate(VanillaFunction &function, Identifier identifier);
  Dynamic evaluate(VanillaFunction &function, const ExpressionNode *expr_ptr);
  Dynamic evaluate(VanillaFunction &function, StringHandle string_handle);
  Dynamic evaluate(VanillaFunction &function, std::int64_t number);
  Dynamic evaluate(VanillaFunction &function, double number);
  Dynamic evaluate(VanillaFunction &function, std::monostate) { return {}; }
  Dynamic evaluate(VanillaFunction &function, Expression expression);

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
