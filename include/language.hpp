#include <condition_variable>
#include <cstdint>
#include <flat_map>
#include <generator>
#include <inplace_vector>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <variant>
#include <vector>

namespace Manadrain {
class CompilationError : public std::runtime_error {
public:
  CompilationError(const char *msg) : std::runtime_error{msg} {}
  CompilationError() : std::runtime_error{"compilation error!"} {}
};
class UnexpectedStringEnd final : public CompilationError {
public:
  UnexpectedStringEnd() : CompilationError{"unexpected string end!"} {}
};
class UnexpectedToken final : public CompilationError {
public:
  UnexpectedToken() : CompilationError{"unexpected token!"} {}
};
class MissingPunctuation final : public CompilationError {
public:
  MissingPunctuation(char32_t ch)
      : CompilationError{"missing punctuation!"}, must_be{ch} {}
  char32_t must_be;
};
class MissingFormalParameter final : public CompilationError {
public:
  MissingFormalParameter() : CompilationError{"missing formal parameter!"} {}
};
class MissingFunctionName final : public CompilationError {
public:
  MissingFunctionName() : CompilationError{"missing function_name!"} {}
};
class DuplicateDeclaration final : public CompilationError {
public:
  DuplicateDeclaration() : CompilationError{"duplicate declaration!"} {}
};
class MissingFieldName final : public CompilationError {
public:
  MissingFieldName() : CompilationError{"missing field name!"} {}
};
class InvalidPropertyName final : public CompilationError {
public:
  InvalidPropertyName() : CompilationError{"invalid property name!"} {}
};
class MissingVariableName final : public CompilationError {
public:
  MissingVariableName() : CompilationError{"missing variable name!"} {}
};
class MissingMethod final : public CompilationError {
public:
  MissingMethod() : CompilationError{"missing method!"} {}
};
class InvalidVariableAccess final : public CompilationError {
public:
  InvalidVariableAccess() : CompilationError{"invalid variable access!"} {}
};
class InvalidReturnStatement final : public CompilationError {
public:
  InvalidReturnStatement() : CompilationError{"invalid return statement!"} {}
};

enum class Keyword {
  MONOSTATE,
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

struct Identifier {
  std::size_t offset;
  auto operator<=>(const Identifier &) const = default;
};

using Token =
    std::variant<std::monostate, char32_t, std::int64_t, double, Operator,
                 Keyword, Identifier, std::string_view, std::u16string_view>;

struct FunctionFrame;
struct ObjectInstance;

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

class Machine;

class Compiler {
public:
  Compiler(Machine &m) : machine{m} {}

  struct ObjectShape;
  class ExecutionBoundary;
  class FunctionDefinition;

  enum class IntrinsicObject { O_CONSOLE };
  struct VoidType {};
  struct NumberType {};
  struct StringType {};
  struct LambdaType {};
  struct AnyType {};
  using ValueType = std::variant<VoidType, NumberType, StringType, LambdaType,
                                 AnyType, const ObjectShape *, IntrinsicObject>;

  struct ObjectShape {
    std::flat_map<Identifier, ValueType> properties;
  };

  class CalleeExpression;

  struct FindMethod {
    Identifier identifier;
    std::unique_ptr<CalleeExpression> operator()(VoidType);
    std::unique_ptr<CalleeExpression> operator()(NumberType);
    std::unique_ptr<CalleeExpression> operator()(StringType);
    std::unique_ptr<CalleeExpression> operator()(LambdaType);
    std::unique_ptr<CalleeExpression> operator()(AnyType);
    std::unique_ptr<CalleeExpression>
    operator()(const ObjectShape *object_shape);
    std::unique_ptr<CalleeExpression>
    operator()(IntrinsicObject intrinsic_object);
  };

  class Expression {
  public:
    virtual ~Expression() = default;

    virtual ValueType datatype() const = 0;

    virtual std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const = 0;

    virtual std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const = 0;
  };

  struct InjectStringify {
    std::unique_ptr<Expression> expression;
    std::unique_ptr<Expression> operator()(VoidType);
    std::unique_ptr<Expression> operator()(NumberType);
    std::unique_ptr<Expression> operator()(StringType);
    std::unique_ptr<Expression> operator()(LambdaType);
    std::unique_ptr<Expression> operator()(AnyType);
    std::unique_ptr<Expression> operator()(const ObjectShape *object_shape);
    std::unique_ptr<Expression> operator()(IntrinsicObject intrinsic_object);
  };

  class CalleeExpression : public Expression {
  public:
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }

    virtual void analyze_arguments(
        ExecutionBoundary &boundary,
        std::span<const std::unique_ptr<Expression>> arguments) = 0;
  };

  class AliasedFunctionCall final : public Expression {
  public:
    std::unique_ptr<Expression> callee;
    std::vector<std::unique_ptr<Expression>> arguments;

    ValueType datatype() const override { std::unreachable(); }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class DirectFunctionCall final : public CalleeExpression {
  public:
    const FunctionDefinition *callee;
    std::vector<Identifier> passed_identifiers;
    std::vector<std::unique_ptr<Expression>> passed_values;

    ValueType datatype() const override { return callee->return_type; }

    void analyze_arguments(
        ExecutionBoundary &boundary,
        std::span<const std::unique_ptr<Expression>> arguments) override;
  };

  class ConsoleLogCall final : public CalleeExpression {
  public:
    std::vector<std::unique_ptr<Expression>> arguments;

    ValueType datatype() const override { return VoidType{}; }

    void analyze_arguments(
        ExecutionBoundary &boundary,
        std::span<const std::unique_ptr<Expression>> arguments) override;
  };

  class ObjectExpression final : public Expression {
  public:
    ObjectShape object_shape;
    std::vector<Identifier> keys;
    std::vector<std::unique_ptr<Expression>> values;

    ValueType datatype() const override { std::unreachable(); }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class MemberExpression final : public Expression {
  public:
    std::unique_ptr<Expression> object;
    Identifier property;

    ValueType datatype() const override { std::unreachable(); }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override;
  };

  class StringLength final : public Expression {
  public:
    std::unique_ptr<Expression> argument;

    ValueType datatype() const override { return NumberType{}; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class StringifyNumber final : public Expression {
  public:
    std::unique_ptr<Expression> argument;

    ValueType datatype() const override { return StringType{}; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class BinaryExpression final : public Expression {
  public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    char32_t op;

    ValueType datatype() const override { return NumberType{}; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class LogicalExpression final : public Expression {
  public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    Operator op;

    ValueType datatype() const override { std::unreachable(); }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class AssignExpression final : public Expression {
  public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    ValueType datatype() const override { std::unreachable(); }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class NumericLiteral final : public Expression {
  public:
    double val;

    ValueType datatype() const override { return NumberType{}; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class AsciiLiteral final : public Expression {
  public:
    std::string val;

    ValueType datatype() const override { return StringType{}; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class UnicodeLiteral final : public Expression {
  public:
    std::u16string val;

    ValueType datatype() const override { return StringType{}; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class VariableAccessor final : public Expression {
  public:
    Identifier identifier;

    ValueType datatype() const override { std::unreachable(); }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override;
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override;
  };

  class ScopeAccessor final : public Expression {
  public:
    ValueType local_type;
    std::size_t scope_offset;
    std::size_t local_offset;

    ValueType datatype() const override { return local_type; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    };
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class IntrinsicAccessor final : public Expression {
  public:
    IntrinsicObject object_type;

    ValueType datatype() const override { return object_type; }
    std::unique_ptr<Expression>
    analyze_as_value(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
    std::unique_ptr<CalleeExpression>
    analyze_as_callee(ExecutionBoundary &boundary) const override {
      std::unreachable();
    }
  };

  class Statement {
  public:
    virtual ~Statement() = default;
    virtual std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const = 0;
  };

  class InitializeVariable final : public Statement {
  public:
    Identifier variable_name;
    std::unique_ptr<Expression> rvalue;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override;
  };

  class InitializeScope final : public Statement {
  public:
    std::size_t local_offset;
    std::unique_ptr<Expression> rvalue;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override {
      std::unreachable();
    };
  };

  class ExpressionStatement final : public Statement {
  public:
    std::unique_ptr<Expression> argument;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override;
  };

  class ReturnStatement final : public Statement {
  public:
    std::unique_ptr<Expression> argument;
    std::unique_ptr<Statement>
    analyze(ExecutionBoundary &boundary) const override;
  };

  class ExecutionBoundary {
  public:
    ExecutionBoundary(Compiler &c) : compiler{c} {}

    const ExecutionBoundary *parent_boundary;
    std::vector<std::unique_ptr<Statement>> program;

    std::list<FunctionDefinition> inner_functions;
    const FunctionDefinition *find_function(Identifier identifier) const;

    std::flat_map<Identifier, ValueType> local_scope;
    std::unique_ptr<ScopeAccessor> find_local(Identifier identifier) const;

    ValueType return_type;
    virtual bool allows_return_stmt() const = 0;

    void parse_statement();
    void analyze();

  protected:
    Compiler &compiler;

    void parse_function_stmt();
    void parse_variable_decl();

    std::unique_ptr<Expression> parse_expression();

    std::unique_ptr<Expression> parse_assign_expr();
    std::unique_ptr<Expression> parse_logical_disjunct();
    std::unique_ptr<Expression> parse_additive_expr();
    std::unique_ptr<Expression> parse_object_literal();

    std::unique_ptr<Expression>
    parse_member_expr(std::unique_ptr<Expression> callee);
    std::unique_ptr<Expression>
    parse_call_expr(std::unique_ptr<Expression> callee);
    std::unique_ptr<Expression> parse_postfix_expr();

    struct ParsePrimaryExpression {
      ExecutionBoundary &boundary;
      std::unique_ptr<Expression> operator()(std::monostate);
      std::unique_ptr<Expression> operator()(char32_t punct);
      std::unique_ptr<Expression> operator()(std::int64_t number);
      std::unique_ptr<Expression> operator()(double number);
      std::unique_ptr<Expression> operator()(Operator op);
      std::unique_ptr<Expression> operator()(Keyword keyword);
      std::unique_ptr<Expression> operator()(Identifier identifier);
      std::unique_ptr<Expression> operator()(std::string_view ascii);
      std::unique_ptr<Expression> operator()(std::u16string_view unicode);
    };
  };

  class FunctionDefinition final : public ExecutionBoundary {
  public:
    FunctionDefinition(Compiler &c) : ExecutionBoundary{c} {}

    std::optional<Identifier> function_name;
    std::vector<Identifier> arguments;

    bool allows_return_stmt() const override { return 1; }
    void parse_function_decl();
  };

  class ModuleDefinition final : public ExecutionBoundary {
  public:
    ModuleDefinition(Compiler &c) : ExecutionBoundary{c} {}
    bool allows_return_stmt() const override { return 0; }
  };

  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;
  void parse_text();
  void analyze_program();

private:
  Machine &machine;

  std::map<std::string, Identifier> atom_atlas;
  std::set<std::string> token_strings;
  std::set<std::u16string> token_u16strings;

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

  ModuleDefinition entry_module{*this};
};

class Machine {
public:
  void evaluate();

private:
  friend class Compiler;
};
} // namespace Manadrain
