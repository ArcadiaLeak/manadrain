#include <condition_variable>
#include <cstdint>
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

struct BinaryExpression {
  char32_t operation;
};
struct LogicalExpression {
  Operator operation;
};
struct MemberExpression {
  Identifier property;
};
struct FunctionCallExpression {
  std::size_t passed_arguments;
  bool under_context;
};
struct AssignExpression {};

struct ObjectShape {
  std::vector<Identifier> properties;
};
struct ObjectExpression {
  const ObjectShape *object_shape;
};

using Expression =
    std::variant<std::monostate, std::u16string_view, std::int64_t, double,
                 Identifier, const FunctionDefinition *, BinaryExpression,
                 LogicalExpression, MemberExpression, FunctionCallExpression,
                 AssignExpression, ObjectExpression>;

struct WriteVariable {
  Identifier variable_name;
  Expression rvalue;
};
struct ReturnStatement {
  Expression argument;
};
using Statement = std::variant<Expression, WriteVariable, ReturnStatement>;

enum class IntrinsicFunction { F_LOG };

struct FunctionFrame;
struct FunctionDefinition {
  std::optional<Identifier> function_name;
  std::vector<Identifier> arguments;
  std::vector<Identifier> local_scope;
  std::vector<std::pair<Identifier, const FunctionDefinition *>>
      nested_functions;
  std::vector<Statement> body;
};

struct ObjectInstance;
using Dynamic =
    std::variant<std::monostate, std::u16string_view, std::int64_t, double,
                 ObjectInstance *, FunctionFrame *, IntrinsicFunction>;

struct ObjectInstance {
  virtual Dynamic *get_property(Identifier property) = 0;
};
struct VanillaObject final : ObjectInstance {
  const ObjectShape *object_shape;
  std::span<Dynamic> properties;
  Dynamic *get_property(Identifier property) override;
};
struct GlobalObject final : ObjectInstance {
  GlobalObject(Dynamic c) : console{c} {};
  Dynamic console;
  Dynamic *get_property(Identifier property) override;
};
struct ConsoleObject final : ObjectInstance {
  ConsoleObject(Dynamic l) : log{l} {};
  Dynamic log;
  Dynamic *get_property(Identifier property) override;
};

struct FunctionFrame {
  const FunctionDefinition *definition;
  std::size_t program_count;
  std::span<std::optional<Dynamic>> own_scope;
  FunctionFrame *closure;
  Dynamic return_val;
  std::optional<Dynamic> *get_variable(Identifier var_handle);
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
  std::vector<std::shared_ptr<const FunctionDefinition>> function_definitions;
  std::vector<std::shared_ptr<const ObjectShape>> object_shapes;
  std::vector<std::shared_ptr<const std::u16string>> permanent_strings;

  std::unique_ptr<std::pmr::monotonic_buffer_resource> resource;
  std::pmr::list<FunctionFrame> function_frames;
  std::pmr::list<std::pmr::vector<std::optional<Dynamic>>> function_scopes;
  std::pmr::list<VanillaObject> object_instances;
  std::pmr::list<std::pmr::vector<Dynamic>> object_properties;

  void initialize(FunctionFrame &frame);

private:
  ConsoleObject console{IntrinsicFunction::F_LOG};
  std::indirect<std::mutex> console_mutex;
  std::indirect<std::condition_variable_any> console_condition;
  std::list<ConsoleMessage> console_messages;

  GlobalObject global_this{&console};
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

  FunctionDefinition *current_function;
  void parse_statement();
  const FunctionDefinition *parse_function_decl();
  WriteVariable parse_variable_decl();
};
} // namespace Manadrain
