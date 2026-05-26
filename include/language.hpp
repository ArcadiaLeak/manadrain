#include <condition_variable>
#include <cstdint>
#include <generator>
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
  K_SWITCH
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

struct ReferentialExpression;
struct FunctionDefinition;
using Expression =
    std::variant<std::monostate, std::u16string_view, std::int64_t, double,
                 Identifier, const ReferentialExpression *,
                 const FunctionDefinition *>;
struct BinaryExpression {
  Expression left;
  Expression right;
  char32_t op;
};
struct LogicalExpression {
  Expression left;
  Expression right;
  Operator op;
};
struct MemberExpression {
  Expression object;
  Identifier property;
};
struct FunctionCallExpression {
  Expression callee;
  std::vector<Expression> arguments;
};
struct AssignExpression {
  Expression left;
  Expression right;
};
struct ObjectExpression {
  std::vector<std::pair<Expression, Expression>> properties;
};
struct ReferentialExpression {
  std::variant<BinaryExpression, LogicalExpression, MemberExpression,
               FunctionCallExpression, AssignExpression, ObjectExpression>
      alt;
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
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  std::vector<Identifier> local_scope;
  std::vector<std::pair<Identifier, const FunctionDefinition *>>
      nested_functions;
  std::vector<Statement> body;
};
using Dynamic = std::variant<std::monostate, std::u16string_view, std::int64_t,
                             double, ObjectInstance *, IntrinsicObject,
                             FunctionClosure *, IntrinsicFunction>;
struct ObjectInstance {
  std::span<const Identifier> object_shape;
  std::pmr::vector<Dynamic> properties;
};
using FunctionScope = std::pmr::vector<std::optional<Dynamic>>;
struct FunctionClosure {
  const FunctionDefinition *definition;
  FunctionClosure *parent_closure;
  FunctionScope own_scope;
  Dynamic return_val;
};

inline constexpr std::size_t IDENT_console{0};
inline constexpr std::size_t IDENT_log{1};
inline constexpr std::size_t IDENT_length{2};

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
  FunctionClosure *main_function;
  std::vector<std::shared_ptr<const ReferentialExpression>>
      referential_expressions;
  std::vector<std::shared_ptr<const FunctionDefinition>> function_definitions;
  std::vector<std::shared_ptr<const std::vector<Identifier>>> object_shapes;
  std::vector<std::shared_ptr<const std::u16string>> permanent_strings;

  std::unique_ptr<std::pmr::monotonic_buffer_resource> resource;
  std::pmr::list<FunctionClosure> function_closures;
  std::pmr::list<ObjectInstance> object_instances;

  void initialize(FunctionClosure &closure);

private:
  static inline const std::array shape_console{
      std::to_array<Identifier>({Identifier{IDENT_log}})};
  static inline const std::array shape_global_this{
      std::to_array<Identifier>({Identifier{IDENT_console}})};

  ObjectInstance *global_this;

  ObjectInstance *console;
  std::indirect<std::mutex> console_mutex;
  std::indirect<std::condition_variable_any> console_condition;
  std::list<ConsoleMessage> console_messages;

  std::generator<FunctionClosure *>
  climb_closure_stack(FunctionClosure *closure_ptr);
  std::optional<Dynamic> *get_variable(FunctionClosure &closure,
                                       Identifier var_handle);
  Dynamic *get_property(ObjectInstance &object, Identifier property_handle);

  std::u16string evaluate_message(std::monostate);
  std::u16string evaluate_message(std::u16string_view permanent_string);
  std::u16string evaluate_message(std::int64_t number);
  std::u16string evaluate_message(double number);
  std::u16string evaluate_message(ObjectInstance *object_instance);
  std::u16string evaluate_message(FunctionClosure *closure);
  std::u16string evaluate_message(IntrinsicObject intrinsic_object);
  std::u16string evaluate_message(IntrinsicFunction intrinsic_function);

  Dynamic evaluate_operation(char32_t op, std::int64_t lhs, std::int64_t rhs);
  Dynamic evaluate_operation(Operator op, std::int64_t lhs, std::int64_t rhs);

  Dynamic evaluate_property(Identifier property, std::monostate);
  Dynamic evaluate_property(Identifier property,
                            std::u16string_view permanent_string);
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
                                 Dynamic context, FunctionClosure *callee_ptr);
  Dynamic evaluate_function_call(FunctionClosure &closure,
                                 const FunctionCallExpression &expr_call,
                                 Dynamic context,
                                 IntrinsicFunction intrinsic_function);

  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure, const MemberExpression &expression);
  std::pair<Dynamic, Dynamic> evaluate_callee(FunctionClosure &closure,
                                              Identifier identifier);
  std::pair<Dynamic, Dynamic>
  evaluate_callee(FunctionClosure &closure,
                  const ReferentialExpression *expr_ptr);

  Dynamic evaluate(FunctionClosure &closure,
                   const BinaryExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const LogicalExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const MemberExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const FunctionCallExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const AssignExpression &expression);
  Dynamic evaluate(FunctionClosure &closure,
                   const ObjectExpression &expression);
  Dynamic evaluate(FunctionClosure &closure, Identifier identifier);
  Dynamic evaluate(FunctionClosure &closure,
                   const FunctionDefinition *definition);
  Dynamic evaluate(FunctionClosure &closure,
                   const ReferentialExpression *expr_ptr);
  Dynamic evaluate(FunctionClosure &closure,
                   std::u16string_view permanent_string);
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

  Expression parse_primary_expr();
  Expression parse_postfix_expr();
  Expression parse_additive_expr();
  Expression parse_logical_disjunct();
  Expression parse_assign_expr();
  Expression parse_member_expr(Expression obj_expr);
  Expression parse_call_expr(Expression callee_expr);
  Expression parse_object_literal();
  Expression parse_expression();

  void parse_statement(FunctionDefinition &definition);
  const FunctionDefinition *parse_function_decl();
  VariableDeclaration parse_variable_decl();
};
} // namespace Manadrain
