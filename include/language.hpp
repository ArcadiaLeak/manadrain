#include <cstdint>
#include <generator>
#include <inplace_vector>
#include <list>
#include <optional>
#include <ranges>
#include <stack>
#include <unordered_set>
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
  W_SWITCH
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
struct UNSUPPORTED {};
class ScriptError : public std::exception {
public:
  using MESSAGE = std::variant<
      INVALID_NUMERIC_LITERAL, INVALID_PROPERTY_NAME, INVALID_BACKSLASH_ESCAPE,
      INVALID_TYPE_ANNOTATION, MISSING_FIELD_NAME, MISSING_VARIABLE_NAME,
      MISSING_FUNCTION_NAME, MISSING_IDENTIFIER, MISSING_STRING_LITERAL,
      MISSING_FORMAL_PARAMETER, MISSING_TYPE_ANNOTATION, MISSING_PUNCT,
      UNEXPECTED_RESERVED_WORD, UNEXPECTED_STRING_END, UNEXPECTED_COMMENT_END,
      UNEXPECTED_TOKEN, UNEXPECTED_END_OF_FILE, UNSUPPORTED>;
  MESSAGE message;

  explicit ScriptError(MESSAGE msg) : message{msg} {}
  const char *what() const noexcept override { return "script error!"; }
};

using DYNAMIC = std::monostate;
struct ATOM_REF {
  std::string_view pool_view;
};

class FunctionDecl;

class VariableDecl {
public:
  std::string_view variable_name;
  std::monostate initializer;
};
using Statement = std::variant<std::monostate, FunctionDecl, VariableDecl>;

class Script {
public:
  std::vector<std::uint8_t> text_source;
  std::size_t position;

  std::list<std::string> atom_pool;
  std::unordered_set<std::string_view> atom_atlas;

  std::vector<std::variant<DYNAMIC>> local_heap;
  std::vector<std::variant<ATOM_REF>> shared_heap;

  std::vector<Statement> script_body;

  void compile_text();
};

class Tokenizer {
public:
  explicit Tokenizer(Script *s) : script{s} {}

  TOKEN tokenize();
  std::generator<TOKEN> traverse_tokens();

private:
  Script *script;
  std::stack<std::optional<char32_t>> backtrace;

  TOKEN tokenize_word(char32_t leading);
  STRING_LITERAL tokenize_string_literal(char32_t separator);
  TOKEN tokenize_numeric_literal(char32_t leading);

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);
};

class FunctionDecl {
public:
  std::string_view function_name;
  std::vector<Statement> function_body;
};

class ParseFunctionDecl {
public:
  explicit ParseFunctionDecl(Tokenizer *t, FunctionDecl *f)
      : tokenizer{t}, funcdecl{f} {}

  void operator()(IDENTIFIER identifier);
  void operator()(char32_t punct);

  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  Tokenizer *tokenizer;
  FunctionDecl *funcdecl;

  enum { FUNCTION_I, FUNCTION_II, FUNCTION_III, FUNCTION_IV };
  int stage;
};

class ParseStatement {
public:
  explicit ParseStatement(Tokenizer *t, Statement *s) : tokenizer{t}, stmt{s} {}

  void operator()(RESERVED reserved);
  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  Tokenizer *tokenizer;
  Statement *stmt;
};

class ParseVariableDecl {
public:
  explicit ParseVariableDecl(Tokenizer *t, VariableDecl *v)
      : tokenizer{t}, vardecl{v} {}

  void operator()(IDENTIFIER identifier);
  void operator()(char32_t punct);

  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  Tokenizer *tokenizer;
  VariableDecl *vardecl;

  enum { VARIABLE_I, VARIABLE_II, VARIABLE_III };
  int stage;
};

class ParseExpression {
public:
  explicit ParseExpression(Tokenizer *t) : tokenizer{t} {}

  void operator()(STRING_LITERAL string_literal);
  void operator()(NUMERIC_LITERAL num_literal);

  template <typename T> void operator()(T visitee) {
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }

private:
  Tokenizer *tokenizer;
};
} // namespace Manadrain
