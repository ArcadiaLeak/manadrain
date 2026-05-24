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
  K_SWITCH
};
struct Identifier {
  std::size_t offset;
  auto operator<=>(const Identifier &) const = default;
};
struct ImmuString {
  std::shared_ptr<const char[]> ptr;
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
                           Operator, Keyword, Identifier, ImmuString>;

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
using Expression = std::variant<std::monostate, ImmuString, std::int64_t,
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
using Dynamic = std::variant<std::monostate, ImmuString, std::int64_t, double,
                             ObjectHandle, FunctionHandle>;

using ObjectShape = std::shared_ptr<const Identifier[]>;
struct VanillaObject {
  std::expected<ObjectShape, IntrinsicSigil> shape_handle;
  std::vector<Dynamic> properties;
};

struct FunctionDefinition {
  Identifier function_name;
  std::vector<Identifier> arguments;
  std::vector<Identifier> local_scope;
  std::vector<std::pair<Identifier, const FunctionDefinition *>>
      nestedly_declared;
  std::vector<Statement> body;
};
struct FunctionClosure {
  const FunctionDefinition *definition;
  std::expected<std::size_t, IntrinsicSigil> parent_handle{
      std::unexpect, IntrinsicSigil::H_NIL};
  std::vector<std::optional<Dynamic>> own_scope;
  Dynamic return_val;
};

class Script {
public:
  void evaluate();

protected:
  std::unordered_map<std::string_view, ImmuString> string_atlas;
  std::unordered_map<std::string_view, Identifier> atom_atlas;
  std::vector<ImmuString> atom_pool;
  std::vector<std::shared_ptr<const ExpressionNode>> expr_pool;

  FunctionClosure main_function;
  std::vector<std::shared_ptr<const FunctionDefinition>> function_definitions;
  std::vector<std::unique_ptr<FunctionClosure>> function_closures;
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

  std::generator<FunctionClosure *>
  climb_closure_stack(FunctionClosure *closure_ptr);
  std::optional<Dynamic> *get_variable(FunctionClosure &closure,
                                       Identifier var_handle);
  Dynamic *get_property(VanillaObject &object, Identifier property_handle);

  std::string evaluate_message(std::monostate);
  std::string evaluate_message(ImmuString immu_string);
  std::string evaluate_message(std::int64_t number);
  std::string evaluate_message(double number);
  std::string evaluate_message(ObjectHandle object_handle);
  std::string evaluate_message(FunctionHandle function_handle);

  Dynamic evaluate_property(Identifier property, std::monostate);
  Dynamic evaluate_property(Identifier property, ImmuString immu_string);
  Dynamic evaluate_property(Identifier property, std::int64_t number);
  Dynamic evaluate_property(Identifier property, double number);
  Dynamic evaluate_property(Identifier property, ObjectHandle object_handle);
  Dynamic evaluate_property(Identifier property,
                            FunctionHandle function_handle);

  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure,
                  const BinaryExpression &expression);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure,
                  const MemberExpression &expression);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure,
                  const FunctionCallExpression &expression);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              Identifier identifier);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              const ExpressionNode *expr_ptr);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              ImmuString immu_string);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              std::int64_t number);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              double number);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              std::monostate);

  Dynamic evaluate(FunctionClosure &closure,
                   const BinaryExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const MemberExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const FunctionCallExpression &expression);
  Dynamic evaluate(FunctionClosure &closure, Identifier identifier);
  Dynamic evaluate(FunctionClosure &closure, const ExpressionNode *expr_ptr);
  Dynamic evaluate(FunctionClosure &closure, ImmuString immu_string);
  Dynamic evaluate(FunctionClosure &closure, std::int64_t number);
  Dynamic evaluate(FunctionClosure &closure, double number);
  Dynamic evaluate(FunctionClosure &closure, std::monostate) { return {}; }
  Dynamic evaluate(FunctionClosure &closure, Expression expression);

  void evaluate(FunctionClosure &closure, VariableDeclaration declaration);
  void evaluate(FunctionClosure &closure, ReturnStatement statement);
  void evaluate(FunctionClosure &closure, Statement statement);
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

  void parse_statement(FunctionDefinition &definition, Token leading);
  const FunctionDefinition *parse_function_decl();
  VariableDeclaration parse_variable_decl();
};
} // namespace Manadrain
