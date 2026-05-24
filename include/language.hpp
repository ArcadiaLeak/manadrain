#include <cstdint>
#include <generator>
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
struct IdentifierAtom {
  std::shared_ptr<const char[]> ptr;
  std::size_t size;
};
struct StringInstance {
  bool garbage;
  std::shared_ptr<const char16_t[]> ptr;
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
                           Operator, Keyword, Identifier, StringInstance *>;

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
using Expression = std::variant<std::monostate, StringInstance *, std::int64_t,
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

enum class IntrinsicObject { O_NULL, O_CONSOLE, O_GLOBAL };
enum class IntrinsicFunction { F_LOG };

struct ObjectInstance;
struct FunctionClosure;
struct FunctionDefinition {
  Identifier function_name;
  std::vector<Identifier> arguments;
  std::vector<Identifier> local_scope;
  std::vector<std::pair<Identifier, const FunctionDefinition *>>
      nested_functions;
  std::vector<Statement> body;
};
using Dynamic = std::variant<std::monostate, StringInstance *, std::int64_t,
                             double, ObjectInstance *, IntrinsicObject,
                             FunctionClosure *, IntrinsicFunction>;
struct ObjectInstance {
  bool garbage;
  std::span<const Identifier> object_shape;
  std::vector<Dynamic> properties;
};
struct FunctionClosure {
  bool garbage;
  const FunctionDefinition *definition;
  FunctionClosure *parent_closure;
  std::vector<std::optional<Dynamic>> own_scope;
  Dynamic return_val;
};

class Script {
public:
  void evaluate();

protected:
  bool garbage_mark{1};

  FunctionClosure main_function;
  std::vector<std::shared_ptr<const ExpressionNode>> expr_pool;
  std::vector<std::shared_ptr<const FunctionDefinition>> function_definitions;
  std::vector<std::shared_ptr<const Identifier[]>> shape_pool;

  std::vector<std::unique_ptr<StringInstance>> permanent_strings;
  std::vector<std::unique_ptr<StringInstance>> collectible_strings;
  std::vector<std::unique_ptr<FunctionClosure>> function_closures;
  std::vector<std::unique_ptr<ObjectInstance>> object_pool;

  void initialize(FunctionClosure &closure);

private:
  static inline const std::array shape_console{
      std::to_array<Identifier>({Identifier{1}})};
  static inline const std::array shape_global_this{
      std::to_array<Identifier>({Identifier{0}})};

  ObjectInstance global_this{.object_shape = shape_global_this,
                             .properties = {IntrinsicObject::O_CONSOLE}};
  ObjectInstance console{.object_shape = shape_console,
                         .properties = {IntrinsicFunction::F_LOG}};
  std::vector<std::string> console_messages;

  std::generator<FunctionClosure *>
  climb_closure_stack(FunctionClosure *closure_ptr);
  std::optional<Dynamic> *get_variable(FunctionClosure &closure,
                                       Identifier var_handle);
  Dynamic *get_property(ObjectInstance &object, Identifier property_handle);

  std::string evaluate_message(std::monostate);
  std::string evaluate_message(StringInstance *string_instance);
  std::string evaluate_message(std::int64_t number);
  std::string evaluate_message(double number);
  std::string evaluate_message(ObjectInstance *object_instance);
  std::string evaluate_message(FunctionClosure *closure);
  std::string evaluate_message(IntrinsicObject intrinsic_object);
  std::string evaluate_message(IntrinsicFunction intrinsic_function);

  Dynamic evaluate_property(Identifier property, std::monostate);
  Dynamic evaluate_property(Identifier property,
                            StringInstance *string_instance);
  Dynamic evaluate_property(Identifier property, std::int64_t number);
  Dynamic evaluate_property(Identifier property, double number);
  Dynamic evaluate_property(Identifier property,
                            ObjectInstance *object_instance);
  Dynamic evaluate_property(Identifier property, FunctionClosure *closure);
  Dynamic evaluate_property(Identifier property,
                            IntrinsicObject intrinsic_object);
  Dynamic evaluate_property(Identifier property,
                            IntrinsicFunction intrinsic_function);

  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context, std::monostate);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context,
                                 StringInstance *string_instance);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context, std::int64_t number);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context, double number);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context,
                                 ObjectInstance *object_instance);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context, FunctionClosure *callee_ptr);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context,
                                 IntrinsicObject intrinsic_object);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context,
                                 IntrinsicFunction intrinsic_function);

  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure, const BinaryExpression &expression);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure, const MemberExpression &expression);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure,
                  const FunctionCallExpression &expression);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              Identifier identifier);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              const ExpressionNode *expr_ptr);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              StringInstance *string_instance);
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
  Dynamic evaluate(FunctionClosure &closure, StringInstance *string_instance);
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
  std::vector<IdentifierAtom> atom_pool;
  std::unordered_map<std::string_view, Identifier> atom_atlas;
  std::unordered_map<std::u16string_view, StringInstance *> string_atlas;

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

  Expression parse_primary_expr();
  Expression parse_postfix_expr();
  Expression parse_additive_expr();
  Expression parse_member_expr(Expression obj_expr);
  Expression parse_call_expr(Expression callee_expr);
  Expression parse_expression();

  void parse_statement(FunctionDefinition &definition);
  const FunctionDefinition *parse_function_decl();
  VariableDeclaration parse_variable_decl();
};
} // namespace Manadrain
