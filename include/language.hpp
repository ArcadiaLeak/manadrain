#include <cstdint>
#include <generator>
#include <inplace_vector>
#include <optional>
#include <ranges>
#include <set>
#include <stack>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Manadrain {
enum class RESERVED {
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
  W_INT,
  W_LONG,
  W_UINT,
  W_ULONG,
  W_FLOAT,
  W_DOUBLE,
  W_STRING
};
struct IDENTIFIER {
  std::string_view pool_view;
};
struct STRING_LITERAL {
  std::string_view pool_view;
  char32_t separator;
};
using NUMERIC_LITERAL = std::variant<std::int32_t, std::int64_t, double>;
using TOKEN = std::variant<char32_t, NUMERIC_LITERAL, RESERVED, IDENTIFIER,
                           STRING_LITERAL>;

enum class DATATYPE { T_I32, T_I64, T_F32, T_F64, T_U32, T_U64, T_STRING };

struct INVALID_NUMERIC_LITERAL {};
struct INVALID_PROPERTY_NAME {};
struct INVALID_BACKSLASH_ESCAPE {};
struct INVALID_TYPE_ANNOTATION {};
struct MISSING_FIELD_NAME {};
struct MISSING_VARIABLE_NAME {};
struct MISSING_FUNCTION_NAME {};
struct MISSING_IDENTIFIER {};
struct MISSING_STRING_LITERAL {};
struct MISSING_FORMAL_PARAMETER {};
struct MISSING_TYPE_ANNOTATION {};
struct MISSING_PUNCT {
  char32_t must_be;
};
struct UNEXPECTED_RESERVED_WORD {};
struct UNEXPECTED_STRING_END {};
struct UNEXPECTED_COMMENT_END {};
struct UNEXPECTED_TOKEN {};
struct UNEXPECTED_END_OF_FILE {};
class ScriptError : public std::exception {
public:
  using MESSAGE = std::variant<
      INVALID_NUMERIC_LITERAL, INVALID_PROPERTY_NAME, INVALID_BACKSLASH_ESCAPE,
      INVALID_TYPE_ANNOTATION, MISSING_FIELD_NAME, MISSING_VARIABLE_NAME,
      MISSING_FUNCTION_NAME, MISSING_IDENTIFIER, MISSING_STRING_LITERAL,
      MISSING_FORMAL_PARAMETER, MISSING_TYPE_ANNOTATION, MISSING_PUNCT,
      UNEXPECTED_RESERVED_WORD, UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END,
      UNEXPECTED_TOKEN, UNEXPECTED_END_OF_FILE>;
  MESSAGE message;

  explicit ScriptError(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "comp error!"; }
};

class Compilation {
public:
  std::vector<std::uint8_t> text_source;
  std::size_t position;

  std::stack<std::optional<char32_t>> backtrace;
  std::set<std::string> string_pool;

  std::vector<std::vector<std::uint64_t>> const_pool;
  std::unordered_map<std::string_view, std::size_t> str_to_const;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  TOKEN tokenize();
  std::generator<TOKEN> traverse_tokens();
  void compile_text();

private:
  TOKEN tokenize_word(char32_t leading);
  STRING_LITERAL tokenize_string_literal(char32_t separator);
  TOKEN tokenize_numeric_literal(char32_t leading);
};

class ParseDeclaration {
public:
  explicit ParseDeclaration(Compilation *c) : comp{c} {}

  void operator()(RESERVED reserved);
  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  Compilation *comp;
};

struct HEAP_ALLOC {
  std::uint8_t dest;
  std::size_t const_idx;
};
using INSTRUCTION = std::variant<HEAP_ALLOC>;

class ParseFunctionDecl {
public:
  explicit ParseFunctionDecl(Compilation *c) : comp{c} {}

  std::string_view funcname;
  DATATYPE return_type;
  std::vector<INSTRUCTION> instruct_vec;

  void operator()(IDENTIFIER identifier);
  void operator()(char32_t punct);
  void operator()(RESERVED punct);

  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  enum {
    FUNCTION_I,
    FUNCTION_II,
    FUNCTION_III,
    FUNCTION_IV,
    FUNCTION_V,
    FUNCTION_VI
  };
  int stage;
  Compilation *comp;
};

class ParseStatement {
public:
  explicit ParseStatement(ParseFunctionDecl *f, Compilation *s)
      : func{f}, comp{s} {}

  std::inplace_vector<DATATYPE, 32> reflection;

  void operator()(RESERVED reserved);
  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  ParseFunctionDecl *func;
  Compilation *comp;
};

class ParseVariableDecl {
public:
  explicit ParseVariableDecl(ParseStatement *ps, ParseFunctionDecl *f,
                             Compilation *c)
      : stmt{ps}, func{f}, comp{c} {}

  std::string_view variable_name;
  DATATYPE variable_type;

  void operator()(IDENTIFIER identifier);
  void operator()(char32_t punct);
  void operator()(RESERVED punct);

  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  enum { VARIABLE_I, VARIABLE_II, VARIABLE_III, VARIABLE_IV, VARIABLE_V };
  int stage;

  ParseStatement *stmt;
  ParseFunctionDecl *func;
  Compilation *comp;
};

class ParseExpression {
public:
  explicit ParseExpression(ParseStatement *ps, ParseFunctionDecl *f,
                           Compilation *c)
      : stmt{ps}, func{f}, comp{c} {}

  void operator()(STRING_LITERAL string_literal);
  void operator()(NUMERIC_LITERAL num_literal);

  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  ParseStatement *stmt;
  ParseFunctionDecl *func;
  Compilation *comp;
};

class Script {};
} // namespace Manadrain
